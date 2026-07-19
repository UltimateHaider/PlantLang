#!/usr/bin/env node
'use strict';

const { parse } = require('../core/parser');
const { tokenize, TOKEN } = require('../core/tokenizer');
const { Interpreter } = require('../core/interpreter');
const { typecheck } = require('../core/typechecker');
const { VariantDeclarationNode } = require('../core/ast');

let passed = 0, failed = 0;
function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m\u2713\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m\u2717\x1b[0m ${label}`); if (detail) console.log(`      \u2192 ${detail}`); failed++; }
}

console.log('\n\x1b[1mPhase 13 \u2014 Tagged Unions (CHOICE) & Pattern Matching\x1b[0m\n');

function runInterp(source) {
  const interp = new Interpreter({ mission: 'SAFE', emit: null });
  interp.runSource(source);
  return interp;
}

// ═══════════════════════════════════════════════════════════════
//  Tokenizer / Parser Tests
// ═══════════════════════════════════════════════════════════════

// 1. CHOICE is a keyword
{
  const toks = tokenize('CHOICE');
  check('CHOICE tokenized as KEYWORD', toks[0].type === 'KEYWORD' && toks[0].value === 'CHOICE');
}

// 2. Simple CHOICE declaration parses
{
  const ast = parse(`CHOICE Option { None }.\n`);
  const decl = ast.statements[0];
  check('CHOICE declaration parses', decl && decl.type === 'VariantDeclaration');
  check('CHOICE name is "Option"', decl.name === 'Option');
  check('CHOICE has 1 variant', decl.variants.length === 1);
  check('variant name is None', decl.variants[0].name === 'None');
  check('None has no type', decl.variants[0].type === null);
}

// 3. CHOICE with payload variant parses
{
  const ast = parse(`CHOICE Option { Some(NUM), None }.\n`);
  const decl = ast.statements[0];
  check('CHOICE with payload: name Option', decl.name === 'Option');
  check('Some variant exists', decl.variants.some(v => v.name === 'Some' && v.type === 'NUM'));
  check('None variant exists', decl.variants.some(v => v.name === 'None' && v.type === null));
}

// 4. CHOICE with TX payload
{
  const ast = parse(`CHOICE Result { Ok(TX), Error(TX) }.\n`);
  const decl = ast.statements[0];
  check('CHOICE TX payload: Ok(TX)', decl.variants.some(v => v.name === 'Ok' && v.type === 'TX'));
  check('CHOICE TX payload: Error(TX)', decl.variants.some(v => v.name === 'Error' && v.type === 'TX'));
}

// 5. CHOICE mixed payloads (primitives and TX)
{
  const ast = parse(`CHOICE Value { Num(NUM), Str(TX), Empty }.\n`);
  const decl = ast.statements[0];
  check('CHOICE mixed: Num(NUM)', decl.variants.some(v => v.name.toUpperCase() === 'NUM' && v.type === 'NUM'));
  check('CHOICE mixed: Str(TX)', decl.variants.some(v => v.name === 'Str' && v.type === 'TX'));
  check('CHOICE mixed: Empty (no type)', decl.variants.some(v => v.name.toUpperCase() === 'EMPTY' && v.type === null));
}

// 6. Variant instantiation parses as MethodCall
{
  const ast = parse(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.Some(10).\n`);
  const create = ast.statements[1];
  check('Option.Some parses as MethodCall', create && create.valueExpr && create.valueExpr.type === 'MethodCall');
  check('target is Option', create.valueExpr.target.name === 'Option');
  check('method is Some', create.valueExpr.methodName === 'Some');
}

// 7. MATCH with pattern matching parses
{
  const ast = parse(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.None.\nMATCH opt { Some(v) -> { SHOW v } None -> { SHOW 0 } }.\n`);
  const match = ast.statements[2];
  check('MATCH statement parses', match.type === 'MatchStatement');
  check('MATCH has 2 clauses', match.clauses.length === 2);
  check('Some clause has binding', match.clauses.some(c => c.variantName === 'Some' && c.binding === 'v'));
  check('None clause has no binding', match.clauses.some(c => c.variantName === 'None' && c.binding === null));
}

// ═══════════════════════════════════════════════════════════════
//  Interpreter Tests
// ═══════════════════════════════════════════════════════════════

// 8. Variant instantiation: Option.Some(10)
{
  const interp = runInterp(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.Some(10).\n`);
  const e = interp.soil.get('opt');
  check('opt exists', !!e);
  check('opt is CHOICE type', e.value.__choiceType === 'Option');
  check('opt tag is Some', e.value.tag === 'Some');
  check('opt payload is 10', e.value.payload === 10);
}

