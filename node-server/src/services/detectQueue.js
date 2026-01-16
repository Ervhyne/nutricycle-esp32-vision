const EventEmitter = require('events');
const pythonClient = require('./pythonClient');
const { DETECT_WORKERS, DETECT_QUEUE_SIZE, PYTHON_DETECT_TIMEOUT } = require('../config/detectConfig');

class DetectQueue extends EventEmitter {
  constructor() {
    super();
    this.queue = [];
    this.active = 0;
    this.workers = DETECT_WORKERS || 2;
    this.maxQueue = DETECT_QUEUE_SIZE || 50;
  }

  enqueue(buffer, opts = {}) {
    return new Promise((resolve, reject) => {
      if (this.queue.length >= this.maxQueue) {
        return reject(new Error('detect_queue_full'));
      }
      this.queue.push({ buffer, opts, resolve, reject, ts: Date.now() });
      this._maybeStartNext();
    });
  }

  _maybeStartNext() {
    // start tasks while under worker limit
    while (this.active < this.workers && this.queue.length > 0) {
      const job = this.queue.shift();
      this._runJob(job);
    }
  }

  async _runJob(job) {
    this.active++;
    this.emit('started', { ts: Date.now(), active: this.active, queueLen: this.queue.length });
    console.log(`[detectQueue] starting job active=${this.active} queued=${this.queue.length}`);
    try {
      const timeout = (job.opts && job.opts.timeout) || PYTHON_DETECT_TIMEOUT || 20000;
      const result = await pythonClient.detect(job.buffer, timeout);
      job.resolve(result);
      this.emit('done', { ts: Date.now(), active: this.active - 1, queueLen: this.queue.length, result });
      console.log(`[detectQueue] job done active=${this.active-1} queued=${this.queue.length}`);
    } catch (err) {
      job.reject(err);
      // Use a non-special event name to avoid crashing the process if no listener is attached
      this.emit('job_error', { ts: Date.now(), active: this.active - 1, queueLen: this.queue.length, error: err });
      console.warn(`[detectQueue] job error active=${this.active-1} queued=${this.queue.length} err=${err && err.message ? err.message : err}`);
    } finally {
      this.active--;
      // start another job if queued
      setImmediate(() => this._maybeStartNext());
    }
  }

  getStats() {
    return { active: this.active, queued: this.queue.length, workers: this.workers, maxQueue: this.maxQueue };
  }
}

module.exports = new DetectQueue();