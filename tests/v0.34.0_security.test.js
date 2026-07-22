const { NonBlockingAuditLogger } = require('../src/security/audit/audit_logger');
const { mTLSJwtGuard, JWTVerificationError } = require('../src/security/network/mtls_jwt_guard');
const { CapabilityGuard, CapabilityViolationError, CAPABILITIES } = require('../src/security/sandbox/capability_guard');
const crypto = require('crypto');

let passed = 0, failed = 0;

function assert(cond, label) {
  if (cond) { passed++; console.log('  \u001b[32m✓\u001b[0m ' + label); }
  else { failed++; console.log('  \u001b[31m✗\u001b[0m ' + label + ' (assertion failed)'); }
}

function assertEqual(a, b, label) {
  if (a === b) { passed++; console.log('  \u001b[32m✓\u001b[0m ' + label); }
  else { failed++; console.log('  \u001b[31m✗\u001b[0m ' + label + ' (expected ' + JSON.stringify(a) + ' === ' + JSON.stringify(b) + ')'); }
}

function assertThrows(fn, label) {
  try { fn(); failed++; console.log('  \u001b[31m✗\u001b[0m ' + label + ' (expected throw)'); }
  catch (e) { passed++; console.log('  \u001b[32m✓\u001b[0m ' + label); }
}

function assertNotThrows(fn, label) {
  try { fn(); passed++; console.log('  \u001b[32m✓\u001b[0m ' + label); }
  catch (e) { failed++; console.log('  \u001b[31m✗\u001b[0m ' + label + ' (unexpected throw: ' + e.message + ')'); }
}

// ── Audit Logger Tests ──
console.log('\u001b[1m--- Audit Logger Integrity ---\u001b[0m');

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 128, workerPath: '/dev/null' });
  logger.record('I', 'test event 1');
  logger.record('W', 'test event 2');
  const snap = logger.snapshot();
  assert(snap.metrics.length >= 2, 'records at least 2 entries');
  assert(snap.metrics[0].eventType === 'I', 'first entry eventType I');
  assert(snap.metrics[1].eventType === 'W', 'second entry eventType W');
  assert(snap.metrics[0].data === 'test event 1', 'first entry data correct');
  assert(snap.metrics[1].data === 'test event 2', 'second entry data correct');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 64, workerPath: '/dev/null' });
  const result = logger.verifyIntegrity();
  assert(result.valid === true, 'empty logger integrity is valid');
  logger.record('I', 'entry one');
  logger.record('W', 'entry two');
  const result2 = logger.verifyIntegrity();
  assert(result2.valid === true, 'integrity valid after 2 entries');
  assert(result2.results.length === 0, 'no integrity issues reported');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 64, workerPath: '/dev/null' });
  logger.record('E', 'tamper test');
  const snap = logger.snapshot();
  assert(snap.metrics.length >= 1, 'tamper test recorded');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 64, workerPath: '/dev/null' });
  for (let i = 0; i < 10; i++) logger.record('I', 'event ' + i);
  const snap = logger.snapshot();
  assert(snap.overflowCount >= 0, 'overflowCount is non-negative');
  assert(snap.metrics.length <= 64, 'snapshot respects buffer capacity');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 1024, workerPath: '/dev/null' });
  const start = Date.now();
  for (let i = 0; i < 100; i++) logger.record('I', 'bench ' + i);
  const elapsed = Date.now() - start;
  assert(elapsed < 50, '100 records in < 50ms (FAST path overhead)');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 16, workerPath: '/dev/null' });
  for (let i = 0; i < 32; i++) logger.record('I', 'overflow ' + i);
  const snap = logger.snapshot();
  assert(snap.overflowCount > 0, 'overflow detected with small ring');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 8, workerPath: '/dev/null' });
  for (let i = 0; i < 100; i++) logger.record('W', 'stress ' + i);
  const result = logger.verifyIntegrity();
  assert(result.valid === true, 'integrity holds after overflow stress');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 4, workerPath: '/dev/null' });
  for (let i = 0; i < 20; i++) logger.record('C', 'crit ' + i);
  const snap = logger.snapshot();
  assert(snap.metrics.length > 0, 'snapshot non-empty after heavy overflow');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 10, workerPath: '/dev/null' });
  for (let i = 0; i < 5; i++) logger.record('I', 'pre-close ' + i);
  logger.close();
  assert(true, 'close completes without error');
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 64, workerPath: '/dev/null' });
  logger.record('I', 'a');
  logger.record('W', 'b');
  logger.record('E', 'c');
  const snap = logger.snapshot();
  assert(snap.hasOwnProperty('uptimeNs'), 'snapshot has uptimeNs');
  assert(typeof snap.uptimeNs === 'bigint', 'uptimeNs is bigint');
  logger.close();
})();

