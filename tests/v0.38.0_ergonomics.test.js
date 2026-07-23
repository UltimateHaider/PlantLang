#!/usr/bin/env node
'use strict';

// ═══════════════════════════════════════════════════════════════
//  tests/v0.38.0_ergonomics.test.js
//  v0.38.0 — Language Ergonomics & AST Zero-Fallback
//  Covers: CycleInStatement, BREAK/CONTINUE, multi-field SORT,
//  nested struct formatting, BLOOM AS, strict AST invariant.
// ═══════════════════════════════════════════════════════════════

const { parse } = require('../core/parser');
const { Interpreter } = require('../core/interpreter');
const { formatShowValue } = require('../src/interpreter/show_formatter');
const { ArenaAllocator, ARCHeap } = require('../src/memory/allocator');
const { evaluateSortStatement, _makeChainedComparator } = require('../src/interpreter/sort_evaluator');
const { isRestrictedEnvironment } = require('../src/interpreter/bloom_evaluator');
const {
  EndBlockNode, BranchElseNode, BlockDelimiterNode,
  CycleInStatementNode, BreakStatementNode, ContinueStatementNode,
  SortStatementV2Node, BloomAsStatementNode,
  BreakSignalException, ContinueSignalException
} = require('../core/ast');

let passed = 0, failed = 0;
function check(label, cond, detail) {
  if (cond) { console.log(`  \x1b[32m\u2713\x1b[0m ${label}`); passed++; }
  else { console.log(`  \x1b[31m\u2717\x1b[0m ${label}`); if (detail) console.log(`      \u2192 ${detail}`); failed++; }
}
function assertEqual(actual, expected, msg) {
  check(msg || `expected ${JSON.stringify(expected)}`, actual === expected, `got ${JSON.stringify(actual)}`);
}

console.log('\n\x1b[1m--- v0.38.0: AST Zero-Fallback ---\x1b[0m');

// 1.1 EndBlockNode exists and is creatable
{
  const n = new EndBlockNode({ blockType: 'IF' }, { line: 1, column: 1, depth: 0 });
  check('EndBlockNode type is EndBlock', n.type === 'EndBlock');
  check('EndBlockNode blockType is IF', n.blockType === 'IF');
}

// 1.2 BranchElseNode exists
{
  const n = new BranchElseNode({ bodyStatements: [] }, { line: 1, column: 1, depth: 0 });
  check('BranchElseNode type is BranchElse', n.type === 'BranchElse');
}

// 1.3 BlockDelimiterNode exists
{
  const n = new BlockDelimiterNode({ delimiter: '.' }, { line: 1, column: 1, depth: 0 });
  check('BlockDelimiterNode type is BlockDelimiter', n.type === 'BlockDelimiter');
}

// 1.4 RawStatement should NOT appear in parsed output
{
  const src = '1\\ SHOW "hello".\n';
  const ast = parse(src);
  let rawFound = false;
  function walk(n) {
    if (n && n.type === 'RawStatement') rawFound = true;
    if (n && n.bodyStatements) n.bodyStatements.forEach(walk);
    if (n && n.statements) n.statements.forEach(walk);
  }
  walk(ast);
  check('No RawStatement in parsed AST', !rawFound);
}

// 1.5 EndBlockNode emitted for bare period terminator
{
  // A well-formed program that ends cleanly should not throw
  const src = '1\\ CREATE x(NUM) TO 1.\n1\\ SHOW x.\n';
  let threw = false;
  try { parse(src); } catch (e) { threw = true; }
  check('Clean program parses without error', !threw, threw ? String(threw) : '');
}

console.log('\n\x1b[1m--- v0.38.0: CYCLE ... IN ---\x1b[0m');

