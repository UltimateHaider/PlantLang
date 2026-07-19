'use strict';
const assert = require('assert');
const { lex } = require('../core/lexer');
const { parse } = require('../core/parser');
const { Interpreter, isMapTypeStr, mapInnerTypes } = require('../core/interpreter');
const { typecheck, TypeChecker, T } = require('../core/typechecker');

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    passed++;
  } catch (e) {
    console.error(`  \x1b[31m✗\x1b[0m ${name}: ${e.message}`);
    failed++;
  }
}

function assertSource(source) {
  const interp = new Interpreter({ mission: 'SAFE' });
  const programNode = parse(source);
  interp.symbolPass(programNode);
  interp.runProgram(programNode);
  return interp;
}

// ── Type-checker helpers ──────────────────────────────────────────────────────

test('isMapTypeStr detects MAP[...]', () => {
  assert.strictEqual(isMapTypeStr('MAP[NUM,TX]'), true);
  assert.strictEqual(isMapTypeStr('MAP[TX,Point]'), true);
  assert.strictEqual(isMapTypeStr('[NUM]'), false);
  assert.strictEqual(isMapTypeStr('NUM'), false);
  assert.strictEqual(isMapTypeStr('MAP'), false);
});

test('mapInnerTypes parses key/value types', () => {
  assert.deepStrictEqual(mapInnerTypes('MAP[NUM,TX]'), { keyType: 'NUM', valueType: 'TX' });
  assert.deepStrictEqual(mapInnerTypes('MAP[TX,Point]'), { keyType: 'TX', valueType: 'Point' });
  assert.strictEqual(mapInnerTypes('[NUM]'), null);
});

// ── CREATE MAP (empty) ────────────────────────────────────────────────────────

test('CREATE empty MAP[NUM,TX]', () => {
  const interp = assertSource(`CREATE m(MAP[NUM,TX]).\n`);
  const entry = interp.soil.get('m');
  assert.ok(entry.value instanceof Map, 'value should be a Map');
  assert.strictEqual(entry.value.size, 0);
  assert.strictEqual(entry.type, 'MAP[NUM,TX]');
});

test('CREATE empty MAP[TX,NUM]', () => {
  const interp = assertSource(`CREATE m(MAP[TX,NUM]).\n`);
  const entry = interp.soil.get('m');
  assert.ok(entry.value instanceof Map);
  assert.strictEqual(entry.type, 'MAP[TX,NUM]');
});

// ── MAP literal ───────────────────────────────────────────────────────────────

test('CREATE MAP with literal { key: value }', () => {
  const interp = assertSource(`CREATE m(MAP[NUM,TX]) TO { 1: "one", 2: "two" }.\n`);
  const entry = interp.soil.get('m');
  assert.ok(entry.value instanceof Map);
  assert.strictEqual(entry.value.get(1), 'one');
  assert.strictEqual(entry.value.get(2), 'two');
  assert.strictEqual(entry.value.size, 2);
});

test('CREATE MAP with TX key literal', () => {
  const interp = assertSource(`CREATE m(MAP[TX,NUM]) TO { "a": 10, "b": 20 }.\n`);
  const entry = interp.soil.get('m');
  assert.strictEqual(entry.value.get('a'), 10);
  assert.strictEqual(entry.value.get('b'), 20);
});

// ── put method via LINK ───────────────────────────────────────────────────────

test('MAP put adds entry via LINK', () => {
  const interp = assertSource(`
    CREATE m(MAP[NUM,TX]).
    LINK 1 WITH "hello" IN m.
  `);
  const entry = interp.soil.get('m');
  assert.strictEqual(entry.value.get(1), 'hello');
  assert.strictEqual(entry.value.size, 1);
});

test('MAP put with TX key via LINK', () => {
  const interp = assertSource(`
    CREATE m(MAP[TX,NUM]).
    LINK "key1" WITH 42 IN m.
  `);
  const entry = interp.soil.get('m');
  assert.strictEqual(entry.value.get('key1'), 42);
});

// ── has method ────────────────────────────────────────────────────────────────

test('MAP has returns true for existing key', () => {
  const interp = assertSource(`
    CREATE m(MAP[NUM,TX]).
    LINK 1 WITH "hello" IN m.
    SHOW m.has(1).
  `);
  const showOutput = interp.output.find(o => o.type === 'inf');
  assert.strictEqual(showOutput.text, 'true');
});

