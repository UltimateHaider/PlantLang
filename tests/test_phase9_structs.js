#!/usr/bin/env node
'use strict';

const { parse, parseFile } = require('../core/parser');
const { tokenize, TOKEN } = require('../core/tokenizer');
const { Interpreter } = require('../core/interpreter');
const { typecheck } = require('../core/typechecker');
const {
  StructDeclarationNode, StructInstantiationExpr, MemberAccessNode,
  ProgramNode, CreateStatementNode, SetStatementNode, IdentifierNode, LiteralNode
} = require('../core/ast');

let passed = 0, failed = 0;
function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m✓\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m✗\x1b[0m ${label}`); if (detail) console.log(`      → ${detail}`); failed++; }
}

console.log('\n\x1b[1mPhase 9 — User-Defined Structs (SHAPE)\x1b[0m\n');

// ── 1. Tokenizer: SHAPE keyword ──
{
  const toks = tokenize('SHAPE');
  check('SHAPE tokenized as KEYWORD', toks[0].type === TOKEN.KEYWORD && toks[0].value === 'SHAPE');
}
{
  const toks = tokenize('{ }');
  check('{ tokenized as PUNCT', toks[0].type === TOKEN.PUNCT && toks[0].value === '{');
  check('} tokenized as PUNCT', toks[1].type === TOKEN.PUNCT && toks[1].value === '}');
}

// ── 2. Parser: SHAPE declaration ──
{
  const ast = parse('SHAPE Point { x(NUM), y(NUM), label(TX) }.\n');
  const stmt = ast.statements[0];
  check('SHAPE produces StructDeclaration node', stmt.type === 'StructDeclaration');
  check('struct name is Point', stmt.name === 'Point');
  check('3 fields defined', stmt.fields.length === 3);
  check('first field x(NUM)', stmt.fields[0].name === 'x' && stmt.fields[0].varType === 'NUM');
  check('second field y(NUM)', stmt.fields[1].name === 'y' && stmt.fields[1].varType === 'NUM');
  check('third field label(TX)', stmt.fields[2].name === 'label' && stmt.fields[2].varType === 'TX');
}

// ── 3. Parser: SHAPE with FACT and SCL fields ──
{
  const ast = parse('SHAPE Config { enabled(FACT), ratio(SCL), name(TX) }.\n');
  const stmt = ast.statements[0];
  check('Config struct parsed', stmt.type === 'StructDeclaration' && stmt.name === 'Config');
  check('enabled(FACT)', stmt.fields[0].name === 'enabled' && stmt.fields[0].varType === 'FACT');
  check('ratio(SCL)', stmt.fields[1].name === 'ratio' && stmt.fields[1].varType === 'SCL');
}

// ── 4. Parser: CREATE with struct type (IDENT as type) ──
{
  const ast = parse('SHAPE Point { x(NUM), y(NUM) }.\nCREATE p(Point) TO Point{ 10, 20 }.\n');
  const createStmt = ast.statements[1];
  check('CREATE with struct type parses', createStmt.type === 'CreateStatement');
  check('varType is Point', createStmt.varType === 'Point');
  check('valueExpr is StructInstantiation', createStmt.valueExpr.type === 'StructInstantiation');
  check('struct name is Point', createStmt.valueExpr.structName === 'Point');
  check('2 constructor args', createStmt.valueExpr.args.length === 2);
}

// ── 5. Parser: Member access (IDENT . IDENT) ──
{
  const ast = parse('1\\ SHOW p.x.\n');
  // SHOW expr will be RAW_EXPR since we handle the full expression
  const stmt = ast.statements[0];
  if (stmt.type === 'ShowStatement' && stmt.expr.type === 'MemberAccess') {
    const ma = stmt.expr;
    check('MemberAccess produced for p.x', ma.type === 'MemberAccess');
    check('member is "x"', ma.member === 'x');
    check('object is identifier "p"', ma.object.type === 'Identifier' && ma.object.name === 'p');
  } else {
    check('MemberAccess for p.x', false, `Got type ${stmt.type} expr ${stmt.expr && stmt.expr.type}`);
  }
}

// ── 6. Parser: SHAPE with empty body ──
{
  const ast = parse('SHAPE Empty { }.\n');
  const stmt = ast.statements[0];
  check('Empty struct declaration', stmt.type === 'StructDeclaration' && stmt.fields.length === 0);
}

// ── 7. Interpreter: struct declaration ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource('SHAPE Point { x(NUM), y(NUM) }.\n');
  check('struct Point registered', interp.structs.has('Point'));
  const fields = interp.structs.get('Point');
  check('Point has 2 fields', fields.length === 2);
  check('Point has field x(NUM)', fields[0].name === 'x' && fields[0].varType === 'NUM');
  check('Point has field y(NUM)', fields[1].name === 'y' && fields[1].varType === 'NUM');
}

// ── 8. Interpreter: struct instantiation ──
{
  const output = [];
  const interp = new Interpreter({
    mission: 'SAFE',
    emit: (text) => { if (text !== undefined) output.push(String(text).replace(/\x1b\[[0-9;]*m/g, '')); }
  });
  interp.runSource(`
SHAPE Point { x(NUM), y(NUM) }.
1\\ CREATE p(Point) TO Point{ 10, 20 }.
  `);
  const soilEntry = interp.soil.get('p');
  check('p is defined', !!soilEntry);
  check('p type is Point', soilEntry.type === 'Point');
  check('p.x is 10', soilEntry.value && soilEntry.value.x === 10);
  check('p.y is 20', soilEntry.value && soilEntry.value.y === 20);
}

// ── 9. Interpreter: struct with TX field ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource(`
SHAPE Labeled { id(NUM), name(TX) }.
1\\ CREATE item(Labeled) TO Labeled{ 1, "hello" }.
  `);
  const entry = interp.soil.get('item');
  check('item defined', !!entry);
  check('item.id is 1', entry.value && entry.value.id === 1);
  check('item.name is "hello"', entry.value && entry.value.name === 'hello');
}

// ── 10. Interpreter: SHOW struct instance (interpreter) ──
{
  const output = [];
  const interp = new Interpreter({
    mission: 'SAFE',
    emit: (text, type) => {
      const cleaned = String(text || '').replace(/\x1b\[[0-9;]*m/g, '');
      if (type === 'inf' || type === undefined) output.push(cleaned);
    }
  });
  interp.runSource(`
SHAPE Point { x(NUM), y(NUM) }.
1\\ CREATE p(Point) TO Point{ 10, 20 }.
1\\ SHOW p.x.
  `);
  check('p.x shows 10', output.some(l => l.includes('10')));
}

// ── 11. Interpreter: SET struct field ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource(`
SHAPE Point { x(NUM), y(NUM) }.
1\\ CREATE p(Point) TO Point{ 10, 20 }.
1\\ SET p.x TO 99.
  `);
  const entry = interp.soil.get('p');
  check('p.x updated to 99', entry.value && entry.value.x === 99);
  check('p.y unchanged at 20', entry.value && entry.value.y === 20);
}

// ── 12. Interpreter: struct with all types ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource(`
SHAPE Full { n(NUM), s(SCL), t(TX), f(FACT) }.
1\\ CREATE item(Full) TO Full{ 42, 3.14, "text", TRUE }.
  `);
  const entry = interp.soil.get('item');
  check('Full.n is 42', entry.value && entry.value.n === 42);
  check('Full.s is 3.14', entry.value && entry.value.s === 3.14);
  check('Full.t is "text"', entry.value && entry.value.t === 'text');
  check('Full.f is true', entry.value && entry.value.f === true);
}

// ── 13. Type checker: struct declaration ──
{
  const ast = parse('SHAPE Point { x(NUM), y(NUM) }.\n');
  const diags = typecheck(ast);
  check('typecheck: SHAPE declaration produces no errors', diags.length === 0);
}

// ── 14. Type checker: struct instantiation type match ──
{
  const ast = parse('SHAPE Point { x(NUM), y(NUM) }.\nCREATE p(Point) TO Point{ 10, 20 }.\n');
  const diags = typecheck(ast);
  check('typecheck: valid struct instantiation no errors', diags.length === 0);
}

// ── 15. Type checker: struct instantiation wrong type name ──
{
  const ast = parse('SHAPE Point { x(NUM), y(NUM) }.\nCREATE p(Wrong) TO Point{ 10, 20 }.\n');
  const diags = typecheck(ast);
  // Struct type name "Wrong" won't match, but typechecker will just treat Wrong as UNKNOWN
  // It should at least not crash
  check('typecheck: unknown struct type does not crash', true);
}

// ── 16. Type checker: struct field access ──
{
  const ast = parse('SHAPE Point { x(NUM), y(NUM) }.\nCREATE p(Point) TO Point{ 10, 20 }.\nSHOW p.x.\n');
  const diags = typecheck(ast);
  check('typecheck: member access no errors', diags.length === 0 || diags.every(d => d.severity !== 'error'));
}

// ── 17. Parsing: member access in SET target ──
{
  const ast = parse('SHAPE P { x(NUM) }.\nCREATE p(P) TO P{ 5 }.\nSET p.x TO 42.\n');
  const setStmt = ast.statements[2];
  check('SET with p.x produces isMemberAccess', setStmt.isMemberAccess === true);
  check('memberObject is p', setStmt.memberObject === 'p');
  check('memberField is x', setStmt.memberField === 'x');
}

// ── 18. Tokenizer: full SHAPE with braces ──
{
  const toks = tokenize('SHAPE Point { x(NUM) }.\n');
  const keywords = toks.filter(t => t.type === TOKEN.KEYWORD);
  const puncts = toks.filter(t => t.type === TOKEN.PUNCT);
  check('SHAPE keyword present', keywords.some(k => k.value === 'SHAPE'));
  check('{ punct present', puncts.some(p => p.value === '{'));
  check('} punct present', puncts.some(p => p.value === '}'));
}

// ── 19. Interpreter: struct with multiple instances ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource(`
SHAPE Item { id(NUM), name(TX) }.
1\\ CREATE a(Item) TO Item{ 1, "apple" }.
1\\ CREATE b(Item) TO Item{ 2, "banana" }.
  `);
  const a = interp.soil.get('a');
  const b = interp.soil.get('b');
  check('a.id is 1', a.value && a.value.id === 1);
  check('b.id is 2', b.value && b.value.id === 2);
  check('a.name is apple', a.value && a.value.name === 'apple');
  check('b.name is banana', b.value && b.value.name === 'banana');
}

// ── 20. SHAPE with no fields ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource('SHAPE Blank { }.\n1\\ CREATE e(Blank) TO Blank{ }.\n');
  check('Blank struct registered', interp.structs.has('Blank'));
  const e = interp.soil.get('e');
  check('e defined', !!e);
  check('e type is Blank', e.type === 'Blank');
  check('e.value is empty object', typeof e.value === 'object' && Object.keys(e.value).filter(k => !k.startsWith('__')).length === 0);
}

// ── 21. Struct in scope ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource(`
SHAPE Point { x(NUM), y(NUM) }.
1\\ CREATE p(Point) TO Point{ 3, 7 }.
  `);
  const p = interp.soil.get('p');
  check('p.x via soil', p.value.x === 3);
  check('p.y via soil', p.value.y === 7);
}

// ── 22. Struct field SET via legacy handler works for plain SET ──
{
  // Testing that SET without member access still works
  const output = [];
  const interp = new Interpreter({
    mission: 'SAFE',
    emit: (text, type) => {
      const cleaned = String(text || '').replace(/\x1b\[[0-9;]*m/g, '');
      if (type === 'inf' || type === undefined) output.push(cleaned);
    }
  });
  interp.runSource(`
1\\ CREATE x(NUM) TO 5.
1\\ SET x TO 10.
1\\ SHOW x.
  `);
  check('regular SET still works', output.some(l => l.includes('10')));
}

// ── 23. Multiple SHAPE declarations ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource(`
SHAPE A { x(NUM) }.
SHAPE B { y(NUM) }.
1\\ CREATE a(A) TO A{ 1 }.
1\\ CREATE b(B) TO B{ 2 }.
  `);
  check('A struct registered', interp.structs.has('A'));
  check('B struct registered', interp.structs.has('B'));
  const a = interp.soil.get('a');
  const b = interp.soil.get('b');
  check('a.x is 1', a.value && a.value.x === 1);
  check('b.y is 2', b.value && b.value.y === 2);
}

// ── 24. Struct with SCL field ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource(`
SHAPE Measurement { value(SCL), unit(TX) }.
1\\ CREATE m(Measurement) TO Measurement{ 3.14, "meters" }.
  `);
  const m = interp.soil.get('m');
  check('m.value is 3.14', m.value && m.value.value === 3.14);
  check('m.unit is meters', m.value && m.value.unit === 'meters');
}

// ── 25. CreateStatementNode for struct passes through parser ──
{
  const ast = parse('SHAPE X { a(NUM), b(TX) }.\nCREATE obj(X) TO X{ 1, "two" }.\n');
  const create = ast.statements[1];
  check('CREATE struct has identifier obj', create.identifier === 'obj');
  check('CREATE struct varType X', create.varType === 'X');
  check('CREATE struct valueExpr type', create.valueExpr.type === 'StructInstantiation');
  check('StructInstantiation args[0] value 1', create.valueExpr.args[0].value === 1);
}

console.log(`\n\x1b[1mPhase 9: ${passed} passed, ${failed} failed\x1b[0m`);
process.exit(failed > 0 ? 1 : 0);
