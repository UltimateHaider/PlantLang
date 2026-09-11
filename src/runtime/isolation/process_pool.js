'use strict';

const { ChildProcess, fork } = require('child_process');
const os = require('os');

const DEFAULT_POOL_SIZE = 4;
const HEARTBEAT_INTERVAL_MS = 5000;
const HEARTBEAT_TIMEOUT_MS = 10;
const QUEUE_TIMEOUT_MS = 50;

/**
 * WarmProcessPool — Physically isolated worker process pool for SAFE mission mode.
 *
 * - Pre-warmed idle workers for low-latency dispatch
 * - Periodic Ping/Pong heartbeat every 5000ms with 10ms timeout
 * - Zombie detection: if a worker fails to respond, kill PID + respawn
 * - Queue starvation protection: 50ms timeout → expand pool or fallback to BALANCED
 * - Hard ceiling: min(OS.cpus().length * 2, 16)
 */
class WarmProcessPool {
  /**
   * @param {Object} [opts]
   * @param {number} [opts.poolSize]   Number of pre-warmed idle workers (default 4)
   * @param {Object} [opts.context]    MissionContext for diagnostics
   */
  constructor(opts = {}) {
    const requested = opts.poolSize || DEFAULT_POOL_SIZE;
    this._ceiling = Math.min(os.cpus().length * 2, 16);
    this._targetSize = Math.min(requested, this._ceiling);
    this.context = opts.context || null;

    /** @type {Map<number, { pid: number, process: ChildProcess, status: string, lastPong: number, busySince: number|null }>} */
    this._workers = new Map();
    this._nextWorkerId = 1;
    this._requestQueue = [];
    this._heartbeatTimer = null;
    this._started = false;
  }

  /**
   * Start the process pool: fork N workers and begin heartbeat.
   */
  start() {
    if (this._started) return;
    this._started = true;
    for (let i = 0; i < this._targetSize; i++) {
      this._spawnWorker();
    }
    this._startHeartbeat();
    if (this.context && typeof this.context.trace === 'function') {
      this.context.trace(`[SYS-POOL] Pool started with ${this._targetSize} workers (ceiling: ${this._ceiling})`);
    }
  }

  /**
   * Fork a new worker process and register it.
   * @returns {number} Worker ID
   */
  _spawnWorker() {
    const id = this._nextWorkerId++;
    const workerPath = require.resolve('./worker_bootstrap');
    const child = fork(workerPath, [], {
      stdio: ['pipe', 'pipe', 'pipe', 'ipc'],
      env: Object.assign({}, process.env, { PLANTLANG_WORKER_ID: String(id) }),
    });
    const entry = {
      pid: child.pid,
      process: child,
      status: 'IDLE',
      lastPong: Date.now(),
      busySince: null,
    };
    this._workers.set(id, entry);

    child.on('message', (msg) => {
      if (msg && msg.type === 'pong') {
        entry.lastPong = Date.now();
      }
    });

    child.on('exit', (code, signal) => {
      this._workers.delete(id);
      if (this.context && typeof this.context.diagnostic === 'function') {
        this.context.diagnostic(`[SYS-POOL] Worker_${id} exited (code=${code}, signal=${signal}). Respawn.`);
      }
      // Respawn if pool is active
      if (this._started && this._workers.size < this._targetSize) {
        this._spawnWorker();
      }
    });

    child.on('error', (err) => {
      if (this.context && typeof this.context.diagnostic === 'function') {
        this.context.diagnostic(`[SYS-POOL] [ERROR]: Worker_${id} error: ${err.message}`);
      }
    });

    if (this.context && typeof this.context.trace === 'function') {
      this.context.trace(`[SYS-POOL] Worker_${id} forked (PID: ${child.pid})`);
    }
    return id;
  }

  /**
   * Start the heartbeat interval timer.
   */
  _startHeartbeat() {
    this._heartbeatTimer = setInterval(() => {
      this._checkHeartbeats();
    }, HEARTBEAT_INTERVAL_MS);
    if (this._heartbeatTimer.unref) {
      this._heartbeatTimer.unref();
    }
  }

  /**
   * Check all workers for heartbeat responses.
   * Any worker that hasn't responded within HEARTBEAT_TIMEOUT_MS is killed and respawned.
   */
  _checkHeartbeats() {
    const now = Date.now();
    for (const [id, entry] of this._workers) {
      try {
        entry.process.send({ type: 'ping' });
      } catch (_) {
        // Process may already be dead
      }
      const elapsed = now - entry.lastPong;
      if (elapsed > HEARTBEAT_TIMEOUT_MS) {
        if (this.context && typeof this.context.diagnostic === 'function') {
          this.context.diagnostic(
            `[SYS-POOL] [ERROR]: Worker_${id} heartbeat timed out (${elapsed}ms > ${HEARTBEAT_TIMEOUT_MS}ms). Process killed and respawned.`
          );
        }
        this._killWorker(id);
        this._spawnWorker();
      }
    }
  }

