const axios = require('axios');
const { pythonDetectUrl } = require('../config');

async function detect(buffer, timeout = 20000) {
  try {
    // Diagnostics: log attempt
    try { console.log(`[pythonClient] calling ${pythonDetectUrl} with ${buffer ? buffer.length : 0} bytes (timeout=${timeout}ms)`); } catch (e) {}

    const resp = await axios.post(pythonDetectUrl, buffer, {
      headers: { 'Content-Type': 'application/octet-stream' },
      timeout
    });
    return resp.data;
  } catch (err) {
    // Surface useful information for debugging
    if (err.response) {
      // Server responded with a status outside 2xx
      const status = err.response.status;
      const data = err.response.data;
      throw new Error(`Python detect error: status=${status}, data=${JSON.stringify(data)}`);
    } else if (err.request) {
      // No response received
      throw new Error(`Python detect no response: ${err.message}`);
    } else {
      // Other errors
      throw new Error(`Python detect error: ${err.message}`);
    }
  }
}

module.exports = { detect };
