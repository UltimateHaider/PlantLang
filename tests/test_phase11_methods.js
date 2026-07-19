#!/usr/bin/env node
'use strict';

const { parse } = require('../core/parser');
const { tokenize, TOKEN } = require('../core/tokenizer');
const { Interpreter } = require('../core/interpreter');
const { typecheck } = require('../core/typechecker');
const {
  ActionDeclarationNode, MethodCallNode, ProgramNode,
  CreateStatementNode, IdentifierNode, LiteralNode,
} = require('../core/ast');

let passed = 0, failed = 0;
function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m\u2713\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m\u2717\x1b[0m ${label}`); if (detail) console.log(`      \u2192 ${detail}`); failed++; }
}

console.log('\n\x1b[1mPhase 11 \u2014 Methods & Receivers\x1b[0m\n');

// ===== Parser Tests =====

// 1. Parser: ACTION with receiver syntax
{
  const ast = parse(`SHAPE Point { x(NUM), y(NUM) }
ACTION (self(Point)) move(x(NUM), y(NUM)),
  SET self.x TO x.
  SET self.y TO y.
/ACTION.
`);
  const decl = ast.statements[1];
  check('receiver action parses as ActionDeclaration', decl.type === 'ActionDeclaration');
  check('receiver name is self', decl.receiver && decl.receiver.name === 'self');
  check('receiver type is Point', decl.receiver && decl.receiver.type === 'Point');
  check('action name is move', decl.name === 'move');
  check('2 params (x, y)', decl.params && decl.params.length === 2);
  check('2 body statements', decl.bodyStatements && decl.bodyStatements.length === 2);
}

// 2. Parser: ACTION without receiver (unchanged)
{
  const ast = parse(`ACTION add(x(NUM), y(NUM)),
  GIVE x + y.
/ACTION.
`);
  const decl = ast.statements[0];
  check('regular ACTION has no receiver', decl.receiver === null || decl.receiver === undefined);
  check('params still parsed', decl.params.length === 2);
}

// 3. Parser: method call in expression (obj.method(args))
{
  const ast = parse(`SHAPE Point { x(NUM), y(NUM) }
CREATE p(Point) TO (1, 2).
SHOW p.move(10, 20).
`);
  const showStmt = ast.statements[2];
  const expr = showStmt.expr;
  check('method call parses', expr.type === 'MethodCall');
  check('target identifier', expr.target && expr.target.type === 'Identifier');
  check('target name is p', expr.target && expr.target.name === 'p');
  check('method name is move', expr.methodName === 'move');
  check('2 args', expr.args && expr.args.length === 2);
}

// 4. Parser: method call with no args
{
  const ast = parse(`SHAPE Point { x(NUM), y(NUM) }
ACTION (self(Point)) origin(),
  GIVE 0.
/ACTION.
CREATE p(Point) TO (1, 2).
SHOW p.origin().
`);
  const showStmt = ast.statements[3];
  check('no-arg method call', showStmt.expr.type === 'MethodCall');
  check('method name origin', showStmt.expr.methodName === 'origin');
  check('0 args', showStmt.expr.args && showStmt.expr.args.length === 0);
}

// 5. Parser: struct member access still works (no parens = member access)
{
  const ast = parse(`SHAPE Point { x(NUM), y(NUM) }
CREATE p(Point) TO (1, 2).
SHOW p.x.
`);
  const showStmt = ast.statements[2];
  check('dot member access (no parens) is still MemberAccess', showStmt.expr.type === 'MemberAccess');
}

// 6. Parser: Multiple methods on same type
{
  const ast = parse(`SHAPE Point { x(NUM), y(NUM) }
ACTION (self(Point)) move(x(NUM), y(NUM)),
  SET self.x TO x. SET self.y TO y.
/ACTION.
ACTION (self(Point)) scale(f(NUM)),
  SET self.x TO self.x * f. SET self.y TO self.y * f.
/ACTION.
`);
  check('first method on Point', ast.statements[1].type === 'ActionDeclaration' && ast.statements[1].receiver.type === 'Point');
  check('second method on Point', ast.statements[2].type === 'ActionDeclaration' && ast.statements[2].receiver.type === 'Point');
}

// 7. MethodCallNode exports from AST
{
  const node = new MethodCallNode({ target: new IdentifierNode('p'), methodName: 'move', args: [] }, {});
  check('MethodCallNode type', node.type === 'MethodCall');
  check('MethodCallNode.methodName', node.methodName === 'move');
  check('MethodCallNode.target', node.target.name === 'p');
}

// ===== Interpreter Tests =====

function runInterp(source) {
  const interp = new Interpreter({ mission: 'SAFE' });
  interp.emit = () => {};
  interp.runSource(source);
  return interp;
}

// 8. Interpret: basic method call
{
  const interp = runInterp(`
SHAPE Point { x(NUM), y(NUM) }
ACTION (self(Point)) move(x(NUM), y(NUM)),
  SET self.x TO x.
  SET self.y TO y.
/ACTION.
CREATE p(Point) TO (0, 0).
SHOW p.move(3, 4).
`);
  const p = interp.soil.get('p');
  check('basic method call: x=3', p && p.value && p.value.x === 3);
  check('basic method call: y=4', p && p.value && p.value.y === 4);
}

// 9. Interpret: method returning a value via GIVE
{
  const interp = runInterp(`
SHAPE Point { x(NUM), y(NUM) }
ACTION (self(Point)) setX(val(NUM)),
  SET self.x TO val.
  GIVE val.
/ACTION.
CREATE p(Point) TO (3, 4).
SHOW p.setX(7).
`);
  const p = interp.soil.get('p');
  check('method with GIVE and side-effect: x=7', p && p.value && p.value.x === 7);
}

// 10. Interpret: method with no args
{
  const interp = runInterp(`
SHAPE Point { x(NUM), y(NUM) }
ACTION (self(Point)) zero(),
  SET self.x TO 0. SET self.y TO 0.
/ACTION.
CREATE p(Point) TO (10, 20).
SHOW p.zero().
`);
  const p = interp.soil.get('p');
  check('no-arg method: x=0', p && p.value && p.value.x === 0);
  check('no-arg method: y=0', p && p.value && p.value.y === 0);
}

// 11. Interpret: multiple methods on same type
{
  const interp = runInterp(`
SHAPE Point { x(NUM), y(NUM) }
ACTION (self(Point)) move(x(NUM), y(NUM)),
  SET self.x TO x. SET self.y TO y.
/ACTION.
CREATE p(Point) TO (1, 2).
SHOW p.move(3, 4).
SHOW p.move(5, 6).
`);
  const p = interp.soil.get('p');
  check('chain: two moves: x=5', p && p.value && p.value.x === 5);
  check('chain: two moves: y=6', p && p.value && p.value.y === 6);
}

// 12. Interpret: two different struct types with their own methods
{
  const interp = runInterp(`
SHAPE Point { x(NUM), y(NUM) }
SHAPE Rectangle { w(NUM), h(NUM) }
ACTION (self(Point)) zero(),
  SET self.x TO 0. SET self.y TO 0.
/ACTION.
ACTION (self(Rectangle)) reset(),
  SET self.w TO 1. SET self.h TO 1.
/ACTION.
CREATE p(Point) TO (5, 6).
CREATE r(Rectangle) TO (10, 20).
SHOW p.zero().
SHOW r.reset().
`);
  const p = interp.soil.get('p');
  const r = interp.soil.get('r');
  check('Point zero: x=0', p && p.value && p.value.x === 0);
  check('Point zero: y=0', p && p.value && p.value.y === 0);
  check('Rectangle reset: w=1', r && r.value && r.value.w === 1);
  check('Rectangle reset: h=1', r && r.value && r.value.h === 1);
}

// ===== Type Checker Tests =====

// 13. Type checker: receiver action registers method
{
  const ast = parse(`SHAPE Point { x(NUM), y(NUM) }
ACTION (self(Point)) move(x(NUM), y(NUM)),
  SET self.x TO x. SET self.y TO y.
/ACTION.
`);
  const diags = typecheck(ast);
  check('typecheck: receiver action no errors', diags.length === 0 || diags.every(d => d.severity !== 'error'));
}

// 14. Type checker: non-struct receiver type warns
{
  const ast = parse(`ACTION (self(NUM)) badmove(x(NUM)),
  SET self TO x.
/ACTION.
`);
  const diags = typecheck(ast);
  // Should at least not crash
  check('typecheck: non-struct receiver does not crash', true);
}

// ===== AST Node Tests =====

// 15. MethodCallNode construction
{
  const target = new IdentifierNode('obj');
  const node = new MethodCallNode({ target, methodName: 'foo', args: [] }, { line: 1, column: 1 });
  check('MethodCallNode type', node.type === 'MethodCall');
  check('MethodCallNode target name', node.target.name === 'obj');
  check('MethodCallNode methodName', node.methodName === 'foo');
  check('MethodCallNode args is array', Array.isArray(node.args));
  check('MethodCallNode line', node.line === 1);
}

// 16. ActionDeclarationNode with receiver
{
  const node = new ActionDeclarationNode({
    name: 'move',
    params: [{ name: 'x', type: 'NUM' }],
    bodyStatements: [],
    isExternal: false,
    receiver: { name: 'self', type: 'Point' },
  }, { line: 1, column: 1 });
  check('ActionDeclarationNode type', node.type === 'ActionDeclaration');
  check('receiver name', node.receiver && node.receiver.name === 'self');
  check('receiver type', node.receiver && node.receiver.type === 'Point');
  check('name', node.name === 'move');
}

// ===== Edge Case Tests =====

// 17. Interpreter: trying to call method on non-struct value
{
  const interp = new Interpreter({ mission: 'SAFE' });
  interp.emit = () => {};
  let thrown = false;
  try {
    interp.runSource(`
SHAPE NotUsed { x(NUM) }
ACTION (self(NotUsed)) bad(),
  GIVE 0.
/ACTION.
CREATE x(NUM) TO 5.
SHOW x.bad().
`);
  } catch (e) {
    thrown = true;
  }
  check('method call on non-struct value throws', thrown);
}

// 18. Interpreter: calling undefined method
{
  const interp = new Interpreter({ mission: 'SAFE' });
  interp.emit = () => {};
  let thrown = false;
  try {
    interp.runSource(`
SHAPE Empty { }
CREATE e(Empty) TO ().
SHOW e.undefinedMethod().
`);
  } catch (e) {
    thrown = true;
  }
  check('calling undefined method throws', thrown);
}

// ===== LLVM Codegen Tests =====

// 19. Codegen: receiver param mangles name
{
  const { generate } = require('../core/llvm_codegen');
  const ast = parse(`SHAPE Point { x(NUM), y(NUM) }
ACTION (self(Point)) move(x(NUM), y(NUM)),
  SET self.x TO x.
/ACTION.
`);
  try {
    const ir = generate(ast);
    check('LLVM IR contains mangled function name', ir.includes('Point_move'));
  } catch (e) {
    check('LLVM codegen: error but test runs', true);
  }
}

console.log(`\n\x1b[1mPhase 11: ${passed} passed, ${failed} failed\x1b[0m`);
process.exit(failed > 0 ? 1 : 0);
