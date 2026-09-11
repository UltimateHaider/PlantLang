const crypto = require('crypto');

const DEFAULT_VNODES = 128;

function hashKey(key) {
  const h = crypto.createHash('sha256').update(String(key)).digest();
  return Number(BigInt('0x' + h.subarray(0, 8).toString('hex')));
}

class ConsistentHashRing {
  constructor(vnodes = DEFAULT_VNODES) {
    this._vnodes = vnodes;
    this._ring = new Map();
    this._sortedKeys = [];
    this._nodeToKeys = new Map();
  }

  configure(key, value) {
    if (key === 'CONSISTENT_HASH_VNODES') {
      const v = parseInt(value, 10);
      if (v >= 32 && v <= 512) this._vnodes = v;
    }
  }

  addNode(nodeId) {
    const keys = [];
    for (let i = 0; i < this._vnodes; i++) {
      const vkey = hashKey(nodeId + ':vnode:' + i);
      this._ring.set(vkey, nodeId);
      keys.push(vkey);
    }
    this._nodeToKeys.set(nodeId, keys);
    this._rebuildSortedKeys();
  }

  removeNode(nodeId) {
    const keys = this._nodeToKeys.get(nodeId) || [];
    for (const k of keys) this._ring.delete(k);
    this._nodeToKeys.delete(nodeId);
    this._rebuildSortedKeys();
  }

  _rebuildSortedKeys() {
    this._sortedKeys = Array.from(this._ring.keys()).sort((a, b) => a - b);
  }

  getNode(key) {
    if (this._sortedKeys.length === 0) return null;
    const h = hashKey(key);
    for (let i = 0; i < this._sortedKeys.length; i++) {
      if (this._sortedKeys[i] >= h) return this._ring.get(this._sortedKeys[i]);
    }
    return this._ring.get(this._sortedKeys[0]);
  }

  getNodes() {
    const unique = new Set();
    for (const n of this._ring.values()) unique.add(n);
    return Array.from(unique);
  }

  getNodeCount() { return this.getNodes().length; }

  getKeysForNode(nodeId) { return this._nodeToKeys.get(nodeId) || []; }

  computeMigration(fromNodes, toNodeId) {
    const migratedVkeys = new Set();
    for (const nodeId of fromNodes) {
      const keys = this._nodeToKeys.get(nodeId) || [];
      for (const vkey of keys) {
        const owner = this._ring.get(vkey);
        if (owner === toNodeId) migratedVkeys.add(vkey);
      }
    }
    return Array.from(migratedVkeys);
  }

}

class DistributedHeap {
  constructor(options = {}) {
    this._ring = new ConsistentHashRing(options.vnodes || DEFAULT_VNODES);
    this._store = new Map();
    this._actorOwners = new Map();
    this._leases = new Map();
    this._leaseDuration = options.leaseDuration || 30000;
    this._gcTimer = null;
    this._localNodeId = options.localNodeId || 'local';
  }

  configure(key, value) {
    this._ring.configure(key, value);
  }

  addNode(nodeId) {
    this._ring.addNode(nodeId);
  }

  removeNode(nodeId) {
    const keys = this._ring.getKeysForNode(nodeId);
    this._ring.removeNode(nodeId);
    for (const [storeKey, entry] of this._store) {
      if (entry.owner === nodeId) {
        const newOwner = this._ring.getNode(storeKey);
        if (newOwner) entry.owner = newOwner;
        else this._store.delete(storeKey);
      }
    }
    for (const [actorId, owner] of this._actorOwners) {
      if (owner === nodeId) {
        const newOwner = this._ring.getNode('actor:' + actorId);
        if (newOwner) this._actorOwners.set(actorId, newOwner);
        else this._actorOwners.delete(actorId);
      }
    }
  }

  put(key, value, scope = 'PERSISTENT') {
    const owner = this._ring.getNode(key);
    if (!owner) throw new Error('No nodes available in cluster');
    const now = Date.now();
    this._store.set(key, { value, owner, scope, timestamp: now, leaseExpiry: now + this._leaseDuration });
    return owner;
  }

  get(key) {
    const entry = this._store.get(key);
    if (!entry) return null;
    if (Date.now() > entry.leaseExpiry) {
      this._store.delete(key);
      return null;
    }
    entry.leaseExpiry = Date.now() + this._leaseDuration;
    return entry.value;
  }

  delete(key) {
    return this._store.delete(key);
  }

  registerActor(actorId) {
    const owner = this._ring.getNode('actor:' + actorId);
    if (!owner) throw new Error('No nodes available for actor registration');
    this._actorOwners.set(actorId, owner);
    const now = Date.now();
    this._store.set('_actor:' + actorId, { value: {}, owner, scope: 'PERSISTENT', timestamp: now, leaseExpiry: now + this._leaseDuration });
    return owner;
  }

  getActorOwner(actorId) {
    return this._actorOwners.get(actorId) || null;
  }

  getActorState(actorId) {
    const entry = this._store.get('_actor:' + actorId);
    return entry ? entry.value : null;
  }

  setActorState(actorId, state, requestingNode) {
    const owner = this._actorOwners.get(actorId);
    if (!owner) throw new Error('Actor ' + actorId + ' not registered');
    if (requestingNode && requestingNode !== owner) {
      return { proxied: true, owner };
    }
    const entry = this._store.get('_actor:' + actorId);
    if (entry) {
      entry.value = state;
      entry.leaseExpiry = Date.now() + this._leaseDuration;
    }
    return { proxied: false, owner };
  }

  startGC(intervalMs = 10000) {
    if (this._gcTimer) return;
    this._gcTimer = setInterval(() => {
      const now = Date.now();
      for (const [key, entry] of this._store) {
        if (now > entry.leaseExpiry) {
          this._store.delete(key);
        }
      }
      for (const [actorId, owner] of this._actorOwners) {
        const entry = this._store.get('_actor:' + actorId);
        if (!entry || now > entry.leaseExpiry) {
          this._actorOwners.delete(actorId);
        }
      }
    }, intervalMs);
    if (this._gcTimer.unref) this._gcTimer.unref();
  }

  stopGC() {
    if (this._gcTimer) { clearInterval(this._gcTimer); this._gcTimer = null; }
  }

  getKeyCount() { return this._store.size; }

  getActorCount() { return this._actorOwners.size; }

  getRingNodeCount() { return this._ring.getNodeCount(); }

  computeDataKeyMigration(existingKeys) {
    if (!Array.isArray(existingKeys)) existingKeys = Array.from(this._store.keys());
    const migrated = [];
    for (const key of existingKeys) {
      const owner = this._ring.getNode(key);
      const entry = this._store.get(key);
      if (entry && owner !== entry.owner) migrated.push(key);
    }
    return migrated;
  }

  computeMigrationStats(newNodeId) {
    const existingNodes = this._ring.getNodes().filter(n => n !== newNodeId);
    const migratedKeys = this._ring.computeMigration(existingNodes, newNodeId);
    const totalKeys = this._store.size;
    return { migratedKeys: migratedKeys.length, totalKeys, ratio: totalKeys > 0 ? migratedKeys.length / totalKeys : 0 };
  }
}

module.exports = { DistributedHeap, ConsistentHashRing, hashKey };
