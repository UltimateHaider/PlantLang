'use strict';
/**
 * webrepl/test_webrepl.js — integration test
 *
 * Loads the REAL index.html/app.js/examples-data.js into a jsdom document,
 * points it at a live CodeWords Compiler Service (started by this script),
 * and drives the actual UI code paths (button clicks, mode switching,
 * fetch calls) to verify the whole pipeline end-to-end — not a mock.
 *
 * Run:  node webrepl/test_webrepl.js
 */

const fs = require('fs');
const path = require('path');
const { JSDOM } = require('jsdom');
const { createServer } = require('../service/codewords-server');

const PORT = 18700 + Math.floor(Math.random() * 500);
let passed = 0, failed = 0;

function test(name, fn) {
  return fn().then(() => {
    console.log(`  \x1b[32m✓\x1b[0m ${name}`);
    passed++;
  }).catch(e => {
    console.log(`  \x1b[31m✗\x1b[0m ${name}`);
    console.log(`    ${e.message}`);
    failed++;
  });
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg || 'Assertion failed');
}

async function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

async function loadApp(serverUrl) {
  const htmlPath = path.join(__dirname, 'index.html');
  const html = fs.readFileSync(htmlPath, 'utf8');

  const dom = new JSDOM(html, {
    url: 'http://localhost/',
    runScripts: 'outside-only',
    resources: 'usable',
    pretendToBeVisual: true,
  });

  const { window } = dom;

  // jsdom's AbortSignal/AbortController live in a different realm than
  // Node's global fetch, which trips a cross-realm check inside undici
  // ("TypeError: fetch failed" with no useful detail). This is purely a
  // Node+jsdom test-harness quirk — real browsers don't split realms like
  // this — so we force window.AbortSignal to Node's own implementation
  // for this test run only.
  window.AbortSignal = AbortSignal;
  window.AbortController = AbortController;

  // jsdom has no fetch by default in this version — wire in Node's global fetch.
  window.fetch = fetch;

  // localStorage polyfill (jsdom supports it, but guard just in case)
  if (!window.localStorage) {
    const store = {};
    window.localStorage = {
      getItem: k => (k in store ? store[k] : null),
      setItem: (k, v) => { store[k] = String(v); },
      removeItem: k => { delete store[k]; },
    };
  }

  // Run the examples data script first, then set the server URL directly
  // in the HTML *before* app.js runs, so its init-time checkConnection()
  // call (which fires the moment app.js is evaluated) targets our test
  // server instead of the page's hardcoded default.
  const examplesSrc = fs.readFileSync(path.join(__dirname, 'examples-data.js'), 'utf8');
  window.eval(examplesSrc);

  const serverInputEl = window.document.getElementById('serverUrl');
  serverInputEl.value = serverUrl;
  serverInputEl.setAttribute('value', serverUrl);

  const appSrc = fs.readFileSync(path.join(__dirname, 'app.js'), 'utf8');
  window.eval(appSrc);

  // Let init's async checkConnection() settle
  await sleep(800);

  return { dom, window, document: window.document };
}

