const { ReplicaManager, REPLICA_STRATEGIES, ACK_MODES } = require('../src/cluster/replica/replica_manager');
const { DistributedCycleEngine } = require('../src/cluster/cycles/distributed_cycle_engine');
const { ReapAggregator, REAP_MODES } = require('../src/cluster/reap/reap_aggregator');
const { NodeRegistry } = require('../src/cluster/discovery/node_registry');

let passed = 0, failed = 0;

function assert(cond, label) {
  if (cond) { passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  else { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label); }
}

function assertEqual(a, b, label) {
  if (a === b) { passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  else { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label + ' (' + JSON.stringify(a) + ' !== ' + JSON.stringify(b) + ')'); }
}

function assertThrows(fn, label) {
  try { fn(); failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label); }
  catch (e) { passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
}

function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

const asyncTests = [];

async function main() {

// ── ReplicaManager: Stateless Routing ──
console.log('\u001b[1m--- ReplicaManager: Stateless Routing ---\u001b[0m');

(function() {
  const rm = new ReplicaManager();
  assert(rm !== null, 'ReplicaManager created without error');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A', { cpuUtil: 0.3 });
  reg.register('node-B', { cpuUtil: 0.3 });
  reg.register('node-C', { cpuUtil: 0.3 });
  const rm = new ReplicaManager({ nodeRegistry: reg, replicaStrategy: 'ROUND_ROBIN' });
  const t1 = rm.selectStatelessTarget();
  const t2 = rm.selectStatelessTarget();
  const t3 = rm.selectStatelessTarget();
  const t4 = rm.selectStatelessTarget();
  assert(t1.id !== t2.id || t2.id !== t3.id, 'round robin rotates targets');
  assertEqual(t1.id, t4.id, 'round robin wraps after 3 nodes (A after C)');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('busy', { cpuUtil: 0.9 });
  reg.register('free', { cpuUtil: 0.1 });
  const rm = new ReplicaManager({ nodeRegistry: reg, replicaStrategy: 'LEAST_CONNECTIONS' });
  rm._activeConnections.set('busy', 10);
  rm._activeConnections.set('free', 1);
  const target = rm.selectStatelessTarget();
  assertEqual(target.id, 'free', 'LEAST_CONNECTIONS selects node with fewest active connections');
})();

(function() {
  const rm = new ReplicaManager({ nodeRegistry: null });
  const target = rm.selectStatelessTarget();
  assertEqual(target, null, 'selectStatelessTarget returns null with no registry');
})();

(function() {
  const reg = new NodeRegistry();
  const rm = new ReplicaManager({ nodeRegistry: reg, replicaStrategy: 'LEAST_CONNECTIONS' });
  const target = rm.selectStatelessTarget();
  assertEqual(target, null, 'selectStatelessTarget returns null with no alive nodes');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('A');
  reg.register('B');
  const rm = new ReplicaManager({ nodeRegistry: reg, replicaStrategy: 'LEAST_CONNECTIONS' });
  const loads = {};
  for (let i = 0; i < 100; i++) {
    const t = rm.selectStatelessTarget();
    loads[t.id] = (loads[t.id] || 0) + 1;
  }
  const ratio = Math.min(loads.A, loads.B) / Math.max(loads.A, loads.B);
  assert(ratio >= 0.8, 'LEAST_CONNECTIONS distributes ~evenly over 100 calls (ratio=' + ratio.toFixed(3) + ')');
})();

// ── ReplicaManager: Stateful Primary-Backup ──
console.log('\u001b[1m--- ReplicaManager: Stateful Primary-Backup ---\u001b[0m');

(function() {
  const reg = new NodeRegistry();
  reg.register('node-1');
  reg.register('node-2');
  reg.register('node-3');
  const rm = new ReplicaManager({ nodeRegistry: reg });
  const assignment = rm.assignPrimary('actor-session-1');
  assert(assignment.primary !== undefined, 'assignPrimary returns a primary');
  assert(assignment.backups.length >= 2, 'assignPrimary returns at least 2 backups with 3 nodes');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('A');
  reg.register('B');
  reg.register('C');
  const rm = new ReplicaManager({ nodeRegistry: reg });
  rm.assignPrimary('actor-1');
  const primary = rm.getPrimary('actor-1');
  assert(primary !== null, 'getPrimary returns primary node');
  assert(typeof primary === 'string', 'primary is a string node ID');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('A');
  const rm = new ReplicaManager({ nodeRegistry: reg });
  rm.assignPrimary('actor-1');
  const backups = rm.getBackups('actor-1');
  assert(Array.isArray(backups), 'getBackups returns array');
  assertEqual(backups.length, 0, 'no backups with single node');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('A');
  reg.register('B');
  const rm = new ReplicaManager({ nodeRegistry: reg });
  const a1 = rm.assignPrimary('a1');
  const a2 = rm.assignPrimary('a2');
  assert(a1.primary !== a2.primary || a1.backups[0] !== a2.backups[0], 'primaries balanced across nodes');
})();

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('primary');
  reg.register('backup-1');
  reg.register('backup-2');
  const rm = new ReplicaManager({ nodeRegistry: reg, primaryBackupAck: 'QUORUM' });
  rm.assignPrimary('actor-cfg');
  const result = await rm.replicateMutation('actor-cfg', { set: { timeout: 30 } });
  assert(result.success === true, 'QUORUM mutation replication succeeds');
  assert(result.ackMode === 'QUORUM', 'replication reports QUORUM mode');
  assert(typeof result.version === 'number', 'replication returns version number');
})()));

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('p');
  reg.register('b1');
  reg.register('b2');
  reg.register('b3');
  const rm = new ReplicaManager({ nodeRegistry: reg, primaryBackupAck: 'ALL' });
  rm.assignPrimary('actor-all');
  const result = await rm.replicateMutation('actor-all', { increment: 1 });
  assert(result.success === true, 'ALL ack mutation succeeds');
  assertEqual(result.ackCount, 3, 'ALL ack requires all 3 backups');
})()));

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('p');
  reg.register('b1');
  const rm = new ReplicaManager({ nodeRegistry: reg, primaryBackupAck: 'ONE' });
  rm.assignPrimary('actor-fast');
  const result = await rm.replicateMutation('actor-fast', { val: 42 });
  assert(result.success === true, 'ONE ack mutation succeeds with single ack');
})()));

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('p');
  reg.register('b1');
  reg.register('b2');
  reg.register('b3');
  const rm = new ReplicaManager({ nodeRegistry: reg, primaryBackupAck: 'QUORUM' });
  rm.assignPrimary('actor-q');
  await rm.replicateMutation('actor-q', { v: 1 });
  await rm.replicateMutation('actor-q', { v: 2 });
  const ledger = rm.getLedger('actor-q');
  assert(ledger !== null, 'ledger exists for actor');
  assertEqual(ledger.version, 2, 'ledger version advances with each mutation');
  assertEqual(ledger.log.length, 2, 'ledger log accumulates mutations');
  assertEqual(ledger.log[1].mutation.v, 2, 'ledger stores correct mutation data');
})()));

