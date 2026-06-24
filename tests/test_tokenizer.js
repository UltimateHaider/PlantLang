#!/usr/bin/env node
'use strict';
// tests/test_tokenizer.js — verifies core/tokenizer.js character-level
// scanning, depth inheritance, and absolute column accuracy.
const { tokenize, TOKEN } = require('../core/tokenizer');

let passed = 0, failed = 0;
function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m✓\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m✗\x1b[0m ${label}`); if (detail) console.log(`      → ${detail}`); failed++; }
}

console.log('\n\x1b[1mTokenizer Verification\x1b[0m\n');

// Test 1: basic CREATE statement — exact column cross-check
{
  const toks = tokenize('1\\ CREATE x(NUM) TO 42.\n');
  const byVal = (v) => toks.find(t => t.value === v);
  check('DEPTH token value=1', toks[0].type === TOKEN.DEPTH && toks[0].value === 1);
  check('CREATE keyword at col 4', byVal('CREATE').column === 4);
  check('identifier x at col 11', byVal('x').column === 11);
  check('NUM keyword at col 13', byVal('NUM').column === 13);
  check('TO keyword at col 18', byVal('TO').column === 18);
  check('NUMBER 42 at col 21', toks.find(t => t.type === TOKEN.NUMBER).column === 21);
}

// Test 2: depth inheritance across multiple lines
{
  const toks = tokenize('1\\ SHOW a.\n2\\   SHOW b.\n3\\ SHOW c.\n');
  const showToks = toks.filter(t => t.value === 'SHOW');
  check('3 SHOW tokens found', showToks.length === 3);
  check('first SHOW depth=1', showToks[0].depth === 1);
  check('second SHOW depth=2', showToks[1].depth === 2);
  check('third SHOW depth=3', showToks[2].depth === 3);
  check('second SHOW column accounts for extra indent (col=6)', showToks[1].column === 6,
    `got ${showToks[1].column}`);
}

// Test 3: string literal with spaces
{
  const toks = tokenize('1\\ SHOW "hello world".\n');
  const str = toks.find(t => t.type === TOKEN.STRING);
  check('string value captured correctly', str.value === 'hello world', str.value);
}

// Test 4: FACT:TRUE / FACT:FALSE literals
{
  const toks = tokenize('1\\ CREATE a(FACT) TO FACT:TRUE.\n1\\ CREATE b(FACT) TO FACT:FALSE.\n');
  const facts = toks.filter(t => t.type === TOKEN.FACT);
  check('2 FACT tokens found', facts.length === 2);
  check('FACT:TRUE -> value true', facts[0].value === true);
  check('FACT:FALSE -> value false', facts[1].value === false);
}

// Test 5: punctuation tokens
{
  const toks = tokenize('1\\ REAP r FROM add, 3, 4.\n');
  const puncts = toks.filter(t => t.type === TOKEN.PUNCT).map(t => t.value);
  check('commas tokenized', puncts.filter(p => p === ',').length === 2, puncts.join(''));
  check('period tokenized', puncts.includes('.'));
}

// Test 6: LISTEN BRANCH keyword set (new grammar from prior phase)
{
  const toks = tokenize('1\\ LISTEN BRANCH ON 8080 WITH cfg AS req MAP,\n');
  const kw = toks.filter(t => t.type === TOKEN.KEYWORD).map(t => t.value);
  ['LISTEN', 'BRANCH', 'ON', 'WITH', 'AS', 'MAP'].forEach(k => {
    check(`"${k}" recognized as KEYWORD`, kw.includes(k));
  });
}

// Test 7: stream always terminates with EOF
{
  const toks = tokenize('1\\ SHOW x.\n');
  check('stream ends with EOF token', toks[toks.length - 1].type === TOKEN.EOF);
}

// Test 8: WEATHER / SHELTER / CALM keyword + column accuracy
// (these were already registered in the original tokenizer build —
// this test exists to lock that in as a permanent regression guard
// for the WEATHER/SHELTER/CALM AST migration milestone)
{
  const toks = tokenize('1\\ WEATHER,\n1\\ SHELTER ZERO_STORM AS err,\n1\\ CALM.\n');
  const byVal = (v) => toks.find(t => t.value === v);
  check('WEATHER tokenized as KEYWORD', byVal('WEATHER').type === TOKEN.KEYWORD);
  check('WEATHER at col 4', byVal('WEATHER').column === 4);
  check('SHELTER tokenized as KEYWORD', byVal('SHELTER').type === TOKEN.KEYWORD);
  check('SHELTER at col 4', byVal('SHELTER').column === 4);
  check('storm type ZERO_STORM tokenized as IDENT (open set, not reserved)',
    byVal('ZERO_STORM').type === TOKEN.IDENT);
  check('AS at col 23', byVal('AS').column === 23);
  check('err identifier at col 26', byVal('err').column === 26);
  check('CALM tokenized as KEYWORD', byVal('CALM').type === TOKEN.KEYWORD);
  check('CALM at col 4', byVal('CALM').column === 4);
}

console.log(`\n${passed} passed, ${failed} failed\n`);
process.exit(failed > 0 ? 1 : 0);
