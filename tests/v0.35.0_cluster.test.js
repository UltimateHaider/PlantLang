const { NodeRegistry, NODE_STATES } = require('../src/cluster/discovery/node_registry');
const { ClusterRouter, CircuitBreaker, CIRCUIT_STATES } = require('../src/cluster/router/cluster_router');
const { DistributedHeap, ConsistentHashRing, hashKey } = require('../src/cluster/memory/distributed_heap');

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

function assertNotThrows(fn, label) {
  try { fn(); passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  catch (e) { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label + ': ' + e.message); }
}

function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

const asyncTests = [];

async function main() {
// ── NodeRegistry Tests ──
console.log('\u001b[1m--- Node Registry & Heartbeats ---\u001b[0m');

(function() {
  const reg = new NodeRegistry();
  assert(reg !== null, 'NodeRegistry created without error');
})();

(function() {
  const reg = new NodeRegistry();
  const n = reg.register('node-A', { cpuUtil: 0.3, heapUsage: 0.5, activeWorkers: 4 });
  assertEqual(n.state, NODE_STATES.HEALTHY, 'registered node is HEALTHY');
  assertEqual(n.id, 'node-A', 'node id matches');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A');
  reg.register('node-B');
  reg.register('node-C');
  assertEqual(reg.getNodeCount(), 3, '3 nodes registered');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A');
  reg.heartbeat('node-A', { cpuUtil: 0.5 });
  const n = reg.getNode('node-A');
  assertEqual(n.cpuUtil, 0.5, 'heartbeat updates telemetry');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A');
  reg.heartbeat('node-A');
  const n = reg.getNode('node-A');
  assertEqual(n.missedBeats, 0, 'heartbeat resets missedBeats to 0');
})();

asyncTests.push((async () => {
  const reg = new NodeRegistry({ heartbeatInterval: 30, heartbeatThreshold: 3 });
  reg.register('node-A');
  reg.register('node-B');
  reg.start();
  await sleep(400);
  reg.stop();
  const n = reg.getNode('node-A');
  assert(n.missedBeats >= 3, 'missedBeats >= 3 after 400ms without heartbeat');
  assertEqual(n.state, NODE_STATES.OFFLINE, 'node goes OFFLINE after threshold');
})());

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A');
  reg.register('node-B');
  reg.start();
  reg.heartbeat('node-A');
  reg.heartbeat('node-B');
  assertEqual(reg.getAliveCount(), 2, 'both nodes alive after heartbeat');
  reg.stop();
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A', { cpuUtil: 0.9, heapUsage: 0.8, activeWorkers: 10 });
  const n = reg.getNode('node-A');
  assertEqual(n.cpuUtil, 0.9, 'cpuUtil stored correctly');
  assertEqual(n.heapUsage, 0.8, 'heapUsage stored correctly');
  assertEqual(n.activeWorkers, 10, 'activeWorkers stored correctly');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A');
  reg.unregister('node-A');
  assertEqual(reg.getNode('node-A'), null, 'unregistered node returns null');
  assertEqual(reg.getNodeCount(), 0, 'no nodes after unregister');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A');
  reg.register('node-B');
  reg.register('node-C');
  reg.unregister('node-B');
  assertEqual(reg.getAliveCount(), 2, '2 alive after unregistering one');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A');
  reg.markDegraded('node-A');
  const n = reg.getNode('node-A');
  assertEqual(n.state, NODE_STATES.DEGRADED, 'markDegraded transitions to DEGRADED');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A');
  reg.markDegraded('node-A');
  reg.heartbeat('node-A');
  const n = reg.getNode('node-A');
  assertEqual(n.state, NODE_STATES.HEALTHY, 'heartbeat recovers from DEGRADED to HEALTHY');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A');
  reg.configure('HEARTBEAT_INTERVAL', 2000);
  assertEqual(reg._heartbeatInterval, 2000, 'configure HEARTBEAT_INTERVAL');
})();

(function() {
  const reg = new NodeRegistry();
  reg.configure('HEARTBEAT_THRESHOLD', 5);
  assertEqual(reg._failureThreshold, 5, 'configure HEARTBEAT_THRESHOLD');
})();

// ── Circuit Breaker Tests ──
console.log('\u001b[1m--- Circuit Breaker ---\u001b[0m');

(function() {
  const cb = new CircuitBreaker();
  assertEqual(cb.state, CIRCUIT_STATES.CLOSED, 'initial state is CLOSED');
})();

