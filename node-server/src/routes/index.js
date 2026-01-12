const express = require('express');
const router = express.Router();
const multer = require('multer');
const upload = multer({ storage: multer.memoryStorage() });

const axios = require('axios');
const { pythonBaseUrl } = require('../config');
const { uploadLimiter } = require('../middleware/limiter');
const requireApiKey = require('../middleware/auth');
const detectController = require('../controllers/detectController');
const videoController = require('../controllers/videoController');

// Helper to bracket IPv6 host literals so URLs like http://::ffff:192.168.1.15:81/stream become valid
function ensureIpv6Brackets(u) {
  try {
    if (!u || u.indexOf('[') !== -1) return u
    // Use the WHATWG URL parser to avoid consuming the port into the hostname.
    const parsed = new URL(u)
    // If hostname contains a colon (IPv6 literal) and isn't already bracketed, bracket it.
    if (parsed.hostname && parsed.hostname.includes(':') && !parsed.hostname.startsWith('[')) {
      const host = `[${parsed.hostname}]` + (parsed.port ? `:${parsed.port}` : '')
      return `${parsed.protocol}//${host}${parsed.pathname || ''}${parsed.search || ''}`
    }
    return u
  } catch (e) { return u }
}

router.get('/health', (req, res) => res.json({ status: 'ok', gateway: true }));

// Convenience viewer route
router.get('/cam', (req, res) => res.redirect('/cam.html'));

// Proxy Python root status (detection_active, frame_count, etc.)
router.get('/python_status', async (req, res) => {
  try {
    const resp = await axios.get(`${pythonBaseUrl}/`);
    return res.json(resp.data);
  } catch (err) {
    console.error('[proxy] python_status failed', err && err.message ? err.message : err);
    return res.status(502).json({ error: 'bad_gateway', details: err.message || String(err) });
  }
});

// Video feed proxy (MJPEG) - Node will proxy `/video_feed` to the local Python server
router.get('/video_feed', videoController.proxyVideoFeed);

// Device stream proxy - direct ESP32 stream forwarding for testing and direct device mode
router.get('/device_stream', async (req, res) => {
  try {
    // Allow optional override via ?url=<encoded_url> for testing/viewer convenience
    const override = req.query && req.query.url ? decodeURIComponent(req.query.url) : null;
    // Default env (for configured devices reachable by IP or hostname), fallback to AP for direct testing
    const defaultEnv = process.env.ESP32_STREAM_URL || null;
    const apDefault = 'http://192.168.4.1:81/stream';

    // Look up per-device mapping first (device_id param or by provided ip mapping)
    const deviceStreamStore = require('../services/deviceStreamStore');
    let espUrl = null;

    if (override) {
      if (!/^https?:\/\/.+/.test(override)) return res.status(400).json({ error: 'invalid_url' });
      espUrl = ensureIpv6Brackets(override);
    }

    if (!espUrl && req.query && req.query.device_id) {
      espUrl = deviceStreamStore.get(req.query.device_id) || null;
      if (espUrl) {
        espUrl = ensureIpv6Brackets(espUrl);
        console.log(`[deviceStreamProxy] resolving stream for device_id=${req.query.device_id} -> ${espUrl}`);
      }
    }

    // If no device-specific mapping, try mapping by `target_ip` query param or by stored mapping for that ip
    if (!espUrl && req.query && req.query.target_ip) {
      const byIp = deviceStreamStore.getByIp(req.query.target_ip);
      if (byIp) {
        espUrl = ensureIpv6Brackets(byIp);
        console.log(`[deviceStreamProxy] resolved stream by target_ip=${req.query.target_ip} -> ${espUrl}`);
      } else {
        // guess URL on same host
        // strip any leading 'ip::' prefix that may be present in id-like forms
        const cleaned = req.query.target_ip.replace(/^ip::+/, '');
        espUrl = `http://${cleaned}:81/stream`;
        console.log(`[deviceStreamProxy] guessed stream for target_ip=${req.query.target_ip} -> ${espUrl}`);
      }
    }

    // If still not found, try env default
    if (!espUrl && defaultEnv) {
      espUrl = ensureIpv6Brackets(defaultEnv);
      console.log(`[deviceStreamProxy] using env ESP32_STREAM_URL -> ${espUrl}`);
    }

    // Finally fall back to AP default for direct testing
    if (!espUrl) {
      espUrl = apDefault;
      console.log(`[deviceStreamProxy] falling back to AP default -> ${espUrl}`);
    }

    // Filter out our own `url`, `device_id`, `target_ip` params before forwarding
    const qs = req.url && req.url.includes('?') ? req.url.slice(req.url.indexOf('?')) : '';
    let qsFiltered = qs.replace(/(\?|&)?url=[^&]*/g, '').replace(/(\?|&)?device_id=[^&]*/g, '').replace(/(\?|&)?target_ip=[^&]*/g, '').replace(/^\?&/, '?');
    // If the filtered query string starts with '&', convert to '?' so concatenation produces a valid query string
    if (qsFiltered && qsFiltered.charAt(0) === '&') qsFiltered = '?' + qsFiltered.slice(1);

    const target = `${espUrl}${qsFiltered}`;
    console.log(`[deviceStreamProxy] ${req.ip || req.connection.remoteAddress} -> ${target}`);

    const resp = await axios.get(target, {
      responseType: 'stream',
      timeout: 0,
      headers: { Accept: 'multipart/x-mixed-replace' }
    });

    const contentType = resp.headers && resp.headers['content-type'] ? resp.headers['content-type'] : 'multipart/x-mixed-replace; boundary=frame';
    res.setHeader('Content-Type', contentType);

    // Diagnostic logging: show upstream status and content-type
    console.log(`[deviceStreamProxy] upstream status=${resp.status} content-type=${contentType}`);

    // Pipe directly — keeps MJPEG frames intact and minimal overhead
    resp.data.pipe(res);

    resp.data.on('error', err => {
      console.error('[deviceStreamProxy] stream error', err && err.message ? err.message : err);
      try { res.end(); } catch (e) {}
    });

    resp.data.on('close', () => {
      console.log('[deviceStreamProxy] upstream stream closed')
    });
  } catch (err) {
    // Detailed error logging to help diagnose network / device issues
    if (err && err.response) {
      const status = err.response.status;
      let body = '(no body)';
      try { body = typeof err.response.data === 'string' ? err.response.data.slice(0,500) : JSON.stringify(err.response.data).slice(0,500); } catch (e) { body = '(unable to read response body)'; }
      console.error(`[deviceStreamProxy] upstream response error: status=${status} body=${body}`);
    } else {
      console.error('[deviceStreamProxy] request error', err && err.message ? err.message : err);
    }

    if (!res.headersSent) {
      res.status(502).json({ error: 'bad_gateway', details: err && err.message ? err.message : String(err) });
    } else {
      try { res.end(); } catch (e) {}
    }
  }
});

