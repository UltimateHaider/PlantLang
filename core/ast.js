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

// ═══════════════════════════════════════════════════════════════
//  WEATHER / SHELTER / CALM — error-handling block nodes.
//
//  PlantLang's existing grammar (see core/interpreter.js's legacy
//  handler) shapes this as a try/catch/finally-style construct, NOT
//  a conditional/if-branch:
//
//    WEATHER,
//      ...protected body, may raise a Storm...
//    SHELTER STORM_TYPE [AS errVar],
//      ...recovery body for that specific storm type...
//    SHELTER ANOTHER_STORM_TYPE,
//      ...another recovery body...
//    CALM.
//
//  Zero or more SHELTER clauses may follow WEATHER (each catching a
//  specific storm type, with "ANY_STORM" acting as a catch-all), and
//  the whole construct is sealed by a single CALM. This AST schema
//  preserves that exact existing semantic (verified end-to-end
//  against the legacy engine's WEATHER/SHELTER/CALM tests in
//  tests/all.plnt and tests/suite.plnt) while exposing an optional
//  `conditionExpr` field for forward compatibility, since the task
//  spec names it a "conditional" — it is left null for the current
//  unconditional try-block grammar and is reserved for a possible
//  future WEATHER IF [cond] variant without requiring another
//  breaking AST shape change.
// ═══════════════════════════════════════════════════════════════

class WeatherStatementNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {AstNode|string|null} [fields.conditionExpr] Reserved for a
   *        future conditional WEATHER variant; null for the current
   *        unconditional try-block grammar.
   * @param {Array} fields.bodyStatements Nested AST statement nodes
   *        forming the protected ("try") block.
   * @param {ShelterStatementNode[]} fields.shelterClauses Ordered list
   *        of SHELTER clauses attached to this WEATHER block.
   * @param {CalmStatementNode|null} fields.calmClause The terminating
   *        CALM clause (always present once parsing succeeds).
   * @param {{line,column,depth}} coords
   */
  constructor({ conditionExpr = null, bodyStatements, shelterClauses, calmClause }, coords) {
    super('WeatherStatement', coords);
    this.conditionExpr = conditionExpr;
    this.bodyStatements = bodyStatements || [];
    this.shelterClauses = shelterClauses || [];
    this.calmClause = calmClause || null;
  }
}

class ShelterStatementNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {string} fields.stormType  The storm type this clause catches
   *        (e.g. "ZERO_STORM"), or "ANY_STORM" as a catch-all.
   * @param {string|null} fields.errVar  Optional bound identifier capturing
   *        the storm's message text (from "SHELTER TYPE AS errVar").
   * @param {Array} fields.bodyStatements Nested AST statement nodes
   *        forming this clause's recovery ("catch") block.
   * @param {{line,column,depth}} coords
   */
  constructor({ stormType, errVar = null, bodyStatements }, coords) {
    super('ShelterStatement', coords);
    this.stormType = stormType;
    this.errVar = errVar;
    this.bodyStatements = bodyStatements || [];
  }
}

class CalmStatementNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {Array} [fields.bodyStatements] Nested AST statement nodes
   *        forming an optional "finally"-style block that always runs
   *        after WEATHER/SHELTER resolve. Empty for the current grammar,
   *        where CALM is purely a block-closing terminator with no body
   *        of its own — reserved for a future CALM-with-body extension.
   * @param {{line,column,depth}} coords
   */
  constructor({ bodyStatements = [] } = {}, coords) {
    super('CalmStatement', coords);
    this.bodyStatements = bodyStatements;
  }
}

// ═══════════════════════════════════════════════════════════════
//  ACTION / SPECIES / BLOOM / TAP — declaration and instantiation
//  nodes for the second compiler-frontend migration milestone.
// ═══════════════════════════════════════════════════════════════

class ActionDeclarationNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {string} fields.name  The action's identifier.
   * @param {Array<{name:string,type:string}>} fields.params Declared parameters.
   * @param {Array} fields.bodyStatements Nested AST statement nodes forming
   *        the action's executable body.
   * @param {boolean} [fields.isExternal] True for FFI declarations (no body).
   * @param {{name:string,type:string}|null} [fields.receiver] Optional receiver binding,
   *        e.g. {name:'self', type:'Point'} for methods.
   * @param {{line,column,depth}} coords
   */
  constructor({ name, params, bodyStatements, isExternal, receiver }, coords) {
    super('ActionDeclaration', coords);
    this.name = name;
    this.params = params || [];
    this.bodyStatements = bodyStatements || [];
    this.isExternal = !!isExternal;
    this.receiver = receiver || null;
  }
}

class MethodCallNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {AstNode} fields.target  The receiver expression (e.g. IdentifierNode for obj).
   * @param {string} fields.methodName  The called method name.
   * @param {Array<AstNode|string>} fields.args  Argument expressions.
   * @param {{line,column,depth}} coords
   */
  constructor({ target, methodName, args }, coords) {
    super('MethodCall', coords);
    this.target = target;
    this.methodName = methodName;
    this.args = args || [];
  }
}

class SpeciesDeclarationNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {string} fields.name  The species' identifier.
   * @param {string|null} fields.parentName  Optional PARENT species to inherit from.
   * @param {Array<{name:string,varType:string,defaultExpr:*}>} fields.fields
   *        Declared VAR fields with their default value expressions.
   * @param {ActionDeclarationNode[]} fields.actions Declared methods.
   * @param {{line,column,depth}} coords
   */
  constructor({ name, parentName = null, fields, actions }, coords) {
    super('SpeciesDeclaration', coords);
    this.name = name;
    this.parentName = parentName;
    this.fields = fields || [];
    this.actions = actions || [];
  }
}

class BloomStatementNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {string} fields.speciesName  The SPECIES being instantiated.
   * @param {string} fields.instanceIdent  The bound identifier for the new instance.
   * @param {{line,column,depth}} coords
   */
  constructor({ speciesName, instanceIdent }, coords) {
    super('BloomStatement', coords);
    this.speciesName = speciesName;
    this.instanceIdent = instanceIdent;
  }
}

class TapStatementNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {string} fields.filename  Quoted-string filename expression text.
   * @param {string} fields.mode  File open mode (e.g. "MARK", "READ").
   * @param {string} fields.handleIdent  The bound identifier for the opened handle.
   * @param {{line,column,depth}} coords
   */
  constructor({ filename, mode, handleIdent }, coords) {
    super('TapStatement', coords);
    this.filename = filename;
    this.mode = mode;
    this.handleIdent = handleIdent;
  }
}


class WheneverStatementNode extends AstNode {
  constructor({ watchIdent, bodyStatements }, coords) {
    super('WheneverStatement', coords);
    this.watchIdent = watchIdent;
    this.bodyStatements = bodyStatements || [];
  }
}


class ReapStatementNode extends AstNode {
  /**
   * Models all forms of REAP:
   *
   *   REAP var FROM action, args...         source:{kind:'ACTION', name}
   *   REAP var FROM lib:FUNCTION, args...   source:{kind:'LIBRARY', lib, fn}
   *   REAP var FROM obj:method [, args...]  source:{kind:'INSTANCE', obj, method}
   *   REAP var FROM SELF:method [, args...] source:{kind:'SELF', method}
   *   REAP var FROM NOW [FORMAT:X]          source:{kind:'NOW', format}
   *   REAP var FROM TYPEOF target           source:{kind:'TYPEOF', target}
   *
   * `variable` is the bound identifier string, or "_" to discard.
   * `args` is an array of raw expression strings (split on top-level commas,
   * NOT yet sub-parsed — they are evaluated at runtime via evalExpr, exactly
   * as the legacy engine's _splitArgs() + E() pipeline does, preserving
   * 100% semantic parity with the proven regex handlers).
   */
  constructor({ variable, source, args }, coords) {
    super('ReapStatement', coords);
    this.variable = variable;
    this.source   = source;   // { kind, name?, lib?, fn?, obj?, method?, format?, target? }
    this.args     = args || [];
  }
}



class SetStatementNode extends AstNode {
  constructor({ identifier, valueExpr }, coords) {
    super('SetStatement', coords);
    this.identifier = identifier;  // may be "x", "SELF:x", "obj:x", "obj:"key""
    this.valueExpr  = valueExpr;   // raw expression text → evalExpr at runtime
  }
}

class IncreaseStatementNode extends AstNode {
  constructor({ identifier, amountExpr }, coords) {
    super('IncreaseStatement', coords);
    this.identifier = identifier;
    this.amountExpr = amountExpr;
  }
}

class DecreaseStatementNode extends AstNode {
  constructor({ identifier, amountExpr }, coords) {
    super('DecreaseStatement', coords);
    this.identifier = identifier;
    this.amountExpr = amountExpr;
  }
}

class LenCallNode extends AstNode {
  constructor(arg, coords) {
    super('LenCall', coords);
    this.arg = arg;
  }
}

