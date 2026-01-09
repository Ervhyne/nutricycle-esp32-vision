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
    } catch (e) {
      console.warn('[upload] diagnostics failed to log headers', e.message || e);
    }

    const data = await pythonClient.detect(buffer);

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
