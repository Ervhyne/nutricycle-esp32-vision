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

// Normalize an IP string that may be IPv6-mapped IPv4 (e.g. ::ffff:192.168.1.15)
function normalizeIp(ip) {
  if (!ip) return ip;
  // strip common 'ip::' prefixes used in some keys
  const stripped = ip.replace(/^ip::+/, '');
  const m = stripped.match(/::ffff:(\d+\.\d+\.\d+\.\d+)$/i);
  if (m) return m[1];
  return stripped;
}

// Normalize a URL by converting IPv6-mapped IPv4 host to plain IPv4 (so it stores as http://192.168.1.15:81/...)
function normalizeUrl(u) {
  if (!u) return u;
  try {
    // Try parsing directly first
    let parsed;
    try { parsed = new URL(u); } catch (e) {
      // If parsing fails (likely because host contains unbracketed IPv6 literal),
      // try to bracket the IPv6-style host (e.g. ::ffff:1.2.3.4)
      const bracketed = u.replace(/^([a-z]+:\/\/)(::ffff:[0-9.]+)(:\d+)?(\/.*|$)/i, (m, scheme, host, port, rest) => {
        return `${scheme}[${host}]${port || ''}${rest || ''}`;
      });
      parsed = new URL(bracketed);
    }

    const host = parsed.hostname || '';
    const m = host.match(/^::ffff:(\d+\.\d+\.\d+\.\d+)$/i);
    if (m) {
      const ipv4 = m[1];
      const port = parsed.port ? `:${parsed.port}` : '';
      return `${parsed.protocol}//${ipv4}${port}${parsed.pathname || ''}${parsed.search || ''}`;
    }
  } catch (e) {
    // Fallback: if hostname couldn't be parsed, do a safe textual replacement for ::ffff:ipv4 patterns
    try {
      const replaced = u.replace(/::ffff:(\d+\.\d+\.\d+\.\d+)/, '$1');
      return replaced;
    } catch (e2) {
      return u;
    }
  }
  return u;
}

function load() {
  try {
    if (!fs.existsSync(DATA_FILE)) return;
    const raw = fs.readFileSync(DATA_FILE, 'utf8');
    const parsed = JSON.parse(raw);
    if (parsed && parsed.data) {
      let migrated = false;
      for (const k of Object.keys(parsed.data)) {
        const entry = parsed.data[k] || {};
        let newKey = k;
        // normalize ip keys like 'ip:::ffff:192.168.1.15' -> 'ip:192.168.1.15'
        if (k.startsWith('ip:')) {
          const rawIp = k.slice(3);
          const cleanIp = normalizeIp(rawIp);
          newKey = `ip:${cleanIp}`;
        }
        const newUrl = entry.url ? normalizeUrl(entry.url) : entry.url;
        store.set(newKey, { url: newUrl, lastSeen: entry.lastSeen || Date.now() });
        if (newKey !== k || (entry.url && newUrl !== entry.url)) migrated = true;
      }
      lastSaved = parsed.saved || Date.now();
      // persist normalized/migrated data back to disk for cleanliness
      if (migrated) {
        try {
          console.log('[deviceStreamStore] migrating stored entries to normalized form');
          persist();
        } catch (e) {}
      }
    }
  } catch (e) {
    console.error('[deviceStreamStore] load failed', e && e.message ? e.message : e);
  }
}

function set(deviceId, url) {
  if (!deviceId || !url) return false;
  const normalizedUrl = normalizeUrl(url);
  store.set(deviceId, { url: normalizedUrl, lastSeen: Date.now() });
  persist();
  return true;
}

function get(deviceId) {
  const v = store.get(deviceId);
  return v ? v.url : null;
}

function setByIp(ip, url) {
  const clean = normalizeIp(ip);
  return set(`ip:${clean}`, url);
}

function getByIp(ip) {
  const clean = normalizeIp(ip);
  return get(`ip:${clean}`);
}

function setRssiForKey(key, rssi) {
  if (!key) return false;
  const v = store.get(key);
  if (!v) return false;
  v.rssi = typeof rssi === 'string' ? Number(rssi) : rssi;
  v.lastSeen = Date.now();
  persist();
  return true;
}

function setRssiByIp(ip, rssi) {
  const clean = normalizeIp(ip);
  return setRssiForKey(`ip:${clean}`, rssi);
}

function setRssiByDeviceId(id, rssi) {
  return setRssiForKey(id, rssi);
}

function list() {
  const out = {};
  for (const [k, v] of store.entries()) {
    out[k] = { url: v.url, lastSeen: v.lastSeen, rssi: v.rssi };
  }
  return out;
}

function getStatus() {
  let size = 0;
  try { size = fs.existsSync(DATA_FILE) ? fs.statSync(DATA_FILE).size : 0; } catch (e) {}
  return { lastSaved, size, entries: store.size };
}

// Run a one-shot normalization/migration on existing in-memory entries and persist
function normalizeAll() {
  let changed = 0;
  const entries = Array.from(store.entries());
  store.clear();
  for (const [k, v] of entries) {
    let newKey = k;
    if (k.startsWith('ip:')) {
      const rawIp = k.slice(3);
      const cleanIp = normalizeIp(rawIp);
      newKey = `ip:${cleanIp}`;
    }
    const newUrl = v && v.url ? normalizeUrl(v.url) : v.url;
    store.set(newKey, { url: newUrl, lastSeen: v && v.lastSeen ? v.lastSeen : Date.now() });
    if (newKey !== k || (v && v.url && newUrl !== v.url)) changed++;
  }
  if (changed > 0) persist();
  return { changed };
}

// initialize
load();

module.exports = { set, get, setByIp, getByIp, list, getStatus, normalizeAll, setRssiForKey, setRssiByIp, setRssiByDeviceId };
