#!/usr/bin/env node
'use strict';

const { parse } = require('../core/parser');
const { typecheck } = require('../core/typechecker');

let passed = 0, failed = 0;
function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m\u2713\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m\u2717\x1b[0m ${label}`); if (detail) console.log(`      \u2192 ${detail}`); failed++; }
}

function expectError(label, src, expectedSubstring) {
  try {
    const prog = parse(src);
    const diags = typecheck(prog);
    const depthErrs = diags.filter(d => d.code === 'DEPTH_CONTRACT');
    if (depthErrs.length > 0) {
      check(label, depthErrs.some(d => d.message.includes(expectedSubstring)),
        `expected to contain "${expectedSubstring}", got: ${depthErrs.map(d=>d.message).join('; ')}`);
    } else {
      check(label, false, 'no depth error from typechecker or parser');
    }
  } catch(e) {
    check(label, e.message.includes(expectedSubstring),
      `expected "${expectedSubstring}", got: ${e.message}`);
  }
}

function expectSuccess(label, src) {
  try {
    const prog = parse(src);
    const diags = typecheck(prog);
    const depthErrs = diags.filter(d => d.code === 'DEPTH_CONTRACT');
    check(label, depthErrs.length === 0,
      depthErrs.length ? depthErrs.map(d=>d.message).join('; ') : '');
  } catch(e) {
    check(label, false, `unexpected error: ${e.message}`);
  }
}

console.log('\n\x1b[1mDepth Contract Law Enforcement\x1b[0m\n');

// ── Positive tests (should succeed) ──
console.log('  \x1b[36m--- Valid programs ---\x1b[0m');

expectSuccess('ACTION declared at depth 0',
  'ACTION greet(n(NUM)),\n1\\\\ SHOW n.\n/ACTION.');

expectSuccess('REAP inside ACTION body',
  'ACTION test(),\n1\\\\ REAP x FROM SPLIT("a,b", ",").\n/ACTION.');

expectSuccess('CYCLE inside ACTION body',
  'ACTION test(),\n1\\\\ CYCLE i FROM 1 TO 5, SHOW i.\n/ACTION.');

expectSuccess('GIVE inside ACTION body',
  'ACTION test(n(NUM)),\n1\\\\ GIVE n.\n/ACTION.');

expectSuccess('SPECIES declared at depth 0',
  'SPECIES Animal,\nVAR name(TX).\n/SPECIES.');

expectSuccess('CYCLE nested inside ACTION with REAP in CYCLE body',
  `ACTION test(),
1\\\\ CYCLE i FROM 1 TO 5,
2\\\\ REAP x FROM SPLIT("a", ",").
2\\\\ SHOW x.
/ACTION.`);

expectSuccess('Braced ACTION body with valid depth',
  'ACTION foo(x(NUM)) { SHOW x. }');

expectSuccess('Braced ACTION body with REAP inside',
  'ACTION foo() { REAP x FROM SPLIT("a,b", ","). SHOW x. }');

// ── Negative tests (should fail with DepthContractError) ──
console.log('  \x1b[36m--- Invalid programs ---\x1b[0m');

expectError('REAP at depth 0',
  'REAP x FROM SPLIT("a,b", ",").',
  'DepthContractError');

expectError('CYCLE at depth 0',
  'CYCLE i FROM 1 TO 5, SHOW i.',
  'DepthContractError');

expectError('GIVE at depth 0',
  'GIVE 5.',
  'DepthContractError');

expectError('ACTION inside another ACTION (nested)',
  `ACTION outer(),
1\\ ACTION inner(), SHOW 1. /ACTION.
/ACTION.`,
  'DepthContractError');

expectError('REAP at top level with depth 0 (different syntax)',
  '1\\ REAP x FROM SPLIT("a", ",").',
  'DepthContractError');

console.log(`\n\x1b[1mResult: ${passed} passed, ${failed} failed\x1b[0m\n`);
process.exit(failed > 0 ? 1 : 0);
