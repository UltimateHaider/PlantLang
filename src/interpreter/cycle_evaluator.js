'use strict';

// ═══════════════════════════════════════════════════════════════
//  src/interpreter/cycle_evaluator.js
//  v0.38.0 — CYCLE ... IN list Loop Control Engine
//  Supports per-iteration scope isolation, BREAK, CONTINUE,
//  optional index variable, and empty/null/undefined safety.
// ═══════════════════════════════════════════════════════════════

const { BreakSignalException, ContinueSignalException } = require('../../core/ast');

/**
 * Evaluate a CycleInStatementNode.
 *
 * Grammar: CYCLE item_var [, index_var] IN list_expr { body_statements }
 *
 * @param {object} node        - CycleInStatementNode instance
 * @param {object} interpreter - Interpreter instance (for evaluateNode, evaluateExpressionNode, etc.)
 * @param {object} soil        - current Soil scope
 * @returns {object|null}      - result object or null
 */
function evaluateCycleInStatement(node, interpreter, soil) {
  const listVal = interpreter.evaluateExpressionNode(node.listExpr, soil);

  // Edge-case safety: empty array, null, or undefined → graceful exit
  if (!listVal || (Array.isArray(listVal) && listVal.length === 0)) {
    return null;
  }

  // Ensure we have an iterable
  const items = Array.isArray(listVal) ? listVal : Object.values(listVal);
  if (items.length === 0) return null;

  for (let i = 0; i < items.length; i++) {
    // Per-iteration scope isolation: create child scope
    const iterSoil = soil.child();

    // Bind iteration variable
    iterSoil.set(node.iterVar, items[i]);

    // Bind optional index variable
    if (node.indexVar) {
      iterSoil.set(node.indexVar, i);
    }

    try {
      // Evaluate body statements
      const r = interpreter._evalBody(node.bodyStatements, iterSoil);
      if (r && r.returned) {
        // GIVE/return detected — propagate upward
        return r;
      }
    } catch (err) {
      if (err instanceof BreakSignalException) {
        // BREAK: exit loop immediately (scope already isolated, auto-destroyed)
        break;
      }
      if (err instanceof ContinueSignalException) {
        // CONTINUE: skip to next iteration (scope isolated, auto-collected)
        continue;
      }
      throw err; // re-throw unexpected errors
    }
    // Child scope iterSoil is eligible for GC after each iteration
  }

  return null;
}

module.exports = { evaluateCycleInStatement };