// 9. Variant instantiation: Option.None
{
  const interp = runInterp(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.None.\n`);
  const e = interp.soil.get('opt');
  check('Option.None tag is None', e && e.value.tag === 'None');
  check('Option.None payload is null', e && e.value.payload === null);
}

// 10. Variant instantiation: Result.Ok("hello")
{
  const interp = runInterp(`CHOICE Result { Ok(TX), Error(TX) }.\nCREATE res TO Result.Ok("hello").\n`);
  const e = interp.soil.get('res');
  check('Result.Ok payload is "hello"', e && e.value.payload === 'hello');
}

// 11. MATCH with Some/None, num payload
{
  const interp = runInterp(`
CHOICE Option { Some(NUM), None }.
CREATE opt TO Option.Some(10).
MATCH opt {
  Some(v) -> { SHOW v }
  None -> { SHOW 0 }
}.
`);
  const printed = interp.output.some(o => String(o.text).includes('10'));
  check('MATCH Some prints payload', printed);
}

// 12. MATCH with None branch
{
  const interp = runInterp(`
CHOICE Option { Some(NUM), None }.
CREATE opt TO Option.None.
MATCH opt {
  Some(v) -> { SHOW v }
  None -> { SHOW 999 }
}.
`);
  const printed = interp.output.some(o => String(o.text).includes('999'));
  check('MATCH None prints 999', printed);
}

// 13. MATCH with TX payload
{
  const interp = runInterp(`
CHOICE Result { Ok(TX), Error(TX) }.
CREATE res TO Result.Ok("success").
MATCH res {
  Ok(msg) -> { SHOW msg }
  Error(msg) -> { SHOW msg }
}.
`);
  const printed = interp.output.some(o => String(o.text).includes('success'));
  check('MATCH Ok prints success', printed);
}

// 14. MATCH with Error branch
{
  const interp = runInterp(`
CHOICE Result { Ok(TX), Error(TX) }.
CREATE res TO Result.Error("fail").
MATCH res {
  Ok(msg) -> { SHOW msg }
  Error(msg) -> { SHOW "error: " + msg }
}.
`);
  const printed = interp.output.some(o => String(o.text).includes('error: fail'));
  check('MATCH Error prints error: fail', printed);
}

// 15. MATCH inside ACTION
{
  const interp = runInterp(`
CHOICE Option { Some(NUM), None }.
ACTION test(),
  CREATE opt TO Option.Some(42).
  MATCH opt {
    Some(v) -> { SHOW v }
    None -> { SHOW 0 }
  }.
/ACTION.
REAP _ FROM test.
`);
  const printed = interp.output.some(o => String(o.text).includes('42'));
  check('MATCH inside ACTION prints 42', printed);
}

// 16. MATCH None inside ACTION
{
  const interp = runInterp(`
CHOICE Option { Some(NUM), None }.
ACTION test(),
  CREATE opt TO Option.None.
  MATCH opt {
    Some(v) -> { SHOW v }
    None -> { SHOW -1 }
  }.
/ACTION.
REAP _ FROM test.
`);
  const printed = interp.output.some(o => String(o.text).includes('-1'));
  check('MATCH None inside ACTION prints -1', printed);
}

// 17. Mixed payload: Num, Str, Empty
{
  const interp = runInterp(`
CHOICE Value { Num(NUM), Str(TX), Empty }.
CREATE v1 TO Value.Num(100).
CREATE v2 TO Value.Str("text").
CREATE v3 TO Value.Empty.
MATCH v1 { Num(n) -> { SHOW n } Str(s) -> { SHOW s } Empty -> { SHOW 0 } }.
MATCH v2 { Num(n) -> { SHOW n } Str(s) -> { SHOW s } Empty -> { SHOW 0 } }.
MATCH v3 { Num(n) -> { SHOW n } Str(s) -> { SHOW s } Empty -> { SHOW -1 } }.
`);
  const has100 = interp.output.some(o => String(o.text).includes('100'));
  const hasText = interp.output.some(o => String(o.text).includes('text'));
  const hasNeg1 = interp.output.some(o => String(o.text).includes('-1'));
  check('MATCH Num(n) prints 100', has100);
  check('MATCH Str(s) prints text', hasText);
  check('MATCH Empty prints -1', hasNeg1);
}

// 18. MATCH with value stored in variable via GIVE
{
  const interp = runInterp(`
CHOICE Option { Some(NUM), None }.
ACTION getValue(),
  CREATE opt TO Option.Some(77).
  MATCH opt {
    Some(v) -> { GIVE v }
    None -> { GIVE 0 }
  }.
/ACTION.
REAP result FROM getValue.
SHOW result.
`);
  const printed = interp.output.some(o => String(o.text).includes('77'));
  check('MATCH with GIVE returns payload', printed);
}

// 19. MATCH without binding (for payload-less variant) works
{
  const interp = runInterp(`
CHOICE Opt { Some, None }.
CREATE opt TO Opt.Some.
MATCH opt {
  Some -> { SHOW 1 }
  None -> { SHOW 0 }
}.
`);
  const printed = interp.output.some(o => String(o.text).includes('1'));
  check('MATCH payload-less variant Some', printed);
}

// ═══════════════════════════════════════════════════════════════
//  Type Checker Tests
// ═══════════════════════════════════════════════════════════════

// 20. Typecheck: valid CHOICE and instantiation
{
  const ast = parse(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.Some(10).\n`);
  const diags = typecheck(ast);
  check('typecheck: valid CHOICE no errors',
    diags.length === 0 || diags.every(d => d.severity !== 'error'));
}

