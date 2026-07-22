'use strict';

/**
 * MissionContext — Injects mission-aware allocator, IPC channel, and telemetry.
 *
 * Provides:
 *   - context.diagnostic(msg): Runtime escalation/warning logs
 *   - context.trace(msg): Verbose execution logging (only when --debug is passed)
 *   - context.getMetrics(): JSON metrics (memory, fragmentation, pool status, GC cycles)
 */
class MissionContext {
  /**
   * @param {Object} [opts]
   * @param {boolean} [opts.debug]  Enable verbose trace logging
   * @param {Object} [opts.allocator]  Active BumpAllocator instance
   * @param {Object} [opts.arcHeap]    Active GlobalARCHeap instance
   * @param {Object} [opts.processPool] Active WarmProcessPool instance
   * @param {Object} [opts.safeChannel] Active SafeChannel instance
   */
  constructor(opts = {}) {
    this._debug = opts.debug || false;
    this._allocator = opts.allocator || null;
    this._arcHeap = opts.arcHeap || null;
    this._processPool = opts.processPool || null;
    this._safeChannel = opts.safeChannel || null;
    this._diagnostics = [];
    this._traces = [];
    this._startTime = Date.now();
  }

  /**
   * Output a runtime escalation or warning diagnostic.
   * Always recorded regardless of debug flag.
   *
   * @param {string} msg
   */
  diagnostic(msg) {
    const entry = { timestamp: Date.now(), level: 'WARN', message: msg };
    this._diagnostics.push(entry);
    console.log(`[${entry.level}] ${msg}`);
  }

  /**
   * Output a verbose execution trace.
   * Only recorded when --debug is passed.
   *
   * @param {string} msg
   */
  trace(msg) {
    if (!this._debug) return;
    const entry = { timestamp: Date.now(), level: 'TRACE', message: msg };
    this._traces.push(entry);
    console.log(`[${entry.level}] ${msg}`);
  }

  /**
   * Return JSON metrics including memory usage, fragmentation percentage,
   * process pool status, and total GC cycle executions.
   *
   * @returns {Object}
   */
  getMetrics() {
    const allocatorMetrics = this._allocator ? {
      heapUsed: this._allocator.used,
      heapCapacity: this._allocator.capacity,
      heapRemaining: this._allocator.remaining,
      fragmentationPct: this._allocator.capacity > 0
        ? Number((1 - this._allocator.used / this._allocator.capacity) * 100).toFixed(2)
        : 0,
      escalated: this._allocator.escalated,
    } : null;

    const arcMetrics = this._arcHeap ? {
      liveObjects: this._arcHeap.size,
      gcCycles: this._arcHeap.gcCycles,
      totalAllocations: this._arcHeap.totalAllocations,
    } : null;

    const poolMetrics = this._processPool ? this._processPool.getMetrics() : null;

    return {
      uptimeMs: Date.now() - this._startTime,
      allocator: allocatorMetrics,
      arcHeap: arcMetrics,
      processPool: poolMetrics,
      diagnosticsCount: this._diagnostics.length,
      tracesCount: this._traces.length,
    };
  }

  /**
   * Bind a BumpAllocator to this context.
   * @param {Object} allocator
   */
  bindAllocator(allocator) {
    this._allocator = allocator;
    allocator.context = this;
  }

  /**
   * Bind a GlobalARCHeap to this context.
   * @param {Object} arcHeap
   */
  bindARCHeap(arcHeap) {
    this._arcHeap = arcHeap;
  }

  /**
   * Bind a WarmProcessPool to this context.
   * @param {Object} pool
   */
  bindProcessPool(pool) {
    this._processPool = pool;
    pool.context = this;
  }

  /**
   * Bind a SafeChannel to this context.
   * @param {Object} channel
   */
  bindSafeChannel(channel) {
    this._safeChannel = channel;
    channel.context = this;
  }

  /**
   * Clear all recorded diagnostics and traces.
   */
  clearLogs() {
    this._diagnostics = [];
    this._traces = [];
  }
}

module.exports = { MissionContext };
