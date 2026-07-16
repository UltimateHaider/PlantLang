'use strict';
/**
 * service/sandbox-runner.js — forked child process worker
 *
 * Receives a single IPC message { mode, source, maxOutputBytes } from
 * codewords-server.js, executes the requested operation, and sends back
 * exactly one IPC message with the result, then lets the parent kill it.
 *
 * Running in a SEPARATE PROCESS (not just a try/catch in the main server)
 * is what makes the timeout in codewords-server.js actually work: an
 * infinite PlantLang loop (e.g. `SEASON TRUE, SHOW 1. 1\.`) blocks the
 * V8 event loop of whichever process runs it. The main server process
 * must stay responsive to serve other requests and to enforce the kill
 * timer, so untrusted execution is pushed into this disposable child.
 *
 * This file is NOT meant to be required directly — it communicates only
 * via process.send()/process.on('message').
 */

const path = require('path');
const PLANTLANG_ROOT = path.join(__dirname, '..');

function loadCore() {
  return {
    Interpreter: require(path.join(PLANTLANG_ROOT, 'core/interpreter')).Interpreter,
    parse:       require(path.join(PLANTLANG_ROOT, 'core/parser')).parse,
    typecheck:   require(path.join(PLANTLANG_ROOT, 'core/typechecker')).typecheck,
    generate:    require(path.join(PLANTLANG_ROOT, 'core/codegen')).generate,
  };
}

// ── Output capture with size cap ─────────────────────────────────────────────
function makeCappedCollector(maxBytes) {
  const lines = [];
  let bytes = 0;
  let truncated = false;
  return {
    push(text) {
      if (truncated) return;
      const b = Buffer.byteLength(text, 'utf8');
      if (bytes + b > maxBytes) {
        lines.push('… (output truncated — limit reached)');
        truncated = true;
        return;
      }
      bytes += b;
      lines.push(text);
    },
    toString() { return lines.join('\n'); },
    isTruncated() { return truncated; },
  };
}

