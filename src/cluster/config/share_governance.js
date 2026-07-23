const EventEmitter = require('events');

const CONSENSUS_MODES = { RAFT: 'RAFT', CRDT: 'CRDT' };
const SHARE_TYPES = { READ_ONLY: 'READ_ONLY', MUTABLE: 'MUTABLE' };

class ShareGovernance extends EventEmitter {
  constructor(options = {}) {
    super();
    this._nodeRegistry = options.nodeRegistry || null;
    this._readStore = new Map();
    this._writeStore = new Map();
    this._gossipTable = new Map();
    this._version = 0;
    this._gossipPropagationMs = parseInt(options.gossipPropagationMs || process.env.GOSSIP_PROPAGATION_MS || '50', 10);
    this._consensusEngine = options.consensusEngine || process.env.CONSENSUS_ENGINE || 'RAFT';
    this._peers = [];
    this._gossipTimer = null;
    this._gossipQueue = [];
    this._nodeId = options.nodeId || 'local';

    if (this._nodeRegistry) {
      this._peers = this._nodeRegistry.getAliveNodes().map(n => n.id);
      this._nodeRegistry.on('node:registered', (ev) => {
        if (!this._peers.includes(ev.nodeId)) this._peers.push(ev.nodeId);
      });
      this._nodeRegistry.on('node:offline', (ev) => {
        this._peers = this._peers.filter(p => p !== ev.nodeId);
      });
    }
    this._startGossipFlush();
  }

  configure(key, value) {
    if (key === 'GOSSIP_PROPAGATION_MS') {
      const v = parseInt(value, 10);
      if (v >= 10 && v <= 1000) this._gossipPropagationMs = v;
    }
    if (key === 'CONSENSUS_ENGINE') {
      if (value === 'RAFT' || value === 'CRDT') this._consensusEngine = value;
    }
  }

  _startGossipFlush() {
    if (this._gossipTimer) return;
    this._gossipTimer = setInterval(() => this._flushGossip(), this._gossipPropagationMs);
    if (this._gossipTimer.unref) this._gossipTimer.unref();
  }

  stop() {
    if (this._gossipTimer) { clearInterval(this._gossipTimer); this._gossipTimer = null; }
  }

  addPeer(peerId) {
    if (!this._peers.includes(peerId)) this._peers.push(peerId);
  }

  removePeer(peerId) {
    this._peers = this._peers.filter(p => p !== peerId);
  }

  parseDirective(directive) {
    const parts = directive.trim().split(/\s+/);
    if (parts.length < 3 || parts[0] !== 'SHARE' || parts[1] !== 'CONFIG') {
      throw new Error('Invalid SHARE CONFIG directive: ' + directive);
    }
    const key = parts[2];
    const shareType = parts.length >= 4 ? parts[3] : 'READ_ONLY';
    if (shareType !== 'READ_ONLY' && shareType !== 'MUTABLE') {
      throw new Error('Invalid share type: ' + shareType + '. Must be READ_ONLY or MUTABLE.');
    }
    let consensus = null;
    for (let i = 4; i < parts.length; i++) {
      if (parts[i].startsWith('CONSENSUS=')) {
        consensus = parts[i].split('=')[1];
        if (consensus !== 'RAFT' && consensus !== 'CRDT') {
          throw new Error('Invalid consensus: ' + consensus + '. Must be RAFT or CRDT.');
        }
      }
    }
    return { key, shareType, consensus: consensus || (shareType === 'MUTABLE' ? this._consensusEngine : null) };
  }

  declareReadOnly(key, value) {
    this._version++;
    const entry = { value, version: this._version, timestamp: Date.now() };
    this._readStore.set(key, entry);
    this._gossipQueue.push({ key, version: this._version, value, type: 'read_only' });
    this.emit('share:declared', { key, shareType: 'READ_ONLY', version: this._version });
    return entry;
  }

  read(key) {
    const entry = this._readStore.get(key);
    if (!entry) return null;
    return { value: entry.value, version: entry.version, source: 'local' };
  }

  readVersion(key) {
    const entry = this._readStore.get(key);
    return entry ? entry.version : -1;
  }

  invalidate(key, newValue) {
    const entry = this._readStore.get(key);
    if (!entry) return false;
    this._version++;
    entry.value = newValue;
    entry.version = this._version;
    entry.timestamp = Date.now();
    this._gossipQueue.push({ key, version: this._version, value: newValue, type: 'read_only' });
    this.emit('share:invalidated', { key, version: this._version });
    return true;
  }

  declareMutable(key, consensusMode) {
    const mode = consensusMode || this._consensusEngine;
    const entry = { value: null, consensus: mode, log: [], committed: 0, leaderId: this._nodeId };
    this._writeStore.set(key, entry);
    if (mode === CONSENSUS_MODES.RAFT) {
      entry.log = [];
      entry.committed = 0;
      entry.leaderId = this._nodeId;
    }
    this.emit('write:declared', { key, consensus: mode });
    return entry;
  }