// ── Hash Chain Tests ──
console.log('\u001b[1m--- Hash Chain Integrity ---\u001b[0m');

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 32, workerPath: '/dev/null' });
  logger.record('I', 'chain test a');
  logger.record('I', 'chain test b');
  logger.record('I', 'chain test c');
  const snap = logger.snapshot();
  assert(snap.metrics.length === 3, '3 entries recorded for chain');
  assert(snap.metrics[0].hash.length === 64, 'first entry hash is 64 hex chars');
  assert(snap.metrics[1].prevHash === snap.metrics[0].hash, 'second entry prevHash matches first hash');
  assert(snap.metrics[2].prevHash === snap.metrics[1].hash, 'third entry prevHash matches second hash');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 32, workerPath: '/dev/null' });
  logger.record('I', 'single');
  const snap = logger.snapshot();
  assert(snap.metrics[0].prevHash === '0000000000000000000000000000000000000000000000000000000000000000', 'first entry prevHash is zero');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 32, workerPath: '/dev/null' });
  logger.record('I', 'a');
  const result = logger.verifyIntegrity();
  assert(result.valid === true, 'verifyIntegrity returns valid for chain');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 32, workerPath: '/dev/null' });
  const snap1 = logger.snapshot();
  logger.record('I', 'after snapshot');
  const result = logger.verifyIntegrity();
  assert(result.valid === true, 'integrity holds after snapshot-read');
  logger.close();
})();

// ── mTLS & JWT Tests ──
console.log('\u001b[1m--- mTLS & JWT Verification ---\u001b[0m');

(function() {
  const guard = new mTLSJwtGuard();
  assert(guard !== null, 'mTLSJwtGuard created without error');
  guard.close();
})();

(function() {
  const { publicKey, privateKey } = crypto.generateKeyPairSync('rsa', { modulusLength: 2048 });
  const header = { alg: 'RS256', typ: 'JWT' };
  const payload = { sub: 'test', exp: Math.floor(Date.now() / 1000) + 3600, jti: crypto.randomUUID() };
  const headerB64 = Buffer.from(JSON.stringify(header)).toString('base64url');
  const payloadB64 = Buffer.from(JSON.stringify(payload)).toString('base64url');
  const sign = crypto.createSign('RSA-SHA256');
  sign.update(headerB64 + '.' + payloadB64);
  sign.end();
  const signature = sign.sign(privateKey, 'base64url');
  const token = headerB64 + '.' + payloadB64 + '.' + signature;

  const guard = new mTLSJwtGuard();
  const result = guard.verifyJWT(token, publicKey.export({ type: 'spki', format: 'pem' }));
  assert(result.verified === true, 'valid RS256 JWT verified');
  assert(result.payload.sub === 'test', 'JWT payload sub matches');
  guard.close();
})();

(function() {
  const { publicKey, privateKey } = crypto.generateKeyPairSync('rsa', { modulusLength: 2048 });
  const header = { alg: 'RS256', typ: 'JWT' };
  const payload = { sub: 'expired', exp: Math.floor(Date.now() / 1000) - 3600, jti: crypto.randomUUID() };
  const headerB64 = Buffer.from(JSON.stringify(header)).toString('base64url');
  const payloadB64 = Buffer.from(JSON.stringify(payload)).toString('base64url');
  const sign = crypto.createSign('RSA-SHA256');
  sign.update(headerB64 + '.' + payloadB64);
  sign.end();
  const signature = sign.sign(privateKey, 'base64url');
  const token = headerB64 + '.' + payloadB64 + '.' + signature;

  const guard = new mTLSJwtGuard();
  let threw = false;
  try {
    guard.verifyJWT(token, publicKey.export({ type: 'spki', format: 'pem' }));
  } catch (e) {
    threw = true;
    assert(e instanceof JWTVerificationError, 'expired token throws JWTVerificationError');
    assert(e.code === 'EXPIRED', 'expired token code is EXPIRED');
  }
  assert(threw, 'expired JWT token rejected');
  guard.close();
})();

