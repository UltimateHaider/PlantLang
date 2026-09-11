'use strict';
/**
 * service/test_service.js — automated tests for codewords-server.js
 *
 * Starts the service on a random high port, exercises every endpoint
 * (including the safety features), and reports pass/fail.
 *
 * Run:  node service/test_service.js
 */

const http = require('http');
const { createServer } = require('./codewords-server');

const PORT = 18500 + Math.floor(Math.random() * 1000);
let passed = 0, failed = 0;

function request(method, urlPath, body) {
  return new Promise((resolve, reject) => {
    const data = body ? JSON.stringify(body) : null;
    const req = http.request({
      hostname: 'localhost', port: PORT, path: urlPath, method,
      headers: data ? { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(data) } : {},
    }, (res) => {
      let chunks = '';
      res.on('data', c => chunks += c);
      res.on('end', () => {
        let parsed = null;
        try { parsed = JSON.parse(chunks); } catch (_) {}
        resolve({ status: res.statusCode, body: parsed, raw: chunks });
      });
    });
    req.on('error', reject);
    if (data) req.write(data);
    req.end();
  });
}

async function test(name, fn) {
  process.stdout.write(`  ${name} ... `);
  try {
    await fn();
    console.log('\x1b[32m✓\x1b[0m');
    passed++;
  } catch (e) {
    console.log('\x1b[31m✗\x1b[0m');
    console.log('    ' + e.message);
    failed++;
  }
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg || 'Assertion failed');
}