async function main() {
  const server = createServer();
  await new Promise(resolve => server.listen(PORT, '127.0.0.1', resolve));
  const serverUrl = `http://localhost:${PORT}`;
  console.log(`PlantLang Web REPL — integration tests (service on :${PORT})\n`);

  await test('page loads with editor, output, and mode buttons present', async () => {
    const { document } = await loadApp(serverUrl);
    assert(document.getElementById('editor'), 'editor missing');
    assert(document.getElementById('output'), 'output missing');
    assert(document.querySelectorAll('.mode-btn').length === 4, 'expected 4 mode buttons');
  });

  await test('examples dropdown is populated from examples-data.js', async () => {
    const { document, window } = await loadApp(serverUrl);
    const select = document.getElementById('exampleSelect');
    assert(select.options.length > 1, 'dropdown should have example options beyond the placeholder');
    assert(window.PLANTLANG_EXAMPLES.length >= 5, 'expected several curated examples');
  });

  await test('connection indicator shows online against a live service', async () => {
    const { document } = await loadApp(serverUrl);
    const label = document.getElementById('connLabel').textContent;
    assert(/connected/.test(label), `expected "connected", got "${label}"`);
  });

  await test('connection indicator shows offline against a dead server', async () => {
    const { document } = await loadApp('http://localhost:1');
    await sleep(300);
    const label = document.getElementById('connLabel').textContent;
    assert(/offline/.test(label), `expected "offline", got "${label}"`);
  });

  await test('Run It executes source and renders SHOW output as settled lines', async () => {
    const { document, window } = await loadApp(serverUrl);
    const editor = document.getElementById('editor');
    editor.value = 'MISSION: SAFE.\n1\\ CREATE x(NUM) TO 20.\n1\\ SHOW x + 22.';
    document.getElementById('runBtn').click();
    await sleep(700);
    const outText = document.getElementById('output').textContent;
    assert(outText.includes('42'), `expected output to contain 42, got: ${outText}`);
  });

  await test('mode switch to Check It updates button label and runs typecheck', async () => {
    const { document } = await loadApp(serverUrl);
    const checkBtn = Array.from(document.querySelectorAll('.mode-btn')).find(b => b.dataset.mode === 'check');
    checkBtn.click();
    assert(document.getElementById('runLabel').textContent === 'Check it', 'run label should update to Check it');

    const editor = document.getElementById('editor');
    editor.value = 'MISSION: SAFE.\n1\\ CREATE n(TX) TO "x".\n1\\ ACTION add(a(NUM), b(NUM)),\n2\\   GIVE a + b.\n1\\ /ACTION.\n1\\ REAP r FROM add, n, 5.';
    document.getElementById('runBtn').click();
    await sleep(700);
    const html = document.getElementById('output').innerHTML;
    assert(html.includes('TYPE_MISMATCH'), `expected TYPE_MISMATCH diagnostic in output, got: ${html.slice(0,300)}`);
  });

  await test('mode switch to Verify It runs VERIFY suite and shows pass/fail counts', async () => {
    const { document } = await loadApp(serverUrl);
    const verifyBtn = Array.from(document.querySelectorAll('.mode-btn')).find(b => b.dataset.mode === 'verify');
    verifyBtn.click();

    const editor = document.getElementById('editor');
    editor.value = 'MISSION: SAFE.\nVERIFY "ok one", 1 IS 1.\nVERIFY "fails one", 1 IS 2.';
    document.getElementById('runBtn').click();
    await sleep(700);
    const text = document.getElementById('output').textContent;
    assert(/1 passed, 1 failed/.test(text), `expected pass/fail summary, got: ${text}`);
  });

  await test('mode switch to Compile It shows compiled output and C source toggle', async () => {
    const { document } = await loadApp(serverUrl);
    const compileBtn = Array.from(document.querySelectorAll('.mode-btn')).find(b => b.dataset.mode === 'compile');
    compileBtn.click();

    const editor = document.getElementById('editor');
    editor.value = 'MISSION: FAST.\n1\\ CREATE n(NUM) TO 6.\n1\\ SHOW n * 7.';
    document.getElementById('runBtn').click();
    await sleep(2000);
    const text = document.getElementById('output').textContent;
    assert(text.includes('42'), `expected compiled output to contain 42, got: ${text}`);
    const details = document.querySelector('.ccode-toggle');
    assert(details, 'expected a "View generated C source" toggle');
    assert(details.querySelector('pre').textContent.includes('int main'), 'expected C source to include int main');
  });

  await test('unsupported construct in Compile It renders diagnostics, not a crash', async () => {
    const { document } = await loadApp(serverUrl);
    const compileBtn = Array.from(document.querySelectorAll('.mode-btn')).find(b => b.dataset.mode === 'compile');
    compileBtn.click();

    const editor = document.getElementById('editor');
    editor.value = 'MISSION: SAFE.\n1\\ CREATE items(LIST) TO a, b.\n1\\ SHOW items.';
    document.getElementById('runBtn').click();
    await sleep(1500);
    const diagItems = document.querySelectorAll('.diag-item');
    assert(diagItems.length > 0, 'expected at least one diagnostic item rendered');
  });

  await test('selecting an example from the dropdown loads it into the editor', async () => {
    const { document, window } = await loadApp(serverUrl);
    const select = document.getElementById('exampleSelect');
    const secondExample = window.PLANTLANG_EXAMPLES[1];
    select.value = secondExample.name;
    select.dispatchEvent(new window.Event('change'));
    const editor = document.getElementById('editor');
    assert(editor.value === secondExample.source, 'editor content should match selected example source');
  });

  await test('char counter updates as the editor content changes', async () => {
    const { document, window } = await loadApp(serverUrl);
    const editor = document.getElementById('editor');
    editor.value = 'abcde';
    editor.dispatchEvent(new window.Event('input'));
    const count = document.getElementById('charCount').textContent;
    assert(/5 char/.test(count), `expected "5 chars", got "${count}"`);
  });

  await test('empty editor shows a friendly message instead of calling the service', async () => {
    const { document } = await loadApp(serverUrl);
    const editor = document.getElementById('editor');
    editor.value = '   ';
    document.getElementById('runBtn').click();
    await sleep(200);
    const text = document.getElementById('output').textContent;
    assert(/Nothing to run/.test(text), `expected empty-input message, got: ${text}`);
  });

  await test('Cmd/Ctrl+Enter triggers execution', async () => {
    const { document, window } = await loadApp(serverUrl);
    const editor = document.getElementById('editor');
    editor.value = 'MISSION: SAFE.\n1\\ SHOW "shortcut works".';
    const evt = new window.KeyboardEvent('keydown', { key: 'Enter', ctrlKey: true, bubbles: true, cancelable: true });
    document.dispatchEvent(evt);
    await sleep(700);
    const text = document.getElementById('output').textContent;
    assert(text.includes('shortcut works'), `expected output to include the SHOW text, got: ${text}`);
  });

  server.close();
  console.log(`\n${passed} passed, ${failed} failed`);
  process.exit(failed > 0 ? 1 : 0);
}

main().catch(e => { console.error('FATAL:', e); process.exit(1); });
