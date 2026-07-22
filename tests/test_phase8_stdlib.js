#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const { parse, parseFile } = require('../core/parser');
const { Interpreter } = require('../core/interpreter');

let passed = 0, failed = 0;
function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m✓\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m✗\x1b[0m ${label}`); if (detail) console.log(`      → ${detail}`); failed++; }
}

console.log('\n\x1b[1mPhase 8 — Standard Library Foundation\u001b[0m\n');

// ── 1. std/ path resolution ──
{
  const { resolveImports, ProgramNode } = require('../core/parser');
  const { ImportStatementNode } = require('../core/ast');
  // Create a program with an IMPORT "std/io" statement and resolve it
  const prog = parse('IMPORT "std/io".\nSHOW 1.\n');
  const stmt = prog.statements[0];
  check('std/io import parsed', stmt.type === 'ImportStatement' && stmt.path === 'std/io');
}

// ── 2. std/io.plnt file exists and parses ──
{
  const ioPath = path.join(__dirname, '..', 'std', 'io.plnt');
  const exists = fs.existsSync(ioPath);
  check('std/io.plnt exists', exists);
  if (exists) {
    const prog = parseFile(ioPath);
    check('std/io.plnt parses', prog.statements.length > 0);
    const hasPrintf = prog.statements.some(s => s.type === 'ActionDeclaration' && s.name === 'plant_printf');
    const hasPutchar = prog.statements.some(s => s.type === 'ActionDeclaration' && s.name === 'plant_puts');
    const hasPrint = prog.statements.some(s => s.type === 'ActionDeclaration' && s.name === 'print');
    const hasPrintln = prog.statements.some(s => s.type === 'ActionDeclaration' && s.name === 'println');
    check('plant_printf declared', hasPrintf);
    check('plant_puts declared', hasPutchar);
    check('print wrapper exists', hasPrint);
    check('println wrapper exists', hasPrintln);
  }
}

// ── 3. std/string.plnt exists and parses ──
{
  const strPath = path.join(__dirname, '..', 'std', 'string.plnt');
  const exists = fs.existsSync(strPath);
  check('std/string.plnt exists', exists);
  if (exists) {
    const prog = parseFile(strPath);
    check('std/string.plnt parses', prog.statements.length > 0);
    const hasConcat = prog.statements.some(s => s.type === 'ActionDeclaration' && s.name === 'concat');
    const hasSubstr = prog.statements.some(s => s.type === 'ActionDeclaration' && s.name === 'substring');
    check('concat action declared', hasConcat);
    check('substring action declared', hasSubstr);
  }
}

// ── 4. std/prelude.plnt exists and parses ──
{
  const preludePath = path.join(__dirname, '..', 'std', 'prelude.plnt');
  check('std/prelude.plnt exists', fs.existsSync(preludePath));
  if (fs.existsSync(preludePath)) {
    const prog = parseFile(preludePath);
    check('std/prelude.plnt parses', prog.statements.length > 0);
  }
}

// ── 5. Interpreter: print via std/io ──
{
  const interp = new Interpreter({ output: [] });
  const src = `
IMPORT "std/io".
ACTION test_print(),
  REAP _ FROM print, "Hello from std/io!".
/ACTION.
1\\ REAP _ FROM test_print.
`;
  interp.runSource(src);
  const printed = interp.output.some(o => String(o.text).includes('Hello from std/io!'));
  check('print outputs via interpreter', printed, interp.output.map(o=>o.text).join('|'));
}

// ── 6. Interpreter: println via std/io ──
{
  const interp = new Interpreter({ output: [] });
  const src = `
IMPORT "std/io".
ACTION test_println(),
  REAP _ FROM println, "line1".
  REAP _ FROM println, "line2".
/ACTION.
1\\ REAP _ FROM test_println.
`;
  interp.runSource(src);
  const hasLine1 = interp.output.some(o => String(o.text).includes('line1'));
  const hasLine2 = interp.output.some(o => String(o.text).includes('line2'));
  check('println outputs line1', hasLine1);
  check('println outputs line2', hasLine2);
}

// ── 7. Interpreter: concat via std/string ──
{
  const interp = new Interpreter({ output: [] });
  const src = `
IMPORT "std/string".
ACTION test_concat(),
  CREATE a(TX) TO "Hello, ".
  CREATE b(TX) TO "World!".
  REAP result FROM concat, a, b.
  SHOW result.
/ACTION.
1\\ REAP _ FROM test_concat.
`;
  interp.runSource(src);
  const printed = interp.output.some(o => String(o.text).includes('Hello, World!'));
  check('concat outputs correct string', printed);
}

// ── 8. Interpreter: substring via std/string ──
{
  const interp = new Interpreter({ output: [] });
  const src = `
IMPORT "std/string".
ACTION test_substring(),
  CREATE t(TX) TO "Hello World".
  REAP result FROM substring, t, 0, 5.
  SHOW result.
/ACTION.
1\\ REAP _ FROM test_substring.
`;
  interp.runSource(src);
  const printed = interp.output.some(o => String(o.text).includes('Hello'));
  check('substring extracts "Hello"', printed);
}

// ── 9. Interpreter: substring mid-string ──
{
  const interp = new Interpreter({ output: [] });
  const src = `
IMPORT "std/string".
ACTION test_substring2(),
  CREATE t(TX) TO "Hello World".
  REAP result FROM substring, t, 6, 11.
  SHOW result.
/ACTION.
1\\ REAP _ FROM test_substring2.
`;
  interp.runSource(src);
  const printed = interp.output.some(o => String(o.text).includes('World'));
  check('substring extracts "World"', printed);
}

// ── 10. Combined io + string ──
{
  const interp = new Interpreter({ output: [] });
  const src = `
IMPORT "std/io".
IMPORT "std/string".
ACTION test_combined(),
  CREATE a(TX) TO "ABC".
  CREATE b(TX) TO "DEF".
  REAP combined FROM concat, a, b.
  REAP _ FROM print, combined.
/ACTION.
1\\ REAP _ FROM test_combined.
`;
  interp.runSource(src);
  const printed = interp.output.some(o => String(o.text).includes('ABCDEF'));
  check('combined io+string outputs correctly', printed);
}

// ── 11. FFI external registered in funcs ──
{
  const interp = new Interpreter();
  const src = `
IMPORT "std/io".
ACTION test_check(), REAP _ FROM plant_puts, "x". /ACTION.
`;
  interp.runSource(src);
  const hasFunc = interp.funcs.has('plant_printf') && interp.funcs.has('plant_puts');
  check('external FFI registered in funcs', hasFunc);
}

// ── 12. Auto-prelude ──
{
  const testDir = path.join(__dirname, 'phase8_tmp_prelude');
  try { fs.rmSync(testDir, { recursive: true }); } catch(e) {}
  fs.mkdirSync(testDir, { recursive: true });
  try {
    // A file that uses print() WITHOUT explicitly importing std/io
    // (the prelude in parseFile should auto-inject it)
    fs.writeFileSync(path.join(testDir, 'test_prelude.plnt'), `
ACTION greet(name(TX)),
  REAP _ FROM print, "Hello, ".
  REAP _ FROM println, name.
/ACTION.
1\\ REAP _ FROM greet, "World".
`);
    const interp = new Interpreter({ output: [] });
    interp.runFile(path.join(testDir, 'test_prelude.plnt'));
    const hasHello = interp.output.some(o => String(o.text).includes('Hello,'));
    const hasWorld = interp.output.some(o => String(o.text).includes('World'));
    check('auto-prelude: print works', hasHello);
    check('auto-prelude: println works', hasWorld);
  } finally {
    try { fs.rmSync(testDir, { recursive: true }); } catch(e) {}
  }
}

// ── 13. FFI stub details — plant_printf function ──
{
  const interp = new Interpreter();
  check('plant_printf stub registered', typeof interp._externalFFI.get('plant_printf') === 'function');
  check('plant_puts stub registered', typeof interp._externalFFI.get('plant_puts') === 'function');
  check('plant_flush stub registered', typeof interp._externalFFI.get('plant_flush') === 'function');
}

// ── 14. concat returns correct value ──
{
  const interp = new Interpreter({ output: [] });
  const src = `
IMPORT "std/string".
ACTION test_concat2(),
  REAP result FROM concat, "foo", "bar".
  SHOW result.
/ACTION.
1\\ REAP _ FROM test_concat2.
`;
  interp.runSource(src);
  const printed = interp.output.some(o => String(o.text).includes('foobar'));
  check('concat "foo"+"bar" = "foobar"', printed);
}

// ── 15. substring with end beyond length ──
{
  const interp = new Interpreter({ output: [] });
  const src = `
IMPORT "std/string".
ACTION test_sub3(),
  REAP result FROM substring, "hello", 0, 100.
  SHOW result.
/ACTION.
1\\ REAP _ FROM test_sub3.
`;
  interp.runSource(src);
  const printed = interp.output.some(o => String(o.text).includes('hello'));
  check('substring with end beyond length', printed);
}

// ── Summary ──
console.log(`\n\x1b[1mResults: ${passed} passed, ${failed} failed\x1b[0m\n`);
process.exit(failed > 0 ? 1 : 0);