test('MAP has returns false for missing key', () => {
  const interp = assertSource(`
    CREATE m(MAP[NUM,TX]).
    SHOW m.has(99).
  `);
  const showOutput = interp.output.find(o => o.type === 'inf');
  assert.strictEqual(showOutput.text, 'false');
});

// ── get + MATCH ───────────────────────────────────────────────────────────────

test('MAP get + MATCH Some dispatches correctly', () => {
  const interp = assertSource(`
    CHOICE Option { Some(TX), None }.
    CREATE m(MAP[NUM,TX]) TO { 1: "one", 2: "two" }.
    CREATE opt TO m.get(2).
    MATCH opt {
      Some(v) -> { SHOW v }
      None -> { SHOW "missing" }
    }.
  `);
  const printed = interp.output.some(o => String(o.text).includes('two'));
  assert.ok(printed, 'Some branch should print value');
});

test('MAP get + MATCH None dispatches correctly', () => {
  const interp = assertSource(`
    CHOICE Option { Some(TX), None }.
    CREATE m(MAP[NUM,TX]).
    CREATE opt TO m.get(42).
    MATCH opt {
      Some(v) -> { SHOW v }
      None -> { SHOW "missing" }
    }.
  `);
  const printed = interp.output.some(o => String(o.text).includes('missing'));
  assert.ok(printed, 'None branch should print default');
});

// ── Overwrite (in-place update) ───────────────────────────────────────────────

test('MAP put overwrites existing key', () => {
  const interp = assertSource(`
    CREATE m(MAP[NUM,TX]) TO { 1: "one" }.
    LINK 1 WITH "updated" IN m.
    SHOW m.get(1).
  `);
  const showOutput = interp.output.find(o => o.type === 'inf');
  // get(1) returns Option with Some("updated")
  // MATCH would be needed to extract. For now just verify map contents.
  const entry = interp.soil.get('m');
  assert.strictEqual(entry.value.get(1), 'updated');
  assert.strictEqual(entry.value.size, 1);
});

// ── Growth → rehash (LINK many entries) ───────────────────────────────────────

test('MAP growth with many entries', () => {
  const interp = assertSource(`
    CREATE m(MAP[NUM,TX]).
    LINK 0 WITH "zero" IN m.
    LINK 1 WITH "one" IN m.
    LINK 2 WITH "two" IN m.
    LINK 3 WITH "three" IN m.
    LINK 4 WITH "four" IN m.
    LINK 5 WITH "five" IN m.
    LINK 6 WITH "six" IN m.
    LINK 7 WITH "seven" IN m.
    LINK 8 WITH "eight" IN m.
    LINK 9 WITH "nine" IN m.
    SHOW m.has(9).
    SHOW m.has(99).
  `);
  const entry = interp.soil.get('m');
  assert.strictEqual(entry.value.size, 10);
  assert.strictEqual(entry.value.get(0), 'zero');
  assert.strictEqual(entry.value.get(9), 'nine');
  const showOutputs = interp.output.filter(o => o.type === 'inf');
  assert.strictEqual(showOutputs[0].text, 'true');
  assert.strictEqual(showOutputs[1].text, 'false');
});

// ── SHOW map ──────────────────────────────────────────────────────────────────

test('SHOW displays map contents', () => {
  const interp = assertSource(`
    CREATE m(MAP[NUM,TX]) TO { 1: "one" }.
    SHOW m.
  `);
  const showOutput = interp.output.find(o => o.type === 'inf');
  assert.ok(showOutput.text.includes('1'), 'display should include key 1');
  assert.ok(showOutput.text.includes('one'), 'display should include value one');
});

// ── Type-checker tests ────────────────────────────────────────────────────────

test('typecheck: MAP creation passes', () => {
  const source = `CREATE m(MAP[NUM,TX]) TO { 1: "one" }.\n`;
  const diags = typecheck(parse(source));
  const errors = diags.filter(d => d.severity === 'error');
  assert.strictEqual(errors.length, 0, 'should have no errors');
});

test('typecheck: MAP literal key type mismatch', () => {
  const source = `CREATE m(MAP[NUM,TX]) TO { "a": "one" }.\n`;
  const diags = typecheck(parse(source));
  const errors = diags.filter(d => d.severity === 'error' && d.code === 'TYPE_MISMATCH');
  assert.ok(errors.length > 0, 'should emit TYPE_MISMATCH for string key in MAP[NUM,TX]');
});

// ── Summary ───────────────────────────────────────────────────────────────────

console.log(`\n\x1b[1mPhase 14 (MAPs): ${passed} passed, ${failed} failed\x1b[0m`);
if (failed > 0) process.exit(1);
