const { ShareGovernance, CONSENSUS_MODES, SHARE_TYPES } = require('../src/cluster/config/share_governance');
const { CallGraphAnalyzer, DEFAULT_MAX_DEPTH } = require('../src/cluster/affinity/call_graph_analyzer');
const { SmartExecutionRouter, ROUTE_TARGETS } = require('../src/cluster/router/smart_execution_router');
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

// ── ShareGovernance: SHARED_READ Tests ──
console.log('\u001b[1m--- ShareGovernance: SHARED_READ ---\u001b[0m');

(function() {
  const sg = new ShareGovernance();
  assert(sg !== null, 'ShareGovernance created without error');
})();

(function() {
  const sg = new ShareGovernance();
  const parsed = sg.parseDirective('SHARE CONFIG max_connections READ_ONLY');
  assertEqual(parsed.key, 'max_connections', 'parseDirective extracts key');
  assertEqual(parsed.shareType, 'READ_ONLY', 'parseDirective extracts shareType');
  assertEqual(parsed.consensus, null, 'READ_ONLY has no consensus');
})();

(function() {
  const sg = new ShareGovernance();
  const parsed = sg.parseDirective('SHARE CONFIG db_host MUTABLE CONSENSUS=RAFT');
  assertEqual(parsed.key, 'db_host', 'parseDirective MUTABLE key');
  assertEqual(parsed.shareType, 'MUTABLE', 'parseDirective MUTABLE shareType');
  assertEqual(parsed.consensus, 'RAFT', 'MUTABLE RAFT consensus');
})();

(function() {
  const sg = new ShareGovernance();
  const parsed = sg.parseDirective('SHARE CONFIG counter MUTABLE CONSENSUS=CRDT');
  assertEqual(parsed.key, 'counter', 'parseDirective CRDT key');
  assertEqual(parsed.consensus, 'CRDT', 'MUTABLE CRDT consensus');
})();

(function() {
  const sg = new ShareGovernance();
  assertThrows(() => sg.parseDirective('INVALID'), 'parseDirective rejects invalid directive');
  assertThrows(() => sg.parseDirective('SHARE CONFIG x INVALID'), 'parseDirective rejects invalid share type');
  assertThrows(() => sg.parseDirective('SHARE CONFIG x MUTABLE CONSENSUS=INVALID'), 'parseDirective rejects invalid consensus');
})();

(function() {
  const sg = new ShareGovernance();
  const entry = sg.declareReadOnly('timeout', 5000);
  assertEqual(entry.version, 1, 'declareReadOnly sets version 1');
  assertEqual(entry.value, 5000, 'declareReadOnly stores value');
})();

(function() {
  const sg = new ShareGovernance();
  sg.declareReadOnly('timeout', 5000);
  const result = sg.read('timeout');
  assert(result !== null, 'read returns entry for existing key');
  assertEqual(result.value, 5000, 'read returns correct value');
  assertEqual(result.source, 'local', 'read returns local source');
})();

(function() {
  const sg = new ShareGovernance();
  const result = sg.read('nonexistent');
  assertEqual(result, null, 'read returns null for missing key');
})();

(function() {
  const sg = new ShareGovernance();
  sg.declareReadOnly('config', { host: 'localhost', port: 8080 });
  const result = sg.read('config');
  assertEqual(result.value.host, 'localhost', 'read returns object value');
  assertEqual(result.value.port, 8080, 'read returns nested value');
})();

(function() {
  const sg = new ShareGovernance();
  sg.declareReadOnly('version', 1);
  sg.invalidate('version', 2);
  const result = sg.read('version');
  assertEqual(result.value, 2, 'invalidate updates value');
  assertEqual(result.version, 2, 'invalidate bumps version');
})();

(function() {
  const sg = new ShareGovernance();
  sg.declareReadOnly('version', 1);
  sg.invalidate('version', 2);
  sg.invalidate('version', 3);
  const result = sg.read('version');
  assertEqual(result.value, 3, 'multiple invalidates converge to latest');
  assertEqual(result.version, 3, 'version is 3 after 2 invalidates');
})();

(function() {
  const sg = new ShareGovernance();
  const result = sg.invalidate('missing', 1);
  assertEqual(result, false, 'invalidate on missing key returns false');
})();

