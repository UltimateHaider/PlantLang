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

// ── 1. Basic STRUCT declaration (STRUCT syntax) ──────────────────

test('STRUCT declaration with colon syntax', () => {
  const ast = parse('STRUCT Point { x: NUM, y: NUM }.\n');
  const structDecl = ast.statements[0];
  assert.strictEqual(structDecl.type, 'StructDeclaration');
  assert.strictEqual(structDecl.name, 'Point');
  assert.strictEqual(structDecl.fields.length, 2);
  assert.strictEqual(structDecl.fields[0].name, 'x');
  assert.strictEqual(structDecl.fields[0].varType, 'NUM');
  assert.strictEqual(structDecl.fields[1].name, 'y');
  assert.strictEqual(structDecl.fields[1].varType, 'NUM');
});

test('SHAPE declaration still works (paren syntax)', () => {
  const ast = parse('SHAPE Point { x(NUM), y(NUM) }.\n');
  const structDecl = ast.statements[0];
  assert.strictEqual(structDecl.type, 'StructDeclaration');
  assert.strictEqual(structDecl.name, 'Point');
  assert.strictEqual(structDecl.fields.length, 2);
});

// ── 2. Struct instantiation ──────────────────────────────────────

test('CREATE with struct literal (named fields)', () => {
  const interp = assertSource(
    'STRUCT Point { x: NUM, y: NUM }.\n' +
    'CREATE p(Point) TO { x: 10, y: 20 }.\n' +
    'SHOW p.x.\n' +
    'SHOW p.y.\n');
  const output = showText(interp);
  assert.strictEqual(output, '10,20');
});

test('CREATE with positional args (SHAPE style)', () => {
  const interp = assertSource(
    'SHAPE Point { x(NUM), y(NUM) }.\n' +
    'CREATE p(Point) TO Point{ 30, 40 }.\n' +
    'SHOW p.x.\n' +
    'SHOW p.y.\n');
  const output = showText(interp);
  assert.strictEqual(output, '30,40');
});

test('CREATE with TX and FACT fields', () => {
  const interp = assertSource(
    'STRUCT Person { name: TX, age: NUM, active: FACT }.\n' +
    'CREATE p(Person) TO { name: "Alice", age: 30, active: FACT:TRUE }.\n' +
    'SHOW p.name.\n' +
    'SHOW p.age.\n' +
    'SHOW p.active.\n');
  const output = showText(interp);
  assert.strictEqual(output, 'Alice,30,true');
});

// ── 3. Field mutation with SET ───────────────────────────────────

test('SET struct field', () => {
  const interp = assertSource(
    'STRUCT Point { x: NUM, y: NUM }.\n' +
    'CREATE p(Point) TO { x: 10, y: 20 }.\n' +
    'SET p.x TO 15.\n' +
    'SHOW p.x.\n');
  const output = showText(interp);
  assert.strictEqual(output, '15');
});

// ── 4. Empty struct instantiation (all defaults) ─────────────────

test('CREATE struct with no value expression (defaults)', () => {
  const interp = assertSource(
    'STRUCT Point { x: NUM, y: NUM }.\n' +
    'CREATE p(Point).\n' +
    'SHOW p.x.\n' +
    'SHOW p.y.\n');
  const output = showText(interp);
  assert.strictEqual(output, '0,0');
});

// ── 5. Struct member access in expressions ───────────────────────

test('struct field in arithmetic expression', () => {
  const interp = assertSource(
    'STRUCT Point { x: NUM, y: NUM }.\n' +
    'CREATE a(Point) TO { x: 5, y: 10 }.\n' +
    'CREATE b(Point) TO { x: 3, y: 4 }.\n' +
    'SHOW a.x + b.y.\n');
  const output = showText(interp);
  assert.strictEqual(output, '9');
});

// ── 6. Nested structs ────────────────────────────────────────────

