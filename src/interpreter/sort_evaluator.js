'use strict';

// ═══════════════════════════════════════════════════════════════
//  src/interpreter/sort_evaluator.js
//  v0.38.0 — Multi-field & Type-Aware SORT Engine
//  Supports multi-field chained comparison with ASC/DESC,
//  simple single-direction sort, null-safe comparison.
// ═══════════════════════════════════════════════════════════════

/**
 * Evaluate a SortStatementV2Node (multi-field sort) or legacy SortStatementNode.
 *
 * Grammar (v2): SORT list_var [BY field1 [ASC|DESC], field2 [ASC|DESC], ...]
 * Grammar (v1): SORT list_var [ASC|DESC]
 *
 * @param {object} node        - SortStatementNode or SortStatementV2Node
 * @param {object} interpreter - Interpreter instance
 * @param {object} soil        - current Soil scope
 * @returns {null}
 */
function evaluateSortStatement(node, interpreter, soil) {
  // Determine list variable name either from listExpr or listIdent
  let listName;
  if (node.listExpr) {
    // v2: listExpr could be a string (variable name) or expression
    listName = typeof node.listExpr === 'string' ? node.listExpr : null;
  } else if (node.listIdent) {
    // v1 legacy
    listName = node.listIdent;
  }

  if (!listName) {
    // Try evaluating as expression
    const listVal = interpreter.evaluateExpressionNode(node.listExpr || node.listIdent, soil);
    if (Array.isArray(listVal)) {
      const arr = listVal;
      const direction = _resolveDirection(node);
      arr.sort(_makeSimpleComparator(direction));
    }
    return null;
  }

  const raw = soil.get(listName);
  if (!raw) return null;

  // Get the actual array
  let arr = (raw && raw.value !== undefined) ? raw.value : raw;
  if (!Array.isArray(arr)) return null;

  // For v2 with explicit fields, do multi-field sort
  if (node.fields && node.fields.length > 0) {
    arr.sort(_makeChainedComparator(node.fields));
  } else {
    // Simple single-direction sort
    const direction = _resolveDirection(node);
    arr.sort(_makeSimpleComparator(direction));
  }

  // If soil stores by reference, the array is sorted in-place
  return null;
}

/**
 * Resolve sort direction from node fields or direction property.
 */
function _resolveDirection(node) {
  if (node.fields && node.fields.length > 0) return node.fields[0].direction || 'ASC';
  return (node.direction || 'ASC').toUpperCase() === 'DESC' ? 'DESC' : 'ASC';
}

/**
 * Create a simple comparator for primitive arrays.
 */
function _makeSimpleComparator(direction) {
  const desc = direction === 'DESC' ? -1 : 1;
  return function(a, b) {
    if (a == null && b == null) return 0;
    if (a == null) return 1;   // nulls to end
    if (b == null) return -1;
    if (typeof a === 'string' && typeof b === 'string') {
      return desc * a.localeCompare(b);
    }
    if (typeof a === 'number' && typeof b === 'number') {
      return desc * (a - b);
    }
    // Fallback: string comparison
    const sa = String(a), sb = String(b);
    return desc * sa.localeCompare(sb);
  };
}

/**
 * Create a chained comparator for multi-field sort.
 * Each field spec: { field: "fieldName", direction: "ASC"|"DESC" }
 * Compares fields sequentially — if field N is equal, proceeds to N+1.
 */
function _makeChainedComparator(fields) {
  return function(a, b) {
    for (const spec of fields) {
      const dir = (spec.direction || 'ASC').toUpperCase() === 'DESC' ? -1 : 1;
      const va = a != null ? a[spec.field] : undefined;
      const vb = b != null ? b[spec.field] : undefined;

      // Null/undefined sort to end regardless of direction
      if (va == null && vb == null) continue;
      if (va == null) return 1;
      if (vb == null) return -1;

      let cmp;
      if (typeof va === 'string' && typeof vb === 'string') {
        cmp = va.localeCompare(vb);
      } else if (typeof va === 'number' && typeof vb === 'number') {
        cmp = va - vb;
      } else {
        cmp = String(va).localeCompare(String(vb));
      }

      if (cmp !== 0) return dir * cmp;
    }
    return 0;
  };
}

module.exports = { evaluateSortStatement, _makeChainedComparator, _makeSimpleComparator };