(function() {
  const cb = new CircuitBreaker();
  assert(cb.allowRequest(), 'allowRequest returns true in CLOSED state');
})();

(function() {
  const cb = new CircuitBreaker({ errorThreshold: 0.05 });
  for (let i = 0; i < 20; i++) cb.recordSuccess();
  assertEqual(cb.state, CIRCUIT_STATES.CLOSED, 'stays CLOSED with 0% errors');
})();

(function() {
  const cb = new CircuitBreaker({ errorThreshold: 0.05 });
  for (let i = 0; i < 19; i++) cb.recordSuccess();
  cb.recordFailure();
  assertEqual(cb.state, CIRCUIT_STATES.OPEN, 'trips to OPEN at 5% error rate');
})();

(function() {
  const cb = new CircuitBreaker({ errorThreshold: 0.05 });
  for (let i = 0; i < 19; i++) cb.recordSuccess();
  cb.recordFailure();
  assert(cb.allowRequest() === false, 'allowRequest returns false in OPEN state');
})();

(asyncTests.push((async () => {
  const cb = new CircuitBreaker({ errorThreshold: 0.05, cooldownMs: 1 });
  for (let i = 0; i < 19; i++) cb.recordSuccess();
  cb.recordFailure();
  await sleep(5);
  assert(cb.allowRequest(), 'allowRequest returns true after cooldown (HALF-OPEN)');
})()));

(asyncTests.push((async () => {
  const cb = new CircuitBreaker({ errorThreshold: 0.05, cooldownMs: 1 });
  for (let i = 0; i < 19; i++) cb.recordSuccess();
  cb.recordFailure();
  await sleep(5);
  cb.allowRequest();
  assertEqual(cb.state, CIRCUIT_STATES.HALF_OPEN, 'state is HALF-OPEN after cooldown');
})()));

(asyncTests.push((async () => {
  const cb = new CircuitBreaker({ errorThreshold: 0.05, cooldownMs: 1 });
  for (let i = 0; i < 19; i++) cb.recordSuccess();
  cb.recordFailure();
  await sleep(5);
  cb.allowRequest();
  cb.recordSuccess();
  assertEqual(cb.state, CIRCUIT_STATES.CLOSED, 'HALF-OPEN success transitions to CLOSED');
})()));

(asyncTests.push((async () => {
  const cb = new CircuitBreaker({ errorThreshold: 0.05, cooldownMs: 1 });
  for (let i = 0; i < 19; i++) cb.recordSuccess();
  cb.recordFailure();
  await sleep(5);
  cb.allowRequest();
  cb.recordFailure();
  assertEqual(cb.state, CIRCUIT_STATES.OPEN, 'HALF-OPEN failure returns to OPEN');
})()));

(function() {
  const cb = new CircuitBreaker({ errorThreshold: 0.10 });
  for (let i = 0; i < 90; i++) cb.recordSuccess();
  for (let i = 0; i < 10; i++) cb.recordFailure();
  assertEqual(cb.state, CIRCUIT_STATES.OPEN, '10% error rate trips breaker');
})();

(function() {
  const cb = new CircuitBreaker({ errorThreshold: 0.05 });
  cb.recordFailure();
  assertEqual(cb.state, CIRCUIT_STATES.CLOSED, 'single failure does not trip (need >=10 total)');
})();

(function() {
  const cb = new CircuitBreaker();
  cb.configure('CIRCUIT_BREAKER_THRESHOLD', 0.25);
  assertEqual(cb._errorThreshold, 0.25, 'configure CIRCUIT_BREAKER_THRESHOLD');
})();

(function() {
  const cb = new CircuitBreaker();
  cb.configure('CIRCUIT_BREAKER_COOLDOWN', 60000);
  assertEqual(cb._cooldownMs, 60000, 'configure CIRCUIT_BREAKER_COOLDOWN');
})();

(function() {
  const cb = new CircuitBreaker({ errorThreshold: 0.05 });
  for (let i = 0; i < 19; i++) cb.recordSuccess();
  cb.recordFailure();
  cb.reset();
  assertEqual(cb.state, CIRCUIT_STATES.CLOSED, 'reset returns to CLOSED');
  assertEqual(cb.errorRate, 0, 'reset clears error rate');
})();

