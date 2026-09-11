'use strict';
// HARVEST — synchronous HTTP/HTTPS using Worker + SharedArrayBuffer.
// Main thread calls harvestSync(), which:
//   1. Spawns a worker thread
//   2. Worker does the HTTP request, writes JSON result at offset 8 in SAB
//   3. Main thread Atomics.wait() on ctrl[0] until worker sets it to 1
// This is deadlock-free because Atomics.wait is OS-level, not event-loop-level.

const { Worker } = require('worker_threads');
const path = require('path');

const WORKER_PATH    = path.join(__dirname, 'harvest_worker.js');
const BUFFER_BYTES   = 4 * 1024 * 1024; // 4MB
const DATA_OFFSET    = 8;                // first 8 bytes = ctrl(int32) + padding

function harvestSync(url, opts = {}) {
  const timeoutMs = Math.max(500, Math.min(opts.timeoutMs || 10000, 30000));
  const sab = new SharedArrayBuffer(DATA_OFFSET + BUFFER_BYTES);
  const ctrl = new Int32Array(sab, 0, 1);  // ctrl[0] = done flag

  let worker;
  try {
    worker = new Worker(WORKER_PATH, {
      workerData: {
        sab,
        url,
        method   : (opts.method  || 'GET').toUpperCase(),
        bodyStr  : opts.body != null ? (typeof opts.body === 'string' ? opts.body : JSON.stringify(opts.body)) : null,
        headers  : opts.headers || {},
        timeout  : timeoutMs,
      }
    });
  } catch (e) {
    return { ok: false, error: `Worker spawn failed: ${e.message}` };
  }

  worker.unref();
  worker.on('error', () => {});  // errors surfaced via result.error field

  const waited = Atomics.wait(ctrl, 0, 0, timeoutMs + 3000);
  try { worker.terminate(); } catch (_) {}

  if (waited === 'timed-out') {
    return { ok: false, error: 'Request timed out', code: 'ETIMEDOUT' };
  }

  // Read JSON result from SAB
  const dataBuf = Buffer.from(sab, DATA_OFFSET, BUFFER_BYTES);
  const nullIdx = dataBuf.indexOf(0);
  const raw = dataBuf.slice(0, nullIdx > 0 ? nullIdx : BUFFER_BYTES).toString('utf8').trim();

  if (!raw) return { ok: false, error: 'Empty response from worker' };

  try {
    return JSON.parse(raw);
  } catch (e) {
    return { ok: false, error: `JSON parse error: ${e.message}`, raw };
  }
}

// Convert parsed JSON to PlantLang-native types (for MAP/LIST assignments)
function toPlantValue(v) {
  if (v === null || v === undefined) return '';
  if (Array.isArray(v)) return v.map(toPlantValue);
  if (typeof v === 'object') {
    const out = {};
    for (const k of Object.keys(v)) out[k] = toPlantValue(v[k]);
    return out;
  }
  return v;
}

module.exports = { harvestSync, toPlantValue };
