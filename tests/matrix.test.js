#!/usr/bin/env node
'use strict';
/**
 * tests/matrix.test.js — Boundary Handshake Matrix test suite.
 * Tests all 25 transitions in the 5x5 matrix (21 ALLOW, 4 DENY)
 * and verifies exact standard error messages for DENY paths.
 */

const { validateBoundary, formatMatrix, MODES, PERMISSION_MATRIX } = require('../core/matrix');
const { BoundaryViolationError } = require('../core/errors');

let passed = 0, failed = 0;

function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m\u2713\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m\u2717\x1b[0m ${label}`); if (detail) console.log(`      \u2192 ${detail}`); failed++; }
}

function expectAllow(label, fromMode, toMode) {
  try {
    const result = validateBoundary(fromMode, toMode);
    check(label, result === true, `expected true, got ${result}`);
  } catch (e) {
    check(label, false, `unexpected error: ${e.message}`);
  }
}

function expectDeny(label, fromMode, toMode, expectedMessageSubstring) {
  try {
    validateBoundary(fromMode, toMode);
    check(label, false, `expected BoundaryViolationError but no error was thrown`);
  } catch (e) {
    if (e instanceof BoundaryViolationError) {
      const msgOk = !expectedMessageSubstring || e.message.includes(expectedMessageSubstring);
      check(label, msgOk, `expected message containing "${expectedMessageSubstring}", got: "${e.message}"`);
    } else {
      check(label, false, `expected BoundaryViolationError, got ${e.constructor.name}: ${e.message}`);
    }
  }
}

console.log('\n\x1b[1m5x5 Boundary Handshake Matrix\x1b[0m\n');
console.log(formatMatrix());
console.log('');

// ── Test all 25 transitions ──
console.log('  \x1b[36m--- ALLOW paths ---\x1b[0m');

// BALANCED -> *
expectAllow('BALANCED -> BALANCED', 'BALANCED', 'BALANCED');
expectAllow('BALANCED -> FAST', 'BALANCED', 'FAST');
expectAllow('BALANCED -> SAFE', 'BALANCED', 'SAFE');
expectAllow('BALANCED -> SMART', 'BALANCED', 'SMART');
expectAllow('BALANCED -> PERSISTENT', 'BALANCED', 'PERSISTENT');

// FAST -> *
expectAllow('FAST -> BALANCED', 'FAST', 'BALANCED');
expectAllow('FAST -> FAST', 'FAST', 'FAST');
expectAllow('FAST -> SMART', 'FAST', 'SMART');
expectAllow('FAST -> PERSISTENT', 'FAST', 'PERSISTENT');

// SAFE -> *
expectAllow('SAFE -> BALANCED', 'SAFE', 'BALANCED');
expectAllow('SAFE -> SAFE', 'SAFE', 'SAFE');

// SMART -> *
expectAllow('SMART -> BALANCED', 'SMART', 'BALANCED');
expectAllow('SMART -> FAST', 'SMART', 'FAST');
expectAllow('SMART -> SAFE', 'SMART', 'SAFE');
expectAllow('SMART -> SMART', 'SMART', 'SMART');
expectAllow('SMART -> PERSISTENT', 'SMART', 'PERSISTENT');

// PERSISTENT -> *
expectAllow('PERSISTENT -> BALANCED', 'PERSISTENT', 'BALANCED');
expectAllow('PERSISTENT -> FAST', 'PERSISTENT', 'FAST');
expectAllow('PERSISTENT -> SMART', 'PERSISTENT', 'SMART');
expectAllow('PERSISTENT -> PERSISTENT', 'PERSISTENT', 'PERSISTENT');

console.log('  \x1b[36m--- DENY paths (exact messages) ---\x1b[0m');

// FAST -> SAFE
expectDeny('FAST -> SAFE',
  'FAST', 'SAFE',
  'FAST cannot invoke SAFE due to conflicting performance/safety requirements');

// SAFE -> FAST
expectDeny('SAFE -> FAST',
  'SAFE', 'FAST',
  'SAFE is isolated and cannot invoke unguarded FAST code');

// SAFE -> SMART
expectDeny('SAFE -> SMART',
  'SAFE', 'SMART',
  'SAFE cannot invoke SMART as it may dynamically route to FAST');

// SAFE -> PERSISTENT
expectDeny('SAFE -> PERSISTENT',
  'SAFE', 'PERSISTENT',
  'SAFE cannot create persistent objects that outlive the isolated scope');

// ── Error path tests ──
console.log('  \x1b[36m--- Error paths ---\x1b[0m');

try {
  validateBoundary('UNKNOWN', 'BALANCED');
  check('Unknown fromMode throws error', false, 'no error thrown');
} catch (e) {
  check('Unknown fromMode throws error', e.message.includes('Unknown mission mode'), e.message);
}

try {
  validateBoundary('BALANCED', 'UNKNOWN');
  check('Unknown toMode throws error', false, 'no error thrown');
} catch (e) {
  check('Unknown toMode throws error', e.message.includes('Unknown mission mode'), e.message);
}

// ── Context propagation ──
console.log('  \x1b[36m--- Context propagation ---\x1b[0m');

try {
  validateBoundary('SAFE', 'FAST', { scopeId: 42, lineContext: 'test line' });
  check('Context scopeId in DENY', false, 'no error thrown');
} catch (e) {
  check('Context scopeId in DENY', e instanceof BoundaryViolationError && e.scopeId === 42,
    `scopeId=${e.scopeId}`);
  check('Context lineContext in DENY', e.lineContext === 'test line',
    `lineContext=${e.lineContext}`);
}

console.log(`\n\x1b[1mResult: ${passed} passed, ${failed} failed\x1b[0m\n`);
process.exit(failed > 0 ? 1 : 0);
