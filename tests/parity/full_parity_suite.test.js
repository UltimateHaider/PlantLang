#!/usr/bin/env node
'use strict';

const { parse } = require('../../core/parser');
const { LLVMEmitter } = require('../../src/codegen/llvm/llvm_emitter');
const { CompilerPipeline } = require('../../src/driver/pipeline');
const { execFileSync, execSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

const TMP = '/tmp/plant_parity';
try { fs.mkdirSync(TMP, { recursive: true }); } catch (_) {}

let pass = 0, fail = 0, skip = 0;
let testIndex = 0;

// ── Run source through the interpreter and capture SHOW stdout ───
function runInterpreter(src) {
    const { Interpreter } = require('../../core/interpreter.js');
    const output = [];
    const emit = (line, type) => {
        if (type === 'SHOW' || type === 'inf') output.push(String(line));
    };
    const interp = new Interpreter({ emit });
    interp.runSource(src);
    return output.join('\n');
}

// ── Compile and run through the native pipeline ─────────────────
function compileAndRunNative(src, optLevel) {
    const fname = `${TMP}/parity_${testIndex++}`;
    fs.writeFileSync(fname + '.plant', src, 'utf8');
    const pipeline = new CompilerPipeline({
        inputFile: fname + '.plant',
        outputFile: fname + '_bin',
        optLevel: optLevel || 'O2',
    });
    try {
        const binaryPath = pipeline.compile();
        const result = execFileSync(binaryPath, [], { encoding: 'utf8' });
        return result.trim();
    } finally {
        try { fs.unlinkSync(fname + '.plant'); } catch (_) {}
        try { fs.unlinkSync(fname + '_bin'); } catch (_) {}
    }
}

function test(name, src) {
    try {
        const expected = runInterpreter(src);
        const gotO0 = compileAndRunNative(src, 'O0');
        const gotO2 = compileAndRunNative(src, 'O2');

        let ok = true;
        let detail = '';

        if (gotO0 !== expected) {
            ok = false;
            detail += ` -O0 mismatch (got ${JSON.stringify(gotO0)}, expected ${JSON.stringify(expected)})`;
        }
        if (gotO2 !== expected) {
            ok = false;
            detail += ` -O2 mismatch (got ${JSON.stringify(gotO2)}, expected ${JSON.stringify(expected)})`;
        }
        if (gotO0 !== gotO2) {
            ok = false;
            detail += ` -O0≠-O2 mismatch (${JSON.stringify(gotO0)} vs ${JSON.stringify(gotO2)})`;
        }

        if (ok) {
            console.log(`  ${name} ... \x1b[32m✓\x1b[0m`);
            pass++;
        } else {
            console.log(`  ${name} ... \x1b[31m✗\x1b[0m${detail}`);
            fail++;
        }
    } catch (e) {
        console.log(`  ${name} ... \x1b[31m✗\x1b[0m (${e.message.split('\n')[0]})`);
        fail++;
    }
}

console.log('\n\x1b[1mPhase 5 — Full Parity Suite (Interpreter vs Native Binary)\x1b[0m\n');

// Some comparison expressions are not supported by the interpreter's
// evalExpr() path (they work via evalCond only in IF conditions).
// Parity tests are restricted to features both interpreter and compiler handle.

// ── Phase 1 parity: Primitives ──────────────────────────
test('SHOW integer literal',    '1\\ SHOW 42.');
test('SHOW decimal literal',    '1\\ SHOW 3.14.');
test('SHOW boolean true',       '1\\ SHOW TRUE.');
test('SHOW boolean false',      '1\\ SHOW FALSE.');
test('SHOW string literal',     '1\\ SHOW "hello".');
test('SHOW NUM var',            '1\\ CREATE x(NUM) TO 7.\n1\\ SHOW x.');
test('SHOW TX var',             '1\\ CREATE s(TX) TO "hi".\n1\\ SHOW s.');
test('SHOW arithmetic',         '1\\ SHOW 1 + 2 * 3.');
test('SHOW parenthesized expr', '1\\ SHOW (1 + 2) * 3.');
test('SHOW multiple vars',      '1\\ CREATE a(NUM) TO 10.\n1\\ CREATE b(NUM) TO 20.\n1\\ SHOW a + b.');

// ── Phase 2 parity: Control Flow ────────────────────────
test('IF true',                 '1\\ IF 1 IS 1, SHOW 1.\n1\\ ELSE, SHOW 2.\n');
test('IF false',                '1\\ IF 0 IS 1, SHOW 1.\n1\\ ELSE, SHOW 2.\n');
test('CYCLE 1 to 3',            '1\\ CYCLE i FROM 1 TO 3,\n2\\ SHOW i.\n');
test('CYCLE step 2',            '1\\ CYCLE i FROM 1 TO 5 STEP 2,\n2\\ SHOW i.\n');
test('CYCLE single iteration',  '1\\ CYCLE i FROM 3 TO 3,\n2\\ SHOW i.\n');
test('IF in CYCLE',             '1\\ CYCLE i FROM 1 TO 3,\n2\\ IF i IS 2, SHOW i.\n2\\ ELSE, SHOW 0.\n');
test('ORIF first true',         '1\\ IF 1 IS 1, SHOW 1.\n1\\ ORIF 2 IS 2, SHOW 2.\n1\\ ELSE, SHOW 3.\n');
test('ORIF second true',        '1\\ IF 0 IS 1, SHOW 1.\n1\\ ORIF 2 IS 2, SHOW 2.\n1\\ ELSE, SHOW 3.\n');

// ── Phase 3 parity: Functions ───────────────────────────
test('ACTION add',              '1\\ ACTION add(a(NUM), b(NUM)),\n2\\ GIVE a + b.\n1\\ /ACTION.\n1\\ REAP x FROM add, 3, 4.\n1\\ SHOW x.\n');
test('ACTION procedure',        '1\\ ACTION proc(x(NUM)),\n2\\ SHOW x.\n1\\ /ACTION.\n1\\ REAP _ FROM proc, 42.\n');
test('ACTION recursion fac',    '1\\ ACTION fac(n(NUM)),\n2\\ IF n IS 0,\n3\\ GIVE 1.\n2\\ REAP tmp FROM fac, n - 1.\n2\\ GIVE n * tmp.\n1\\ /ACTION.\n1\\ REAP x FROM fac, 5.\n1\\ SHOW x.\n');
test('ACTION recursion fib',    '1\\ ACTION fib(n(NUM)),\n2\\ IF n IS 0, GIVE 0.\n2\\ IF n IS 1, GIVE 1.\n2\\ REAP a FROM fib, n - 1.\n2\\ REAP b FROM fib, n - 2.\n2\\ GIVE a + b.\n1\\ /ACTION.\n1\\ REAP x FROM fib, 10.\n1\\ SHOW x.\n');
test('ACTION nested calls',     '1\\ ACTION double(x(NUM)),\n2\\ GIVE x + x.\n1\\ /ACTION.\n1\\ REAP a FROM double, 5.\n1\\ REAP b FROM double, a.\n1\\ SHOW b.\n');
test('ACTION call with expr',   '1\\ ACTION add(a(NUM), b(NUM)),\n2\\ GIVE a + b.\n1\\ /ACTION.\n1\\ REAP x FROM add, 2 + 3, 4.\n1\\ SHOW x.\n');
test('no-arg function',         '1\\ ACTION five(),\n2\\ GIVE 5.\n1\\ /ACTION.\n1\\ REAP x FROM five.\n1\\ SHOW x.\n');
test('two functions',           '1\\ ACTION inc(x(NUM)),\n2\\ GIVE x + 1.\n1\\ /ACTION.\n1\\ ACTION add2(x(NUM)),\n2\\ REAP a FROM inc, x.\n2\\ REAP b FROM inc, a.\n2\\ GIVE b.\n1\\ /ACTION.\n1\\ REAP x FROM add2, 5.\n1\\ SHOW x.\n');

// ── Phase 4 parity: Strings & Arrays ────────────────────
test('string concat literals',  '1\\ SHOW "hello" + " " + "world".\n');
test('string concat var',       '1\\ CREATE s(TX) TO "Hello".\n1\\ SHOW s + ", World!".\n');
test('create array and read',   '1\\ CREATE arr(LIST) TO [10, 20, 30].\n1\\ SHOW arr[0].\n1\\ SHOW arr[1].\n1\\ SHOW arr[2].\n');
test('array index with var',    '1\\ CREATE arr(LIST) TO [1, 2, 3].\n1\\ CREATE i(NUM) TO 1.\n1\\ SHOW arr[i].\n');
test('string concat in fn',     '1\\ ACTION greet(n(TX)),\n2\\ SHOW "Hello, " + n + "!".\n1\\ /ACTION.\n1\\ REAP _ FROM greet, "World".\n');

console.log(`\n\x1b[1mResults:\x1b[0m  ${pass} passed, ${fail} failed, ${skip} skipped\n`);
process.exit(fail > 0 ? 1 : 0);
