const EventEmitter = require('events');

const REPLICA_STRATEGIES = { LEAST_CONNECTIONS: 'LEAST_CONNECTIONS', ROUND_ROBIN: 'ROUND_ROBIN' };
const ACK_MODES = { ONE: 'ONE', QUORUM: 'QUORUM', ALL: 'ALL' };

class ReplicaManager extends EventEmitter {
  constructor(options = {}) {
    super();
    this._nodeRegistry = options.nodeRegistry || null;
    this._strategy = options.replicaStrategy || process.env.REPLICA_STRATEGY || 'LEAST_CONNECTIONS';
    this._ackMode = options.primaryBackupAck || process.env.PRIMARY_BACKUP_ACK || 'QUORUM';
    this._activeConnections = new Map();
    this._roundRobinIndex = 0;
    this._primaries = new Map();
    this._backups = new Map();
    this._replicaLedger = new Map();
  }

  configure(key, value) {
    if (key === 'REPLICA_STRATEGY') {
      if (value === 'LEAST_CONNECTIONS' || value === 'ROUND_ROBIN') this._strategy = value;
    }
    if (key === 'PRIMARY_BACKUP_ACK') {
      if (value === 'ONE' || value === 'QUORUM' || value === 'ALL') this._ackMode = value;
    }
  }

  getAliveNodes() {
    if (!this._nodeRegistry) return [];
    return this._nodeRegistry.getAliveNodes();
  }

  _getActiveCount(nodeId) {
    return this._activeConnections.get(nodeId) || 0;
  }

  _incrementActive(nodeId) {
    this._activeConnections.set(nodeId, this._getActiveCount(nodeId) + 1);
  }

  _decrementActive(nodeId) {
    const c = this._getActiveCount(nodeId);
    if (c > 0) this._activeConnections.set(nodeId, c - 1);
  }

  selectStatelessTarget() {
    const alive = this.getAliveNodes();
    if (alive.length === 0) return null;
    if (this._strategy === REPLICA_STRATEGIES.ROUND_ROBIN) {
      const idx = this._roundRobinIndex % alive.length;
      this._roundRobinIndex = (this._roundRobinIndex + 1) % alive.length;
      const selected = alive[idx];
      this._incrementActive(selected.id);
      return selected;
    }
    const selected = alive.reduce((a, b) => {
      const ca = this._getActiveCount(a.id);
      const cb = this._getActiveCount(b.id);
      return ca <= cb ? a : b;
    });
    this._incrementActive(selected.id);
    return selected;
  }

  assignPrimary(actorId) {
    const alive = this.getAliveNodes();
    if (alive.length === 0) throw new Error('No alive nodes to assign primary for actor ' + actorId);
    const primary = alive.reduce((a, b) => {
      const pA = Array.from(this._primaries.values()).filter(p => p === a.id).length;
      const pB = Array.from(this._primaries.values()).filter(p => p === b.id).length;
      return pA <= pB ? a : b;
    });
    this._primaries.set(actorId, primary.id);
    const backups = alive.filter(n => n.id !== primary.id);
    const backupIds = backups.map(n => n.id);
    this._backups.set(actorId, backupIds);
    this._replicaLedger.set(actorId, { primary: primary.id, backups: backupIds, log: [], version: 0 });
    this.emit('actor:primary_assigned', { actorId, primary: primary.id, backups: backupIds });
    return { primary: primary.id, backups: backupIds };
  }

  getPrimary(actorId) {
    return this._primaries.get(actorId) || null;
  }

  getBackups(actorId) {
    return this._backups.get(actorId) || [];
  }

  async replicateMutation(actorId, mutation) {
    const entry = this._replicaLedger.get(actorId);
    if (!entry) throw new Error('Actor ' + actorId + ' has no replica ledger');
    entry.version++;
    const logEntry = { version: entry.version, mutation, timestamp: Date.now() };
    entry.log.push(logEntry);
    const backups = this.getBackups(actorId);
    if (backups.length === 0) return { success: true, version: entry.version, ackMode: this._ackMode, ackCount: 0 };
    let ackCount = 0;
    const syncErrors = [];
    for (const backupId of backups) {
      try {
        this._syncToBackup(backupId, actorId, logEntry);
        ackCount++;
      } catch (err) {
        syncErrors.push(backupId);
      }
    }
    const totalBackups = backups.length;
    let committed = false;
    if (this._ackMode === ACK_MODES.ONE) {
      committed = ackCount >= 1;
    } else if (this._ackMode === ACK_MODES.QUORUM) {
      const required = Math.floor(totalBackups / 2) + 1;
      committed = ackCount >= required;
    } else {
      committed = ackCount >= totalBackups;
    }
    this.emit('mutation:replicated', { actorId, version: entry.version, ackCount, totalBackups, committed, ackMode: this._ackMode });
    if (!committed) {
      throw new Error('Mutation replication failed: got ' + ackCount + '/' + totalBackups + ' acks (need ' + this._ackMode + ')');
    }
    return { success: true, version: entry.version, ackCount, totalBackups, ackMode: this._ackMode };
  }

  _syncToBackup(backupId, actorId, logEntry) {
    this.emit('backup:sync', { backupId, actorId, version: logEntry.version });
    return true;
  }

  handleNodeFailure(failedNodeId) {
    this.emit('node:failover_start', { failedNodeId });
    const affectedActors = [];
    for (const [actorId, primary] of this._primaries) {
      if (primary === failedNodeId) affectedActors.push(actorId);
    }
    for (const actorId of affectedActors) {
      this._failoverActor(actorId, failedNodeId);
    }
    for (const [actorId, backups] of this._backups) {
      this._backups.set(actorId, backups.filter(b => b !== failedNodeId));
    }
    this.emit('node:failover_complete', { failedNodeId, affectedCount: affectedActors.length });
    return affectedActors;
  }

  _failoverActor(actorId, failedNodeId) {
    const entry = this._replicaLedger.get(actorId);
    if (!entry) return;
    const backups = this.getBackups(actorId).filter(b => b !== failedNodeId);
    if (backups.length === 0) {
      this._primaries.delete(actorId);
      this._backups.delete(actorId);
      this.emit('actor:lost', { actorId, reason: 'no backups available' });
      return;
    }
    const newPrimary = backups[0];
    this._primaries.set(actorId, newPrimary);
    const remainingBackups = backups.slice(1);
    this._backups.set(actorId, remainingBackups);
    entry.primary = newPrimary;
    this.emit('actor:failover', { actorId, newPrimary, previousPrimary: failedNodeId, backups: remainingBackups });
  }

  getLedger(actorId) {
    return this._replicaLedger.get(actorId) || null;
  }

  getStats() {
    return {
      strategy: this._strategy,
      ackMode: this._ackMode,
      primaries: this._primaries.size,
      backups: Array.from(this._backups.values()).reduce((s, b) => s + b.length, 0),
      activeConnections: Array.from(this._activeConnections.values()).reduce((s, c) => s + c, 0),
    };
  }
}

module.exports = { ReplicaManager, REPLICA_STRATEGIES, ACK_MODES };
