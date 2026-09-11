const EventEmitter = require('events');

const REAP_MODES = { LOCAL: 'LOCAL_REAP', REMOTE: 'REMOTE_REAP' };

class ReapAggregator extends EventEmitter {
  constructor(options = {}) {
    super();
    this._mode = options.mode || 'LOCAL_REAP';
    this._remoteTarget = options.remoteTarget || process.env.REMOTE_REAP_TARGET || 'MEMORY_BUFFER';
    this._results = [];
    this._streamTargets = new Map();
  }

  configure(key, value) {
    if (key === 'REMOTE_REAP_TARGET') {
      if (value === 'MEMORY_BUFFER' || /^[a-z][a-z0-9]*:\/\//.test(value)) this._remoteTarget = value;
    }
  }

  setMode(mode) {
    if (mode === 'LOCAL_REAP' || mode === 'REMOTE_REAP') this._mode = mode;
  }

  registerStreamTarget(uri, handler) {
    this._streamTargets.set(uri, handler);
    this.emit('stream:registered', { uri });
  }

  unregisterStreamTarget(uri) {
    this._streamTargets.delete(uri);
    this.emit('stream:unregistered', { uri });
  }

  async collect(chunkResult) {
    if (this._mode === 'LOCAL_REAP') {
      return this._localCollect(chunkResult);
    }
    return this._remoteCollect(chunkResult);
  }

  async _localCollect(chunkResult) {
    this._results.push(chunkResult);
    this.emit('reap:local_collected', { resultIndex: this._results.length - 1 });
    return { collected: true, mode: 'LOCAL_REAP', totalResults: this._results.length };
  }

  async _remoteCollect(chunkResult) {
    if (this._remoteTarget === 'MEMORY_BUFFER') {
      this._results.push(chunkResult);
      this.emit('reap:remote_buffered', { resultIndex: this._results.length - 1, target: 'MEMORY_BUFFER' });
      return { collected: true, mode: 'REMOTE_REAP', target: 'MEMORY_BUFFER', totalResults: this._results.length };
    }
    const handler = this._streamTargets.get(this._remoteTarget);
    if (handler) {
      await handler(chunkResult);
      this.emit('reap:remote_streamed', { target: this._remoteTarget });
      return { collected: true, mode: 'REMOTE_REAP', target: this._remoteTarget, streamed: true };
    }
    this.emit('reap:remote_target_unavailable', { target: this._remoteTarget });
    this._results.push(chunkResult);
    return { collected: true, mode: 'REMOTE_REAP', target: this._remoteTarget, fallback: 'memory' };
  }

  reduce(reduceFn, initialValue) {
    if (this._results.length === 0) return initialValue !== undefined ? initialValue : null;
    const start = initialValue !== undefined ? initialValue : this._results[0];
    const rest = initialValue !== undefined ? this._results : this._results.slice(1);
    return rest.reduce(reduceFn, start);
  }

  merge(keyFn, mergeFn) {
    const merged = new Map();
    for (const r of this._results) {
      const key = keyFn(r);
      if (merged.has(key)) {
        merged.set(key, mergeFn(merged.get(key), r));
      } else {
        merged.set(key, r);
      }
    }
    return Array.from(merged.values());
  }

  flush() {
    const count = this._results.length;
    this._results = [];
    this.emit('reap:flushed', { count });
    return count;
  }

  getResults() {
    return this._results;
  }

  getResultCount() {
    return this._results.length;
  }

  getStats() {
    return {
      mode: this._mode,
      remoteTarget: this._remoteTarget,
      totalResults: this._results.length,
      streamTargets: this._streamTargets.size,
    };
  }
}

module.exports = { ReapAggregator, REAP_MODES };
