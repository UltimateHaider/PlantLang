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

// ── 1. SPECIES declaration with { } body syntax ──────────────────

test('SPECIES with { } body parses fields', () => {
  const ast = parse(
    `SPECIES Animal {
  name: TX
}
`
  );
  const decl = ast.statements[0];
  assert.strictEqual(decl.type, 'SpeciesDeclaration');
  assert.strictEqual(decl.name, 'Animal');
  assert.strictEqual(decl.fields.length, 1);
  assert.strictEqual(decl.fields[0].name, 'name');
  assert.strictEqual(decl.fields[0].varType, 'TX');
});

test('SPECIES with action in { } body', () => {
  const ast = parse(
    `SPECIES Greeter {
  msg: TX
  ACTION greet() {
    GIVE SELF:msg + " world!".
  }
}
`
  );
  const decl = ast.statements[0];
  assert.strictEqual(decl.type, 'SpeciesDeclaration');
  assert.strictEqual(decl.name, 'Greeter');
  const act = decl.actions[0];
  assert.strictEqual(act.name, 'greet');
  assert.ok(act.bodyStatements);
});

// ── 2. BLOOM expression in CREATE ────────────────────────────────

test('BLOOM expression in CREATE produces INSTANCE', () => {
  const interp = assertSource(
    `SPECIES Greeter {
  msg: TX
}
CREATE g TO BLOOM Greeter.
`
  );
  const g = interp.soil.get('g');
  assert.ok(g, 'g exists in soil');
  assert.strictEqual(g.type, 'INSTANCE');
});

test('BLOOM + field read via : works', () => {
  const interp = assertSource(
    `SPECIES Greeter {
  msg: TX
}
CREATE g TO BLOOM Greeter.
SET g:msg TO "Hello".
`
  );
  const g = interp.soil.get('g');
  assert.ok(g);
  assert.strictEqual(g.value.msg, 'Hello');
});

// ── 3. REAP from species method ──────────────────────────────────

test('REAP FROM obj:method works', () => {
  const interp = assertSource(
    `SPECIES Greeter {
  msg: TX
  ACTION greet() {
    GIVE SELF:msg + " world!".
  }
}
CREATE g TO BLOOM Greeter.
SET g:msg TO "Hello".
1\\ REAP result FROM g:greet.
1\\ SHOW result.
`
  );
  const out = showText(interp);
  assert.ok(out.includes('Hello world!'), `output contains "Hello world!" — got "${out}"`);
});

// ── 4. SPECIES with FROM inheritance ─────────────────────────────

test('SPECIES FROM parses parentName', () => {
  const ast = parse(
    `SPECIES Animal {
  name: TX
}
SPECIES Dog FROM Animal {
  breed: TX
}
`
  );
  const dog = ast.statements[1];
  assert.strictEqual(dog.parentName, 'Animal');
  assert.strictEqual(dog.fields.length, 1);
  assert.strictEqual(dog.fields[0].name, 'breed');
});

test('SPECIES FROM at runtime inherits parent fields', () => {
  const interp = assertSource(
    `SPECIES Animal {
  name: TX
}
SPECIES Dog FROM Animal {
  breed: TX
}
CREATE d TO BLOOM Dog.
SET d:name TO "Rex".
SET d:breed TO "Husky".
SHOW d:name.
SHOW d:breed.
`
  );
  const out = showText(interp);
  assert.ok(out.includes('Rex'), `output has Rex — got "${out}"`);
  assert.ok(out.includes('Husky'), `output has Husky — got "${out}"`);
});

// ── 5. Type checker: SPECIES with { } body ───────────────────────

test('typecheck: SPECIES { } body no errors', () => {
  const ast = parse(
    `SPECIES Greeter {
  msg: TX
  ACTION greet() {
    GIVE SELF:msg + " world!".
  }
}
`
  );
  const diags = typecheck(ast);
  const errors = diags.filter(d => d.severity === 'error');
  assert.strictEqual(errors.length, 0, `expected 0 errors, got ${errors.length}: ${JSON.stringify(errors)}`);
});

// ── 6. SET SELF:field in action body (species method mutation) ───

test('SET SELF:field in species action body', () => {
  const interp = assertSource(
    `SPECIES Counter {
  count: NUM
  ACTION increment() {
    SET SELF:count TO SELF:count + 1.
  }
  ACTION getCount() {
    GIVE SELF:count.
  }
}
CREATE c TO BLOOM Counter.
1\\ REAP _ FROM c:increment.
1\\ REAP _ FROM c:increment.
1\\ REAP result FROM c:getCount.
1\\ SHOW result.
`
  );
  const out = showText(interp);
  assert.ok(out.includes('2'), `count should be 2 — got "${out}"`);
});

// ── 7. Parent method call via SELF: ──────────────────────────────

test('parent method accessible on child instance', () => {
  const interp = assertSource(
    `SPECIES Animal {
  name: TX
  ACTION speak() {
    GIVE SELF:name + " says hello!".
  }
}
SPECIES Dog FROM Animal {
}
CREATE d TO BLOOM Dog.
SET d:name TO "Rex".
1\\ REAP result FROM d:speak.
1\\ SHOW result.
`
  );
  const out = showText(interp);
  assert.ok(out.includes('Rex says hello!'), `got "${out}"`);
});

// ── Summary ──────────────────────────────────────────────────────

console.log(`\nPhase 17 (Species: OOP): ${passed} passed, ${failed} failed${failed > 0 ? ' — FAIL' : ' — all ok'}`);
process.exit(failed > 0 ? 1 : 0);