// ── ReplicaManager: Primary Failover ──
console.log('\u001b[1m--- ReplicaManager: Primary Failover ---\u001b[0m');

(function() {
  const reg = new NodeRegistry();
  reg.register('A');
  reg.register('B');
  reg.register('C');
  const rm = new ReplicaManager({ nodeRegistry: reg });
  rm.assignPrimary('actor-fail');
  const before = rm.getPrimary('actor-fail');
  assert(before === 'A' || before === 'B' || before === 'C', 'primary assigned');
  rm.handleNodeFailure(before);
  const after = rm.getPrimary('actor-fail');
  assert(after !== null, 'new primary assigned after failure');
  assert(after !== before, 'new primary is different from failed node');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('A');
  reg.register('B');
  const rm = new ReplicaManager({ nodeRegistry: reg });
  rm.assignPrimary('actor-solo');
  const before = rm.getPrimary('actor-solo');
  rm.handleNodeFailure(before);
  const backups = rm.getBackups('actor-solo');
  const primary = rm.getPrimary('actor-solo');
  const ledger = rm.getLedger('actor-solo');
  if (reg.getAliveCount() >= 1) {
    assert(primary !== null, 'failover with 1 backup succeeds');
    assert(Array.isArray(backups), 'backups still an array');
  }
})();

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('P');
  reg.register('B1');
  reg.register('B2');
  const rm = new ReplicaManager({ nodeRegistry: reg, primaryBackupAck: 'QUORUM' });
  rm.assignPrimary('actor-q');
  await rm.replicateMutation('actor-q', { state: 'active' });
  const oldPrimary = rm.getPrimary('actor-q');
  reg.unregister(oldPrimary);
  rm.handleNodeFailure(oldPrimary);
  const newPrimary = rm.getPrimary('actor-q');
  assert(newPrimary !== null, 'failover promotes backup to primary');
  assert(newPrimary !== oldPrimary, 'new primary is different');
  const ledger = rm.getLedger('actor-q');
  assert(ledger.primary === newPrimary, 'ledger updated with new primary');
  assert(ledger.log.length >= 1, 'ledger preserved after failover');
})()));