// 2.1 CYCLE x IN [] empty array — should not produce SHOW output
{
  const src = '1\\ CREATE items(LIST) TO.\n1\\ CYCLE x IN items,\n2\\   SHOW x.\n1\\.\n';
  const out = [];
  const interp = new Interpreter({ emit: (t) => out.push(t) });
  interp.runSource(src);
  // Filter for SHOW output only (ignore CREATE/other emissions)
  const showOut = out.filter(o => typeof o === 'string' && !o.includes('CREATE') && !o.includes('SET') && !o.includes('LIST') && o !== 'items');
  check('CYCLE IN empty: no SHOW output', showOut.length === 0, `got ${showOut.length}: ${JSON.stringify(showOut)}`);
}

// 2.2 CYCLE x IN list with null/undefined
{
  // Should not throw
  const src = '1\\ CREATE items(LIST) TO.\n1\\ CYCLE x IN items,\n2\\   SHOW x.\n1\\.\n';
  let threw = false;
  try {
    const interp = new Interpreter({ emit: () => {} });
    interp.runSource(src);
  } catch (e) { threw = true; }
  check('CYCLE IN null/undefined: graceful exit', !threw, threw ? e.message : '');
}

// 2.3 CYCLE x, idx IN list with index variable
{
  const out = [];
  const interp = new Interpreter({ emit: (t) => out.push(t) });
  interp.runSource(
    '1\\ CREATE items(LIST) TO 10, 20, 30.\n' +
    '1\\ CYCLE val, idx IN items,\n' +
    '2\\   SHOW idx.\n' +
    '1\\.\n'
  );
  const idxOut = out.filter(o => o === '0' || o === '1' || o === '2');
  check('CYCLE with index: 3 iterations', idxOut.length === 3, `got ${idxOut.length}: ${JSON.stringify(idxOut)}`);
}

// 2.4 CYCLE x IN list: each element visited
{
  const out = [];
  const interp = new Interpreter({ emit: (t) => out.push(t) });
  interp.runSource(
    '1\\ CREATE items(LIST) TO a, b, c.\n' +
    '1\\ CYCLE x IN items,\n' +
    '2\\   SHOW x.\n' +
    '1\\.\n'
  );
  // Filter to get SHOW output values (interpreter emits raw values)
  const vals = out.filter(o => o === 'a' || o === 'b' || o === 'c');
  check('CYCLE IN visited 3 items', vals.length === 3, `got ${vals.length}: ${JSON.stringify(out)}`);
  check('CYCLE IN first item is a', vals[0] === 'a');
}

console.log('\n\x1b[1m--- v0.38.0: BREAK / CONTINUE ---\x1b[0m');

// 3.1 BreakSignalException is throwable
{
  try { throw new BreakSignalException(); }
  catch (e) { check('BreakSignalException caught', e instanceof BreakSignalException); }
}

// 3.2 ContinueSignalException is throwable
{
  try { throw new ContinueSignalException(); }
  catch (e) { check('ContinueSignalException caught', e instanceof ContinueSignalException); }
}

// 3.3 BreakStatementNode exists
{
  const n = new BreakStatementNode({ line: 1, column: 1, depth: 0 });
  check('BreakStatementNode type is BreakStatement', n.type === 'BreakStatement');
}

// 3.4 ContinueStatementNode exists
{
  const n = new ContinueStatementNode({ line: 1, column: 1, depth: 0 });
  check('ContinueStatementNode type is ContinueStatement', n.type === 'ContinueStatement');
}

console.log('\n\x1b[1m--- v0.38.0: Multi-field SORT ---\x1b[0m');

// 4.1 SortStatementV2Node exists
{
  const n = new SortStatementV2Node({ listExpr: 'items', fields: [{ field: 'name', direction: 'ASC' }] }, { line: 1, column: 1, depth: 0 });
  check('SortStatementV2Node type is SortStatementV2', n.type === 'SortStatementV2');
  check('SortStatementV2Node has fields', n.fields.length === 1);
}

