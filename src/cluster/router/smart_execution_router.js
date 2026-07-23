const EventEmitter = require('events');

const ROUTE_TARGETS = { LOCAL_CPU: 'LOCAL_CPU', REMOTE_NODE: 'REMOTE_NODE', GPU_ACCELERATED: 'GPU_ACCELERATED' };

class SmartExecutionRouter extends EventEmitter {
  constructor(options = {}) {
    super();
    this._nodeRegistry = options.nodeRegistry || null;
    this._gpuMinBytes = parseInt(options.gpuMinBytes || process.env.SMART_ROUTE_GPU_MIN_BYTES || '1048576', 10);
    this._maxLatencyMs = parseInt(options.maxLatencyMs || process.env.SMART_ROUTE_MAX_LATENCY_MS || '15', 10);
    this._latencyCache = new Map();
    this._gpuPipelines = new Set();
    this._localCpuLoad = 0;
  }

  configure(key, value) {
    if (key === 'SMART_ROUTE_GPU_MIN_BYTES') {
      const v = parseInt(value, 10);
      if (v >= 65536 && v <= 1073741824) this._gpuMinBytes = v;
    }
    if (key === 'SMART_ROUTE_MAX_LATENCY_MS') {
      const v = parseFloat(value);
      if (v >= 1 && v <= 100) this._maxLatencyMs = v;
    }
  }

  registerGpuPipeline(pipelineId) {
    this._gpuPipelines.add(pipelineId);
    this.emit('gpu:registered', { pipelineId });
  }

  unregisterGpuPipeline(pipelineId) {
    this._gpuPipelines.delete(pipelineId);
    this.emit('gpu:unregistered', { pipelineId });
  }

  hasGpuPipeline() {
    return this._gpuPipelines.size > 0;
  }

  updateLocalCpuLoad(load) {
    this._localCpuLoad = Math.max(0, Math.min(1, load));
  }

  measureLatency(nodeId) {
    const cached = this._latencyCache.get(nodeId);
    if (cached && (Date.now() - cached.timestamp < 5000)) return cached.latency;
    const latency = Math.random() * 30;
    this._latencyCache.set(nodeId, { latency, timestamp: Date.now() });
    return latency;
  }

  setLatency(nodeId, latency) {
    this._latencyCache.set(nodeId, { latency, timestamp: Date.now() });
  }

  estimatePayloadSize(payload) {
    if (payload === null || payload === undefined) return 0;
    if (typeof payload === 'number' || typeof payload === 'boolean') return 8;
    if (typeof payload === 'string') return Buffer.byteLength(payload);
    if (Array.isArray(payload)) {
      if (payload.length === 0) return 0;
      const sample = payload[0];
      const elemSize = typeof sample === 'number' ? 8 : typeof sample === 'string' ? Buffer.byteLength(sample) : 64;
      return payload.length * elemSize;
    }
    if (typeof payload === 'object') {
      try { return Buffer.byteLength(JSON.stringify(payload)); }
      catch (e) { return 1024; }
    }
    return 64;
  }

  isMatrixOrVectorOp(action) {
    const vecKeywords = ['mat', 'vec', 'matrix', 'vector', 'dot', 'cross', 'multiply', 'transpose', 'convolution', 'fft', 'tensor'];
    const lower = action.toLowerCase();
    return vecKeywords.some(k => lower.includes(k));
  }

  selectTarget(action, payload) {
    const payloadSize = this.estimatePayloadSize(payload);
    const isVectorOp = this.isMatrixOrVectorOp(action);
    const start = Date.now();

    if (isVectorOp && payloadSize >= this._gpuMinBytes && this.hasGpuPipeline()) {
      const elapsed = Date.now() - start;
      this.emit('router:decision', { target: ROUTE_TARGETS.GPU_ACCELERATED, reason: 'GPU threshold exceeded', payloadSize, elapsedMs: elapsed });
      return { target: ROUTE_TARGETS.GPU_ACCELERATED, reason: 'GPU threshold exceeded', payloadSize, elapsedMs: elapsed };
    }

    if (this._nodeRegistry) {
      const alive = this._nodeRegistry.getAliveNodes();
      if (alive.length > 0 && this._localCpuLoad > 0.7) {
        const candidates = alive.filter(n => {
          const l = this.measureLatency(n.id);
          return l < this._maxLatencyMs;
        });
        if (candidates.length > 0) {
          const selected = candidates.reduce((a, b) => {
            const la = this.measureLatency(a.id);
            const lb = this.measureLatency(b.id);
            return la <= lb ? a : b;
          });
          const elapsed = Date.now() - start;
          this.emit('router:decision', { target: ROUTE_TARGETS.REMOTE_NODE, nodeId: selected.id, reason: 'local CPU overload, remote available', payloadSize, elapsedMs: elapsed });
          return { target: ROUTE_TARGETS.REMOTE_NODE, nodeId: selected.id, reason: 'local CPU overload, remote available', payloadSize, elapsedMs: elapsed };
        }
      }
    }

    const elapsed = Date.now() - start;
    this.emit('router:decision', { target: ROUTE_TARGETS.LOCAL_CPU, reason: 'default target', payloadSize, elapsedMs: elapsed });
    return { target: ROUTE_TARGETS.LOCAL_CPU, reason: 'default target', payloadSize, elapsedMs: elapsed };
  }

  async route(action, payload, options = {}) {
    const decision = this.selectTarget(action, payload, options);
    if (decision.elapsedMs !== undefined && decision.elapsedMs > 0.05) {
      this.emit('router:overhead_warning', { elapsedMs: decision.elapsedMs, action });
    }

    if (decision.target === ROUTE_TARGETS.GPU_ACCELERATED) {
      return { ...decision, result: 'gpu_dispatched' };
    }
    if (decision.target === ROUTE_TARGETS.REMOTE_NODE) {
      return { ...decision, result: 'remote_dispatched' };
    }
    return { ...decision, result: 'local_executed' };
  }

  getStats() {
    return {
      gpuMinBytes: this._gpuMinBytes,
      maxLatencyMs: this._maxLatencyMs,
      gpuPipelines: this._gpuPipelines.size,
      localCpuLoad: this._localCpuLoad,
      latencyCacheSize: this._latencyCache.size,
    };
  }
}

module.exports = { SmartExecutionRouter, ROUTE_TARGETS };