// O(1) Local Read Benchmark
(function() {
  const sg = new ShareGovernance();
  for (let i = 0; i < 100; i++) sg.declareReadOnly('k' + i, i);
  const start = Date.now();
  for (let i = 0; i < 100000; i++) sg.read('k' + (i % 100));
  const elapsed = Date.now() - start;
  assert(elapsed < 100, '100000 SHARED_READ reads in < 100ms (' + elapsed + 'ms)');
})();

// ── ShareGovernance: Gossip Invalidation ──
console.log('\u001b[1m--- ShareGovernance: Gossip Propagation ---\u001b[0m');

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A');
  reg.register('node-B');
  const sgA = new ShareGovernance({ nodeRegistry: reg, nodeId: 'node-A', gossipPropagationMs: 10 });
  const sgB = new ShareGovernance({ nodeRegistry: reg, nodeId: 'node-B', gossipPropagationMs: 10 });
  sgA.addPeer('node-B');
  sgB.addPeer('node-A');
  sgA.declareReadOnly('shared_key', 'initial');
  const batch = sgA._gossipQueue.slice();
  sgA._flushGossip();
  sgB.receiveGossip({ batch, origin: 'node-A' });
  const result = sgB.read('shared_key');
  assert(result !== null, 'gossip propagates SHARED_READ to peer');
  assertEqual(result.value, 'initial', 'gossip carries correct value');
  sgA.stop(); sgB.stop();
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A');
  reg.register('node-B');
  const sgA = new ShareGovernance({ nodeRegistry: reg, nodeId: 'node-A', gossipPropagationMs: 10 });
  const sgB = new ShareGovernance({ nodeRegistry: reg, nodeId: 'node-B', gossipPropagationMs: 10 });
  sgA.addPeer('node-B');
  sgB.addPeer('node-A');
  sgA.declareReadOnly('key', 1);
  let batch = sgA._gossipQueue.slice();
  sgA._flushGossip();
  sgB.receiveGossip({ batch, origin: 'node-A' });
  sgA.invalidate('key', 42);
  batch = sgA._gossipQueue.slice();
  sgA._flushGossip();
  sgB.receiveGossip({ batch, origin: 'node-A' });
  const result = sgB.read('key');
  assertEqual(result.value, 42, 'gossip propagates invalidation with new value');
  assertEqual(result.version, 2, 'gossip propagates bumped version');
  sgA.stop(); sgB.stop();
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A');
  const sgA = new ShareGovernance({ nodeRegistry: reg, nodeId: 'node-A', gossipPropagationMs: 50 });
  const sgB = new ShareGovernance({ nodeRegistry: reg, nodeId: 'node-B', gossipPropagationMs: 50 });
  sgA.addPeer('node-B');
  sgB.addPeer('node-A');
  sgA.declareReadOnly('fast', 'value');
  const batch = sgA._gossipQueue.slice();
  sgA._flushGossip();
  const start = Date.now();
  sgB.receiveGossip({ batch, origin: 'node-A' });
  const elapsed = Date.now() - start;
  assert(elapsed < 50, 'gossip receive within GOSSIP_PROPAGATION_MS (' + elapsed + 'ms)');
  sgA.stop(); sgB.stop();
})();

// ── ShareGovernance: SHARED_WRITE Raft ──
console.log('\u001b[1m--- ShareGovernance: SHARED_WRITE Raft ---\u001b[0m');

(function() {
  const sg = new ShareGovernance({ consensusEngine: 'RAFT' });
  const entry = sg.declareMutable('leader_key');
  assert(entry !== null, 'declareMutable returns entry');
  assertEqual(entry.consensus, 'RAFT', 'Raft consensus set');
})();

(function() {
  const sg = new ShareGovernance({ consensusEngine: 'RAFT' });
  sg.declareMutable('my_key');
  const result = sg.readWrite('my_key');
  assert(result !== null, 'readWrite returns entry');
  assertEqual(result.consensus, 'RAFT', 'readWrite reports RAFT');
})();

