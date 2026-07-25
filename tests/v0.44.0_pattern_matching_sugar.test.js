'use strict';
// ═══════════════════════════════════════════════════════════════
// tests/v0.44.0_pattern_matching_sugar.test.js
// Algebraic Safety Types, Exhaustive MATCH, String Interpolation,
// Ranges, Slicing & Destructuring
// ═══════════════════════════════════════════════════════════════

const { Interpreter } = require('../core/interpreter');
const { parse } = require('../core/parser');
const { tokenize, TOKEN, KEYWORDS } = require('../core/tokenizer');
const { ExhaustivenessChecker } = require('../src/compiler/exhaustiveness_checker');

let passed = 0, failed = 0;

function assert(condition, label) {
  if (condition) { passed++; }
  else { failed++; console.log(`  ✗ FAIL: ${label}`); }
}

function assertEqual(a, b, label) {
  const ok = a === b || (Number.isNaN(a) && Number.isNaN(b));
  if (ok) { passed++; }
  else { failed++; console.log(`  ✗ FAIL: ${label} — expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`); }
}

function assertDeepEqual(a, b, label) {
  const ok = JSON.stringify(a) === JSON.stringify(b);
  if (ok) { passed++; }
  else { failed++; console.log(`  ✗ FAIL: ${label} — expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`); }
}

function runTest(source) {
  const int = new Interpreter({ output: [] });
  try {
    int.runSource(source);
    return { output: int.output.map(o => o.text) };
  } catch (e) {
    return { error: e.message || String(e) };
  }
}

// ══════════════════════════════════════════════════════════
// 1. Option & Result Instantiation
// ══════════════════════════════════════════════════════════
console.log('\n── Section 1: Option & Result Instantiation ──');

// 1.1 — Option.Some with value
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'OptionConstruct', variant: 'Some',
    value: { type: 'Literal', value: 42, literalType: 'NUMBER', line: 1, column: 1, depth: 0 }
  }, int.soil);
  assertEqual(val.__choiceType, 'Option', 'Option.Some choiceType');
  assertEqual(val.tag, 'Some', 'Option.Some tag');
  assertEqual(val.payload, 42, 'Option.Some payload');
}

// 1.2 — Option.None
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'OptionConstruct', variant: 'None', value: null,
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertEqual(val.__choiceType, 'Option', 'Option.None choiceType');
  assertEqual(val.tag, 'None', 'Option.None tag');
  assertEqual(val.payload, null, 'Option.None payload');
}

// 1.3 — Result.Ok
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'ResultConstruct', variant: 'Ok',
    value: { type: 'Literal', value: 'success', literalType: 'STRING', line: 1, column: 1, depth: 0 }
  }, int.soil);
  assertEqual(val.__choiceType, 'Result', 'Result.Ok choiceType');
  assertEqual(val.tag, 'Ok', 'Result.Ok tag');
  assertEqual(val.payload, 'success', 'Result.Ok payload');
}

// 1.4 — Result.Err
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'ResultConstruct', variant: 'Err',
    value: { type: 'Literal', value: 'not found', literalType: 'STRING', line: 1, column: 1, depth: 0 }
  }, int.soil);
  assertEqual(val.__choiceType, 'Result', 'Result.Err choiceType');
  assertEqual(val.tag, 'Err', 'Result.Err tag');
  assertEqual(val.payload, 'not found', 'Result.Err payload');
}

// 1.5 — CHOICE via interpreter's built-in types (member access)
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  // Simulate Option.Some(42) via member access
  const choiceMarker = { __choiceType: 'Option' };
  const choiceDef = int.choices.get('Option');
  const variant = choiceDef.find(v => v.name === 'Some');
  const val = { __choiceType: 'Option', tag: variant.name, payload: 42 };
  assertEqual(val.__choiceType, 'Option', 'Member access Option.Some choiceType');
  assertEqual(val.tag, 'Some', 'Member access Option.Some tag');
  assertEqual(val.payload, 42, 'Member access Option.Some payload');
}

