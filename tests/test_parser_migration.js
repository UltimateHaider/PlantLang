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
  interp._verifyDryRun = true;
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

// ── 12. WEATHER/SHELTER/CALM: well-formed grammar produces a fully ──
//       nested AST with accurate coordinates at every level ─────
{
  const src =
    'MISSION: SAFE.\n' +
    '1\\ WEATHER,\n' +
    '2\\   SHOW 10 / 0.\n' +
    '1\\ SHELTER ZERO_STORM AS err,\n' +
    '2\\   SHOW err.\n' +
    '1\\ CALM.\n';
  const ast = parse(src);
  const node = ast.statements.find(s => s.type === 'WeatherStatement');
  check('WeatherStatement node produced', !!node);
  check('conditionExpr is null (unconditional try-block grammar)', node && node.conditionExpr === null);
  check('bodyStatements has 1 nested statement', node && node.bodyStatements.length === 1);
  check('1 shelterClause captured', node && node.shelterClauses.length === 1);
  const shelter = node && node.shelterClauses[0];
  check('ShelterStatement node produced', shelter && shelter.type === 'ShelterStatement');
  check('stormType captured as ZERO_STORM', shelter && shelter.stormType === 'ZERO_STORM');
  check('errVar captured as "err"', shelter && shelter.errVar === 'err');
  check('shelter body has 1 nested statement', shelter && shelter.bodyStatements.length === 1);
  check('calmClause produced', node && node.calmClause && node.calmClause.type === 'CalmStatement');
  check('every node carries line/column/depth', node &&
    typeof node.line === 'number' && typeof node.column === 'number' && typeof node.depth === 'number' &&
    typeof shelter.line === 'number' && typeof shelter.column === 'number');
}

// ── 13. WEATHER: multiple SHELTER clauses + ANY_STORM fallback ──
{
  const src =
    'MISSION: SAFE.\n' +
    '1\\ WEATHER,\n' +
    '2\\   SHOW 1.\n' +
    '1\\ SHELTER ZERO_STORM,\n' +
    '2\\   SHOW "zero".\n' +
    '1\\ SHELTER TYPE_STORM,\n' +
    '2\\   SHOW "type".\n' +
    '1\\ SHELTER ANY_STORM,\n' +
    '2\\   SHOW "any".\n' +
    '1\\ CALM.\n';
  const ast = parse(src);
  const node = ast.statements.find(s => s.type === 'WeatherStatement');
  check('3 SHELTER clauses captured in order', node && node.shelterClauses.length === 3);
  check('clause order preserved: ZERO_STORM, TYPE_STORM, ANY_STORM',
    node && node.shelterClauses.map(c => c.stormType).join(',') === 'ZERO_STORM,TYPE_STORM,ANY_STORM');
}

// ── 14. WEATHER/SHELTER/CALM execution: success path (no storm) ──
{
  const out = [];
  const interp = new Interpreter({ emit: (t, tp) => out.push(t) });
  interp.runSource(
    'MISSION: SAFE.\n1\\ CREATE x(NUM) TO 10.\n1\\ WEATHER,\n2\\   SHOW x.\n' +
    '1\\ SHELTER ANY_STORM,\n2\\   SHOW "caught".\n1\\ CALM.\n');
  check('protected body runs and SHELTER does not fire on success', out.includes('10') && !out.includes('caught'));
}

// ── 15. WEATHER/SHELTER/CALM execution: storm caught, errVar bound ──
{
  const out = [];
  const interp = new Interpreter({ emit: (t, tp) => out.push(t) });
  interp.runSource(
    'MISSION: SAFE.\n1\\ WEATHER,\n2\\   SHOW 10 / 0.\n' +
    '1\\ SHELTER ZERO_STORM AS err,\n2\\   SHOW err.\n1\\ CALM.\n1\\ SHOW "after".\n');
  check('storm caught and errVar bound to the message text', out.some(t => t.includes('zero')));
  check('execution continues normally after CALM', out.includes('after'));
}

