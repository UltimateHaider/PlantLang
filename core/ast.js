'use strict';
// ═══════════════════════════════════════════════════════════════
//  core/ast.js — formal Abstract Syntax Tree node schema for the
//  compiler-frontend migration (tokenizer -> parser -> AST).
//
//  Every node extends AstNode and explicitly records its origin
//  coordinates {line, column, depth}, derived from the starting
//  token that produced it — this is what lets core/diagnostics.js
//  render the caret (^) at the exact offending position even when
//  errors surface deep inside nested AST evaluation rather than at
//  the flat-statement level.
//
//  Migration note: the engine's legacy regex-statement interpreter
//  (core/interpreter.js, core/lexer.js) remains the path that the
//  176-test regression matrix runs through. This AST schema and its
//  companion core/parser.js / core/tokenizer.js are being adopted
//  incrementally, statement type by statement type (SHOW and CREATE
//  first per the migration plan), verified at each milestone via
//  tests/test_parser_migration.js before any further statement kind
//  is moved over.
// ═══════════════════════════════════════════════════════════════

class AstNode {
  /**
   * @param {string} type
   * @param {{line:number, column:number, depth:number}} coords
   */
  constructor(type, coords = {}) {
    this.type = type;
    this.line = coords.line;
    this.column = coords.column;
    this.depth = coords.depth;
  }
}

class ProgramNode extends AstNode {
  /** @param {AstNode[]} statements */
  constructor(statements = []) {
    super('Program', { line: 1, column: 1, depth: 0 });
    this.statements = statements;
  }
}

class CreateStatementNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {string} fields.identifier  Variable name being declared
   * @param {string} fields.varType     NUM | SCL | TX | FACT | LIST | MAP
   * @param {AstNode|string} fields.valueExpr  Parsed expression node (or
   *        raw expression text, while expression-sub-parsing is still
   *        delegated to the legacy evaluator during this migration phase)
   * @param {{line,column,depth}} coords
   */
  constructor({ identifier, varType, valueExpr }, coords) {
    super('CreateStatement', coords);
    this.identifier = identifier;
    this.varType = varType;
    this.valueExpr = valueExpr;
  }
}

class ShowStatementNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {AstNode|string} fields.expr  Expression to print
   * @param {{line,column,depth}} coords
   */
  constructor({ expr }, coords) {
    super('ShowStatement', coords);
    this.expr = expr;
  }
}

class IdentifierNode extends AstNode {
  constructor(name, coords) {
    super('Identifier', coords);
    this.name = name;
  }
}

class LiteralNode extends AstNode {
  /** @param {*} value  @param {'NUMBER'|'STRING'|'FACT'} literalType */
  constructor(value, literalType, coords) {
    super('Literal', coords);
    this.value = value;
    this.literalType = literalType;
  }
}

class ListenBranchStatementNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {string} fields.portExpr    Raw expression text for the port (e.g. "8080" or a var)
   * @param {string} fields.configExpr  Raw expression text for the config (e.g. "cfg" — a MAP var)
   * @param {string} fields.requestIdent The bound request-MAP identifier (e.g. "req")
   * @param {Array}  fields.bodyStatements Nested statement records forming the handler body
   * @param {number} [fields.line]   legacy positional fallback (pre-AstNode call sites)
   * @param {number} [fields.column] legacy positional fallback
   * @param {{line,column,depth}} [coords]
   */
  constructor({ portExpr, configExpr, requestIdent, bodyStatements, line, column }, coords) {
    super('ListenBranchStatement', coords || { line, column, depth: undefined });
    this.portExpr = portExpr;
    this.configExpr = configExpr;
    this.requestIdent = requestIdent;
    this.bodyStatements = bodyStatements || [];
  }
}

class ResponseStatementNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {string} fields.responseExpr Raw expression text to be sent back as the response
   * @param {number} [fields.line]   legacy positional fallback
   * @param {number} [fields.column] legacy positional fallback
   * @param {{line,column,depth}} [coords]
   */
  constructor({ responseExpr, line, column }, coords) {
    super('ResponseStatement', coords || { line, column, depth: undefined });
    this.responseExpr = responseExpr;
  }
}

module.exports = {
  AstNode,
  ProgramNode,
  CreateStatementNode,
  ShowStatementNode,
  IdentifierNode,
  LiteralNode,
  ListenBranchStatementNode,
  ResponseStatementNode,
};