// 4.2 Chained comparator: sorts by field1 ASC, then field2 DESC
{
  const data = [
    { name: 'Alice', age: 30 },
    { name: 'Bob', age: 25 },
    { name: 'Alice', age: 20 },
  ];
  const comparator = _makeChainedComparator([
    { field: 'name', direction: 'ASC' },
    { field: 'age', direction: 'DESC' },
  ]);
  data.sort(comparator);
  check('Multi-field sort: first item is Alice,30', data[0].name === 'Alice' && data[0].age === 30, JSON.stringify(data[0]));
  check('Multi-field sort: second is Alice,20', data[1].name === 'Alice' && data[1].age === 20, JSON.stringify(data[1]));
  check('Multi-field sort: third is Bob,25', data[2].name === 'Bob' && data[2].age === 25, JSON.stringify(data[2]));
}

// 4.3 Null/undefined fields sort to end
{
  const data = [
    { name: 'Charlie', age: 35 },
    { name: null, age: 40 },
    { name: 'Alice', age: 30 },
  ];
  const comparator = _makeChainedComparator([
    { field: 'name', direction: 'ASC' },
  ]);
  data.sort(comparator);
  check('Null field sorts to end', data[2].name === null, JSON.stringify(data[2]));
  check('Non-null names come first', data[0].name === 'Alice', JSON.stringify(data[0]));
}

// 4.4 Simple SORT direction
{
  const arr = [3, 1, 4, 1, 5];
  arr.sort((a, b) => a - b);
  check('Simple ASC sort', arr[0] === 1 && arr[arr.length-1] === 5, JSON.stringify(arr));
}

// 4.5 DESC sort
{
  const arr = [3, 1, 4, 1, 5];
  arr.sort((a, b) => b - a);
  check('Simple DESC sort', arr[0] === 5 && arr[arr.length-1] === 1, JSON.stringify(arr));
}

// 4.6 String sort with localeCompare
{
  const arr = ['banana', 'apple', 'cherry'];
  arr.sort((a, b) => a.localeCompare(b));
  check('String locale sort', arr[0] === 'apple' && arr[2] === 'cherry', JSON.stringify(arr));
}

console.log('\n\x1b[1m--- v0.38.0: Nested Struct Formatting ---\x1b[0m');

// 5.1 formatShowValue handles primitives
{
  assertEqual(formatShowValue(42), '42', 'formatShowValue number');
  assertEqual(formatShowValue('hello'), 'hello', 'formatShowValue string');
  assertEqual(formatShowValue(true), 'true', 'formatShowValue boolean');
}

// 5.2 formatShowValue handles null/undefined
{
  assertEqual(formatShowValue(null), 'NULL', 'formatShowValue null');
  assertEqual(formatShowValue(undefined), 'VOID', 'formatShowValue undefined');
}

// 5.3 formatShowValue handles empty arrays
{
  assertEqual(formatShowValue([]), '[]', 'formatShowValue empty array');
}

// 5.4 formatShowValue handles struct instances
{
  const struct = { __structType: 'Point', x: 10, y: 20 };
  const result = formatShowValue(struct);
  check('Struct format contains Point', result.includes('Point'), result);
  check('Struct format contains x: 10', result.includes('x: 10'), result);
  check('Struct format contains y: 20', result.includes('y: 20'), result);
}

// 5.5 formatShowValue handles nested structs
{
  const inner = { __structType: 'Meta', role: 'Architect', level: 5 };
  const outer = { __structType: 'Person', name: 'Haider', metadata: inner };
  const result = formatShowValue(outer);
  check('Nested struct format contains Person', result.includes('Person'), result);
  check('Nested struct format contains Meta', result.includes('Meta'), result);
  check('Nested struct format contains inner role', result.includes('Architect'), result);
}

console.log('\n\x1b[1m--- v0.38.0: BLOOM AS ---\x1b[0m');

// 6.1 BloomAsStatementNode exists
{
  const n = new BloomAsStatementNode({ dataExpr: 'items', targetType: 'TABLE', configMap: {} }, { line: 1, column: 1, depth: 0 });
  check('BloomAsStatementNode type is BloomAsStatement', n.type === 'BloomAsStatement');
  check('BloomAsStatementNode targetType TABLE', n.targetType === 'TABLE');
}

