#!/usr/bin/env node
'use strict';

const { parse } = require('../../core/parser');
const { LLVMEmitter } = require('../../src/codegen/llvm/llvm_emitter');
const { execFileSync } = require('child_process');
const fs = require('fs');

const TMP = '/tmp/llvm_test_fn';
try { fs.mkdirSync(TMP, { recursive: true }); } catch (_) {}

let pass = 0, fail = 0, skip = 0;
let index = 0;

function compileAndRun(src) {
    const prog = parse(src);
    const emit = new LLVMEmitter();
    const ir = emit.generate(prog);
    const fname = `${TMP}/test_${index++}`;
    fs.writeFileSync(fname + '.ll', ir, 'utf8');
    execFileSync('llvm-as', [fname + '.ll', '-o', fname + '.bc'], { stdio: 'pipe' });
    execFileSync('llc', [fname + '.ll', '-o', fname + '.s'], { stdio: 'pipe' });
    execFileSync('gcc', [fname + '.s', 'runtime/c/plant_runtime.o', '-no-pie', '-lm', '-o', fname], { stdio: 'pipe' });
    return execFileSync(fname, [], { encoding: 'utf8' }).trim();
}

function test(name, src, expected) {
    try {
        const out = compileAndRun(src);
        if (out === expected) {
            console.log(`  ${name} ... \x1b[32m✓\x1b[0m`);
            pass++;
        } else {
            console.log(`  ${name} ... \x1b[31m✗\x1b[0m (got ${JSON.stringify(out)}, expected ${JSON.stringify(expected)})`);
            fail++;
        }
    } catch (e) {
        console.log(`  ${name} ... \x1b[31m✗\x1b[0m (${e.message.split('\n')[0]})`);
        fail++;
    }
}

console.log('\n\x1b[1mPhase 3 — Functions & Call Protocol (LLVM IR)\x1b[0m\n');

// ── Basic function returning value ─────────────────────────
test('add(3,4)',
  '1\\ ACTION add(a(NUM), b(NUM)),\n2\\ GIVE a + b.\n1\\ /ACTION.\n1\\ REAP x FROM add, 3, 4.\n1\\ SHOW x.\n',
  '7');

// ── Procedure (no GIVE) ───────────────────────────────────
test('procedure (no return)',
  '1\\ ACTION proc(x(NUM)),\n2\\ SHOW x.\n1\\ /ACTION.\n1\\ REAP _ FROM proc, 42.\n1\\ SHOW 99.\n',
  '42\n99');

// ── Multi-parameter ───────────────────────────────────────
test('mul3(2,3,4)',
  '1\\ ACTION mul3(a(NUM), b(NUM), c(NUM)),\n2\\ GIVE a * b * c.\n1\\ /ACTION.\n1\\ REAP x FROM mul3, 2, 3, 4.\n1\\ SHOW x.\n',
  '24');

// ── No-arg function ──────────────────────────────────────
test('no-arg function',
  '1\\ ACTION five(),\n2\\ GIVE 5.\n1\\ /ACTION.\n1\\ REAP x FROM five.\n1\\ SHOW x.\n',
  '5');

// ── Recursion: factorial ─────────────────────────────────
test('fac(5)',
  '1\\ ACTION fac(n(NUM)),\n2\\ IF n IS 0,\n3\\ GIVE 1.\n2\\ REAP tmp FROM fac, n - 1.\n2\\ GIVE n * tmp.\n1\\ /ACTION.\n1\\ REAP x FROM fac, 5.\n1\\ SHOW x.\n',
  '120');

// ── Recursion: fibonacci ─────────────────────────────────
test('fib(10)',
  '1\\ ACTION fib(n(NUM)),\n2\\ IF n IS 0, GIVE 0.\n2\\ IF n IS 1, GIVE 1.\n2\\ REAP a FROM fib, n - 1.\n2\\ REAP b FROM fib, n - 2.\n2\\ GIVE a + b.\n1\\ /ACTION.\n1\\ REAP x FROM fib, 10.\n1\\ SHOW x.\n',
  '55');

// ── Nested calls ──────────────────────────────────────────
test('double(double(5))',
  '1\\ ACTION double(x(NUM)),\n2\\ GIVE x + x.\n1\\ /ACTION.\n1\\ REAP a FROM double, 5.\n1\\ REAP b FROM double, a.\n1\\ SHOW b.\n',
  '20');

// ── Call with expression arguments ────────────────────────
test('call with 2+3 * 4',
  '1\\ ACTION add(a(NUM), b(NUM)),\n2\\ GIVE a + b.\n1\\ /ACTION.\n1\\ REAP x FROM add, 2 + 3, 4.\n1\\ SHOW x.\n',
  '9');

// ── Scope isolation ──────────────────────────────────────
test('local var not visible outside',
  '1\\ ACTION inner(),\n2\\ CREATE z(NUM) TO 99.\n1\\ /ACTION.\n1\\ REAP _ FROM inner.\n1\\ SHOW z.\n',
  '');

// ── Function calling another function ─────────────────────
test('two functions',
  '1\\ ACTION inc(x(NUM)),\n2\\ GIVE x + 1.\n1\\ /ACTION.\n1\\ ACTION add2(x(NUM)),\n2\\ REAP a FROM inc, x.\n2\\ REAP b FROM inc, a.\n2\\ GIVE b.\n1\\ /ACTION.\n1\\ REAP x FROM add2, 5.\n1\\ SHOW x.\n',
  '7');

// ── GIVE with no expression (return 0) ────────────────────
test('empty GIVE',
  '1\\ ACTION zero(),\n2\\ GIVE.\n1\\ /ACTION.\n1\\ REAP x FROM zero.\n1\\ SHOW x.\n',
  '0');

// ── Variable reuse in REAP ───────────────────────────────
test('reap into existing var',
  '1\\ CREATE x(NUM) TO 0.\n1\\ ACTION double(x(NUM)),\n2\\ GIVE x + x.\n1\\ /ACTION.\n1\\ REAP x FROM double, 7.\n1\\ SHOW x.\n',
  '14');

console.log(`\n\x1b[1mResults:\x1b[0m  ${pass} passed, ${fail} failed, ${skip} skipped\n`);
process.exit(fail > 0 ? 1 : 0);