(asyncTests.push((async () => {
  const sg = new ShareGovernance({ consensusEngine: 'RAFT' });
  sg.declareMutable('config_key');
  const result = await sg.write('config_key', { db: 'primary' });
  assert(result.success === true, 'Raft write succeeds');
  assert(result.consensus === 'RAFT', 'Raft write reports RAFT consensus');
  const read = sg.readWrite('config_key');
  assertEqual(read.value.db, 'primary', 'Raft write stores value');
  sg.stop();
})()));

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('node-A');
  reg.register('node-B');
  reg.register('node-C');
  const sg = new ShareGovernance({ nodeRegistry: reg, consensusEngine: 'RAFT', nodeId: 'node-A' });
  sg.addPeer('node-B');
  sg.addPeer('node-C');
  sg.declareMutable('cluster_config');
  const result = await sg.write('cluster_config', { replicas: 3 });
  assert(result.success === true, 'Raft write with followers succeeds');
  assert(typeof result.commitIndex === 'number', 'Raft write returns commitIndex');
  sg.stop();
})()));

(asyncTests.push((async () => {
  const sg = new ShareGovernance({ consensusEngine: 'RAFT' });
  sg.declareMutable('counter');
  await sg.write('counter', 1);
  await sg.write('counter', 2);
  const read = sg.readWrite('counter');
  assertEqual(read.value, 2, 'Raft sequential writes converge to latest');
  assertEqual(read.committed, 2, 'Raft committed index advances');
  sg.stop();
})()));

(asyncTests.push((async () => {
  const sg = new ShareGovernance({ consensusEngine: 'RAFT' });
  sg.declareMutable('test');
  await sg.write('test', 'a');
  await sg.write('test', 'b');
  await sg.write('test', 'c');
  const read = sg.readWrite('test');
  assertEqual(read.value, 'c', 'Raft multiple writes converge');
  sg.stop();
})()));

// ── ShareGovernance: SHARED_WRITE CRDT ──
console.log('\u001b[1m--- ShareGovernance: SHARED_WRITE CRDT ---\u001b[0m');

(function() {
  const sg = new ShareGovernance({ consensusEngine: 'CRDT' });
  const entry = sg.declareMutable('crdt_key', 'CRDT');
  assertEqual(entry.consensus, 'CRDT', 'CRDT consensus set');
})();

(asyncTests.push((async () => {
  const sg = new ShareGovernance({ consensusEngine: 'CRDT' });
  sg.declareMutable('state');
  const r1 = await sg.write('state', { count: 1 });
  assert(r1.success === true, 'CRDT write succeeds');
  assertEqual(r1.consensus, 'CRDT', 'CRDT write reports CRDT');
  sg.stop();
})()));

(asyncTests.push((async () => {
  const sgA = new ShareGovernance({ consensusEngine: 'CRDT', nodeId: 'node-A' });
  const sgB = new ShareGovernance({ consensusEngine: 'CRDT', nodeId: 'node-B' });
  sgA.addPeer('node-B');
  sgB.addPeer('node-A');
  sgA.declareMutable('shared_state', 'CRDT');
  sgB.declareMutable('shared_state', 'CRDT');
  await sgA.write('shared_state', { count: 1 });
  await sgB.write('shared_state', { count: 2 });
  const mergeA = sgA._writeStore.get('shared_state').value;
  const mergeB = sgB._writeStore.get('shared_state').value;
  sgA.crdtMerge('shared_state', mergeB);
  sgB.crdtMerge('shared_state', mergeA);
  const readA = sgA.readWrite('shared_state');
  const readB = sgB.readWrite('shared_state');
  assert(readA.value.value.count === readB.value.value.count, 'CRDT merged state converges (A === B: count=' + readA.value.value.count + ')');
  sgA.stop(); sgB.stop();
})()));

(asyncTests.push((async () => {
  const sg = new ShareGovernance({ consensusEngine: 'CRDT' });
  sg.declareMutable('counter');
  for (let i = 0; i < 10; i++) await sg.write('counter', i);
  const read = sg.readWrite('counter');
  assertEqual(read.value.value, 9, 'CRDT converges after 10 sequential writes');
  sg.stop();
})()));

// ── CallGraphAnalyzer Tests ──
console.log('\u001b[1m--- CallGraphAnalyzer ---\u001b[0m');

(function() {
  const cga = new CallGraphAnalyzer();
  assert(cga !== null, 'CallGraphAnalyzer created without error');
})();

(function() {
  const cga = new CallGraphAnalyzer();
  cga.addFunction('main', ['readFile', 'parseData']);
  assertEqual(cga.getNodeCount(), 3, 'addFunction adds caller + callees');
  assertEqual(cga.getEdgeCount(), 2, '2 edges for main->readFile, main->parseData');
})();