// ── 16. WEATHER/SHELTER/CALM: scope sandboxing — no leakage ──────
{
  const interp = new Interpreter({ emit: () => {} });
  interp.runSource(
    'MISSION: SAFE.\n1\\ WEATHER,\n2\\   CREATE inner_var(NUM) TO 99.\n' +
    '1\\ SHELTER ANY_STORM,\n2\\   SHOW "x".\n1\\ CALM.\n');
  check('WEATHER body locals do NOT leak to the outer scope', interp.soil.get('inner_var') === null);

  const interp2 = new Interpreter({ emit: () => {} });
  interp2.runSource(
    'MISSION: SAFE.\n1\\ WEATHER,\n2\\   SHOW 1 / 0.\n' +
    '1\\ SHELTER ZERO_STORM AS err,\n2\\   CREATE recovery_var(TX) TO "r".\n1\\ CALM.\n');
  check('SHELTER errVar does NOT leak to the outer scope', interp2.soil.get('err') === null);
  check('SHELTER body locals do NOT leak to the outer scope', interp2.soil.get('recovery_var') === null);
}

// ── 17. WEATHER/SHELTER/CALM: uncaught storm propagates with the ──
//        INNERMOST statement's coordinates, not the header's ──────
//        (regression guard for the location-backfill ordering fix)
{
  const src =
    'MISSION: SAFE.\n1\\ WEATHER,\n2\\   SHOW 10 / 0.\n' +
    '1\\ SHELTER TYPE_STORM,\n2\\   SHOW "wrong type".\n1\\ CALM.\n';
  const interp = new Interpreter({ emit: () => {} });
  let error = null;
  try { interp.runSource(src); } catch (e) { error = e; }
  check('non-matching SHELTER does not swallow the storm', error && error.stormType === 'ZERO_STORM');
  check('propagated storm carries the INNER SHOW statement\'s line (3)', error && error.line === 3,
    `got line ${error && error.line}`);
  check('propagated storm carries the INNER SHOW statement\'s column (6)', error && error.column === 6,
    `got column ${error && error.column}`);
  const panel = formatStormDiagnostic(error, null, src);
  check('diagnostic panel renders a caret for the uncaught storm', panel.includes('^'));
}

// ── 18. WEATHER/SHELTER grammar violations — SYNTAX_STORM accuracy ──
{
  let error = null;
  try {
    parse('1\\ WEATHER,\n2\\   SHOW 1.\n1\\ SHELTER ANY_STORM,\n2\\   SHOW 2.\n');
  } catch (e) { error = e; }
  check('missing CALM throws SYNTAX_STORM', error && error.stormType === 'SYNTAX_STORM');
  check('missing CALM error message is actionable', error && error.message.includes('CALM'));

  let error2 = null;
  try {
    parse('1\\ WEATHER,\n2\\   SHOW 1.\n1\\ SHELTER,\n2\\   SHOW 2.\n1\\ CALM.\n');
  } catch (e) { error2 = e; }
  check('SHELTER missing storm type throws SYNTAX_STORM', error2 && error2.stormType === 'SYNTAX_STORM');
  check('SHELTER missing storm type carries line/column', error2 && typeof error2.line === 'number' && typeof error2.column === 'number');
}

// ── 19. Full corpus: examples/03_storms.plnt (real pre-existing ──
//        WEATHER/SHELTER/CALM usage, 7 separate blocks) executes ──
//        end-to-end through the AST pipeline without throwing ─────
{
  const src = fs.readFileSync(path.join(__dirname, '..', 'examples', '03_storms.plnt'), 'utf8');
  const out = [];
  const interp = new Interpreter({ emit: (t, tp) => out.push(t) });
  let threw = false, errInfo = null;
  try { interp.runSource(src); } catch (e) { threw = true; errInfo = `${e.stormType}: ${e.message} (line ${e.line})`; }
  check('examples/03_storms.plnt (7 real WEATHER blocks) runs end-to-end without throwing',
    !threw, errInfo);
  check('CREATE colors(LIST) TO red, green. produces a real array (LIST special-case regression guard)',
    out.some(t => t.includes('colors') && t.includes('[red, green]')));
  check('ROOT MAX_SIZE referenced from inside a WEATHER block resolves correctly (regression guard)',
    out.some(t => t.includes('LOCK_STORM')));
}

