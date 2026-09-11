'use strict';

const { CodeWordsChecker } = require('../security/codewords_governance');

class TestRunner {
  constructor(options = {}) {
    this._passed = 0;
    this._failed = 0;
    this._suites = [];
    this._results = [];
    this._verbose = options.verbose || false;
    this._codeWords = options.codeWords || null;
  }

  runSuite(suiteNode, context) {
    const suiteResult = {
      name: suiteNode.name || 'unnamed',
      passed: 0,
      failed: 0,
      assertions: [],
      startTime: Date.now(),
    };
    this._suites.push(suiteResult);
    if (this._verbose) console.error(`[test] SUITE: ${suiteResult.name}`);
    const body = suiteNode.bodyStatements || [];
    for (const stmt of body) {
      if (stmt.type === 'VerifyStatement') {
        this._runVerify(stmt, suiteResult, context);
      } else if (stmt.type === 'SuiteStatement') {
        const innerResult = this.runSuite(stmt, context);
        suiteResult.passed += innerResult.passed;
        suiteResult.failed += innerResult.failed;
      }
    }
    suiteResult.endTime = Date.now();
    suiteResult.elapsed = suiteResult.endTime - suiteResult.startTime;
    if (this._verbose) {
      console.error(`[test] SUITE "${suiteResult.name}": ${suiteResult.passed} passed, ${suiteResult.failed} failed (${suiteResult.elapsed}ms)`);
    }
    return suiteResult;
  }

  _runVerify(verifyNode, suiteResult, context) {
    const label = verifyNode.label || 'unnamed assertion';
    let passed = false;
    let error = null;
    try {
      const result = this._evaluateAssertion(verifyNode.assertion, context);
      passed = !!result;
    } catch (e) {
      error = e.message;
      passed = false;
    }
    const entry = { label, passed, error, line: verifyNode.line, column: verifyNode.column };
    suiteResult.assertions.push(entry);
    if (passed) {
      suiteResult.passed++;
      this._passed++;
      if (this._verbose) console.error(`  [test]   ✓ ${label}`);
    } else {
      suiteResult.failed++;
      this._failed++;
      if (this._verbose) console.error(`  [test]   ✗ ${label}${error ? ': ' + error : ''}`);
    }
  }

  _evaluateAssertion(assertion, context) {
    if (assertion === null || assertion === undefined) return false;
    if (typeof assertion === 'boolean') return assertion;
    if (typeof assertion === 'string') {
      const trimmed = assertion.trim();
      if (trimmed === 'true' || trimmed === 'TRUE') return true;
      if (trimmed === 'false' || trimmed === 'FALSE') return false;
      if (/^\d+$/.test(trimmed)) return parseInt(trimmed, 10) !== 0;
      if (context && context[trimmed] !== undefined) return !!context[trimmed];
      return trimmed.length > 0;
    }
    if (typeof assertion === 'number') return assertion !== 0;
    return true;
  }

  runAll(suites, context) {
    this._passed = 0;
    this._failed = 0;
    this._suites = [];
    this._results = [];
    for (const suite of suites) {
      this.runSuite(suite, context);
    }
    return this.getSummary();
  }

  getSummary() {
    return {
      passed: this._passed,
      failed: this._failed,
      total: this._passed + this._failed,
      suites: this._suites.map(s => ({
        name: s.name,
        passed: s.passed,
        failed: s.failed,
        total: s.passed + s.failed,
        elapsed: s.elapsed,
        assertions: s.assertions,
      })),
    };
  }

  printSummary() {
    const summary = this.getSummary();
    console.log('');
    console.log('═══════════════════════════════════════');
    console.log('  Test Runner Summary');
    console.log('═══════════════════════════════════════');
    for (const suite of summary.suites) {
      const status = suite.failed === 0 ? '✓' : '✗';
      console.log(`  ${status} "${suite.name}": ${suite.passed}/${suite.total} passed (${suite.elapsed}ms)`);
      for (const a of suite.assertions) {
        if (!a.passed) {
          console.log(`    ✗ ${a.label}${a.error ? ': ' + a.error : ''}`);
        }
      }
    }
    console.log('───────────────────────────────────────');
    console.log(`  Total: ${summary.passed}/${summary.total} passed${summary.failed > 0 ? ', ' + summary.failed + ' FAILED' : ''}`);
    console.log('═══════════════════════════════════════');
    return summary;
  }

  getExitCode() {
    return this._failed > 0 ? 1 : 0;
  }
}

module.exports = { TestRunner };