(function() {
  const cga = new CallGraphAnalyzer();
  cga.addFunction('main', ['helper']);
  cga.addFunction('helper', ['util']);
  assertEqual(cga.getNodeCount(), 3, '3 nodes in chain');
  assertEqual(cga.getEdgeCount(), 2, '2 edges in chain');
})();

(function() {
  const cga = new CallGraphAnalyzer({ maxDepth: 3 });
  cga.addFunction('a', ['b']);
  cga.addFunction('b', ['c']);
  cga.addFunction('c', ['d']);
  cga.addFunction('d', ['e']);
  const depthA = cga.getDepth('a');
  assert(depthA <= 3, 'getDepth bounded at maxDepth 3 (' + depthA + ')');
})();

(function() {
  const cga = new CallGraphAnalyzer({ maxDepth: 5 });
  cga.addFunction('a', ['b']);
  cga.addFunction('b', ['c']);
  cga.addFunction('c', ['d']);
  const depthA = cga.getDepth('a');
  assertEqual(depthA, 3, 'getDepth correctly computes depth 3');
})();

(function() {
  const cga = new CallGraphAnalyzer({ maxDepth: 3 });
  cga.addFunction('root', ['a', 'b', 'c']);
  cga.addFunction('a', ['d']);
  cga.addFunction('b', ['e']);
  cga.addFunction('c', ['f']);
  cga.addFunction('d', ['g']);
  cga.addFunction('g', ['h']);
  assert(cga.getNodeCount() >= 6, 'bounded graph still includes deep nodes');
  const groups = cga.computeAffinityGroups();
  assert(Array.isArray(groups), 'affinity groups is array');
  assert(groups.length > 0, 'at least one affinity group');
})();

(function() {
  const cga = new CallGraphAnalyzer();
  cga.setEdgeWeight('a', 'b', 10);
  cga.setEdgeWeight('b', 'c', 5);
  assertEqual(cga.getEdgeWeight('a', 'b'), 10, 'setEdgeWeight/getEdgeWeight roundtrip');
  assertEqual(cga.getEdgeWeight('b', 'c'), 5, 'edge weight for b->c');
  assertEqual(cga.getEdgeWeight('a', 'c'), 0, 'unset edge returns 0');
})();

(function() {
  const cga = new CallGraphAnalyzer();
  cga.addFunction('main', ['read', 'write']);
  cga.addFunction('read', ['parse']);
  cga.addFunction('parse', ['tokenize']);
  cga.addFunction('write', ['format']);
  cga.addFunction('format', ['escape']);
  const groups = cga.computeAffinityGroups();
  const placement = cga.computePlacement(['node-1', 'node-2']);
  assert(placement.has('main'), 'placement includes main');
  assert(placement.has('read') || placement.has('write'), 'placement includes co-located functions');
  const stats = cga.getStats();
  assert(stats.nodeCount >= 6, 'stats reports node count');
  assert(stats.groupsCount > 0, 'stats reports groups count');
  assert(stats.analyzed === true, 'stats reports analyzed');
})();

(function() {
  const cga = new CallGraphAnalyzer({ maxDepth: 3 });
  const depth = cga.getMaxDepth();
  assertEqual(depth, 3, 'getMaxDepth returns configured maxDepth');
})();

(function() {
  const cga = new CallGraphAnalyzer();
  cga.addFunction('orphan');
  const groups = cga.getAffinityGroups();
  const group = cga.getGroupForFunction('orphan');
  assert(group !== null, 'getGroupForFunction returns group for orphan');
  assert(group.includes('orphan'), 'orphan belongs to its own group');
})();

// ── SmartExecutionRouter Tests ──
console.log('\u001b[1m--- SmartExecutionRouter ---\u001b[0m');

(function() {
  const router = new SmartExecutionRouter();
  assert(router !== null, 'SmartExecutionRouter created without error');
})();

(function() {
  const router = new SmartExecutionRouter();
  router.registerGpuPipeline('nvidia-0');
  assert(router.hasGpuPipeline() === true, 'hasGpuPipeline returns true after registration');
  router.unregisterGpuPipeline('nvidia-0');
  assert(router.hasGpuPipeline() === false, 'hasGpuPipeline returns false after unregistration');
})();