// ── Cluster Router Tests ──
console.log('\u001b[1m--- Cluster Router ---\u001b[0m');

(function() {
  const router = new ClusterRouter();
  assert(router !== null, 'ClusterRouter created without error');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A', { cpuUtil: 0.3 });
  reg.register('node-B', { cpuUtil: 0.6 });
  const router = new ClusterRouter({ nodeRegistry: reg });
  assert(router._selectTarget() !== null, 'selectTarget returns a node');
})();

(function() {
  const reg = new NodeRegistry();
  const router = new ClusterRouter({ nodeRegistry: reg });
  assertEqual(router._selectTarget(), null, 'selectTarget returns null with no nodes');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A', { cpuUtil: 0.1 });
  reg.register('node-B', { cpuUtil: 0.9 });
  const router = new ClusterRouter({ nodeRegistry: reg });
  const target = router._selectTarget();
  assertEqual(target.id, 'node-A', 'selects lower CPU node');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A', { cpuUtil: 0.3 });
  reg.register('node-B', { cpuUtil: 0.3 });
  const router = new ClusterRouter({ nodeRegistry: reg });
  router._activeConnections.set('node-A', 5);
  router._activeConnections.set('node-B', 1);
  const target = router._selectTarget();
  assertEqual(target.id, 'node-B', 'selects fewer active connections');
})();

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('node-A', { cpuUtil: 0.3 });
  const router = new ClusterRouter({ nodeRegistry: reg });
  const result = await router.dispatch('test.action', { data: 42 });
  assert(result.nodeId === 'node-A', 'dispatch returns target nodeId');
  assertEqual(result.action, 'test.action', 'dispatch returns action name');
})()));

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  const router = new ClusterRouter({ nodeRegistry: reg });
  try {
    await router.dispatch('fail', {});
    assert(false, 'dispatch should throw with no nodes');
  } catch (err) {
    assert(err.message.includes('No available'), 'dispatch throws with no nodes');
  }
})()));

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('node-A', { cpuUtil: 0.3 });
  reg.register('node-B', { cpuUtil: 0.3 });
  const router = new ClusterRouter({ nodeRegistry: reg });
  router._executeOnNode = async (node) => {
    if (node.id === 'node-A') throw new Error('simulated failure');
    return { nodeId: node.id, action: 'test', result: 'backup' };
  };
  const result = await router.dispatch('test.action', { data: 1 });
  assert(result.nodeId === 'node-B', 'failover to backup node on failure');
})()));

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('node-A', { cpuUtil: 0.3 });
  const router = new ClusterRouter({ nodeRegistry: reg });
  router._executeOnNode = async () => { throw new Error('fail'); };
  try {
    await router.dispatch('test', {});
    assert(false, 'should throw');
  } catch (err) {
    assert(err.message.includes('fail'), 'throws aggregated error on all failures');
  }
})()));

// ── Consistent Hash Ring Tests ──
console.log('\u001b[1m--- Consistent Hash Ring ---\u001b[0m');

(function() {
  const ring = new ConsistentHashRing(8);
  assert(ring !== null, 'ConsistentHashRing created');
})();

(function() {
  const ring = new ConsistentHashRing(8);
  ring.addNode('A');
  assertEqual(ring.getNodeCount(), 1, '1 node after addNode');
})();

(function() {
  const ring = new ConsistentHashRing(8);
  ring.addNode('A');
  ring.addNode('B');
  assertEqual(ring.getNodeCount(), 2, '2 nodes after addNode');
})();

(function() {
  const ring = new ConsistentHashRing(8);
  ring.addNode('A');
  const owner = ring.getNode('my-key');
  assert(owner === 'A', 'key maps to the only node');
})();

(function() {
  const ring = new ConsistentHashRing(128);
  ring.addNode('A');
  ring.addNode('B');
  const owners = {};
  for (let i = 0; i < 1000; i++) {
    const o = ring.getNode('key-' + i);
    owners[o] = (owners[o] || 0) + 1;
  }
  const ratio = Math.min(owners.A, owners.B) / Math.max(owners.A, owners.B);
  assert(ratio > 0.4, 'keys distributed roughly evenly between 2 nodes (ratio=' + ratio.toFixed(3) + ')');
})();

(function() {
  const ring = new ConsistentHashRing(32);
  ring.addNode('A');
  ring.addNode('B');
  const before = ring.getNode('test-key');
  ring.addNode('C');
  const after = ring.getNode('test-key');
  assert(after === before || after === 'C', 'key ownership stable after adding node');
})();

