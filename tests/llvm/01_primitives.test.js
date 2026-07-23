#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const os = require('os');
const { execFileSync } = require('child_process');

const { parse } = require('../../core/parser');
const { LLVMEmitter } = require('../../src/codegen/llvm/llvm_emitter');
const { Interpreter } = require('../../core/interpreter');

let passed = 0, failed = 0, skipped = 0;

/* ── Tooling ─────────────────────────────────────────────────────── */
function findLLC() {
    for (const bin of ['llc', 'llc-18', 'llc-17', 'llc-16', 'llc-15', 'llc-14']) {
        try { execFileSync(bin, ['--version'], { stdio: 'pipe' }); return bin; } catch (_) {}
    }
    return null;
}

const LLC_BIN = findLLC();
const RUNTIME_O = path.join(__dirname, '..', '..', 'runtime', 'c', 'plant_runtime.o');

function stripAnsi(s) {
    return s.replace(/\x1b\[[0-9;]*m/g, '');
}

/* ── Interpreter runner ──────────────────────────────────────────── */
function runInterpreted(source) {
    const lines = [];
    const interp = new Interpreter({
        mission: 'SAFE',
        emit: (text, type) => {
            if (type === 'inf' || type === undefined) lines.push(stripAnsi(text));
        },
    });
    interp.runSource(source);
    return lines.join('\n').trim();
}

/* ── LLVM-compiled runner ────────────────────────────────────────── */
function runCompiled(source, tmpDir) {
    const prog = parse(source);
    const emitter = new LLVMEmitter();
    const ir = emitter.generate(prog);

    const llPath = path.join(tmpDir, 'test.ll');
    const sPath = path.join(tmpDir, 'test.s');
    const binPath = path.join(tmpDir, 'test_bin');

    fs.writeFileSync(llPath, ir, 'utf8');
    execFileSync(LLC_BIN, [llPath, '-O2', '-o', sPath], { stdio: 'pipe' });
    execFileSync('gcc', [sPath, RUNTIME_O, '-no-pie', '-lm', '-o', binPath], { stdio: 'pipe' });
    const out = execFileSync(binPath, [], { encoding: 'utf8' });
    return out.trim();
}

/* ── Differential test: compare LLVM output vs interpreter ───────── */
function test(name, source) {
    if (!LLC_BIN) {
        console.log(`  \x1b[33m⊘ ${name} (skipped — no llc)\x1b[0m`);
        skipped++;
        return;
    }
    process.stdout.write(`  ${name} ... `);
    let tmpDir;
    try {
        tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'plnt-llvm-'));
        const expected = runInterpreted(source);
        const actual = runCompiled(source, tmpDir);
        if (expected === actual) {
            console.log('\x1b[32m✓\x1b[0m');
            passed++;
        } else {
            console.log(`\x1b[31m✗\x1b[0m`);
            console.log(`      expected: ${JSON.stringify(expected)}`);
            console.log(`      actual:   ${JSON.stringify(actual)}`);
            console.log(`      source: ${JSON.stringify(source)}`);
            failed++;
        }
    } catch (err) {
        console.log(`\x1b[31m✗\x1b[0m`);
        console.log(`      error: ${err.message}`);
        failed++;
    } finally {
        if (tmpDir) {
            try {
                for (const f of ['test.ll', 'test.s', 'test_bin']) {
                    const p = path.join(tmpDir, f);
                    if (fs.existsSync(p)) fs.unlinkSync(p);
                }
                fs.rmdirSync(tmpDir);
            } catch (_) {}
        }
    }
}

