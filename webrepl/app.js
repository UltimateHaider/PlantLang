'use strict';
/**
 * webrepl/app.js — PlantLang Web REPL front-end logic.
 *
 * Talks to a running CodeWords Compiler Service (service/codewords-server.js)
 * over HTTP. No build step, no dependencies — plain DOM + fetch.
 */

const editor        = document.getElementById('editor');
const output         = document.getElementById('output');
const runBtn         = document.getElementById('runBtn');
const runLabel       = document.getElementById('runLabel');
const outputLabel    = document.getElementById('outputLabel');
const elapsedTimeEl  = document.getElementById('elapsedTime');
const charCountEl    = document.getElementById('charCount');
const serverUrlInput = document.getElementById('serverUrl');
const connDot        = document.getElementById('connDot');
const connLabel      = document.getElementById('connLabel');
const exampleSelect  = document.getElementById('exampleSelect');
const modeButtons    = Array.from(document.querySelectorAll('.mode-btn'));

let currentMode = 'run';
let isRunning   = false;

const MODE_LABELS = {
  run:     { button: 'Run it',        output: 'Output' },
  check:   { button: 'Check it',      output: 'Diagnostics' },
  verify:  { button: 'Verify it',     output: 'Test results' },
  compile: { button: 'Compile it',    output: 'Compiled output' },
};

// ── Persisted state (server URL + last source) ──────────────────────────────
const STORAGE_KEY_SRC    = 'plantlang-webrepl-source';
const STORAGE_KEY_SERVER = 'plantlang-webrepl-server';

function loadPersisted() {
  try {
    const savedSrc = localStorage.getItem(STORAGE_KEY_SRC);
    const savedServer = localStorage.getItem(STORAGE_KEY_SERVER);
    if (savedServer) serverUrlInput.value = savedServer;
    if (savedSrc) {
      editor.value = savedSrc;
    } else if (window.PLANTLANG_EXAMPLES && window.PLANTLANG_EXAMPLES.length) {
      editor.value = window.PLANTLANG_EXAMPLES[0].source;
    }
  } catch (_) { /* localStorage unavailable — fine, just don't persist */ }
}

function persistSource() {
  try { localStorage.setItem(STORAGE_KEY_SRC, editor.value); } catch (_) {}
}
function persistServer() {
  try { localStorage.setItem(STORAGE_KEY_SERVER, serverUrlInput.value); } catch (_) {}
}

// ── Populate example dropdown ────────────────────────────────────────────────
function populateExamples() {
  if (!window.PLANTLANG_EXAMPLES) return;
  for (const ex of window.PLANTLANG_EXAMPLES) {
    const opt = document.createElement('option');
    opt.value = ex.name;
    opt.textContent = ex.name;
    exampleSelect.appendChild(opt);
  }
}

exampleSelect.addEventListener('change', () => {
  const chosen = window.PLANTLANG_EXAMPLES.find(e => e.name === exampleSelect.value);
  if (chosen) {
    editor.value = chosen.source;
    updateCharCount();
    persistSource();
    editor.focus();
  }
  exampleSelect.value = '';
});

// ── Char count ────────────────────────────────────────────────────────────────
function updateCharCount() {
  const n = editor.value.length;
  charCountEl.textContent = `${n.toLocaleString()} char${n === 1 ? '' : 's'}`;
}
editor.addEventListener('input', () => { updateCharCount(); persistSource(); });

// ── Mode switching ────────────────────────────────────────────────────────────
function setMode(mode) {
  currentMode = mode;
  modeButtons.forEach(b => b.classList.toggle('active', b.dataset.mode === mode));
  runLabel.textContent = MODE_LABELS[mode].button;
  outputLabel.textContent = MODE_LABELS[mode].output;
}
modeButtons.forEach(btn => btn.addEventListener('click', () => setMode(btn.dataset.mode)));