class CapCallNode extends AstNode {
  constructor(arg, coords) {
    super('CapCall', coords);
    this.arg = arg;
  }
}

class ListOpNode extends AstNode {
  constructor(arg, operation, coords) {
    super('ListOp', coords);
    this.arg = arg;
    this.operation = operation;
  }
}

class StringOpNode extends AstNode {
  constructor(arg1, arg2, operation, coords) {
    super('StringOp', coords);
    this.arg1 = arg1;
    this.arg2 = arg2;
    this.operation = operation;
  }
}

class IndexAccessNode extends AstNode {
  constructor(target, index, coords) {
    super('IndexAccess', coords);
    this.target = target;
    this.index = index;
  }
}

class ArrayLiteralNode extends AstNode {
  constructor(elements, coords) {
    super('ArrayLiteral', coords);
    this.elements = elements || [];
  }
}

class StructDeclarationNode extends AstNode {
  constructor({ name, fields }, coords) {
    super('StructDeclaration', coords);
    this.name = name;
    this.fields = fields || []; // [{ name, varType }]
  }
}

class StructInstantiationExpr extends AstNode {
  constructor({ structName, args }, coords) {
    super('StructInstantiation', coords);
    this.structName = structName;
    this.args = args || []; // expression nodes (positional)
  }
}

class StructLiteralNode extends AstNode {
  constructor({ structName, fields }, coords) {
    super('StructLiteral', coords);
    this.structName = structName;
    this.fields = fields || []; // [{ name, value: exprNode }]
  }
}

class MemberAccessNode extends AstNode {
  constructor({ object, member }, coords) {
    super('MemberAccess', coords);
    this.object = object;
    this.member = member;
  }
}

class ImportStatementNode extends AstNode {
  /**
   * @param {Object} fields
   * @param {string} fields.path  The imported file path (as written in source).
   * @param {string} fields.resolvedPath  The resolved absolute path.
   * @param {AstNode[]} fields.importedStatements  The merged AST nodes from the imported file.
   * @param {{line,column,depth}} coords
   */
  constructor({ path, resolvedPath, importedStatements }, coords) {
    super('ImportStatement', coords);
    this.path = path;
    this.resolvedPath = resolvedPath;
    this.importedStatements = importedStatements || [];
  }
}

module.exports = {
  AstNode,
  ProgramNode,
  CreateStatementNode,
  ShowStatementNode,
  IdentifierNode,
  LiteralNode,
  LenCallNode,
  CapCallNode,
  ListOpNode,
  StringOpNode,
  IndexAccessNode,
  StructDeclarationNode,
  StructInstantiationExpr,
  MemberAccessNode,
  ArrayLiteralNode,
  ImportStatementNode,
  ListenBranchStatementNode,
  ResponseStatementNode,
  WeatherStatementNode,
  ShelterStatementNode,
  CalmStatementNode,
  ActionDeclarationNode,
  MethodCallNode,
  SpeciesDeclarationNode,
  BloomStatementNode,
  TapStatementNode,
  WheneverStatementNode,
  ReapStatementNode,
  SetStatementNode,
  IncreaseStatementNode,
  DecreaseStatementNode,
};

// appended — remaining statement node types for full migration

class IfStatementNode extends AstNode {
  // IF cond, body [ORIF cond, body]* [ELSE, body]
  constructor({ branches }, coords) {
    super('IfStatement', coords);
    this.branches = branches || []; // [{cond:string|null, bodyStatements:[]}]
  }
}

class CycleStatementNode extends AstNode {
  // CYCLE var IN expr,  body  1\.
  // CYCLE var FROM lo TO hi [STEP n],  body  1\.
  constructor({ iterVar, sourceExpr, fromExpr, toExpr, stepExpr, bodyStatements }, coords) {
    super('CycleStatement', coords);
    this.iterVar        = iterVar;
    this.sourceExpr     = sourceExpr  || null;  // IN list
    this.fromExpr       = fromExpr    || null;  // FROM n
    this.toExpr         = toExpr      || null;  // TO m
    this.stepExpr       = stepExpr    || null;  // STEP k
    this.bodyStatements = bodyStatements || [];
  }
}

class SeasonStatementNode extends AstNode {
  // SEASON cond, body 1\.
  constructor({ condExpr, bodyStatements }, coords) {
    super('SeasonStatement', coords);
    this.condExpr       = condExpr;
    this.bodyStatements = bodyStatements || [];
  }
}