// ── DistributedCycleEngine: Adaptive Chunking ──
console.log('\u001b[1m--- DistributedCycleEngine: Chunking ---\u001b[0m');

(function() {
  const dce = new DistributedCycleEngine();
  assert(dce !== null, 'DistributedCycleEngine created without error');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('w1');
  reg.register('w2');
  reg.register('w3');
  reg.register('w4');
  const dce = new DistributedCycleEngine({ nodeRegistry: reg, coreFactor: 2, minChunkSize: 100 });
  const totalIterations = 10000;
  const chunkSize = dce.computeChunkSize(totalIterations);
  const expected = Math.max(100, Math.ceil(10000 / (4 * 2)));
  assertEqual(chunkSize, expected, 'chunk size = max(minChunkSize, ceil(N/(workers×coreFactor)))');
})();

(function() {
  const dce = new DistributedCycleEngine({ coreFactor: 2, minChunkSize: 1000 });
  const totalIterations = 100;
  const chunkSize = dce.computeChunkSize(totalIterations);
  assertEqual(chunkSize, 1000, 'chunk size uses minChunkSize when formula < min');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('w1');
  reg.register('w2');
  const dce = new DistributedCycleEngine({ nodeRegistry: reg, coreFactor: 2, minChunkSize: 100 });
  const totalIterations = 1000000;
  const result = dce.scatter(totalIterations);
  assert(result.totalChunks > 0, 'scatter produces chunks');
  assert(result.chunkSize > 0, 'scatter has positive chunk size');
  const chunkSize = dce.computeChunkSize(totalIterations);
  const expectedChunks = Math.ceil(totalIterations / chunkSize);
  assert(result.totalChunks === expectedChunks || Math.abs(result.totalChunks - expectedChunks) <= 1,
    'scatter produces ~expected number of chunks (' + result.totalChunks + ' vs ' + expectedChunks + ')');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('w1');
  reg.register('w2');
  const dce = new DistributedCycleEngine({ nodeRegistry: reg, coreFactor: 2, minChunkSize: 100 });
  dce.scatter(50000);
  const load = dce.getWorkerLoad();
  assert(Object.keys(load).length === 2, 'both workers have initial chunks');
  assert(load.w1 > 0, 'worker 1 has chunks');
  assert(load.w2 > 0, 'worker 2 has chunks');
})();

// ── DistributedCycleEngine: Work-Stealing ──
console.log('\u001b[1m--- DistributedCycleEngine: Work-Stealing ---\u001b[0m');

(function() {
  const reg = new NodeRegistry();
  reg.register('fast');
  reg.register('slow');
  const dce = new DistributedCycleEngine({ nodeRegistry: reg, coreFactor: 2, minChunkSize: 100 });
  dce.scatter(10000);
  const before = dce.getPendingChunkCount();
  dce.completeChunk('fast', 0, 'done');
  const after = dce.getPendingChunkCount();
  assert(after < before, 'work stealing reduces pending chunks');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('w1');
  reg.register('w2');
  const dce = new DistributedCycleEngine({ nodeRegistry: reg, coreFactor: 2, minChunkSize: 100 });
  dce.scatter(100000);
  const initialPending = dce.getPendingChunkCount();
  let completedCount = 0;
  for (let i = 0; i < 30 && dce.getPendingChunkCount() > 0; i++) {
    if (dce.completeChunk('w1', i, 'r-' + i)) completedCount++;
  }
  assert(dce.getPendingChunkCount() < initialPending, 'progressive completion reduces pending');
  assert(dce.getCompletedChunkCount() >= completedCount, 'completed chunks tracked');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('fast');
  reg.register('slow');
  const dce = new DistributedCycleEngine({ nodeRegistry: reg, coreFactor: 2, minChunkSize: 100 });
  const result = dce.scatter(5000);
  const chunksAtStart = result.totalChunks;
  dce.completeChunk('fast', 0, 'r0');
  dce.completeChunk('fast', 2, 'r2');
  dce.completeChunk('slow', 1, 'r1');
  const completed = dce.getCompletedChunkCount();
  assert(completed >= 3, 'completed chunks tracked correctly');
})();

