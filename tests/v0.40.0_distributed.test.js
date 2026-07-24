const { GeoTopologyManager } = require('../src/cluster/topology/geo_topology');
const { StreamCompactor } = require('../src/cluster/reap/stream_compactor');
const { DistributedCycleEngine } = require('../src/cluster/cycles/distributed_cycle_engine');
const { ReplicaManager } = require('../src/cluster/replica/replica_manager');
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

function assertNotThrows(fn, label) {
  try { fn(); passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  catch (e) { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label + ': ' + e.message); }
}

function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

const asyncTests = [];

async function main() {

// ── GeoTopologyManager: Latency Matrix & Optimal Nodes ──
console.log('\u001b[1m--- GeoTopologyManager: Latency Matrix & Optimal Nodes ---\u001b[0m');

(function() {
  const geo = new GeoTopologyManager();
  assert(geo !== null, 'GeoTopologyManager created without error');
  assertEqual(geo.getStats().nodeCount, 0, 'empty topology has zero nodes');
})();

(function() {
  const geo = new GeoTopologyManager();
  geo.registerNode('node-us-1', { region: 'us', zone: 'us-east', datacenter: 'us-east-1a', localityKey: 'us-east', weight: 1 });
  geo.registerNode('node-us-2', { region: 'us', zone: 'us-east', datacenter: 'us-east-1b', localityKey: 'us-east', weight: 1 });
  geo.registerNode('node-eu-1', { region: 'eu', zone: 'eu-west', datacenter: 'eu-west-1a', localityKey: 'eu-west', weight: 1 });
  assertEqual(geo.getStats().nodeCount, 3, 'three nodes registered');
  geo.probeAll();
  const latEE = geo.getLatency('node-us-1', 'node-us-2');
  assert(latEE < 5, 'same-datacenter latency < 5ms (got ' + latEE + 'ms)');
  const latCross = geo.getLatency('node-us-1', 'node-eu-1');
  assert(latCross > 10, 'cross-region latency > 10ms (got ' + latCross + 'ms)');
})();

(function() {
  const geo = new GeoTopologyManager();
  geo.registerNode('node-us-1', { region: 'us', zone: 'us-east', datacenter: 'us-east-1a', localityKey: 'us-east', weight: 1 });
  geo.registerNode('node-us-2', { region: 'us', zone: 'us-west', datacenter: 'us-west-1a', localityKey: 'us-west', weight: 1 });
  geo.registerNode('node-us-3', { region: 'us', zone: 'us-east', datacenter: 'us-east-1b', localityKey: 'us-east', weight: 1 });
  geo.probeAll();
  const optimal = geo.getOptimalNodes('us-east', 2);
  assertEqual(optimal.length, 2, 'getOptimalNodes returns requested count');
  assert(optimal[0].localityKey === 'us-east', 'first optimal node matches dataLocalityKey');
  const latBetween = geo.getLatency(optimal[0].nodeId || optimal[0].id, optimal[1].nodeId || optimal[1].id);
  assert(latBetween < 10, 'optimal nodes have low inter-node latency (got ' + latBetween + 'ms)');
})();

(function() {
  const geo = new GeoTopologyManager();
  const optimal = geo.getOptimalNodes('us-east', 2);
  assertEqual(optimal.length, 0, 'getOptimalNodes returns empty with no nodes');
})();

// ── StreamCompactor: REAP Stream Compression ──
console.log('\u001b[1m--- StreamCompactor: REAP Stream Compression ---\u001b[0m');

(function() {
  const sc = new StreamCompactor();
  assert(sc !== null, 'StreamCompactor created without error');
  assertEqual(sc.getStats().compressionLevel, 6, 'default compression level is 6');
})();

(function() {
  const sc = new StreamCompactor();
  const headers = { type: 'CYCLE_RESULT', source: 'node-us-1', version: 1 };
  const payload = JSON.stringify({ data: Array(1000).fill('x').join(''), meta: { count: 1000 } });
  const compressed = sc.compressReapStream(headers, payload);
  assert(Buffer.isBuffer(compressed), 'compressReapStream returns a Buffer');
  assert(compressed.length > 0, 'compressed buffer is non-empty');
  const reduction = Math.round((1 - compressed.length / Buffer.byteLength(payload)) * 100);
  assert(reduction >= 60, 'compression reduction >= 60% (got ' + reduction + '%)');
})();

(function() {
  const sc = new StreamCompactor();
  const headers = { type: 'REAP_AGGREGATE', source: 'node-eu-1', version: 2, tags: ['high', 'priority'] };
  const payload = JSON.stringify({ values: Array(500).fill('test-data-123').join(',') });
  const compressed = sc.compressReapStream(headers, payload);
  const decompressed = sc.decompressReapStream(compressed);
  assert(decompressed !== null, 'decompressReapStream returns result');
  assertEqual(decompressed.headers.type, 'REAP_AGGREGATE', 'decompressed headers match');
  assertEqual(decompressed.headers.source, 'node-eu-1', 'decompressed source matches');
  assertEqual(decompressed.headers.version, 2, 'decompressed version matches');
  assertEqual(decompressed.payload, payload, 'decompressed payload matches original');
})();

(function() {
  const sc = new StreamCompactor();
  assertThrows(() => sc.decompressReapStream('not-a-buffer'), 'decompressReapStream rejects non-Buffer');
  assertThrows(() => sc.decompressReapStream(Buffer.alloc(3)), 'decompressReapStream rejects short buffer');
  assertThrows(() => sc.decompressReapStream(Buffer.from([0x00, 0x00, 0x00, 0x00])), 'decompressReapStream rejects bad magic');
})();

// ── DistributedCycleEngine: Geo-Aware Execution ──
console.log('\u001b[1m--- DistributedCycleEngine: Geo-Aware Execution ---\u001b[0m');

(function() {
  const dce = new DistributedCycleEngine();
  assert(dce !== null, 'DistributedCycleEngine created without error');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-us-1', { cpuUtil: 0.3 });
  reg.register('node-us-2', { cpuUtil: 0.3 });
  const dce = new DistributedCycleEngine({ nodeRegistry: reg });
  const geo = new GeoTopologyManager();
  geo.registerNode('node-us-1', { region: 'us', zone: 'us-east', datacenter: 'us-east-1a', localityKey: 'us-east' });
  geo.registerNode('node-us-2', { region: 'us', zone: 'us-east', datacenter: 'us-east-1b', localityKey: 'us-east' });
  geo.probeAll();
  dce.setGeoTopologyManager(geo);
  const result = dce.executeCycleBlock('test-block-data', 'us-east');
  assert(result.executed === true, 'executeCycleBlock succeeds with geo awareness');
  assert(result.geoAffinity === 'us-east', 'geoAffinity is set on execution result');
  assert(typeof result.workerId === 'string', 'workerId is returned');
})();

(function() {
  const dce = new DistributedCycleEngine();
  const result = dce.executeCycleBlock('test-block');
  assert(result.executed === false, 'executeCycleBlock returns not-executed with no workers');
  assertEqual(result.reason, 'no workers available', 'reason reflects no workers');
})();

// ── ReplicaManager: Node Join / Leave Rebalancing ──
console.log('\u001b[1m--- ReplicaManager: Node Join / Leave Rebalancing ---\u001b[0m');

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A', { cpuUtil: 0.3 });
  reg.register('node-B', { cpuUtil: 0.3 });
  reg.register('node-C', { cpuUtil: 0.3 });
  const rm = new ReplicaManager({ nodeRegistry: reg });
  rm.assignPrimary('actor-1');
  rm.assignPrimary('actor-2');
  rm.assignPrimary('actor-3');
  const initialPrimaries = rm._primaries.size;
  const result = rm.handleNodeJoin('node-D');
  assert(result.rebalanced === true, 'handleNodeJoin returns rebalanced=true');
  assert(result.healed === true, 'handleNodeJoin returns healed=true');
  assertEqual(rm._primaries.size, initialPrimaries, 'primary count unchanged after join');
  assert(rm._replicaLedger.get('actor-1').backups.length >= 1, 'replicas healed after join');
})();

(function() {
  const reg = new NodeRegistry();
  reg.register('node-A', { cpuUtil: 0.3 });
  reg.register('node-B', { cpuUtil: 0.3 });
  reg.register('node-C', { cpuUtil: 0.3 });
  const rm = new ReplicaManager({ nodeRegistry: reg });
  rm.assignPrimary('actor-1');
  rm.assignPrimary('actor-2');
  const leaveResult = rm.handleNodeLeave('node-A');
  assert(typeof leaveResult.affectedActors === 'number', 'handleNodeLeave returns affectedActors count');
  assert(leaveResult.affectedActors >= 0, 'affected actors is non-negative');
})();

// ── Summary ──
console.log('');
const total = passed + failed;
console.log('\u001b[1m--- Summary: ' + passed + '/' + total + ' passed' + (failed > 0 ? ', ' + failed + ' FAILED ---' : ' ---') + '\u001b[0m');
if (failed > 0) process.exit(1);
}

main().catch(e => { console.error('Unhandled:', e); process.exit(1); });
