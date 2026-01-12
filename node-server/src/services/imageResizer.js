let sharp = null;
try {
  sharp = require('sharp');
} catch (err) {
  console.warn('[imageResizer] sharp not found; skipping resize fallback to original buffer');
}

async function resizeTo320(buffer) {
  if (!sharp) {
    // No sharp available; return original buffer
    return buffer;
  }
  return await sharp(buffer)
    .resize(320, 320, { fit: 'inside' })
    .jpeg({ quality: 70 })
    .toBuffer();
}

module.exports = { resizeTo320 };