// ── 20. Conditional WEATHER IF [cond], — new in this milestone ──
{
  // Parsing: bare WEATHER, still produces conditionExpr === null
  const astBare = parse('1\\ WEATHER,\n2\\   SHOW 1.\n1\\ SHELTER ANY_STORM,\n2\\   SHOW 2.\n1\\ CALM.\n');
  check('bare "WEATHER," still has conditionExpr === null (backward compat)',
    astBare.statements[0].conditionExpr === null);

  // Parsing: WEATHER IF cond, captures the condition text
  const astCond = parse('1\\ WEATHER IF flag,\n2\\   SHOW 1.\n1\\ SHELTER ANY_STORM,\n2\\   SHOW 2.\n1\\ CALM.\n');
  check('"WEATHER IF flag," captures conditionExpr', astCond.statements[0].conditionExpr === 'flag');

  // Execution: condition true -> body runs
  const out1 = [];
  new (require('../core/interpreter').Interpreter)({ emit: (t) => out1.push(t) }).runSource(
    'MISSION: SAFE.\n1\\ CREATE flag(FACT) TO FACT:TRUE.\n1\\ WEATHER IF flag,\n2\\   SHOW "ran".\n' +
    '1\\ SHELTER ANY_STORM,\n2\\   SHOW "caught".\n1\\ CALM.\n1\\ SHOW "after".\n');
  check('conditional WEATHER: TRUE condition runs the protected body', out1.includes('ran'));
  check('conditional WEATHER: TRUE condition does not trigger SHELTER', !out1.includes('caught'));
  check('conditional WEATHER: execution continues after CALM (true branch)', out1.includes('after'));

  // Execution: condition false -> body AND shelter both skipped, CALM still runs (no-op)
  const out2 = [];
  new (require('../core/interpreter').Interpreter)({ emit: (t) => out2.push(t) }).runSource(
    'MISSION: SAFE.\n1\\ CREATE flag(FACT) TO FACT:FALSE.\n1\\ WEATHER IF flag,\n2\\   SHOW "should not run".\n' +
    '1\\ SHELTER ANY_STORM,\n2\\   SHOW "should not run either".\n1\\ CALM.\n1\\ SHOW "after".\n');
  check('conditional WEATHER: FALSE condition skips the protected body', !out2.includes('should not run'));
  check('conditional WEATHER: FALSE condition skips SHELTER entirely', !out2.includes('should not run either'));
  check('conditional WEATHER: execution continues normally after CALM (false branch)', out2.includes('after'));

  // SYNTAX_STORM: WEATHER IF with no condition expression before the comma
  let error = null;
  try { parse('1\\ WEATHER IF,\n2\\   SHOW 1.\n1\\ CALM.\n'); } catch (e) { error = e; }
  check('"WEATHER IF," with no condition throws SYNTAX_STORM', error && error.stormType === 'SYNTAX_STORM');
}

// ── 21. ACTION: parses into typed AST node with params and body ──
{
  const ast = parse(
    'MISSION: SAFE.\n1\\ ACTION add(a(NUM), b(NUM)),\n2\\   GIVE a + b.\n1\\ /ACTION.\n');
  const node = ast.statements.find(s => s.type === 'ActionDeclaration');
  check('ActionDeclaration node produced', !!node);
  check('action name captured', node && node.name === 'add');
  check('2 params captured', node && node.params.length === 2);
  check('param a has type NUM', node && node.params[0].name === 'a' && node.params[0].type === 'NUM');
  check('body has 1 statement', node && node.bodyStatements.length === 1);
  check('ActionDeclaration has line/column/depth', node && typeof node.line === 'number');
}

// ── 22. ACTION: executes correctly via AST pipeline ─────────────
{
  const out = [];
  const interp = new Interpreter({ emit: (t) => out.push(t) });
  interp.runSource(
    'MISSION: SAFE.\n1\\ ACTION multiply(a(NUM), b(NUM)),\n2\\   GIVE a * b.\n1\\ /ACTION.\n' +
    '1\\ REAP result FROM multiply, 6, 7.\n1\\ SHOW result.\n');
  check('ACTION executes and returns correct value via AST pipeline', out.includes('42'),
    JSON.stringify(out));
}