class MatchStatementNode extends AstNode {
  // Pattern-matching MATCH:
  //   MATCH expr { Variant1(binding) -> { stmts } Variant2 -> { stmts } }
  constructor({ subjectExpr, clauses }, coords) {
    super('MatchStatement', coords);
    this.subjectExpr = subjectExpr;
    this.clauses     = clauses || []; // [{variantName, binding, bodyStatements}]
  }
}

class GiveStatementNode extends AstNode {
  // GIVE expr.  (return from ACTION)
  constructor({ valueExpr }, coords) {
    super('GiveStatement', coords);
    this.valueExpr = valueExpr;
  }
}

class StopIfStatementNode extends AstNode {
  // STOP IF cond [, action]
  constructor({ condExpr, actionExpr }, coords) {
    super('StopIfStatement', coords);
    this.condExpr   = condExpr;
    this.actionExpr = actionExpr || null;
  }
}

class PutStatementNode extends AstNode {
  // PUT val INTO list / SELF:list
  constructor({ valueExpr, targetExpr }, coords) {
    super('PutStatement', coords);
    this.valueExpr  = valueExpr;
    this.targetExpr = targetExpr;
  }
}

class TakeStatementNode extends AstNode {
  // TAKE val FROM list
  constructor({ valueExpr, listExpr }, coords) {
    super('TakeStatement', coords);
    this.valueExpr = valueExpr;
    this.listExpr  = listExpr;
  }
}

class LinkStatementNode extends AstNode {
  // LINK "key" WITH val IN map
  constructor({ keyExpr, valueExpr, mapIdent }, coords) {
    super('LinkStatement', coords);
    this.keyExpr   = keyExpr;
    this.valueExpr = valueExpr;
    this.mapIdent  = mapIdent;
  }
}

class SortStatementNode extends AstNode {
  constructor({ listIdent }, coords) {
    super('SortStatement', coords);
    this.listIdent = listIdent;
  }
}

class ShakeStatementNode extends AstNode {
  constructor({ listIdent }, coords) {
    super('ShakeStatement', coords);
    this.listIdent = listIdent;
  }
}

class EvaporateStatementNode extends AstNode {
  constructor({ identifier }, coords) {
    super('EvaporateStatement', coords);
    this.identifier = identifier;
  }
}

class LockStatementNode extends AstNode {
  constructor({ identifier }, coords) {
    super('LockStatement', coords);
    this.identifier = identifier;
  }
}

class BraidStatementNode extends AstNode {
  // BRAID a WITH b AS result [MAP]
  constructor({ list1, list2, resultIdent, asMap }, coords) {
    super('BraidStatement', coords);
    this.list1       = list1;
    this.list2       = list2;
    this.resultIdent = resultIdent;
    this.asMap       = asMap || false;
  }
}

class HarvestStatementNode extends AstNode {
  // HARVEST "url" [METHOD:x] [BODY:y] [HEADERS:h] [TIMEOUT:n] AS result
  constructor({ urlExpr, method, bodyExpr, headersIdent, timeoutExpr, resultIdent }, coords) {
    super('HarvestStatement', coords);
    this.urlExpr      = urlExpr;
    this.method       = method || 'GET';
    this.bodyExpr     = bodyExpr     || null;
    this.headersIdent = headersIdent || null;
    this.timeoutExpr  = timeoutExpr  || null;
    this.resultIdent  = resultIdent;
  }
}

class AnalyzeStatementNode extends AstNode {
  constructor({ identifier }, coords) {
    super('AnalyzeStatement', coords);
    this.identifier = identifier;
  }
}

class WaitStatementNode extends AstNode {
  constructor({ secsExpr }, coords) {
    super('WaitStatement', coords);
    this.secsExpr = secsExpr;
  }
}

class ShowVerifySummaryNode extends AstNode {
  constructor(coords) { super('ShowVerifySummary', coords); }
}

class VerifyStatementNode extends AstNode {
  // VERIFY "label", assertion
  constructor({ label, assertion }, coords) {
    super('VerifyStatement', coords);
    this.label     = label;
    this.assertion = assertion;
  }
}

class SuiteStatementNode extends AstNode {
  // SUITE "name", ...body... SUITE/.
  constructor({ name, bodyStatements }, coords) {
    super('SuiteStatement', coords);
    this.name           = name;
    this.bodyStatements = bodyStatements || [];
  }
}

class PlantStatementNode extends AstNode {
  constructor({ libName }, coords) {
    super('PlantStatement', coords);
    this.libName = libName;
  }
}