(function() {
  const router = new SmartExecutionRouter();
  router.registerGpuPipeline('gpu-0');
  router.registerGpuPipeline('gpu-1');
  assertEqual(router.getStats().gpuPipelines, 2, 'multiple GPU pipelines registered');
})();

(function() {
  const router = new SmartExecutionRouter();
  assertEqual(router.estimatePayloadSize(42), 8, 'estimatePayloadSize NUM = 8');
  assertEqual(router.estimatePayloadSize('hello'), 5, 'estimatePayloadSize TX = length');
  assertEqual(router.estimatePayloadSize(null), 0, 'estimatePayloadSize null = 0');
  assert(typeof router.estimatePayloadSize([1,2,3]) === 'number', 'estimatePayloadSize array returns number');
  assert(typeof router.estimatePayloadSize({ a: 1 }) === 'number', 'estimatePayloadSize object returns number');
})();

(function() {
  const router = new SmartExecutionRouter();
  assert(router.isMatrixOrVectorOp('mat_multiply') === true, 'isMatrixOrVectorOp detects mat_');
  assert(router.isMatrixOrVectorOp('vector_dot') === true, 'isMatrixOrVectorOp detects vector_');
  assert(router.isMatrixOrVectorOp('tensor_convolution') === true, 'isMatrixOrVectorOp detects tensor_');
  assert(router.isMatrixOrVectorOp('simple_add') === false, 'isMatrixOrVectorOp rejects non-vector op');
})();

(function() {
  const router = new SmartExecutionRouter();
  router.updateLocalCpuLoad(0.3);
  const stats = router.getStats();
  assertEqual(stats.localCpuLoad, 0.3, 'updateLocalCpuLoad sets load correctly');
  router.updateLocalCpuLoad(1.5);
  assertEqual(router.getStats().localCpuLoad, 1, 'updateLocalCpuLoad clamps at 1');
  router.updateLocalCpuLoad(-0.5);
  assertEqual(router.getStats().localCpuLoad, 0, 'updateLocalCpuLoad clamps at 0');
})();

(function() {
  const router = new SmartExecutionRouter();
  router.setLatency('node-remote', 5);
  const lat = router.measureLatency('node-remote');
  assert(lat <= 5.1, 'measureLatency returns set latency (' + lat + ')');
})();

// LOCAL_CPU default
(function() {
  const router = new SmartExecutionRouter();
  const decision = router.selectTarget('simple_add', { x: 1, y: 2 });
  assertEqual(decision.target, ROUTE_TARGETS.LOCAL_CPU, 'default route is LOCAL_CPU');
  assertEqual(decision.reason, 'default target', 'default route reason is default target');
})();

// REMOTE_NODE when CPU is high
(function() {
  const reg = new NodeRegistry();
  reg.register('remote-1', { cpuUtil: 0.3 });
  reg.register('remote-2', { cpuUtil: 0.4 });
  const router = new SmartExecutionRouter({ nodeRegistry: reg, maxLatencyMs: 50 });
  router.updateLocalCpuLoad(0.85);
  router.setLatency('remote-1', 10);
  router.setLatency('remote-2', 20);
  const decision = router.selectTarget('process', { data: [1,2,3] });
  assertEqual(decision.target, ROUTE_TARGETS.REMOTE_NODE, 'high CPU triggers REMOTE_NODE');
  assert(decision.nodeId !== undefined, 'REMOTE_NODE includes nodeId');
})();

// GPU_ACCELERATED for large vector ops
(function() {
  const router = new SmartExecutionRouter({ gpuMinBytes: 100 });
  router.registerGpuPipeline('cuda-0');
  const largeArray = new Array(1000).fill(1.0);
  const decision = router.selectTarget('matrix_multiply', largeArray);
  assertEqual(decision.target, ROUTE_TARGETS.GPU_ACCELERATED, 'large vector op triggers GPU_ACCELERATED');
  assertEqual(decision.reason, 'GPU threshold exceeded', 'GPU reason correct');
})();

// GPU not triggered without pipeline
(function() {
  const router = new SmartExecutionRouter({ gpuMinBytes: 100 });
  const largeArray = new Array(1000).fill(1.0);
  const decision = router.selectTarget('matrix_multiply', largeArray);
  assertEqual(decision.target, ROUTE_TARGETS.LOCAL_CPU, 'no GPU pipeline falls back to LOCAL_CPU');
})();

