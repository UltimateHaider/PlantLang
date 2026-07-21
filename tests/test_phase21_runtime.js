'use strict';
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const { parse } = require('../core/parser');
const { generate } = require('../core/llvm_codegen');

let passed = 0, failed = 0, skipped = 0;

function findLLC() {
  for (const bin of ['llc', 'llc-18', 'llc-17', 'llc-16', 'llc-15', 'llc-14']) {
    try { execFileSync(bin, ['--version'], { stdio: 'pipe' }); return bin; } catch (_) {}
  }
  return null;
}

const LLC_BIN = findLLC();

function compileAndRun(name, source) {
  const prog = parse(source);
  const { ir, errors } = generate(prog);
  if (errors.length) throw new Error('codegen errors: ' + errors.map(e => e.message).join('; '));

  const tmpBase = path.join('/tmp', 'plnt_rt_test_' + name + '_' + Date.now());
  const llPath = tmpBase + '.ll';
  const sPath = tmpBase + '.s';
  const binPath = tmpBase;

  fs.writeFileSync(llPath, ir, 'utf8');
  execFileSync(LLC_BIN, [llPath, '-O2', '-o', sPath], { stdio: 'pipe' });
  const runtimeLibDir = path.join(__dirname, '..', 'runtime');
  execFileSync('gcc', [sPath, '-no-pie', '-L' + runtimeLibDir, '-Wl,-rpath,' + runtimeLibDir, '-lplantlang', '-lm', '-o', binPath], { stdio: 'pipe' });
  const out = execFileSync(binPath, [], { encoding: 'utf8' });

  fs.unlinkSync(llPath);
  fs.unlinkSync(sPath);
  fs.unlinkSync(binPath);
  return out.trim();
}

function irContains(source, expectedSubstring) {
  const prog = parse(source);
  const { ir, errors } = generate(prog);
  if (errors.length) throw new Error('codegen errors: ' + errors.map(e => e.message).join('; '));
  return ir.includes(expectedSubstring);
}

function test(name, source, expectedOutput) {
  if (!LLC_BIN) {
    console.log(`  ${name} ... \x1b[33m⊘ skipped (no llc found)\x1b[0m`);
    skipped++;
    return;
  }
  process.stdout.write(`  ${name} ... `);
  try {
    const out = compileAndRun(name.replace(/[^a-zA-Z0-9]/g, '_'), source);
    if (out === expectedOutput) {
      console.log('\x1b[32m✓\x1b[0m');
      passed++;
    } else {
      console.log('\x1b[31m✗\x1b[0m');
      console.log('    expected:', JSON.stringify(expectedOutput));
      console.log('    got:     ', JSON.stringify(out));
      failed++;
    }
  } catch (e) {
    console.log('\x1b[31m✗ (error)\x1b[0m');
    console.log('   ', e.message.split('\n')[0]);
    failed++;
  }
}

function testIR(name, source, expectedIR) {
  process.stdout.write(`  ${name} ... `);
  try {
    if (irContains(source, expectedIR)) {
      console.log('\x1b[32m✓\x1b[0m');
      passed++;
    } else {
      console.log('\x1b[31m✗\x1b[0m');
      console.log('    expected IR to contain:', JSON.stringify(expectedIR));
      failed++;
    }
  } catch (e) {
    console.log('\x1b[31m✗ (error)\x1b[0m');
    console.log('   ', e.message.split('\n')[0]);
    failed++;
  }
}

console.log('Phase 21 — C Runtime FFI');
console.log(LLC_BIN ? `  (using ${LLC_BIN})\n` : '  (llc not found — execution tests will be skipped)\n');

// ── Codegen IR smoke tests (no llc needed) ────────────────────────────
testIR('IR: declares sqrt as double(double)', `
MISSION: SAFE.
1\\ ACTION sqrt(x(SCL)) -> external.
1\\ CREATE x(SCL) TO 0.
1\\ REAP x FROM sqrt, x.
`, 'declare double @sqrt(double)');

testIR('IR: declares plnt_string_len as i64(%fat_ptr)', `
MISSION: SAFE.
1\\ ACTION plnt_string_len(s(TX)) -> external.
1\\ CREATE s(TX) TO "".
1\\ CREATE n(NUM) TO 0.
1\\ REAP n FROM plnt_string_len, s.
`, 'declare i64 @plnt_string_len(%fat_ptr)');

testIR('IR: uses NATIVE ACTION syntax', `
MISSION: SAFE.
1\\ NATIVE ACTION sqrt(x(SCL)) -> external.
1\\ CREATE x(SCL) TO 9.
1\\ CREATE r(SCL) TO 0.
1\\ REAP r FROM sqrt, x.
`, 'declare double @sqrt(double)');

// ── Execution tests (require llc + runtime library) ──────────────────
test('sqrt(9.0) = 3', `
MISSION: SAFE.
ACTION sqrt(x(SCL)) -> external.
ACTION floor(x(SCL)) -> external.
1\\ CREATE x(SCL) TO 9.0.
1\\ CREATE r(SCL) TO 0.
1\\ REAP r FROM sqrt, x.
1\\ SHOW r.
`, '3');

test('floor(3.7) = 3', `
MISSION: SAFE.
ACTION floor(x(SCL)) -> external.
ACTION sqrt(x(SCL)) -> external.
1\\ CREATE x(SCL) TO 3.7.
1\\ CREATE r(SCL) TO 0.
1\\ REAP r FROM floor, x.
1\\ SHOW r.
`, '3');

