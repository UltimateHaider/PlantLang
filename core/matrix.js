'use strict';
/**
 * core/matrix.js — 5x5 Boundary Handshake Matrix for the Five-Mission Architecture.
 *
 * Defines the mission-mode transition permission matrix and provides
 * `validateBoundary()` for both static (typechecker) and dynamic (dispatcher) checks.
 */

const { BoundaryViolationError } = require('./errors');

// All mission modes in canonical order
const MODES = ['BALANCED', 'FAST', 'SAFE', 'SMART', 'PERSISTENT'];

/**
 * The 5x5 permission matrix.
 * ALLOW = true, DENY = false.
 *
 * Rows   = fromMode (caller)
 * Columns = toMode (callee)
 */
const PERMISSION_MATRIX = {
  BALANCED:   { BALANCED: true, FAST: true, SAFE: true, SMART: true, PERSISTENT: true },
  FAST:       { BALANCED: true, FAST: true, SAFE: false, SMART: true, PERSISTENT: true },
  SAFE:       { BALANCED: true, FAST: false, SAFE: true, SMART: false, PERSISTENT: false },
  SMART:      { BALANCED: true, FAST: true, SAFE: true, SMART: true, PERSISTENT: true },
  PERSISTENT: { BALANCED: true, FAST: true, SAFE: false, SMART: true, PERSISTENT: true },
};

/**
 * Check whether a cross-mode call is permitted.
 *
 * @param {string} fromMode  The caller's mission mode.
 * @param {string} toMode    The callee's mission mode.
 * @param {Object} [context] Optional context with { scopeId, lineContext }.
 * @returns {boolean} true if the call is allowed.
 * @throws {BoundaryViolationError} if the call is forbidden.
 */
function validateBoundary(fromMode, toMode, context = {}) {
  const fm = fromMode.toUpperCase();
  const tm = toMode.toUpperCase();
  const { scopeId = null, lineContext = '' } = context;

  // Validate that both modes are known
  if (!MODES.includes(fm)) {
    throw new Error(`Unknown mission mode: "${fromMode}". Valid modes: ${MODES.join(', ')}`);
  }
  if (!MODES.includes(tm)) {
    throw new Error(`Unknown mission mode: "${toMode}". Valid modes: ${MODES.join(', ')}`);
  }

  const allowed = PERMISSION_MATRIX[fm][tm];
  if (!allowed) {
    const msg = BoundaryViolationError.defaultMessage(fm, tm);
    throw new BoundaryViolationError(fm, tm, scopeId, lineContext, msg);
  }
  return true;
}

/**
 * Return the full matrix as a readable table (for documentation).
 */
function formatMatrix() {
  const header = `| From \\ To | ${MODES.join(' | ')} |`;
  const sep = `|${MODES.map(() => '---').join('|')}|`;
  const rows = MODES.map(fm => {
    const cells = MODES.map(tm => PERMISSION_MATRIX[fm][tm] ? 'ALLOW' : '**DENY**');
    return `| **${fm}** | ${cells.join(' | ')} |`;
  });
  return [header, sep, ...rows].join('\n');
}

module.exports = { MODES, PERMISSION_MATRIX, validateBoundary, formatMatrix };
