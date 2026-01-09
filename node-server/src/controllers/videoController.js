const axios = require('axios');
const { pythonBaseUrl } = require('../config');

async function proxyVideoFeed(req, res) {
  try {
    // Preserve query string if present (e.g. ?t=timestamp)
    const qs = req.url && req.url.includes('?') ? req.url.slice(req.url.indexOf('?')) : '';
    const target = `${pythonBaseUrl}/video_feed${qs}`;

    console.log(`[videoProxy] ${req.ip || req.connection.remoteAddress} -> ${target}`);

    const resp = await axios.get(target, {
      responseType: 'stream',
      timeout: 60000,
      headers: { Accept: 'multipart/x-mixed-replace' }
    });

    // Forward relevant headers (at minimum content-type)
    if (resp.headers && resp.headers['content-type']) {
      res.setHeader('Content-Type', resp.headers['content-type']);
    } else {
      res.setHeader('Content-Type', 'multipart/x-mixed-replace; boundary=frame');
    }

    // Pipe the streaming data through to the client
    resp.data.on('data', chunk => {
      try { res.write(chunk); } catch (e) { /* ignore write errors */ }
    });

    resp.data.on('end', () => {
      try { res.end(); } catch (e) { /* ignore */ }
    });

    resp.data.on('error', err => {
      console.error('[videoProxy] stream error', err && err.message ? err.message : err);
      try { res.end(); } catch (e) { /* ignore */ }
    });
  } catch (err) {
    console.error('[videoProxy] failed to proxy video_feed', err && err.message ? err.message : err);
    if (!res.headersSent) {
      res.status(502).json({ error: 'bad_gateway', details: err.message || String(err) });
    } else {
      try { res.end(); } catch (e) { /* ignore */ }
    }
  }
}

module.exports = { proxyVideoFeed };