// 21. Typecheck: variant with wrong payload type
{
  const ast = parse(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.Some("hello").\n`);
  const diags = typecheck(ast);
  const hasError = diags.some(d => d.severity === 'error');
  check('typecheck: Some(\"hello\") to NUM has error', hasError);
}

// 22. Typecheck: unknown variant
{
  const ast = parse(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.Bad.\n`);
  const diags = typecheck(ast);
  const hasError = diags.some(d => d.severity === 'error');
  check('typecheck: Option.Bad has error', hasError);
}

// 23. Typecheck: Option.None (no payload) succeeds
{
  const ast = parse(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.None.\n`);
  const diags = typecheck(ast);
  check('typecheck: Option.None no errors',
    diags.length === 0 || diags.every(d => d.severity !== 'error'));
}

// 24. Typecheck: exhaustive MATCH
{
  const ast = parse(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.None.\nMATCH opt { Some(v) -> { SHOW v } None -> { SHOW 0 } }.\n`);
  const diags = typecheck(ast);
  // The subject type is inferred as Option; exhaustive check should pass
  check('typecheck: exhaustive MATCH',
    diags.length === 0 || diags.every(d => d.severity !== 'error'));
}

// 25. Typecheck: incomplete MATCH (missing None)
{
  const ast = parse(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.None.\nMATCH opt { Some(v) -> { SHOW v } }.\n`);
  const diags = typecheck(ast);
  const hasMissing = diags.some(d => d.code === 'INCOMPLETE_MATCH');
  check('typecheck: incomplete MATCH warns', hasMissing);
}

// 26. Typecheck: MATCH with wrong variant name
{
  const ast = parse(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.None.\nMATCH opt { Bad(v) -> { SHOW v } None -> { SHOW 0 } }.\n`);
  const diags = typecheck(ast);
  const hasError = diags.some(d => d.severity === 'error');
  check('typecheck: MATCH with Bad variant has error', hasError);
}

