const rateLimit = require('express-rate-limit');

const uploadLimiter = rateLimit({ windowMs: 5 * 1000, max: 6, message: { error: 'rate_limited' } });

module.exports = { uploadLimiter };
