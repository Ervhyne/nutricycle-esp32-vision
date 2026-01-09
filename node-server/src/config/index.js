const dotenv = require('dotenv');
dotenv.config();

// Derive a base URL for the Python server from the full detect endpoint
const pythonDetectUrl = process.env.PYTHON_DETECT_URL || 'http://localhost:5000/detect';
const pythonBaseUrl = pythonDetectUrl.replace(/\/detect\/?$/, '');

module.exports = {
  port: process.env.PORT || 3000,
  pythonDetectUrl,
  pythonBaseUrl,
  apiKey: process.env.API_KEY || null,
  maxBodySize: process.env.MAX_BODY_SIZE || '5mb'
};