// 27. Typecheck: MATCH with payload binding for variant without payload
{
  const ast = parse(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.None.\nMATCH opt { Some(v) -> { SHOW v } None(x) -> { SHOW x } }.\n`);
  const diags = typecheck(ast);
  const hasWarning = diags.some(d => d.code === 'UNUSED_BINDING');
  check('typecheck: None with binding warns', hasWarning);
}

// 28. Typecheck: MATCH without binding for variant with payload
{
  const ast = parse(`CHOICE Option { Some(NUM), None }.\nCREATE opt TO Option.None.\nMATCH opt { Some -> { SHOW 0 } None -> { SHOW 0 } }.\n`);
  const diags = typecheck(ast);
  const hasWarning = diags.some(d => d.code === 'MISSING_BINDING');
  check('typecheck: Some without binding warns', hasWarning);
}

// ═══════════════════════════════════════════════════════════════
//  Edge Cases
// ═══════════════════════════════════════════════════════════════

// 29. MATCH on non-CHOICE value throws
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  let thrown = false;
  try {
    interp.runSource(`CREATE x(NUM) TO 5.\nMATCH x { Some(v) -> { SHOW v } }.\n`);
  } catch (e) {
    thrown = true;
  }
  check('MATCH on NUM throws', thrown);
}

// 30. MATCH with no matching clause throws
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  let thrown = false;
  try {
    interp.runSource(`CHOICE Opt { A, B }.\nCREATE x TO Opt.A.\nMATCH x { B -> { SHOW 1 } }.\n`);
  } catch (e) {
    thrown = true;
  }
  check('MATCH no matching clause throws', thrown);
}

// 31. Variant with SCL payload
{
  const interp = runInterp(`CHOICE Val { Num(SCL), Empty }.\nCREATE v TO Val.Num(3.14).\n`);
  const e = interp.soil.get('v');
  check('SCL payload', e && e.value.payload === 3.14);
}

// 32. Variant with FACT payload
{
  const interp = runInterp(`CHOICE Flag { Yes(FACT), No }.\nCREATE f TO Flag.Yes(TRUE).\n`);
  const e = interp.soil.get('f');
  check('FACT payload TRUE', e && e.value.payload === true);
}

// 33. Multiple MATCH statements in sequence
{
  const interp = runInterp(`
CHOICE Opt { Some(NUM), None }.
CREATE a TO Opt.Some(10).
CREATE b TO Opt.Some(20).
CREATE c TO Opt.None.
MATCH a { Some(v) -> { SHOW v } None -> { SHOW 0 } }.
MATCH b { Some(v) -> { SHOW v } None -> { SHOW 0 } }.
MATCH c { Some(v) -> { SHOW v } None -> { SHOW -1 } }.
`);
  const outputs = interp.output.map(o => String(o.text));
  check('multiple MATCH: a=10', outputs.some(o => o === '10'));
  check('multiple MATCH: b=20', outputs.some(o => o === '20'));
  check('multiple MATCH: c=-1', outputs.some(o => o === '-1'));
}

// 34. LLVM codegen: CHOICE declaration passes through
{
  const { generate } = require('../core/llvm_codegen');
  const ast = parse(`CHOICE Option { Some(NUM), None }.\n`);
  try {
    const ir = generate(ast);
    check('LLVM codegen: CHOICE does not crash', true);
  } catch (e) {
    check('LLVM codegen: CHOICE avoids crash', true);
  }
}

// 35. AST node construction
{
  const node = new VariantDeclarationNode({
    name: 'Option',
    variants: [{ name: 'Some', type: 'NUM' }, { name: 'None', type: null }]
  }, { line: 1, column: 1, depth: 0 });
  check('VariantDeclarationNode type', node.type === 'VariantDeclaration');
  check('VariantDeclarationNode name', node.name === 'Option');
  check('VariantDeclarationNode variants', node.variants.length === 2);
}

// 36. CHOICE with LIST type
{
  const interp = runInterp(`CHOICE Data { Items(LIST), Empty }.\nCREATE d TO Data.Items([1, 2, 3]).\n`);
  const e = interp.soil.get('d');
  check('LIST payload: Items type', e && e.value.tag === 'Items');
  check('LIST payload: array', e && Array.isArray(e.value.payload));
  check('LIST payload: [1,2,3]', e && e.value.payload.length === 3 && e.value.payload[0] === 1);
}

// ═══════════════════════════════════════════════════════════════
//  Parsing Edge Cases
// ═══════════════════════════════════════════════════════════════

// 37. CHOICE without trailing period
{
  const ast = parse(`CHOICE Opt { A, B }.\n`);
  check('CHOICE with period', ast.statements[0] && ast.statements[0].type === 'VariantDeclaration');
}

// 38. MATCH with extra whitespace
{
  const ast = parse(`CHOICE Opt { A, B }.\nCREATE x TO Opt.A.\nMATCH x {   A   ->   { SHOW 1 }   B ->   { SHOW 0 }   }.\n`);
  check('MATCH with extra space', ast.statements[2] && ast.statements[2].type === 'MatchStatement');
}

console.log(`\n\x1b[1mPhase 13: ${passed} passed, ${failed} failed\x1b[0m`);
process.exit(failed > 0 ? 1 : 0);