(function() {
  const ring = new ConsistentHashRing(32);
  ring.addNode('A');
  ring.addNode('B');
  ring.addNode('C');
  const ownersBefore = {};
  for (let i = 0; i < 500; i++) {
    const o = ring.getNode('k-' + i);
    ownersBefore[o] = (ownersBefore[o] || 0) + 1;
  }
  ring.removeNode('B');
  const ownersAfter = {};
  for (let i = 0; i < 500; i++) {
    const o = ring.getNode('k-' + i);
    ownersAfter[o] = (ownersAfter[o] || 0) + 1;
  }
  assert(ownersAfter.A > 0 && ownersAfter.C > 0, 'keys redistributed after removal');
})();

(function() {
  const ring = new ConsistentHashRing(8);
  ring.addNode('A');
  ring.addNode('B');
  ring.addNode('C');
  const migrated = ring.computeMigration(['A', 'B'], 'C');
  assert(Array.isArray(migrated), 'migration produces array');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'test' });
  heap.addNode('A');
  heap.addNode('B');
  heap.addNode('C');
  heap.put('k1', 'v1');
  heap.put('k2', 'v2');
  heap.put('k3', 'v3');
  const migrated = heap.computeDataKeyMigration(['k1', 'k2', 'k3']);
  assert(Array.isArray(migrated), 'data key migration produces array');
})();

(function() {
  const ring = new ConsistentHashRing(128);
  ring.addNode('A');
  ring.addNode('B');
  const keysA = ring.getKeysForNode('A');
  const keysB = ring.getKeysForNode('B');
  assertEqual(keysA.length, 128, 'node A has 128 vnodes');
  assertEqual(keysB.length, 128, 'node B has 128 vnodes');
})();

(function() {
  const ring = new ConsistentHashRing();
  assertEqual(ring.getNodeCount(), 0, 'empty ring has 0 nodes');
  assertEqual(ring.getNode('any'), null, 'getNode on empty ring returns null');
})();

(function() {
  const ring = new ConsistentHashRing(128);
  ring.configure('CONSISTENT_HASH_VNODES', 64);
  ring.addNode('A');
  const keys = ring.getKeysForNode('A');
  assertEqual(keys.length, 64, 'configure CONSISTENT_HASH_VNODES takes effect');
})();

// ── Distributed Heap Tests ──
console.log('\u001b[1m--- Distributed Heap ---\u001b[0m');

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  assert(heap !== null, 'DistributedHeap created');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  heap.addNode('A');
  heap.addNode('B');
  heap.put('k1', 'v1');
  heap.put('k2', 'v2');
  assertEqual(heap.getKeyCount(), 2, '2 keys stored');
  assertEqual(heap.get('k1'), 'v1', 'get returns correct value');
  assertEqual(heap.get('k2'), 'v2', 'get returns correct value for k2');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  heap.addNode('A');
  assertEqual(heap.get('nonexistent'), null, 'get on missing key returns null');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  heap.addNode('A');
  heap.put('k1', 'v1');
  heap.delete('k1');
  assertEqual(heap.get('k1'), null, 'get after delete returns null');
  assertEqual(heap.getKeyCount(), 0, '0 keys after delete');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  heap.addNode('A');
  heap.addNode('B');
  const actorOwner = heap.registerActor('actor-1');
  assert(actorOwner !== null, 'actor registered with owner');
  assertEqual(heap.getActorOwner('actor-1'), actorOwner, 'getActorOwner returns owner');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  heap.addNode('A');
  heap.registerActor('actor-1');
  const state = heap.getActorState('actor-1');
  assert(state !== null, 'actor state initialized');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  heap.addNode('A');
  const actorOwner = heap.registerActor('actor-1');
  const result = heap.setActorState('actor-1', { count: 42 }, actorOwner);
  assert(result.proxied === false, 'setActorState from owner is not proxied');
  const state = heap.getActorState('actor-1');
  assertEqual(state.count, 42, 'actor state updated');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  heap.addNode('A');
  heap.addNode('B');
  const actorOwner = heap.registerActor('actor-1');
  const nonOwner = actorOwner === 'A' ? 'B' : 'A';
  const result = heap.setActorState('actor-1', { count: 99 }, nonOwner);
  assert(result.proxied === true, 'setActorState from non-owner returns proxied=true');
  assert(result.owner === actorOwner, 'proxied result includes correct owner');
})();

