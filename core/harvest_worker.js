'use strict';
// This worker performs the synchronous block itself: it receives a request
// description, runs the async fetch INSIDE the worker (where blocking the
// worker's own thread via Atomics.wait is safe), and writes the JSON result
// directly into a SharedArrayBuffer that the main thread polls.
//
// Architecture:
//   main thread -> spawns worker with {url, method, headers, body, sab}
//   worker      -> does async fetch, writes result bytes into sab, sets flag, notifies
//   main thread -> Atomics.wait on sab[0] (this WAIT happens on the worker's
//                  parent — but critically we now wait via Atomics.wait directly
//                  on the SharedArrayBuffer from the main thread while the
//                  worker writes to it independently and asynchronously,
//                  which does NOT require any event on the main thread's
//                  event loop to fire — Atomics.wait/notify works across
//                  threads at the OS futex level, independent of either
//                  thread's JS event loop.)
const { workerData } = require('worker_threads');

const { url, method, headers, body, timeoutMs, sab, sabBytes } = workerData;
const flag = new Int32Array(sab, 0, 1);          // [0] = ready flag
const lenView = new Int32Array(sab, 4, 1);        // [1] = payload byte length
const bytesView = new Uint8Array(sab, 8, sabBytes - 8);

function writeResult(obj) {
  const json = JSON.stringify(obj);
  const encoded = Buffer.from(json, 'utf8');
  const n = Math.min(encoded.length, bytesView.length);
  bytesView.set(encoded.subarray(0, n));
  Atomics.store(lenView, 0, n);
  Atomics.store(flag, 0, 1);
  Atomics.notify(flag, 0);
}

(async () => {
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), timeoutMs || 10000);

    const init = { method: method || 'GET', headers: headers || {}, signal: controller.signal };
    if (body !== undefined && body !== null && method !== 'GET' && method !== 'HEAD') {
      if (typeof body === 'object') {
        init.body = JSON.stringify(body);
        init.headers = { 'Content-Type': 'application/json', ...init.headers };
      } else {
        init.body = String(body);
      }
    }

    const res = await fetch(url, init);
    clearTimeout(timer);

    const status = res.status, ok = res.ok;
    const contentType = res.headers.get('content-type') || '';
    const rawText = await res.text();
    let data;
    if (contentType.includes('application/json')) {
      try { data = JSON.parse(rawText); } catch (_) { data = rawText; }
    } else {
      data = rawText;
    }

    writeResult({ success: true, status, ok, data });
  } catch (e) {
    writeResult({ success: false, error: e.name === 'AbortError' ? 'TIMEOUT' : (e.message || String(e)) });
  }
})();
