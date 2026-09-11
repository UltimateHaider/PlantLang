'use strict';

const DEFAULT_CAPACITY = 8 * 1024 * 1024; // 8 MB
const HARD_CAP = 64 * 1024 * 1024;         // 64 MB
const ALIGN_MASK = 7;                       // 8-byte alignment

/**
 * BumpAllocator — O(1) linear bump allocator for FAST mission mode.
 *
 * - Strict 8-byte alignment
 * - Configurable capacity (default 8MB, hard cap 64MB)
 * - O(1) reset: ptr = 0 on scope exit (no compaction or free)
 * - Graceful escalation: if allocation exceeds capacity, emits diagnostic
 *   and signals BALANCED fallback instead of throwing.
 */
class BumpAllocator {
  /**
   * @param {Object} [opts]
   * @param {number} [opts.capacity]  Total buffer size in bytes (default 8MB, max 64MB)
   * @param {Object} [opts.context]   MissionContext instance for diagnostics
   */
  constructor(opts = {}) {
    const requested = opts.capacity || DEFAULT_CAPACITY;
    this.capacity = Math.min(Math.max(1, requested), HARD_CAP);
    this._buffer = Buffer.alloc(this.capacity, 0);
    this._offset = 0;
    this.context = opts.context || null;
    this._escalated = false;
  }

  /**
   * Align a size up to the nearest 8-byte boundary.
   * @param {number} size
   * @returns {number}
   */
  static align(size) {
    return (size + ALIGN_MASK) & ~ALIGN_MASK;
  }

  /**
   * Allocate a block of memory.
   *
   * @param {number} size  Requested bytes (will be 8-byte aligned)
   * @returns {{ ptr: number, buffer: Buffer, escalated: boolean }}
   *          Returns an object with the offset, the underlying buffer,
   *          and an escalated flag (true if BALANCED fallback was triggered).
   * @throws {Error} If size is negative or allocation exceeds hard cap
   */
  alloc(size) {
    if (size < 0) {
      throw new Error(`BumpAllocator: negative allocation size ${size}`);
    }
    const aligned = BumpAllocator.align(size);
    const escalated = this._tryEscalate(aligned);
    if (escalated) {
      return { ptr: -1, buffer: null, escalated: true };
    }
    const ptr = this._offset;
    this._offset += aligned;
    return { ptr, buffer: this._buffer, escalated: false };
  }

  /**
   * Check if allocation fits; if not, emit escalation and return true.
   * @param {number} alignedSize
   * @returns {boolean} true if escalated to BALANCED
   */
  _tryEscalate(alignedSize) {
    if (this._offset + alignedSize <= this.capacity) return false;
    if (this._escalated) return true;
    this._escalated = true;
    if (this.context && typeof this.context.diagnostic === 'function') {
      this.context.diagnostic('WARN: Fast heap capacity exceeded. Escalated to BALANCED.');
    }
    return true;
  }

  /**
   * O(1) reset — zero the bump pointer. All prior allocations are invalidated.
   */
  reset() {
    this._offset = 0;
    this._escalated = false;
  }

  /**
   * Current usage in bytes.
   * @returns {number}
   */
  get used() {
    return this._offset;
  }

  /**
   * Remaining capacity in bytes.
   * @returns {number}
   */
  get remaining() {
    return this.capacity - this._offset;
  }

  /**
   * Whether the allocator has escalated to BALANCED.
   * @returns {boolean}
   */
  get escalated() {
    return this._escalated;
  }
}

module.exports = { BumpAllocator, DEFAULT_CAPACITY, HARD_CAP };
