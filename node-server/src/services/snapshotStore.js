let latest = null;

function setLatest(buffer, contentType = 'image/jpeg') {
  latest = {
    buf: Buffer.from(buffer),
    contentType,
    ts: Date.now()
  };
}

function getLatest() {
  if (!latest) return null;
  return latest;
}

module.exports = { setLatest, getLatest };