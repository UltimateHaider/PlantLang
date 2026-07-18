#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const { tokenize, TOKEN } = require('../core/tokenizer');
const { parse, Parser, parseFile, resolveImports } = require('../core/parser');
const { Interpreter } = require('../core/interpreter');
const { ProgramNode, ImportStatementNode, ActionDeclarationNode } = require('../core/ast');

let passed = 0, failed = 0;
function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m✓\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m✗\x1b[0m ${label}`); if (detail) console.log(`      → ${detail}`); failed++; }
}

const testDir = path.join(__dirname, 'phase7_tmp');
function setup() {
  if (!fs.existsSync(testDir)) fs.mkdirSync(testDir, { recursive: true });
}
function cleanup() {
  try { fs.rmSync(testDir, { recursive: true }); } catch (e) {}
}

console.log('\n\x1b[1mPhase 7 — Module System (IMPORT) & FFI\x1b[0m\n');

// ── 1. IMPORT statement parsing ──
{
  const ast = parse('IMPORT "helpers".\nSHOW 1.\n');
  const node = ast.statements[0];
  check('ImportStatement node produced', node.type === 'ImportStatement');
  check('path captured', node.path === 'helpers');
  check('coords set', node.line === 1 && node.column === 1);
}

// ── 2. IMPORT with full .plnt extension ──
{
  const ast = parse('IMPORT "lib/utils.plnt".\n');
  const node = ast.statements[0];
  check('IMPORT with .plnt extension', node.type === 'ImportStatement' && node.path === 'lib/utils.plnt');
}

// ── 3. ACTION -> external; FFI declaration parsing ──
{
  const ast = parse('ACTION my_ffi(a(NUM), b(NUM)) -> external.\n');
  const node = ast.statements[0];
  check('FFI external node produced', node.type === 'ActionDeclaration');
  check('isExternal is true', node.isExternal === true);
  check('name captured', node.name === 'my_ffi');
  check('params captured', node.params.length === 2);
  check('param types', node.params[0].type === 'NUM' && node.params[1].type === 'NUM');
  check('no body statements', node.bodyStatements.length === 0);
}

// ── 4. Regular ACTION still works ──
{
  const src = `ACTION greet(name(TX)), SHOW name. /ACTION.
`;
  const ast = parse(src);
  const node = ast.statements[0];
  check('Regular ACTION parses', node.type === 'ActionDeclaration');
  check('isExternal is false', node.isExternal === false);
  check('has body', node.bodyStatements.length > 0);
}

// ── 5. Arrow token in tokenizer ──
{
  const tokens = tokenize('->');
  check('-> tokenized as PUNCT ->', tokens[0].type === 'PUNCT' && tokens[0].value === '->');
}

// ── 6. parseFile resolves IMPORT recursively ──
{
  setup();
  try {
    fs.writeFileSync(path.join(testDir, 'math.plnt'), `
ACTION add(a(NUM), b(NUM)),
  CREATE result(NUM) TO a + b.
  GIVE result.
/ACTION.
`);
    fs.writeFileSync(path.join(testDir, 'main.plnt'), `IMPORT "math".
SHOW 42.
`);
    const prog = parseFile(path.join(testDir, 'main.plnt'), { noPrelude: true });
    check('parseFile resolves imports', prog.statements.length === 2);
    check('imported action is present', prog.statements[0].type === 'ActionDeclaration' && prog.statements[0].name === 'add');
    check('original statement preserved', prog.statements[1].type === 'ShowStatement');
  } finally { cleanup(); }
}

// ── 7. Circular import detection ──
{
  setup();
  try {
    fs.writeFileSync(path.join(testDir, 'a.plnt'), `IMPORT "b".
`);
    fs.writeFileSync(path.join(testDir, 'b.plnt'), `IMPORT "a".
`);
    let threw = false;
    try { parseFile(path.join(testDir, 'a.plnt'), { noPrelude: true }); }
    catch (e) { threw = e.message.toLowerCase().includes('circular'); }
    check('circular import detected', threw);
  } finally { cleanup(); }
}

// ── 8. Import file not found ──
{
  setup();
  try {
    fs.writeFileSync(path.join(testDir, 'test.plnt'), `IMPORT "nonexistent".
`);
    let threw = false;
    try { parseFile(path.join(testDir, 'test.plnt'), { noPrelude: true }); }
    catch (e) { threw = e.message.toLowerCase().includes('not found'); }
    check('missing import file detected', threw);
  } finally { cleanup(); }
}

// ── 9. IMPORT chain (A imports B, B imports C) ──
{
  setup();
  try {
    fs.writeFileSync(path.join(testDir, 'c.plnt'), `ACTION greet(), SHOW "Hello from C!". /ACTION.
`);
    fs.writeFileSync(path.join(testDir, 'b.plnt'), `IMPORT "c".
ACTION wrapper(), REAP _ FROM greet. /ACTION.
`);
    fs.writeFileSync(path.join(testDir, 'a.plnt'), `IMPORT "b".
`);
    const prog = parseFile(path.join(testDir, 'a.plnt'), { noPrelude: true });
    check('transitive import resolved', prog.statements.length === 2);
    check('c.plnt action present', prog.statements[0].type === 'ActionDeclaration' && prog.statements[0].name === 'greet');
    check('b.plnt action present', prog.statements[1].type === 'ActionDeclaration' && prog.statements[1].name === 'wrapper');
  } finally { cleanup(); }
}

// ── 10. Interpreter runFile with imports ──
{
  setup();
  try {
    fs.writeFileSync(path.join(testDir, 'adder.plnt'), `
ACTION add(a(NUM), b(NUM)),
  CREATE result(NUM) TO a + b.
  GIVE result.
/ACTION.
`);
    fs.writeFileSync(path.join(testDir, 'use_adder.plnt'), `IMPORT "adder".
CREATE x(NUM) TO 10.
CREATE y(NUM) TO 20.
REAP result FROM add, x, y.
SHOW result.
`);
    const interp = new Interpreter();
    interp.runFile(path.join(testDir, 'use_adder.plnt'));
    check('interpreter runs imported action', interp.output.some(o => String(o.text).includes('30')));
  } finally { cleanup(); }
}

// ── 11. Type checker with external FFI ──
{
  const { typecheck } = require('../core/typechecker');
  const src = `ACTION external_func(a(NUM)) -> external.
ACTION internal_func(a(NUM)),
  REAP result FROM external_func, a.
  GIVE result.
/ACTION.
`;
  const prog = parse(src);
  const diags = typecheck(prog, src);
  check('FFI action passes typecheck', diags.filter(d => d.severity === 'error').length === 0,
    diags.map(d => d.message).join('; '));
}

// ── 12. LLVM codegen with FFI external ──
{
  const { generate } = require('../core/llvm_codegen');
  const src = `ACTION external_add(a(NUM), b(NUM)) -> external.
ACTION use_add(a(NUM), b(NUM)),
  REAP result FROM external_add, a, b.
  GIVE result.
/ACTION.
`;
  const prog = parse(src);
  const { ir, errors } = generate(prog);
  check('LLVM codegen succeeds', errors.length === 0);
  check('FFI declare emitted', ir.includes('declare i64 @external_add(i64 %a, i64 %b)'));
  check('define uses call', ir.includes('call i64 @external_add'));
}

// ── 13. FFI single param with TX type ──
{
  const ast = parse('ACTION process(label(TX)) -> external.\n');
  const node = ast.statements[0];
  check('FFI with TX param', node.isExternal && node.params[0].type === 'TX');
}

// ── 14. Import with absolute path ──
{
  setup();
  try {
    const absPath = path.join(testDir, 'abs_imported.plnt');
    fs.writeFileSync(absPath, `ACTION hi(), SHOW "hi". /ACTION.
`);
    const mainPath = path.join(testDir, 'abs_main.plnt');
    fs.writeFileSync(mainPath, `IMPORT "${absPath}".
`);
    const prog = parseFile(mainPath, { noPrelude: true });
    check('absolute path import works', prog.statements.length === 1);
    check('imported action correct', prog.statements[0].name === 'hi');
  } finally { cleanup(); }
}

// ── 15. Typechecker registers external FFI param types ──
{
  const checkMod = require('../core/typechecker');
  const src = `ACTION ext(a(NUM), b(TX)) -> external.
`;
  const prog = parse(src);
  const diags = checkMod.typecheck(prog, src);
  check('typechecker accepts external with multiple param types',
    diags.filter(d => d.severity === 'error').length === 0);
}

// ── 16. Interpreter FFI stub registration ──
{
  const interp = new Interpreter();
  interp._externalFFI.set('my_ffi_fn', (args) => {
    return args[0] + args[1];
  });
  const src = `ACTION my_ffi_fn(a(NUM), b(NUM)) -> external.
CREATE x(NUM) TO 3.
CREATE y(NUM) TO 4.
REAP result FROM my_ffi_fn, x, y.
SHOW result.
`;
  interp.runSource(src);
  check('interpreter calls FFI stub', interp.output.some(o => String(o.text).includes('7')));
}

// ── 17. LLVM codegen — external with zero params ──
{
  const { generate } = require('../core/llvm_codegen');
  const src = `ACTION zero() -> external.
`;
  const prog = parse(src);
  const { ir, errors } = generate(prog);
  check('zero-param FFI works', errors.length === 0);
  check('declare has no params', ir.includes('declare i64 @zero()'));
}

// ── 18. LLVM codegen — regular action still compiles correctly ──
{
  const { generate } = require('../core/llvm_codegen');
  const src = `ACTION double(a(NUM)),
  GIVE a * 2.
/ACTION.
`;
  const prog = parse(src);
  const { ir, errors } = generate(prog);
  check('regular action compiles', errors.length === 0);
  check('define emitted', ir.includes('define i64 @double'));

}

// ── 19. ImportStatementNode has correct AST type string ──
{
  const node = new ImportStatementNode({ path: 'test', resolvedPath: '/abs/test.plnt', importedStatements: [] }, { line: 1, column: 1, depth: 0 });
  check('ImportStatementNode type is ImportStatement', node.type === 'ImportStatement');
  check('path preserved', node.path === 'test');
  check('resolvedPath preserved', node.resolvedPath === '/abs/test.plnt');
}

// ── 20. Syntax error on invalid IMPORT ──
{
  const src = `IMPORT 42.
`;
  let threw = false;
  try { parse(src); }
  catch (e) { threw = e.message.toLowerCase().includes('string literal'); }
  check('IMPORT without string throws', threw);
}

// ── Summary ──
console.log(`\n\x1b[1mResults: ${passed} passed, ${failed} failed\x1b[0m\n`);
process.exit(failed > 0 ? 1 : 0);
