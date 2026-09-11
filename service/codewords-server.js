'use strict';
/**
 * service/codewords-server.js — CodeWords Compiler Service
 *
 * A standalone Node.js HTTP API that lets external clients (a Web REPL UI,
 * curl, other tools) submit PlantLang source code and get back:
 *   - interpreted execution output (POST /run)
 *   - static type-check diagnostics (POST /check)
 *   - generated C source + compiled binary output (POST /compile)
 *   - VERIFY test results (POST /verify)
 *
 * This is a plain Node.js HTTP service (NOT written in PlantLang) because
 * it needs to run the interpreter as a library, manage OS processes for
 * gcc, and enforce timeouts/sandboxing around arbitrary user code —
 * none of which PlantLang itself can safely do to its own host process.
 *
 * ── Safety model ─────────────────────────────────────────────────────────
 * User-submitted PlantLang runs IN-PROCESS via the Interpreter class, so:
 *   - A wall-clock timeout (default 5s) aborts long-running/infinite loops
 *     by racing execution against a timer and killing the process cleanly
 *     is NOT possible for synchronous in-process code — so run/verify/check
 *     execute in a forked child process per request, which CAN be killed.
 *   - Output is capped (default 64KB) to prevent memory exhaustion from
 *     runaway SHOW loops.
 *   - HARVEST/LISTEN BRANCH are blocked in the sandbox (no outbound network
 *     or port-binding from submitted code) — the whole point of this
 *     service is to let people try PlantLang, not attack the host.
 *   - Compilation (gcc) also runs in a child process with a timeout.
 *
 * ── Endpoints ────────────────────────────────────────────────────────────
 *   GET  /health                    → { ok: true, version }
 *   POST /run     { source }        → { ok, output, diagnostics, elapsedMs }
 *   POST /check   { source }        → { ok, diagnostics, elapsedMs }
 *   POST /verify  { source }        → { ok, output, passed, failed, elapsedMs }
 *   POST /compile { source }        → { ok, output, cCode?, diagnostics, elapsedMs }
 *
 * Run:
 *   node service/codewords-server.js [--port 8420]
 */

const http    = require('http');
const path    = require('path');
const { fork } = require('child_process');

const PLANTLANG_ROOT = path.join(__dirname, '..');
const RUNNER_PATH     = path.join(__dirname, 'sandbox-runner.js');

// ── Config ──────────────────────────────────────────────────────────────────
const DEFAULT_PORT       = 8420;
const EXEC_TIMEOUT_MS    = 5000;   // per-request wall clock limit
const MAX_SOURCE_BYTES   = 64 * 1024;   // 64KB max submitted source
const MAX_OUTPUT_BYTES   = 64 * 1024;   // 64KB max captured output
const MAX_BODY_BYTES     = 128 * 1024;  // 128KB max raw HTTP body

// ── CORS (Web REPL UI runs from a browser on a different origin) ───────────
function setCORS(res) {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
}

// ── JSON helpers ─────────────────────────────────────────────────────────────
function sendJSON(res, status, obj) {
  const body = JSON.stringify(obj);
  res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Content-Length': Buffer.byteLength(body) });
  res.end(body);
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    let chunks = [];
    let bytes = 0;
    req.on('data', (c) => {
      bytes += c.length;
      if (bytes > MAX_BODY_BYTES) {
        reject(new Error('Request body too large'));
        req.destroy();
        return;
      }
      chunks.push(c);
    });
    req.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
    req.on('error', reject);
  });
}

// ── Sandbox execution ────────────────────────────────────────────────────────
// Runs `mode` (run|check|verify|compile) against `source` in a forked child
// process (service/sandbox-runner.js) so it can be hard-killed on timeout
// without taking down this server. Returns a Promise resolving to the
// runner's JSON result, or rejecting with a timeout/crash error.
function runSandboxed(mode, source, timeoutMs) {
  return new Promise((resolve, reject) => {
    const child = fork(RUNNER_PATH, [], {
      cwd: PLANTLANG_ROOT,
      stdio: ['ignore', 'ignore', 'ignore', 'ipc'],
      // Disable network/env inheritance surface where possible.
      env: { NODE_ENV: 'sandbox', PATH: process.env.PATH },
    });

    let settled = false;
    const timer = setTimeout(() => {
      if (settled) return;
      settled = true;
      child.kill('SIGKILL');
      reject(new SandboxError('TIMEOUT', `Execution exceeded ${timeoutMs}ms`));
    }, timeoutMs);

    child.on('message', (msg) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      child.kill('SIGKILL'); // done — no need to keep it alive
      resolve(msg);
    });

    child.on('error', (err) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      reject(new SandboxError('SPAWN_ERROR', err.message));
    });

    child.on('exit', (code, signal) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      reject(new SandboxError('CRASHED', `Sandbox process exited (code=${code}, signal=${signal})`));
    });

    child.send({ mode, source, maxOutputBytes: MAX_OUTPUT_BYTES });
  });
}

