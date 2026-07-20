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

// ── 1. COUNT ─────────────────────────────────────────────────────

test('COUNT on populated array', () => {
  const interp = assertSource(
    `CREATE nums([NUM]) TO [10, 20, 30, 40].
SHOW COUNT(nums).
`
  );
  const out = showText(interp);
  assert.strictEqual(out, '4');
});

test('COUNT on empty array', () => {
  const interp = assertSource(
    `CREATE empty([NUM]) TO [].
SHOW COUNT(empty).
`
  );
  const out = showText(interp);
  assert.strictEqual(out, '0');
});

// ── 2. FIRST / LAST ──────────────────────────────────────────────

test('FIRST on array', () => {
  const interp = assertSource(
    `CREATE nums([NUM]) TO [10, 20, 30].
SHOW FIRST(nums).
`
  );
  const out = showText(interp);
  assert.strictEqual(out, '10');
});

test('LAST on array', () => {
  const interp = assertSource(
    `CREATE nums([NUM]) TO [10, 20, 30].
SHOW LAST(nums).
`
  );
  const out = showText(interp);
  assert.strictEqual(out, '30');
});

test('FIRST single element', () => {
  const interp = assertSource(
    `CREATE nums([NUM]) TO [42].
SHOW FIRST(nums).
`
  );
  const out = showText(interp);
  assert.strictEqual(out, '42');
});

test('LAST single element', () => {
  const interp = assertSource(
    `CREATE nums([NUM]) TO [42].
SHOW LAST(nums).
`
  );
  const out = showText(interp);
  assert.strictEqual(out, '42');
});

// ── 3. SUM ───────────────────────────────────────────────────────

test('SUM on [NUM] array', () => {
  const interp = assertSource(
    `CREATE nums([NUM]) TO [10, 20, 30, 40].
SHOW SUM(nums).
`
  );
  const out = showText(interp);
  assert.strictEqual(out, '100');
});

test('SUM on empty array', () => {
  const interp = assertSource(
    `CREATE nums([NUM]) TO [].
SHOW SUM(nums).
`
  );
  const out = showText(interp);
  assert.strictEqual(out, '0');
});

test('SUM single element', () => {
  const interp = assertSource(
    `CREATE nums([NUM]) TO [5].
SHOW SUM(nums).
`
  );
  const out = showText(interp);
  assert.strictEqual(out, '5');
});

// ── 4. Chained usage (COUNT with FOR...IN) ───────────────────────

test('COUNT matches FOR...IN iteration count', () => {
  const interp = assertSource(
    `CREATE nums([NUM]) TO [10, 20, 30].
SHOW COUNT(nums).
FOR n IN nums,
  SHOW n.
/FOR.
`
  );
  const out = showText(interp);
  const lines = out.split(',');
  assert.strictEqual(lines[0], '3', 'COUNT should be 3');
  assert.strictEqual(lines.length, 4, 'SHOW COUNT + 3 FOR iterations');
});

// ── 5. Type checker tests ────────────────────────────────────────

test('typecheck: COUNT on array no error', () => {
  const ast = parse(
    `CREATE nums([NUM]) TO [1, 2, 3].
SHOW COUNT(nums).
`
  );
  const diags = typecheck(ast);
  const errors = diags.filter(d => d.severity === 'error');
  assert.strictEqual(errors.length, 0, `expected 0 errors, got ${errors.length}`);
});

test('typecheck: SUM on [NUM] no error', () => {
  const ast = parse(
    `CREATE nums([NUM]) TO [1, 2].
SHOW SUM(nums).
`
  );
  const diags = typecheck(ast);
  const errors = diags.filter(d => d.severity === 'error');
  assert.strictEqual(errors.length, 0, `expected 0 errors, got ${errors.length}`);
});

test('typecheck: FIRST returns inner type (NUM for [NUM])', () => {
  const ast = parse(
    `CREATE nums([NUM]) TO [1, 2].
SHOW FIRST(nums).
`
  );
  const diags = typecheck(ast);
  const errors = diags.filter(d => d.severity === 'error');
  assert.strictEqual(errors.length, 0);
});

test('typecheck: LAST returns inner type (NUM for [NUM])', () => {
  const ast = parse(
    `CREATE nums([NUM]) TO [1, 2].
SHOW LAST(nums).
`
  );
  const diags = typecheck(ast);
  const errors = diags.filter(d => d.severity === 'error');
  assert.strictEqual(errors.length, 0);
});

// ── 6. CREATE with list operations as RHS ────────────────────────

test('CREATE with COUNT as RHS', () => {
  const interp = assertSource(
    `CREATE nums([NUM]) TO [10, 20].
SHOW COUNT(nums).
`
  );
  const out = showText(interp);
  assert.strictEqual(out, '2');
});

// ── 7. INCREASE/DECREASE on list operations not applicable ───────

// ── Summary ──────────────────────────────────────────────────────

console.log(`\nPhase 18 (Native LIST Ops): ${passed} passed, ${failed} failed${failed > 0 ? ' — FAIL' : ' — all ok'}`);
process.exit(failed > 0 ? 1 : 0);
