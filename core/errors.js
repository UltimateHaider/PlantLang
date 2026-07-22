'use strict';
/**
 * core/errors.js — PlantLang Error Hierarchy for the Five-Mission Architecture.
 *
 * Defines BoundaryViolationError with context-rich properties for the
 * 5x5 Boundary Handshake Matrix enforcement.
 */

class BoundaryViolationError extends Error {
  /**
   * @param {string} fromMode  The caller's mission mode
   * @param {string} toMode    The callee's mission mode
   * @param {number|null} scopeId  The scope identifier (if available)
   * @param {string} lineContext  Source line or description for context
   * @param {string} message   Pre-formatted message (if not provided, auto-generated)
   */
  constructor(fromMode, toMode, scopeId = null, lineContext = '', message = null) {
    const msg = message || BoundaryViolationError.defaultMessage(fromMode, toMode);
    super(msg);
    this.name = 'BoundaryViolationError';
    this.fromMode = fromMode;
    this.toMode = toMode;
    this.scopeId = scopeId;
    this.lineContext = lineContext;
  }

  /**
   * Return the exact standard diagnostic error message for a given
   * (fromMode, toMode) violation, as specified in the architecture docs.
   */
  static defaultMessage(fromMode, toMode) {
    const fm = fromMode.toUpperCase();
    const tm = toMode.toUpperCase();
    if (fm === 'SAFE' && tm === 'FAST') {
      return '[BoundaryViolationError] SAFE -> FAST: SAFE is isolated and cannot invoke unguarded FAST code.';
    }
    if (fm === 'SAFE' && tm === 'PERSISTENT') {
      return '[BoundaryViolationError] SAFE -> PERSISTENT: SAFE cannot create persistent objects that outlive the isolated scope.';
    }
    if (fm === 'FAST' && tm === 'SAFE') {
      return '[BoundaryViolationError] FAST -> SAFE: FAST cannot invoke SAFE due to conflicting performance/safety requirements.';
    }
    if (fm === 'SAFE' && tm === 'SMART') {
      return '[BoundaryViolationError] SAFE -> SMART: SAFE cannot invoke SMART as it may dynamically route to FAST.';
    }
    return `[BoundaryViolationError] ${fromMode} -> ${toMode}: Boundary violation between incompatible mission modes.`;
  }
}

module.exports = { BoundaryViolationError };