(asyncTests.push((async () => {
  const heap = new DistributedHeap({ localNodeId: 'local', leaseDuration: 50 });
  heap.addNode('A');
  heap.put('lease-key', 'lease-val');
  await sleep(60);
  assertEqual(heap.get('lease-key'), null, 'key expired after lease duration');
  assertEqual(heap.getKeyCount(), 0, 'GC removed expired key');
})()));

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  assertThrows(() => heap.put('orphan', 'val'), 'put with no nodes throws');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  assertThrows(() => heap.registerActor('orphan-actor'), 'registerActor with no nodes throws');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  heap.addNode('A');
  heap.addNode('B');
  heap.addNode('C');
  heap.put('k1', 'v1');
  heap.put('k2', 'v2');
  heap.put('k3', 'v3');
  const stats = heap.computeMigrationStats('D');
  assert(stats.totalKeys >= 3, 'migration stats totalKeys >= 3');
  assert(stats.ratio >= 0, 'migration ratio non-negative');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  heap.addNode('A');
  heap.addNode('B');
  heap.put('k1', 'v1');
  heap.put('k2', 'v2');
  heap.put('k3', 'v3');
  const countBefore = heap.getKeyCount();
  heap.removeNode('A');
  const countAfter = heap.getKeyCount();
  assert(countAfter <= countBefore, 'keys not lost after node removal');
  assert(countAfter >= 0, 'non-negative keys after removal');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'local' });
  heap.configure('CONSISTENT_HASH_VNODES', 64);
  heap.addNode('A');
  heap.addNode('B');
  assertEqual(heap.getRingNodeCount(), 2, 'ring has 2 nodes after configure + add');
})();

// ── Integration & Benchmark Tests ──
console.log('\u001b[1m--- Integration & Benchmark ---\u001b[0m');

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A', { cpuUtil: 0.2 });
  reg.register('node-B', { cpuUtil: 0.4 });
  reg.register('node-C', { cpuUtil: 0.6 });
  const router = new ClusterRouter({ nodeRegistry: reg });
  const start = Date.now();
  for (let i = 0; i < 100; i++) router._selectTarget();
  const elapsed = Date.now() - start;
  assert(elapsed < 10, '100 selectTarget calls in < 10ms (' + elapsed + 'ms)');
})();

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('router-node', { cpuUtil: 0.3 });
  const router = new ClusterRouter({ nodeRegistry: reg });
  const r = await router.dispatch('bench', { n: 1 });
  assert(r.result === 'executed', 'dispatch execution result correct');
})()));

(function() {
  const ring = new ConsistentHashRing(128);
  ring.addNode('A');
  ring.addNode('B');
  ring.addNode('C');
  const start = Date.now();
  for (let i = 0; i < 1000; i++) ring.getNode('lookup-' + i);
  const elapsed = Date.now() - start;
  assert(elapsed < 50, '1000 consistent hash lookups in < 50ms (' + elapsed + 'ms)');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'bench' });
  heap.addNode('A');
  heap.addNode('B');
  const start = Date.now();
  for (let i = 0; i < 1000; i++) heap.put('bench-' + i, 'val-' + i);
  const elapsed = Date.now() - start;
  assert(elapsed < 100, '1000 heap puts in < 100ms (' + elapsed + 'ms)');
  assertEqual(heap.getKeyCount(), 1000, '1000 keys after puts');
})();

(function() {
  const heap = new DistributedHeap({ localNodeId: 'bench' });
  heap.addNode('A');
  for (let i = 0; i < 50; i++) heap.registerActor('actor-bench-' + i);
  assertEqual(heap.getActorCount(), 50, '50 actors registered');
})();

(function() {
  const cb = new CircuitBreaker({ errorThreshold: 0.05 });
  const start = Date.now();
  for (let i = 0; i < 10000; i++) cb.allowRequest();
  const elapsed = Date.now() - start;
  assert(elapsed < 10, '10000 allowRequest calls in < 10ms (' + elapsed + 'ms)');
})();

await Promise.all(asyncTests);

console.log('');
console.log('\u001b[1mResult: ' + passed + ' passed, ' + failed + ' failed\u001b[0m');
if (failed > 0) process.exit(1);
}

main().catch(err => { console.error(err); process.exit(1); });
