#!/usr/bin/env node
'use strict';
// ═══════════════════════════════════════════════════════════════
//  tests/test_parser_migration.js
//
//  Verifies the compiler-frontend migration milestone: tokenizer →
//  parser → AST → evaluateNode() bridge, for the statement kinds
//  migrated so far (SHOW, CREATE, LISTEN BRANCH, RESPONSE), plus
//  full-corpus parse-without-crash coverage and SYNTAX_STORM
//  diagnostic accuracy through the new pipeline.
//
//  This suite is ADDITIVE — it does not replace or modify
//  tests/test_diagnostics.js, tests/all.plnt, or tests/suite.plnt,
//  which continue to exercise the original regex-based engine and
//  must remain green (176/176) per the migration's hard constraint.
// ═══════════════════════════════════════════════════════════════
const fs = require('fs');
const path = require('path');
const { tokenize, TOKEN } = require('../core/tokenizer');
const { parse, Parser } = require('../core/parser');
const { Interpreter } = require('../core/interpreter');
const { formatStormDiagnostic } = require('../core/diagnostics');

let passed = 0, failed = 0;
function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m✓\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m✗\x1b[0m ${label}`); if (detail) console.log(`      → ${detail}`); failed++; }
}

console.log('\n\x1b[1mParser / AST Migration Verification\x1b[0m\n');

// ── 1. CREATE statement parses into a typed AST node with exact coords ──
{
  const ast = parse('1\\ CREATE x(NUM) TO 42.\n');
  const node = ast.statements[0];
  check('CreateStatement node produced', node.type === 'CreateStatement');
  check('identifier captured', node.identifier === 'x');
  check('varType captured', node.varType === 'NUM');
  check('value literal captured', node.valueExpr.value === 42 && node.valueExpr.literalType === 'NUMBER');
  check('node line/column/depth recorded', node.line === 1 && node.column === 4 && node.depth === 1);
}

// ── 2. SHOW statement parses into a typed AST node ──
{
  const ast = parse('1\\ CREATE x(NUM) TO 5.\n1\\ SHOW x.\n');
  const node = ast.statements[1];
  check('ShowStatement node produced', node.type === 'ShowStatement');
  check('expr resolves to an Identifier node', node.expr.type === 'Identifier' && node.expr.name === 'x');
}

// ── 3. AST execution bridge: CREATE + SHOW actually run through Soil ──
{
  const out = [];
  const interp = new Interpreter({ emit: (t, tp) => out.push({ t, tp }) });
  interp.runSource('1\\ CREATE x(NUM) TO 42.\n1\\ CREATE name(TX) TO "PlantLang".\n1\\ SHOW x.\n1\\ SHOW name.\n');
  check('CREATE executed via evaluateCreateStatement', out.some(o => o.t.includes('CREATE "x"(NUM) = 42')));
  check('SHOW executed via evaluateShowStatement (42)', out.some(o => o.t === '42'));
  check('SHOW executed via evaluateShowStatement (PlantLang)', out.some(o => o.t === 'PlantLang'));
}

// ── 4. CREATE with PULSE modifier ──
{
  const ast = parse('1\\ CREATE temp(NUM) PULSE TO 20.\n');
  check('PULSE modifier recognized', ast.statements[0].isPulse === true);
}

// ── 5. CREATE x(LIST) TO. (bare empty-list idiom) parses without error ──
{
  let threw = false;
  try { parse('1\\ CREATE items(LIST) TO.\n'); } catch (e) { threw = true; }
  check('bare "TO." for empty LIST does not throw', !threw);
}

// ── 6. SYNTAX_STORM on malformed CREATE — exact column verified ──
{
  let error = null;
  try { parse('1\\ CREATE x NUM) TO 42.\n'); } catch (e) { error = e; }
  check('SYNTAX_STORM thrown for missing "("', error && error.stormType === 'SYNTAX_STORM');
  check('error column points at "NUM" (col 13)', error && error.column === 13, `got ${error && error.column}`);
  const panel = formatStormDiagnostic(error, null, '1\\ CREATE x NUM) TO 42.\n');
  check('diagnostic panel renders a caret', panel.includes('^'));
}

// ── 7. LISTEN BRANCH: well-formed grammar produces a fully nested AST ──
{
  const src =
    'MISSION: SAFE.\n' +
    '1\\ CREATE cfg(MAP).\n' +
    '1\\ LISTEN BRANCH ON 8080 WITH cfg AS req MAP,\n' +
    '2\\   CREATE greeting(TX) TO "Hello".\n' +
    '2\\   GIVE greeting AS RESPONSE.\n' +
    '1\\ LISTEN/.\n';
  const ast = parse(src);
  const node = ast.statements.find(s => s.type === 'ListenBranchStatement');
  check('ListenBranchStatement node produced', !!node);
  check('portExpr captured', node && node.portExpr === '8080');
  check('configExpr captured', node && node.configExpr === 'cfg');
  check('requestIdent captured', node && node.requestIdent === 'req');
  check('body has 2 nested statements', node && node.bodyStatements.length === 2,
    node ? node.bodyStatements.length : 'n/a');
  check('first body statement is a typed CreateStatement (not RawStatement)',
    node && node.bodyStatements[0].type === 'CreateStatement');
  check('second body statement is a typed ResponseStatement', node && node.bodyStatements[1].type === 'ResponseStatement');
  check('ResponseStatement captures responseExpr', node && node.bodyStatements[1].responseExpr === 'greeting');
}

// ── 8. LISTEN BRANCH: full AST execution produces correct RESPONSE ──
//      (regression guard for the execution-order bug found & fixed
//      during this migration: RESPONSE must evaluate AFTER prior
//      body statements have run, not via a pre-scan)
{
  const src =
    'MISSION: SAFE.\n' +
    '1\\ CREATE cfg(MAP).\n' +
    '1\\ LISTEN BRANCH ON 8080 WITH cfg AS req MAP,\n' +
    '2\\   CREATE greeting(TX) TO "Hello AST".\n' +
    '2\\   GIVE greeting AS RESPONSE.\n' +
    '1\\ LISTEN/.\n';
  const out = [];
  const interp = new Interpreter({ emit: (t, tp) => out.push({ t, tp }) });
  interp.runSource(src);
  check('RESPONSE resolved to the correct value (not the bare identifier text)',
    out.some(o => o.t.includes('RESPONSE: Hello AST')),
    JSON.stringify(out));
}

// ── 9. LISTEN BRANCH grammar violations — all 4 connective keywords ──
{
  const cases = [
    { name: 'missing ON',   line: '1\\ LISTEN BRANCH 8080 WITH cfg AS req MAP,', expectWord: 'ON' },
    { name: 'missing WITH', line: '1\\ LISTEN BRANCH ON 8080 cfg AS req MAP,',   expectWord: 'WITH' },
    { name: 'missing AS',   line: '1\\ LISTEN BRANCH ON 8080 WITH cfg req MAP,', expectWord: 'AS' },
    { name: 'missing MAP',  line: '1\\ LISTEN BRANCH ON 8080 WITH cfg AS req,',  expectWord: 'MAP' },
  ];
  for (const c of cases) {
    const src = `MISSION: SAFE.\n1\\ CREATE cfg(MAP).\n${c.line}\n2\\   GIVE "x" AS RESPONSE.\n1\\ LISTEN/.\n`;
    let error = null;
    try { parse(src); } catch (e) { error = e; }
    check(`${c.name}: SYNTAX_STORM thrown by new parser`, error && error.stormType === 'SYNTAX_STORM',
      error ? error.message : 'no error');
    check(`${c.name}: message references "${c.expectWord}"`, error && error.message.includes(c.expectWord));
    check(`${c.name}: carries line/column`, error && typeof error.line === 'number' && typeof error.column === 'number');
  }
}

// ── 10. Whole-corpus parse-without-crash coverage ──────────────
{
  const exampleDir = path.join(__dirname, '..', 'examples');
  const files = fs.readdirSync(exampleDir).filter(f => f.endsWith('.plnt'));
  for (const f of files) {
    const src = fs.readFileSync(path.join(exampleDir, f), 'utf8');
    const isIntentionallyBroken = f.includes('syntax_error');
    let threw = false, stormType = null;
    try { parse(src); } catch (e) { threw = true; stormType = e.stormType; }
    if (isIntentionallyBroken) {
      check(`${f}: intentionally-broken file correctly throws SYNTAX_STORM`, threw && stormType === 'SYNTAX_STORM');
    } else {
      check(`${f}: parses without crashing`, !threw, threw ? stormType : '');
    }
  }
  for (const f of ['all.plnt', 'suite.plnt']) {
    const src = fs.readFileSync(path.join(__dirname, f), 'utf8');
    let threw = false;
    try { parse(src); } catch (e) { threw = true; }
    check(`tests/${f}: parses without crashing`, !threw);
  }
}

// ── 11. Legacy engine (run()/_execBlock) remains completely untouched ──
{
  const { execSync } = require('child_process');
  const root = path.join(__dirname, '..');
  let r1 = '', r2 = '';
  try { r1 = execSync(`node ${path.join(root, 'chloroplast.js')} run ${path.join(root, 'tests/all.plnt')} 2>&1`, { encoding: 'utf8' }); } catch (e) { r1 = e.stdout || ''; }
  try { r2 = execSync(`node ${path.join(root, 'chloroplast.js')} verify ${path.join(root, 'tests/suite.plnt')} 2>&1`, { encoding: 'utf8' }); } catch (e) { r2 = e.stdout || ''; }
  check('legacy tests/all.plnt still 56/56 via the original engine', r1.includes('✕ فشل: 0'));
  check('legacy tests/suite.plnt still passing via the original engine', r2.includes('0 failed'));
}

console.log(`\n${passed} passed, ${failed} failed\n`);
process.exit(failed > 0 ? 1 : 0);