  async write(key, value) {
    const entry = this._writeStore.get(key);
    if (!entry) throw new Error('Key ' + key + ' is not declared as MUTABLE. Use declareMutable() first.');
    if (entry.consensus === CONSENSUS_MODES.RAFT) {
      return this._raftWrite(entry, key, value);
    }
    return this._crdtWrite(entry, key, value);
  }

  readWrite(key) {
    const entry = this._writeStore.get(key);
    if (!entry) return null;
    return { value: entry.value, consensus: entry.consensus, committed: entry.committed };
  }

  _raftWrite(entry, key, value) {
    const index = entry.log.length;
    entry.log.push({ index, value, term: 1, timestamp: Date.now() });
    const followers = this._peers.filter(p => p !== this._nodeId);
    let replicated = 0;
    for (const f of followers) {
      if (this._replicateToFollower(f, key, value, index)) replicated++;
    }
    const majority = Math.floor((this._peers.length + 1) / 2);
    if (replicated >= majority || followers.length === 0) {
      entry.committed = index + 1;
      entry.value = value;
      this._gossipQueue.push({ key, value, version: index, type: 'raft_commit' });
      this.emit('write:committed', { key, value, consensus: 'RAFT', commitIndex: index });
      return { success: true, commitIndex: index, consensus: 'RAFT' };
    }
    this.emit('write:replication_failure', { key, replicated, required: majority });
    return { success: false, commitIndex: index, consensus: 'RAFT', replicated, required: majority };
  }

  _replicateToFollower(followerId, key, value, index) {
    this.emit('raft:replicate', { followerId, key, index });
    return true;
  }

  _crdtWrite(entry, key, value) {
    const lamport = Date.now();
    const delta = { value, lamport, nodeId: this._nodeId, key };
    const existing = entry.value;
    if (!existing || this._nodeId === existing.nodeId || lamport > (existing.lamport || 0) || (lamport === (existing.lamport || 0) && this._nodeId > (existing.nodeId || ''))) {
      entry.value = { ...delta };
      this._gossipQueue.push({ key, delta, type: 'crdt_delta' });
      this.emit('write:merged', { key, value, consensus: 'CRDT', lamport });
      return { success: true, lamport, consensus: 'CRDT' };
    }
    return { success: true, lamport, consensus: 'CRDT', merged: false };
  }

  crdtMerge(key, delta) {
    const entry = this._writeStore.get(key);
    if (!entry || entry.consensus !== CONSENSUS_MODES.CRDT) return false;
    const existing = entry.value;
    if (!existing || delta.lamport > (existing.lamport || 0) || (delta.lamport === (existing.lamport || 0) && (delta.nodeId || '') > (existing.nodeId || ''))) {
      entry.value = { ...delta };
      this.emit('write:merged', { key, value: delta.value, consensus: 'CRDT', lamport: delta.lamport, source: 'gossip' });
      return true;
    }
    return false;
  }

  _flushGossip() {
    if (this._gossipQueue.length === 0) return;
    const batch = this._gossipQueue.splice(0, Math.min(this._gossipQueue.length, 50));
    this.emit('gossip:flush', { batch, origin: this._nodeId });
    for (const peerId of this._peers) {
      this.emit('gossip:send', { target: peerId, batch, origin: this._nodeId });
    }
  }

  receiveGossip(data) {
    if (!data || !data.batch) return;
    for (const msg of data.batch) {
      if (msg.type === 'read_only' || msg.type === 'read_only_invalidation') {
        const existing = this._readStore.get(msg.key);
        if (!existing || msg.version > existing.version) {
          this._readStore.set(msg.key, { value: msg.value, version: msg.version, timestamp: Date.now() });
          this.emit('gossip:received', { key: msg.key, version: msg.version, type: 'read_only' });
        }
      } else if (msg.type === 'raft_commit') {
        const entry = this._writeStore.get(msg.key);
        if (entry && entry.consensus === CONSENSUS_MODES.RAFT) {
          entry.value = msg.value;
          if (msg.version !== undefined) entry.committed = Math.max(entry.committed, msg.version + 1);
        }
      } else if (msg.type === 'crdt_delta') {
        this.crdtMerge(msg.key, msg.delta);
      }
    }
  }

  getSnapshot() {
    const readSnapshot = {};
    for (const [key, entry] of this._readStore) {
      readSnapshot[key] = { value: entry.value, version: entry.version };
    }
    const writeSnapshot = {};
    for (const [key, entry] of this._writeStore) {
      writeSnapshot[key] = { value: entry.value, consensus: entry.consensus, committed: entry.committed };
    }
    return { readStore: readSnapshot, writeStore: writeSnapshot, version: this._version, peers: this._peers.length };
  }

  getStats() {
    return {
      readEntries: this._readStore.size,
      writeEntries: this._writeStore.size,
      gossipPropagationMs: this._gossipPropagationMs,
      consensusEngine: this._consensusEngine,
      peers: this._peers.length,
      gossipQueueSize: this._gossipQueue.length,
      version: this._version,
    };
  }
}

module.exports = { ShareGovernance, CONSENSUS_MODES, SHARE_TYPES };