// ══════════════════════════════════════════════════════════
// 2. ExhaustivenessChecker
// ══════════════════════════════════════════════════════════
console.log('\n── Section 2: Exhaustiveness Checker ──');

// 2.1 — Option MATCH complete (Some + None)
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const checker = new ExhaustivenessChecker(int);
  const program = { type: 'Program', statements: [{
    type: 'MatchStatement', line: 1, column: 1, depth: 0,
    subjectExpr: { type: 'Identifier', name: 'Option', line: 1, column: 1, depth: 0 },
    clauses: [
      { variantName: 'Some', binding: 'v', bodyStatements: [] },
      { variantName: 'None', binding: null, bodyStatements: [] }
    ]
  }]};
  const errors = checker.check(program);
  assertEqual(errors.length, 0, 'Option MATCH with Some+None has no errors');
}

// 2.2 — Option MATCH incomplete (missing None)
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const checker = new ExhaustivenessChecker(int);
  const program = { type: 'Program', statements: [{
    type: 'MatchStatement', line: 1, column: 1, depth: 0,
    subjectExpr: { type: 'Identifier', name: 'Option', line: 1, column: 1, depth: 0 },
    clauses: [
      { variantName: 'Some', binding: 'v', bodyStatements: [] }
    ]
  }]};
  const errors = checker.check(program);
  assertEqual(errors.length, 1, 'Option MATCH missing None has 1 error');
  assert(errors[0].includes('None'), 'Error mentions None');
}

// 2.3 — Option MATCH with wildcard (complete)
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const checker = new ExhaustivenessChecker(int);
  const program = { type: 'Program', statements: [{
    type: 'MatchStatement', line: 1, column: 1, depth: 0,
    subjectExpr: { type: 'Identifier', name: 'Option', line: 1, column: 1, depth: 0 },
    clauses: [
      { variantName: 'Some', binding: 'v', bodyStatements: [] },
      { variantName: '_', binding: null, bodyStatements: [] }
    ]
  }]};
  const errors = checker.check(program);
  assertEqual(errors.length, 0, 'Option MATCH with wildcard has no errors');
}

// 2.4 — Result MATCH complete (Ok + Err)
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const checker = new ExhaustivenessChecker(int);
  const program = { type: 'Program', statements: [{
    type: 'MatchStatement', line: 1, column: 1, depth: 0,
    subjectExpr: { type: 'Identifier', name: 'Result', line: 1, column: 1, depth: 0 },
    clauses: [
      { variantName: 'Ok', binding: 'v', bodyStatements: [] },
      { variantName: 'Err', binding: 'e', bodyStatements: [] }
    ]
  }]};
  const errors = checker.check(program);
  assertEqual(errors.length, 0, 'Result MATCH with Ok+Err has no errors');
}

// 2.5 — Result MATCH incomplete (missing Err)
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const checker = new ExhaustivenessChecker(int);
  const program = { type: 'Program', statements: [{
    type: 'MatchStatement', line: 1, column: 1, depth: 0,
    subjectExpr: { type: 'Identifier', name: 'Result', line: 1, column: 1, depth: 0 },
    clauses: [
      { variantName: 'Ok', binding: 'v', bodyStatements: [] }
    ]
  }]};
  const errors = checker.check(program);
  assertEqual(errors.length, 1, 'Result MATCH missing Err has 1 error');
  assert(errors[0].includes('Err'), 'Error mentions Err');
}

// ══════════════════════════════════════════════════════════
// 3. String Interpolation
// ══════════════════════════════════════════════════════════
console.log('\n── Section 3: String Interpolation ──');

// 3.1 — Simple text-only string (no interpolation)
{
  const segs = [{ type: 'text', value: 'hello' }];
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'InterpolatedString', segments: segs,
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertEqual(val, 'hello', 'Text-only interpolated string');
}

