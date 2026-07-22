'use strict';
/**
 * tests/test_llvm_codegen.js — smoke tests for core/llvm_codegen.js
 *
 * For each fixture:
 *   1. Run interpreted (capture SHOW output only, stripped of ANSI/CREATE noise)
 *   2. Generate LLVM IR, lower with llc, link with gcc, run the binary
 *   3. Assert stdout matches exactly
 *
 * Requires `llc` (any version — llc, llc-18, llc-17, ...) to be on PATH.
 * If no llc is found, tests are skipped with a clear message rather than
 * failing (this generator is an alternative backend, not a hard dependency
 * for the rest of the project).
 *
 * Run directly:  node tests/test_llvm_codegen.js
 */

const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const { Interpreter } = require('../core/interpreter');
const { parse } = require('../core/parser');
const { generate } = require('../core/llvm_codegen');

let passed = 0, failed = 0, skipped = 0;

function findLLC() {
  for (const bin of ['llc', 'llc-18', 'llc-17', 'llc-16', 'llc-15', 'llc-14']) {
    try { execFileSync(bin, ['--version'], { stdio: 'pipe' }); return bin; } catch (_) {}
  }
  return null;
}

const LLC_BIN = findLLC();

function stripAnsi(s) {
  return s.replace(/\x1b\[[0-9;]*m/g, '');
}

function runInterpreted(source) {
  const lines = [];
  const interp = new Interpreter({
    mission: 'SAFE',
    emit: (text, type) => {
      if (type === 'inf' || type === undefined) lines.push(stripAnsi(text));
    }
  });
  interp.runSource(source);
  return lines.join('\n').trim();
}

function runCompiledLLVM(source, tmpBase) {
  const prog = parse(source);
  const { ir, errors } = generate(prog);
  if (errors.length) throw new Error('codegen errors: ' + errors.map(e => e.message).join('; '));

  const llPath  = tmpBase + '.ll';
  const sPath   = tmpBase + '.s';
  const binPath = tmpBase;

  fs.writeFileSync(llPath, ir, 'utf8');
  execFileSync(LLC_BIN, [llPath, '-O2', '-o', sPath], { stdio: 'pipe' });
  execFileSync('gcc', [sPath, '-no-pie', '-lm', '-o', binPath], { stdio: 'pipe' });
  const out = execFileSync(binPath, [], { encoding: 'utf8' });

  fs.unlinkSync(llPath);
  fs.unlinkSync(sPath);
  fs.unlinkSync(binPath);
  return out.trim();
}

function test(name, source) {
  if (!LLC_BIN) {
    console.log(`  ${name} ... \x1b[33m⊘ skipped (no llc found)\x1b[0m`);
    skipped++;
    return;
  }
  process.stdout.write(`  ${name} ... `);
  try {
    const interpOut = runInterpreted(source);
    const tmpBase = path.join('/tmp', 'plnt_llvm_test_' + Date.now() + '_' + Math.random().toString(36).slice(2));
    const compiledOut = runCompiledLLVM(source, tmpBase);
    if (interpOut === compiledOut) {
      console.log('\x1b[32m✓\x1b[0m');
      passed++;
    } else {
      console.log('\x1b[31m✗\x1b[0m');
      console.log('    interpreted:', JSON.stringify(interpOut));
      console.log('    compiled:   ', JSON.stringify(compiledOut));
      failed++;
    }
  } catch (e) {
    console.log('\x1b[31m✗ (error)\x1b[0m');
    console.log('   ', e.message.split('\n')[0]);
    failed++;
  }
}

function testErrors(name, source, expectedSubstring) {
  process.stdout.write(`  ${name} ... `);
  try {
    const { errors } = generate(parse(source));
    if (errors.length === 0) {
      console.log('\x1b[31m✗ (expected an error, got none)\x1b[0m');
      failed++;
      return;
    }
    const found = errors.some(e => e.message.includes(expectedSubstring));
    if (found) {
      console.log('\x1b[32m✓\x1b[0m');
      passed++;
    } else {
      console.log('\x1b[31m✗\x1b[0m');
      console.log('    expected an error containing:', JSON.stringify(expectedSubstring));
      console.log('    got:', errors.map(e => e.message));
      failed++;
    }
  } catch (e) {
    console.log('\x1b[31m✗ (threw)\x1b[0m');
    console.log('   ', e.message);
    failed++;
  }
}

console.log('PlantLang LLVM IR Code Generator — smoke tests');
console.log(LLC_BIN ? `  (using ${LLC_BIN})\n` : '  (llc not found — execution tests will be skipped)\n');

// ── Basic values and arithmetic ──────────────────────────────────────────────
test('basic CREATE/SHOW', `
MISSION: SAFE.
1\\ CREATE x(NUM) TO 42.
1\\ SHOW x.
`);

test('arithmetic: all operators', `
MISSION: SAFE.
1\\ CREATE a(NUM) TO 17.
1\\ CREATE b(NUM) TO 5.
1\\ SHOW a + b.
1\\ SHOW a - b.
1\\ SHOW a * b.
1\\ SHOW a % b.
1\\ SHOW a ** 2.
`);

test('SCL (double) arithmetic', `
MISSION: SAFE.
1\\ CREATE pi(SCL) TO 3.14159.
1\\ CREATE r(SCL) TO 2.0.
1\\ SHOW pi * r * r.
1\\ SHOW pi / r.
`);

test('NUM/SCL mixed promotion', `
MISSION: SAFE.
1\\ CREATE n(NUM) TO 10.
1\\ CREATE s(SCL) TO 2.5.
1\\ SHOW n + s.
1\\ SHOW n * s.
`);

test('division always produces SCL', `
MISSION: SAFE.
1\\ CREATE a(NUM) TO 10.
1\\ CREATE b(NUM) TO 4.
1\\ SHOW a / b.
`);

test('FACT (boolean) values', `
MISSION: SAFE.
1\\ CREATE active(FACT) TO TRUE.
1\\ CREATE inactive(FACT) TO FALSE.
1\\ SHOW active.
1\\ SHOW inactive.
`);

test('negative numbers and unary minus', `
MISSION: SAFE.
1\\ CREATE x(NUM) TO 10.
1\\ SHOW -x.
1\\ SHOW -5 + 3.
`);

// ── Strings ───────────────────────────────────────────────────────────────────
test('string literal SHOW', `
MISSION: SAFE.
1\\ SHOW "Hello, World!".
`);

test('string concatenation: two strings', `
MISSION: SAFE.
1\\ CREATE greeting(TX) TO "Hello".
1\\ CREATE name(TX) TO "World".
1\\ SHOW greeting + ", " + name + "!".
`);

test('string concatenation: string + number', `
MISSION: SAFE.
1\\ CREATE score(NUM) TO 95.
1\\ SHOW "Score: " + score.
`);

test('string concatenation: string + SCL + FACT', `
MISSION: SAFE.
1\\ CREATE pi(SCL) TO 3.14.
1\\ CREATE ok(FACT) TO TRUE.
1\\ SHOW "pi=" + pi + " ok=" + ok.
`);

// ── Comparisons ───────────────────────────────────────────────────────────────
test('comparison operators', `
MISSION: SAFE.
1\\ CREATE a(NUM) TO 10.
1\\ CREATE b(NUM) TO 20.
1\\ IF a GREATER THAN b,
2\\   SHOW "a>b".
1\\ ELSE,
2\\   SHOW "a<=b".
1\\.
1\\ IF a LESS THAN b,
2\\   SHOW "a<b".
1\\.
1\\ IF a IS 10,
2\\   SHOW "a is 10".
1\\.
`);

test('BETWEEN condition', `
MISSION: SAFE.
1\\ CREATE score(NUM) TO 75.
1\\ IF score BETWEEN 70 79,
2\\   SHOW "C grade".
1\\ ELSE,
2\\   SHOW "not C".
1\\.
`);

test('string equality comparison', `
MISSION: SAFE.
1\\ CREATE name(TX) TO "Ahmed".
1\\ IF name IS "Ahmed",
2\\   SHOW "matched".
1\\ ELSE,
2\\   SHOW "no match".
1\\.
`);

test('AND / OR / NOT logical operators', `
MISSION: SAFE.
1\\ CREATE a(NUM) TO 5.
1\\ CREATE b(NUM) TO 15.
1\\ IF a GREATER THAN 0 AND b GREATER THAN 10,
2\\   SHOW "both true".
1\\.
1\\ IF a GREATER THAN 100 OR b GREATER THAN 10,
2\\   SHOW "or true".
1\\.
1\\ IF NOT a GREATER THAN 100,
2\\   SHOW "not works".
1\\.
`);

// ── Control flow ──────────────────────────────────────────────────────────────
test('IF/ORIF/ELSE full chain', `
MISSION: SAFE.
1\\ CREATE score(NUM) TO 85.
1\\ IF score GREATER THAN OR EQUAL 90,
2\\   SHOW "A".
1\\ ORIF score GREATER THAN OR EQUAL 80,
2\\   SHOW "B".
1\\ ORIF score GREATER THAN OR EQUAL 70,
2\\   SHOW "C".
1\\ ELSE,
2\\   SHOW "F".
1\\.
`);

test('CYCLE FROM TO', `
MISSION: SAFE.
1\\ CYCLE i FROM 1 TO 5,
2\\   SHOW i.
1\\.
`);

test('CYCLE FROM TO STEP', `
MISSION: SAFE.
1\\ CYCLE i FROM 0 TO 20 STEP 5,
2\\   SHOW i.
1\\.
`);

test('SEASON while loop', `
MISSION: SAFE.
1\\ CREATE n(NUM) TO 5.
1\\ SEASON n GREATER THAN 0,
2\\   SHOW n.
2\\   DECREASE n BY 1.
1\\.
`);

test('nested CYCLE + IF (branch correctness across iterations)', `
MISSION: SAFE.
1\\ CYCLE i FROM 1 TO 15,
2\\   IF i GREATER THAN 10,
3\\     SHOW "big".
2\\   ORIF i GREATER THAN 5,
3\\     SHOW "medium".
2\\   ELSE,
3\\     SHOW "small".
2\\.
1\\.
`);

test('nested CYCLE inside CYCLE', `
MISSION: SAFE.
1\\ CYCLE i FROM 1 TO 3,
2\\   CYCLE j FROM 1 TO 3,
3\\     SHOW i * 10 + j.
2\\.
1\\.
`);

test('SEASON inside CYCLE', `
MISSION: SAFE.
1\\ CYCLE i FROM 1 TO 3,
2\\   CREATE countdown(NUM) TO i.
2\\   SEASON countdown GREATER THAN 0,
3\\     SHOW countdown.
3\\     DECREASE countdown BY 1.
2\\.
1\\.
`);

// ── Real-world style program ─────────────────────────────────────────────────
test('FizzBuzz-style combined program', `
MISSION: SAFE.
1\\ CREATE sum(NUM) TO 0.
1\\ CYCLE n FROM 1 TO 20,
2\\   INCREASE sum BY n.
2\\   IF n GREATER THAN OR EQUAL 15,
3\\     SHOW "late: " + n.
2\\.
1\\.
1\\ SHOW "Total: " + sum.
`);

test('variable reassignment with SET', `
MISSION: SAFE.
1\\ CREATE x(NUM) TO 1.
1\\ SHOW x.
1\\ SET x TO 100.
1\\ SHOW x.
1\\ SET x TO x + 1.
1\\ SHOW x.
`);

// ── Error handling: unsupported constructs must fail cleanly, never silently miscompile
testErrors('LIST is rejected with a clear message', `
MISSION: SAFE.
1\\ CREATE items(LIST) TO a, b, c.
`, 'CREATE with type LIST');

testErrors('undeclared variable in SHOW is rejected', `
MISSION: SAFE.
1\\ SHOW undefined_var.
`, 'was not declared');

testErrors('ACTION/REAP is rejected (not yet supported)', `
MISSION: SAFE.
1\\ ACTION add(a(NUM), b(NUM)),
2\\   GIVE a + b.
1\\ /ACTION.
`, 'Unsupported construct');

console.log(`\n${passed} passed, ${failed} failed, ${skipped} skipped`);
process.exit(failed > 0 ? 1 : 0);
