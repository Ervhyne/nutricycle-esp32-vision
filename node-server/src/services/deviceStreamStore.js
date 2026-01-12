// Simple in-memory device stream registry
// key: deviceId string or ip string
// value: { url, lastSeen: timestamp }

const fs = require('fs');
const path = require('path');

const DATA_FILE = path.join(__dirname, '..', '..', 'data', 'device_streams.json');
const BACKUP_FILE = DATA_FILE + '.bak';

const store = new Map();
let lastSaved = 0;

function ensureDataDir() {
  const dir = path.dirname(DATA_FILE);
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
}

function persist() {
  try {
    ensureDataDir();
    const obj = {};
    for (const [k, v] of store.entries()) obj[k] = v;
    const tmp = DATA_FILE + '.tmp';
    fs.writeFileSync(tmp, JSON.stringify({ version: 1, data: obj, saved: Date.now() }, null, 2));
    // atomic replace
    if (fs.existsSync(DATA_FILE)) fs.renameSync(DATA_FILE, BACKUP_FILE);
    fs.renameSync(tmp, DATA_FILE);
    lastSaved = Date.now();
  } catch (e) {
    console.error('[deviceStreamStore] persist failed', e && e.message ? e.message : e);
  }
}

function load() {
  try {
    if (!fs.existsSync(DATA_FILE)) return;
    const raw = fs.readFileSync(DATA_FILE, 'utf8');
    const parsed = JSON.parse(raw);
    if (parsed && parsed.data) {
      for (const k of Object.keys(parsed.data)) {
        store.set(k, parsed.data[k]);
      }
      lastSaved = parsed.saved || Date.now();
    }
  } catch (e) {
    console.error('[deviceStreamStore] load failed', e && e.message ? e.message : e);
  }
}

function set(deviceId, url) {
  if (!deviceId || !url) return false;
  store.set(deviceId, { url, lastSeen: Date.now() });
  persist();
  return true;
}

function get(deviceId) {
  const v = store.get(deviceId);
  return v ? v.url : null;
}

function setByIp(ip, url) {
  return set(`ip:${ip}`, url);
}

function getByIp(ip) {
  return get(`ip:${ip}`);
}

function list() {
  const out = {};
  for (const [k, v] of store.entries()) {
    out[k] = { url: v.url, lastSeen: v.lastSeen };
  }
  return out;
}

function getStatus() {
  let size = 0;
  try { size = fs.existsSync(DATA_FILE) ? fs.statSync(DATA_FILE).size : 0; } catch (e) {}
  return { lastSaved, size, entries: store.size };
}

// initialize
load();

module.exports = { set, get, setByIp, getByIp, list, getStatus };