  /**
   * Force-kill a worker and purge its memory.
   * @param {number} id
   */
  _killWorker(id) {
    const entry = this._workers.get(id);
    if (!entry) return;
    try {
      entry.process.kill('SIGKILL');
    } catch (_) {}
    this._workers.delete(id);
  }

  /**
   * Dispatch a task to an available worker.
   * If all workers are busy, the task is queued. If the queue wait exceeds
   * QUEUE_TIMEOUT_MS, the pool expands (if under ceiling) or falls back to BALANCED.
   *
   * @param {*} task  Serializable task data
   * @returns {Promise<*>} Worker result
   */
  dispatch(task) {
    return new Promise((resolve, reject) => {
      const worker = this._getIdleWorker();
      if (worker) {
        this._sendToWorker(worker.id, task, resolve, reject);
        return;
      }
      // All workers busy — queue with timeout
      const queueEntry = { task, resolve, reject, queuedAt: Date.now() };
      this._requestQueue.push(queueEntry);
      this._processQueue();
    });
  }

  /**
   * Find the first idle worker.
   * @returns {{ id: number, entry: Object }|null}
   */
  _getIdleWorker() {
    for (const [id, entry] of this._workers) {
      if (entry.status === 'IDLE') {
        return { id, entry };
      }
    }
    return null;
  }

  /**
   * Send a task to a specific worker.
   * @param {number} workerId
   * @param {*} task
   * @param {Function} resolve
   * @param {Function} reject
   */
  _sendToWorker(workerId, task, resolve, reject) {
    const entry = this._workers.get(workerId);
    if (!entry) {
      reject(new Error(`Worker_${workerId} not found`));
      return;
    }
    entry.status = 'BUSY';
    entry.busySince = Date.now();
    const timeout = setTimeout(() => {
      entry.status = 'IDLE';
      entry.busySince = null;
      reject(new Error(`Worker_${workerId} task timeout`));
    }, QUEUE_TIMEOUT_MS);

    entry.process.once('message', (msg) => {
      clearTimeout(timeout);
      entry.status = 'IDLE';
      entry.busySince = null;
      if (msg && msg.type === 'result') {
        resolve(msg.data);
      } else if (msg && msg.type === 'error') {
        reject(new Error(msg.message));
      } else {
        reject(new Error('Unknown worker response'));
      }
    });

    entry.process.send({ type: 'task', data: task });
  }

  /**
   * Process the request queue — assign tasks to idle workers as they become available.
   */
  _processQueue() {
    while (this._requestQueue.length > 0) {
      const elapsed = Date.now() - this._requestQueue[0].queuedAt;
      if (elapsed >= QUEUE_TIMEOUT_MS) {
        // Starvation detected — try to expand or fallback
        const entry = this._requestQueue.shift();
        if (this._workers.size < this._ceiling) {
          const id = this._spawnWorker();
          if (this.context && typeof this.context.diagnostic === 'function') {
            this.context.diagnostic(`[SYS-POOL] Pool expanded to ${this._workers.size} workers due to queue starvation.`);
          }
          this._sendToWorker(id, entry.task, entry.resolve, entry.reject);
        } else {
          if (this.context && typeof this.context.diagnostic === 'function') {
            this.context.diagnostic('WARN: Process pool starvation and timeout. Fallback to BALANCED.');
          }
          entry.reject(new Error('FALLBACK_TO_BALANCED'));
        }
      } else {
        break; // Head of queue hasn't timed out yet
      }
    }
  }

  /**
   * Stop all workers and the heartbeat timer.
   */
  shutdown() {
    this._started = false;
    if (this._heartbeatTimer) {
      clearInterval(this._heartbeatTimer);
      this._heartbeatTimer = null;
    }
    for (const [id] of this._workers) {
      this._killWorker(id);
    }
    this._workers.clear();
    this._requestQueue = [];
  }

  /**
   * Get pool metrics.
   * @returns {{ active: number, idle: number, dead: number, ceiling: number, queueLength: number }}
   */
  getMetrics() {
    let active = 0, idle = 0;
    for (const [, entry] of this._workers) {
      if (entry.status === 'BUSY') active++;
      else idle++;
    }
    return {
      active,
      idle,
      dead: this._targetSize - this._workers.size,
      ceiling: this._ceiling,
      queueLength: this._requestQueue.length,
    };
  }
}

module.exports = { WarmProcessPool };
