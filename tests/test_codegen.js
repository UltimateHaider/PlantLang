'use strict';
/**
 * tests/test_codegen.js — smoke tests for core/codegen.js
 *
 * For each fixture:
 *   1. Run interpreted (capture SHOW output only, stripped of ANSI/CREATE noise)
 *   2. Compile to C, gcc it, run the binary
 *   3. Assert stdout matches
 *
 * Run directly:  node tests/test_codegen.js
 */

const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const { Interpreter } = require('../core/interpreter');
const { parse } = require('../core/parser');
const { generate } = require('../core/codegen');

let passed = 0, failed = 0;

function stripAnsi(s) {
  return s.replace(/\x1b\[[0-9;]*m/g, '');
}

function runInterpreted(source) {
  const lines = [];
  const interp = new Interpreter({
    mission: 'SAFE',
    emit: (text, type) => {
      // Only capture actual SHOW output ('inf'/plain emit calls use type undefined/'inf')
      if (type === 'inf' || type === undefined) lines.push(stripAnsi(text));
    }
  });
  interp.runSource(source);
  return lines.join('\n').trim();
}

function runCompiled(source, tmpBase) {
  const prog = parse(source);
  const { code, errors } = generate(prog);
  if (errors.length) throw new Error('codegen errors: ' + errors.map(e => e.message).join('; '));

  const cPath = tmpBase + '.c';
  const binPath = tmpBase;
  fs.writeFileSync(cPath, code, 'utf8');
  execFileSync('gcc', [cPath, '-O2', '-lm', '-o', binPath], { stdio: 'pipe' });
  const out = execFileSync(binPath, [], { encoding: 'utf8' });
  fs.unlinkSync(cPath);
  fs.unlinkSync(binPath);
  return out.trim();
}

function test(name, source) {
  process.stdout.write(`  ${name} ... `);
  try {
    const interpOut   = runInterpreted(source);
    const tmpBase = path.join('/tmp', 'plnt_codegen_test_' + Date.now() + '_' + Math.random().toString(36).slice(2));
    const compiledOut = runCompiled(source, tmpBase);
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

console.log('PlantLang C Code Generator — smoke tests\n');

test('basic CREATE/SHOW', `
MISSION: SAFE.
1\\ CREATE x(NUM) TO 42.
1\\ SHOW x.
`);

test('string concat', `
MISSION: SAFE.
1\\ CREATE name(TX) TO "World".
1\\ SHOW "Hello, " + name + "!".
`);

test('arithmetic', `
MISSION: SAFE.
1\\ CREATE a(NUM) TO 10.
1\\ CREATE b(NUM) TO 3.
1\\ SHOW a + b.
1\\ SHOW a * b.
`);

test('IF/ORIF/ELSE', `
MISSION: SAFE.
1\\ CREATE score(NUM) TO 85.
1\\ IF score GREATER THAN OR EQUAL 90,
2\\   SHOW "A".
1\\ ORIF score GREATER THAN OR EQUAL 80,
2\\   SHOW "B".
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
1\\ CREATE n(NUM) TO 3.
1\\ SEASON n GREATER THAN 0,
2\\   SHOW n.
2\\   DECREASE n BY 1.
1\\.
`);

test('SCL decimal type', `
MISSION: SAFE.
1\\ CREATE pi(SCL) TO 3.14.
1\\ SHOW pi.
`);

test('nested CYCLE + IF', `
MISSION: SAFE.
1\\ CYCLE i FROM 1 TO 5,
2\\   IF i GREATER THAN 2,
3\\     SHOW i.
2\\.
1\\.
`);

console.log(`\n${passed} passed, ${failed} failed`);
process.exit(failed > 0 ? 1 : 0);
