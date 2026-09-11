'use strict';

/**
 * SafeChannel — Adaptive IPC pipeline between Main Thread and Worker Processes.
 *
 * Automated Transfer Strategy:
 *   - Small payloads (≤ 1MB): Structured Clone algorithm
 *   - Large payloads (> 1MB): Transferable Objects (ArrayBuffer) for O(1) zero-copy
 *   - Shared read-only state: SharedArrayBuffer for lookups/tensors
 *   - Continuous streams: ReadableStream / WritableStream for streaming data
 *
 * Emits [TRACE] logs identifying the active mechanism per send call.
 */
class SafeChannel {
  /**
   * @param {Object} [opts]
   * @param {Object} [opts.context]  MissionContext for trace/diagnostic logging
   */
  constructor(opts = {}) {
    this.context = opts.context || null;
    this._transferThreshold = 1024 * 1024; // 1 MB
  }

  /**
   * Serialize a payload for IPC transfer, selecting the optimal mechanism.
   *
   * @param {*} payload  The data to transfer
   * @returns {{ data: *, transferList: Array|null, mechanism: string }}
   */
  serialize(payload) {
    const size = this._estimateSize(payload);

    if (this._isStream(payload)) {
      this._trace(`SafeChannel streaming mode activated for large payload.`);
      return { data: payload, transferList: null, mechanism: 'stream' };
    }

    if (size > this._transferThreshold) {
      // Large payload: use Transferable Objects
      const ab = this._toArrayBuffer(payload);
      this._trace(`SafeChannel transferable mode activated for ${size} bytes payload.`);
      return { data: ab, transferList: [ab.buffer || ab], mechanism: 'transferable' };
    }

    if (this._isSharedBuffer(payload)) {
      this._trace(`SafeChannel shared buffer mode activated for read-only state.`);
      return { data: payload, transferList: null, mechanism: 'shared_buffer' };
    }

    // Small payload: Structured Clone
    this._trace(`SafeChannel structured clone mode activated for ${size} bytes payload.`);
    return { data: this._structuredClone(payload), transferList: null, mechanism: 'structured_clone' };
  }

  /**
   * Deserialize data received through the channel.
   *
   * @param {*} data  The raw received data
   * @param {string} mechanism  The mechanism used during serialization
   * @returns {*} Deserialized payload
   */
  deserialize(data, mechanism) {
    switch (mechanism) {
      case 'transferable':
        return this._fromArrayBuffer(data);
      case 'stream':
        return data; // Streams pass through
      case 'shared_buffer':
      case 'structured_clone':
      default:
        return data;
    }
  }

  /**
   * Estimate the byte size of a serializable payload.
   * @param {*} value
   * @returns {number}
   */
  _estimateSize(value) {
    if (value === null || value === undefined) return 0;
    if (typeof value === 'boolean' || typeof value === 'number') return 8;
    if (typeof value === 'string') return value.length * 2;
    if (value instanceof ArrayBuffer) return value.byteLength;
    if (value instanceof Uint8Array || value instanceof Int8Array) return value.byteLength;
    if (value instanceof Float64Array || value instanceof Float32Array) return value.byteLength;
    if (Array.isArray(value)) {
      return value.reduce((sum, v) => sum + this._estimateSize(v), 0);
    }
    if (typeof value === 'object') {
      let total = 0;
      for (const k of Object.keys(value)) {
        total += k.length * 2 + this._estimateSize(value[k]);
      }
      return total;
    }
    return 0;
  }

  /**
   * Convert a value to an ArrayBuffer for zero-copy transfer.
   * @param {*} value
   * @returns {ArrayBuffer|Uint8Array}
   */
  _toArrayBuffer(value) {
    if (value instanceof ArrayBuffer) return value;
    if (ArrayBuffer.isView(value)) {
      return value.buffer.slice(value.byteOffset, value.byteOffset + value.byteLength);
    }
    const json = JSON.stringify(value);
    const encoder = new TextEncoder();
    return encoder.encode(json).buffer;
  }

  /**
   * Convert an ArrayBuffer back to a value.
   * @param {ArrayBuffer} buffer
   * @returns {*}
   */
  _fromArrayBuffer(buffer) {
    try {
      const decoder = new TextDecoder();
      const text = decoder.decode(buffer);
      return JSON.parse(text);
    } catch (_) {
      return buffer;
    }
  }

  /**
   * Check if a value is a SharedArrayBuffer-based view.
   * @param {*} value
   * @returns {boolean}
   */
  _isSharedBuffer(value) {
    return value instanceof SharedArrayBuffer ||
      (value && value.constructor && value.constructor.name === 'SharedArrayBuffer') ||
      (value && value.buffer instanceof SharedArrayBuffer);
  }

  /**
   * Structured clone a value (deep copy).
   * @param {*} value
   * @returns {*}
   */
  _structuredClone(value) {
    if (typeof structuredClone === 'function') {
      return structuredClone(value);
    }
    // Fallback for older Node versions
    return JSON.parse(JSON.stringify(value));
  }

  /**
   * Check if a value is a readable or writable stream (Web API or Node.js).
   * @param {*} value
   * @returns {boolean}
   */
  _isStream(value) {
    if (!value || typeof value !== 'object') return false;
    const name = value.constructor ? value.constructor.name : '';
    return (
      value instanceof ReadableStream ||
      value instanceof WritableStream ||
      name === 'Readable' ||
      name === 'Writable' ||
      name === 'Transform' ||
      name === 'Duplex' ||
      (typeof value.pipe === 'function' && typeof value.on === 'function')
    );
  }

  /**
   * Emit a trace log if context supports it.
   * @param {string} msg
   */
  _trace(msg) {
    if (this.context && typeof this.context.trace === 'function') {
      this.context.trace(msg);
    } else if (typeof process !== 'undefined' && process.env.DEBUG) {
      console.log(`[TRACE] ${msg}`);
    }
  }
}

module.exports = { SafeChannel };
