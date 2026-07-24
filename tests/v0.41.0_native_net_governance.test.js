const { CodeWordsChecker, SecurityViolationError, ALLOWED_DIRECTIVES } = require('../src/security/codewords_governance');
const { TestRunner } = require('../src/testing/test_runner');

let passed = 0, failed = 0;

function assert(cond, label) {
  if (cond) { passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  else { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label); }
}

function assertEqual(a, b, label) {
  if (a === b) { passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  else { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label + ' (' + JSON.stringify(a) + ' !== ' + JSON.stringify(b) + ')'); }
}

function assertThrows(fn, label) {
  try { fn(); failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label); }
  catch (e) { passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
}

function assertNotThrows(fn, label) {
  try { fn(); passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  catch (e) { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label + ': ' + e.message); }
}

function main() {

// ═══════════════════════════════════════════════════════════════
//  CodeWords Governance — Directive Parsing
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- CodeWords: Directive Parsing ---\u001b[0m');

(function() {
  const dirs = CodeWordsChecker.parseDirectives('#ALLOW_NETWORK\n#ALLOW_HARVEST\n');
  assertEqual(dirs.length, 2, 'parseDirectives finds two directives');
  assertEqual(dirs[0], '#ALLOW_NETWORK', 'first directive is #ALLOW_NETWORK');
  assertEqual(dirs[1], '#ALLOW_HARVEST', 'second directive is #ALLOW_HARVEST');
})();

(function() {
  const dirs = CodeWordsChecker.parseDirectives('SHOW "hello".\n#ALLOW_LISTEN\nREAP x FROM NOW.');
  assertEqual(dirs.length, 1, 'parseDirectives filters non-directive lines');
  assertEqual(dirs[0], '#ALLOW_LISTEN', 'only #ALLOW_LISTEN extracted');
})();

(function() {
  const dirs = CodeWordsChecker.parseDirectives('');
  assertEqual(dirs.length, 0, 'parseDirectives on empty source');
})();

(function() {
  const dirs = CodeWordsChecker.parseDirectives('# comment\n#ALLOW_NETWORK  \n');
  assertEqual(dirs.length, 1, 'parseDirectives trims whitespace');
  assertEqual(dirs[0], '#ALLOW_NETWORK', 'extracted #ALLOW_NETWORK');
})();

// ═══════════════════════════════════════════════════════════════
//  CodeWords Governance — Permission Checks
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- CodeWords: Permission Checks ---\u001b[0m');

(function() {
  const checker = new CodeWordsChecker(['#ALLOW_NETWORK']);
  assert(checker.hasDirective('#ALLOW_NETWORK'), '#ALLOW_NETWORK present');
  assert(checker.hasDirective('#ALLOW_HARVEST'), '#ALLOW_HARVEST implied by #ALLOW_NETWORK');
  assert(checker.hasDirective('#ALLOW_LISTEN'), '#ALLOW_LISTEN implied by #ALLOW_NETWORK');
})();

(function() {
  const checker = new CodeWordsChecker(['#ALLOW_HARVEST']);
  assert(checker.hasDirective('#ALLOW_HARVEST'), '#ALLOW_HARVEST present');
  assert(!checker.hasDirective('#ALLOW_LISTEN'), '#ALLOW_LISTEN not implied by #ALLOW_HARVEST');
  assert(!checker.hasDirective('#ALLOW_NETWORK'), '#ALLOW_NETWORK not implied by #ALLOW_HARVEST');
})();

(function() {
  const checker = new CodeWordsChecker([]);
  assert(!checker.hasDirective('#ALLOW_NETWORK'), 'no directives, no network');
  assert(!checker.hasDirective('#ALLOW_HARVEST'), 'no directives, no harvest');
  assert(!checker.hasDirective('#ALLOW_LISTEN'), 'no directives, no listen');
})();

(function() {
  const checker = new CodeWordsChecker(['#ALLOW_LISTEN']);
  assert(checker.hasDirective('#ALLOW_LISTEN'), '#ALLOW_LISTEN present');
  assert(!checker.hasDirective('#ALLOW_HARVEST'), '#ALLOW_HARVEST not implied by #ALLOW_LISTEN');
})();

(function() {
  const checker = new CodeWordsChecker(['#ALLOW_NETWORK', '#ALLOW_HARVEST']);
  assert(checker.hasDirective('#ALLOW_NETWORK'), 'both present: network');
  assert(checker.hasDirective('#ALLOW_HARVEST'), 'both present: harvest');
  assert(checker.hasDirective('#ALLOW_LISTEN'), 'both present: listen (implied by network)');
  assertEqual(checker.getDirectives().length, 2, 'getDirectives returns 2 entries');
})();

// ═══════════════════════════════════════════════════════════════
//  CodeWords Governance — Static AST Check
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- CodeWords: AST Security Check ---\u001b[0m');

(function() {
  const checker = new CodeWordsChecker(['#ALLOW_HARVEST']);
  const node = { type: 'HarvestStatement', line: 5, column: 3, urlExpr: 'http://test' };
  const ok = checker.checkNode(node, 'test.plant');
  assert(ok, 'HarvestStatement allowed with #ALLOW_HARVEST');
  assertEqual(checker.getViolations().length, 0, 'no violations');
})();

(function() {
  const checker = new CodeWordsChecker([]);
  const node = { type: 'HarvestStatement', line: 10, column: 1, urlExpr: 'http://test' };
  const ok = checker.checkNode(node, 'test.plant');
  assert(!ok, 'HarvestStatement blocked without #ALLOW_HARVEST');
  assertEqual(checker.getViolations().length, 1, 'one violation recorded');
  assert(checker.getViolations()[0] instanceof SecurityViolationError, 'violation is SecurityViolationError');
  assert(checker.getViolations()[0].message.includes('#ALLOW_HARVEST'), 'message mentions required directive');
})();

(function() {
  const checker = new CodeWordsChecker([]);
  const node = { type: 'ListenBranchStatement', line: 15, column: 1, portExpr: '8080' };
  const ok = checker.checkNode(node, 'test.plant');
  assert(!ok, 'ListenBranchStatement blocked without #ALLOW_LISTEN');
  assertEqual(checker.getViolations().length, 1, 'one violation recorded');
  assert(checker.getViolations()[0].message.includes('#ALLOW_LISTEN'), 'message mentions #ALLOW_LISTEN');
})();

(function() {
  const checker = new CodeWordsChecker(['#ALLOW_NETWORK']);
  const harvest = { type: 'HarvestStatement', line: 1, column: 1, urlExpr: 'http://x' };
  const listen = { type: 'ListenBranchStatement', line: 2, column: 1, portExpr: '9090' };
  assert(checker.checkNode(harvest, 'test.plant'), 'Harvest allowed by #ALLOW_NETWORK');
  assert(checker.checkNode(listen, 'test.plant'), 'ListenBranch allowed by #ALLOW_NETWORK');
})();

(function() {
  const checker = new CodeWordsChecker(['#ALLOW_HARVEST']);
  const program = {
    type: 'Program',
    statements: [
      { type: 'ShowStatement', line: 1, column: 1, expr: '"hello"' },
      { type: 'HarvestStatement', line: 2, column: 1, urlExpr: 'http://api' },
    ]
  };
  const violations = checker.checkAST(program, 'test.plant');
  assertEqual(violations.length, 0, 'AST with Harvest + #ALLOW_HARVEST passes');
})();

(function() {
  const checker = new CodeWordsChecker([]);
  const program = {
    type: 'Program',
    statements: [
      { type: 'ShowStatement', expr: '"hello"' },
      { type: 'ListenBranchStatement', line: 20, column: 5, portExpr: '8080' },
    ]
  };
  const violations = checker.checkAST(program, 'no_directives.plant');
  assertEqual(violations.length, 1, 'AST with ListenBranch + no directives fails');
  assert(violations[0].message.includes('#ALLOW_LISTEN'), 'violation mentions #ALLOW_LISTEN');
})();

// ═══════════════════════════════════════════════════════════════
//  CodeWords Governance — SecurityViolationError
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- CodeWords: SecurityViolationError ---\u001b[0m');

(function() {
  const err = new SecurityViolationError('HarvestStatement', '#ALLOW_HARVEST', 'details');
  assert(err instanceof Error, 'SecurityViolationError is an Error');
  assertEqual(err.name, 'SecurityViolationError', 'error name set');
  assert(err.message.includes('HarvestStatement'), 'message includes node type');
  assert(err.message.includes('#ALLOW_HARVEST'), 'message includes directive');
  assertEqual(err.nodeType, 'HarvestStatement', 'nodeType property set');
  assertEqual(err.requiredDirective, '#ALLOW_HARVEST', 'requiredDirective property set');
})();

// ═══════════════════════════════════════════════════════════════
//  TestRunner — Basic SUITE / VERIFY Execution
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- TestRunner: Basic SUITE/VERIFY ---\u001b[0m');

(function() {
  const runner = new TestRunner();
  const result = runner.runSuite({
    type: 'SuiteStatement',
    name: 'Math Tests',
    bodyStatements: [
      { type: 'VerifyStatement', label: 'true is true', assertion: true, line: 1, column: 1 },
      { type: 'VerifyStatement', label: 'false is false', assertion: false, line: 2, column: 1 },
    ]
  });
  assertEqual(result.name, 'Math Tests', 'suite name preserved');
  assertEqual(result.passed, 1, 'one assertion passed');
  assertEqual(result.failed, 1, 'one assertion failed');
  assertEqual(result.assertions.length, 2, 'two assertions recorded');
  assert(result.assertions[0].passed, 'first assertion passed');
  assert(!result.assertions[1].passed, 'second assertion failed');
})();

(function() {
  const runner = new TestRunner();
  const result = runner.runSuite({
    type: 'SuiteStatement',
    name: 'String Assertions',
    bodyStatements: [
      { type: 'VerifyStatement', label: 'truthy string', assertion: '"hello"', line: 1, column: 1 },
      { type: 'VerifyStatement', label: 'falsy string', assertion: 'false', line: 2, column: 1 },
    ]
  });
  assertEqual(result.passed, 1, 'truthy string passes');
  assertEqual(result.failed, 1, 'false string fails');
})();

(function() {
  const runner = new TestRunner();
  const result = runner.runSuite({
    type: 'SuiteStatement',
    name: 'Number Assertions',
    bodyStatements: [
      { type: 'VerifyStatement', label: 'non-zero passes', assertion: '42', line: 1, column: 1 },
      { type: 'VerifyStatement', label: 'zero passes', assertion: '0', line: 2, column: 1 },
    ]
  });
  assertEqual(result.passed, 1, 'non-zero number passes');
  assertEqual(result.failed, 1, 'zero number fails');
})();

// ═══════════════════════════════════════════════════════════════
//  TestRunner — Nested SUITE Blocks
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- TestRunner: Nested SUITE Blocks ---\u001b[0m');

(function() {
  const runner = new TestRunner();
  const outerResult = runner.runSuite({
    type: 'SuiteStatement',
    name: 'Outer Suite',
    bodyStatements: [
      { type: 'VerifyStatement', label: 'outer-1', assertion: true, line: 1, column: 1 },
      {
        type: 'SuiteStatement',
        name: 'Inner Suite',
        bodyStatements: [
          { type: 'VerifyStatement', label: 'inner-1', assertion: true, line: 2, column: 1 },
        ]
      },
    ]
  });
  assertEqual(outerResult.passed, 2, 'outer nested: both passed');
  assertEqual(outerResult.failed, 0, 'outer nested: none failed');
})();

// ═══════════════════════════════════════════════════════════════
//  TestRunner — runAll and Summary
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- TestRunner: runAll and Summary ---\u001b[0m');

(function() {
  const runner = new TestRunner();
  const suites = [
    { type: 'SuiteStatement', name: 'A', bodyStatements: [{ type: 'VerifyStatement', label: 'a1', assertion: true, line: 1 }] },
    { type: 'SuiteStatement', name: 'B', bodyStatements: [{ type: 'VerifyStatement', label: 'b1', assertion: false, line: 1 }] },
  ];
  const summary = runner.runAll(suites);
  assertEqual(summary.passed, 1, 'runAll: 1 passed');
  assertEqual(summary.failed, 1, 'runAll: 1 failed');
  assertEqual(summary.total, 2, 'runAll: 2 total');
  assertEqual(summary.suites.length, 2, 'runAll: 2 suites in summary');
  assertEqual(summary.suites[0].name, 'A', 'runAll: first suite name A');
  assertEqual(summary.suites[1].name, 'B', 'runAll: second suite name B');
})();

(function() {
  const runner = new TestRunner();
  assertEqual(runner.getExitCode(), 0, 'exit code 0 when no failures');
  runner._failed = 3;
  assertEqual(runner.getExitCode(), 1, 'exit code 1 when failures present');
})();

// ═══════════════════════════════════════════════════════════════
//  CodeWords + TestRunner Integration
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- CodeWords + TestRunner Integration ---\u001b[0m');

(function() {
  const source = '#ALLOW_HARVEST\nSUITE "Net",\n  VERIFY "true", TRUE\nSUITE/.';
  const dirs = CodeWordsChecker.parseDirectives(source);
  const checker = new CodeWordsChecker(dirs);
  const program = { type: 'Program', statements: [
    { type: 'SuiteStatement', name: 'Net', bodyStatements: [
      { type: 'VerifyStatement', label: 'true', assertion: true, line: 3 }
    ]}
  ]};
  const violations = checker.checkAST(program, 'test.plant');
  assertEqual(violations.length, 0, 'integration: no violations for clean source');
  assert(checker.hasDirective('#ALLOW_HARVEST'), 'integration: #ALLOW_HARVEST parsed');
})();

// ═══════════════════════════════════════════════════════════════
//  Valid Directives Enumeration
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- CodeWords: Valid Directives ---\u001b[0m');

(function() {
  const valid = CodeWordsChecker.getValidDirectives();
  assert(valid.includes('#ALLOW_NETWORK'), '#ALLOW_NETWORK is valid');
  assert(valid.includes('#ALLOW_HARVEST'), '#ALLOW_HARVEST is valid');
  assert(valid.includes('#ALLOW_LISTEN'), '#ALLOW_LISTEN is valid');
  assertEqual(valid.length, 3, 'exactly 3 valid directives');
})();

// ═══════════════════════════════════════════════════════════════
//  Print summary
// ═══════════════════════════════════════════════════════════════
console.log('');
console.log(`\u001b[1mResults: ${passed} passed, ${failed} failed\u001b[0m`);
if (failed > 0) process.exit(1);
}

main();