(function() {
  const { publicKey, privateKey } = crypto.generateKeyPairSync('rsa', { modulusLength: 2048 });
  const header = { alg: 'RS256', typ: 'JWT' };
  const payload = { sub: 'forge', exp: Math.floor(Date.now() / 1000) + 3600, jti: crypto.randomUUID() };
  const headerB64 = Buffer.from(JSON.stringify(header)).toString('base64url');
  const payloadB64 = Buffer.from(JSON.stringify(payload)).toString('base64url');
  const sign = crypto.createSign('RSA-SHA256');
  sign.update(headerB64 + '.' + payloadB64);
  sign.end();
  const signature = sign.sign(privateKey, 'base64url');
  const token = headerB64 + '.' + payloadB64 + '.' + signature + 'tampered';

  const guard = new mTLSJwtGuard();
  let threw = false;
  try {
    guard.verifyJWT(token, publicKey.export({ type: 'spki', format: 'pem' }));
  } catch (e) {
    threw = true;
    assert(e instanceof JWTVerificationError, 'forged token throws JWTVerificationError');
    assert(e.code === 'FORGERY' || e.code === 'ENCODING', 'forged token code is FORGERY or ENCODING');
  }
  assert(threw, 'forged JWT signature rejected');
  guard.close();
})();

(function() {
  const { publicKey, privateKey } = crypto.generateKeyPairSync('rsa', { modulusLength: 2048 });
  const jti = crypto.randomUUID();
  const header = { alg: 'RS256', typ: 'JWT' };
  const payload = { sub: 'replay', exp: Math.floor(Date.now() / 1000) + 3600, jti };
  const headerB64 = Buffer.from(JSON.stringify(header)).toString('base64url');
  const payloadB64 = Buffer.from(JSON.stringify(payload)).toString('base64url');
  const sign = crypto.createSign('RSA-SHA256');
  sign.update(headerB64 + '.' + payloadB64);
  sign.end();
  const signature = sign.sign(privateKey, 'base64url');
  const token = headerB64 + '.' + payloadB64 + '.' + signature;

  const guard = new mTLSJwtGuard();
  const r1 = guard.verifyJWT(token, publicKey.export({ type: 'spki', format: 'pem' }));
  assert(r1.verified === true, 'first use of jti accepted');

  let threw = false;
  try {
    guard.verifyJWT(token, publicKey.export({ type: 'spki', format: 'pem' }));
  } catch (e) {
    threw = true;
    assert(e instanceof JWTVerificationError, 'replay throws JWTVerificationError');
    assert(e.code === 'REPLAY', 'replay code is REPLAY');
  }
  assert(threw, 'replay attack (jti reused) rejected');
  guard.close();
})();

(function() {
  const guard = new mTLSJwtGuard();
  let threw = false;
  try {
    guard.verifyJWT('invalid.token', null);
  } catch (e) {
    threw = true;
    assert(e instanceof JWTVerificationError, 'malformed token throws error');
  }
  assert(threw, 'malformed JWT rejected');
  guard.close();
})();

(function() {
  const guard = new mTLSJwtGuard();
  const status = guard.checkCertificateExpiry();
  assert(status.valid === false, 'certificate expiry check returns invalid when no cert loaded');
  guard.close();
})();

(function() {
  const guard = new mTLSJwtGuard();
  let threw = false;
  try {
    guard.verifyTLSPeer(null);
  } catch (e) {
    threw = true;
    assert(e instanceof JWTVerificationError, 'null peer cert throws JWTVerificationError');
  }
  assert(threw, 'mTLS peer verification rejects null cert');
  guard.close();
})();

(function() {
  const { publicKey, privateKey } = crypto.generateKeyPairSync('ed25519');
  const header = { alg: 'Ed25519', typ: 'JWT' };
  const payload = { sub: 'ed25519-test', exp: Math.floor(Date.now() / 1000) + 3600, jti: crypto.randomUUID() };
  const headerB64 = Buffer.from(JSON.stringify(header)).toString('base64url');
  const payloadB64 = Buffer.from(JSON.stringify(payload)).toString('base64url');
  const signedContent = headerB64 + '.' + payloadB64;
  const signature = crypto.sign(null, Buffer.from(signedContent), privateKey);
  const token = headerB64 + '.' + payloadB64 + '.' + signature.toString('base64url');

  const guard = new mTLSJwtGuard();
  const result = guard.verifyJWT(token, publicKey.export({ type: 'spki', format: 'pem' }));
  assert(result.verified === true, 'valid Ed25519 JWT verified');
  guard.close();
})();