class MissionStatementNode extends AstNode {
  constructor({ mode }, coords) {
    super('MissionStatement', coords);
    this.mode = mode;
  }
}

class MissionBlockNode extends AstNode {
  /**
   * Wraps an ACTION declaration with a mission mode for the 5-Mission Architecture.
   * @param {Object} fields
   * @param {string} fields.mode  'BALANCED' | 'FAST' | 'SAFE' | 'SMART' | 'PERSISTENT'
   * @param {number} fields.scopeId  Auto-incrementing scope identifier
   * @param {ActionDeclarationNode} fields.action  The wrapped action
   * @param {{line,column,depth}} coords
   */
  constructor({ mode, scopeId, action }, coords) {
    super('MissionBlock', coords);
    this.mode = mode || 'BALANCED';
    this.scopeId = scopeId;
    this.action = action;
  }
}

class RootStatementNode extends AstNode {
  // ROOT name TO expr
  constructor({ identifier, valueExpr }, coords) {
    super('RootStatement', coords);
    this.identifier = identifier;
    this.valueExpr  = valueExpr;
  }
}

class RootScopeStatementNode extends AstNode {
  // ROOT_SCOPE name, LINK ... ROOT_SCOPE/.
  constructor({ identifier, links }, coords) {
    super('RootScopeStatement', coords);
    this.identifier = identifier;
    this.links      = links || []; // [{key,valueExpr}]
  }
}

class VariantDeclarationNode extends AstNode {
  // CHOICE Name { Variant1(Type), Variant2 }
  constructor({ name, variants }, coords) {
    super('VariantDeclaration', coords);
    this.name = name;
    this.variants = variants || []; // [{ name, type }] — type may be null for empty variants
  }
}

class KeyValuePairNode extends AstNode {
  // key: value  (inside MAP literal)
  constructor({ key, value }, coords) {
    super('KeyValuePair', coords);
    this.key = key;
    this.value = value;
  }
}

class MapLiteralNode extends AstNode {
  // { key: value, key: value, ... }
  constructor({ entries }, coords) {
    super('MapLiteral', coords);
    this.entries = entries || []; // [KeyValuePairNode]
  }
}

class FlowStatementNode extends AstNode {
  // REAP x FROM src FLOW a FLOW b ...
  // (handled by ReapStatement with flowChain)
  constructor({ variable, sourceExpr, flowChain, args }, coords) {
    super('FlowStatement', coords);
    this.variable   = variable;
    this.sourceExpr = sourceExpr;
    this.flowChain  = flowChain || [];
    this.args       = args || [];
  }
}

class ForInStatementNode extends AstNode {
  // FOR item IN collection,
  //   body...
  // .
  constructor({ iterVar, sourceExpr, bodyStatements }, coords) {
    super('ForInStatement', coords);
    this.iterVar        = iterVar;
    this.sourceExpr     = sourceExpr;
    this.bodyStatements = bodyStatements || [];
  }
}

class MethodCallStatementNode extends AstNode {
  // obj:method(args).
  constructor({ target, methodName, params, args }, coords) {
    super('MethodCallStatement', coords);
    this.target = target;         // IdentifierNode — the instance
    this.methodName = methodName; // string
    this.params = params || [];   // [{name, type}] from declaration
    this.args = args || [];       // [AstNode] argument expressions
  }
}

class SelfExpressionNode extends AstNode {
  // SELF — reference to the current instance inside a method
  constructor(coords) {
    super('SelfExpression', coords);
  }
}

class BloomExpressionNode extends AstNode {
  // BLOOM SpeciesName — instantiation operator (expression form)
  constructor({ speciesName }, coords) {
    super('BloomExpression', coords);
    this.speciesName = speciesName;
  }
}

// ── v0.38.0: Block Delimiter Nodes ──
class EndBlockNode extends AstNode {
  constructor({ blockType }, coords) {
    super('EndBlock', coords);
    this.blockType = blockType; // e.g. 'IF', 'CYCLE', 'ACTION', 'SPECIES', 'SEASON'
  }
}

class BranchElseNode extends AstNode {
  constructor({ bodyStatements }, coords) {
    super('BranchElse', coords);
    this.bodyStatements = bodyStatements || [];
  }
}

class BlockDelimiterNode extends AstNode {
  constructor({ delimiter, bodyStatements }, coords) {
    super('BlockDelimiter', coords);
    this.delimiter = delimiter; // e.g. '.', '}', '/IF.', etc.
    this.bodyStatements = bodyStatements || [];
  }
}

