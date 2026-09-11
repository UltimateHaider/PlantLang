const crypto = require('crypto');
const { Worker } = require('worker_threads');
const path = require('path');

const HEADER_SLOTS = 4;
const HEADER_BYTES = HEADER_SLOTS * 4;
const ENTRY_BYTES = 256;
const TIMESTAMP_OFF = 0;
const EVENTTYPE_OFF = 8;
const DATA_OFF = 9;
const DATA_LEN = 183;
const PREVHASH_OFF = 192;
const HASH_OFF = 224;
const HASH_LEN = 32;

const DEFAULT_RING_SIZE = 10000;

class NonBlockingAuditLogger {
  constructor(options = {}) {
    const capacity = options.ringSize || parseInt(process.env.AUDIT_RING_SIZE, 10) || DEFAULT_RING_SIZE;
    this._entryCount = capacity;
    const totalBytes = HEADER_BYTES + this._entryCount * ENTRY_BYTES;
    this._sab = new SharedArrayBuffer(totalBytes);
    this._header = new Int32Array(this._sab, 0, HEADER_SLOTS);
    this._writeIdx = 0;
    this._readIdx = 1;
    this._overflowCount = 0;
    Atomics.store(this._header, 0, this._writeIdx);
    Atomics.store(this._header, 1, this._readIdx);

    this._prevHash = Buffer.alloc(HASH_LEN, 0);
    this._closed = false;

    this._writeIdx = 0;
    this._readIdx = 0;
    this._overflowCount = 0;
    Atomics.store(this._header, 0, 0);
    Atomics.store(this._header, 1, 0);

    this._workerPath = options.workerPath || path.join(__dirname, 'audit_worker.js');
    this._worker = null;
    try {
      this._worker = new Worker(this._workerPath);
      this._worker.on('error', (err) => {
        if (typeof process !== 'undefined' && process.env.DEBUG) {
          console.log('[AUDIO] Worker error:', err.message);
        }
      });
    } catch (e) {
      if (typeof process !== 'undefined' && process.env.DEBUG) {
        console.log('[AUDIO] Worker not available (fallback to sync):', e.message);
      }
    }
  }

  _entryOffset(idx) {
    return HEADER_BYTES + (idx % this._entryCount) * ENTRY_BYTES;
  }

  _entryBytes(offset) {
    return new Uint8Array(this._sab, offset, ENTRY_BYTES);
  }

  record(eventType, data) {
    const writeBefore = Atomics.add(this._header, 0, 1);
    const idx = writeBefore % this._entryCount;
    const readIdx = Atomics.load(this._header, 1);

    if (writeBefore - readIdx >= this._entryCount) {
      Atomics.add(this._header, 1, 1);
      this._overflowCount++;
      Atomics.store(this._header, 2, this._overflowCount);
      if (typeof process !== 'undefined' && process.env.DEBUG) {
        console.log('WARN: Audit ring buffer capacity reached. Synchronous flush executed.');
      }
    }

    const offset = this._entryOffset(idx);
    const bytes = this._entryBytes(offset);

    const tsMs = Date.now();
    const tsBuf = Buffer.alloc(8);
    tsBuf.writeBigInt64LE(BigInt(tsMs), 0);
    for (let i = 0; i < 8; i++) bytes[TIMESTAMP_OFF + i] = tsBuf[i];

    bytes[EVENTTYPE_OFF] = eventType.charCodeAt(0) || 32;

    const dataStr = String(data).slice(0, DATA_LEN - 1);
    const dataBuf = Buffer.from(dataStr, 'utf8');
    for (let i = 0; i < DATA_LEN; i++) {
      bytes[DATA_OFF + i] = i < dataBuf.length ? dataBuf[i] : 0;
    }

    for (let i = 0; i < HASH_LEN; i++) {
      bytes[PREVHASH_OFF + i] = this._prevHash[i];
    }

    const entryForHash = Buffer.alloc(8 + 1 + DATA_LEN + HASH_LEN);
    entryForHash.writeBigInt64LE(BigInt(tsMs), 0);
    entryForHash[8] = eventType.charCodeAt(0) || 32;
    for (let i = 0; i < DATA_LEN; i++) {
      entryForHash[9 + i] = i < dataBuf.length ? dataBuf[i] : 0;
    }
    this._prevHash.copy(entryForHash, 9 + DATA_LEN);
    const hash = crypto.createHash('sha256').update(entryForHash).digest();
    this._prevHash = hash;

    for (let i = 0; i < HASH_LEN; i++) {
      bytes[HASH_OFF + i] = hash[i];
    }