// ── Connection health check ──────────────────────────────────────────────────
async function checkConnection() {
  const base = serverUrlInput.value.replace(/\/+$/, '');
  try {
    const res = await fetch(`${base}/health`, { method: 'GET', signal: AbortSignal.timeout(3000) });
    if (res.ok) {
      const data = await res.json();
      connDot.className = 'conn-dot online';
      connLabel.textContent = `connected · v${data.version || '?'}`;
      return true;
    }
    throw new Error('bad status');
  } catch (_) {
    connDot.className = 'conn-dot offline';
    connLabel.textContent = 'offline';
    return false;
  }
}

serverUrlInput.addEventListener('change', () => { persistServer(); checkConnection(); });
serverUrlInput.addEventListener('blur', () => { persistServer(); checkConnection(); });

// ── Output rendering ──────────────────────────────────────────────────────────
function clearOutput() {
  output.innerHTML = '';
}

function renderEmpty(msg) {
  output.innerHTML = `<div class="output-empty">${escapeHtml(msg)}</div>`;
}

function renderSpinner(msg) {
  output.innerHTML = `
    <div class="spinner-line">
      <span class="spinner-dot"></span><span class="spinner-dot"></span><span class="spinner-dot"></span>
      <span>${escapeHtml(msg)}</span>
    </div>`;
}

function escapeHtml(s) {
  const div = document.createElement('div');
  div.textContent = s;
  return div.innerHTML;
}

function appendLine(text, cls, delayMs) {
  const div = document.createElement('div');
  div.className = 'out-line ' + (cls || 'out-show');
  div.textContent = text;
  div.style.animationDelay = `${delayMs}ms`;
  output.appendChild(div);
}

function classifyLine(line) {
  if (/^✓/.test(line)) return 'out-ok';
  if (/^✕/.test(line)) return 'out-error';
  if (/^⚠/.test(line)) return 'out-warn';
  if (/^›/.test(line) || /^\s*→/.test(line)) return 'out-info';
  return 'out-show';
}

function renderPlainOutput(text) {
  clearOutput();
  const lines = (text || '').split('\n');
  let delay = 0;
  let shown = 0;
  for (const line of lines) {
    if (line === '' && shown === 0) continue; // skip leading blank lines
    appendLine(line, classifyLine(line), delay);
    delay += 18;
    shown++;
  }
  if (shown === 0) renderEmpty('(no output)');
}

function renderMeta(html) {
  const div = document.createElement('div');
  div.className = 'out-meta';
  div.innerHTML = html;
  output.appendChild(div);
}

function renderDiagnostics(diags) {
  clearOutput();
  if (!diags || diags.length === 0) {
    renderEmpty('No issues found.');
    return;
  }
  diags.forEach((d, i) => {
    const item = document.createElement('div');
    item.className = 'diag-item ' + (d.severity || 'error');
    item.style.opacity = '0';
    item.style.animation = `settle .28s ease forwards`;
    item.style.animationDelay = `${i * 30}ms`;
    item.innerHTML = `
      <div class="diag-code">${escapeHtml(d.code || d.severity)}</div>
      <div class="diag-msg">${escapeHtml(d.message)}</div>
      <div class="diag-loc">line ${d.line}, column ${d.column}</div>
    `;
    output.appendChild(item);
  });
}

function renderCCodeToggle(cCode) {
  if (!cCode) return;
  const details = document.createElement('details');
  details.className = 'ccode-toggle';
  const summary = document.createElement('summary');
  summary.textContent = 'View generated C source';
  const pre = document.createElement('pre');
  pre.textContent = cCode;
  details.appendChild(summary);
  details.appendChild(pre);
  output.appendChild(details);
}

