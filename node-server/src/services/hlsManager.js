const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');
const crypto = require('crypto');

const HLS_BASE = path.join(os.tmpdir(), 'nutricycle_hls');
const IDLE_TTL = process.env.HLS_IDLE_TTL ? Number(process.env.HLS_IDLE_TTL) : 30000; // 30s

function ensureBase() {
  if (!fs.existsSync(HLS_BASE)) fs.mkdirSync(HLS_BASE, { recursive: true });
}

function idForUrl(url) {
  const h = crypto.createHash('sha1').update(url).digest('hex');
  return h.slice(0, 12);
}

function ffmpegAvailable() {
  try {
    const proc = spawn('ffmpeg', ['-version']);
    proc.kill();
    return true;
  } catch (e) { return false }
}

const map = new Map();

function start(url, opts = {}) {
  ensureBase();
  const id = idForUrl(url);
  if (map.has(id)) {
    const entry = map.get(id);
    entry.lastActive = Date.now();
    return entry;
  }

  const dir = path.join(HLS_BASE, id);
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });

  // ffmpeg args (defaults tuned for low-latency HLS)
  const width = opts.width || 320;
  const height = opts.height || 240;
  const bitrate = opts.bitrate || '400k';
  const segTime = typeof opts.segmentTime !== 'undefined' ? opts.segmentTime : 1;
  const playlist = path.join(dir, 'index.m3u8');

  const args = [
    '-hide_banner', '-y', '-fflags', 'nobuffer', '-i', url,
    '-c:v', 'libx264', '-preset', 'veryfast', '-tune', 'zerolatency', '-b:v', bitrate, '-maxrate', '500k', '-bufsize', '1000k',
    '-vf', `scale=${width}:${height}`, '-g', '30', '-keyint_min', '30',
    '-f', 'hls', '-hls_time', `${segTime}`, '-hls_list_size', '5', '-hls_flags', 'delete_segments+append_list', playlist
  ];

  const proc = spawn('ffmpeg', args, { stdio: ['ignore', 'pipe', 'pipe'] });
  proc.stdout.on('data', d => console.log(`[hls:${id}] ${d.toString().trim()}`));
  proc.stderr.on('data', d => console.log(`[hls:${id}] ${d.toString().trim()}`));
  proc.on('exit', (code, sig) => {
    console.log(`[hls:${id}] ffmpeg exited code=${code} sig=${sig}`);
    const e = map.get(id);
    if (e) e.proc = null;
  });

  const entry = { id, url, dir, proc, lastActive: Date.now(), playlist };
  map.set(id, entry);
  return entry;
}

function touch(id) {
  const e = map.get(id);
  if (!e) return;
  e.lastActive = Date.now();
}

function stop(id) {
  const e = map.get(id);
  if (!e) return;
  try { if (e.proc) e.proc.kill('SIGTERM'); } catch (e) {}
  map.delete(id);
  // keep files for a short while; could delete if desired
}

function getEntry(id) {
  return map.get(id) || null;
}

// Idle reaper
setInterval(() => {
  const now = Date.now();
  for (const [id, e] of Array.from(map.entries())) {
    if (!e.lastActive) continue;
    if (now - e.lastActive > IDLE_TTL) {
      console.log(`[hlsManager] idle stop ${id}`);
      stop(id);
    }
  }
}, 5000);

module.exports = { start, stop, getEntry, idForUrl, ffmpegAvailable };