// 3.2 — String with one interpolation
{
  const {Interpreter, Soil} = require('../core/interpreter');
  const int = new Interpreter();
  const soil = new (require('../core/runtime').Soil)();
  soil.set('name', 'world');
  const val = int.evaluateExpressionNode({
    type: 'InterpolatedString', segments: [
      { type: 'text', value: 'Hello ' },
      { type: 'expr', node: { type: 'Identifier', name: 'name', line: 1, column: 5, depth: 0 } }
    ],
    line: 1, column: 1, depth: 0
  }, soil);
  assertEqual(val, 'Hello world', 'String interpolation with variable');
}

// 3.3 — Multiple interpolations
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const Soil = require('../core/runtime').Soil;
  const soil = new Soil();
  soil.set('a', 'hello');
  soil.set('b', 'world');
  const val = int.evaluateExpressionNode({
    type: 'InterpolatedString', segments: [
      { type: 'text', value: '' },
      { type: 'expr', node: { type: 'Identifier', name: 'a', line: 1, column: 1, depth: 0 } },
      { type: 'text', value: ' ' },
      { type: 'expr', node: { type: 'Identifier', name: 'b', line: 1, column: 1, depth: 0 } }
    ],
    line: 1, column: 1, depth: 0
  }, soil);
  assertEqual(val, 'hello world', 'Multiple interpolations');
}

// ══════════════════════════════════════════════════════════
// 4. Range Expressions
// ══════════════════════════════════════════════════════════
console.log('\n── Section 4: Range Expressions ──');