// ── v0.38.0: CycleInStatement (CYCLE x [, idx] IN list { ... }) ──
class CycleInStatementNode extends AstNode {
  constructor({ iterVar, indexVar, listExpr, bodyStatements }, coords) {
    super('CycleInStatement', coords);
    this.iterVar = iterVar;
    this.indexVar = indexVar || null;
    this.listExpr = listExpr;
    this.bodyStatements = bodyStatements || [];
  }
}

// ── v0.38.0: BreakStatement / ContinueStatement ──
class BreakStatementNode extends AstNode {
  constructor(coords) {
    super('BreakStatement', coords);
  }
}

class ContinueStatementNode extends AstNode {
  constructor(coords) {
    super('ContinueStatement', coords);
  }
}

// ── v0.38.0: Multi-field SortStatement ──
class SortStatementV2Node extends AstNode {
  constructor({ listExpr, fields, direction }, coords) {
    super('SortStatementV2', coords);
    this.listExpr = listExpr;              // the list variable name or expression
    this.fields = fields || [];            // [{ field: "name", direction: "ASC"|"DESC" }]
    this.direction = direction || 'ASC';   // fallback simple direction
  }
}

// ── v0.38.0: BloomAsStatement (BLOOM data AS GRAPH|TABLE|CHART { config }) ──
class BloomAsStatementNode extends AstNode {
  constructor({ dataExpr, targetType, configMap }, coords) {
    super('BloomAsStatement', coords);
    this.dataExpr = dataExpr;
    this.targetType = targetType; // 'GRAPH', 'TABLE', 'CHART'
    this.configMap = configMap || {};
  }
}

// ── v0.43.0: ConstDeclaration ──
class ConstDeclarationNode extends AstNode {
  constructor({ identifier, varType, valueExpr }, coords) {
    super('ConstDeclaration', coords);
    this.identifier = identifier;
    this.varType = varType;
    this.valueExpr = valueExpr;
  }
}

// ── v0.43.0: EnumDeclaration ──
class EnumDeclarationNode extends AstNode {
  constructor({ name, members }, coords) {
    super('EnumDeclaration', coords);
    this.name = name;
    this.members = members || []; // [{ name, value: auto-increment int }]
  }
}

// ── v0.43.0: TypeAliasDeclaration ──
class TypeAliasDeclarationNode extends AstNode {
  constructor({ alias, targetType }, coords) {
    super('TypeAliasDeclaration', coords);
    this.alias = alias;
    this.targetType = targetType;
  }
}

// Re-export everything
const _orig = module.exports;
Object.assign(module.exports, {
  StructDeclarationNode,
  StructInstantiationExpr,
  StructLiteralNode,
  MemberAccessNode,
  ArrayLiteralNode,
  MethodCallNode,
  IfStatementNode,
  CycleStatementNode,
  SeasonStatementNode,
  MatchStatementNode,
  GiveStatementNode,
  StopIfStatementNode,
  PutStatementNode,
  TakeStatementNode,
  LinkStatementNode,
  SortStatementNode,
  ShakeStatementNode,
  EvaporateStatementNode,
  LockStatementNode,
  BraidStatementNode,
  HarvestStatementNode,
  AnalyzeStatementNode,
  WaitStatementNode,
  ShowVerifySummaryNode,
  VerifyStatementNode,
  SuiteStatementNode,
  PlantStatementNode,
  MissionStatementNode,
  MissionBlockNode,
  RootStatementNode,
  RootScopeStatementNode,
  FlowStatementNode,
  VariantDeclarationNode,
  KeyValuePairNode,
  MapLiteralNode,
  ForInStatementNode,
  MethodCallStatementNode,
  SelfExpressionNode,
  BloomExpressionNode,
  EndBlockNode,
  BranchElseNode,
  BlockDelimiterNode,
  CycleInStatementNode,
  BreakStatementNode,
  ContinueStatementNode,
  SortStatementV2Node,
  BloomAsStatementNode,
  ConstDeclarationNode,
  EnumDeclarationNode,
  TypeAliasDeclarationNode,
});

// ── v0.38.0: BREAK/CONTINUE control flow signals ──
class BreakSignalException extends Error {
  constructor() { super('BREAK signal'); this.name = 'BreakSignalException'; }
}
class ContinueSignalException extends Error {
  constructor() { super('CONTINUE signal'); this.name = 'ContinueSignalException'; }
}
// Also export these
module.exports.BreakSignalException = BreakSignalException;
module.exports.ContinueSignalException = ContinueSignalException;