// ── 23. SPECIES: parses with fields, methods, inheritance ───────
{
  const ast = parse(
    'MISSION: SAFE.\n1\\ SPECIES Animal,\n2\\   VAR name(TX) TO "?".\n' +
    '2\\   ACTION speak(),\n3\\     GIVE SELF:name.\n2\\   /ACTION.\n1\\ /SPECIES.\n');
  const node = ast.statements.find(s => s.type === 'SpeciesDeclaration');
  check('SpeciesDeclaration node produced', !!node);
  check('species name captured', node && node.name === 'Animal');
  check('parentName is null for base species', node && node.parentName === null);
  check('1 field captured (name)', node && node.fields.length === 1);
  check('1 method captured (speak)', node && node.actions.length === 1);
  check('method name captured', node && node.actions[0].name === 'speak');
}

// ── 24. SPECIES + BLOOM: full execution via AST pipeline ─────────
{
  const out = [];
  const interp = new Interpreter({ emit: (t) => out.push(t) });
  interp.runSource(
    'MISSION: SAFE.\n' +
    '1\\ SPECIES Greeter,\n2\\   VAR msg(TX) TO "Hello".\n' +
    '2\\   ACTION greet(),\n3\\     GIVE SELF:msg + " world!".\n2\\   /ACTION.\n1\\ /SPECIES.\n' +
    '1\\ BLOOM Greeter AS g.\n1\\ REAP result FROM g:greet.\n1\\ SHOW result.\n');
  check('SPECIES + BLOOM + method call via AST pipeline', out.some(t => t.includes('Hello world!')),
    JSON.stringify(out));
}

// ── 25. SPECIES inheritance via PARENT ───────────────────────────
{
  const out = [];
  const interp = new Interpreter({ emit: (t) => out.push(t) });
  interp.runSource(
    'MISSION: SAFE.\n' +
    '1\\ SPECIES Base,\n2\\   VAR x(NUM) TO 10.\n1\\ /SPECIES.\n' +
    '1\\ SPECIES Child PARENT Base,\n2\\   VAR y(NUM) TO 20.\n1\\ /SPECIES.\n' +
    '1\\ BLOOM Child AS c.\n1\\ SHOW c:x.\n1\\ SHOW c:y.\n');
  check('inherited field x is visible on Child instance', out.some(t => String(t) === '10' || t.includes('10')));
  check('own field y is visible on Child instance', out.some(t => String(t) === '20' || t.includes('20')));
}

// ── 26. symbolPass: forward reference works (ACTION called before its ──
//        declaration line in the source) ──────────────────────────
{
  const out = [];
  const interp = new Interpreter({ emit: (t) => out.push(t) });
  interp.runSource(
    'MISSION: SAFE.\n' +
    '1\\ REAP result FROM triple, 7.\n' +   // called BEFORE the declaration
    '1\\ SHOW result.\n' +
    '1\\ ACTION triple(n(NUM)),\n2\\   GIVE n * 3.\n1\\ /ACTION.\n');
  check('symbolPass: forward reference to ACTION before declaration succeeds', out.includes('21'));
}

// ── 27. symbolPass: ROOT constant registered via symbolPass (not ──
//        legacy _firstPass on raw stmts) ─────────────────────────
{
  const out = [];
  const interp = new Interpreter({ emit: (t) => out.push(t) });
  interp.runSource(
    'MISSION: SAFE.\nROOT MAX_VAL TO 100.\n1\\ SHOW MAX_VAL.\n');
  check('ROOT constant registered via symbolPass and visible at runtime',
    out.some(t => String(t) === '100'));
}

// ── 28. _symbolPassDone: ACTION/SPECIES not double-emitted ──────
{
  const out = [];
  const interp = new Interpreter({ emit: (t, tp) => tp === 'ok' && out.push(t) });
  interp.runSource(
    'MISSION: SAFE.\n1\\ ACTION foo(),\n2\\   GIVE 1.\n1\\ /ACTION.\n');
  const actionEmits = out.filter(t => t.includes('ACTION "foo"'));
  check('ACTION declaration emitted exactly once (not twice)', actionEmits.length === 1,
    `emitted ${actionEmits.length} times`);
}

console.log(`\n${passed} passed, ${failed} failed\n`);
process.exit(failed > 0 ? 1 : 0);
