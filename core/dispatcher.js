'use strict';
/**
 * core/dispatcher.js — Mission Dispatcher for the Five-Mission Architecture.
 *
 * Provides:
 *   - MissionStack: active execution context tracking during call trees
 *   - ScopedArena: temporally-scoped memory allocation tied to a scopeId
 *   - AdaptiveSMARTRouter: input-size-based execution path selection for SMART mode
 */

const { validateBoundary } = require('./matrix');

// ── MissionStack ──────────────────────────────────────────────────────────────

class MissionStack {
  /**
   * Maintain the active execution mission context as a stack.
   * Initialized with ['BALANCED'] per the architecture spec.
   */
  constructor() {
    this._stack = ['BALANCED'];
  }

  /**
   * Push a new mission mode onto the stack.
   * @param {string} mode
   */
  push(mode) {
    this._stack.push(mode.toUpperCase());
  }

  /**
   * Pop the most recent mission mode from the stack.
   * @returns {string} the popped mode
   * @throws {Error} if only the root BALANCED entry remains
   */
  pop() {
    if (this._stack.length <= 1) {
      throw new Error('Cannot pop the root BALANCED mission from the stack.');
    }
    return this._stack.pop();
  }

  /**
   * Peek at the current (top) mission mode.
   * @returns {string} current mode ('BALANCED' by default)
   */
  current() {
    return this._stack[this._stack.length - 1];
  }

  /**
   * Get the full stack depth.
   */
  get depth() {
    return this._stack.length;
  }

  /**
   * Create a snapshot of the current stack (for debugging/tracing).
   */
  snapshot() {
    return [...this._stack];
  }
}

// ── ScopedArena ───────────────────────────────────────────────────────────────

class ScopedArena {
  /**
   * Manage temporary allocation blocks tied to a scopeId.
   * Each ScopedArena is a contiguous bump-allocated buffer.
   *
   * @param {number} scopeId   Unique scope identifier
   * @param {number} capacity  Total buffer size in bytes (default 65536 = 64KB)
   */
  constructor(scopeId, capacity = 65536) {
    this.scopeId = scopeId;
    this.capacity = capacity;
    this._buffer = Buffer.alloc(capacity, 0);
    this._offset = 0;
  }

  /**
   * Reserve contiguous buffer space.
   * @param {number} size  Number of bytes to allocate
   * @returns {number}     Byte offset from the start of the buffer
   * @throws {Error}       If allocation exceeds capacity
   */
  alloc(size) {
    if (this._offset + size > this.capacity) {
      throw new Error(
        `ScopedArena ${this.scopeId}: allocation of ${size} bytes exceeds capacity (${this.capacity} bytes at offset ${this._offset}).`
      );
    }
    const ptr = this._offset;
    this._offset += size;
    return ptr;
  }

  /**
   * Write bytes at a given offset.
   * @param {number} offset  Offset from the start of the buffer
   * @param {Buffer|Uint8Array} data  Data to write
   */
  write(offset, data) {
    if (offset + data.length > this.capacity) {
      throw new Error(`ScopedArena ${this.scopeId}: write ${data.length} bytes at offset ${offset} exceeds capacity.`);
    }
    for (let i = 0; i < data.length; i++) {
      this._buffer[offset + i] = data[i];
    }
  }

  /**
   * Read bytes from a given offset.
   * @param {number} offset  Offset from the start of the buffer
   * @param {number} length  Number of bytes to read
   * @returns {Buffer}       Slice of the underlying buffer
   */
  read(offset, length) {
    if (offset + length > this.capacity) {
      throw new Error(`ScopedArena ${this.scopeId}: read ${length} bytes at offset ${offset} exceeds capacity.`);
    }
    return this._buffer.slice(offset, offset + length);
  }

  /**
   * Reset the bump pointer to zero — instantly reclaims all memory
   * without GC (matches PlantLang's Rooted Depth System semantics).
   */
  reset() {
    this._offset = 0;
  }

  /**
   * Current usage in bytes.
   */
  get used() {
    return this._offset;
  }

  /**
   * Remaining capacity in bytes.
   */
  get remaining() {
    return this.capacity - this._offset;
  }
}