(function() {
  const guard = new mTLSJwtGuard();
  const opts = guard.getTLSOptions();
  assert(opts.rejectUnauthorized === false, 'TLS options default to no-auth when no certs loaded');
  guard.close();
})();

(function() {
  const guard = new mTLSJwtGuard({ maxJtiEntries: 5, jtiCleanupInterval: 50 });
  const { publicKey, privateKey } = crypto.generateKeyPairSync('rsa', { modulusLength: 2048 });
  for (let i = 0; i < 8; i++) {
    const header = { alg: 'RS256', typ: 'JWT' };
    const payload = { sub: 'jti-cleanup', exp: Math.floor(Date.now() / 1000) + 3600, jti: 'jti-' + i };
    const headerB64 = Buffer.from(JSON.stringify(header)).toString('base64url');
    const payloadB64 = Buffer.from(JSON.stringify(payload)).toString('base64url');
    const sign = crypto.createSign('RSA-SHA256');
    sign.update(headerB64 + '.' + payloadB64);
    sign.end();
    const sig = sign.sign(privateKey, 'base64url');
    guard.verifyJWT(headerB64 + '.' + payloadB64 + '.' + sig, publicKey.export({ type: 'spki', format: 'pem' }));
  }
  assert(true, '8 unique jti tokens accepted');
  guard.close();
})();

// ── Capability Sandboxing Tests ──
console.log('\u001b[1m--- Capability Sandboxing ---\u001b[0m');

(function() {
  const guard = new CapabilityGuard();
  assert(guard !== null, 'CapabilityGuard created without error');
})();

(function() {
  const guard = new CapabilityGuard();
  const perms = guard.getPermissions('SAFE');
  assert(perms.length === 0, 'SAFE mode has zero permissions by default');
})();

(function() {
  const guard = new CapabilityGuard();
  assert(guard.hasPermission('SAFE', CAPABILITIES.FILE_READ) === false, 'SAFE cannot FILE_READ by default');
  assert(guard.hasPermission('SAFE', CAPABILITIES.NET_CONNECT) === false, 'SAFE cannot NET_CONNECT by default');
})();

(function() {
  const guard = new CapabilityGuard();
  guard.grantPermission('SAFE', CAPABILITIES.FILE_READ);
  assert(guard.hasPermission('SAFE', CAPABILITIES.FILE_READ) === true, 'SAFE can FILE_READ after grant');
})();

(function() {
  const guard = new CapabilityGuard();
  guard.grantPermission('SAFE', CAPABILITIES.FILE_READ);
  guard.revokePermission('SAFE', CAPABILITIES.FILE_READ);
  assert(guard.hasPermission('SAFE', CAPABILITIES.FILE_READ) === false, 'SAFE loses FILE_READ after revoke');
})();

(function() {
  const guard = new CapabilityGuard();
  assert(guard.hasPermission('BALANCED', CAPABILITIES.FILE_READ) === true, 'BALANCED has FILE_READ');
  assert(guard.hasPermission('BALANCED', CAPABILITIES.NET_CONNECT) === true, 'BALANCED has NET_CONNECT');
  assert(guard.hasPermission('BALANCED', CAPABILITIES.FILE_WRITE) === false, 'BALANCED lacks FILE_WRITE');
})();

(function() {
  const guard = new CapabilityGuard();
  assert(guard.hasPermission('FAST', CAPABILITIES.FILE_WRITE) === true, 'FAST has FILE_WRITE');
  assert(guard.hasPermission('FAST', CAPABILITIES.NET_CONNECT) === true, 'FAST has NET_CONNECT');
})();

(function() {
  const guard = new CapabilityGuard();
  assert(guard.hasPermission('PERSISTENT', CAPABILITIES.NET_LISTEN) === true, 'PERSISTENT has NET_LISTEN');
})();

(function() {
  const guard = new CapabilityGuard();
  assertThrows(() => guard.checkPermission('SAFE', CAPABILITIES.FILE_READ, '/etc/passwd'), 'SAFE FILE_READ throws CapabilityViolationError');
})();