test('nested struct (struct field referencing another struct)', () => {
  const interp = assertSource(
    'STRUCT Point { x: NUM, y: NUM }.\n' +
    'STRUCT Rect { p1: Point, p2: Point }.\n' +
    'CREATE p1(Point) TO { x: 1, y: 2 }.\n' +
    'CREATE p2(Point) TO { x: 3, y: 4 }.\n' +
    'CREATE r(Rect) TO Rect{ p1, p2 }.\n');
  // Just parse and verify no crash
  assert.ok(true);
});

// ── 7. INCREASE/DECREASE on NUM struct fields ────────────────────

test('INCREASE and DECREASE on struct NUM fields', () => {
  const interp = assertSource(
    'STRUCT Counter { val: NUM }.\n' +
    'CREATE c(Counter) TO { val: 10 }.\n' +
    'INCREASE c.val BY 5.\n' +
    'SHOW c.val.\n' +
    'DECREASE c.val BY 3.\n' +
    'SHOW c.val.\n');
  const output = showText(interp);
  assert.strictEqual(output, '15,12');
});

// ── 8. FOR ... IN with struct array ──────────────────────────────

test('FOR ... IN with array of structs', () => {
  const interp = assertSource(
    'STRUCT Point { x: NUM, y: NUM }.\n' +
    'CREATE pts([Point]) TO [Point{ 1, 2 }, Point{ 3, 4 }, Point{ 5, 6 }].\n' +
    'FOR pt IN pts,\n' +
    '2\\  SHOW pt.x.\n' +
    '/FOR.\n');
  const output = showText(interp);
  assert.strictEqual(output, '1,3,5');
});

// ── 9. Partial struct literal (missing fields get defaults) ──────

test('partial struct literal fills missing fields with defaults', () => {
  const interp = assertSource(
    'STRUCT Config { host: TX, port: NUM, debug: FACT }.\n' +
    'CREATE c(Config) TO { host: "localhost", port: 8080 }.\n' +
    'SHOW c.host.\n' +
    'SHOW c.port.\n' +
    'SHOW c.debug.\n');
  const output = showText(interp);
  assert.strictEqual(output, 'localhost,8080,false');
});

// ── 10. Error cases (parse errors) ───────────────────────────────

test('missing field name in struct literal throws', () => {
  assert.throws(() => {
    parse('STRUCT Point { x: NUM, y: NUM }.\nCREATE p(Point) TO { : 10 }.\n');
  }, /SYNTAX_STORM/);
});

test('undefined field in struct literal throws', () => {
  // This is a runtime type error, not parse error
  assert.throws(() => {
    const interp = assertSource(
      'STRUCT Point { x: NUM, y: NUM }.\n' +
      'CREATE p(Point) TO { x: 10, z: 99 }.\n');
  }, /TYPE_STORM|MISSING_STORM/);
});

test('access non-existent struct field throws', () => {
  assert.throws(() => {
    const interp = assertSource(
      'STRUCT Point { x: NUM, y: NUM }.\n' +
      'CREATE p(Point) TO { x: 10, y: 20 }.\n' +
      'SHOW p.z.\n');
  }, /MISSING_STORM/);
});

// ── 11. STRUCT vs SHAPE keyword equivalence ──────────────────────

test('STRUCT and SHAPE produce identical AST', () => {
  const ast1 = parse('STRUCT A { x: NUM }.\n');
  const ast2 = parse('SHAPE A { x(NUM) }.\n');
  assert.strictEqual(ast1.statements[0].type, 'StructDeclaration');
  assert.strictEqual(ast2.statements[0].type, 'StructDeclaration');
  assert.strictEqual(ast1.statements[0].fields[0].name, 'x');
  assert.strictEqual(ast2.statements[0].fields[0].name, 'x');
  assert.strictEqual(ast1.statements[0].fields[0].varType, 'NUM');
  assert.strictEqual(ast2.statements[0].fields[0].varType, 'NUM');
});

// ── Summary ──────────────────────────────────────────────────────

console.log(`\nPhase 16 (Structs): ${passed} passed, ${failed} failed${failed ? ' — SOME TESTS FAILED' : ' — all ok'}`);
if (failed > 0) process.exit(1);