// ── Request execution ─────────────────────────────────────────────────────────
async function executeCurrentMode() {
  if (isRunning) return;

  const source = editor.value;
  if (!source.trim()) {
    renderEmpty('Nothing to run — write some PlantLang first.');
    return;
  }

  const base = serverUrlInput.value.replace(/\/+$/, '');
  isRunning = true;
  runBtn.disabled = true;
  elapsedTimeEl.textContent = '';
  renderSpinner({
    run: 'Running…', check: 'Checking types…',
    verify: 'Verifying…', compile: 'Compiling to C…',
  }[currentMode]);

  const t0 = performance.now();
  try {
    const res = await fetch(`${base}/${currentMode}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ source }),
      signal: AbortSignal.timeout(15000),
    });
    const data = await res.json();
    const elapsed = Math.round(performance.now() - t0);
    elapsedTimeEl.textContent = `${elapsed}ms`;

    renderResult(currentMode, data);
    connDot.className = 'conn-dot online';
    connLabel.textContent = 'connected';
  } catch (err) {
    const elapsed = Math.round(performance.now() - t0);
    elapsedTimeEl.textContent = `${elapsed}ms`;
    clearOutput();
    const isAbort = err.name === 'AbortError' || err.name === 'TimeoutError';
    appendLine(
      isAbort
        ? 'Request timed out — the service took too long to respond.'
        : `Could not reach the service at ${base}. Is it running?`,
      'out-error', 0
    );
    connDot.className = 'conn-dot offline';
    connLabel.textContent = 'offline';
  } finally {
    isRunning = false;
    runBtn.disabled = false;
  }
}

function renderResult(mode, data) {
  if (mode === 'check') {
    renderDiagnostics(data.diagnostics);
    return;
  }

  if (mode === 'verify') {
    clearOutput();
    if (data.error && !data.output) {
      appendLine(data.error, 'out-error', 0);
      return;
    }
    renderPlainOutput(data.output || '');
    if (typeof data.passed === 'number') {
      const cls = data.failed > 0 ? 'bad' : 'good';
      renderMeta(`<span class="${cls}">${data.passed} passed, ${data.failed} failed</span>`);
    }
    return;
  }

  if (mode === 'compile') {
    clearOutput();
    if (!data.ok && data.diagnostics && data.diagnostics.length) {
      renderDiagnostics(data.diagnostics.map(d => ({ ...d, severity: 'error', code: 'UNSUPPORTED' })));
      if (data.cCode) renderCCodeToggle(data.cCode);
      return;
    }
    if (!data.ok) {
      appendLine(data.error || 'Compilation failed.', 'out-error', 0);
      if (data.cCode) renderCCodeToggle(data.cCode);
      return;
    }
    renderPlainOutput(data.output || '(no output)');
    if (data.cCode) renderCCodeToggle(data.cCode);
    return;
  }

  // mode === 'run'
  clearOutput();
  if (!data.ok && !data.output) {
    appendLine(data.error || 'Execution failed.', 'out-error', 0);
    if (data.line) renderMeta(`<span class="bad">line ${data.line}, column ${data.column || 0}</span>`);
    return;
  }
  renderPlainOutput(data.output || '');
  if (!data.ok && data.error) {
    renderMeta(`<span class="bad">✕ ${escapeHtml(data.error)}${data.line ? ` (line ${data.line})` : ''}</span>`);
  }
}

// ── Wiring ────────────────────────────────────────────────────────────────────
runBtn.addEventListener('click', executeCurrentMode);

document.addEventListener('keydown', (e) => {
  const isMod = e.metaKey || e.ctrlKey;
  if (isMod && e.key === 'Enter') {
    e.preventDefault();
    executeCurrentMode();
  }
});

// Tab key inserts a literal tab in the editor instead of moving focus
editor.addEventListener('keydown', (e) => {
  if (e.key === 'Tab') {
    e.preventDefault();
    const start = editor.selectionStart, end = editor.selectionEnd;
    editor.value = editor.value.slice(0, start) + '  ' + editor.value.slice(end);
    editor.selectionStart = editor.selectionEnd = start + 2;
  }
});

// ── Init ──────────────────────────────────────────────────────────────────────
populateExamples();
loadPersisted();
updateCharCount();
setMode('run');
checkConnection();
setInterval(checkConnection, 15000);