(function() {
  const guard = new CapabilityGuard();
  guard.grantPermission('SAFE', CAPABILITIES.FILE_READ);
  assertNotThrows(() => guard.checkPermission('SAFE', CAPABILITIES.FILE_READ, '/tmp/test'), 'SAFE FILE_READ allowed after grant');
})();

(function() {
  const guard = new CapabilityGuard();
  assertThrows(() => guard.enforceSandbox('SAFE', 'execve', '/bin/sh'), 'SAFE execve throws SIGSYS');
  assertThrows(() => guard.enforceSandbox('SAFE', 'ptrace', null), 'SAFE ptrace throws SIGSYS');
})();

(function() {
  const guard = new CapabilityGuard();
  assertNotThrows(() => guard.enforceSandbox('BALANCED', CAPABILITIES.FILE_READ, '/etc/config'), 'BALANCED FILE_READ allowed');
})();

(function() {
  const guard = new CapabilityGuard();
  let violated = false;
  guard.onViolation((severity, msg) => { violated = true; });
  try { guard.checkPermission('SAFE', CAPABILITIES.NET_CONNECT, 'example.com:80'); } catch (e) {}
  assert(violated, 'violation hook fires on denial');
})();

(function() {
  const guard = new CapabilityGuard();
  guard.resetToDefaults();
  const perms = guard.getPermissions('SAFE');
  assert(perms.length === 0, 'resetToDefaults clears SAFE permissions');
})();

(function() {
  const guard = new CapabilityGuard();
  guard.grantPermission('SAFE', CAPABILITIES.FILE_READ);
  guard.grantPermission('SAFE', CAPABILITIES.NET_CONNECT);
  const perms = guard.getPermissions('SAFE');
  assert(perms.includes('FILE_READ'), 'SAFE permissions include FILE_READ');
  assert(perms.includes('NET_CONNECT'), 'SAFE permissions include NET_CONNECT');
})();

(function() {
  const guard = new CapabilityGuard();
  guard.grantPermission('SMART', CAPABILITIES.FILE_WRITE);
  guard.grantPermission('SMART', CAPABILITIES.NET_LISTEN);
  assert(guard.hasPermission('SMART', CAPABILITIES.FILE_WRITE) === true, 'SMART can FILE_WRITE after grant');
  assert(guard.hasPermission('SMART', CAPABILITIES.NET_LISTEN) === true, 'SMART can NET_LISTEN after grant');
})();

(function() {
  const guard = new CapabilityGuard();
  assert(guard.hasPermission('SMART', CAPABILITIES.FILE_READ) === true, 'SMART has FILE_READ by default');
})();

(function() {
  const guard = new CapabilityGuard();
  let msg = '';
  guard.onViolation((severity, message) => { msg = message; });
  try { guard.checkPermission('SAFE', CAPABILITIES.PROCESS_SPAWN, null); } catch (e) {}
  assert(msg.includes('CRITICAL'), 'violation message contains CRITICAL');
})();

(function() {
  const guard = new CapabilityGuard();
  assertThrows(() => guard.checkPermission('UNKNOWN_MODE', CAPABILITIES.FILE_READ, '/tmp'), 'unknown mode throws');
})();

// ── Integration Tests ──
console.log('\u001b[1m--- Integration Tests ---\u001b[0m');

(function() {
  const guard = new CapabilityGuard();
  let violations = [];
  guard.onViolation((sev, msg, detail) => violations.push({ sev, msg, detail }));
  try { guard.checkPermission('SAFE', CAPABILITIES.NET_CONNECT, 'internal:8443'); } catch (e) {}
  assert(violations.length === 1, 'violation hook captures one event');
  assert(violations[0].detail.mode === 'SAFE', 'violation detail has mode');
  assert(violations[0].detail.capability === 'NET_CONNECT', 'violation detail has capability');
})();

(function() {
  const { publicKey, privateKey } = crypto.generateKeyPairSync('rsa', { modulusLength: 2048 });
  const header = { alg: 'RS256', typ: 'JWT' };
  const payload = { sub: 'no-exp', jti: crypto.randomUUID() };
  const headerB64 = Buffer.from(JSON.stringify(header)).toString('base64url');
  const payloadB64 = Buffer.from(JSON.stringify(payload)).toString('base64url');
  const sign = crypto.createSign('RSA-SHA256');
  sign.update(headerB64 + '.' + payloadB64);
  sign.end();
  const sig = sign.sign(privateKey, 'base64url');
  const token = headerB64 + '.' + payloadB64 + '.' + sig;

  const guard = new mTLSJwtGuard();
  const result = guard.verifyJWT(token, publicKey.export({ type: 'spki', format: 'pem' }));
  assert(result.verified === true, 'JWT without exp still accepted (no expiry check if absent)');
  guard.close();
})();