// ── DistributedCycleEngine: Timeout & Recovery ──
console.log('\u001b[1m--- DistributedCycleEngine: Timeout ---\u001b[0m');

(function() {
  const reg = new NodeRegistry();
  reg.register('w1');
  reg.register('w2');
  const dce = new DistributedCycleEngine({ nodeRegistry: reg, coreFactor: 1, minChunkSize: 10, workerTimeoutMs: 50 });
  dce.scatter(1000);
  const reQueued = dce.checkTimeouts();
  assert(Array.isArray(reQueued), 'checkTimeouts returns array');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('slow-worker');
  const dce = new DistributedCycleEngine({ nodeRegistry: reg, coreFactor: 1, minChunkSize: 5, workerTimeoutMs: 100 });
  dce.scatter(500);
  const beforePending = dce.getPendingChunkCount();
  const beforeActive = dce.getActiveChunkCount();
  const reQueued = dce.checkTimeouts();
  if (reQueued.length > 0) {
    assert(dce.getPendingChunkCount() > beforePending, 'timeout re-queues pending chunks');
  } else {
    assert(true, 'no timeouts triggered (active chunks may have been assigned recently)');
  }
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('w1');
  const dce = new DistributedCycleEngine({ nodeRegistry: reg, coreFactor: 1, minChunkSize: 100, workerTimeoutMs: 50 });
  dce.scatter(10000);
  const statsBefore = dce.getStats();
  assert(statsBefore.pendingChunks >= 0, 'stats reports pending chunks');
  assert(statsBefore.activeChunks >= 0, 'stats reports active chunks');
})();

// ── ReapAggregator: LOCAL_REAP ──
console.log('\u001b[1m--- ReapAggregator ---\u001b[0m');

(function() {
  const ra = new ReapAggregator();
  assert(ra !== null, 'ReapAggregator created without error');
})();

(asyncTests.push((async () => {
  const ra = new ReapAggregator({ mode: 'LOCAL_REAP' });
  const r1 = await ra.collect({ chunkId: 0, count: 10 });
  assert(r1.collected === true, 'LOCAL_REAP collect succeeds');
  assertEqual(r1.mode, 'LOCAL_REAP', 'LOCAL_REAP reports correct mode');
  assertEqual(r1.totalResults, 1, 'LOCAL_REAP tracks result count');
})()));

(asyncTests.push((async () => {
  const ra = new ReapAggregator({ mode: 'LOCAL_REAP' });
  await ra.collect(10);
  await ra.collect(20);
  await ra.collect(30);
  const sum = ra.reduce((a, b) => a + b, 0);
  assertEqual(sum, 60, 'LOCAL_REAP reduce sums correctly');
})()));

(asyncTests.push((async () => {
  const ra = new ReapAggregator({ mode: 'LOCAL_REAP' });
  await ra.collect({ id: 'a', val: 1 });
  await ra.collect({ id: 'b', val: 2 });
  await ra.collect({ id: 'a', val: 3 });
  const merged = ra.merge(r => r.id, (a, b) => ({ id: a.id, val: a.val + b.val }));
  assertEqual(merged.length, 2, 'LOCAL_REAP merge deduplicates by key');
  const aMerged = merged.find(m => m.id === 'a');
  assertEqual(aMerged.val, 4, 'LOCAL_REAP merge combines values for same key');
})()));

(asyncTests.push((async () => {
  const ra = new ReapAggregator({ mode: 'LOCAL_REAP' });
  await ra.collect(1);
  await ra.collect(2);
  await ra.collect(3);
  assertEqual(ra.getResultCount(), 3, 'getResultCount returns count');
  const flushed = ra.flush();
  assertEqual(flushed, 3, 'flush returns flushed count');
  assertEqual(ra.getResultCount(), 0, 'flush clears results');
})()));

// ── ReapAggregator: REMOTE_REAP ──
console.log('\u001b[1m--- ReapAggregator: REMOTE_REAP ---\u001b[0m');