async function main() {
  const server = createServer();
  await new Promise(resolve => server.listen(PORT, '127.0.0.1', resolve));
  console.log(`CodeWords Compiler Service — test suite (port ${PORT})\n`);

  await test('GET /health returns ok', async () => {
    const r = await request('GET', '/health');
    assert(r.status === 200, `status=${r.status}`);
    assert(r.body.ok === true, 'expected ok:true');
  });

  await test('POST /run — basic program', async () => {
    const r = await request('POST', '/run', {
      source: 'MISSION: SAFE.\n1\\ CREATE x(NUM) TO 10.\n1\\ SHOW x + 5.'
    });
    assert(r.status === 200, `status=${r.status}`);
    assert(r.body.ok === true, JSON.stringify(r.body));
    assert(r.body.output.includes('15'), `output was: ${r.body.output}`);
  });

  await test('POST /run — missing source returns 400', async () => {
    const r = await request('POST', '/run', {});
    assert(r.status === 400, `status=${r.status}`);
    assert(r.body.ok === false);
  });

  await test('POST /run — invalid JSON returns 400', async () => {
    const r = await new Promise((resolve, reject) => {
      const req = http.request({ hostname: 'localhost', port: PORT, path: '/run', method: 'POST',
        headers: { 'Content-Type': 'application/json' } }, (res) => {
        let chunks = ''; res.on('data', c => chunks += c);
        res.on('end', () => resolve({ status: res.statusCode, body: JSON.parse(chunks) }));
      });
      req.on('error', reject);
      req.write('{not valid json');
      req.end();
    });
    assert(r.status === 400, `status=${r.status}`);
  });

  await test('POST /run — parse error surfaces cleanly', async () => {
    const r = await request('POST', '/run', { source: '@@@ garbage !!!' });
    assert(r.body.ok === false);
    assert(typeof r.body.error === 'string' && r.body.error.length > 0);
  });

  await test('POST /run — LISTEN BRANCH is blocked', async () => {
    const r = await request('POST', '/run', {
      source: 'MISSION: SAFE.\n1\\ LISTEN BRANCH ON 9999 WITH cfg AS req MAP,\n2\\   GIVE 1 AS RESPONSE.\n1\\ LISTEN/.'
    });
    assert(r.body.ok === false);
    assert(/LISTEN BRANCH/.test(r.body.error));
  });

  await test('POST /run — HARVEST is blocked', async () => {
    const r = await request('POST', '/run', {
      source: 'MISSION: SAFE.\n1\\ HARVEST "http://example.com" AS r.'
    });
    assert(r.body.ok === false);
    assert(/HARVEST/.test(r.body.error));
  });

  await test('POST /run — infinite loop times out (allow up to 8s)', async () => {
    const start = Date.now();
    const r = await request('POST', '/run', {
      source: 'MISSION: SAFE.\n1\\ SEASON TRUE,\n2\\   SHOW 1.\n1\\.'
    });
    const elapsed = Date.now() - start;
    assert(r.status === 408, `status=${r.status}`);
    assert(r.body.code === 'TIMEOUT', JSON.stringify(r.body));
    assert(elapsed < 7000, `took too long: ${elapsed}ms`);
  });

  await test('POST /check — clean program has no errors', async () => {
    const r = await request('POST', '/check', {
      source: 'MISSION: SAFE.\n1\\ CREATE x(NUM) TO 1.\n1\\ SHOW x.'
    });
    assert(r.body.ok === true, JSON.stringify(r.body));
    assert(r.body.diagnostics.length === 0);
  });

  await test('POST /check — type mismatch is detected', async () => {
    const r = await request('POST', '/check', {
      source: 'MISSION: SAFE.\n1\\ CREATE n(TX) TO "x".\n1\\ ACTION add(a(NUM), b(NUM)),\n2\\   GIVE a + b.\n1\\ /ACTION.\n1\\ REAP r FROM add, n, 5.'
    });
    assert(r.body.ok === false);
    assert(r.body.diagnostics.some(d => d.code === 'TYPE_MISMATCH'), JSON.stringify(r.body.diagnostics));
  });

  await test('POST /verify — passing and failing assertions counted', async () => {
    const r = await request('POST', '/verify', {
      source: 'MISSION: SAFE.\nVERIFY "pass", 1 IS 1.\nVERIFY "fail", 1 IS 2.'
    });
    assert(r.body.passed === 1, JSON.stringify(r.body));
    assert(r.body.failed === 1, JSON.stringify(r.body));
    assert(r.body.ok === false);
  });

  await test('POST /compile — generates and runs a binary', async () => {
    const r = await request('POST', '/compile', {
      source: 'MISSION: FAST.\n1\\ CREATE n(NUM) TO 6.\n1\\ SHOW n * 7.'
    });
    assert(r.body.ok === true, JSON.stringify(r.body));
    assert(r.body.output.trim() === '42', `output was: ${r.body.output}`);
    assert(typeof r.body.cCode === 'string' && r.body.cCode.includes('int main'));
  });

  await test('POST /compile — unsupported construct reports diagnostics', async () => {
    const r = await request('POST', '/compile', {
      source: 'MISSION: SAFE.\n1\\ CREATE items(LIST) TO a, b.\n1\\ SHOW items.'
    });
    assert(r.body.ok === false);
    assert(Array.isArray(r.body.diagnostics) && r.body.diagnostics.length > 0);
  });

  await test('GET unknown route returns 404', async () => {
    const r = await request('GET', '/nonexistent');
    assert(r.status === 404);
  });

  await test('Concurrent requests do not block each other', async () => {
    const slow = request('POST', '/run', { source: 'MISSION: SAFE.\n1\\ SEASON TRUE,\n2\\   SHOW 1.\n1\\.' });
    await new Promise(r => setTimeout(r, 300));
    const fastStart = Date.now();
    const fast = await request('POST', '/run', { source: 'MISSION: SAFE.\n1\\ SHOW "fast".' });
    const fastElapsed = Date.now() - fastStart;
    assert(fast.body.ok === true);
    assert(fastElapsed < 2000, `fast request took ${fastElapsed}ms — server may be blocked`);
    await slow; // let the slow one finish/timeout before ending the test
  });

  server.close();
  console.log(`\n${passed} passed, ${failed} failed`);
  process.exit(failed > 0 ? 1 : 0);
}

main().catch(e => { console.error('FATAL:', e); process.exit(1); });
