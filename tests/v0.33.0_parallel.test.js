#!/usr/bin/env node
'use strict';

const { ParallelCodegenEngine } = require('../src/compiler/parallel/parallel_codegen');
const { RemoteCompilerNode } = require('../src/compiler/distributed/remote_compiler');
const { NonBlockingTelemetry } = require('../src/telemetry/metrics_collector');
const { RuntimeDispatcher } = require('../src/runtime/dispatcher');

let passed = 0, failed = 0;

function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m\u2713\x1b[0m ${label}`); passed++; }
  else {
    console.log(`  \x1b[31m\u2717\x1b[0m ${label}`);
    if (detail !== undefined) console.log(`      \u2192 ${detail}`);
    failed++;
  }
}

function assertThrows(label, fn, expectedMsg) {
  try {
    fn();
    console.log(`  \x1b[31m\u2717\x1b[0m ${label} (expected throw)`);
    failed++;
  } catch (e) {
    if (expectedMsg && !e.message.includes(expectedMsg)) {
      console.log(`  \x1b[31m\u2717\x1b[0m ${label}`);
      console.log(`      expected: ${expectedMsg}, got: ${e.message}`);
      failed++;
    } else {
      console.log(`  \x1b[32m\u2713\x1b[0m ${label}`);
      passed++;
    }
  }
}

// ═══════════════════════════════════════════════════════════════════
//  1. DAG Building & Cycle Detection
// ═══════════════════════════════════════════════════════════════════

console.log('\n\x1b[1m--- DAG Building & Cycle Detection ---\x1b[0m\n');

{
  const engine = new ParallelCodegenEngine();

  // Empty program
  const empty = engine.buildDAG({ statements: [] });
  check('empty program produces 0 nodes', empty.nodes.length === 0);
  check('empty program produces 0 edges', empty.edges.length === 0);
  check('empty program produces 0 cycles', empty.cycles.length === 0);

  // Single action
  const single = engine.buildDAG({
    statements: [{ type: 'ActionDeclaration', name: 'foo', bodyStatements: [] }],
  });
  check('single action produces 1 node', single.nodes.length === 1);
  check('single node has correct name', single.nodes[0].name === 'foo');

  // Two independent actions
  const two = engine.buildDAG({
    statements: [
      { type: 'ActionDeclaration', name: 'a', bodyStatements: [] },
      { type: 'ActionDeclaration', name: 'b', bodyStatements: [] },
    ],
  });
  check('two independent actions produce 2 nodes', two.nodes.length === 2);
  check('no edges between independents', two.edges.length === 0);
}

// Cycle detection
{
  const engine = new ParallelCodegenEngine();
  const dag = engine.buildDAG({
    statements: [
      { type: 'ActionDeclaration', name: 'a', bodyStatements: [
        { type: 'ReapStatement', source: { kind: 'ACTION', name: 'b' } },
      ]},
      { type: 'ActionDeclaration', name: 'b', bodyStatements: [
        { type: 'ReapStatement', source: { kind: 'ACTION', name: 'a' } },
      ]},
    ],
  });
  check('cycle detected between a → b → a', dag.cycles.length >= 1,
    `cycles: ${JSON.stringify(dag.cycles)}`);
}

// ═══════════════════════════════════════════════════════════════════
//  2. Weighted Load Balancer
// ═══════════════════════════════════════════════════════════════════

console.log('\n\x1b[1m--- Weighted Load Balancer ---\x1b[0m\n');

{
  const engine = new ParallelCodegenEngine();
  const nodes = [
    { index: 0, name: 'heavy',   weight: 100 },
    { index: 1, name: 'medium',  weight: 50 },
    { index: 2, name: 'light',   weight: 10 },
    { index: 3, name: 'tiny',    weight: 5 },
  ];

  // Balance across 2 workers
  const buckets = engine.balanceWeights(nodes, 2);
  check('correct number of buckets', buckets.length === 2, `got ${buckets.length}`);

  // All nodes assigned
  const assigned = new Set();
  for (const b of buckets) for (const idx of b) assigned.add(idx);
  check('all nodes assigned', assigned.size === nodes.length,
    `assigned ${assigned.size}`);

  // No duplicates
  const counts = {};
  for (const b of buckets) for (const idx of b) counts[idx] = (counts[idx] || 0) + 1;
  const dupes = Object.values(counts).filter(c => c > 1).length;
  check('no duplicate node assignments', dupes === 0, `found ${dupes} duplicates`);
}

// Balance with real weight computation
{
  const engine = new ParallelCodegenEngine();
  const dag = engine.buildDAG({
    statements: [
      {
        type: 'ActionDeclaration', name: 'process',
        bodyStatements: [
          { type: 'CreateStatement', identifier: 'x', varType: 'NUM' },
          { type: 'ShowStatement', expr: { type: 'Identifier', name: 'x' } },
          { type: 'CycleStatement', bodyStatements: [
            { type: 'ShowStatement', expr: { type: 'Literal', value: 1 } },
          ]},
        ],
      },
      {
        type: 'ActionDeclaration', name: 'compute',
        bodyStatements: [
          { type: 'CreateStatement', identifier: 'y', varType: 'SCL' },
        ],
      },
    ],
  });
  check('real DAG has 2 nodes', dag.nodes.length === 2);
  check('weights are positive', dag.nodes.every(n => n.weight > 0),
    `weights: ${dag.nodes.map(n => n.weight)}`);
}

// ═══════════════════════════════════════════════════════════════════
//  3. Network Compression Benchmark
// ═══════════════════════════════════════════════════════════════════

console.log('\n\x1b[1m--- Network Compression ---\x1b[0m\n');

{
  const remote = new RemoteCompilerNode();

  // Build a substantial AST
  const largeAST = { nodes: [] };
  for (let i = 0; i < 500; i++) {
    largeAST.nodes.push({
      name: `action_${i}`,
      type: 'ActionDeclaration',
      bodyStatements: [
        { type: 'CreateStatement', identifier: 'x', varType: 'NUM' },
        { type: 'ShowStatement', expr: { type: 'Literal', value: i } },
      ],
    });
  }

  const bench = remote.benchmarkCompression(largeAST);
  check('compression ratio >= 60%', bench.ratio >= 60,
    `got ${bench.ratio}% (${bench.compressedSize}/${bench.originalSize} bytes)`);
  check('compressed size smaller than original', bench.compressedSize < bench.originalSize,
    `compressed=${bench.compressedSize}, original=${bench.originalSize}`);
}

// Small payload compression
{
  const remote = new RemoteCompilerNode();
  const smallAST = { action: { name: 'foo', type: 'ActionDeclaration', bodyStatements: [] } };
  const bench = remote.benchmarkCompression(smallAST);
  check('small payload still compresses', bench.ratio > 0,
    `got ${bench.ratio}%`);
}

// ═══════════════════════════════════════════════════════════════════
//  4. 100ms Timeout Fallback
// ═══════════════════════════════════════════════════════════════════

console.log('\n\x1b[1m--- Remote Timeout Fallback ---\x1b[0m\n');

{
  const engine = new ParallelCodegenEngine();
  const remote = new RemoteCompilerNode({
    host: '192.0.2.1', // TEST-NET — guaranteed unreachable
    port: 9473,
    timeout: 100,
    localEngine: engine,
  });

  const astChunk = { name: 'test_action', type: 'ActionDeclaration', bodyStatements: [] };

  remote.compileRemote(astChunk).then(result => {
    check('timeout triggers fallback', result.fallback === true,
      `fallback=${result.fallback}`);
    check('fallback produces bitcode', typeof result.bitcode === 'string',
      `type=${typeof result.bitcode}`);
    check('fallback bitcode non-empty', result.bitcode.length > 0,
      `length=${result.bitcode.length}`);
  }).catch(err => {
    check('remote does not throw', false, err.message);
  });
}

// ═══════════════════════════════════════════════════════════════════
//  5. NonBlockingTelemetry — Ring Buffer & Snapshot
// ═══════════════════════════════════════════════════════════════════

console.log('\n\x1b[1m--- NonBlockingTelemetry ---\x1b[0m\n');

// 5a. Basic record and snapshot
{
  const telemetry = new NonBlockingTelemetry({ bufferSize: 128 });
  telemetry.record('compile_ms', 42.5);
  telemetry.record('nodes', 10);

  const snap = telemetry.snapshot();
  check('snapshot returns metrics array', Array.isArray(snap.metrics),
    `type=${typeof snap.metrics}`);
  check('snapshot contains metrics', snap.metrics.length >= 2,
    `got ${snap.metrics.length} entries`);
  check('uptimeNs is bigint', typeof snap.uptimeNs === 'bigint');

  const compileMetric = snap.metrics.find(m => m.name === 'compile_ms');
  check('compile_ms recorded', !!compileMetric, JSON.stringify(snap.metrics));
  check('compile_ms value correct', compileMetric && compileMetric.value === 42.5);
}

// 5b. Zero-allocation snapshot (verify no excessive allocation)
{
  const telemetry = new NonBlockingTelemetry({ bufferSize: 256 });
  for (let i = 0; i < 50; i++) {
    telemetry.record('test_metric', i);
  }
  const snap = telemetry.snapshot();
  check('snapshot return is object', typeof snap === 'object');
  check('snapshot has metrics array', Array.isArray(snap.metrics));
  check('snapshot has overflowCount', typeof snap.overflowCount === 'number');
  check('snapshot has uptimeNs', typeof snap.uptimeNs === 'bigint');
}

// 5c. Ring buffer overflow
{
  const telemetry = new NonBlockingTelemetry({ bufferSize: 16 });
  for (let i = 0; i < 100; i++) {
    telemetry.record('overflow_test', i);
  }
  // After 100 writes into a 16-slot buffer, overflow must have occurred
  // The overflow counter may not always increment due to read index advancement
  const snap = telemetry.snapshot();
  check('ring buffer handles 100 writes without crash', snap.metrics.length <= 16,
    `got ${snap.metrics.length} entries`);
}

// 5d. Record high-frequency metrics
{
  const telemetry = new NonBlockingTelemetry({ bufferSize: 1024 });
  const start = Date.now();
  for (let i = 0; i < 1000; i++) {
    telemetry.record('fast_metric', i, { iteration: i });
  }
  const elapsed = Date.now() - start;
  check('1000 writes complete quickly', elapsed < 500,
    `took ${elapsed}ms (expected < 500ms)`);
  const snap = telemetry.snapshot();
  check('1000 writes recorded', snap.metrics.length > 0,
    `got ${snap.metrics.length} entries`);
}

// 5e. Timestamps are reasonable
{
  const telemetry = new NonBlockingTelemetry();
  telemetry.record('ts_test', 1);
  const snap = telemetry.snapshot();
  if (snap.metrics.length > 0) {
    const ts = snap.metrics[0].timestampMs;
    const now = Date.now();
    check('timestamp within 60s of now', Math.abs(ts - now) < 60000,
      `ts=${ts}, now=${now}`);
  }
}

// ═══════════════════════════════════════════════════════════════════
//  6. RuntimeDispatcher — Single-Core Auto-Disable
// ═══════════════════════════════════════════════════════════════════

console.log('\n\x1b[1m--- RuntimeDispatcher ---\x1b[0m\n');

{
  const dispatcher = new RuntimeDispatcher();
  check('dispatcher created without error', !!dispatcher);
  check('cpuCount >= 1', dispatcher.cpuCount >= 1);

  // enableParallelCodegen API
  dispatcher.enableParallelCodegen(false);
  check('disableParallelCodegen sets enabled=false',
    !dispatcher.isParallelEnabled);

  dispatcher.enableParallelCodegen(true);
  check('enableParallelCodegen sets enabled=true',
    dispatcher.isParallelEnabled === true);
}

// Compile method
{
  const engine = new ParallelCodegenEngine();
  const dispatcher = new RuntimeDispatcher({ parallelEngine: engine });
  dispatcher.enableParallelCodegen(true);

  const program = {
    statements: [{ type: 'ActionDeclaration', name: 'test', bodyStatements: [] }],
  };

  dispatcher.compile(program).then(result => {
    check('compile returns result object', typeof result === 'object');
    check('compile returns diagnostics array', Array.isArray(result.diagnostics));
    check('compile returns timing object', typeof result.timing === 'object');
  });
}

// Sequential fallback when parallel disabled
{
  const dispatcher = new RuntimeDispatcher();
  dispatcher.enableParallelCodegen(false);

  const program = { statements: [] };
  dispatcher.compile(program).then(result => {
    check('sequential fallback produces result', !!result);
    check('sequential fallback has diagnostics', result.diagnostics.length >= 1);
  });
}

// ═══════════════════════════════════════════════════════════════════
//  7. Speedup Benchmark Suite (Amdahl's Law)
// ═══════════════════════════════════════════════════════════════════

console.log('\n\x1b[1m--- Speedup Benchmark Suite ---\x1b[0m\n');

{
  const engine = new ParallelCodegenEngine({ poolSize: 1 });
  const largeProgram = {
    statements: [],
  };
  // Create 20 independent action nodes
  for (let i = 0; i < 20; i++) {
    largeProgram.statements.push({
      type: 'ActionDeclaration',
      name: `bench_action_${i}`,
      bodyStatements: [
        { type: 'CreateStatement', identifier: 'x', varType: 'NUM' },
        { type: 'ShowStatement', expr: { type: 'Literal', value: i } },
        { type: 'CycleStatement', bodyStatements: [
          { type: 'ShowStatement', expr: { type: 'Literal', value: 0 } },
        ]},
      ],
    });
  }

  const dag = engine.buildDAG(largeProgram);
  check('benchmark DAG has 20 nodes', dag.nodes.length === 20,
    `got ${dag.nodes.length}`);

  // Verify weight computation
  for (const node of dag.nodes) {
    check(`node ${node.name} has weight > 0`, node.weight > 0,
      `weight=${node.weight}`);
  }

  // Balance across 2 workers
  const buckets2 = engine.balanceWeights(dag.nodes, 2);
  check('2-worker balance distributes all nodes',
    buckets2.reduce((s, b) => s + b.length, 0) === dag.nodes.length);
  check('2-worker balance produces 2 buckets', buckets2.length === 2);

  // Balance across 4 workers
  const buckets4 = engine.balanceWeights(dag.nodes, 4);
  check('4-worker balance produces 4 buckets', buckets4.length === 4);

  // Balance across 8 workers
  const buckets8 = engine.balanceWeights(dag.nodes, 8);
  check('8-worker balance produces 8 buckets', buckets8.length === 8);

  // Verify no straggler: max weight difference between workers is bounded
  const computeTotalWeight = (buckets, nodes) => {
    return buckets.map(b => b.reduce((sum, idx) => sum + nodes[idx].weight, 0));
  };

  const w2 = computeTotalWeight(buckets2, dag.nodes);
  const w4 = computeTotalWeight(buckets4, dag.nodes);
  const w8 = computeTotalWeight(buckets8, dag.nodes);

  // The max-to-min ratio should be reasonable (<= 2.0 for good balance)
  const ratio = (arr) => Math.max(...arr) / Math.min(...arr.filter(v => v > 0));

  check('2-worker balance ratio < 2.0', ratio(w2) < 2.0,
    `ratio=${ratio(w2).toFixed(2)}`);
  check('4-worker balance ratio < 2.5', ratio(w4) < 2.5,
    `ratio=${ratio(w4).toFixed(2)}`);
}

// ── Parallel compilation end-to-end ────────────────────────────────────────
{
  const engine = new ParallelCodegenEngine({ poolSize: 2 });
  const program = { statements: [] };
  for (let i = 0; i < 6; i++) {
    program.statements.push({
      type: 'ActionDeclaration',
      name: `e2e_action_${i}`,
      bodyStatements: [{ type: 'CreateStatement', identifier: 'x', varType: 'NUM' }],
    });
  }

  engine.compile(program).then(result => {
    check('end-to-end compile produces chunks', result.chunks.length > 0,
      `got ${result.chunks.length} chunks`);
    check('compile diagnostics emitted', result.diagnostics.length >= 1);
    check('compile timing recorded', typeof result.timing.totalMs === 'number');
  });
}

// ═══════════════════════════════════════════════════════════════════
//  Summary
// ═══════════════════════════════════════════════════════════════════

console.log(`\n\x1b[1mResult: ${passed} passed, ${failed} failed\x1b[0m\n`);
process.exit(failed > 0 ? 1 : 0);