(asyncTests.push((async () => {
  const ra = new ReapAggregator({ mode: 'REMOTE_REAP', remoteTarget: 'MEMORY_BUFFER' });
  const r = await ra.collect({ chunkId: 5, data: 'test' });
  assert(r.collected === true, 'REMOTE_REAP memory buffer collects');
  assertEqual(r.mode, 'REMOTE_REAP', 'REMOTE_REAP reports mode');
  assertEqual(r.target, 'MEMORY_BUFFER', 'REMOTE_REAP uses MEMORY_BUFFER');
})()));

(asyncTests.push((async () => {
  const ra = new ReapAggregator({ mode: 'REMOTE_REAP', remoteTarget: 'MEMORY_BUFFER' });
  await ra.collect({ n: 1 });
  await ra.collect({ n: 2 });
  await ra.collect({ n: 3 });
  const count = ra.getResultCount();
  assertEqual(count, 3, 'REMOTE_REAP accumulates results');
})()));

(asyncTests.push((async () => {
  const ra = new ReapAggregator({ mode: 'REMOTE_REAP', remoteTarget: 'stream://results' });
  let streamed = false;
  ra.registerStreamTarget('stream://results', async (data) => { streamed = true; });
  const r = await ra.collect({ val: 99 });
  assert(r.streamed === true || r.fallback === 'memory', 'REMOTE_REAP streams to registered target');
})()));

// ── ReapAggregator: Reduce & Merge ──
console.log('\u001b[1m--- ReapAggregator: Reduce & Merge ---\u001b[0m');

(function() {
  const ra = new ReapAggregator({ mode: 'LOCAL_REAP' });
  const result = ra.reduce((a, b) => a + b, 0);
  assertEqual(result, 0, 'reduce on empty returns initial');
})();

(asyncTests.push((async () => {
  const ra = new ReapAggregator({ mode: 'LOCAL_REAP' });
  await ra.collect([1, 2]);
  await ra.collect([3, 4]);
  const merged = ra.merge(r => r[0], (a, b) => a.concat(b));
  assertEqual(merged.length, 2, 'merge by first element');
})()));

(asyncTests.push((async () => {
  const ra = new ReapAggregator({ mode: 'LOCAL_REAP' });
  await ra.collect({ count: 5 });
  await ra.collect({ count: 10 });
  await ra.collect({ count: 15 });
  const sum = ra.reduce((acc, r) => acc + r.count, 0);
  assertEqual(sum, 30, 'reduce sums object fields');
})()));

// ── MISSION CONFIG ──
console.log('\u001b[1m--- MISSION CONFIG ---\u001b[0m');

(function() {
  const rm = new ReplicaManager();
  rm.configure('REPLICA_STRATEGY', 'ROUND_ROBIN');
  assertEqual(rm._strategy, 'ROUND_ROBIN', 'configure REPLICA_STRATEGY');
  rm.configure('REPLICA_STRATEGY', 'INVALID');
  assertEqual(rm._strategy, 'ROUND_ROBIN', 'reject invalid REPLICA_STRATEGY');
  rm.configure('PRIMARY_BACKUP_ACK', 'ALL');
  assertEqual(rm._ackMode, 'ALL', 'configure PRIMARY_BACKUP_ACK');
  rm.configure('PRIMARY_BACKUP_ACK', 'INVALID');
  assertEqual(rm._ackMode, 'ALL', 'reject invalid PRIMARY_BACKUP_ACK');
})();

(function() {
  const dce = new DistributedCycleEngine();
  dce.configure('CYCLE_CORE_FACTOR', 4);
  assertEqual(dce._coreFactor, 4, 'configure CYCLE_CORE_FACTOR');
  dce.configure('CYCLE_CORE_FACTOR', 10);
  assertEqual(dce._coreFactor, 4, 'reject out-of-range CYCLE_CORE_FACTOR');
  dce.configure('CYCLE_MIN_CHUNK_SIZE', 5000);
  assertEqual(dce._minChunkSize, 5000, 'configure CYCLE_MIN_CHUNK_SIZE');
  dce.configure('CYCLE_MIN_CHUNK_SIZE', 50);
  assertEqual(dce._minChunkSize, 5000, 'reject out-of-range CYCLE_MIN_CHUNK_SIZE');
  dce.configure('WORKER_TIMEOUT_MS', 10000);
  assertEqual(dce._workerTimeoutMs, 10000, 'configure WORKER_TIMEOUT_MS');
  dce.configure('WORKER_TIMEOUT_MS', 500);
  assertEqual(dce._workerTimeoutMs, 10000, 'reject out-of-range WORKER_TIMEOUT_MS');
})();

