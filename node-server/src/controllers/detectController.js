const pythonClient = require('../services/pythonClient');
const sockets = require('../sockets');

function extractBufferFromReq(req) {
  if (req.file && req.file.buffer) return req.file.buffer;
  if (req.body && Buffer.isBuffer(req.body) && req.body.length > 0) return req.body;
  if (req.body && req.body.image) return Buffer.from(req.body.image, 'base64');
  return null;
}

async function upload(req, res) {
  try {
    const buffer = extractBufferFromReq(req);
    if (!buffer) return res.status(400).json({ error: 'no_image_provided' });

    // Diagnostics: log request origin, size and headers
    try {
      const ip = req.ip || req.connection.remoteAddress || 'unknown';
      console.log(`[upload] incoming from ${ip} - size=${buffer.length} bytes - headers=${JSON.stringify(req.headers)}`);

      // If the uploader provided an explicit stream URL (e.g., ngrok or router IP), register it
      const deviceStreamStore = require('../services/deviceStreamStore');
      const streamHeader = req.header('x-stream-url') || req.header('x-forwarded-stream-url');
      const deviceIdHeader = req.header('x-device-id') || null;
      const rssiHeader = req.header('x-rssi') || req.header('x-device-rssi') || null;

      // Store RSSI if provided
      try {
        if (rssiHeader && deviceIdHeader) {
          deviceStreamStore.setRssiByDeviceId(deviceIdHeader, Number(rssiHeader));
        } else if (rssiHeader) {
          const ipKey = req.ip || req.connection.remoteAddress || null;
          if (ipKey) deviceStreamStore.setRssiByIp(ipKey, Number(rssiHeader));
        }
      } catch (e) {
        console.warn('[upload] failed to set RSSI', e && e.message ? e.message : e);
      }

      if (streamHeader && /^https?:\/\//.test(streamHeader)) {
        // Clean IPv4-mapped IPv6 hostnames like '::ffff:192.168.1.27' which can break upstream parsing
        const cleanedStream = streamHeader.replace(/::ffff:(\d+\.\d+\.\d+\.\d+)/g, '$1').trim();
        if (deviceIdHeader) {
          deviceStreamStore.set(deviceIdHeader, cleanedStream);
          console.log(`[upload] registered stream URL for device ${deviceIdHeader} -> ${cleanedStream}`);
        } else {
          // fallback: register by uploader source IP
          const ipKey = req.ip || req.connection.remoteAddress || null;
          if (ipKey) {
            deviceStreamStore.setByIp(ipKey, cleanedStream);
            console.log(`[upload] registered stream URL for ip ${ipKey} -> ${cleanedStream}`);
          }
        }
      } else {
        // If no explicit stream URL, and no mapping exists, we can guess a likely stream URL on the same LAN
        const ipKey = req.ip || req.connection.remoteAddress || null;
        if (ipKey && !deviceStreamStore.getByIp(ipKey)) {
          // Normalize ipKey if it's an IPv4-mapped IPv6 literal
          const cleanedIp = String(ipKey).replace(/^::ffff:/, '');
          const guessed = `http://${cleanedIp}:81/stream`;
          deviceStreamStore.setByIp(cleanedIp, guessed);
          console.log(`[upload] guessed and registered stream URL for ip ${cleanedIp} -> ${guessed}`);

          // If the device provided a device-id header, also register the guessed URL under that id
          try {
            const deviceIdHeader = req.header('x-device-id') || null;
            if (deviceIdHeader && !deviceStreamStore.get(deviceIdHeader)) {
              deviceStreamStore.set(deviceIdHeader, guessed);
              console.log(`[upload] guessed and registered stream URL for device ${deviceIdHeader} -> ${guessed}`);
            }
          } catch (e) {
            console.warn('[upload] failed to register guessed URL under device id', e && e.message ? e.message : e);
          }
        }
      }
    } catch (e) {
      console.warn('[upload] diagnostics failed to log headers or register stream', e.message || e);
    }

    // Store latest snapshot in memory for mobile/clients (also resize for mobile-friendly size)
    try {
      const snapshotStore = require('../services/snapshotStore');
      const imageResizer = require('../services/imageResizer');
      const resized = await imageResizer.resizeTo320(buffer);
      const deviceIdHeader = req.header('x-device-id') || null;
      snapshotStore.setLatest(resized, 'image/jpeg', deviceIdHeader);

      // Broadcast latest frame to connected frontends (binary + metadata)
      try {
        const sockets = require('../sockets');
        const deviceIdHeader = req.header('x-device-id') || null;
        const frameId = `${deviceIdHeader || 'unknown'}-${Date.now()}`;
        sockets.broadcastFrame(frameId, Buffer.from(resized));
        sockets.broadcastMeta(frameId, { deviceId: deviceIdHeader || null, size: Buffer.from(resized).length, ts: Date.now() });
      } catch (e) {
        console.warn('[upload] failed to broadcast frame', e && e.message ? e.message : e);
      }

    } catch (e) {
      console.warn('[upload] snapshot store/resizer failed:', e.message || e);
      try { 
        require('../services/snapshotStore').setLatest(buffer, 'image/jpeg'); 
        // Broadcast original buffer if resize failed
        try {
          const sockets = require('../sockets');
          const deviceIdHeader = req.header('x-device-id') || null;
          const frameId = `${deviceIdHeader || 'unknown'}-${Date.now()}`;
          sockets.broadcastFrame(frameId, Buffer.from(buffer));
          sockets.broadcastMeta(frameId, { deviceId: deviceIdHeader || null, size: Buffer.from(buffer).length, ts: Date.now() });
        } catch (e2) { console.warn('[upload] failed to broadcast original frame', e2 && e2.message ? e2.message : e2) }
      } catch (ex) {}
    }
    // Enqueue detection work into the bounded worker pool so Python is not overwhelmed
    try {
      const detectQueue = require('../services/detectQueue');
      const stats = detectQueue.getStats();
      if (stats.queued >= stats.maxQueue) {
        console.warn('[upload] detect queue is full, rejecting request');
        return res.status(503).json({ ok: false, error: 'detect_queue_full' });
      }

      detectQueue.enqueue(buffer)
        .then((data) => {
          console.log('[upload] detect job completed, broadcasting detections');
          sockets.broadcast('detections', data);
        })
        .catch((err) => {
          // If enqueue or detect job fails, log warning
          console.warn('[upload] detect job failed or was rejected', err && err.message ? err.message : err);
        });
      // Immediately acknowledge receipt and queueing
    } catch (e) {
      console.warn('[upload] failed to enqueue detect job', e && e.message ? e.message : e);
      return res.status(500).json({ ok: false, error: 'detect_enqueue_failed' });
    }

    // If deviceId present and we have a configured public base URL (e.g., ngrok), return a public stream URL
    let publicStreamUrl = null;
    try {
      const { publicBase } = require('../config');
      const deviceIdHeader = req.header('x-device-id') || null;
      if (publicBase && deviceIdHeader) {
        const base = publicBase.replace(/\/$/, '');
        publicStreamUrl = `${base}/device_stream?device_id=${encodeURIComponent(deviceIdHeader)}`;
        console.log(`[upload] public stream URL for ${deviceIdHeader} -> ${publicStreamUrl}`);
      }
    } catch (e) {
      // ignore
    }

    // Quickly acknowledge receipt so the device can resume without waiting for detection
    const resp = { ok: true, queued: true };
    if (publicStreamUrl) resp.publicStreamUrl = publicStreamUrl;

    return res.json(resp);
  } catch (err) {
    console.error('Upload controller error:', err);
    // include stack if available for diagnostics
    return res.status(500).json({ error: 'internal_error', details: (err && err.message) ? err.message : String(err) });
  }
}

async function detect(req, res) {
  try {
    const buffer = extractBufferFromReq(req);
    if (!buffer) return res.status(400).json({ error: 'no_image_provided' });

    const data = await pythonClient.detect(buffer);

    // Broadcast detections optionally
    sockets.broadcast('detections', data);

    return res.json(data);
  } catch (err) {
    console.error('Detect controller error:', err.message || err);
    return res.status(500).json({ error: 'internal_error', details: err.message });
  }
}

module.exports = { upload, detect };
