'use strict';
// harvest_worker.js — runs inside a Worker thread.
const { workerData } = require('worker_threads');
const https = require('https');
const http  = require('http');

const { sab, url, method, bodyStr, headers, timeout } = workerData;
const ctrl = new Int32Array(sab, 0, 1);
const DATA_OFFSET = 8;
const DATA_MAX    = sab.byteLength - DATA_OFFSET;

function writeResult(obj) {
  try {
    const enc = Buffer.from(JSON.stringify(obj), 'utf8');
    enc.slice(0, DATA_MAX).copy(Buffer.from(sab, DATA_OFFSET, DATA_MAX));
  } catch (_) {}
  Atomics.store(ctrl, 0, 1);
  Atomics.notify(ctrl, 0);
}

function mapify(v) {
  if (v === null || v === undefined) return null;
  if (Array.isArray(v)) return v.map(mapify);
  if (typeof v === 'object') {
    const out = {};
    for (const k of Object.keys(v)) out[k] = mapify(v[k]);
    return out;
  }
  return v;
}

try {
  const u = new URL(url);
  const lib = u.protocol === 'https:' ? https : http;
  const meth = (method || 'GET').toUpperCase();
  const reqHeaders = Object.assign({ 'User-Agent': 'PlantLang-Chloroplast/0.6' }, headers || {});

  if (bodyStr) {
    reqHeaders['Content-Type']   = reqHeaders['Content-Type']   || 'application/json';
    reqHeaders['Content-Length'] = Buffer.byteLength(bodyStr);
  }

  const opts = {
    hostname : u.hostname,
    port     : u.port || undefined,
    path     : u.pathname + u.search,
    method   : meth,
    headers  : reqHeaders,
    timeout  : timeout || 10000,
  };

  const req = lib.request(opts, (res) => {
    const chunks = [];
    res.on('data', c => chunks.push(c));
    res.on('end', () => {
      const raw = Buffer.concat(chunks).toString('utf8');
      let body = raw;
      const ct = res.headers['content-type'] || '';
      if (ct.includes('application/json') || /^\s*[\[{]/.test(raw)) {
        try { body = mapify(JSON.parse(raw)); } catch (_) { body = raw; }
      }
      writeResult({ ok: res.statusCode >= 200 && res.statusCode < 300, status: res.statusCode, headers: res.headers, body });
    });
  });

  req.on('error',   e => writeResult({ ok: false, error: e.message, code: e.code }));
  req.on('timeout', () => { req.destroy(); writeResult({ ok: false, error: 'Request timed out', code: 'ETIMEDOUT' }); });
  if (bodyStr) req.write(bodyStr);
  req.end();
} catch (e) {
  writeResult({ ok: false, error: e.message });
}