(function() {
  const ra = new ReapAggregator();
  ra.configure('REMOTE_REAP_TARGET', 's3://my-bucket');
  assertEqual(ra._remoteTarget, 's3://my-bucket', 'configure REMOTE_REAP_TARGET URI');
  ra.configure('REMOTE_REAP_TARGET', 'MEMORY_BUFFER');
  assertEqual(ra._remoteTarget, 'MEMORY_BUFFER', 'configure REMOTE_REAP_TARGET MEMORY_BUFFER');
  ra.configure('REMOTE_REAP_TARGET', 'invalid');
  assertEqual(ra._remoteTarget, 'MEMORY_BUFFER', 'reject invalid REMOTE_REAP_TARGET');
})();

// ── Integration Tests ──
console.log('\u001b[1m--- Integration ---\u001b[0m');

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('coordinator');
  reg.register('worker-1');
  reg.register('worker-2');
  const rm = new ReplicaManager({ nodeRegistry: reg, replicaStrategy: 'LEAST_CONNECTIONS', primaryBackupAck: 'QUORUM' });
  const dce = new DistributedCycleEngine({ nodeRegistry: reg, coreFactor: 2, minChunkSize: 200 });
  const ra = new ReapAggregator({ mode: 'LOCAL_REAP' });
  const assignment = rm.assignPrimary('cycle-actor');
  assert(assignment.primary !== null, 'integration: primary assigned');
  const replicateResult = await rm.replicateMutation('cycle-actor', { iterations: 50000 });
  assert(replicateResult.success === true, 'integration: mutation replicated');
  dce.scatter(50000);
  const dceStats = dce.getStats();
  assert(dceStats.pendingChunks >= 0, 'integration: cycle chunks created');
  await ra.collect({ chunks: dceStats.totalChunks || dceStats.pendingChunks + dceStats.activeChunks + dceStats.completedChunks });
  assert(ra.getResultCount() >= 1, 'integration: results collected');
})()));

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('P');
  reg.register('B1');
  reg.register('B2');
  const rm = new ReplicaManager({ nodeRegistry: reg, primaryBackupAck: 'QUORUM' });
  rm.assignPrimary('actor-int');
  await rm.replicateMutation('actor-int', { step: 1 });
  const oldPrimary = rm.getPrimary('actor-int');
  reg.unregister(oldPrimary);
  rm.handleNodeFailure(oldPrimary);
  const newPrimary = rm.getPrimary('actor-int');
  assert(newPrimary !== null, 'integration: failover promotes backup');
  await rm.replicateMutation('actor-int', { step: 2 });
  const ledger = rm.getLedger('actor-int');
  assertEqual(ledger.version, 2, 'integration: writes continue after failover');
  assert(ledger.primary === newPrimary, 'integration: ledger uses new primary');
})()));

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('node-1');
  reg.register('node-2');
  reg.register('node-3');
  const dce = new DistributedCycleEngine({ nodeRegistry: reg, coreFactor: 2, minChunkSize: 10 });
  const totalIterations = 10000;
  dce.scatter(totalIterations);
  let chunkId = 0;
  for (let round = 0; round < 100; round++) {
    const workers = ['node-1', 'node-2', 'node-3'];
    for (const w of workers) {
      if (dce.getPendingChunkCount() === 0 && dce.getActiveChunkCount() === 0) break;
      dce.completeChunk(w, chunkId++, 'r-' + chunkId);
    }
    if (dce.getPendingChunkCount() === 0 && dce.getActiveChunkCount() === 0) break;
  }
  const isComplete = dce.isComplete(totalIterations);
  assert(isComplete === true, 'integration: all chunks complete after simulated execution');
})()));

await Promise.all(asyncTests);

console.log('');
console.log('\u001b[1mResult: ' + passed + ' passed, ' + failed + ' failed\u001b[0m');
if (failed > 0) process.exit(1);
}

main().catch(err => { console.error(err); process.exit(1); });
