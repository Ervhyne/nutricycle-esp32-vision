const EventEmitter = require('events');
const emitter = new EventEmitter();
// Increase allowed listeners so many concurrent viewers don't trigger warnings
emitter.setMaxListeners(100);

let latest = null;

function setLatest(buffer, contentType = 'image/jpeg', deviceId = null) {
  latest = {
    buf: Buffer.from(buffer),
    contentType,
    ts: Date.now(),
    deviceId: deviceId || null
  };
  // emit update event with a shallow copy to avoid mutation
  try { emitter.emit('update', Object.assign({}, latest)); } catch (e) { console.warn('[snapshotStore] emit update failed', e && e.message ? e.message : e); }
}

function getLatest() {
  if (!latest) return null;
  return latest;
}

function onUpdate(cb) {
  emitter.on('update', cb);
  // debug: emitter.listenerCount('update') may be helpful when diagnosing leaks
}

function offUpdate(cb) {
  emitter.removeListener('update', cb);
}

function listenerCount() {
  return emitter.listenerCount('update');
}

module.exports = { setLatest, getLatest, onUpdate, offUpdate, listenerCount };