// GPU not triggered for small payloads
(function() {
  const router = new SmartExecutionRouter({ gpuMinBytes: 999999 });
  router.registerGpuPipeline('cuda-0');
  const decision = router.selectTarget('matrix_multiply', [1, 2, 3]);
  assertEqual(decision.target, ROUTE_TARGETS.LOCAL_CPU, 'small payload stays LOCAL_CPU even with GPU');
})();

// REMOTE not triggered when latency too high
(function() {
  const reg = new NodeRegistry();
  reg.register('far-away', { cpuUtil: 0.1 });
  const router = new SmartExecutionRouter({ nodeRegistry: reg, maxLatencyMs: 5 });
  router.updateLocalCpuLoad(0.85);
  router.setLatency('far-away', 100);
  const decision = router.selectTarget('task', { n: 1 });
  assertEqual(decision.target, ROUTE_TARGETS.LOCAL_CPU, 'high latency remote stays LOCAL_CPU');
})();

// Benchmark: route() overhead < 0.05ms
(function() {
  const router = new SmartExecutionRouter();
  const start = Date.now();
  for (let i = 0; i < 1000; i++) router.selectTarget('op' + i, { data: i });
  const elapsed = Date.now() - start;
  assert(elapsed < 50, '1000 selectTarget calls in < 50ms (' + elapsed + 'ms)');
})();

// ── Integration Tests ──
console.log('\u001b[1m--- Integration ---\u001b[0m');

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('worker-1', { cpuUtil: 0.3 });
  reg.register('worker-2', { cpuUtil: 0.6 });
  const sg = new ShareGovernance({ nodeRegistry: reg, nodeId: 'coordinator', consensusEngine: 'RAFT' });
  sg.addPeer('worker-1');
  sg.addPeer('worker-2');
  sg.declareReadOnly('config', { mode: 'production' });
  sg.declareMutable('counter', 'CRDT');
  await sg.write('counter', { value: 0 });
  const r1 = sg.read('config');
  assertEqual(r1.value.mode, 'production', 'integration: SHARED_READ works');
  const w1 = sg.readWrite('counter');
  assert(w1 !== null, 'integration: SHARED_WRITE works');
  sg.stop();
})()));

(asyncTests.push((async () => {
  const cga = new CallGraphAnalyzer({ maxDepth: 3 });
  cga.addFunction('api_handler', ['auth', 'validate', 'process']);
  cga.addFunction('auth', ['db_lookup']);
  cga.addFunction('process', ['transform', 'enrich', 'store']);
  cga.addFunction('transform', ['compute']);
  const groups = cga.computeAffinityGroups();
  const placement = cga.computePlacement(['node-1', 'node-2', 'node-3']);
  assert(placement.has('api_handler'), 'integration: placement covers api_handler');
  assert(placement.has('auth'), 'integration: placement covers auth');
  assert(placement.has('process'), 'integration: placement covers process');
  const groupFor = cga.getGroupForFunction('api_handler');
  assert(groupFor !== null, 'integration: affinity group found for api_handler');
  if (groupFor) {
    assert(groupFor.includes('auth') || groupFor.includes('process') || groupFor.includes('validate'),
      'integration: co-located functions share affinity group');
  }
})()));

(asyncTests.push((async () => {
  const reg = new NodeRegistry();
  reg.register('node-1', { cpuUtil: 0.2 });
  reg.register('gpu-node', { cpuUtil: 0.1 });
  const router = new SmartExecutionRouter({
    nodeRegistry: reg,
    gpuMinBytes: 200,
    maxLatencyMs: 20,
  });
  router.registerGpuPipeline('cuda-0');
  router.setLatency('node-1', 5);
  router.setLatency('gpu-node', 8);
  const decision1 = router.selectTarget('simple_add', { a: 1, b: 2 });
  assertEqual(decision1.target, ROUTE_TARGETS.LOCAL_CPU, 'integration: small op -> LOCAL_CPU');
  router.updateLocalCpuLoad(0.85);
  const decision2 = router.selectTarget('process_data', { items: [1,2,3] });
  assertEqual(decision2.target, ROUTE_TARGETS.REMOTE_NODE, 'integration: high CPU -> REMOTE_NODE');
  const decision3 = router.selectTarget('matrix_multiply', new Array(500).fill(3.14));
  assertEqual(decision3.target, ROUTE_TARGETS.GPU_ACCELERATED, 'integration: large matrix -> GPU');
})()));