(function() {
  const guard = new mTLSJwtGuard();
  const opts = guard.getTLSOptions();
  assert(opts !== null, 'getTLSOptions returns object');
  guard.close();
})();

(function() {
  const guard = new CapabilityGuard();
  const perms = guard.getPermissions('FAST');
  assert(perms.includes('FILE_READ'), 'FAST has FILE_READ');
  assert(perms.includes('FILE_WRITE'), 'FAST has FILE_WRITE');
  assert(perms.includes('NET_CONNECT'), 'FAST has NET_CONNECT');
})();

(function() {
  const guard = new CapabilityGuard();
  const p1 = guard.getPermissions('PERSISTENT');
  assert(p1.includes('NET_LISTEN'), 'PERSISTENT has NET_LISTEN');
  assert(p1.includes('FILE_WRITE'), 'PERSISTENT has FILE_WRITE');
})();

(function() {
  const guard = new CapabilityGuard();
  guard.grantPermission('SAFE', CAPABILITIES.FILE_READ);
  guard.grantPermission('SAFE', CAPABILITIES.FILE_READ);
  assert(guard.hasPermission('SAFE', CAPABILITIES.FILE_READ) === true, 'duplicate grant is idempotent');
})();

(function() {
  const guard = new CapabilityGuard();
  guard.revokePermission('SAFE', CAPABILITIES.FILE_READ);
  guard.revokePermission('SAFE', CAPABILITIES.FILE_READ);
  assert(true, 'duplicate revoke is no-op');
})();

(function() {
  const guard = new CapabilityGuard();
  assertNotThrows(() => guard.checkPermission('BALANCED', CAPABILITIES.FILE_READ, '/config'), 'BALANCED FILE_READ allowed');
})();

(function() {
  const guard = new CapabilityGuard();
  assertThrows(() => guard.checkPermission('BALANCED', CAPABILITIES.PROCESS_SPAWN, '/bin/sh'), 'BALANCED PROCESS_SPAWN denied');
})();

// ── Benchmark Suite ──
console.log('\u001b[1m--- Audit Logger Benchmark Suite ---\u001b[0m');

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 10000, workerPath: '/dev/null' });
  const start = process.hrtime.bigint();
  for (let i = 0; i < 1000; i++) logger.record('I', 'benchmark record ' + i);
  const end = process.hrtime.bigint();
  const elapsedMs = Number(end - start) / 1e6;
  const perRecordUs = (elapsedMs * 1000) / 1000;
  assert(perRecordUs < 100, 'auditLogger.record() overhead < 100µs per record (' + perRecordUs.toFixed(2) + 'µs)');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 10000, workerPath: '/dev/null' });
  const start = Date.now();
  for (let i = 0; i < 100; i++) {
    logger.record('I', 'bench ' + i);
    logger.snapshot();
  }
  const elapsed = Date.now() - start;
  assert(elapsed < 200, '100 record+snapshot cycles in < 200ms');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 10000, workerPath: '/dev/null' });
  const start = Date.now();
  for (let i = 0; i < 50; i++) logger.verifyIntegrity();
  const elapsed = Date.now() - start;
  assert(elapsed < 200, '50 verifyIntegrity calls in < 200ms');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 10000, workerPath: '/dev/null' });
  for (let i = 0; i < 5000; i++) logger.record('I', 'stress ' + i);
  const snap = logger.snapshot();
  assert(snap.metrics.length > 0, '5000 records produce non-empty snapshot');
  logger.close();
})();

(function() {
  const logger = new NonBlockingAuditLogger({ ringSize: 10000, workerPath: '/dev/null' });
  const start = Date.now();
  for (let i = 0; i < 10000; i++) logger.record('I', 'bulk ' + i);
  const elapsed = Date.now() - start;
  assert(elapsed < 500, '10000 records in < 500ms');
  logger.close();
})();

console.log('');
console.log('\u001b[1mResult: ' + passed + ' passed, ' + failed + ' failed\u001b[0m');
if (failed > 0) process.exit(1);
