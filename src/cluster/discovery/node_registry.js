const EventEmitter = require('events');

const NODE_STATES = { HEALTHY: 'HEALTHY', DEGRADED: 'DEGRADED', OFFLINE: 'OFFLINE' };

class NodeRegistry extends EventEmitter {
  constructor(options = {}) {
    super();
    this._nodes = new Map();
    this._heartbeatInterval = options.heartbeatInterval || parseInt(process.env.HEARTBEAT_INTERVAL, 10) || 1000;
    this._failureThreshold = options.heartbeatThreshold || parseInt(process.env.HEARTBEAT_THRESHOLD, 10) || 3;
    this._timer = null;
    this._started = false;
  }

  configure(key, value) {
    const v = parseInt(value, 10);
    if (key === 'HEARTBEAT_INTERVAL' && v >= 100 && v <= 10000) {
      this._heartbeatInterval = v;
      if (this._started) { this.stop(); this.start(); }
    }
    if (key === 'HEARTBEAT_THRESHOLD' && v >= 2 && v <= 10) {
      this._failureThreshold = v;
    }
  }

  register(nodeId, info = {}) {
    const now = Date.now();
    this._nodes.set(nodeId, {
      id: nodeId,
      state: NODE_STATES.HEALTHY,
      firstSeen: now,
      lastHeartbeat: now,
      missedBeats: 0,
      cpuUtil: info.cpuUtil || 0,
      heapUsage: info.heapUsage || 0,
      activeWorkers: info.activeWorkers || 0,
      address: info.address || `node-${nodeId}:9374`,
    });
    this.emit('node:registered', { nodeId, state: NODE_STATES.HEALTHY });
    return this._nodes.get(nodeId);
  }

  unregister(nodeId) {
    const node = this._nodes.get(nodeId);
    if (node) {
      node.state = NODE_STATES.OFFLINE;
      this.emit('node:offline', { nodeId });
    }
    this._nodes.delete(nodeId);
  }

  heartbeat(nodeId, telemetry = {}) {
    const node = this._nodes.get(nodeId);
    if (!node) return null;
    node.lastHeartbeat = Date.now();
    node.missedBeats = 0;
    if (node.state === NODE_STATES.DEGRADED || node.state === NODE_STATES.OFFLINE) {
      node.state = NODE_STATES.HEALTHY;
      this.emit('node:healthy', { nodeId });
    }
    if (telemetry.cpuUtil !== undefined) node.cpuUtil = telemetry.cpuUtil;
    if (telemetry.heapUsage !== undefined) node.heapUsage = telemetry.heapUsage;
    if (telemetry.activeWorkers !== undefined) node.activeWorkers = telemetry.activeWorkers;
    return node;
  }

  markDegraded(nodeId) {
    const node = this._nodes.get(nodeId);
    if (node && node.state === NODE_STATES.HEALTHY) {
      node.state = NODE_STATES.DEGRADED;
      this.emit('node:degraded', { nodeId });
    }
  }

  _checkHeartbeats() {
    const now = Date.now();
    for (const [nodeId, node] of this._nodes) {
      if (node.state === NODE_STATES.OFFLINE) continue;
      const elapsed = now - node.lastHeartbeat;
      if (elapsed >= this._heartbeatInterval) {
        node.missedBeats++;
        if (node.missedBeats >= this._failureThreshold) {
          const oldState = node.state;
          node.state = NODE_STATES.OFFLINE;
          this.emit('node:offline', { nodeId, previousState: oldState, missedBeats: node.missedBeats });
        } else if (node.missedBeats >= Math.ceil(this._failureThreshold / 2) && node.state === NODE_STATES.HEALTHY) {
          node.state = NODE_STATES.DEGRADED;
          this.emit('node:degraded', { nodeId, missedBeats: node.missedBeats });
        }
      }
    }
  }

  start() {
    if (this._started) return;
    this._started = true;
    this._timer = setInterval(() => this._checkHeartbeats(), this._heartbeatInterval);
    if (this._timer.unref) this._timer.unref();
  }

  stop() {
    this._started = false;
    if (this._timer) { clearInterval(this._timer); this._timer = null; }
  }

  getNode(nodeId) { return this._nodes.get(nodeId) || null; }

  getAliveNodes() {
    const alive = [];
    for (const n of this._nodes.values()) {
      if (n.state !== NODE_STATES.OFFLINE) alive.push(n);
    }
    return alive;
  }

  getNodes() { return Array.from(this._nodes.values()); }

  getNodeCount() { return this._nodes.size; }

  getAliveCount() { return this.getAliveNodes().length; }
}

module.exports = { NodeRegistry, NODE_STATES };