// ── MISSION CONFIG Tests ──
console.log('\u001b[1m--- MISSION CONFIG ---\u001b[0m');

(function() {
  const sg = new ShareGovernance();
  sg.configure('GOSSIP_PROPAGATION_MS', 200);
  assertEqual(sg._gossipPropagationMs, 200, 'configure GOSSIP_PROPAGATION_MS');
  sg.configure('GOSSIP_PROPAGATION_MS', 5);
  assertEqual(sg._gossipPropagationMs, 200, 'reject out-of-range GOSSIP_PROPAGATION_MS');
  sg.configure('CONSENSUS_ENGINE', 'CRDT');
  assertEqual(sg._consensusEngine, 'CRDT', 'configure CONSENSUS_ENGINE to CRDT');
})();

(function() {
  const cga = new CallGraphAnalyzer();
  cga.configure('CALL_GRAPH_MAX_DEPTH', 5);
  assertEqual(cga._maxDepth, 5, 'configure CALL_GRAPH_MAX_DEPTH');
  cga.configure('CALL_GRAPH_MAX_DEPTH', 20);
  assertEqual(cga._maxDepth, 5, 'reject out-of-range CALL_GRAPH_MAX_DEPTH');
})();

(function() {
  const router = new SmartExecutionRouter();
  router.configure('SMART_ROUTE_GPU_MIN_BYTES', 65536);
  assertEqual(router._gpuMinBytes, 65536, 'configure SMART_ROUTE_GPU_MIN_BYTES');
  router.configure('SMART_ROUTE_GPU_MIN_BYTES', 1000);
  assertEqual(router._gpuMinBytes, 65536, 'reject out-of-range SMART_ROUTE_GPU_MIN_BYTES');
  router.configure('SMART_ROUTE_MAX_LATENCY_MS', 50);
  assertEqual(router._maxLatencyMs, 50, 'configure SMART_ROUTE_MAX_LATENCY_MS');
  router.configure('SMART_ROUTE_MAX_LATENCY_MS', 200);
  assertEqual(router._maxLatencyMs, 50, 'reject out-of-range SMART_ROUTE_MAX_LATENCY_MS');
})();

// ── Snapshot / Stats Tests ──
console.log('\u001b[1m--- Snapshots & Stats ---\u001b[0m');

(function() {
  const sg = new ShareGovernance();
  sg.declareReadOnly('k1', 'v1');
  sg.declareMutable('k2', 'RAFT');
  const snap = sg.getSnapshot();
  assert(snap.readStore.k1 !== undefined, 'snapshot includes read store');
  assertEqual(snap.readStore.k1.value, 'v1', 'snapshot read value correct');
  assert(snap.writeStore.k2 !== undefined, 'snapshot includes write store');
  assert(snap.version > 0, 'snapshot includes global version');
})();

(function() {
  const sg = new ShareGovernance();
  sg.declareReadOnly('a', 1);
  sg.declareReadOnly('b', 2);
  sg.declareMutable('c', 'CRDT');
  const stats = sg.getStats();
  assertEqual(stats.readEntries, 2, 'stats readEntries');
  assertEqual(stats.writeEntries, 1, 'stats writeEntries');
})();

(function() {
  const cga = new CallGraphAnalyzer();
  cga.addFunction('x', ['y']);
  cga.addFunction('y', ['z']);
  cga.computeAffinityGroups();
  const stats = cga.getStats();
  assert(stats.nodeCount === 3, 'cga stats nodeCount');
  assert(stats.groupsCount > 0, 'cga stats groupsCount');
})();

(function() {
  const router = new SmartExecutionRouter();
  router.registerGpuPipeline('gpu-0');
  router.updateLocalCpuLoad(0.5);
  const stats = router.getStats();
  assertEqual(stats.gpuPipelines, 1, 'router stats gpuPipelines');
  assertEqual(stats.localCpuLoad, 0.5, 'router stats localCpuLoad');
})();

await Promise.all(asyncTests);

console.log('');
console.log('\u001b[1mResult: ' + passed + ' passed, ' + failed + ' failed\u001b[0m');
if (failed > 0) process.exit(1);
}

main().catch(err => { console.error(err); process.exit(1); });