// ── AdaptiveSMARTRouter ───────────────────────────────────────────────────────

const SMART_SCALAR_THRESHOLD = 1000;

/**
 * Execute a function in scalar (inline) mode.
 * Used by SMART when input size N < 1000.
 *
 * @param {Function} actionFn  The action to execute
 * @param {Array} data         Input data
 * @returns {*}                Result of the action
 */
function executeScalarInline(actionFn, data) {
  return actionFn(data);
}

/**
 * Execute a function in parallel vector mode.
 * Used by SMART when input size N >= 1000.
 * In the JS runtime this uses chunked processing to simulate
 * vectorized execution.
 *
 * @param {Function} actionFn  The action to execute
 * @param {Array} data         Input data
 * @returns {Array}            Array of results per element
 */
function executeParallelVector(actionFn, data) {
  const chunkSize = Math.max(1, Math.floor(SMART_SCALAR_THRESHOLD / 10));
  const results = [];
  for (let i = 0; i < data.length; i += chunkSize) {
    const chunk = data.slice(i, i + chunkSize);
    const chunkResults = chunk.map(item => actionFn(item));
    results.push(...chunkResults);
  }
  return results;
}

/**
 * Adaptive SMART path router: selects execution strategy based on input size.
 *
 * @param {Function} actionFn  The action/function to execute
 * @param {Array|*} data       Input data (if array, size determines routing)
 * @returns {*}                Result (scalar for N<1000, array for N>=1000)
 */
function routeSMART(actionFn, data) {
  if (Array.isArray(data) && data.length >= SMART_SCALAR_THRESHOLD) {
    return executeParallelVector(actionFn, data);
  }
  return executeScalarInline(actionFn, data);
}

// ── MissionDispatcher ─────────────────────────────────────────────────────────

class MissionDispatcher {
  /**
   * Central dispatch controller that orchestrates boundary validation,
   * mission stack tracking, and SMART routing.
   *
   * @param {Object} [opts]
   * @param {MissionStack} [opts.missionStack]
   */
  constructor(opts = {}) {
    this.missionStack = opts.missionStack || new MissionStack();
    this._arenas = new Map(); // scopeId → ScopedArena
  }

  /**
   * Perform a cross-mission function call.
   *
   * @param {string} fromMode  Caller's mission mode
   * @param {string} toMode    Callee's mission mode
   * @param {Function} fn      The function to execute
   * @param {Array|*} data     The input data
   * @param {Object} [context] Optional context { scopeId, lineContext }
   * @returns {*}              The function's result
   */
  dispatch(fromMode, toMode, fn, data, context = {}) {
    // 1. Validate boundary (throws BoundaryViolationError on DENY)
    validateBoundary(fromMode, toMode, context);

    // 2. Push callee's mode onto the mission stack
    this.missionStack.push(toMode);

    try {
      // 3. Execute — use SMART routing if target mode is SMART
      if (toMode.toUpperCase() === 'SMART') {
        return routeSMART(fn, data);
      }
      return fn(data);
    } finally {
      // 4. Pop the callee mode
      this.missionStack.pop();
    }
  }

  /**
   * Get or create a ScopedArena for the given scopeId.
   * @param {number} scopeId
   * @param {number} [capacity]
   * @returns {ScopedArena}
   */
  getArena(scopeId, capacity) {
    if (!this._arenas.has(scopeId)) {
      this._arenas.set(scopeId, new ScopedArena(scopeId, capacity));
    }
    return this._arenas.get(scopeId);
  }

  /**
   * Reset all tracked arenas.
   */
  resetAllArenas() {
    for (const arena of this._arenas.values()) {
      arena.reset();
    }
  }

  /**
   * Reset a specific arena by scopeId.
   * @param {number} scopeId
   */
  resetArena(scopeId) {
    const arena = this._arenas.get(scopeId);
    if (arena) arena.reset();
  }
}

module.exports = {
  MissionStack,
  ScopedArena,
  MissionDispatcher,
  routeSMART,
  executeScalarInline,
  executeParallelVector,
  SMART_SCALAR_THRESHOLD,
};
