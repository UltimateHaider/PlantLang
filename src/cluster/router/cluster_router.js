const EventEmitter = require('events');

const CIRCUIT_STATES = { CLOSED: 'CLOSED', OPEN: 'OPEN', HALF_OPEN: 'HALF-OPEN' };

class CircuitBreaker {
  constructor(options = {}) {
    this._errorThreshold = options.errorThreshold || 0.05;
    this._cooldownMs = options.cooldownMs || 30000;
    this._state = CIRCUIT_STATES.CLOSED;
    this._totalCalls = 0;
    this._errorCalls = 0;
    this._lastTripTime = 0;
    this._halfOpenProbeSent = false;
  }

  configure(key, value) {
    if (key === 'CIRCUIT_BREAKER_THRESHOLD') {
      const v = parseFloat(value);
      if (v >= 0.01 && v <= 0.50) this._errorThreshold = v;
    }
    if (key === 'CIRCUIT_BREAKER_COOLDOWN') {
      const v = parseInt(value, 10);
      if (v >= 1000 && v <= 300000) this._cooldownMs = v;
    }
  }

  get state() { return this._state; }
  get errorRate() { return this._totalCalls > 0 ? this._errorCalls / this._totalCalls : 0; }

  recordSuccess() {
    this._totalCalls++;
    if (this._state === CIRCUIT_STATES.HALF_OPEN) {
      this._state = CIRCUIT_STATES.CLOSED;
      this._totalCalls = 0;
      this._errorCalls = 0;
      this._halfOpenProbeSent = false;
    }
  }

  recordFailure() {
    this._totalCalls++;
    this._errorCalls++;
    this._evaluate();
  }

  _evaluate() {
    if (this._state === CIRCUIT_STATES.OPEN) return;
    const rate = this.errorRate;
    if (rate >= this._errorThreshold && this._totalCalls >= 10) {
      this._state = CIRCUIT_STATES.OPEN;
      this._lastTripTime = Date.now();
      this._halfOpenProbeSent = false;
    }
  }

  allowRequest() {
    if (this._state === CIRCUIT_STATES.CLOSED) return true;
    if (this._state === CIRCUIT_STATES.OPEN) {
      if (Date.now() - this._lastTripTime >= this._cooldownMs) {
        this._state = CIRCUIT_STATES.HALF_OPEN;
        this._halfOpenProbeSent = false;
        return true;
      }
      return false;
    }
    if (this._state === CIRCUIT_STATES.HALF_OPEN) {
      if (!this._halfOpenProbeSent) {
        this._halfOpenProbeSent = true;
        return true;
      }
      return false;
    }
    return true;
  }

  reset() {
    this._state = CIRCUIT_STATES.CLOSED;
    this._totalCalls = 0;
    this._errorCalls = 0;
    this._lastTripTime = 0;
    this._halfOpenProbeSent = false;
  }
}

class ClusterRouter extends EventEmitter {
  constructor(options = {}) {
    super();
    this._nodeRegistry = options.nodeRegistry || null;
    this._jwtGuard = options.jwtGuard || null;
    this._activeConnections = new Map();
    this._circuitBreakers = new Map();
    this._defaultCircuitBreaker = new CircuitBreaker(options);
  }

  configure(key, value) {
    this._defaultCircuitBreaker.configure(key, value);
  }

  getCircuitBreaker(nodeId) {
    if (!this._circuitBreakers.has(nodeId)) {
      const cb = new CircuitBreaker({
        errorThreshold: this._defaultCircuitBreaker._errorThreshold,
        cooldownMs: this._defaultCircuitBreaker._cooldownMs,
      });
      this._circuitBreakers.set(nodeId, cb);
    }
    return this._circuitBreakers.get(nodeId);
  }