// Probe device stream (test reachability and content-type)
router.get('/probe_stream', async (req, res) => {
  try {
    const override = req.query && req.query.url ? decodeURIComponent(req.query.url) : null;
    const target_ip = req.query && req.query.target_ip ? req.query.target_ip : null;
    let target = null;

    if (override) {
      if (!/^https?:\/\//.test(override)) return res.status(400).json({ error: 'invalid_url' });
      target = ensureIpv6Brackets(override);
    } else if (target_ip) {
      // strip any leading 'ip::' prefix that may be present
      const cleaned = target_ip.replace(/^ip::+/, '');
      target = `http://${cleaned}:81/stream`;
    } else if (process.env.ESP32_STREAM_URL) {
      target = ensureIpv6Brackets(process.env.ESP32_STREAM_URL);
    } else {
      return res.status(400).json({ error: 'missing_target' });
    }

    console.log(`[probe_stream] probing ${target}`);
    const resp = await axios.get(target, { method: 'GET', responseType: 'stream', timeout: 5000, headers: { Accept: 'multipart/x-mixed-replace' } });
    const ct = resp.headers['content-type'] || 'unknown';
    return res.json({ ok: true, target, status: resp.status, contentType: ct });
  } catch (err) {
    if (err && err.response) {
      return res.status(502).json({ ok: false, error: 'upstream_response', status: err.response.status, body: (typeof err.response.data === 'string' ? err.response.data.slice(0,500) : '(non-string body)') });
    }
    return res.status(502).json({ ok: false, error: 'request_error', details: err && err.message ? err.message : String(err) });
  }
});

// Snapshot endpoint for mobile/clients (returns latest uploaded frame)
const snapshotService = require('../services/snapshotStore');
router.get('/snapshot', (req, res) => {
  const latest = snapshotService.getLatest();
  if (!latest) return res.status(404).json({ error: 'no_snapshot' });
  res.setHeader('Content-Type', latest.contentType);
  res.setHeader('Cache-Control', 'no-cache, no-store, must-revalidate');
  return res.send(latest.buf);
});

// Device stream registration endpoints
const deviceStreamStore = require('../services/deviceStreamStore');

// Register stream URL for a device (requires API key)
router.post('/devices/:id/register_stream', requireApiKey, async (req, res) => {
  try {
    const id = req.params.id;
    const url = req.body && req.body.url;
    if (!id || !url || !/^https?:\/\//.test(url)) return res.status(400).json({ error: 'invalid' });
    deviceStreamStore.set(id, url);
    return res.json({ ok: true, id, url });
  } catch (err) {
    console.error('[devices/register_stream] failed', err && err.message ? err.message : err);
    return res.status(500).json({ error: 'internal_error' });
  }
});

router.get('/devices/:id/stream', requireApiKey, (req, res) => {
  const id = req.params.id;
  const url = deviceStreamStore.get(id);
  if (!url) return res.status(404).json({ error: 'not_found' });
  return res.json({ id, url });
});

// Simple listing (admin) - requires API key
router.get('/devices', requireApiKey, (req, res) => {
  return res.json(deviceStreamStore.list());
});

// Status endpoint for device stream persistence
router.get('/devices/status', requireApiKey, (req, res) => {
  try {
    const status = deviceStreamStore.getStatus ? deviceStreamStore.getStatus() : { entries: Object.keys(deviceStreamStore.list()).length };
    return res.json({ ok: true, status });
  } catch (err) {
    console.error('[devices/status] failed', err && err.message ? err.message : err);
    return res.status(500).json({ error: 'internal_error' });
  }
});

// Admin: normalize and migrate stored device entries (fix ::ffff and ip:: prefixes)
// Requires API key
router.post('/devices/normalize', requireApiKey, (req, res) => {
  try {
    const result = deviceStreamStore.normalizeAll ? deviceStreamStore.normalizeAll() : { changed: 0 };
    return res.json({ ok: true, result });
  } catch (err) {
    console.error('[devices/normalize] failed', err && err.message ? err.message : err);
    return res.status(500).json({ error: 'internal_error' });
  }
});

// HLS endpoints
const hlsManager = require('../services/hlsManager');

router.get('/hls/available', (req, res) => {
  return res.json({ ok: true, available: hlsManager.ffmpegAvailable() });
});

// Start HLS on demand for a given url, target_ip or device_id. Returns { ok:true, id, playlist }
router.get('/hls', (req, res) => {
  try {
    const override = req.query && req.query.url ? decodeURIComponent(req.query.url) : null;
    const target_ip = req.query && req.query.target_ip ? req.query.target_ip : null;
    const device_id = req.query && req.query.device_id ? req.query.device_id : null;
    let target = null;
    if (override) target = override;
    else if (device_id) {
      const url = deviceStreamStore.get(device_id);
      if (!url) return res.status(404).json({ error: 'not_found' });
      target = url;
    } else if (target_ip) {
      const byIp = deviceStreamStore.getByIp(target_ip);
      if (byIp) target = byIp;
      else target = `http://${target_ip}:81/stream`;
    } else return res.status(400).json({ error: 'missing_target' });

    if (!/^https?:\/\//.test(target)) return res.status(400).json({ error: 'invalid_url' });
    const entry = hlsManager.start(target, { width: 320, height: 240, bitrate: '400k', segmentTime: 1 });
    const playlist = `/hls/${entry.id}/index.m3u8?ts=${Date.now()}`;
    return res.json({ ok: true, id: entry.id, playlist });
  } catch (err) {
    console.error('[hls] start failed', err && err.message ? err.message : err);
    return res.status(500).json({ ok: false, error: 'internal_error' });
  }
});

// Serve playlist and segments from tmp dir
router.get('/hls/:id/index.m3u8', (req, res) => {
  try {
    const id = req.params.id;
    const entry = hlsManager.getEntry(id);
    if (!entry) return res.status(404).json({ error: 'not_found' });
    hlsManager.getEntry(id).lastActive = Date.now();
    const file = path.join(entry.dir, 'index.m3u8');
    if (!fs.existsSync(file)) return res.status(404).json({ error: 'not_ready' });
    return res.sendFile(file);
  } catch (err) {
    console.error('[hls] playlist failed', err && err.message ? err.message : err);
    return res.status(500).json({ error: 'internal_error' });
  }
});

router.get('/hls/:id/:segment', (req, res) => {
  try {
    const id = req.params.id;
    const seg = req.params.segment;
    const entry = hlsManager.getEntry(id);
    if (!entry) return res.status(404).json({ error: 'not_found' });
    hlsManager.getEntry(id).lastActive = Date.now();
    const file = path.join(entry.dir, seg);
    if (!fs.existsSync(file)) return res.status(404).json({ error: 'not_found' });
    return res.sendFile(file);
  } catch (err) {
    console.error('[hls] segment failed', err && err.message ? err.message : err);
    return res.status(500).json({ error: 'internal_error' });
  }
});

// Lightweight endpoint to return the normalized target URL without probing
router.get('/normalize_stream', (req, res) => {
  try {
    const override = req.query && req.query.url ? decodeURIComponent(req.query.url) : null;
    const target_ip = req.query && req.query.target_ip ? req.query.target_ip : null;
    let target = null;

    if (override) {
      if (!/^https?:\/\//.test(override)) return res.status(400).json({ error: 'invalid_url' });
      target = ensureIpv6Brackets(override);
    } else if (target_ip) {
      const cleaned = target_ip.replace(/^ip::+/, '');
      target = `http://${cleaned}:81/stream`;
    } else if (process.env.ESP32_STREAM_URL) {
      target = ensureIpv6Brackets(process.env.ESP32_STREAM_URL);
    } else {
      return res.status(400).json({ error: 'missing_target' });
    }

    return res.json({ ok: true, target });
  } catch (err) {
    console.error('[normalize_stream] failed', err && err.message ? err.message : err);
    return res.status(500).json({ error: 'internal_error' });
  }
});

// --- Proxy control & monitoring endpoints to the Python server ---
router.post('/start_detection', async (req, res) => {
  try {
    const resp = await axios.post(`${pythonBaseUrl}/start_detection`);
    return res.json(resp.data);
  } catch (err) {
    console.error('[proxy] start_detection failed', err && err.message ? err.message : err);
    return res.status(502).json({ error: 'bad_gateway', details: err.message || String(err) });
  }
});

router.post('/stop_detection', async (req, res) => {
  try {
    const resp = await axios.post(`${pythonBaseUrl}/stop_detection`);
    return res.json(resp.data);
  } catch (err) {
    console.error('[proxy] stop_detection failed', err && err.message ? err.message : err);
    return res.status(502).json({ error: 'bad_gateway', details: err.message || String(err) });
  }
});

router.post('/reset_statistics', async (req, res) => {
  try {
    const resp = await axios.post(`${pythonBaseUrl}/reset_statistics`);
    return res.json(resp.data);
  } catch (err) {
    console.error('[proxy] reset_statistics failed', err && err.message ? err.message : err);
    return res.status(502).json({ error: 'bad_gateway', details: err.message || String(err) });
  }
});

router.get('/statistics', async (req, res) => {
  try {
    const resp = await axios.get(`${pythonBaseUrl}/statistics`);
    return res.json(resp.data);
  } catch (err) {
    console.error('[proxy] statistics failed', err && err.message ? err.message : err);
    return res.status(502).json({ error: 'bad_gateway', details: err.message || String(err) });
  }
});

router.get('/stream_status', async (req, res) => {
  try {
    const resp = await axios.get(`${pythonBaseUrl}/stream_status`);
    return res.json(resp.data);
  } catch (err) {
    console.error('[proxy] stream_status failed', err && err.message ? err.message : err);
    return res.status(502).json({ error: 'bad_gateway', details: err.message || String(err) });
  }
});

router.post('/start_batch', async (req, res) => {
  try {
    const resp = await axios.post(`${pythonBaseUrl}/start_batch`);
    return res.json(resp.data);
  } catch (err) {
    console.error('[proxy] start_batch failed', err && err.message ? err.message : err);
    return res.status(502).json({ error: 'bad_gateway', details: err.message || String(err) });
  }
});

router.post('/end_batch', async (req, res) => {
  try {
    const resp = await axios.post(`${pythonBaseUrl}/end_batch`);
    return res.json(resp.data);
  } catch (err) {
    console.error('[proxy] end_batch failed', err && err.message ? err.message : err);
    return res.status(502).json({ error: 'bad_gateway', details: err.message || String(err) });
  }
});

// batch_history endpoint removed

// ESP32 uploads (multipart or raw bytes)
router.post('/upload', requireApiKey, uploadLimiter, upload.single('image'), detectController.upload);

// Detect proxy
router.post('/detect', requireApiKey, upload.single('image'), detectController.detect);

module.exports = router;
