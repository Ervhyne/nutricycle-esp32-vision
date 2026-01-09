const { apiKey } = require('../config');

module.exports = function requireApiKey(req, res, next) {
  if (!apiKey) return next(); // API key not configured
  const key = req.header('x-api-key') || req.query.api_key;
  if (!key || key !== apiKey) {
    return res.status(401).json({ error: 'unauthorized' });
  }
  next();
};
