const express = require('express');
const cors = require('cors');
const helmet = require('helmet');
const morgan = require('morgan');
const http = require('http');
const path = require('path');
const routes = require('./routes');
const sockets = require('./sockets');
const { port, maxBodySize } = require('./config');

const app = express();
app.use(helmet());
app.use(cors());
app.use(morgan('dev'));
app.use(express.json({ limit: maxBodySize }));
app.use(express.urlencoded({ extended: true, limit: maxBodySize }));
app.use(express.raw({ type: ['image/*', 'application/octet-stream'], limit: maxBodySize }));

// Serve public static files (viewer pages)
app.use(express.static(path.join(__dirname, '..', 'public')));

// Wire routes
app.use('/', routes);

// Error handler (simple)
app.use((err, req, res, next) => {
  console.error('Unhandled error:', err);
  res.status(500).json({ error: 'internal_error' });
});

const os = require('os');
const server = http.createServer(app);
const io = sockets.init(server);

function getLocalIPv4Addresses() {
  const nets = os.networkInterfaces();
  const results = [];
  for (const name of Object.keys(nets)) {
    for (const net of nets[name]) {
      // Skip over non-IPv4 and internal (i.e. 127.0.0.1)
      if (net.family === 'IPv4' && !net.internal) {
        results.push(net.address);
      }
    }
  }
  return results;
}

// Check for ffmpeg availability for optional HLS feature
try {
  const hlsManager = require('./services/hlsManager');
  if (hlsManager.ffmpegAvailable()) {
    console.log('[hls] ffmpeg found in PATH — HLS available');
  } else {
    console.warn('[hls] ffmpeg not found in PATH — HLS disabled. Install ffmpeg (apt, brew, or download) to enable HLS transcoding');
  }
} catch (e) {
  console.warn('[hls] could not verify ffmpeg availability');
}

server.listen(port, () => {
  const ips = getLocalIPv4Addresses();
  if (ips.length > 0) {
    console.log(`NutriCycle Gateway listening on port ${port} (addresses: ${ips.map(ip => `http://${ip}:${port}`).join(', ')})`);
  } else {
    console.log(`NutriCycle Gateway listening on port ${port} (no non-internal IPv4 address found)`);
  }

  console.log(`Socket.IO and HTTP server running. Python detect proxy: ${require('./config').pythonDetectUrl}`);
});

module.exports = { app, server, io };
