const EventEmitter = require('events');

class DistributedCycleEngine extends EventEmitter {
  constructor(options = {}) {
    super();
    this._nodeRegistry = options.nodeRegistry || null;
    this._coreFactor = parseInt(options.coreFactor || process.env.CYCLE_CORE_FACTOR || '2', 10);
    this._minChunkSize = parseInt(options.minChunkSize || process.env.CYCLE_MIN_CHUNK_SIZE || '1000', 10);
    this._workerTimeoutMs = parseInt(options.workerTimeoutMs || process.env.WORKER_TIMEOUT_MS || '5000', 10);
    this._workerChunks = new Map();
    this._chunkResults = new Map();
    this._pendingChunks = [];
    this._chunkIdCounter = 0;
  }

  configure(key, value) {
    if (key === 'CYCLE_CORE_FACTOR') {
      const v = parseInt(value, 10);
      if (v >= 1 && v <= 8) this._coreFactor = v;
    }
    if (key === 'CYCLE_MIN_CHUNK_SIZE') {
      const v = parseInt(value, 10);
      if (v >= 100 && v <= 100000) this._minChunkSize = v;
    }
    if (key === 'WORKER_TIMEOUT_MS') {
      const v = parseInt(value, 10);
      if (v >= 1000 && v <= 60000) this._workerTimeoutMs = v;
    }
  }

  _getWorkers() {
    if (!this._nodeRegistry) return [];
    return this._nodeRegistry.getAliveNodes();
  }

  computeChunkSize(totalIterations) {
    const workers = this._getWorkers();
    const activeCount = Math.max(workers.length, 1);
    const chunkSize = Math.max(this._minChunkSize, Math.ceil(totalIterations / (activeCount * this._coreFactor)));
    return chunkSize;
  }

  scatter(totalIterations) {
    const chunkSize = this.computeChunkSize(totalIterations);
    const workers = this._getWorkers();
    this._pendingChunks = [];
    this._chunkResults = new Map();
    this._workerChunks = new Map();
    let remaining = totalIterations;
    let offset = 0;
    this._totalChunksCreated = 0;
    while (remaining > 0) {
      const size = Math.min(chunkSize, remaining);
      const chunkId = this._chunkIdCounter++;
      this._pendingChunks.push({ chunkId, offset, size, totalIterations });
      offset += size;
      remaining -= size;
      this._totalChunksCreated++;
    }
    this.emit('cycle:scattered', { totalChunks: this._totalChunksCreated, chunkSize, workers: workers.length });
    if (workers.length > 0) {
      this._assignInitialChunks(workers);
    }
    return { totalChunks: this._totalChunksCreated, chunkSize, pending: this._pendingChunks.length };
  }

  _assignInitialChunks(workers) {
    const assigned = [];
    for (let i = 0; i < this._pendingChunks.length && i < workers.length; i++) {
      const worker = workers[i % workers.length];
      const chunk = this._pendingChunks[i];
      this._assignChunk(worker, chunk);
      assigned.push({ chunkId: chunk.chunkId, workerId: worker.id });
    }
    for (const a of assigned) {
      this._pendingChunks = this._pendingChunks.filter(c => c.chunkId !== a.chunkId);
    }
    this.emit('cycle:initial_assignment', { assigned });
  }

  _assignChunk(worker, chunk) {
    if (!this._workerChunks.has(worker.id)) this._workerChunks.set(worker.id, []);
    this._workerChunks.get(worker.id).push({ ...chunk, assignedAt: Date.now() });
    this.emit('chunk:assigned', { workerId: worker.id, chunkId: chunk.chunkId, offset: chunk.offset, size: chunk.size });
  }

  completeChunk(workerId, chunkId, result) {
    const workerChunks = this._workerChunks.get(workerId);
    if (!workerChunks) return false;
    const idx = workerChunks.findIndex(c => c.chunkId === chunkId);
    if (idx === -1) return false;
    workerChunks.splice(idx, 1);
    this._chunkResults.set(chunkId, result);
    this.emit('chunk:completed', { workerId, chunkId, resultSize: typeof result === 'string' ? result.length : JSON.stringify(result).length });
    this._trySteal(workerId);
    return true;
  }

  _trySteal(workerId) {
    if (this._pendingChunks.length === 0) return false;
    const chunk = this._pendingChunks.shift();
    const worker = this._getWorkers().find(w => w.id === workerId);
    if (worker) {
      this._assignChunk(worker, chunk);
      this.emit('chunk:stolen', { workerId, chunkId: chunk.chunkId });
      return true;
    }
    this._pendingChunks.unshift(chunk);
    return false;
  }

  checkTimeouts() {
    const now = Date.now();
    const reQueued = [];
    for (const [workerId, chunks] of this._workerChunks) {
      const timedOut = chunks.filter(c => now - c.assignedAt >= this._workerTimeoutMs);
      for (const chunk of timedOut) {
        this._workerChunks.set(workerId, chunks.filter(c => c.chunkId !== chunk.chunkId));
        this._pendingChunks.push({ chunkId: chunk.chunkId, offset: chunk.offset, size: chunk.size, totalIterations: chunk.totalIterations });
        reQueued.push({ chunkId: chunk.chunkId, workerId });
        this.emit('chunk:timeout', { workerId, chunkId: chunk.chunkId, elapsedMs: now - chunk.assignedAt });
      }
    }
    if (reQueued.length > 0) {
      this._reassignPending();
    }
    return reQueued;
  }

  _reassignPending() {
    const workers = this._getWorkers();
    if (workers.length === 0) return;
    for (const worker of workers) {
      this._trySteal(worker.id);
    }
  }

  getPendingChunkCount() {
    return this._pendingChunks.length;
  }

  getActiveChunkCount() {
    let count = 0;
    for (const chunks of this._workerChunks.values()) count += chunks.length;
    return count;
  }

  getCompletedChunkCount() {
    return this._chunkResults.size;
  }

  getChunkResults() {
    const results = Array.from(this._chunkResults.values());
    return results;
  }

  getWorkerLoad() {
    const load = {};
    for (const [workerId, chunks] of this._workerChunks) {
      load[workerId] = chunks.length;
    }
    return load;
  }

  isComplete(totalIterations) {
    const chunkSize = this.computeChunkSize(totalIterations);
    const totalChunks = Math.ceil(totalIterations / chunkSize);
    return this._chunkResults.size >= totalChunks && this._pendingChunks.length === 0 && this.getActiveChunkCount() === 0;
  }

  getStats() {
    return {
      coreFactor: this._coreFactor,
      minChunkSize: this._minChunkSize,
      workerTimeoutMs: this._workerTimeoutMs,
      pendingChunks: this._pendingChunks.length,
      activeChunks: this.getActiveChunkCount(),
      completedChunks: this._chunkResults.size,
      workers: this._getWorkers().length,
    };
  }
}

module.exports = { DistributedCycleEngine };