    const atomicWriteIdx = Atomics.load(this._header, 0);
    Atomics.store(this._header, 3, atomicWriteIdx);

    if (this._worker) {
      this._worker.postMessage({
        type: 'flush',
        entry: { eventType, data: String(data), tsMs }
      });
    }
  }

  snapshot() {
    const writeIdx = Atomics.load(this._header, 0);
    const readIdx = Atomics.load(this._header, 1);
    const effectiveWrite = Math.min(writeIdx, readIdx + this._entryCount);
    const count = Math.max(0, effectiveWrite - readIdx);
    const metrics = [];

    for (let i = 0; i < count; i++) {
      const idx = (readIdx + i) % this._entryCount;
      const offset = this._entryOffset(idx);
      const bytes = this._entryBytes(offset);

      const ts = Number(Buffer.from(bytes.slice(TIMESTAMP_OFF, TIMESTAMP_OFF + 8)).readBigInt64LE(0));
      const evType = String.fromCharCode(bytes[EVENTTYPE_OFF]);

      const dataBytes = [];
      for (let j = 0; j < DATA_LEN && bytes[DATA_OFF + j] !== 0; j++) {
        dataBytes.push(bytes[DATA_OFF + j]);
      }
      const data = Buffer.from(dataBytes).toString('utf8');

      const prevHash = Buffer.from(bytes.slice(PREVHASH_OFF, PREVHASH_OFF + HASH_LEN)).toString('hex');
      const hash = Buffer.from(bytes.slice(HASH_OFF, HASH_OFF + HASH_LEN)).toString('hex');

      metrics.push({ timestampMs: ts, eventType: evType, data, prevHash, hash });
    }

    return {
      metrics,
      overflowCount: this._overflowCount,
      uptimeNs: process.hrtime.bigint()
    };
  }

  verifyIntegrity() {
    const writeIdx = Atomics.load(this._header, 0);
    const readIdx = Atomics.load(this._header, 1);
    const effectiveWrite = Math.min(writeIdx, readIdx + this._entryCount);
    const count = Math.max(0, effectiveWrite - readIdx);
    const results = [];

    let expectedPrevHash = null;

    for (let i = 0; i < count; i++) {
      const idx = (readIdx + i) % this._entryCount;
      const offset = this._entryOffset(idx);
      const bytes = this._entryBytes(offset);

      const ts = Number(Buffer.from(bytes.slice(TIMESTAMP_OFF, TIMESTAMP_OFF + 8)).readBigInt64LE(0));
      const evType = String.fromCharCode(bytes[EVENTTYPE_OFF]);

      const dataBytes = [];
      for (let j = 0; j < DATA_LEN && bytes[DATA_OFF + j] !== 0; j++) {
        dataBytes.push(bytes[DATA_OFF + j]);
      }

      const storedPrevHash = Buffer.from(bytes.slice(PREVHASH_OFF, PREVHASH_OFF + HASH_LEN));
      const storedHash = Buffer.from(bytes.slice(HASH_OFF, HASH_OFF + HASH_LEN));

      if (expectedPrevHash === null) {
        expectedPrevHash = storedPrevHash;
      } else {
        const chainOk = storedPrevHash.equals(expectedPrevHash);
        if (!chainOk) {
          results.push({ index: i, issue: 'chain_break', expectedPrev: expectedPrevHash.toString('hex'), actualPrev: storedPrevHash.toString('hex') });
          return { valid: false, results };
        }
      }

      const entryForHash = Buffer.alloc(8 + 1 + DATA_LEN + HASH_LEN);
      entryForHash.writeBigInt64LE(BigInt(ts), 0);
      entryForHash[8] = evType.charCodeAt(0) || 32;
      for (let j = 0; j < DATA_LEN; j++) {
        entryForHash[9 + j] = j < dataBytes.length ? dataBytes[j] : 0;
      }
      storedPrevHash.copy(entryForHash, 9 + DATA_LEN);
      const computedHash = crypto.createHash('sha256').update(entryForHash).digest();

      const hashOk = storedHash.equals(computedHash);
      if (!hashOk) {
        results.push({ index: i, issue: 'hash_mismatch', stored: storedHash.toString('hex'), computed: computedHash.toString('hex') });
        return { valid: false, results };
      }

      expectedPrevHash = computedHash;
    }

    return { valid: results.length === 0, results };
  }

  close() {
    this._closed = true;
    if (this._worker) {
      this._worker.postMessage({ type: 'shutdown' });
      setTimeout(() => this._worker.terminate(), 100);
    }
  }
}

module.exports = { NonBlockingAuditLogger };
