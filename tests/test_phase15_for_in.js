'use strict';
const assert = require('assert');
const { parse } = require('../core/parser');
const { Interpreter } = require('../core/interpreter');
const { typecheck } = require('../core/typechecker');

let passed = 0;
let failed = 0;

function test(name, fn) {
  try { fn(); passed++; }
  catch (e) { console.error(`  \x1b[31m✗\x1b[0m ${name}: ${e.message}`); failed++; }
}

function assertSource(source) {
  const interp = new Interpreter({ mission: 'SAFE' });
  const programNode = parse(source);
  interp.symbolPass(programNode);
  interp.runProgram(programNode);
  return interp;
}

function showText(interp) {
  return interp.output.filter(o => o.type === 'inf').map(o => o.text).join(',');
}

// ── FOR ... IN array iteration ─────────────────────────────────

test('FOR ... IN iterates array elements', () => {
  const interp = assertSource(
    'CREATE arr([NUM]) TO [1, 2, 3].\n' +
    'FOR x IN arr,\n' +
    '2\\  SHOW x.\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, '1,2,3');
});

test('FOR ... IN empty array body not executed', () => {
  const interp = assertSource(
    'CREATE arr([NUM]) TO [].\n' +
    'FOR x IN arr,\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, '');
});

test('FOR ... IN array with single element', () => {
  const interp = assertSource(
    'CREATE arr([NUM]) TO [42].\n' +
    'FOR x IN arr,\n' +
    '2\\  SHOW x.\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, '42');
});

test('FOR ... IN with TX array', () => {
  const interp = assertSource(
    'CREATE arr([TX]) TO ["a", "b", "c"].\n' +
    'FOR s IN arr,\n' +
    '2\\  SHOW s.\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, 'a,b,c');
});

test('FOR ... IN loop variable does not leak', () => {
  const interp = assertSource(
    'CREATE arr([NUM]) TO [10, 20].\n' +
    'FOR x IN arr,\n' +
    '2\\  SHOW x.\n' +
    '/FOR.\n');
  const post = interp.soil.get('x');
  assert.strictEqual(post, null);
});

test('FOR ... IN nested loops', () => {
  const interp = assertSource(
    'CREATE a([NUM]) TO [1, 2].\n' +
    'CREATE b([NUM]) TO [10, 20].\n' +
    'FOR x IN a,\n' +
    '2\\  FOR y IN b,\n' +
    '3\\    SHOW x + y.\n' +
    '2\\  /FOR.\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, '11,21,12,22');
});

test('FOR ... IN with SET inside body', () => {
  const interp = assertSource(
    'CREATE arr([NUM]) TO [1, 2, 3].\n' +
    'CREATE total(NUM).\n' +
    'FOR x IN arr,\n' +
    '2\\  INCREASE total BY x.\n' +
    '/FOR.\n' +
    'SHOW total.\n');
  const output = showText(interp);
  assert.strictEqual(output, '6');
});

// ── FOR ... IN MAP iteration ───────────────────────────────────

test('FOR ... IN iterates MAP values', () => {
  const interp = assertSource(
    'CREATE m(MAP[NUM,TX]) TO {1: "a", 2: "b", 3: "c"}.\n' +
    'FOR v IN m,\n' +
    '2\\  SHOW v.\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, 'a,b,c');
});

test('FOR ... IN empty MAP body not executed', () => {
  const interp = assertSource(
    'CREATE m(MAP[NUM,TX]).\n' +
    'FOR v IN m,\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, '');
});

test('FOR ... IN MAP with single entry', () => {
  const interp = assertSource(
    'CREATE m(MAP[NUM,TX]) TO {42: "hello"}.\n' +
    'FOR v IN m,\n' +
    '2\\  SHOW v.\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, 'hello');
});

test('FOR ... IN MAP[NUM,NUM] values', () => {
  const interp = assertSource(
    'CREATE m(MAP[NUM,NUM]) TO {1: 100, 2: 200}.\n' +
    'FOR v IN m,\n' +
    '2\\  SHOW v.\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, '100,200');
});

test('FOR ... IN MAP[TX,NUM] values', () => {
  const interp = assertSource(
    'CREATE m(MAP[TX,NUM]) TO {"a": 10, "b": 20}.\n' +
    'FOR v IN m,\n' +
    '2\\  SHOW v.\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, '10,20');
});

test('FOR ... IN MAP values summed', () => {
  const interp = assertSource(
    'CREATE m(MAP[NUM,NUM]) TO {1: 10, 2: 20, 3: 30}.\n' +
    'CREATE total(NUM).\n' +
    'FOR v IN m,\n' +
    '2\\  INCREASE total BY v.\n' +
    '/FOR.\n' +
    'SHOW total.\n');
  const output = showText(interp);
  assert.strictEqual(output, '60');
});

// ── Type checking ──────────────────────────────────────────────

test('typecheck FOR ... IN array passes without errors', () => {
  const diags = typecheck(parse('CREATE arr([NUM]).\nFOR x IN arr,\n/FOR.\n'));
  const errors = diags.filter(d => d.severity === 'error');
  assert.strictEqual(errors.length, 0);
});

test('typecheck FOR ... IN MAP passes without errors', () => {
  const diags = typecheck(parse('CREATE m(MAP[NUM,TX]).\nFOR v IN m,\n/FOR.\n'));
  const errors = diags.filter(d => d.severity === 'error');
  assert.strictEqual(errors.length, 0);
});

test('typecheck FOR ... IN MAP[TX,NUM] passes without errors', () => {
  const diags = typecheck(parse('CREATE m(MAP[TX,NUM]).\nFOR v IN m,\n/FOR.\n'));
  const errors = diags.filter(d => d.severity === 'error');
  assert.strictEqual(errors.length, 0);
});

// ── Edge cases ─────────────────────────────────────────────────

test('FOR ... IN with STOPIF exits early', () => {
  const interp = assertSource(
    'CREATE arr([NUM]) TO [1, 2, 3, 4, 5].\n' +
    'FOR x IN arr,\n' +
    '2\\  STOP IF x IS 3.\n' +
    '2\\  SHOW x.\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, '1,2');
});

test('FOR ... IN with nested IF', () => {
  const interp = assertSource(
    'CREATE arr([NUM]) TO [1, 2, 3, 4, 5].\n' +
    'FOR x IN arr,\n' +
    '2\\  IF x GREATER THAN 3, SHOW x.\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, '4,5');
});

test('FOR ... IN variable shadowing', () => {
  const interp = assertSource(
    'CREATE x(NUM).\n' +
    'SET x TO 99.\n' +
    'CREATE arr([NUM]) TO [1, 2].\n' +
    'FOR x IN arr,\n' +
    '2\\  SHOW x.\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, '1,2');
});

// ── Summary ────────────────────────────────────────────────────

console.log(`\nPhase 15 (FOR...IN): ${passed} passed, ${failed} failed${failed ? ' — SOME TESTS FAILED' : ' — all ok'}`);
if (failed > 0) process.exit(1);
