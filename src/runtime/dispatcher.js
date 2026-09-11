'use strict';

const os = require('os');

/**
 * Runtime Dispatcher — Parallel compilation control & single-core auto-disable.
 *
 * Provides:
 *   - dispatcher.enableParallelCodegen(enabled): explicit parallel compilation toggle
 *   - Auto-disables on single-CPU systems (OS.cpus().length === 1)
 *   - Integration with ParallelCodegenEngine and NonBlockingTelemetry
 */
class RuntimeDispatcher {
  /**
   * @param {Object} [opts]
   * @param {Object} [opts.parallelEngine]  ParallelCodegenEngine instance
   * @param {Object} [opts.telemetry]       NonBlockingTelemetry instance
   */
  constructor(opts = {}) {
    this._parallelEngine = opts.parallelEngine || null;
    this._telemetry = opts.telemetry || null;
    this._parallelEnabled = true;
    this._autoDisabled = false;
    this._diagnostics = [];
    this._cpuCount = os.cpus().length;

    // Auto-disable on single-core systems
    if (this._cpuCount === 1) {
      this._parallelEnabled = false;
      this._autoDisabled = true;
      this._emitDiagnostic(
        'INFO: Single CPU core detected. Parallel codegen auto-disabled.'
      );
    }
  }

  /**
   * Enable or disable parallel code generation at runtime.
   * @param {boolean} enabled
   */
  enableParallelCodegen(enabled) {
    const prev = this._parallelEnabled;
    this._parallelEnabled = !!enabled;
    if (prev !== this._parallelEnabled) {
      const status = this._parallelEnabled ? 'enabled' : 'disabled';
      this._emitDiagnostic(`Parallel codegen ${status} via API.`);
    }
  }

  /**
   * Check whether parallel compilation is active.
   * @returns {boolean}
   */
  get isParallelEnabled() {
    return this._parallelEnabled && !this._autoDisabled;
  }

  /**
   * Whether auto-disable was triggered by single-core detection.
   * @returns {boolean}
   */
  get isAutoDisabled() {
    return this._autoDisabled;
  }

  /**
   * Get CPU core count.
   * @returns {number}
   */
  get cpuCount() {
    return this._cpuCount;
  }

  /**
   * Compile a program, dispatching to parallel or sequential engine.
   *
   * @param {Object} programNode  Parsed AST
   * @returns {Promise<Object>} Compilation result
   */
  async compile(programNode) {
    if (!this._parallelEngine || !this.isParallelEnabled) {
      // Sequential fallback
      return {
        chunks: ['; sequential compilation\n'],
        diagnostics: [
          this._autoDisabled
            ? 'Single-core auto-disable: using sequential compilation.'
            : 'Parallel disabled: using sequential compilation.',
        ],
        cycles: [],
        timing: { totalMs: 0 },
      };
    }

    if (this._telemetry) {
      const start = Date.now();
      const result = await this._parallelEngine.compile(programNode);
      this._telemetry.record('compile_ms', Date.now() - start);
      this._telemetry.record('compile_nodes', (programNode.statements || []).length);
      return result;
    }

    return this._parallelEngine.compile(programNode);
  }

  /**
   * Emit a diagnostic message.
   * @param {string} msg
   */
  _emitDiagnostic(msg) {
    this._diagnostics.push(msg);
    console.log(`[${msg.startsWith('INFO') ? 'INFO' : msg.startsWith('WARN') ? 'WARN' : 'DISPATCH'}] ${msg}`);
  }

  /**
   * Clear diagnostics.
   */
  clearDiagnostics() {
    this._diagnostics = [];
  }

  /**
   * Get accumulated diagnostics.
   * @returns {string[]}
   */
  get diagnostics() {
    return [...this._diagnostics];
  }
}

module.exports = { RuntimeDispatcher };
