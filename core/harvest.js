'use strict';
// HARVEST — synchronous-style HTTP/HTTPS client for PlantLang.
//
// PlantLang executes statements strictly in order with no async/await
// concept exposed to the language. To let `HARVEST url AS result.` behave
// like every other statement (run, get a value, move to next line), the
// actual network I/O is delegated to a worker thread that performs the
// real `fetch()` call, then writes its JSON-encoded result into a
// SharedArrayBuffer. The main thread blocks via Atomics.wait() directly on
// that buffer — which is safe and correct because Atomics.wait/notify are
// OS-level futex primitives that work across threads independently of
// either thread's JS event loop (unlike awaiting a worker 'message' event,
// which would deadlock if the main thread's loop is blocked).
const { Worker } = require('worker_threads');
const path = require('path');

const WORKER_PATH = path.join(__dirname, 'harvest_worker.js');
const DEFAULT_TIMEOUT_MS = 10000;
const PAYLOAD_BUFFER_BYTES = 2 * 1024 * 1024; // 2MB cap on response body

class HarvestError extends Error {
  constructor(message, code) {
    super(message);
    this.name = 'HarvestError';
    this.code = code || 'NETWORK_STORM';
  }
}

/**
 * Perform a blocking HTTP request from the main thread.
 * @param {string} url
 * @param {{method?:string, headers?:object, body?:any, timeoutMs?:number}} opts
 * @returns {{success:boolean, status?:number, ok?:boolean, data?:any, error?:string}}
 */
function harvestSync(url, opts = {}) {
  if (!/^https?:\/\//i.test(url)) {
    return { success: false, error: `Invalid URL (must start with http:// or https://): ${url}` };
  }

  const timeoutMs = Math.max(1000, Math.min(opts.timeoutMs || DEFAULT_TIMEOUT_MS, 30000));
  const sabBytes = 8 + PAYLOAD_BUFFER_BYTES;
  const sab = new SharedArrayBuffer(sabBytes);
  const flag = new Int32Array(sab, 0, 1);
  const lenView = new Int32Array(sab, 4, 1);
  const bytesView = new Uint8Array(sab, 8, sabBytes - 8);

  let worker;
  try {
    worker = new Worker(WORKER_PATH, {
      workerData: {
        url,
        method: (opts.method || 'GET').toUpperCase(),
        headers: opts.headers || {},
        body: opts.body,
        timeoutMs,
        sab,
        sabBytes
      }
    });
  } catch (e) {
    return { success: false, error: `Worker spawn failed: ${e.message}` };
  }
  worker.unref();
  worker.on('error', () => { /* surfaced via timeout/flag check below */ });

  const waitResult = Atomics.wait(flag, 0, 0, timeoutMs + 2000);
  try { worker.terminate(); } catch (_) {}

  if (waitResult === 'timed-out') {
    return { success: false, error: 'TIMEOUT' };
  }

  const len = Atomics.load(lenView, 0);
  if (len <= 0 || len > bytesView.length) {
    return { success: false, error: 'EMPTY_RESPONSE' };
  }

  const json = Buffer.from(bytesView.buffer, bytesView.byteOffset, len).toString('utf8');
  try {
    return JSON.parse(json);
  } catch (e) {
    return { success: false, error: `Response parse error: ${e.message}` };
  }
}

/**
 * Map a fetch result's `data` payload into a PlantLang-native value:
 * JSON objects -> plain JS object (interpreted as MAP)
 * JSON arrays  -> JS array (interpreted as LIST)
 * everything else (text, numbers-as-text, etc.) -> string (TX)
 */
function toPlantValue(data) {
  if (data === null || data === undefined) return '';
  if (Array.isArray(data)) return data.map(toPlantValue);
  if (typeof data === 'object') {
    const out = {};
    for (const k of Object.keys(data)) out[k] = toPlantValue(data[k]);
    return out;
  }
  if (typeof data === 'number' || typeof data === 'boolean') return data;
  return String(data);
}

module.exports = { harvestSync, toPlantValue, HarvestError };
