#!/usr/bin/env node
'use strict';
// ═══════════════════════════════════════════════════════════════
//  test_diagnostics.js — verifies the precision line/column
//  tracking and visual caret (^) pointer system end-to-end.
//
//  Run:  node tests/test_diagnostics.js
// ═══════════════════════════════════════════════════════════════
const path = require('path');
const { Interpreter } = require('../core/interpreter');
const { formatStormDiagnostic } = require('../core/diagnostics');

let passed = 0, failed = 0;

function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m✓\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m✗\x1b[0m ${label}`); if (detail) console.log(`      → ${detail}`); failed++; }
}

function runAndCapture(filePath) {
  const interp = new Interpreter({ emit: () => {} });
  try {
    interp.runFile(filePath);
    return { error: null };
  } catch (e) {
    return { error: e };
  }
}

console.log('\n\x1b[1mDiagnostic System Verification\x1b[0m\n');

// ── Test 1: MISSING_STORM carries correct line/column ──────────
{
  const file = path.join(__dirname, '..', 'examples', '06_diagnostics.plnt');
  const { error } = runAndCapture(file);
  check('MISSING_STORM is thrown', error && error.stormType === 'MISSING_STORM',
    error ? `got ${error.stormType}` : 'no error thrown');
  check('error carries a line number', error && typeof error.line === 'number', `line=${error && error.line}`);
  check('error carries a column number', error && typeof error.column === 'number', `column=${error && error.column}`);
  check('line points at the broken statement (21)', error && error.line === 21, `got line ${error && error.line}`);

  const panel = formatStormDiagnostic(error, file);
  check('panel contains storm banner', panel.includes('Atmospheric Storm Panic: MISSING_STORM'));
  check('panel contains --> location arrow', panel.includes('-->'));
  check('panel contains the exact file:line:column', panel.includes(`${file}:21:9`));
  check('panel contains the source line text', panel.includes('SHOW subtotl'));
  check('panel contains a caret (^) pointer', panel.includes('^'));
  check('panel does NOT leak a raw JS stack trace', !/at \S+ \(.*:\d+:\d+\)/.test(panel));

  // Verify the caret visually aligns under column 4 (the 'S' of SHOW)
  const lines = panel.split('\n');
  const codeLineIdx = lines.findIndex(l => l.includes('SHOW subtotl'));
  const caretLineIdx = lines.findIndex((l, i) => i > codeLineIdx && l.includes('^'));
  check('caret line immediately follows the code line', caretLineIdx === codeLineIdx + 1);
}

// ── Test 2: ZERO_STORM via evalExpr also carries location ──────
{
  const tmp = require('os').tmpdir() + '/diag_test_zero.plnt';
  require('fs').writeFileSync(tmp,
    'MISSION: SAFE.\n1\\ CREATE x(NUM) TO 10.\n1\\ SHOW x / 0.\n');
  const { error } = runAndCapture(tmp);
  check('ZERO_STORM is thrown', error && error.stormType === 'ZERO_STORM');
  check('ZERO_STORM carries line/column even though it originates in evaluator.js',
    error && typeof error.line === 'number' && typeof error.column === 'number',
    `line=${error && error.line} column=${error && error.column}`);
}

// ── Test 3: LOCK_STORM via Soil.update() (runtime.js) also carries location ──
{
  const tmp = require('os').tmpdir() + '/diag_test_lock.plnt';
  require('fs').writeFileSync(tmp,
    'MISSION: SAFE.\n1\\ CREATE score(NUM) TO 85.\n1\\ LOCK score.\n1\\ SET score TO 90.\n');
  const { error } = runAndCapture(tmp);
  check('LOCK_STORM is thrown', error && error.stormType === 'LOCK_STORM');
  check('LOCK_STORM carries line/column even though it originates in runtime.js Soil.update()',
    error && typeof error.line === 'number' && typeof error.column === 'number',
    `line=${error && error.line} column=${error && error.column}`);
  check('LOCK_STORM line is correct (4)', error && error.line === 4, `got line ${error && error.line}`);
}

// ── Test 4: clean process exit behavior (no crash) ──────────────
{
  const { execSync } = require('child_process');
  const file = path.join(__dirname, '..', 'examples', '06_diagnostics.plnt');
  let exitCode = 0, output = '';
  try {
    output = execSync(`node ${path.join(__dirname, '..', 'chloroplast.js')} run ${file} 2>&1`, { encoding: 'utf8' });
  } catch (e) {
    exitCode = e.status;
    output = (e.stdout || '') + (e.stderr || '');
  }
  check('CLI exits with code 1 (not a crash/segfault)', exitCode === 1, `exit code ${exitCode}`);
  check('CLI output contains the storm panel', output.includes('Atmospheric Storm Panic'));
  check('CLI output contains caret pointer', output.includes('^'));
  check('CLI did not print a raw Node stack trace', !output.includes('at Interpreter.'));
}

// ── Test 5: LISTEN BRANCH happy-path grammar parses correctly ──
{
  const tmp = require('os').tmpdir() + '/diag_test_listen_ok.plnt';
  require('fs').writeFileSync(tmp,
    'MISSION: SAFE.\n' +
    '1\\ CREATE cfg(MAP).\n' +
    '1\\ LISTEN BRANCH ON 8080 WITH cfg AS req MAP,\n' +
    '2\\   CREATE greeting(TX) TO "Hello".\n' +
    '2\\   GIVE greeting AS RESPONSE.\n' +
    '1\\ LISTEN/.\n');
  const { error } = runAndCapture(tmp);
  check('valid LISTEN BRANCH grammar does not throw', error === null,
    error ? `${error.stormType}: ${error.message}` : '');
}

// ── Test 6: SYNTAX_STORM fires with correct line/column/caret for ──
//           each of the four required connective keywords ────────
{
  const cases = [
    { name: 'missing ON',   line: '1\\ LISTEN BRANCH 8080 WITH cfg AS req MAP,', expectWord: 'ON' },
    { name: 'missing WITH', line: '1\\ LISTEN BRANCH ON 8080 cfg AS req MAP,',   expectWord: 'WITH' },
    { name: 'missing AS',   line: '1\\ LISTEN BRANCH ON 8080 WITH cfg req MAP,', expectWord: 'AS' },
    { name: 'missing MAP',  line: '1\\ LISTEN BRANCH ON 8080 WITH cfg AS req,',  expectWord: 'MAP' },
  ];
  for (const c of cases) {
    const tmp = require('os').tmpdir() + '/diag_test_listen_bad.plnt';
    require('fs').writeFileSync(tmp,
      `MISSION: SAFE.\n1\\ CREATE cfg(MAP).\n${c.line}\n2\\   GIVE "x" AS RESPONSE.\n1\\ LISTEN/.\n`);
    const { error } = runAndCapture(tmp);
    check(`${c.name}: SYNTAX_STORM is thrown`, error && error.stormType === 'SYNTAX_STORM',
      error ? `got ${error.stormType}` : 'no error');
    check(`${c.name}: carries line/column`, error && typeof error.line === 'number' && typeof error.column === 'number');
    check(`${c.name}: message references "${c.expectWord}"`, error && error.message.includes(c.expectWord),
      error ? error.message : '');

    const panel = formatStormDiagnostic(error, tmp);
    check(`${c.name}: panel has caret`, panel.includes('^'));
    check(`${c.name}: panel has SYNTAX_STORM banner`, panel.includes('SYNTAX_STORM'));
  }
}

// ── Test 7: reference example file (07_server_syntax_error.plnt) ──
{
  const { execSync } = require('child_process');
  const file = path.join(__dirname, '..', 'examples', '07_server_syntax_error.plnt');
  let exitCode = 0, output = '';
  try {
    output = execSync(`node ${path.join(__dirname, '..', 'chloroplast.js')} run ${file} 2>&1`, { encoding: 'utf8' });
  } catch (e) {
    exitCode = e.status;
    output = (e.stdout || '') + (e.stderr || '');
  }
  check('07_server_syntax_error.plnt: CLI exits with code 1', exitCode === 1, `exit code ${exitCode}`);
  check('07_server_syntax_error.plnt: shows SYNTAX_STORM panel', output.includes('SYNTAX_STORM'));
  check('07_server_syntax_error.plnt: shows caret pointer', output.includes('^'));
  check('07_server_syntax_error.plnt: no leaked JS stack trace', !output.includes('at Interpreter.'));
}

console.log(`\n${passed} passed, ${failed} failed\n`);
process.exit(failed > 0 ? 1 : 0);
