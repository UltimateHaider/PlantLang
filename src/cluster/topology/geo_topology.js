const EventEmitter = require('events');

class GeoTopologyManager extends EventEmitter {
  constructor(options = {}) {
    super();
    this._nodes = new Map();
    this._latencyMatrix = new Map();
    this._probeInterval = options.probeInterval || parseInt(process.env.GEO_PROBE_INTERVAL, 10) || 5000;
    this._probeTimeout = options.probeTimeout || parseInt(process.env.GEO_PROBE_TIMEOUT, 10) || 1000;
    this._timer = null;
    this._started = false;
  }

  configure(key, value) {
    const v = parseInt(value, 10);
    if (key === 'GEO_PROBE_INTERVAL' && v >= 1000 && v <= 60000) {
      this._probeInterval = v;
      if (this._started) { this.stop(); this.start(); }
    }
    if (key === 'GEO_PROBE_TIMEOUT' && v >= 100 && v <= 10000) {
      this._probeTimeout = v;
    }
  }

  registerNode(nodeId, info = {}) {
    const node = {
      id: nodeId,
      region: info.region || null,
      zone: info.zone || null,
      datacenter: info.datacenter || null,
      localityKey: info.localityKey || null,
      weight: info.weight || 1,
      alive: true,
      lastRtt: null,
    };
    this._nodes.set(nodeId, node);
    this._initNodeLatency(nodeId);
    this.emit('node:registered', { nodeId });
    return node;
  }

  unregisterNode(nodeId) {
    this._nodes.delete(nodeId);
    this._latencyMatrix.delete(nodeId);
    for (const [otherId] of this._latencyMatrix) {
      this._latencyMatrix.get(otherId).delete(nodeId);
    }
    this.emit('node:unregistered', { nodeId });
  }

  _initNodeLatency(nodeId) {
    if (!this._latencyMatrix.has(nodeId)) {
      this._latencyMatrix.set(nodeId, new Map());
    }
    for (const [otherId] of this._nodes) {
      if (otherId !== nodeId) {
        if (!this._latencyMatrix.get(nodeId).has(otherId)) {
          this._latencyMatrix.get(nodeId).set(otherId, Infinity);
        }
        if (!this._latencyMatrix.has(otherId)) {
          this._latencyMatrix.set(otherId, new Map());
        }
        if (!this._latencyMatrix.get(otherId).has(nodeId)) {
          this._latencyMatrix.get(otherId).set(nodeId, Infinity);
        }
      }
    }
  }

  _simulateRtt(nodeAId, nodeBId) {
    const a = this._nodes.get(nodeAId);
    const b = this._nodes.get(nodeBId);
    if (!a || !b) return Infinity;
    const sameRegion = a.region && b.region && a.region === b.region;
    const sameZone = a.zone && b.zone && a.zone === b.zone;
    const sameDatacenter = a.datacenter && b.datacenter && a.datacenter === b.datacenter;
    let baseRtt;
    if (sameDatacenter) baseRtt = 0.5 + Math.random() * 0.5;
    else if (sameZone) baseRtt = 2 + Math.random() * 1;
    else if (sameRegion) baseRtt = 10 + Math.random() * 5;
    else baseRtt = 50 + Math.random() * 100;
    return Math.round(baseRtt * 100) / 100;
  }

  probeNode(nodeId) {
    const node = this._nodes.get(nodeId);
    if (!node) return;
    for (const [otherId] of this._nodes) {
      if (otherId === nodeId) continue;
      const rtt = this._simulateRtt(nodeId, otherId);
      this._latencyMatrix.get(nodeId).set(otherId, rtt);
      if (this._latencyMatrix.has(otherId)) {
        this._latencyMatrix.get(otherId).set(nodeId, rtt);
      }
    }
    const avgRtt = this.getAverageLatency(nodeId);
    node.lastRtt = avgRtt;
    this.emit('node:probed', { nodeId, avgRtt });
  }

  probeAll() {
    for (const [nodeId] of this._nodes) {
      this.probeNode(nodeId);
    }
  }

  start() {
    if (this._started) return;
    this._started = true;
    this.probeAll();
    this._timer = setInterval(() => this.probeAll(), this._probeInterval);
    if (this._timer.unref) this._timer.unref();
  }

  stop() {
    this._started = false;
    if (this._timer) { clearInterval(this._timer); this._timer = null; }
  }

  getLatency(fromNodeId, toNodeId) {
    const row = this._latencyMatrix.get(fromNodeId);
    if (!row) return Infinity;
    return row.get(toNodeId) || Infinity;
  }

  getAverageLatency(nodeId) {
    const row = this._latencyMatrix.get(nodeId);
    if (!row || row.size === 0) return Infinity;
    let sum = 0, count = 0;
    for (const [, rtt] of row) {
      if (rtt !== Infinity) { sum += rtt; count++; }
    }
    return count === 0 ? Infinity : Math.round((sum / count) * 100) / 100;
  }

  getOptimalNodes(dataLocalityKey, count = 1) {
    const candidates = [];
    for (const [nodeId, node] of this._nodes) {
      if (!node.alive) continue;
      let score;
      if (dataLocalityKey && node.localityKey && node.localityKey === dataLocalityKey) {
        score = 0;
      } else {
        const avgLat = this.getAverageLatency(nodeId);
        score = avgLat === Infinity ? 1e9 : avgLat;
      }
      score = score / node.weight;
      candidates.push({ nodeId, score, ...node });
    }
    candidates.sort((a, b) => a.score - b.score);
    const selected = candidates.slice(0, count);
    if (selected.length === 0) return [];
    return selected.map(s => ({ id: s.nodeId, region: s.region, zone: s.zone, datacenter: s.datacenter, localityKey: s.localityKey, score: s.score }));
  }

  getLatencyMatrix() {
    const matrix = {};
    for (const [fromId, row] of this._latencyMatrix) {
      matrix[fromId] = {};
      for (const [toId, rtt] of row) {
        matrix[fromId][toId] = rtt;
      }
    }
    return matrix;
  }

  getTopology() {
    const topology = {};
    for (const [nodeId, node] of this._nodes) {
      topology[nodeId] = {
        region: node.region,
        zone: node.zone,
        datacenter: node.datacenter,
        alive: node.alive,
        lastRtt: node.lastRtt,
      };
    }
    return topology;
  }

  getStats() {
    return {
      nodeCount: this._nodes.size,
      probeInterval: this._probeInterval,
      probeTimeout: this._probeTimeout,
      started: this._started,
    };
  }
}

module.exports = { GeoTopologyManager };
