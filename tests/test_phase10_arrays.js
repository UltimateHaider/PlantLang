#!/usr/bin/env node
'use strict';

const { parse, parseFile } = require('../core/parser');
const { tokenize, TOKEN } = require('../core/tokenizer');
const { Interpreter } = require('../core/interpreter');
const { typecheck } = require('../core/typechecker');
const {
  ArrayLiteralNode, ProgramNode, CreateStatementNode,
  IdentifierNode, LiteralNode, IndexAccessNode, LenCallNode, CapCallNode,
} = require('../core/ast');

let passed = 0, failed = 0;
function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m✓\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m✗\x1b[0m ${label}`); if (detail) console.log(`      → ${detail}`); failed++; }
}

console.log('\n\x1b[1mPhase 10 — Dynamic Arrays [NUM], [TX], [SCL], [FACT]\x1b[0m\n');

// ── 1. Tokenizer: [ and ] punctuation ──
{
  const toks = tokenize('[ 10 , 20 ]');
  check('[ tokenized as PUNCT', toks[0].type === TOKEN.PUNCT && toks[0].value === '[');
  check('] tokenized as PUNCT', toks[4].type === TOKEN.PUNCT && toks[4].value === ']');
  check(', tokenized as PUNCT', toks[2].type === TOKEN.PUNCT && toks[2].value === ',');
}

// ── 2. Parser: CREATE with [NUM] type ──
{
  const ast = parse('CREATE nums([NUM]) TO [10, 20, 30].\n');
  const stmt = ast.statements[0];
  check('CREATE with [NUM] type parses', stmt.type === 'CreateStatement');
  check('varType is [NUM]', stmt.varType === '[NUM]');
  check('valueExpr is ArrayLiteral', stmt.valueExpr && stmt.valueExpr.type === 'ArrayLiteral');
  check('3 elements', stmt.valueExpr && stmt.valueExpr.elements && stmt.valueExpr.elements.length === 3);
  check('first element 10', stmt.valueExpr && stmt.valueExpr.elements && stmt.valueExpr.elements[0].value === 10);
}

// ── 3. Parser: CREATE with [TX] type ──
{
  const ast = parse('CREATE words([TX]) TO ["hello", "world"].\n');
  const stmt = ast.statements[0];
  check('CREATE with [TX] type', stmt.varType === '[TX]');
  check('2 string elements', stmt.valueExpr && stmt.valueExpr.elements && stmt.valueExpr.elements.length === 2);
  check('first element "hello"', stmt.valueExpr.elements[0].value === 'hello');
}

// ── 4. Parser: CREATE with [SCL] type ──
{
  const ast = parse('CREATE vals([SCL]) TO [1.5, 2.5, 3.5].\n');
  const stmt = ast.statements[0];
  check('varType is [SCL]', stmt.varType === '[SCL]');
  check('3 SCL elements', stmt.valueExpr && stmt.valueExpr.elements && stmt.valueExpr.elements.length === 3);
}

// ── 5. Parser: CREATE with [FACT] type ──
{
  const ast = parse('CREATE flags([FACT]) TO [TRUE, FALSE, TRUE].\n');
  const stmt = ast.statements[0];
  check('varType is [FACT]', stmt.varType === '[FACT]');
  check('3 FACT elements (stored as identifiers)', stmt.valueExpr && stmt.valueExpr.elements && stmt.valueExpr.elements.length === 3);
}

// ── 6. Parser: empty array ──
{
  const ast = parse('CREATE arr([NUM]) TO [].\n');
  const stmt = ast.statements[0];
  check('empty array parses', stmt.valueExpr && stmt.valueExpr.type === 'ArrayLiteral');
  check('0 elements', stmt.valueExpr && stmt.valueExpr.elements && stmt.valueExpr.elements.length === 0);
}

// ── 7. Parser: single element array ──
{
  const ast = parse('CREATE arr([NUM]) TO [42].\n');
  const stmt = ast.statements[0];
  check('single element', stmt.valueExpr && stmt.valueExpr.elements && stmt.valueExpr.elements.length === 1);
  check('element is 42', stmt.valueExpr.elements[0].value === 42);
}

// ── 8. Parser: CREATE without TO (default) with array type ──
{
  const ast = parse('CREATE arr([NUM]).\n');
  const stmt = ast.statements[0];
  check('CREATE [NUM] without TO', stmt.varType === '[NUM]');
  check('valueExpr is null', stmt.valueExpr === null);
}

// ── 9. Interpreter: CREATE [NUM] array ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource('CREATE nums([NUM]) TO [10, 20, 30].\n');
  const e = interp.soil.get('nums');
  check('nums defined', !!e);
  check('type is [NUM]', e.type === '[NUM]');
  check('value is array', Array.isArray(e.value));
  check('length 3', e.value.length === 3);
  check('nums[0] is 10', e.value[0] === 10);
  check('nums[1] is 20', e.value[1] === 20);
  check('nums[2] is 30', e.value[2] === 30);
}

// ── 10. Interpreter: CREATE [TX] array ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource('CREATE words([TX]) TO ["foo", "bar"].\n');
  const e = interp.soil.get('words');
  check('words defined', !!e);
  check('words[0] is "foo"', e.value[0] === 'foo');
  check('words[1] is "bar"', e.value[1] === 'bar');
}

// ── 11. Interpreter: CREATE [SCL] array with mixed NUM/SCL ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource('CREATE vals([SCL]) TO [1, 2.5, 3].\n');
  const e = interp.soil.get('vals');
  check('vals[0] is 1', e.value[0] === 1);
  check('vals[1] is 2.5', e.value[1] === 2.5);
  check('vals[2] is 3', e.value[2] === 3);
}

// ── 12. Interpreter: empty array ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource('CREATE arr([NUM]) TO [].\n');
  const e = interp.soil.get('arr');
  check('empty array defined', !!e);
  check('empty array has value', Array.isArray(e.value));
  check('empty array length 0', e.value.length === 0);
}

// ── 13. Type checker: array type declared ──
{
  const ast = parse('CREATE nums([NUM]) TO [10, 20, 30].\n');
  const diags = typecheck(ast);
  check('typecheck: [NUM] array no errors', diags.length === 0 || diags.every(d => d.severity !== 'error'));
}

// ── 14. Type checker: [TX] array ──
{
  const ast = parse('CREATE words([TX]) TO ["a", "b"].\n');
  const diags = typecheck(ast);
  check('typecheck: [TX] array no errors', diags.length === 0 || diags.every(d => d.severity !== 'error'));
}

// ── 15. Parser: IndexAccess on array variable in expression ──
{
  const ast = parse('SHOW nums[0].\n');
  const stmt = ast.statements[0];
  check('parser handles nums[0] expression', stmt.type === 'ShowStatement');
}

// ── 16. Interpreter: array of NUM values ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource('CREATE items([NUM]) TO [1, 2, 3].\n');
  const e = interp.soil.get('items');
  check('items array defined', !!e);
  check('items[0] is 1', e.value[0] === 1);
  check('items[2] is 3', e.value[2] === 3);
}

// ── 17. Multiple array declarations ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource(`
CREATE a([NUM]) TO [1, 2].
CREATE b([TX]) TO ["x", "y"].
`);
  const ea = interp.soil.get('a');
  const eb = interp.soil.get('b');
  check('a[0] is 1', ea.value[0] === 1);
  check('b[1] is "y"', eb.value[1] === 'y');
}

// ── 18. Array with FACT values ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource('CREATE flags([FACT]) TO [TRUE, FALSE].\n');
  const e = interp.soil.get('flags');
  check('FACT array length 2', e.value.length === 2);
  check('TRUE is truthy', e.value[0] === true);
  check('FALSE is falsy', e.value[1] === false);
}

// ── 19. Array type string format ──
{
  const ast = parse('CREATE arr([NUM]) TO [5, 10].\n');
  check('array type string format', ast.statements[0].varType === '[NUM]');
}

// ── 20. Interpreter: array container with creation message ──
{
  const output = [];
  const interp = new Interpreter({
    mission: 'SAFE',
    emit: (text, type) => {
      const cleaned = (text || '').replace(/\x1b\[[0-9;]*m/g, '');
      if (type === 'ok') output.push(cleaned);
    }
  });
  interp.runSource('CREATE arr([NUM]) TO [1, 2, 3].\n');
  check('CREATE output mentions [NUM]', output.some(l => l.includes('[NUM]')));
}

// ── 21. Array with various integer values ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource('CREATE vals([NUM]) TO [0, 100, -5, 999].\n');
  const e = interp.soil.get('vals');
  check('vals[0] is 0', e.value[0] === 0);
  check('vals[2] is -5', e.value[2] === -5);
  check('vals[3] is 999', e.value[3] === 999);
}

// ── 22. CREATE [NUM] without TO — default path ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource('CREATE arr([NUM]).\n');
  const e = interp.soil.get('arr');
  check('arr defined', !!e);
  check('arr type is [NUM]', e.type === '[NUM]');
}

// ── 23. Array with variable identifier elements ──
{
  const interp = new Interpreter({ mission: 'SAFE', emit: () => {} });
  interp.runSource(`
CREATE x(NUM) TO 5.
CREATE arr([NUM]) TO [x, 10].
`);
  const e = interp.soil.get('arr');
  check('array with variable refs', !!e && Array.isArray(e.value));
  // Variable identifiers in array literals — evaluated at runtime
}

// ── 24. Parser: [TX] with string elements ──
{
  const ast = parse('CREATE data([TX]) TO ["hello", "world"].\n');
  const stmt = ast.statements[0];
  check('[TX] string array parsed', stmt.varType === '[TX]');
}

// ── 25. Type checker: [SCL] array ──
{
  const ast = parse('CREATE vals([SCL]) TO [1.0, 2.0, 3.0].\n');
  const diags = typecheck(ast);
  check('typecheck: [SCL] array no errors', diags.length === 0 || diags.every(d => d.severity !== 'error'));
}

console.log(`\n\x1b[1mPhase 10: ${passed} passed, ${failed} failed\x1b[0m`);
process.exit(failed > 0 ? 1 : 0);