class SandboxError extends Error {
  constructor(code, message) {
    super(message);
    this.code = code;
  }
}

// ── Request validation ──────────────────────────────────────────────────────
function validateSource(body) {
  let parsed;
  try {
    parsed = JSON.parse(body);
  } catch (e) {
    return { error: 'Invalid JSON body' };
  }
  if (typeof parsed.source !== 'string') {
    return { error: '"source" field (string) is required' };
  }
  if (Buffer.byteLength(parsed.source, 'utf8') > MAX_SOURCE_BYTES) {
    return { error: `Source too large — max ${MAX_SOURCE_BYTES} bytes` };
  }
  if (parsed.source.trim() === '') {
    return { error: '"source" cannot be empty' };
  }
  return { source: parsed.source };
}

// ── Route handlers ───────────────────────────────────────────────────────────
async function handleModeEndpoint(mode, req, res) {
  const t0 = Date.now();
  let body;
  try {
    body = await readBody(req);
  } catch (e) {
    return sendJSON(res, 413, { ok: false, error: e.message });
  }

  const { source, error } = validateSource(body);
  if (error) return sendJSON(res, 400, { ok: false, error });

  try {
    const result = await runSandboxed(mode, source, EXEC_TIMEOUT_MS);
    result.elapsedMs = Date.now() - t0;
    sendJSON(res, 200, result);
  } catch (e) {
    if (e instanceof SandboxError) {
      const status = e.code === 'TIMEOUT' ? 408 : 500;
      return sendJSON(res, status, {
        ok: false,
        error: e.message,
        code: e.code,
        elapsedMs: Date.now() - t0,
      });
    }
    sendJSON(res, 500, { ok: false, error: 'Internal error', elapsedMs: Date.now() - t0 });
  }
}

// ── Server ────────────────────────────────────────────────────────────────────
function createServer() {
  const server = http.createServer(async (req, res) => {
    setCORS(res);

    if (req.method === 'OPTIONS') {
      res.writeHead(204);
      return res.end();
    }

    const url = new URL(req.url, 'http://localhost');

    if (req.method === 'GET' && url.pathname === '/health') {
      return sendJSON(res, 200, { ok: true, service: 'CodeWords Compiler Service', version: '1.0.0' });
    }

    if (req.method === 'POST' && url.pathname === '/run') {
      return handleModeEndpoint('run', req, res);
    }
    if (req.method === 'POST' && url.pathname === '/check') {
      return handleModeEndpoint('check', req, res);
    }
    if (req.method === 'POST' && url.pathname === '/verify') {
      return handleModeEndpoint('verify', req, res);
    }
    if (req.method === 'POST' && url.pathname === '/compile') {
      return handleModeEndpoint('compile', req, res);
    }

    sendJSON(res, 404, { ok: false, error: `No route: ${req.method} ${url.pathname}` });
  });

  return server;
}

// ── CLI entry point ──────────────────────────────────────────────────────────
if (require.main === module) {
  const args = process.argv.slice(2);
  const pIdx = args.indexOf('--port');
  const port = pIdx !== -1 ? parseInt(args[pIdx + 1], 10) : DEFAULT_PORT;

  const server = createServer();
  server.listen(port, '0.0.0.0', () => {
    console.log(`🌿 CodeWords Compiler Service listening on :${port}`);
    console.log(`   GET  /health`);
    console.log(`   POST /run      { source }`);
    console.log(`   POST /check    { source }`);
    console.log(`   POST /verify   { source }`);
    console.log(`   POST /compile  { source }`);
  });

  process.on('SIGINT', () => { server.close(() => process.exit(0)); });
  process.on('SIGTERM', () => { server.close(() => process.exit(0)); });
}

module.exports = { createServer };