/* ── Raw test: compare LLVM output against an explicit expected value ── */
function testRaw(name, source, expected) {
    if (!LLC_BIN) {
        console.log(`  \x1b[33m⊘ ${name} (skipped — no llc)\x1b[0m`);
        skipped++;
        return;
    }
    process.stdout.write(`  ${name} ... `);
    let tmpDir;
    try {
        tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'plnt-llvm-'));
        const actual = runCompiled(source, tmpDir);
        if (expected === actual) {
            console.log('\x1b[32m✓\x1b[0m');
            passed++;
        } else {
            console.log(`\x1b[31m✗\x1b[0m`);
            console.log(`      expected: ${JSON.stringify(expected)}`);
            console.log(`      actual:   ${JSON.stringify(actual)}`);
            failed++;
        }
    } catch (err) {
        console.log(`\x1b[31m✗\x1b[0m`);
        console.log(`      error: ${err.message}`);
        failed++;
    } finally {
        if (tmpDir) {
            try {
                for (const f of ['test.ll', 'test.s', 'test_bin']) {
                    const p = path.join(tmpDir, f);
                    if (fs.existsSync(p)) fs.unlinkSync(p);
                }
                fs.rmdirSync(tmpDir);
            } catch (_) {}
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════ */
console.log('\n\x1b[1mPhase 1 — Primitives & Early SHOW (LLVM IR)\x1b[0m\n');

// ── 1. Literal nodes ──────────────────────────────────────────────
test('SHOW integer literal',          '1\\ SHOW 42.\n');
test('SHOW decimal literal',          '1\\ SHOW 3.14.\n');
test('SHOW boolean true',             '1\\ SHOW FACT:TRUE.\n');
test('SHOW boolean false',            '1\\ SHOW FACT:FALSE.\n');
test('SHOW string literal',           '1\\ SHOW "hello PlantLang".\n');

// ── 2. Variables ──────────────────────────────────────────────────
test('CREATE NUM + SHOW',             '1\\ CREATE x(NUM) TO 42.\n1\\ SHOW x.\n');
test('CREATE SCL + SHOW',             '1\\ CREATE y(SCL) TO 3.14.\n1\\ SHOW y.\n');
test('CREATE FACT + SHOW',            '1\\ CREATE b(FACT) TO FACT:TRUE.\n1\\ SHOW b.\n');
test('CREATE TX + SHOW',              '1\\ CREATE s(TX) TO "world".\n1\\ SHOW s.\n');

// ── 3. SET ────────────────────────────────────────────────────────
test('CREATE + SET + SHOW (NUM)',     '1\\ CREATE x(NUM) TO 10.\n1\\ SET x TO 20.\n1\\ SHOW x.\n');

// ── 4. Arithmetic expressions ─────────────────────────────────────
test('SHOW 1 + 2',                    '1\\ SHOW 1 + 2.\n');
test('SHOW 10 - 3',                   '1\\ SHOW 10 - 3.\n');
test('SHOW 5 * 6',                    '1\\ SHOW 5 * 6.\n');
test('SHOW 20 / 4',                   '1\\ SHOW 20 / 4.\n');
test('SHOW 15 % 4',                   '1\\ SHOW 15 % 4.\n');
test('SHOW 1 + 2 * 3',               '1\\ SHOW 1 + 2 * 3.\n');
test('SHOW (1 + 2) * 3',             '1\\ SHOW (1 + 2) * 3.\n');
test('SHOW 1 + 2.5',                  '1\\ SHOW 1 + 2.5.\n');
test('CREATE + expr with var',        '1\\ CREATE x(NUM) TO 5.\n1\\ SHOW x + 3.\n');
test('SHOW (1 + 2) * (5 - 3)',       '1\\ SHOW (1 + 2) * (5 - 3).\n');
test('SHOW 10 / 2 + 3 * 4',          '1\\ SHOW 10 / 2 + 3 * 4.\n');

// ── 5. Comparison expressions ──────────────────────────────────────
// NOTE: the interpreter falls back to the legacy regex path for
// RAW_EXPR compound expressions, so it outputs the raw text instead
// of evaluating them.  Our LLVM compiler evaluates correctly, so we
// test with known expected values.
testRaw('SHOW 3 IS 3',                    '1\\ SHOW 3 IS 3.\n',                    'true');
testRaw('SHOW 3 IS 4',                    '1\\ SHOW 3 IS 4.\n',                    'false');
testRaw('SHOW 3 IS NOT 4',               '1\\ SHOW 3 IS NOT 4.\n',                 'true');
testRaw('SHOW 5 GREATER THAN 3',          '1\\ SHOW 5 GREATER THAN 3.\n',          'true');
testRaw('SHOW 3 GREATER THAN 5',          '1\\ SHOW 3 GREATER THAN 5.\n',          'false');
testRaw('SHOW 3 LESS THAN 5',             '1\\ SHOW 3 LESS THAN 5.\n',             'true');
testRaw('SHOW 5 LESS THAN 3',             '1\\ SHOW 5 LESS THAN 3.\n',             'false');
testRaw('SHOW 5 GREATER THAN OR EQUAL 5', '1\\ SHOW 5 GREATER THAN OR EQUAL 5.\n', 'true');
testRaw('SHOW 4 GREATER THAN OR EQUAL 5', '1\\ SHOW 4 GREATER THAN OR EQUAL 5.\n', 'false');
testRaw('SHOW 3 LESS THAN OR EQUAL 3',    '1\\ SHOW 3 LESS THAN OR EQUAL 3.\n',    'true');
testRaw('SHOW 4 LESS THAN OR EQUAL 3',    '1\\ SHOW 4 LESS THAN OR EQUAL 3.\n',    'false');

// ── 6. Logical expressions ─────────────────────────────────────────
testRaw('SHOW FACT:TRUE AND FACT:TRUE',     '1\\ SHOW FACT:TRUE AND FACT:TRUE.\n',     'true');
testRaw('SHOW FACT:TRUE AND FACT:FALSE',    '1\\ SHOW FACT:TRUE AND FACT:FALSE.\n',    'false');
testRaw('SHOW FACT:FALSE OR FACT:FALSE',    '1\\ SHOW FACT:FALSE OR FACT:FALSE.\n',    'false');
testRaw('SHOW FACT:FALSE OR FACT:TRUE',     '1\\ SHOW FACT:FALSE OR FACT:TRUE.\n',     'true');
testRaw('SHOW NOT FACT:TRUE',               '1\\ SHOW NOT FACT:TRUE.\n',               'false');
testRaw('SHOW NOT FACT:FALSE',              '1\\ SHOW NOT FACT:FALSE.\n',              'true');

// ── 7. Multiple SHOWs ──────────────────────────────────────────────
test('multiple SHOWs', '1\\ SHOW 1.\n1\\ SHOW 2.\n1\\ SHOW 3.\n');

/* ═══════════════════════════════════════════════════════════════════ */
console.log(`\n\x1b[1mResults:\x1b[0m  ${passed} passed, ${failed} failed, ${skipped} skipped\n`);
process.exit(failed > 0 ? 1 : 0);
