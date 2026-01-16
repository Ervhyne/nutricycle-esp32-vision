// Separate file for detection queue related configuration (can be overridden by env vars)
module.exports = {
  DETECT_WORKERS: Number(process.env.DETECT_WORKERS || 2),
  DETECT_QUEUE_SIZE: Number(process.env.DETECT_QUEUE_SIZE || 50),
  PYTHON_DETECT_TIMEOUT: Number(process.env.PYTHON_DETECT_TIMEOUT || 20000)
};