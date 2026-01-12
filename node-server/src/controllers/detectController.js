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
        if (deviceIdHeader) {
          deviceStreamStore.set(deviceIdHeader, streamHeader);
          console.log(`[upload] registered stream URL for device ${deviceIdHeader} -> ${streamHeader}`);
        } else {
          // fallback: register by uploader source IP
          const ipKey = req.ip || req.connection.remoteAddress || null;
          if (ipKey) {
            deviceStreamStore.setByIp(ipKey, streamHeader);
            console.log(`[upload] registered stream URL for ip ${ipKey} -> ${streamHeader}`);
          }
        }
      } else {
        // If no explicit stream URL, and no mapping exists, we can guess a likely stream URL on the same LAN
        const ipKey = req.ip || req.connection.remoteAddress || null;
        if (ipKey && !deviceStreamStore.getByIp(ipKey)) {
          const guessed = `http://${ipKey}:81/stream`;
          deviceStreamStore.setByIp(ipKey, guessed);
          console.log(`[upload] guessed and registered stream URL for ip ${ipKey} -> ${guessed}`);
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
      snapshotStore.setLatest(resized, 'image/jpeg');
    } catch (e) {
      console.warn('[upload] snapshot store/resizer failed:', e.message || e);
      try { require('../services/snapshotStore').setLatest(buffer, 'image/jpeg'); } catch (ex) {}
    }
    const data = await pythonClient.detect(buffer); // send original to python for best accuracy

    // Broadcast detections to connected clients
    sockets.broadcast('detections', data);

    return res.json({ ok: true, detections: data.detections || [] });
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
