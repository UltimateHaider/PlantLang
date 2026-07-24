#!/usr/bin/env node
'use strict';

const { parse } = require('../../core/parser');
const { LLVMEmitter } = require('../../src/codegen/llvm/llvm_emitter');
const { execFileSync } = require('child_process');
const fs = require('fs');

const TMP = '/tmp/llvm_test_ml';
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

console.log('\n\x1b[1mPhase 4 — Memory Lifecycle & Heap (LLVM IR)\x1b[0m\n');

// ── String concatenation ─────────────────────────────────
test('string concat literals',
  '1\\ SHOW "hello" + " " + "world".\n',
  'hello world');

test('string concat with variable',
  '1\\ CREATE s(TX) TO "Hello".\n1\\ SHOW s + ", World!".\n',
  'Hello, World!');

test('string concat multiple parts',
  '1\\ SHOW "a" + "b" + "c" + "d".\n',
  'abcd');

// ── Array literal and indexing ───────────────────────────
test('create array and read elements',
  '1\\ CREATE arr(LIST) TO [10, 20, 30].\n1\\ SHOW arr[0].\n1\\ SHOW arr[1].\n1\\ SHOW arr[2].\n',
  '10\n20\n30');

test('array index with variable',
  '1\\ CREATE arr(LIST) TO [1, 2, 3].\n1\\ CREATE i(NUM) TO 1.\n1\\ SHOW arr[i].\n',
  '2');

// ── Array with function ─────────────────────────────────
test('array in function',
  '1\\ ACTION show_first(a(LIST)),\n2\\ SHOW a[0].\n1\\ /ACTION.\n1\\ CREATE arr(LIST) TO [100, 200].\n1\\ REAP _ FROM show_first, arr.\n',
  '100');

// ── String concat in function ───────────────────────────
test('string concat in function',
  '1\\ ACTION greet(n(TX)),\n2\\ SHOW "Hello, " + n + "!".\n1\\ /ACTION.\n1\\ REAP _ FROM greet, "World".\n',
  'Hello, World!');

test('string concat across lines in function',
  '1\\ ACTION yell(n(TX)),\n2\\ SHOW n + "!!".\n1\\ /ACTION.\n1\\ REAP _ FROM yell, "Hey".\n',
  'Hey!!');

// ── Empty array ─────────────────────────────────────────
test('empty array',
  '1\\ CREATE arr(LIST) TO [].\n1\\ SHOW 42.\n',
  '42');

console.log(`\n\x1b[1mResults:\x1b[0m  ${pass} passed, ${fail} failed, ${skip} skipped\n`);
process.exit(fail > 0 ? 1 : 0);