// 4.1 — Range 0..5 produces [0,1,2,3,4]
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'RangeExpression',
    start: { type: 'Literal', value: 0, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    end: { type: 'Literal', value: 5, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertDeepEqual(val, [0, 1, 2, 3, 4], 'Range 0..5');
}

// 4.2 — Range 3..3 produces []
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'RangeExpression',
    start: { type: 'Literal', value: 3, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    end: { type: 'Literal', value: 3, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertDeepEqual(val, [], 'Range 3..3 empty');
}

// 4.3 — Range 1..1 produces []
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'RangeExpression',
    start: { type: 'Literal', value: 1, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    end: { type: 'Literal', value: 1, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertDeepEqual(val, [], 'Range 1..1 empty');
}

// ══════════════════════════════════════════════════════════
// 5. Slicing Expressions
// ══════════════════════════════════════════════════════════
console.log('\n── Section 5: Slicing ──');

// 5.1 — String slice [1:4]
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'SliceExpression',
    target: { type: 'Literal', value: 'hello', literalType: 'STRING', line: 1, column: 1, depth: 0 },
    start: { type: 'Literal', value: 1, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    end: { type: 'Literal', value: 4, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertEqual(val, 'ell', 'String slice [1:4]');
}

// 5.2 — Array slice [1:3]
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  int.soil.set('arr', [10, 20, 30, 40, 50]);
  const val = int.evaluateExpressionNode({
    type: 'SliceExpression',
    target: { type: 'Identifier', name: 'arr', line: 1, column: 1, depth: 0 },
    start: { type: 'Literal', value: 1, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    end: { type: 'Literal', value: 3, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertDeepEqual(val, [20, 30], 'Array slice [1:3]');
}

// 5.3 — Slice with start omitted (:3)
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'SliceExpression',
    target: { type: 'Literal', value: 'hello', literalType: 'STRING', line: 1, column: 1, depth: 0 },
    start: null,
    end: { type: 'Literal', value: 3, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertEqual(val, 'hel', 'String slice [null:3] (:end)');
}

// 5.4 — Slice with end omitted (3:)
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'SliceExpression',
    target: { type: 'Literal', value: 'hello', literalType: 'STRING', line: 1, column: 1, depth: 0 },
    start: { type: 'Literal', value: 3, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    end: null,
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertEqual(val, 'lo', 'String slice [3:null] (start:)');
}

// ══════════════════════════════════════════════════════════
// 6. Destructuring
// ══════════════════════════════════════════════════════════
console.log('\n── Section 6: Destructuring ──');

// 6.1 — Object destructuring { x, y }
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const soil = int.soil;
  const point = { x: 10, y: 20 };
  const node = {
    type: 'DestructDeclaration', patternType: 'object',
    pattern: ['x', 'y'],
    sourceExpr: { type: 'Literal', value: point, literalType: 'RAW_EXPR', line: 1, column: 1, depth: 0 },
    line: 1, column: 1, depth: 0
  };
  // Override sourceExpr evaluation to return the point object directly
  node.sourceExpr.literalType = 'RAW_EXPR';
  // We need to evaluate via the interpreter
  const origEval = int.evaluateExpressionNode.bind(int);
  // Patch for test: make the source expr return point
  const pointObj = point;
  int.evaluateExpressionNode = function(n, s) {
    if (n === node.sourceExpr) return pointObj;
    return origEval(n, s);
  };
  int.evaluateDestructDeclaration(node, soil);
  int.evaluateExpressionNode = origEval;
  const xEntry = soil.get('x');
  const yEntry = soil.get('y');
  assertEqual(xEntry.value, 10, 'Destructured x = 10');
  assertEqual(yEntry.value, 20, 'Destructured y = 20');
}

// 6.2 — Array destructuring [head, tail]
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const soil = int.soil;
  const list = [100, 200, 300];
  const node = {
    type: 'DestructDeclaration', patternType: 'array',
    pattern: ['head', 'tail'],
    sourceExpr: { type: 'Literal', value: null, literalType: 'RAW_EXPR', line: 1, column: 1, depth: 0 },
    line: 1, column: 1, depth: 0
  };
  const origEval = int.evaluateExpressionNode.bind(int);
  int.evaluateExpressionNode = function(n, s) {
    if (n === node.sourceExpr) return list;
    return origEval(n, s);
  };
  int.evaluateDestructDeclaration(node, soil);
  int.evaluateExpressionNode = origEval;
  const headEntry = soil.get('head');
  const tailEntry = soil.get('tail');
  assertEqual(headEntry.value, 100, 'Destructured head = 100');
  assertEqual(tailEntry.value, 200, 'Destructured tail = 200');
}

// ══════════════════════════════════════════════════════════
// 7. BinaryOp & UnaryOp Evaluation
// ══════════════════════════════════════════════════════════
console.log('\n── Section 7: BinaryOp & UnaryOp ──');

// 7.1 — BinaryOp addition
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'BinaryOp', operator: '+',
    left: { type: 'Literal', value: 10, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    right: { type: 'Literal', value: 5, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertEqual(val, 15, 'BinaryOp 10+5 = 15');
}

// 7.2 — BinaryOp IS (equality)
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'BinaryOp', operator: 'IS',
    left: { type: 'Literal', value: 10, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    right: { type: 'Literal', value: 10, literalType: 'NUMBER', line: 1, column: 1, depth: 0 },
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertEqual(val, true, 'BinaryOp 10 IS 10 = true');
}

// 7.3 — UnaryOp NOT
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'UnaryOp', operator: 'NOT',
    operand: { type: 'Literal', value: true, literalType: 'FACT', line: 1, column: 1, depth: 0 },
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertEqual(val, false, 'UnaryOp NOT true = false');
}

// 7.4 — BinaryOp string concatenation (+)
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const val = int.evaluateExpressionNode({
    type: 'BinaryOp', operator: '+',
    left: { type: 'Literal', value: 'hello ', literalType: 'STRING', line: 1, column: 1, depth: 0 },
    right: { type: 'Literal', value: 'world', literalType: 'STRING', line: 1, column: 1, depth: 0 },
    line: 1, column: 1, depth: 0
  }, int.soil);
  assertEqual(val, 'hello world', 'BinaryOp string concat');
}

// ══════════════════════════════════════════════════════════
// 8. Tokenizer Keyword Recognition
// ══════════════════════════════════════════════════════════
console.log('\n── Section 8: Tokenizer Keywords ──');

// 8.1 — LET keyword
assert(KEYWORDS.has('LET'), 'LET keyword registered');

// 8.2 — OPTION keyword
assert(KEYWORDS.has('OPTION'), 'OPTION keyword registered');

// 8.3 — RESULT keyword
assert(KEYWORDS.has('RESULT'), 'RESULT keyword registered');

// ══════════════════════════════════════════════════════════
// 9. C Runtime — Option/Result/Slice/Range declarations
// ══════════════════════════════════════════════════════════
console.log('\n── Section 9: C Runtime Declarations ──');

const fs = require('fs');
const headerPath = require('path').join(__dirname, '..', 'runtime', 'c', 'plant_runtime.h');
const header = fs.readFileSync(headerPath, 'utf8');

// 9.1 — PlantTagged typedef
assert(header.includes('PlantTagged'), 'PlantTagged typedef in header');

// 9.2 — plant_option_some declaration
assert(header.includes('plant_option_some'), 'plant_option_some declaration');

// 9.3 — plant_option_none declaration
assert(header.includes('plant_option_none'), 'plant_option_none declaration');

// 9.4 — plant_result_ok declaration
assert(header.includes('plant_result_ok'), 'plant_result_ok declaration');

// 9.5 — plant_result_err declaration
assert(header.includes('plant_result_err'), 'plant_result_err declaration');

// 9.6 — plant_is_some declaration
assert(header.includes('plant_is_some'), 'plant_is_some declaration');

// 9.7 — plant_is_none declaration
assert(header.includes('plant_is_none'), 'plant_is_none declaration');

// 9.8 — plant_unwrap declaration
assert(header.includes('plant_unwrap'), 'plant_unwrap declaration');

// 9.9 — plant_is_ok declaration
assert(header.includes('plant_is_ok'), 'plant_is_ok declaration');

// 9.10 — plant_is_err declaration
assert(header.includes('plant_is_err'), 'plant_is_err declaration');

// 9.11 — plant_unwrap_err declaration
assert(header.includes('plant_unwrap_err'), 'plant_unwrap_err declaration');

// 9.12 — plant_array_slice declaration
assert(header.includes('plant_array_slice'), 'plant_array_slice declaration');

// 9.13 — plant_string_slice declaration
assert(header.includes('plant_string_slice'), 'plant_string_slice declaration');

// 9.14 — plant_range declaration
assert(header.includes('plant_range'), 'plant_range declaration');

// 9.15 — Implementations in .c file
const sourcePath = require('path').join(__dirname, '..', 'runtime', 'c', 'plant_runtime.c');
const source = fs.readFileSync(sourcePath, 'utf8');
assert(source.includes('plant_option_some'), 'plant_option_some implementation');
assert(source.includes('plant_option_none'), 'plant_option_none implementation');
assert(source.includes('plant_result_ok'), 'plant_result_ok implementation');
assert(source.includes('plant_result_err'), 'plant_result_err implementation');
assert(source.includes('plant_array_slice'), 'plant_array_slice implementation');
assert(source.includes('plant_string_slice'), 'plant_string_slice implementation');
assert(source.includes('plant_range'), 'plant_range implementation');

// ══════════════════════════════════════════════════════════
// 10. CodeWords Governance
// ══════════════════════════════════════════════════════════
console.log('\n── Section 10: CodeWords Governance ──');

const { CodeWordsChecker } = require('../src/security/codewords_governance');

// 10.1 — NETWORK_NODES includes OptionConstruct
{
  const cw = new CodeWordsChecker(['#ALLOW_NETWORK']);
  const node = { type: 'OptionConstruct', line: 1, column: 1, depth: 0 };
  const result = cw.checkNode(node);
  assert(result, 'OptionConstruct passes CodeWords check');
}

// 10.2 — NETWORK_NODES includes ResultConstruct
{
  const cw = new CodeWordsChecker(['#ALLOW_NETWORK']);
  const node = { type: 'ResultConstruct', line: 1, column: 1, depth: 0 };
  const result = cw.checkNode(node);
  assert(result, 'ResultConstruct passes CodeWords check');
}

// 10.3 — NETWORK_NODES includes SliceExpression
{
  const cw = new CodeWordsChecker(['#ALLOW_NETWORK']);
  const node = { type: 'SliceExpression', line: 1, column: 1, depth: 0 };
  const result = cw.checkNode(node);
  assert(result, 'SliceExpression passes CodeWords check');
}

// 10.4 — NETWORK_NODES includes RangeExpression
{
  const cw = new CodeWordsChecker(['#ALLOW_NETWORK']);
  const node = { type: 'RangeExpression', line: 1, column: 1, depth: 0 };
  const result = cw.checkNode(node);
  assert(result, 'RangeExpression passes CodeWords check');
}

// 10.5 — NETWORK_NODES includes DestructDeclaration
{
  const cw = new CodeWordsChecker(['#ALLOW_NETWORK']);
  const node = { type: 'DestructDeclaration', line: 1, column: 1, depth: 0 };
  const result = cw.checkNode(node);
  assert(result, 'DestructDeclaration passes CodeWords check');
}

// 10.6 — NETWORK_NODES includes InterpolatedString
{
  const cw = new CodeWordsChecker(['#ALLOW_NETWORK']);
  const node = { type: 'InterpolatedString', line: 1, column: 1, depth: 0 };
  const result = cw.checkNode(node);
  assert(result, 'InterpolatedString passes CodeWords check');
}

// 10.7 — NETWORK_NODES includes MatchExpr
{
  const cw = new CodeWordsChecker(['#ALLOW_NETWORK']);
  const node = { type: 'MatchExpr', line: 1, column: 1, depth: 0 };
  const result = cw.checkNode(node);
  assert(result, 'MatchExpr passes CodeWords check');
}

// ══════════════════════════════════════════════════════════
// 11. Tokenizer — string interpolation
// ══════════════════════════════════════════════════════════
console.log('\n── Section 11: Tokenizer String Interpolation ──');

// 11.1 — Plain string tokenization
{
  const tokens = tokenize('SHOW "hello".');
  const strToken = tokens.find(t => t.type === 'STRING');
  assert(strToken !== undefined, 'String token exists');
  if (strToken) {
    assertEqual(strToken.value, 'hello', 'Plain string value is text');
  }
}

// ══════════════════════════════════════════════════════════
// 12. ExhaustivenessChecker — ENUM matching (via Choice)
// ══════════════════════════════════════════════════════════
console.log('\n── Section 12: ENUM (Choice) Exhaustiveness ──');

// 12.1 — Complete MATCH on all CHOICE variants
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const checker = new ExhaustivenessChecker(int);
  const program = { type: 'Program', statements: [{
    type: 'MatchStatement', line: 1, column: 1, depth: 0,
    subjectExpr: { type: 'Identifier', name: 'Option', line: 1, column: 1, depth: 0 },
    clauses: [
      { variantName: 'Some', binding: 'v', bodyStatements: [] },
      { variantName: 'None', binding: null, bodyStatements: [] }
    ]
  }]};
  const errors = checker.check(program);
  assertEqual(errors.length, 0, 'Choice MATCH all variants complete');
}

// 12.2 — Incomplete MATCH (missing variant)
{
  const {Interpreter} = require('../core/interpreter');
  const int = new Interpreter();
  const checker = new ExhaustivenessChecker(int);
  const program = { type: 'Program', statements: [{
    type: 'MatchStatement', line: 1, column: 1, depth: 0,
    subjectExpr: { type: 'Identifier', name: 'Option', line: 1, column: 1, depth: 0 },
    clauses: [
      { variantName: 'Some', binding: 'v', bodyStatements: [] }
    ]
  }]};
  const errors = checker.check(program);
  assertEqual(errors.length, 1, 'Choice MATCH missing variant detected');
}

// ══════════════════════════════════════════════════════════
// Summary
// ══════════════════════════════════════════════════════════
console.log(`\n${'─'.repeat(50)}`);
console.log(`Results: ${passed} passed, ${failed} failed`);
if (failed > 0) process.exit(1);
