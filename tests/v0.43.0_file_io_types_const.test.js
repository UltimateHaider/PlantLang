const { ASTConstantFolder } = require('../src/compiler/ast_constant_folder');
const { CodeWordsChecker } = require('../src/security/codewords_governance');

const path = require('path');
const fs = require('fs');

let passed = 0, failed = 0;

function assert(cond, label) {
  if (cond) { passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  else { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label); }
}

function assertEqual(a, b, label) {
  if (a === b) { passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  else { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label + ' (' + JSON.stringify(a) + ' !== ' + JSON.stringify(b) + ')'); }
}

function assertNotThrows(fn, label) {
  try { fn(); passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  catch (e) { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label + ': ' + e.message); }
}

function assertThrows(fn, label) {
  try { fn(); failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label); }
  catch (e) { passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
}

const tmpDir = path.join(__dirname, '..', '.v043_test_tmp');
if (!fs.existsSync(tmpDir)) fs.mkdirSync(tmpDir, { recursive: true });

function cleanTmp() {
  try {
    for (const f of fs.readdirSync(tmpDir)) {
      try { fs.unlinkSync(path.join(tmpDir, f)); } catch (_) {}
    }
  } catch (_) {}
}

function main() {

// ═══════════════════════════════════════════════════════════════
//  1. AST Constant Folder — Arithmetic folding
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 1. AST Constant Folder: Arithmetic ---\u001b[0m');

(function() {
  const folder = new ASTConstantFolder();
  const program = {
    type: 'Program',
    statements: [
      { type: 'ConstDeclaration', identifier: 'x', varType: 'NUM',
        valueExpr: { type: 'Literal', literalType: 'NUMBER', value: 10 } },
    ]
  };
  const result = folder.fold(program);
  assert(folder._consts.get('x') === 10, 'CONST x collects value 10');
})();

(function() {
  const folder = new ASTConstantFolder();
  const program = {
    type: 'Program',
    statements: [
      { type: 'ShowStatement', expr: {
        type: 'BinaryExpression', operator: '+',
        left: { type: 'Literal', literalType: 'NUMBER', value: 10 },
        right: { type: 'Literal', literalType: 'NUMBER', value: 5 }
      }}
    ]
  };
  const result = folder.fold(program);
  const show = result.statements[0];
  assert(show.expr.type === 'Literal', '10+5 folded to Literal');
  assert(show.expr.value === 15, '10+5 = 15');
})();

(function() {
  const folder = new ASTConstantFolder();
  const result = folder.fold({
    type: 'Program',
    statements: [{
      type: 'ShowStatement', expr: {
        type: 'BinaryExpression', operator: '-',
        left: { type: 'Literal', literalType: 'NUMBER', value: 100 },
        right: { type: 'Literal', literalType: 'NUMBER', value: 23 }
      }
    }]
  });
  assert(result.statements[0].expr.value === 77, '100-23 = 77');
})();

(function() {
  const folder = new ASTConstantFolder();
  const result = folder.fold({
    type: 'Program',
    statements: [{
      type: 'ShowStatement', expr: {
        type: 'BinaryExpression', operator: '*',
        left: { type: 'Literal', literalType: 'NUMBER', value: 7 },
        right: { type: 'Literal', literalType: 'NUMBER', value: 8 }
      }
    }]
  });
  assert(result.statements[0].expr.value === 56, '7*8 = 56');
})();

(function() {
  const folder = new ASTConstantFolder();
  const result = folder.fold({
    type: 'Program',
    statements: [{
      type: 'ShowStatement', expr: {
        type: 'BinaryExpression', operator: '/',
        left: { type: 'Literal', literalType: 'NUMBER', value: 10 },
        right: { type: 'Literal', literalType: 'NUMBER', value: 4 }
      }
    }]
  });
  assert(result.statements[0].expr.value === 2.5, '10/4 = 2.5');
})();

(function() {
  const folder = new ASTConstantFolder();
  const result = folder.fold({
    type: 'Program',
    statements: [{
      type: 'ShowStatement', expr: {
        type: 'BinaryExpression', operator: '**',
        left: { type: 'Literal', literalType: 'NUMBER', value: 2 },
        right: { type: 'Literal', literalType: 'NUMBER', value: 10 }
      }
    }]
  });
  assert(result.statements[0].expr.value === 1024, '2**10 = 1024');
})();

(function() {
  const folder = new ASTConstantFolder();
  const result = folder.fold({
    type: 'Program',
    statements: [{
      type: 'ShowStatement', expr: {
        type: 'BinaryExpression', operator: 'AND',
        left: { type: 'Literal', literalType: 'FACT', value: true },
        right: { type: 'Literal', literalType: 'FACT', value: false }
      }
    }]
  });
  assert(result.statements[0].expr.value === false, 'true AND false = false');
})();

(function() {
  const folder = new ASTConstantFolder();
  const result = folder.fold({
    type: 'Program',
    statements: [{
      type: 'ShowStatement', expr: {
        type: 'BinaryExpression', operator: 'IS',
        left: { type: 'Literal', literalType: 'NUMBER', value: 42 },
        right: { type: 'Literal', literalType: 'NUMBER', value: 42 }
      }
    }]
  });
  assert(result.statements[0].expr.value === true, '42 IS 42 = true');
})();

// ═══════════════════════════════════════════════════════════════
//  2. AST Constant Folder — String concatenation folding
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 2. AST Constant Folder: String ---\u001b[0m');

(function() {
  const folder = new ASTConstantFolder();
  const result = folder.fold({
    type: 'Program',
    statements: [{
      type: 'ShowStatement', expr: {
        type: 'BinaryExpression', operator: '+',
        left: { type: 'Literal', literalType: 'STRING', value: 'Hello ' },
        right: { type: 'Literal', literalType: 'STRING', value: 'World' }
      }
    }]
  });
  assert(result.statements[0].expr.value === 'Hello World', '"Hello " + "World" = "Hello World"');
})();

// ═══════════════════════════════════════════════════════════════
//  3. ENUM — Declaration and member resolution
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 3. ENUM Declaration ---\u001b[0m');

(function() {
  const { EnumDeclarationNode } = require('../core/ast');
  const enumNode = new EnumDeclarationNode({ name: 'Color', members: [
    { name: 'RED', value: 0 },
    { name: 'GREEN', value: 1 },
    { name: 'BLUE', value: 2 },
  ]}, { line: 1, column: 1, depth: 0 });
  assert(enumNode.type === 'EnumDeclaration', 'EnumDeclaration node type');
  assert(enumNode.name === 'Color', 'Enum name is Color');
  assert(enumNode.members.length === 3, '3 enum members');
  assert(enumNode.members[0].name === 'RED', 'First member is RED');
  assert(enumNode.members[0].value === 0, 'RED = 0');
  assert(enumNode.members[1].value === 1, 'GREEN = 1');
  assert(enumNode.members[2].value === 2, 'BLUE = 2');
})();

(function() {
  const { EnumDeclarationNode } = require('../core/ast');
  const enumNode = new EnumDeclarationNode({ name: 'Status', members: [
    { name: 'PENDING', value: 0 },
    { name: 'ACTIVE', value: 1 },
    { name: 'INACTIVE', value: 2 },
    { name: 'ARCHIVED', value: 3 },
  ]}, { line: 1, column: 1, depth: 0 });
  assert(enumNode.members[3].name === 'ARCHIVED', '4th member ARCHIVED');
  assert(enumNode.members[3].value === 3, 'ARCHIVED = 3');
})();

// ═══════════════════════════════════════════════════════════════
//  4. TYPE — Alias declarations
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 4. TYPE Alias Declaration ---\u001b[0m');

(function() {
  const { TypeAliasDeclarationNode } = require('../core/ast');
  const aliasNode = new TypeAliasDeclarationNode({ alias: 'MyNum', targetType: 'NUM' }, { line: 1, column: 1, depth: 0 });
  assert(aliasNode.type === 'TypeAliasDeclaration', 'TypeAliasDeclaration node type');
  assert(aliasNode.alias === 'MyNum', 'Alias name MyNum');
  assert(aliasNode.targetType === 'NUM', 'Alias target type NUM');
})();

(function() {
  const { TypeAliasDeclarationNode } = require('../core/ast');
  const aliasNode = new TypeAliasDeclarationNode({ alias: 'String', targetType: 'TX' }, { line: 1, column: 1, depth: 0 });
  assert(aliasNode.alias === 'String', 'Alias name String');
  assert(aliasNode.targetType === 'TX', 'Alias target type TX');
})();

// ═══════════════════════════════════════════════════════════════
//  5. CONST — Declaration and fold propagation
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 5. CONST Declaration ---\u001b[0m');

(function() {
  const { ConstDeclarationNode, LiteralNode } = require('../core/ast');
  const constNode = new ConstDeclarationNode({
    identifier: 'PI',
    varType: 'SCL',
    valueExpr: new LiteralNode(3.14159, 'NUMBER', { line: 1, column: 1, depth: 0 })
  }, { line: 1, column: 1, depth: 0 });
  assert(constNode.type === 'ConstDeclaration', 'ConstDeclaration node type');
  assert(constNode.identifier === 'PI', 'CONST name PI');
  assert(constNode.valueExpr.value === 3.14159, 'CONST value 3.14159');
})();

(function() {
  const folder = new ASTConstantFolder();
  const result = folder.fold({
    type: 'Program',
    statements: [
      { type: 'ConstDeclaration', identifier: 'MAX', varType: 'NUM',
        valueExpr: { type: 'Literal', literalType: 'NUMBER', value: 100 } },
      { type: 'ShowStatement', expr: {
        type: 'Identifier', name: 'MAX'
      }}
    ]
  });
  assert(folder._consts.has('MAX'), 'CONST MAX collected');
  assert(folder._consts.get('MAX') === 100, 'CONST MAX = 100');
  // The Identifier reference to MAX is folded
  assert(result.statements[1].expr.type === 'Literal', 'MAX reference folded to Literal');
  assert(result.statements[1].expr.value === 100, 'MAX folded value = 100');
})();

// ═══════════════════════════════════════════════════════════════
//  6. Native File Primitives — C runtime declarations
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 6. Native File I/O C Runtime Declarations ---\u001b[0m');

(function() {
  const plant_runtime = require('fs').readFileSync(
    path.join(__dirname, '..', 'runtime/c/plant_runtime.h'), 'utf8');
  assert(plant_runtime.includes('plant_file_read'), 'plant_file_read declared in .h');
  assert(plant_runtime.includes('plant_file_write'), 'plant_file_write declared in .h');
  assert(plant_runtime.includes('plant_file_exists'), 'plant_file_exists declared in .h');
  assert(plant_runtime.includes('plant_file_delete'), 'plant_file_delete declared in .h');
  assert(plant_runtime.includes('plant_string_split'), 'plant_string_split declared in .h');
  assert(plant_runtime.includes('plant_string_trim'), 'plant_string_trim declared in .h');
  assert(plant_runtime.includes('plant_string_index_of'), 'plant_string_index_of declared in .h');
  assert(plant_runtime.includes('PlantArray'), 'PlantArray typedef declared in .h');
})();

(function() {
  const source = require('fs').readFileSync(
    path.join(__dirname, '..', 'runtime/c/plant_runtime.c'), 'utf8');
  assert(source.includes('plant_file_read'), 'plant_file_read implemented in .c');
  assert(source.includes('plant_file_write'), 'plant_file_write implemented in .c');
  assert(source.includes('plant_file_exists'), 'plant_file_exists implemented in .c');
  assert(source.includes('plant_file_delete'), 'plant_file_delete implemented in .c');
  assert(source.includes('plant_string_split'), 'plant_string_split implemented in .c');
  assert(source.includes('plant_string_trim'), 'plant_string_trim implemented in .c');
  assert(source.includes('plant_string_index_of'), 'plant_string_index_of implemented in .c');
})();

// ═══════════════════════════════════════════════════════════════
//  7. Native File I/O — JS interpreter parity
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 7. Native File I/O Interpreter Parity ---\u001b[0m');

(function() {
  cleanTmp();
  const testFile = path.join(tmpDir, 'test_read.txt');
  fs.writeFileSync(testFile, 'Hello PlantLang!');
  const content = fs.readFileSync(testFile, 'utf8');
  assert(content === 'Hello PlantLang!', 'file_read returns file content');
  assert(fs.existsSync(testFile), 'file_exists returns true for existing file');
  fs.unlinkSync(testFile);
  assert(!fs.existsSync(testFile), 'file_delete removes file');
  assert(!fs.existsSync(path.join(tmpDir, 'nonexistent.txt')), 'file_exists returns false for missing file');
})();

(function() {
  cleanTmp();
  const testFile = path.join(tmpDir, 'test_write.txt');
  fs.writeFileSync(testFile, 'Initial content');
  fs.writeFileSync(testFile, 'Overwritten content');
  const content = fs.readFileSync(testFile, 'utf8');
  assert(content === 'Overwritten content', 'file_write overwrites existing file');
  fs.unlinkSync(testFile);
})();

(function() {
  cleanTmp();
  const testDir = path.join(tmpDir, 'nested', 'dir');
  fs.mkdirSync(testDir, { recursive: true });
  const testFile = path.join(testDir, 'deep.txt');
  fs.writeFileSync(testFile, 'deep content');
  assert(fs.existsSync(testFile), 'file_exists works in nested directories');
  const content = fs.readFileSync(testFile, 'utf8');
  assert(content === 'deep content', 'file_read works in nested directories');
})();

// ═══════════════════════════════════════════════════════════════
//  8. String Manipulation — C runtime parity
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 8. String Manipulation ---\u001b[0m');

(function() {
  const input = '  hello world  ';
  const trimmed = input.trim();
  assert(trimmed === 'hello world', 'string_trim removes leading/trailing whitespace');
})();

(function() {
  const str = 'a,b,c,d';
  const parts = str.split(',');
  assert(parts.length === 4, 'string_split with comma delimiter yields 4 parts');
  assert(parts[0] === 'a', 'First split part is a');
  assert(parts[1] === 'b', 'Second split part is b');
  assert(parts[2] === 'c', 'Third split part is c');
  assert(parts[3] === 'd', 'Fourth split part is d');
})();

(function() {
  const str = 'hello';
  const idx1 = str.indexOf('ll');
  assert(idx1 === 2, 'string_index_of finds substring at correct position');
  const idx2 = str.indexOf('xyz');
  assert(idx2 === -1, 'string_index_of returns -1 for missing substring');
  const idx3 = str.indexOf('hello');
  assert(idx3 === 0, 'string_index_of finds substring at start');
})();

(function() {
  const str = 'a,,b,,c';
  const parts = str.split(',');
  assert(parts.length >= 3, 'string_split handles empty segments');
})();

(function() {
  const str = 'single';
  const parts = str.split(',');
  assert(parts.length === 1 && parts[0] === 'single', 'string_split with no delimiter returns single element');
})();

// ═══════════════════════════════════════════════════════════════
//  9. CodeWords Security — File I/O directives
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 9. CodeWords Governance for File I/O ---\u001b[0m');

(function() {
  const checker = new CodeWordsChecker([]);
  const violations = checker.checkAST({ type: 'Program', statements: [
    { type: 'FileReadStatement', filepath: '"test.txt"', identifier: 'data' }
  ]});
  assert(violations.length > 0, 'FileRead without #ALLOW_FILE_READ is rejected');
  assert(violations[0].nodeType === 'FileReadStatement', 'Violation references FileReadStatement');
})();

(function() {
  const checker = new CodeWordsChecker(['#ALLOW_FILE_READ']);
  const violations = checker.checkAST({ type: 'Program', statements: [
    { type: 'FileReadStatement', filepath: '"test.txt"', identifier: 'data' }
  ]});
  assert(violations.length === 0, 'FileRead with #ALLOW_FILE_READ passes');
})();

(function() {
  const checker = new CodeWordsChecker(['#ALLOW_FILE_WRITE']);
  const violations = checker.checkAST({ type: 'Program', statements: [
    { type: 'FileWriteStatement', filepath: '"test.txt"', identifier: 'count' }
  ]});
  assert(violations.length === 0, 'FileWrite with #ALLOW_FILE_WRITE passes');
})();

(function() {
  const checker = new CodeWordsChecker([]);
  assert(checker.hasDirective('#ALLOW_FILE_READ') === false, 'No directives: FILE_READ not allowed');
  assert(checker.hasDirective('#ALLOW_FILE_WRITE') === false, 'No directives: FILE_WRITE not allowed');
})();

// ═══════════════════════════════════════════════════════════════
//  10. AST Folder — Comparison folding
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 10. Comparison Constant Folding ---\u001b[0m');

(function() {
  const folder = new ASTConstantFolder();
  const result = folder.fold({
    type: 'Program',
    statements: [{
      type: 'ShowStatement', expr: {
        type: 'BinaryExpression', operator: 'GREATER_THAN',
        left: { type: 'Literal', literalType: 'NUMBER', value: 10 },
        right: { type: 'Literal', literalType: 'NUMBER', value: 5 }
      }
    }]
  });
  assert(result.statements[0].expr.value === true, '10 > 5 = true');
})();

(function() {
  const folder = new ASTConstantFolder();
  const result = folder.fold({
    type: 'Program',
    statements: [{
      type: 'ShowStatement', expr: {
        type: 'BinaryExpression', operator: 'LESS_THAN',
        left: { type: 'Literal', literalType: 'NUMBER', value: 3 },
        right: { type: 'Literal', literalType: 'NUMBER', value: 7 }
      }
    }]
  });
  assert(result.statements[0].expr.value === true, '3 < 7 = true');
})();

(function() {
  const folder = new ASTConstantFolder();
  const result = folder.fold({
    type: 'Program',
    statements: [{
      type: 'ShowStatement', expr: {
        type: 'UnaryExpression', operator: 'NOT',
        operand: { type: 'Literal', literalType: 'FACT', value: true }
      }
    }]
  });
  assert(result.statements[0].expr.value === false, 'NOT true = false');
})();

// ═══════════════════════════════════════════════════════════════
//  11. ENUM — auto-increment values
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 11. ENUM Auto-increment ---\u001b[0m');

(function() {
  const { EnumDeclarationNode } = require('../core/ast');
  const enumNode = new EnumDeclarationNode({ name: 'Weekday', members: [
    { name: 'MON', value: 0 },
    { name: 'TUE', value: 1 },
    { name: 'WED', value: 2 },
    { name: 'THU', value: 3 },
    { name: 'FRI', value: 4 },
    { name: 'SAT', value: 5 },
    { name: 'SUN', value: 6 },
  ]}, { line: 1, column: 1, depth: 0 });
  assert(enumNode.members[0].value === 0, 'MON = 0');
  assert(enumNode.members[3].value === 3, 'THU = 3');
  assert(enumNode.members[6].value === 6, 'SUN = 6');
})();

// ═══════════════════════════════════════════════════════════════
//  12. Parser — Verify new keywords are tokenized
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 12. Tokenizer Keywords ---\u001b[0m');

(function() {
  const tokenizer = require('../core/tokenizer');
  const tokens = tokenizer.tokenize('CONST x = 10.');
  const hasConst = tokens.some(t => t.type === 'KEYWORD' && t.value === 'CONST');
  assert(hasConst, 'CONST keyword tokenized');
})();

(function() {
  const tokenizer = require('../core/tokenizer');
  const tokens = tokenizer.tokenize('ENUM Color { RED, GREEN, BLUE }.');
  const hasEnum = tokens.some(t => t.type === 'KEYWORD' && t.value === 'ENUM');
  assert(hasEnum, 'ENUM keyword tokenized');
})();

(function() {
  const tokenizer = require('../core/tokenizer');
  const tokens = tokenizer.tokenize('TYPE MyNum = NUM.');
  const hasType = tokens.some(t => t.type === 'KEYWORD' && t.value === 'TYPE');
  assert(hasType, 'TYPE keyword tokenized');
})();

// ═══════════════════════════════════════════════════════════════
//  13. Integration: Folder handles nested expressions
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 13. Nested Expression Folding ---\u001b[0m');

(function() {
  const folder = new ASTConstantFolder();
  const result = folder.fold({
    type: 'Program',
    statements: [{
      type: 'ShowStatement', expr: {
        type: 'BinaryExpression', operator: '+',
        left: {
          type: 'BinaryExpression', operator: '*',
          left: { type: 'Literal', literalType: 'NUMBER', value: 2 },
          right: { type: 'Literal', literalType: 'NUMBER', value: 3 }
        },
        right: { type: 'Literal', literalType: 'NUMBER', value: 4 }
      }
    }]
  });
  assert(result.statements[0].expr.type === 'Literal', 'Nested (2*3)+4 folded to Literal');
  assert(result.statements[0].expr.value === 10, '(2*3)+4 = 10');
})();

// ═══════════════════════════════════════════════════════════════
//  Summary
// ═══════════════════════════════════════════════════════════════
cleanTmp();
console.log('');
console.log(`\u001b[1mResults: ${passed} passed, ${failed} failed\u001b[0m`);
if (failed > 0) process.exit(1);

}

main();