  _selectTarget() {
    if (!this._nodeRegistry) return null;
    const alive = this._nodeRegistry.getAliveNodes();
    if (alive.length === 0) return null;

    const candidates = alive.filter(n => {
      const cb = this._circuitBreakers.get(n.id);
      return !cb || cb.allowRequest();
    });

    if (candidates.length === 0) {
      const fallback = alive.reduce((a, b) => {
        const ca = this._activeConnections.get(a.id) || 0;
        const cb = this._activeConnections.get(b.id) || 0;
        return ca <= cb ? a : b;
      });
      this.emit('router:fallback', { nodeId: fallback.id, reason: 'all circuits open' });
      return fallback;
    }

    const selected = candidates.reduce((a, b) => {
      const activeA = this._activeConnections.get(a.id) || 0;
      const activeB = this._activeConnections.get(b.id) || 0;
      const weightA = 1 - (a.cpuUtil || 0);
      const weightB = 1 - (b.cpuUtil || 0);
      const scoreA = activeA * weightA;
      const scoreB = activeB * weightB;
      return scoreA <= scoreB ? a : b;
    });

    return selected;
  }

  async dispatch(action, payload, options = {}) {
    const target = this._selectTarget();
    if (!target) {
      throw new Error('No available cluster nodes for dispatch');
    }

    const connKey = target.id;
    this._activeConnections.set(connKey, (this._activeConnections.get(connKey) || 0) + 1);
    const cb = this.getCircuitBreaker(target.id);

    try {
      const jwtToken = options.jwtToken || null;
      const result = await this._executeOnNode(target, action, payload, jwtToken);
      cb.recordSuccess();
      return result;
    } catch (err) {
      cb.recordFailure();
      if (cb.state === CIRCUIT_STATES.OPEN) {
        this.emit('router:circuit_open', {
          nodeId: target.id,
          errorRate: cb.errorRate,
          threshold: cb._errorThreshold,
          message: 'SECURITY_ALERT: Node peer disconnected. Executing failover.'
        });
      }
      const backupTarget = this._selectBackup(target.id);
      if (backupTarget) {
        this._activeConnections.set(backupTarget.id, (this._activeConnections.get(backupTarget.id) || 0) + 1);
        try {
          const jwtToken = options.jwtToken || null;
          const result = await this._executeOnNode(backupTarget, action, payload, jwtToken);
          this.getCircuitBreaker(backupTarget.id).recordSuccess();
          return result;
        } catch (backupErr) {
          this.getCircuitBreaker(backupTarget.id).recordFailure();
          throw new Error('Cluster dispatch failed on primary and backup: ' + backupErr.message);
        }
      }
      throw err;
    } finally {
      this._activeConnections.set(connKey, Math.max(0, (this._activeConnections.get(connKey) || 0) - 1));
    }
  }

  _selectBackup(excludeNodeId) {
    if (!this._nodeRegistry) return null;
    const alive = this._nodeRegistry.getAliveNodes().filter(n => n.id !== excludeNodeId);
    if (alive.length === 0) return null;
    return alive.reduce((a, b) => {
      const activeA = this._activeConnections.get(a.id) || 0;
      const activeB = this._activeConnections.get(b.id) || 0;
      return activeA <= activeB ? a : b;
    });
  }

  async _executeOnNode(node, action, payload, jwtToken) {
    if (this._jwtGuard && jwtToken) {
      try {
        this._jwtGuard.verifyJWT(jwtToken, null);
      } catch (jwtErr) {
        this.emit('router:auth_failure', { nodeId: node.id, error: jwtErr.message });
        throw new Error('JWT verification failed for dispatch to ' + node.id + ': ' + jwtErr.message);
      }
    }
    return { nodeId: node.id, action, result: 'executed' };
  }

  resetCircuitBreaker(nodeId) {
    const cb = this._circuitBreakers.get(nodeId);
    if (cb) cb.reset();
  }

  resetAllCircuitBreakers() {
    for (const cb of this._circuitBreakers.values()) cb.reset();
  }

  getNodeConnections(nodeId) { return this._activeConnections.get(nodeId) || 0; }
}

module.exports = { ClusterRouter, CircuitBreaker, CIRCUIT_STATES };