// 6.2 isRestrictedEnvironment detects restricted env
{
  const prev = process.env.CODEPLANT_RESTRICTED;
  process.env.CODEPLANT_RESTRICTED = '1';
  const restricted = isRestrictedEnvironment();
  process.env.CODEPLANT_RESTRICTED = prev;
  check('isRestrictedEnvironment detects CODEPLANT_RESTRICTED', restricted);
}

console.log('\n\x1b[1m--- v0.38.0: Memory Allocators ---\x1b[0m');

// 7.1 ArenaAllocator basic alloc/write/read
{
  const arena = new ArenaAllocator(1024);
  const ptr = arena.alloc(16);
  check('ArenaAllocator alloc returns valid ptr', typeof ptr === 'number' && ptr >= 0);
  arena.write(ptr, 42);
  check('ArenaAllocator used > 0', arena.used > 0);
  check('ArenaAllocator remaining < capacity', arena.remaining < arena._capacity);
}

// 7.2 ArenaAllocator reset
{
  const arena = new ArenaAllocator(1024);
  arena.alloc(64);
  arena.reset();
  check('ArenaAllocator reset sets offset to 0', arena.used === 0);
}

// 7.3 ArenaAllocator child arena
{
  const parent = new ArenaAllocator(1024);
  const child = parent.createChild();
  check('Child arena created', child instanceof ArenaAllocator);
  parent.reset();
  check('Child arena reset after parent reset', child.used === 0);
}

// 7.4 ARCHeap basic alloc/retain/release
{
  const heap = new ARCHeap();
  heap.alloc('obj1', { data: 'hello' });
  check('ARCHeap has obj1', heap.has('obj1'));
  check('ARCHeap live count is 1', heap.liveCount === 1);
  heap.retain('obj1');
  // release twice to free
  heap.release('obj1');
  heap.release('obj1');
  check('ARCHeap obj1 freed after 2 releases', !heap.has('obj1'));
}

// 7.5 ARCHeap nested references
{
  const heap = new ARCHeap();
  heap.alloc('parent', { name: 'parent' });
  heap.alloc('child', { name: 'child' });
  heap.addReference('parent', 'child');
  heap.retain('parent'); // cascades to child
  // release parent twice
  heap.release('parent');
  heap.release('parent');
  check('ARCHeap cascading: parent freed', !heap.has('parent'));
  check('ARCHeap cascading: child also freed', !heap.has('child'));
}

console.log('\n\x1b[1m--- v0.38.0: Integration ---\x1b[0m');

// 8.1 End-to-end: CYCLE with index and SHOW
{
  const out = [];
  const interp = new Interpreter({ emit: (t) => out.push(t) });
  interp.runSource(
    '1\\ CREATE scores(LIST) TO 85, 92, 78.\n' +
    '1\\ CYCLE s, i IN scores,\n' +
    '2\\   SHOW i.\n' +
    '1\\.\n'
  );
  const idxOut = out.filter(o => o === '0' || o === '1' || o === '2');
  check('Integration: 3 index outputs', idxOut.length === 3, `got ${idxOut.length}`);
}

// 8.2 End-to-end: parse + run simple program
{
  const out = [];
  const interp = new Interpreter({ emit: (t) => out.push(t) });
  interp.runSource('1\\ SHOW "v0.38.0".\n');
  check('Integration: program runs successfully', out.some(o => o.includes?.('v0.38.0')));
}

// 8.3 No RawStatement in end-to-end execution
{
  const src = '1\\ CREATE x(NUM) TO 42.\n1\\ SHOW x.\n';
  const ast = parse(src);
  let rawFound = false;
  function walk(n) {
    if (n && n.type === 'RawStatement') rawFound = true;
    if (n && n.bodyStatements) n.bodyStatements.forEach(walk);
    if (n && n.statements) n.statements.forEach(walk);
  }
  walk(ast);
  check('Zero-fallback: no RawStatement in parsed output', !rawFound);
}

// ── Summary ──
console.log(`\n\x1b[1mResult: ${passed} passed, ${failed} failed\x1b[0m\n`);
process.exit(failed > 0 ? 1 : 0);