test('ceil(3.2) = 4', `
MISSION: SAFE.
ACTION ceil(x(SCL)) -> external.
1\\ CREATE x(SCL) TO 3.2.
1\\ CREATE r(SCL) TO 0.
1\\ REAP r FROM ceil, x.
1\\ SHOW r.
`, '4');

test('abs(-2.5) = 2.5', `
MISSION: SAFE.
ACTION fabs(x(SCL)) -> external.
1\\ CREATE x(SCL) TO -2.5.
1\\ CREATE r(SCL) TO 0.
1\\ REAP r FROM fabs, x.
1\\ SHOW r.
`, '2.5');

// ── SORT execution tests ──────────────────────────────────────────
testIR('IR: SORT [NUM] emits plnt_sort_i64 declare', `
MISSION: SAFE.
1\\ CREATE arr([NUM]) TO [3, 1, 2].
1\\ SORT(arr).
`, 'declare void @plnt_sort_i64(i8*, i64)');

test('SORT [NUM] array in-place', `
MISSION: SAFE.
1\\ CREATE arr([NUM]) TO [3, 1, 2].
1\\ SORT(arr).
1\\ SHOW FIRST(arr).
1\\ SHOW LAST(arr).
1\\ SHOW COUNT(arr).
`, '1\n3\n3');

test('SORT [SCL] array in-place', `
MISSION: SAFE.
1\\ CREATE arr([SCL]) TO [3.0, 1.5, 2.0].
1\\ SORT(arr).
1\\ SHOW FIRST(arr).
1\\ SHOW LAST(arr).
`, '1.5\n3');

// ── String split/join tests ────────────────────────────────────────────
testIR('IR: SPLIT emits plnt_str_split declare', `
MISSION: SAFE.
ACTION plnt_str_split(s(TX), d(TX)) -> external.
1\\ CREATE s(TX) TO "a,b".
1\\ CREATE d(TX) TO ",".
1\\ CREATE r([TX]) TO [""].
1\\ REAP r FROM plnt_str_split, s, d.
`, 'declare void @plnt_str_split(%fat_ptr*, i8*, i64, i64, i8*, i64, i64)');

testIR('IR: JOIN emits plnt_str_join declare', `
MISSION: SAFE.
ACTION plnt_str_join(parts(TX), d(TX)) -> external.
1\\ CREATE parts(TX) TO "".
1\\ CREATE d(TX) TO ",".
1\\ CREATE r(TX) TO "".
1\\ REAP r FROM plnt_str_join, parts, d.
`, 'declare void @plnt_str_join(%fat_ptr*, i8*, i64, i64, i8*, i64, i64)');

test('JOIN ["a","b","c"] by "," -> "a,b,c"', `
MISSION: SAFE.
ACTION plnt_str_join(parts(TX), d(TX)) -> external.
1\\ CREATE parts([TX]) TO ["a", "b", "c"].
1\\ CREATE d(TX) TO ",".
1\\ CREATE r(TX) TO "".
1\\ REAP r FROM plnt_str_join, parts, d.
1\\ SHOW r.
`, 'a,b,c');

test('JOIN ["hello","world"] by " " -> "hello world"', `
MISSION: SAFE.
ACTION plnt_str_join(parts(TX), d(TX)) -> external.
1\\ CREATE parts([TX]) TO ["hello", "world"].
1\\ CREATE d(TX) TO " ".
1\\ CREATE r(TX) TO "".
1\\ REAP r FROM plnt_str_join, parts, d.
1\\ SHOW r.
`, 'hello world');

// ── Native SPLIT/JOIN via REAP (expression sources) ──────────
testIR('IR: native SPLIT via REAP emits plnt_str_split declare', `
MISSION: SAFE.
1\\ REAP parts FROM SPLIT("a,b", ",").
`, 'declare void @plnt_str_split(%fat_ptr*, i8*, i64, i64, i8*, i64, i64)');

testIR('IR: native JOIN via REAP emits plnt_str_join declare', `
MISSION: SAFE.
1\\ REAP parts FROM SPLIT("a,b", ",").
1\\ REAP joined FROM JOIN(parts, ":").
`, 'declare void @plnt_str_join(%fat_ptr*, i8*, i64, i64, i8*, i64, i64)');

test('native SPLIT via REAP + COUNT', `
MISSION: SAFE.
1\\ REAP parts FROM SPLIT("a,b,c", ",").
1\\ SHOW COUNT(parts).
`, '3');

test('native SPLIT→JOIN roundtrip via REAP', `
MISSION: SAFE.
1\\ REAP parts FROM SPLIT("hello,world,test", ",").
1\\ REAP joined FROM JOIN(parts, "-").
1\\ SHOW joined.
`, 'hello-world-test');

test('native SPLIT + index access via REAP', `
MISSION: SAFE.
1\\ REAP parts FROM SPLIT("apple,banana", ",").
1\\ REAP first FROM parts[0].
1\\ SHOW first.
`, 'apple');

// ── Large-string stress test (~70KB split/join roundtrip) ──────
test('stress test: 70KB string split/join roundtrip', `
MISSION: SAFE.
ACTION plnt_stress_test_split_join() -> external.
1\\ REAP _ FROM plnt_stress_test_split_join.
`, 'Stress test: input len=70000, expected parts=6364\nSplit count: 6364\nJoined length: 70000\nPASS: stress test roundtrip OK');

console.log(`\nPhase 21: ${passed} passed, ${failed} failed, ${skipped} skipped`);
process.exit(failed > 0 ? 1 : 0);
