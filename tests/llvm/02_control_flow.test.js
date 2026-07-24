'use strict';

const { parse } = require('../../core/parser');
const { LLVMEmitter } = require('../../src/codegen/llvm/llvm_emitter');
const { execFileSync } = require('child_process');
const fs = require('fs');

const TMP = '/tmp/llvm_test_cf';
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

console.log('\n\x1b[1mPhase 2 — Control Flow & SSA (LLVM IR)\x1b[0m\n');

// ── IF / ELSE ───────────────────────────────────────────────
test('IF true',           '1\\ IF 1 IS 1, SHOW 1.\n1\\ ELSE, SHOW 2.\n', '1');
test('IF false',          '1\\ IF 0 IS 1, SHOW 1.\n1\\ ELSE, SHOW 2.\n', '2');
test('IF true (block)',   '1\\ IF 1 IS 1,\n2\\ SHOW 1.\n1\\ ELSE,\n2\\ SHOW 2.\n', '1');
test('IF false (block)',  '1\\ IF 0 IS 1,\n2\\ SHOW 1.\n1\\ ELSE,\n2\\ SHOW 2.\n', '2');

// ── ORIF ────────────────────────────────────────────────────
test('ORIF first true',     '1\\ IF 1 IS 1, SHOW 1.\n1\\ ORIF 2 IS 2, SHOW 2.\n1\\ ELSE, SHOW 3.\n', '1');
test('ORIF second true',    '1\\ IF 0 IS 1, SHOW 1.\n1\\ ORIF 2 IS 2, SHOW 2.\n1\\ ELSE, SHOW 3.\n', '2');
test('ORIF all false ELSE', '1\\ IF 0 IS 1, SHOW 1.\n1\\ ORIF 2 IS 3, SHOW 2.\n1\\ ELSE, SHOW 3.\n', '3');
test('ORIF no ELSE',        '1\\ IF 0 IS 1, SHOW 1.\n1\\ ORIF 2 IS 3, SHOW 2.\n', '');

// ── CYCLE ───────────────────────────────────────────────────
test('CYCLE 1 to 3',            '1\\ CYCLE i FROM 1 TO 3,\n2\\ SHOW i.\n', '1\n2\n3');
test('CYCLE step 2',            '1\\ CYCLE i FROM 1 TO 5 STEP 2,\n2\\ SHOW i.\n', '1\n3\n5');
test('CYCLE single iteration',  '1\\ CYCLE i FROM 3 TO 3,\n2\\ SHOW i.\n', '3');
test('CYCLE negative TO (none)','1\\ CYCLE i FROM 3 TO 1,\n2\\ SHOW i.\n', '');

// ── BREAK / CONTINUE ────────────────────────────────────────
test('BREAK at 3',       '1\\ CYCLE i FROM 1 TO 5,\n2\\ IF i IS 3, BREAK.\n2\\ SHOW i.\n', '1\n2');
test('CONTINUE at 3',    '1\\ CYCLE i FROM 1 TO 5,\n2\\ IF i IS 3, CONTINUE.\n2\\ SHOW i.\n', '1\n2\n4\n5');
test('BREAK immediately', '1\\ CYCLE i FROM 1 TO 5,\n2\\ BREAK.\n2\\ SHOW i.\n', '');
test('CONTINUE always',  '1\\ CYCLE i FROM 1 TO 3,\n2\\ CONTINUE.\n2\\ SHOW i.\n', '');

// ── Short-circuit AND / OR ──────────────────────────────────
test('AND true',  '1\\ IF 1 IS 1 AND 2 IS 2, SHOW 1.\n1\\ ELSE, SHOW 2.\n', '1');
test('AND false', '1\\ IF 1 IS 2 AND 2 IS 2, SHOW 1.\n1\\ ELSE, SHOW 2.\n', '2');
test('OR true',   '1\\ IF 1 IS 2 OR 2 IS 2, SHOW 1.\n1\\ ELSE, SHOW 2.\n', '1');
test('OR false',  '1\\ IF 1 IS 2 OR 2 IS 3, SHOW 1.\n1\\ ELSE, SHOW 2.\n', '2');

// ── Nested IF inside CYCLE ──────────────────────────────────
test('IF in CYCLE', '1\\ CYCLE i FROM 1 TO 3,\n2\\ IF i IS 2, SHOW i.\n2\\ ELSE, SHOW 0.\n', '0\n2\n0');

// ── Scope isolation ─────────────────────────────────────────
test('scope isolation', '1\\ IF 1 IS 1,\n2\\ CREATE x(NUM) TO 10.\n1\\ SHOW x.\n', '');

console.log(`\n\x1b[1mResults:\x1b[0m  ${pass} passed, ${fail} failed, ${skip} skipped\n`);
process.exit(fail > 0 ? 1 : 0);