// Strip ANSI color codes the interpreter's emit() may include via 'type' hints
// — sandbox output should be plain text for API consumers.
function stripAnsi(s) {
  return String(s).replace(/\x1b\[[0-9;]*m/g, '');
}

// ── Mode handlers ─────────────────────────────────────────────────────────────

function doRun(source, maxOutputBytes) {
  const { Interpreter, parse } = loadCore();
  const out = makeCappedCollector(maxOutputBytes);

  let prog;
  try {
    prog = parse(source);
  } catch (e) {
    return { ok: false, error: `Parse error: ${e.message}`, output: '' };
  }

  const interp = new Interpreter({
    mission: 'SAFE',
    emit: (text) => out.push(stripAnsi(text)),
  });

  // Block network-capable statements at the source-text level as a defense
  // in depth measure (LISTEN BRANCH would otherwise try to bind a real port
  // and keep this child process alive past its timeout window; HARVEST
  // would let submitted code reach arbitrary hosts from our server).
  if (/\bLISTEN\s+BRANCH\b/i.test(source)) {
    return { ok: false, error: 'LISTEN BRANCH is not permitted in the sandboxed compiler service.', output: '' };
  }
  if (/\bHARVEST\b/i.test(source)) {
    return { ok: false, error: 'HARVEST is not permitted in the sandboxed compiler service.', output: '' };
  }

  try {
    interp.runSource(source);
    return { ok: true, output: out.toString(), truncated: out.isTruncated() };
  } catch (e) {
    return {
      ok: false,
      error: e.message || String(e),
      stormType: e.stormType,
      line: e.line,
      column: e.column,
      output: out.toString(),
    };
  }
}

function doCheck(source) {
  const { parse, typecheck } = loadCore();
  let prog;
  try {
    prog = parse(source);
  } catch (e) {
    return { ok: false, error: `Parse error: ${e.message}`, diagnostics: [] };
  }

  const diags = typecheck(prog, source).map(d => ({
    severity: d.severity,
    code: d.code,
    message: d.message,
    line: d.line,
    column: d.column,
  }));

  const errorCount = diags.filter(d => d.severity === 'error').length;
  return { ok: errorCount === 0, diagnostics: diags };
}

function doVerify(source, maxOutputBytes) {
  const { Interpreter, parse } = loadCore();
  const out = makeCappedCollector(maxOutputBytes);

  if (/\bLISTEN\s+BRANCH\b/i.test(source)) {
    return { ok: false, error: 'LISTEN BRANCH is not permitted in the sandboxed compiler service.', output: '' };
  }
  if (/\bHARVEST\b/i.test(source)) {
    return { ok: false, error: 'HARVEST is not permitted in the sandboxed compiler service.', output: '' };
  }

  const fullSource = source + '\nSHOW_VERIFY_SUMMARY.';
  const interp = new Interpreter({
    mission: 'SAFE',
    emit: (text) => out.push(stripAnsi(text)),
  });

  try {
    interp.runSource(fullSource);
    return {
      ok: interp.verifyStats.failed === 0,
      output: out.toString(),
      truncated: out.isTruncated(),
      passed: interp.verifyStats.passed,
      failed: interp.verifyStats.failed,
    };
  } catch (e) {
    return {
      ok: false,
      error: e.message || String(e),
      output: out.toString(),
      passed: interp.verifyStats.passed,
      failed: interp.verifyStats.failed,
    };
  }
}

function doCompile(source, maxOutputBytes) {
  const { parse, generate } = loadCore();
  const fs   = require('fs');
  const os   = require('os');
  const { execFileSync } = require('child_process');

  let prog;
  try {
    prog = parse(source);
  } catch (e) {
    return { ok: false, error: `Parse error: ${e.message}`, diagnostics: [] };
  }

  const { code, errors } = generate(prog);
  if (errors.length > 0) {
    return {
      ok: false,
      cCode: code,
      diagnostics: errors.map(e => ({ message: e.message, line: e.line, column: e.column })),
      output: '',
    };
  }

  const tmpDir  = fs.mkdtempSync(path.join(os.tmpdir(), 'codewords-'));
  const cPath   = path.join(tmpDir, 'program.c');
  const binPath = path.join(tmpDir, 'program');

  try {
    fs.writeFileSync(cPath, code, 'utf8');
    execFileSync('gcc', [cPath, '-O2', '-lm', '-o', binPath], { stdio: 'pipe', timeout: 8000 });
    const out = execFileSync(binPath, [], { encoding: 'utf8', timeout: 3000, maxBuffer: maxOutputBytes });
    return { ok: true, cCode: code, output: out };
  } catch (e) {
    return {
      ok: false,
      cCode: code,
      error: (e.stderr && e.stderr.toString()) || e.message || String(e),
      output: (e.stdout && e.stdout.toString()) || '',
    };
  } finally {
    try { fs.rmSync(tmpDir, { recursive: true, force: true }); } catch (_) {}
  }
}

// ── Entry point ────────────────────────────────────────────────────────────────
process.on('message', (msg) => {
  const { mode, source, maxOutputBytes } = msg || {};
  let result;
  try {
    switch (mode) {
      case 'run':     result = doRun(source, maxOutputBytes);     break;
      case 'check':   result = doCheck(source);                   break;
      case 'verify':  result = doVerify(source, maxOutputBytes);  break;
      case 'compile': result = doCompile(source, maxOutputBytes); break;
      default:        result = { ok: false, error: `Unknown mode: ${mode}` };
    }
  } catch (e) {
    result = { ok: false, error: `Sandbox internal error: ${e.message}` };
  }
  try { process.send(result); } catch (_) { /* parent may have already given up */ }
  // Parent kills us right after receiving the message; exit defensively too.
  setTimeout(() => process.exit(0), 50);
});

// If somehow no message arrives (shouldn't happen — server always sends one
// immediately after fork), don't hang forever.
setTimeout(() => process.exit(1), 30000);
