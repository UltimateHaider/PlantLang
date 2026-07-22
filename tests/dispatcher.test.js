#!/usr/bin/env node
'use strict';
/**
 * tests/dispatcher.test.js — Mission Dispatcher test suite.
 *
 * Tests:
 *   1. MissionStack push/pop/current
 *   2. ScopedArena alloc/write/read/reset
 *   3. MissionDispatcher boundary enforcement
 *   4. SMART threshold routing (N=999 scalar, N=1000 parallel vector)
 *   5. Memory isolation via ScopedArena
 *   6. Multi-hop call chain validation
 */

const { MissionStack, ScopedArena, MissionDispatcher, routeSMART, executeScalarInline, executeParallelVector, SMART_SCALAR_THRESHOLD } = require('../core/dispatcher');
const { BoundaryViolationError } = require('../core/errors');

let passed = 0, failed = 0;

function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m\u2713\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m\u2717\x1b[0m ${label}`); if (detail) console.log(`      \u2192 ${detail}`); failed++; }
}

console.log('\n\x1b[1mMission Dispatcher\x1b[0m\n');
console.log('  \x1b[36m--- MissionStack ---\x1b[0m');

const stack = new MissionStack();
check('Initial mode is BALANCED', stack.current() === 'BALANCED');
check('Initial depth is 1', stack.depth === 1);

stack.push('FAST');
check('Push FAST → current is FAST', stack.current() === 'FAST');
check('Depth is 2', stack.depth === 2);

stack.push('SAFE');
check('Push SAFE → current is SAFE', stack.current() === 'SAFE');
check('Depth is 3', stack.depth === 3);

const popped = stack.pop();
check('Pop returns SAFE', popped === 'SAFE');
check('After pop, current is FAST', stack.current() === 'FAST');

stack.pop();
check('After second pop, current is BALANCED', stack.current() === 'BALANCED');

try {
  stack.pop();
  check('Pop root BALANCED throws error', false, 'no error thrown');
} catch (e) {
  check('Pop root BALANCED throws error', e.message.includes('Cannot pop the root'), e.message);
}

const snap = stack.snapshot();
check('Snapshot returns [BALANCED]', JSON.stringify(snap) === '["BALANCED"]');

console.log('  \x1b[36m--- ScopedArena ---\x1b[0m');

const arena = new ScopedArena(1, 1024);
check('Arena scopeId is 1', arena.scopeId === 1);
check('Arena capacity is 1024', arena.capacity === 1024);
check('Used is 0 initially', arena.used === 0);
check('Remaining is 1024', arena.remaining === 1024);

const ptr1 = arena.alloc(16);
check('First alloc returns offset 0', ptr1 === 0);
check('Used is 16', arena.used === 16);

const ptr2 = arena.alloc(32);
check('Second alloc returns offset 16', ptr2 === 16);
check('Used is 48', arena.used === 48);

// Write and read
const data = Buffer.from([1, 2, 3, 4]);
arena.write(ptr2, data);
const readBack = arena.read(ptr2, 4);
check('Write/read roundtrip',
  readBack[0] === 1 && readBack[1] === 2 && readBack[2] === 3 && readBack[3] === 4);

// Reset
arena.reset();
check('After reset, used is 0', arena.used === 0);
check('After reset, remaining is 1024', arena.remaining === 1024);

// Capacity exceeded
try {
  arena.alloc(2048);
  check('Alloc beyond capacity throws', false, 'no error thrown');
} catch (e) {
  check('Alloc beyond capacity throws', e.message.includes('exceeds capacity'), e.message);
}

console.log('  \x1b[36m--- MissionDispatcher ---\x1b[0m');

const dispatcher = new MissionDispatcher();

// Allowed call
let callCount = 0;
const result = dispatcher.dispatch('BALANCED', 'FAST', (data) => {
  callCount++;
  return data * 2;
}, 21);
check('Allowed dispatch returns correct result', result === 42);
check('Allowed dispatch executes function', callCount === 1);
check('Stack back to BALANCED after dispatch', dispatcher.missionStack.current() === 'BALANCED');

// Denied call: SAFE -> FAST
try {
  dispatcher.dispatch('SAFE', 'FAST', () => 'should not run', null);
  check('SAFE -> FAST throws BoundaryViolationError', false, 'no error thrown');
} catch (e) {
  check('SAFE -> FAST throws BoundaryViolationError', e instanceof BoundaryViolationError, e.message);
  check('SAFE -> FAST exact message', e.message.includes('SAFE is isolated and cannot invoke unguarded FAST code'), e.message);
}

// Denied call: SAFE -> PERSISTENT
try {
  dispatcher.dispatch('SAFE', 'PERSISTENT', () => 'should not run', null);
  check('SAFE -> PERSISTENT throws BoundaryViolationError', false, 'no error thrown');
} catch (e) {
  check('SAFE -> PERSISTENT throws BoundaryViolationError', e instanceof BoundaryViolationError, e.message);
}

// Denied call: FAST -> SAFE
try {
  dispatcher.dispatch('FAST', 'SAFE', () => 'should not run', null);
  check('FAST -> SAFE throws BoundaryViolationError', false, 'no error thrown');
} catch (e) {
  check('FAST -> SAFE throws BoundaryViolationError', e instanceof BoundaryViolationError, e.message);
}

// Denied call: SAFE -> SMART
try {
  dispatcher.dispatch('SAFE', 'SMART', () => 'should not run', null);
  check('SAFE -> SMART throws BoundaryViolationError', false, 'no error thrown');
} catch (e) {
  check('SAFE -> SMART throws BoundaryViolationError', e instanceof BoundaryViolationError, e.message);
}

console.log('  \x1b[36m--- SMART Threshold Routing ---\x1b[0m');

// N < 1000 → scalar (processes entire array as one unit)
const smallData = Array.from({ length: 999 }, (_, i) => i);
let scalarCallCount = 0;
const scalarResult = routeSMART((arr) => { scalarCallCount++; return arr.map(x => x * 2); }, smallData);
check('SMART N=999 uses scalar path', scalarCallCount === 1);
check('SMART scalar result is from direct call',
  JSON.stringify(scalarResult) === JSON.stringify(smallData.map(x => x * 2)));

// N >= 1000 → parallel vector (processes element-by-element)
const largeData = Array.from({ length: 1000 }, (_, i) => i);
let vectorCallCount = 0;
const vectorResult = routeSMART((item) => { vectorCallCount++; return item * 2; }, largeData);
check('SMART N=1000 uses vector path', vectorCallCount === 1000);
check('SMART vector result length', vectorResult.length === 1000);
check('SMART vector first element correct', vectorResult[0] === 0);
check('SMART vector last element correct', vectorResult[999] === 1998);

// Smart dispatcher integration
const smartDispatcher = new MissionDispatcher();
const smartResult = smartDispatcher.dispatch('BALANCED', 'SMART', (data) => {
  return data.map(x => x + 1);
}, [1, 2, 3]);
check('SMART dispatch via MissionDispatcher', JSON.stringify(smartResult) === '[2,3,4]');

console.log('  \x1b[36m--- Multi-hop call chains ---\x1b[0m');

// BALANCED -> FAST -> SAFE must fail at step 2 (FAST -> SAFE)
const chainStack = new MissionStack();
const chainDispatcher = new MissionDispatcher({ missionStack: chainStack });

// Step 1: BALANCED -> FAST (allowed)
chainDispatcher.dispatch('BALANCED', 'FAST', () => 'step 1 ok', null);

// Step 2: FAST -> SAFE (denied)
try {
  chainDispatcher.dispatch('FAST', 'SAFE', () => 'step 2 should not run', null);
  check('BALANCED->FAST->SAFE fails at FAST->SAFE', false, 'no error at step 2');
} catch (e) {
  check('BALANCED->FAST->SAFE fails at FAST->SAFE', e instanceof BoundaryViolationError, e.message);
}

// BALANCED -> PERSISTENT -> FAST must succeed completely (both allowed)
const chainStack2 = new MissionStack();
const chainDispatcher2 = new MissionDispatcher({ missionStack: chainStack2 });

let chainSteps = [];
chainDispatcher2.dispatch('BALANCED', 'PERSISTENT', () => { chainSteps.push('persistent'); return 'ok'; }, null);
chainDispatcher2.dispatch('PERSISTENT', 'FAST', () => { chainSteps.push('fast'); return 'ok'; }, null);
check('BALANCED->PERSISTENT->FAST chain completes', chainSteps.length === 2 &&
  chainSteps[0] === 'persistent' && chainSteps[1] === 'fast',
  `steps: ${JSON.stringify(chainSteps)}`);

console.log('  \x1b[36m--- Memory Isolation (SAFE mode) ---\x1b[0m');

const isoArena = new ScopedArena(99, 256);
const p1 = isoArena.alloc(10);
isoArena.write(p1, Buffer.from([0xAA, 0xBB, 0xCC]));
check('SAFE arena alloc succeeds', p1 === 0);

// Reset and verify isolation
isoArena.reset();
const p2 = isoArena.alloc(10);
check('After reset, offset is 0 again', p2 === 0);

// Write new data after reset
isoArena.write(p2, Buffer.from([0x11, 0x22]));
const afterReset = isoArena.read(p2, 2);
check('After reset, old data is overwritten',
  afterReset[0] === 0x11 && afterReset[1] === 0x22);

// Arena isolation: different scopeIds are independent
const arenaA = new ScopedArena(1, 64);
const arenaB = new ScopedArena(2, 64);
arenaA.alloc(20);
arenaB.alloc(30);
check('Arena A used is 20', arenaA.used === 20);
check('Arena B used is 30', arenaB.used === 30);
arenaA.reset();
check('After A.reset, A used is 0', arenaA.used === 0);
check('After A.reset, B used is still 30', arenaB.used === 30);

console.log(`\n\x1b[1mResult: ${passed} passed, ${failed} failed\x1b[0m\n`);
process.exit(failed > 0 ? 1 : 0);
