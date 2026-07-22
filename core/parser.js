'use strict';
// ═══════════════════════════════════════════════════════════════
//  core/parser.js — Recursive-descent parser (Phase 1 of the
//  compiler-frontend migration).
//
//  Consumes the flat Token[] stream produced by core/tokenizer.js
//  and builds a formal AST (core/ast.js) via peek()/consume()
//  look-ahead, exactly as specified. On any grammar violation, it
//  throws a localized SYNTAX_STORM carrying the exact breaking
//  token's {line, column} so core/diagnostics.js can render the
//  caret precisely beneath the offending character — never a bare
//  generic panic.
//
//  MIGRATION SCOPE (this milestone): SHOW and CREATE are fully
//  parsed into typed AST nodes. Every other statement kind is
//  currently passed through as a RawStatementNode wrapping its
//  token span's reconstructed text, so the parser can walk an
//  entire real-world .plnt file end-to-end without crashing on
//  constructs not yet migrated — satisfying the task's explicit
//  "incremental, verify at every milestone" constraint instead of
//  silently truncating or breaking unmigrated programs.
// ═══════════════════════════════════════════════════════════════

const fs = require('fs');
const path = require('path');
const { tokenize, TOKEN } = require('./tokenizer');

// Determine the project root (parent of core/) for std/ module resolution
const PROJECT_ROOT = path.resolve(__dirname, '..');
const STD_DIR = path.join(PROJECT_ROOT, 'std');
const { storm, PlantStorm } = require('./runtime');
const {
  ProgramNode, CreateStatementNode, ShowStatementNode,
  IdentifierNode, LiteralNode, AstNode,
  LenCallNode, CapCallNode, ListOpNode, IndexAccessNode,
  ListenBranchStatementNode, ResponseStatementNode,
  WeatherStatementNode, ShelterStatementNode, CalmStatementNode,
  ActionDeclarationNode, SpeciesDeclarationNode, BloomStatementNode, TapStatementNode,
  WheneverStatementNode, ReapStatementNode, ImportStatementNode,
  SetStatementNode, IncreaseStatementNode, DecreaseStatementNode,
  StructDeclarationNode, StructInstantiationExpr, StructLiteralNode, MemberAccessNode,
  ArrayLiteralNode, MethodCallNode,
} = require('./ast');


/**
 * Reconstruct source text from a token span, with comma/colon/paren
 * punctuation joined without preceding space — matching the original
 * source formatting that the legacy regex handlers were designed for.
 */
function joinTokens(span) {
  let out = '';
  for (let i = 0; i < span.length; i++) {
    const t = span[i];
    const val = t.type === TOKEN.STRING ? `"${t.value}"` : String(t.value);
    // Punctuation that binds to the PREVIOUS token without a space: , : ) /
    const bindLeft = t.type === TOKEN.PUNCT && ',:.)/'.includes(t.value);
    // Previous token was a punctuation that binds right (no space after it): ( :
    const prev = span[i - 1];
    const prevBindRight = prev && prev.type === TOKEN.PUNCT && '(:'.includes(prev.value);
    if (i === 0 || bindLeft || prevBindRight) {
      out += val;
    } else {
      out += ' ' + val;
    }
  }
  return out;
}

class RawStatementNode extends AstNode {
  /** Fallback wrapper for statement kinds not yet migrated to a typed node. */
  constructor(text, coords) {
    super('RawStatement', coords);
    this.text = text;
  }
}

class Parser {
  constructor(tokens) {
    this.tokens = tokens;
    this.pos = 0;
    this.structNames = new Set(); // known struct names for literal disambiguation
    this.currentDepth = 0;         // semantic block depth for Depth Contract Law
  }

  peek(offset = 0) {
    return this.tokens[Math.min(this.pos + offset, this.tokens.length - 1)];
  }

  current() { return this.peek(0); }

  isAtEnd() { return this.current().type === TOKEN.EOF; }

  advance() {
    const t = this.current();
    if (!this.isAtEnd()) this.pos++;
    return t;
  }

  /** Consume a token expected to match (type, value?); throws SYNTAX_STORM if not. */
  consume(type, value, expectedDescription) {
    const t = this.current();
    const typeMatches = t.type === type;
    const valueMatches = value === undefined || (typeof t.value === 'string' && typeof value === 'string'
      ? t.value.toUpperCase() === value.toUpperCase()
      : t.value === value);
    if (!typeMatches || !valueMatches) {
      const found = t.type === TOKEN.EOF ? '(end of file)' : `"${t.value}"`;
      storm('SYNTAX_STORM',
        `Expected ${expectedDescription || (value !== undefined ? `"${value}"` : type)}, found ${found}`,
        t.line, t.column);
    }
    return this.advance();
  }

  match(type, value) {
    const t = this.current();
    if (t.type !== type) return false;
    if (value !== undefined) {
      if (typeof t.value === 'string' && typeof value === 'string') {
        if (t.value.toUpperCase() !== value.toUpperCase()) return false;
      } else if (t.value !== value) return false;
    }
    return true;
  }

  /**
   * Skip every token up to and including the next statement-terminating
   * period at the given baseline depth — used both for normal raw
   * statement consumption and for grammar-error recovery boundaries.
   */
  skipToPeriodAtDepth(depth) {
    const startTokens = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.PUNCT && t.value === '.' && t.depth === depth) {
        startTokens.push(this.advance());
        break;
      }
      startTokens.push(this.advance());
    }
    return startTokens;
  }

  /**
   * Block-Depth Contract Law enforcement.  Checks that the current
   * semantic depth (this.currentDepth) is within [minDepth, maxDepth].
   * Throws a SYNTAX_STORM with a [DepthContractError] message otherwise.
   */
  enforceDepthContract(nodeType, minDepth, maxDepth = Infinity, token) {
    if (this.currentDepth < minDepth || this.currentDepth > maxDepth) {
      const maxStr = maxDepth === Infinity ? '∞' : maxDepth;
      storm('SYNTAX_STORM',
        `[DepthContractError] Cannot declare '${nodeType}' at Depth ${this.currentDepth}. Expected depth range: [${minDepth}..${maxStr}] at Line ${token.line}, Col ${token.column}.`,
        token.line, token.column);
    }
  }

  // ── Top level ────────────────────────────────────────────────
  parseProgram() {
    const statements = [];
    while (!this.isAtEnd()) {
      // DEPTH tokens are structural markers — skip them.
      if (this.match(TOKEN.DEPTH)) { this.advance(); continue; }
      // "  N\" style (leading-space depth prefix) → NUMBER + PUNCT("\")
      // Treat as depth marker and parse the real statement that follows.
      if (this.current().type === TOKEN.NUMBER &&
          this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '\\') {
        this.advance(); this.advance(); // consume N and         continue;
      }
      const stmt = this.parseStatement();
      if (stmt) statements.push(stmt);
    }
    return new ProgramNode(statements);
  }

  parseStatement() {
    const startTok = this.current();
    const coords = { line: startTok.line, column: startTok.column, depth: startTok.depth };

    if (this.match(TOKEN.KEYWORD, 'IMPORT')) return this.parseImportStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'SHOW')) return this.parseShowStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'CREATE')) return this.parseCreateStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'LISTEN') && this.peek(1).type === TOKEN.KEYWORD && this.peek(1).value === 'BRANCH') {
      return this.parseListenBranchStatement(coords);
    }
    if (this.match(TOKEN.KEYWORD, 'WEATHER')) return this.parseWeatherStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'NATIVE')) {
      this.advance();
      const action = this.parseActionDeclaration(coords);
      action.isExternal = true;
      return action;
    }
    if (this.match(TOKEN.KEYWORD, 'ACTION')) return this.parseActionDeclaration(coords);
    if (this.match(TOKEN.KEYWORD, 'SPECIES')) return this.parseSpeciesDeclaration(coords);
    if (this.match(TOKEN.KEYWORD, 'BLOOM')) return this.parseBloomStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'TAP')) return this.parseTapStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'INFUSE')) return this.parseInfuseStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'ABSORB')) return this.parseAbsorbStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'SEAL'))   return this.parseSealStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'EMPTY'))  return this.parseEmptyStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'WHENEVER')) return this.parseWheneverStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'REAP')) return this.parseReapStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'SET')) return this.parseSetStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'INCREASE')) return this.parseIncreaseStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'DECREASE')) return this.parseDecreaseStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'SHAPE'))  return this.parseStructDeclaration(coords);
    if (this.match(TOKEN.KEYWORD, 'STRUCT')) return this.parseStructDeclaration(coords);
    if (this.match(TOKEN.KEYWORD, 'CHOICE')) return this.parseVariantDeclaration(coords);

    // New dispatches — remaining statement types
    if (this.match(TOKEN.KEYWORD, 'FOR'))    return this.parseForInStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'IF'))     return this.parseIfStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'CYCLE'))  return this.parseCycleStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'SEASON')) return this.parseSeasonStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'MATCH'))  return this.parseMatchStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'GIVE') && this.containsResponseAheadOnLine())
      return this.parseResponseStatementAst(coords);
    if (this.match(TOKEN.KEYWORD, 'GIVE'))   return this.parseGiveStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'STOP'))   return this.parseStopIfStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'PUT'))    return this.parsePutStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'TAKE'))   return this.parseTakeStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'LINK'))   return this.parseLinkStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'SORT'))   return this.parseSortStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'SHAKE'))  return this.parseShakeStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'EVAPORATE')) return this.parseEvaporateStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'LOCK'))   return this.parseLockStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'BRAID'))  return this.parseBraidStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'HARVEST'))return this.parseHarvestStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'ANALYZE'))return this.parseAnalyzeStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'WAIT'))   return this.parseWaitStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'VERIFY')) return this.parseVerifyStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'SUITE'))  return this.parseSuiteStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'PLANT'))  return this.parsePlantStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'MISSION'))return this.parseMissionStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'ROOT')) {
      if (this.peek(1).value === 'SCOPE' || (this.peek(1).type === TOKEN.IDENT && false)) {
        // peek for ROOT_SCOPE (tokenizer emits ROOT then SCOPE as separate tokens)
      }
      return this.parseRootStatement(coords);
    }
    if (this.match(TOKEN.KEYWORD, 'ROOT_SCOPE')) return this.parseRootScopeStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'SHOW_VERIFY_SUMMARY')) {
      this.advance();
      if (this.match(TOKEN.PUNCT, '.')) this.advance();
      const {ShowVerifySummaryNode} = require('./ast');
      return new ShowVerifySummaryNode(coords);
    }
    // Closer tokens — skip silently
    if (this.match(TOKEN.KEYWORD, 'ORIF'))  { this._skipToEOL(); return null; }
    if (this.match(TOKEN.KEYWORD, 'ELSE'))  { this._skipToEOL(); return null; }
    if (this.match(TOKEN.KEYWORD, 'CALM'))  { this._skipToEOL(); return null; }
    if (this.match(TOKEN.KEYWORD, 'SHELTER')) { this._skipToEOL(); return null; }

    if (this.match(TOKEN.KEYWORD, 'GIVE') && this.containsResponseAheadOnLine()) {
      return this.parseResponseStatementAst(coords);
    }

    // Method call statement: obj:method(args).
    if (this.current().type === TOKEN.IDENT &&
        this.peek(1) && this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === ':' &&
        this.peek(2) && (this.peek(2).type === TOKEN.IDENT || this.peek(2).type === TOKEN.KEYWORD) &&
        this.peek(3) && this.peek(3).type === TOKEN.PUNCT && this.peek(3).value === '(') {
      return this.parseMethodCallStatement(coords);
    }

    // Not yet migrated — consume through to the terminating period (or a
    // depth-marker boundary acting as an implicit terminator, matching the
    // legacy lexer's own leniency) and wrap as a RawStatementNode so the
    // parser can traverse a whole real program without aborting.
    const span = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.PUNCT && t.value === '.') { span.push(this.advance()); break; }
      if (t.type === TOKEN.DEPTH && span.length > 0) break; // next statement begins
      span.push(this.advance());
    }
    const text = joinTokens(span.filter(t => t.type !== TOKEN.PUNCT || t.value !== '.'));
    return new RawStatementNode(text, coords);
  }

  /**
   * LISTEN BRANCH ON [portExpr] WITH [configExpr] AS [requestIdent] MAP,
   *   ...bodyStatements (recursively parsed, depth > header depth)...
   * LISTEN/.
   *
   * Each connective keyword (ON/WITH/AS/MAP) is checked independently so
   * a missing or misspelled one throws SYNTAX_STORM with the caret aimed
   * at the exact token found in its place — not just the statement start.
   */
  parseListenBranchStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'LISTEN', '"LISTEN"');
    this.consume(TOKEN.KEYWORD, 'BRANCH', '"BRANCH"');

    this.consume(TOKEN.KEYWORD, 'ON', '"ON" after BRANCH');
    const portExpr = this.parseHeaderExpressionUntilKeyword('WITH');

    this.consume(TOKEN.KEYWORD, 'WITH', '"WITH" after the port expression');
    const configExpr = this.parseHeaderExpressionUntilKeyword('AS');

    this.consume(TOKEN.KEYWORD, 'AS', '"AS" after the config expression');

    const identTok = this.current();
    if (identTok.type !== TOKEN.IDENT) {
      storm('SYNTAX_STORM',
        `Expected a request identifier after AS, found "${identTok.value}"`,
        identTok.line, identTok.column);
    }
    const requestIdent = this.advance().value;

    this.consume(TOKEN.KEYWORD, 'MAP', '"MAP" after the request identifier');
    this.consume(TOKEN.PUNCT, ',', '"," to open the LISTEN BRANCH body');

    // Recursively collect the nested body: every statement whose depth is
    // greater than the header's depth, until the matching "LISTEN/." closer
    const headerDepth = coords.depth;
    const bodyTokenStatements = [];
    while (!this.isAtEnd()) {
      // Skip DEPTH tokens
      const aheadIsDepth = this.match(TOKEN.DEPTH);
      const afterDepthOffset = aheadIsDepth ? 1 : 0;

      // Skip N\\ style depth markers (NUMBER + PUNCT "\")
      const isNumBackslash = !aheadIsDepth &&
        this.current().type === TOKEN.NUMBER &&
        this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '\\';
      if (isNumBackslash) {
        this.advance(); this.advance(); // skip N and \
        if (this.isAtEnd()) break;
      }

      const lineFirstTok = aheadIsDepth ? this.peek(1) : this.current();
      const lineSecondTok = aheadIsDepth ? this.peek(2) : this.peek(1);

      if (lineFirstTok.type === TOKEN.KEYWORD && lineFirstTok.value === 'LISTEN' &&
          lineSecondTok.type === TOKEN.PUNCT && lineSecondTok.value === '/') {
        if (aheadIsDepth) this.advance();
        break;
      }

      if (aheadIsDepth && this.peek(0).value <= headerDepth && !isNumBackslash) {
        const t = this.current();
        storm('SYNTAX_STORM',
          'LISTEN BRANCH: body statement must be nested deeper than the header (missing "LISTEN/." closer?)',
          t.line, t.column);
      }
      if (aheadIsDepth) this.advance();
      const stmt = this.parseStatement();
      if (stmt) bodyTokenStatements.push(stmt);
    }
    if (this.isAtEnd()) {
      storm('SYNTAX_STORM', 'LISTEN BRANCH: missing closing "LISTEN/." for this block', coords.line, coords.column);
    }
    this.consume(TOKEN.KEYWORD, 'LISTEN', '"LISTEN" in the closing "LISTEN/."');
    this.consume(TOKEN.PUNCT, '/', '"/" in the closing "LISTEN/."');
    if (this.match(TOKEN.PUNCT, '.')) this.advance();

    return new ListenBranchStatementNode(
      { portExpr, configExpr, requestIdent, bodyStatements: bodyTokenStatements },
      coords
    );
  }

  /**
   * Shared depth-aware nested-body collector: gathers statements whose
   * depth is strictly greater than `headerDepth`, stopping as soon as a
   * line at depth <= headerDepth is reached. The `isStopLine(tok)`
   * predicate lets the caller recognize its own specific closer/sibling
   * keyword (e.g. "SHELTER", "CALM", "LISTEN/.") without consuming it —
   * the stop line is left in place for the caller to consume explicitly.
   *
   * This generalizes the body-collection pattern proven correct in
   * parseListenBranchStatement (including its DEPTH-token consumption
   * fix — a body statement's leading DEPTH marker must be consumed
   * before delegating to parseStatement(), or the depth value leaks
   * into the reconstructed RawStatement text as contamination).
   *
   * @param {number} headerDepth
   * @param {(firstTok:Object, secondTok:Object)=>boolean} isStopLine
   * @returns {Array} collected AST statement nodes
   */
  collectNestedBody(headerDepth, isStopLine) {
    const body = [];
    while (!this.isAtEnd()) {
      // Skip N\\ style depth markers (NUMBER + PUNCT "\") that appear
      // instead of proper DEPTH tokens when inside indented blocks
      if (this.current().type === TOKEN.NUMBER &&
          this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '\\') {
        this.advance(); this.advance(); // consume N and \
        if (this.isAtEnd()) break;
        // Check stop condition on the token after the depth marker
        const lft = this.current();
        const lst = this.peek(1);
        if (isStopLine(lft, lst)) break;
        const stmt = this.parseStatement();
        if (stmt) body.push(stmt);
        continue;
      }

      const aheadIsDepth = this.match(TOKEN.DEPTH);
      const afterDepthOffset = aheadIsDepth ? 1 : 0;
      const lineFirstTok = this.peek(afterDepthOffset);
      const lineSecondTok = this.peek(afterDepthOffset + 1);

      if (isStopLine(lineFirstTok, lineSecondTok)) {
        break;
      }

      if (aheadIsDepth && this.peek(0).value <= headerDepth) {
        const t = this.current();
        storm('SYNTAX_STORM',
          'Body statement must be nested deeper than its enclosing block header (missing block closer?)',
          t.line, t.column);
      }
      if (aheadIsDepth) this.advance();
      const stmt = this.parseStatement();
      if (stmt) body.push(stmt);
    }
    return body;
  }

  /**
   * WEATHER,
   *   ...bodyStatements (the protected "try" block)...
   * SHELTER STORM_TYPE [AS errVar],
   *   ...recovery body...
   * [SHELTER ANOTHER_TYPE, ...]*
   * CALM.
   *
   * Zero or more SHELTER clauses may appear; CALM is mandatory and seals
   * the block. Every connective is validated independently so a missing
   * or misplaced SHELTER/CALM raises SYNTAX_STORM with the caret aimed
   * at the exact offending token.
   */
  /**
   * WEATHER,                              — unconditional (try-block; default, unchanged)
   * WEATHER IF [conditionExpr],           — conditional: body only runs if conditionExpr is truthy
   *   ...bodyStatements (the protected "try" block)...
   * SHELTER STORM_TYPE [AS errVar],
   *   ...recovery body...
   * [SHELTER ANOTHER_TYPE, ...]*
   * CALM.
   *
   * Backward compatibility is the hard constraint here: every existing
   * WEATHER usage in the 285-test baseline is bare "WEATHER," with no
   * condition, and must keep working byte-for-byte identically. The
   * explicit "IF" marker right after WEATHER makes the conditional form
   * syntactically unambiguous — if "IF" isn't present, conditionExpr
   * stays null exactly as before and nothing about the existing grammar
   * changes. SYNTAX_STORM is raised if "IF" is present but no expression
   * follows it before the opening comma.
   */
  parseWeatherStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'WEATHER', '"WEATHER"');

    let conditionExpr = null;
    if (this.match(TOKEN.KEYWORD, 'IF')) {
      this.advance();
      const condTok = this.current();
      if (condTok.type === TOKEN.PUNCT && condTok.value === ',') {
        storm('SYNTAX_STORM',
          'Expected a condition expression after "WEATHER IF", found ","',
          condTok.line, condTok.column);
      }
      conditionExpr = this.parseHeaderExpressionUntilComma();
    }

    this.consume(TOKEN.PUNCT, ',', '"," to open the WEATHER body');

    const headerDepth = coords.depth;
    const isShelterOrCalmLine = (firstTok) =>
      firstTok.type === TOKEN.KEYWORD && (firstTok.value === 'SHELTER' || firstTok.value === 'CALM');

    const bodyStatements = this.collectNestedBody(headerDepth, isShelterOrCalmLine);

    // Consume the DEPTH marker preceding the first SHELTER/CALM line, if any,
    // mirroring collectNestedBody's contract (it leaves the stop line intact).
    if (this.match(TOKEN.DEPTH)) this.advance();

    const shelterClauses = [];
    while (this.match(TOKEN.KEYWORD, 'SHELTER')) {
      shelterClauses.push(this.parseShelterClause(headerDepth));
      if (this.match(TOKEN.DEPTH)) this.advance();
    }

    if (!this.match(TOKEN.KEYWORD, 'CALM')) {
      const t = this.current();
      storm('SYNTAX_STORM',
        `Expected "CALM" to close this WEATHER block, found "${t.value === null ? '(end of file)' : t.value}"`,
        t.line, t.column);
    }
    const calmClause = this.parseCalmClause();

    return new WeatherStatementNode(
      { conditionExpr, bodyStatements, shelterClauses, calmClause },
      coords
    );
  }

  /** Consume tokens up to (not including) the opening comma, joined as text. */
  parseHeaderExpressionUntilComma() {
    const span = [];
    while (!this.isAtEnd()) {
      if (this.match(TOKEN.PUNCT, ',') || this.match(TOKEN.PUNCT, '.')) break;
      span.push(this.advance());
    }
    if (span.length === 0) {
      const t = this.current();
      storm('SYNTAX_STORM', 'Expected a condition expression', t.line, t.column);
    }
    return joinTokens(span);
  }

  /**
   * SHELTER STORM_TYPE [AS errVar],
   *   ...bodyStatements...
   *
   * STORM_TYPE is an open-set identifier (ZERO_STORM, NETWORK_STORM,
   * ANY_STORM, ...) — not a fixed keyword — since user code and future
   * storm types must remain extensible without a tokenizer change.
   */
  parseShelterClause(weatherHeaderDepth) {
    const startTok = this.current();
    const coords = { line: startTok.line, column: startTok.column, depth: startTok.depth };
    this.consume(TOKEN.KEYWORD, 'SHELTER', '"SHELTER"');

    const stormTypeTok = this.current();
    if (stormTypeTok.type !== TOKEN.IDENT) {
      storm('SYNTAX_STORM',
        `Expected a storm type after SHELTER (e.g. "ZERO_STORM"), found "${stormTypeTok.value}"`,
        stormTypeTok.line, stormTypeTok.column);
    }
    const stormType = this.advance().value.toUpperCase();

    let errVar = null;
    if (this.match(TOKEN.KEYWORD, 'AS')) {
      this.advance();
      const identTok = this.current();
      if (identTok.type !== TOKEN.IDENT) {
        storm('SYNTAX_STORM',
          `Expected an identifier after "AS", found "${identTok.value}"`,
          identTok.line, identTok.column);
      }
      errVar = this.advance().value;
    }

    this.consume(TOKEN.PUNCT, ',', '"," to open the SHELTER body');

    const clauseDepth = coords.depth;
    const isNextShelterOrCalmLine = (firstTok) =>
      firstTok.type === TOKEN.KEYWORD && (firstTok.value === 'SHELTER' || firstTok.value === 'CALM');
    const bodyStatements = this.collectNestedBody(clauseDepth, isNextShelterOrCalmLine);

    return new ShelterStatementNode({ stormType, errVar, bodyStatements }, coords);
  }

  /**
   * CALM.
   *
   * Per the existing engine's proven grammar, CALM is a bare
   * block-closing terminator with no body of its own — it purely seals
   * the WEATHER/SHELTER chain (verified against the legacy interpreter's
   * `stmt.match(/^CALM$/i)` handling and every WEATHER/SHELTER/CALM use
   * in tests/all.plnt + tests/suite.plnt). CalmStatementNode still
   * exposes an empty `bodyStatements` array for forward compatibility
   * with a possible future "finally"-style CALM extension, without
   * introducing an untested, unused parsing path now.
   */
  parseCalmClause() {
    const startTok = this.current();
    const coords = { line: startTok.line, column: startTok.column, depth: startTok.depth };
    this.consume(TOKEN.KEYWORD, 'CALM', '"CALM"');
    this.consume(TOKEN.PUNCT, '.', 'a terminating period (.) after CALM');
    return new CalmStatementNode({ bodyStatements: [] }, coords);
  }

  // ── ACTION name(a(TYPE), b(TYPE)), ...body... /ACTION. ──────────
  // ── ACTION name(a(TYPE)) -> external. (FFI external declaration) ───
  parseActionDeclaration(coords) {
    this.consume(TOKEN.KEYWORD, 'ACTION', '"ACTION"');
    // Check for receiver syntax: ACTION (self(Type)) methodName(x(TYPE)), ...
    let receiver = null;
    if (this.match(TOKEN.PUNCT, '(') &&
        this.peek(1) && (this.peek(1).type === TOKEN.IDENT || this.peek(1).type === TOKEN.KEYWORD) &&
        this.peek(2) && this.peek(2).type === TOKEN.PUNCT && this.peek(2).value === '(' &&
        this.peek(4) && this.peek(4).type === TOKEN.PUNCT && this.peek(4).value === ')') {
      // This is ACTION (self(Type)) methodName(...)
      this.advance(); // consume (
      const recvNameTok = this.current();
      const recvName = this.advance().value.toLowerCase();
      this.consume(TOKEN.PUNCT, '(', '"(" after receiver name');
      const recvType = this.current().value;
      this.advance();
      this.consume(TOKEN.PUNCT, ')', '")" after receiver type');
      this.consume(TOKEN.PUNCT, ')', '")" to close receiver block');
      receiver = { name: recvName, type: recvType };
    }

    const nameTok = this.current();
    if (nameTok.type !== TOKEN.IDENT) {
      storm('SYNTAX_STORM', `Expected an action name after ACTION, found "${nameTok.value}"`,
        nameTok.line, nameTok.column);
    }
    const name = this.advance().value;
    this.consume(TOKEN.PUNCT, '(', '"(" after the action name');
    const params = this.parseParamList();
    this.consume(TOKEN.PUNCT, ')', '")" after the parameter list');

    // Check for { } body syntax (new style: ACTION name(params) { body })
    if (this.match(TOKEN.PUNCT, '{')) {
      this.advance(); // consume {
      this.currentDepth++;
      try {
        const bodyStatements = [];
        let braceDepth = 1;
        while (!this.isAtEnd() && braceDepth > 0) {
          if (this.match(TOKEN.PUNCT, '{')) { braceDepth++; this.advance(); continue; }
          if (this.match(TOKEN.PUNCT, '}')) {
            braceDepth--;
            if (braceDepth === 0) { this.advance(); break; }
            this.advance();
            continue;
          }
          // Collect statements up to '.' or '}' or '{'
          const stmt = this.parseStatement();
          if (stmt) bodyStatements.push(stmt);
          // Skip stray closers and depth markers
          if (this.match(TOKEN.DEPTH)) this.advance();
          while (!this.isAtEnd() && (this.match(TOKEN.PUNCT, '.') || this.match(TOKEN.KEYWORD, 'ORIF') || this.match(TOKEN.KEYWORD, 'ELSE') || this.match(TOKEN.KEYWORD, 'SHELTER') || this.match(TOKEN.KEYWORD, 'CALM'))) {
            if (this.match(TOKEN.PUNCT, '.')) { this.advance(); break; }
            this.advance();
          }
        }
        if (this.match(TOKEN.PUNCT, '.')) this.advance();
        return new ActionDeclarationNode({ name, params, bodyStatements, isExternal: false, receiver }, coords);
      } finally {
        this.currentDepth--;
      }
    }

    // FFI external declaration: -> external.
    const arrow = this.peek(0);
    const extKw = this.peek(1);
    if (arrow.type === TOKEN.PUNCT && arrow.value === '->' &&
        extKw.type === TOKEN.KEYWORD && extKw.value.toUpperCase() === 'EXTERNAL') {
      this.advance(); // ->
      this.advance(); // external
      if (this.match(TOKEN.PUNCT, '.')) this.advance();
      return new ActionDeclarationNode({ name, params, isExternal: true, receiver }, coords);
    }

    this.consume(TOKEN.PUNCT, ',', '"," to open the ACTION body');

    const headerDepth = coords.depth;
    // Only stop at /ACTION (not /SEASON, /SPECIES, etc.)
    const isActionCloser = (ft, st) =>
      ft.type === TOKEN.PUNCT && ft.value === '/' &&
      st && st.type === TOKEN.KEYWORD && st.value === 'ACTION';
    let bodyStatements;
    this.currentDepth++;
    try {
      bodyStatements = this.collectNestedBody(headerDepth, isActionCloser);
    } finally {
      this.currentDepth--;
    }

    if (this.match(TOKEN.DEPTH)) this.advance();
    this.consume(TOKEN.PUNCT, '/', '"/" in the "/ACTION." closer');
    this.consume(TOKEN.KEYWORD, 'ACTION', '"ACTION" in the "/ACTION." closer');
    if (this.match(TOKEN.PUNCT, '.')) this.advance();

    return new ActionDeclarationNode({ name, params, bodyStatements, isExternal: false, receiver }, coords);
  }

  // ── SPECIES name [FROM|PARENT base] { ... } or , ... /SPECIES. ──
  parseSpeciesDeclaration(coords) {
    this.consume(TOKEN.KEYWORD, 'SPECIES', '"SPECIES"');
    const nameTok = this.current();
    if (nameTok.type !== TOKEN.IDENT) {
      storm('SYNTAX_STORM', `Expected a species name after SPECIES, found "${nameTok.value}"`,
        nameTok.line, nameTok.column);
    }
    const name = this.advance().value;

    let parentName = null;
    if (this.match(TOKEN.KEYWORD, 'FROM') || this.match(TOKEN.KEYWORD, 'PARENT')) {
      this.advance();
      const pTok = this.current();
      if (pTok.type !== TOKEN.IDENT) {
        storm('SYNTAX_STORM', `Expected a parent species name, found "${pTok.value}"`,
          pTok.line, pTok.column);
      }
      parentName = this.advance().value;
    }

    // New syntax: SPECIES Name { field: TYPE ... }
    if (this.match(TOKEN.PUNCT, '{')) {
      this.advance();
      const fields = [], actions = [];
      while (!this.isAtEnd() && !this.match(TOKEN.PUNCT, '}')) {
        if (this.match(TOKEN.DEPTH)) { this.advance(); continue; }
        // Skip bare numbers (legacy depth prefix remnants inside blocks)
        if (this.current().type === TOKEN.NUMBER && this.peek(1) && this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '\\') {
          this.advance(); this.advance(); continue;
        }

        if (this.match(TOKEN.KEYWORD, 'ACTION')) {
          const action = this.parseActionDeclaration(coords);
          actions.push(action);
          continue;
        }
        // Field: fieldName: Type
        const tok = this.current();
        if (tok.type === TOKEN.IDENT || tok.type === TOKEN.KEYWORD) {
          if (this.peek(1) && this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === ':') {
            const fName = this.advance().value;
            this.advance(); // consume ':'
            const fTypeTok = this.current();
            const fType = (fTypeTok.type === TOKEN.IDENT || fTypeTok.type === TOKEN.KEYWORD) ? fTypeTok.value : 'TX';
            this.advance();
            if (this.match(TOKEN.PUNCT, '.')) this.advance();
            fields.push({ name: fName, varType: fType.toUpperCase(), defaultExpr: null });
            continue;
          }
        }
        // Skip bare tokens (newlines, etc.)
        if (this.match(TOKEN.PUNCT, '.')) { this.advance(); continue; }
        if (this.isAtEnd() || this.match(TOKEN.PUNCT, '}')) break;
        this.advance();
      }
      if (this.match(TOKEN.PUNCT, '}')) this.advance();
      if (this.match(TOKEN.PUNCT, '.')) this.advance();
      return new SpeciesDeclarationNode({ name, parentName, fields, actions }, coords);
    }

    // Old syntax: SPECIES name, ... /SPECIES.
    this.consume(TOKEN.PUNCT, ',', '"," to open the SPECIES body');

    const headerDepth = coords.depth;
    const fields = [], actions = [];
    while (!this.isAtEnd()) {
      const aheadIsDepth = this.match(TOKEN.DEPTH);
      const off = aheadIsDepth ? 1 : 0;
      const lineFirstTok = this.peek(off);
      if (lineFirstTok.type === TOKEN.PUNCT && lineFirstTok.value === '/') {
        if (aheadIsDepth) this.advance();
        break;
      }
      if (aheadIsDepth) this.advance();
      const memberCoords = { line: this.current().line, column: this.current().column, depth: this.current().depth };
      if (this.match(TOKEN.KEYWORD, 'VAR')) {
        this.advance();
        const fNameTok = this.current();
        if (fNameTok.type !== TOKEN.IDENT && fNameTok.type !== TOKEN.KEYWORD) {
          storm('SYNTAX_STORM', `Expected a field name after VAR, found "${fNameTok.value}"`,
            fNameTok.line, fNameTok.column);
        }
        const fName = this.advance().value;
        this.consume(TOKEN.PUNCT, '(', '"(" after field name');
        const fType = this.current().value || 'TX';
        this.advance();
        this.consume(TOKEN.PUNCT, ')', '")" after field type');
        let defaultExpr = null;
        if (this.match(TOKEN.KEYWORD, 'TO')) {
          this.advance();
          if (!this.match(TOKEN.PUNCT, '.')) {
            defaultExpr = this.parseExpressionSpan();
          }
        }
        if (this.match(TOKEN.PUNCT, '.')) this.advance();
        fields.push({ name: fName, varType: fType.toUpperCase(), defaultExpr });
        continue;
      }
      if (this.match(TOKEN.KEYWORD, 'ACTION')) {
        const action = this.parseActionDeclaration(memberCoords);
        actions.push(action);
        continue;
      }
      while (!this.isAtEnd() && !(this.match(TOKEN.PUNCT, '.') || this.match(TOKEN.DEPTH))) {
        this.advance();
      }
      if (this.match(TOKEN.PUNCT, '.')) this.advance();
    }

    this.consume(TOKEN.PUNCT, '/', '"/" in the "/SPECIES." closer');
    this.consume(TOKEN.KEYWORD, 'SPECIES', '"SPECIES" in the "/SPECIES." closer');
    if (this.match(TOKEN.PUNCT, '.')) this.advance();

    return new SpeciesDeclarationNode({ name, parentName, fields, actions }, coords);
  }

  // ── obj:method(args). ────────────────────────────────────────────
  parseMethodCallStatement(coords) {
    const targetTok = this.current();
    const target = new IdentifierNode(targetTok.value, { line: targetTok.line, column: targetTok.column, depth: targetTok.depth });
    this.advance(); // consume obj
    this.consume(TOKEN.PUNCT, ':', '":" after the target in method call');
    const methodTok = this.current();
    const methodName = methodTok.value;
    this.advance(); // consume method name
    this.consume(TOKEN.PUNCT, '(', '"(" after method name');
    const args = [];
    while (!this.isAtEnd() && !this.match(TOKEN.PUNCT, ')')) {
      if (this.match(TOKEN.PUNCT, ',')) { this.advance(); continue; }
      const expr = this.parseExpressionSpan();
      if (expr) args.push(expr);
    }
    if (this.match(TOKEN.PUNCT, ')')) this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    const { MethodCallStatementNode } = require('./ast');
    return new MethodCallStatementNode({ target, methodName, args }, coords);
  }

  // ── BLOOM SpeciesName AS instanceIdent. ─────────────────────────
  parseBloomStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'BLOOM', '"BLOOM"');
    const speciesTok = this.current();
    if (speciesTok.type !== TOKEN.IDENT) {
      storm('SYNTAX_STORM', `Expected a species name after BLOOM, found "${speciesTok.value}"`,
        speciesTok.line, speciesTok.column);
    }
    const speciesName = this.advance().value;
    this.consume(TOKEN.KEYWORD, 'AS', '"AS" after species name in BLOOM');
    const identTok = this.current();
    if (identTok.type !== TOKEN.IDENT) {
      storm('SYNTAX_STORM', `Expected an instance identifier after AS, found "${identTok.value}"`,
        identTok.line, identTok.column);
    }
    const instanceIdent = this.advance().value;
    this.consume(TOKEN.PUNCT, '.', 'a terminating period (.) after BLOOM');
    return new BloomStatementNode({ speciesName, instanceIdent }, coords);
  }

  // ── TAP "filename" MODE:word AS handle. ─────────────────────────
  parseTapStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'TAP', '"TAP"');
    const filenameTok = this.current();
    if (filenameTok.type !== TOKEN.STRING) {
      storm('SYNTAX_STORM', `Expected a quoted filename after TAP, found "${filenameTok.value}"`,
        filenameTok.line, filenameTok.column);
    }
    const filename = this.advance().value;

    // Consume "MODE" : "word"
    const modeName = this.current().value;
    if ((this.current().type !== TOKEN.IDENT && this.current().type !== TOKEN.KEYWORD) || modeName.toUpperCase() !== 'MODE') {
      const t = this.current();
      storm('SYNTAX_STORM', `Expected "MODE" after filename in TAP, found "${t.value}"`,
        t.line, t.column);
    }
    this.advance(); // MODE
    this.consume(TOKEN.PUNCT, ':', '":" after MODE');
    const modeTok = this.current();
    if (modeTok.type !== TOKEN.KEYWORD && modeTok.type !== TOKEN.IDENT) {
      storm('SYNTAX_STORM', `Expected a mode keyword (e.g. MARK, READ) after MODE:, found "${modeTok.value}"`,
        modeTok.line, modeTok.column);
    }
    const mode = this.advance().value.toUpperCase();

    this.consume(TOKEN.KEYWORD, 'AS', '"AS" after the mode in TAP');
    const handleTok = this.current();
    if (handleTok.type !== TOKEN.IDENT) {
      storm('SYNTAX_STORM', `Expected a handle identifier after AS in TAP, found "${handleTok.value}"`,
        handleTok.line, handleTok.column);
    }
    const handleIdent = this.advance().value;
    this.consume(TOKEN.PUNCT, '.', 'a terminating period (.) after TAP');
    return new TapStatementNode({ filename, mode, handleIdent }, coords);
  }

  // ── WHENEVER varName CHANGES, ...body... N\. ────────────────────
  parseWheneverStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'WHENEVER', '"WHENEVER"');
    const watchTok = this.current();
    if (watchTok.type !== TOKEN.IDENT) {
      storm('SYNTAX_STORM', `Expected a variable name after WHENEVER, found "${watchTok.value}"`,
        watchTok.line, watchTok.column);
    }
    const watchIdent = this.advance().value;
    // "CHANGES" may tokenize as IDENT (not in KEYWORDS set)
    const changesTok = this.current();
    if (changesTok.value.toUpperCase() !== 'CHANGES') {
      storm('SYNTAX_STORM', `Expected "CHANGES" after variable name, found "${changesTok.value}"`,
        changesTok.line, changesTok.column);
    }
    this.advance(); // CHANGES
    this.consume(TOKEN.PUNCT, ',', '"," to open WHENEVER body');

    const headerDepth = coords.depth;
    // Closer: N\. (DEPTH token + PUNCT "\" + PUNCT ".")
    const isWheneverCloser = (ft, st) =>
      ft.type === TOKEN.PUNCT && ft.value === '\\';

    const bodyStatements = this.collectNestedBody(headerDepth, isWheneverCloser);
    if (this.match(TOKEN.DEPTH)) this.advance();
    if (this.match(TOKEN.PUNCT, '\\')) this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance();

    return new WheneverStatementNode({ watchIdent, bodyStatements }, coords);
  }

  // ── REAP variable FROM source [, args...]. ─────────────────────
  // Handles all REAP variants by inspecting the first token(s) after FROM:
  //   FROM NOW [FORMAT:X]         → source.kind = 'NOW'
  //   FROM TYPEOF target          → source.kind = 'TYPEOF'
  //   FROM SELF:method [, args]   → source.kind = 'SELF'
  //   FROM lib:FUNC [, args]      → source.kind = 'LIBRARY' or 'INSTANCE'
  //   FROM action [, args]        → source.kind = 'ACTION'
  //
  // Args are collected as raw expression strings (NOT sub-parsed) so they
  // are evaluated at runtime by evalExpr — identical to the legacy
  // _splitArgs(rawArgs).map(a => E(a.trim())) pipeline, preserving exact
  // semantic parity with the proven regex handlers.
  parseReapStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'REAP', '"REAP"');

    const varTok = this.current();
    const variable = (varTok.type === TOKEN.IDENT || varTok.type === TOKEN.KEYWORD || varTok.value === '_')
      ? this.advance().value
      : (() => { storm('SYNTAX_STORM', `Expected a variable name after REAP, found "${varTok.value}"`, varTok.line, varTok.column); })();

    this.consume(TOKEN.KEYWORD, 'FROM', '"FROM" after variable name in REAP');

    const firstTok = this.current();
    let source, args = [];

    // ── FROM NOW [FORMAT:word] ────────────────────────────────
    if (firstTok.type === TOKEN.KEYWORD && firstTok.value === 'NOW') {
      this.advance();
      let format = 'FULL';
      // FORMAT is an identifier (not keyword) but may appear as either
      const fmtTok = this.current();
      if (fmtTok.type !== TOKEN.PUNCT || fmtTok.value !== '.') {
        const fmtName = fmtTok.value;
        if (fmtName && fmtName.toUpperCase() === 'FORMAT') {
          this.advance();
          this.consume(TOKEN.PUNCT, ':', '":" after FORMAT');
          format = this.advance().value.toUpperCase();
        }
      }
      source = { kind: 'NOW', format };

    // ── FROM TYPEOF target ────────────────────────────────────
    } else if (firstTok.type === TOKEN.KEYWORD && firstTok.value === 'TYPEOF') {
      this.advance();
      const targetTok = this.current();
      const target = this.advance().value;
      source = { kind: 'TYPEOF', target };

    // ── FROM SELF:method [, args] ─────────────────────────────
    } else if (firstTok.type === TOKEN.KEYWORD && firstTok.value === 'SELF') {
      this.advance();
      this.consume(TOKEN.PUNCT, ':', '":" after SELF');
      const method = this.advance().value;
      source = { kind: 'SELF', method };
      if (this.match(TOKEN.PUNCT, ',')) { this.advance(); args = this.parseReapArgs(); }

    // ── FROM lib:FUNC or obj:method [, args] ─────────────────
    } else if ((firstTok.type === TOKEN.IDENT || firstTok.type === TOKEN.KEYWORD) &&
               this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === ':') {
      const name = this.advance().value;
      this.advance(); // consume ':'
      const fn = this.advance().value;
      source = { kind: 'INSTANCE_OR_LIBRARY', name, fn };
      if (this.match(TOKEN.PUNCT, ',')) { this.advance(); args = this.parseReapArgs(); }

    // ── FROM "string literal" FLOW ... (pipeline on literal value) ─
    } else if (firstTok.type === TOKEN.STRING) {
      const strVal = this.advance().value;
      source = { kind: 'LITERAL', value: strVal };
      if (this.match(TOKEN.PUNCT, ',')) { this.advance(); args = this.parseReapArgs(); }

    // ── FROM action [, args] ──────────────────────────────────
    } else if (firstTok.type === TOKEN.IDENT || firstTok.type === TOKEN.KEYWORD) {
      // If followed by '(' or '[', it's an expression (SPLIT, JOIN, COUNT, parts[0])
      const nextIsPunct = this.peek(1) && this.peek(1).type === TOKEN.PUNCT;
      if (nextIsPunct && (this.peek(1).value === '(' || this.peek(1).value === '[')) {
        const expr = this.parseExpressionSpan();
        source = { kind: 'EXPR', expr };
      } else {
        const name = this.advance().value;
        source = { kind: 'ACTION', name };
        if (this.match(TOKEN.PUNCT, ',')) { this.advance(); args = this.parseReapArgs(); }
      }

    } else {
      storm('SYNTAX_STORM', `Expected a source after FROM in REAP, found "${firstTok.value}"`,
        firstTok.line, firstTok.column);
    }

    this.consume(TOKEN.PUNCT, '.', 'a terminating period (.) after REAP');
    return new ReapStatementNode({ variable, source, args }, coords);
  }

  // Collect comma-separated raw arg strings up to the terminating period.
  // Parses using the same depth-tracking logic as _splitArgs so nested
  // parens and quoted strings are handled correctly.
  parseReapArgs() {
    const args = [];
    let spanTokens = [];
    let depth = 0;

    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.PUNCT && t.value === '.' && depth === 0) break;
      if (t.type === TOKEN.PUNCT && t.value === ',' && depth === 0) {
        if (spanTokens.length > 0) args.push(joinTokens(spanTokens).trim());
        spanTokens = [];
        this.advance();
        continue;
      }
      if (t.type === TOKEN.PUNCT && (t.value === '(' || t.value === '[')) depth++;
      if (t.type === TOKEN.PUNCT && (t.value === ')' || t.value === ']')) depth--;
      spanTokens.push(this.advance());
    }
    if (spanTokens.length > 0) args.push(joinTokens(spanTokens).trim());
    return args;
  }

  /** Parse a comma-separated parameter list: a(NUM), b(TX), ... */
  parseSetStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'SET', '"SET"');
    // Collect the full identifier token span up to TO
    // Supports: SET x TO val, SET SELF:x TO val, SET obj:prop TO val, SET obj:"key" TO val
    // SET p.x TO val (member access)
    const identSpan = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.KEYWORD && t.value === 'TO') break;
      if (t.type === TOKEN.PUNCT && t.value === '.' &&
          this.peek(1) && this.peek(1).type === TOKEN.IDENT) {
        // Part of member access like p.x — include the dot and continue
        identSpan.push(this.advance()); // push '.'
        identSpan.push(this.advance()); // push field name
        continue;
      }
      if (t.type === TOKEN.PUNCT && t.value === '.') break; // statement terminator
      identSpan.push(this.advance());
    }
    const identifier = joinTokens(identSpan).trim();

    this.consume(TOKEN.KEYWORD, 'TO', '"TO" in SET statement');
    const valSpan = [];
    while (!this.isAtEnd() && !(this.match(TOKEN.PUNCT, '.'))) valSpan.push(this.advance());
    this.consume(TOKEN.PUNCT, '.', 'a terminating period (.) after SET');

    // If the identifier contains a dot but not a colon, it's member access
    const node = new SetStatementNode({ identifier, valueExpr: joinTokens(valSpan).trim() }, coords);
    if (identifier.includes('.') && !identifier.includes(':')) {
      const parts = identifier.split('.').map(s => s.trim());
      if (parts.length === 2 && parts[0].length > 0 && parts[1].length > 0) {
        node.isMemberAccess = true;
        // Normalize SELF/self → __self for consistent receiver access
        const objName = parts[0].toLowerCase();
        node.memberObject = objName === 'self' ? '__self' : objName;
        node.memberField = parts[1];
      }
    }
    return node;
  }

  parseIncreaseStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'INCREASE', '"INCREASE"');
    const identSpan = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.KEYWORD && t.value === 'BY') break;
      if (t.type === TOKEN.PUNCT && t.value === '.') break;
      identSpan.push(this.advance());
    }
    const identifier = joinTokens(identSpan).trim();
    this.consume(TOKEN.KEYWORD, 'BY', '"BY" in INCREASE statement');
    const amtSpan = [];
    while (!this.isAtEnd() && !(this.match(TOKEN.PUNCT, '.'))) amtSpan.push(this.advance());
    this.consume(TOKEN.PUNCT, '.', 'a terminating period (.) after INCREASE');
    return new IncreaseStatementNode({ identifier, amountExpr: joinTokens(amtSpan).trim() }, coords);
  }

  parseIncreaseStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'INCREASE', '"INCREASE"');
    const identSpan = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if ((t.type === TOKEN.KEYWORD || t.type === TOKEN.IDENT) && t.value.toUpperCase() === 'BY') break;
      if (t.type === TOKEN.PUNCT && t.value === '.' &&
          this.peek(1) && this.peek(1).type === TOKEN.IDENT) {
        identSpan.push(this.advance());
        identSpan.push(this.advance());
        continue;
      }
      if (t.type === TOKEN.PUNCT && t.value === '.') break;
      identSpan.push(this.advance());
    }
    const identifier = joinTokens(identSpan).trim();
    if ((this.current().type === TOKEN.KEYWORD || this.current().type === TOKEN.IDENT) && this.current().value.toUpperCase() === 'BY') this.advance();
    else { const t=this.current(); storm('SYNTAX_STORM',`Expected "BY" in INCREASE, found "${t.value}"`,t.line,t.column); }
    const amtSpan = [];
    while (!this.isAtEnd() && !(this.match(TOKEN.PUNCT, '.'))) amtSpan.push(this.advance());
    this.consume(TOKEN.PUNCT, '.', 'a terminating period (.) after INCREASE');
    return new IncreaseStatementNode({ identifier, amountExpr: joinTokens(amtSpan).trim() }, coords);
  }

  parseDecreaseStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'DECREASE', '"DECREASE"');
    const identSpan = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if ((t.type === TOKEN.KEYWORD || t.type === TOKEN.IDENT) && t.value.toUpperCase() === 'BY') break;
      if (t.type === TOKEN.PUNCT && t.value === '.' &&
          this.peek(1) && this.peek(1).type === TOKEN.IDENT) {
        identSpan.push(this.advance());
        identSpan.push(this.advance());
        continue;
      }
      if (t.type === TOKEN.PUNCT && t.value === '.') break;
      identSpan.push(this.advance());
    }
    const identifier = joinTokens(identSpan).trim();
    if ((this.current().type === TOKEN.KEYWORD || this.current().type === TOKEN.IDENT) && this.current().value.toUpperCase() === 'BY') this.advance();
    else { const t=this.current(); storm('SYNTAX_STORM',`Expected "BY" in DECREASE, found "${t.value}"`,t.line,t.column); }
    const amtSpan = [];
    while (!this.isAtEnd() && !(this.match(TOKEN.PUNCT, '.'))) amtSpan.push(this.advance());
    this.consume(TOKEN.PUNCT, '.', 'a terminating period (.) after DECREASE');
    return new DecreaseStatementNode({ identifier, amountExpr: joinTokens(amtSpan).trim() }, coords);
  }

  parseParamList() {
    const params = [];
    while (!this.isAtEnd() && !this.match(TOKEN.PUNCT, ')')) {
      const pNameTok = this.current();
      if (pNameTok.type !== TOKEN.IDENT) break;
      const pName = this.advance().value;
      let pType = 'ANY';
      if (this.match(TOKEN.PUNCT, '(')) {
        this.advance();
        pType = this.current().value || 'ANY';
        this.advance();
        if (this.match(TOKEN.PUNCT, ')')) this.advance();
      }
      params.push({ name: pName, type: pType.toUpperCase() });
      if (this.match(TOKEN.PUNCT, ',')) this.advance();
    }
    return params;
  }

  /** Consume tokens up to (not including) the given keyword, joined as text. */
  parseHeaderExpressionUntilKeyword(stopKeyword) {
    const span = [];
    while (!this.isAtEnd()) {
      if (this.match(TOKEN.KEYWORD, stopKeyword)) break;
      if (this.match(TOKEN.PUNCT, ',') || this.match(TOKEN.PUNCT, '.')) break;
      span.push(this.advance());
    }
    if (span.length === 0) {
      const t = this.current();
      storm('SYNTAX_STORM', `Expected an expression before "${stopKeyword}"`, t.line, t.column);
    }
    return joinTokens(span);
  }

  /** GIVE [expr] AS RESPONSE.  →  ResponseStatementNode */
  parseResponseStatementAst(coords) {
    this.consume(TOKEN.KEYWORD, 'GIVE', '"GIVE"');
    const span = [];
    while (!this.isAtEnd()) {
      if (this.match(TOKEN.KEYWORD, 'AS')) break;
      if (this.match(TOKEN.PUNCT, '.')) break;
      span.push(this.advance());
    }
    const responseExpr = joinTokens(span);
    this.consume(TOKEN.KEYWORD, 'AS', '"AS" after the response expression');
    this.consume(TOKEN.KEYWORD, 'RESPONSE', '"RESPONSE" after "AS"');
    this.consume(TOKEN.PUNCT, '.', 'a terminating period (.)');
    return new ResponseStatementNode({ responseExpr }, coords);
  }

  /** Lookahead: does this statement (up to the next period) contain "AS RESPONSE"? */
  containsResponseAheadOnLine() {
    let off = 0;
    while (true) {
      const t = this.peek(off);
      if (t.type === TOKEN.EOF) return false;
      if (t.type === TOKEN.PUNCT && t.value === '.') return false;
      if (t.type === TOKEN.KEYWORD && t.value === 'AS' &&
          this.peek(off + 1).type === TOKEN.KEYWORD && this.peek(off + 1).value === 'RESPONSE') {
        return true;
      }
      off++;
    }
  }

  parseShowStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'SHOW', '"SHOW"');
    const expr = this.parseExpressionSpan();
    this.consume(TOKEN.PUNCT, '.', 'a terminating period (.)');
    return new ShowStatementNode({ expr }, coords);
  }

  // ── CREATE ident(TYPE) TO expr. ──────────────────────────────
  parseCreateStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'CREATE', '"CREATE"');

    const identTok = this.current();
    // Keywords (body, method, path, result, etc.) are valid variable names in PlantLang
    if (identTok.type !== TOKEN.IDENT && identTok.type !== TOKEN.KEYWORD) {
      storm('SYNTAX_STORM',
        `Expected a variable identifier after CREATE, found "${identTok.value}"`,
        identTok.line, identTok.column);
    }
    const identifier = this.advance().value;

    // Optional type annotation: CREATE ident(TYPE) TO expr.
    // If the next token is not '(', infer type from value expression
    let varType = null;
    if (this.match(TOKEN.PUNCT, '(')) {
      this.advance(); // consume (
      // Handle MAP[K,V] syntax: MAP [ K , V ]
      if (this.match(TOKEN.KEYWORD, 'MAP') && this.peek(1) && this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '[') {
        this.advance(); // consume MAP
        this.advance(); // consume [
        const keyTok = this.current();
        if (keyTok.type !== TOKEN.KEYWORD && keyTok.type !== TOKEN.IDENT) {
          storm('SYNTAX_STORM', `Expected a key type inside MAP[ ], found "${keyTok.value}"`, keyTok.line, keyTok.column);
        }
        const keyType = this.advance().value;
        this.consume(TOKEN.PUNCT, ',', '"," between key and value types in MAP[K,V]');
        const valTok = this.current();
        if (valTok.type !== TOKEN.KEYWORD && valTok.type !== TOKEN.IDENT) {
          storm('SYNTAX_STORM', `Expected a value type inside MAP[ ], found "${valTok.value}"`, valTok.line, valTok.column);
        }
        const valType = this.advance().value;
        this.consume(TOKEN.PUNCT, ']', '"]" to close MAP type');
        varType = `MAP[${keyType},${valType}]`;
      } else if (this.match(TOKEN.PUNCT, '[')) {
        this.advance(); // consume [
        const innerTok = this.current();
        if (innerTok.type !== TOKEN.KEYWORD && innerTok.type !== TOKEN.IDENT) {
          storm('SYNTAX_STORM',
            `Expected a type inside [ ], found "${innerTok.value}"`,
            innerTok.line, innerTok.column);
        }
        const innerType = this.advance().value;
        this.consume(TOKEN.PUNCT, ']', '"]" to close array type');
        varType = `[${innerType}]`;
      } else {
        const typeTok = this.current();
        const VALID_TYPES = new Set(['NUM', 'SCL', 'TX', 'FACT', 'LIST', 'MAP', 'INSTANCE', 'VEIN']);
        const isBuiltin = typeTok.type === TOKEN.KEYWORD && VALID_TYPES.has(typeTok.value);
        const isStruct = typeTok.type === TOKEN.IDENT || typeTok.type === TOKEN.KEYWORD;
        if (!isBuiltin && !isStruct) {
          storm('SYNTAX_STORM',
            `Expected a type (NUM, SCL, TX, FACT, LIST, MAP, or struct name), found "${typeTok.value}"`,
            typeTok.line, typeTok.column);
        }
        varType = this.advance().value;
      }
      this.consume(TOKEN.PUNCT, ')', '")" after the type');
    }

    // Optional PULSE modifier: CREATE x(NUM) PULSE TO 20.
    let isPulse = false;
    if (this.match(TOKEN.KEYWORD, 'PULSE')) { this.advance(); isPulse = true; }

    // TO is optional entirely (e.g. some grammars allow a bare default),
    // and even when present may have no following expression — used for
    // "CREATE x(LIST) TO." to declare an empty list relying on the type's
    // default value, an existing idiom in the language.
    let valueExpr = null;
    if (this.match(TOKEN.KEYWORD, 'TO')) {
      this.advance();
      if (!(this.match(TOKEN.PUNCT, '.'))) {
        // Check for struct instantiation syntax: StructName{ args }
        if (this.peek(0).type === TOKEN.IDENT && this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '{') {
          valueExpr = this.parseStructInstantiation();
        } else if (this.match(TOKEN.KEYWORD, 'BLOOM')) {
          this.advance(); // consume BLOOM keyword
          const speciesTok = this.current();
          if (speciesTok.type !== TOKEN.IDENT) {
            storm('SYNTAX_STORM', `Expected a species name after BLOOM, found "${speciesTok.value}"`,
              speciesTok.line, speciesTok.column);
          }
          const speciesName = this.advance().value;
          const { BloomExpressionNode } = require('./ast');
          valueExpr = new BloomExpressionNode({ speciesName }, { line: speciesTok.line, column: speciesTok.column, depth: speciesTok.depth });
        } else if (this.match(TOKEN.PUNCT, '[')) {
          valueExpr = this.parseArrayLiteral();
        } else if (this.match(TOKEN.PUNCT, '{')) {
          // If the declared type is a known struct, parse as struct literal
          if (varType && this.structNames.has(varType)) {
            valueExpr = this.parseStructLiteral();
            valueExpr.structName = varType;
          } else {
            valueExpr = this.parseMapLiteral();
          }
        } else {
          valueExpr = this.parseExpressionSpan();
        }
      }
    }

    this.consume(TOKEN.PUNCT, '.', 'a terminating period (.)');
    const node = new CreateStatementNode({ identifier, varType, valueExpr }, coords);
    node.isPulse = isPulse;
    return node;
  }

  /**
   * Parse a single expression token span up to (but not including) the
   * terminating period, returning a Literal/Identifier node for the
   * simple single-token case, or a RawStatementNode-style string for
   * compound expressions — full expression-grammar (operator
   * precedence, function calls, etc.) is delegated to the existing
   * evaluator during this migration phase per the task's incremental
   * constraint; this parser's job is statement-level grammar, not a
   * full expression parser rewrite.
   */
  parseExpressionSpan() {
    const tokensInSpan = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.PUNCT && t.value === '.') {
        // Check for member access / method call pattern: IDENT/KW . IDENT/KW
        // Target variable may be a keyword (e.g. EMPTY); field/method name
        // may also be a keyword (CHOICE variant like Num, Empty).
        // To avoid false matches like "b . GIVE" inside "a + b. GIVE result",
        // KEYWORD field names are accepted only when:
        //   1. The field is on the same line as '.'
        //   2. The token immediately after the field is NOT an IDENT
        //      (which would indicate a statement keyword like SET followed by
        //       its subject).
        const isTarget = (tok) => tok && (tok.type === TOKEN.IDENT || tok.type === TOKEN.KEYWORD);
        const isFieldName = (dotTok, fieldTok) => {
          if (!fieldTok) return false;
          if (fieldTok.type === TOKEN.IDENT) return true;
          if (fieldTok.type === TOKEN.KEYWORD) {
            if (dotTok.line !== fieldTok.line) return false;
            const afterField = this.peek(2);
            return !(afterField && afterField.type === TOKEN.IDENT);
          }
          return false;
        };
        if (tokensInSpan.length > 0 &&
            isTarget(tokensInSpan[tokensInSpan.length - 1]) &&
            this.peek(1) && isFieldName(t, this.peek(1))) {
          tokensInSpan.push(this.advance()); // push '.'
          const fieldTok = this.current();
          tokensInSpan.push(this.advance()); // push field name
          // Check for method call: IDENT . IDENT/KW ( args )
          if (this.match(TOKEN.PUNCT, '(')) {
            this.advance(); // consume (
            const objCoords = { line: tokensInSpan[0].line, column: tokensInSpan[0].column, depth: tokensInSpan[0].depth };
            const obj = new IdentifierNode(tokensInSpan[0].value, objCoords);
            const methodName = fieldTok.value;
            const args = [];
            let depth = 1;
            const argSpan = [];
            while (!this.isAtEnd() && depth > 0) {
              const at = this.current();
              if (at.type === TOKEN.PUNCT && (at.value === '(' || at.value === '[')) { depth++; argSpan.push(this.advance()); }
              else if (at.type === TOKEN.PUNCT && (at.value === ')' || at.value === ']')) {
                depth--;
                if (depth === 0) break;
                argSpan.push(this.advance());
              } else if (at.type === TOKEN.PUNCT && at.value === ',' && depth === 1) {
                this.advance();
                if (argSpan.length > 0) {
                  args.push(new LiteralNode(joinTokens(argSpan), 'RAW_EXPR', objCoords));
                  argSpan.length = 0;
                }
              } else {
                argSpan.push(this.advance());
              }
            }
            if (argSpan.length > 0) {
              args.push(new LiteralNode(joinTokens(argSpan), 'RAW_EXPR', objCoords));
            }
            if (!this.isAtEnd()) this.advance(); // consume )
            const coords = { line: tokensInSpan[0].line, column: tokensInSpan[0].column, depth: tokensInSpan[0].depth };
            return new MethodCallNode({ target: obj, methodName, args }, coords);
          }
          continue;
        }
        break; // statement terminator
      }
      tokensInSpan.push(this.advance());
    }
    if (tokensInSpan.length === 0) {
      const t = this.current();
      storm('SYNTAX_STORM', 'Expected an expression', t.line, t.column);
    }
    // Check for member access pattern: IDENT/KW . IDENT/KW (span has 3 tokens)
    // Target may be a keyword variable name; field may also be a keyword (CHOICE variant).
    const isTarget = (tok) => tok && (tok.type === TOKEN.IDENT || tok.type === TOKEN.KEYWORD);
    if (tokensInSpan.length === 3 &&
        isTarget(tokensInSpan[0]) &&
        tokensInSpan[1].type === TOKEN.PUNCT && tokensInSpan[1].value === '.' &&
        (tokensInSpan[2].type === TOKEN.IDENT ||
         tokensInSpan[2].type === TOKEN.KEYWORD)) {
      const objCoords = { line: tokensInSpan[0].line, column: tokensInSpan[0].column, depth: tokensInSpan[0].depth };
      const obj = new IdentifierNode(tokensInSpan[0].value, objCoords);
      const member = tokensInSpan[2].value;
      const coords = { line: tokensInSpan[0].line, column: tokensInSpan[0].column, depth: tokensInSpan[0].depth };
      return new MemberAccessNode({ object: obj, member }, coords);
    }

    if (tokensInSpan.length === 1) {
      const t = tokensInSpan[0];
      const coords = { line: t.line, column: t.column, depth: t.depth };
      if (t.type === TOKEN.NUMBER) return new LiteralNode(t.value, 'NUMBER', coords);
      if (t.type === TOKEN.STRING) return new LiteralNode(t.value, 'STRING', coords);
      if (t.type === TOKEN.FACT) return new LiteralNode(t.value, 'FACT', coords);
      if (t.type === TOKEN.IDENT) return new IdentifierNode(t.value, coords);
    }
    // Check for len(x) / cap(x) / COUNT(x) / SPLIT(x, delim) pattern:
    // IDENT|KEYWORD LPAREN ... RPAREN
    const first = tokensInSpan[0];
    if (tokensInSpan.length >= 4 &&
        (first.type === TOKEN.IDENT || first.type === TOKEN.KEYWORD) &&
        tokensInSpan[1].type === TOKEN.PUNCT && tokensInSpan[1].value === '(' &&
        tokensInSpan[tokensInSpan.length - 1].type === TOKEN.PUNCT && tokensInSpan[tokensInSpan.length - 1].value === ')') {
      const fnName = first.value.toUpperCase();
      const coords = { line: first.line, column: first.column, depth: first.depth };
      const innerTokens = tokensInSpan.slice(2, tokensInSpan.length - 1);

      // Multi-argument functions: SPLIT(str, delim), JOIN(arr, delim)
      if (fnName === 'SPLIT' || fnName === 'JOIN') {
        const { StringOpNode } = require('./ast');
        // Find comma to split arguments
        const commaIdx = innerTokens.findIndex(t => t.type === TOKEN.PUNCT && t.value === ',');
        let arg1, arg2;
        const leftTokens = commaIdx >= 0 ? innerTokens.slice(0, commaIdx) : innerTokens;
        const rightTokens = commaIdx >= 0 ? innerTokens.slice(commaIdx + 1) : [];
        const leftCoords = { line: leftTokens[0].line, column: leftTokens[0].column, depth: leftTokens[0].depth };
        const rightCoords = rightTokens.length > 0
          ? { line: rightTokens[0].line, column: rightTokens[0].column, depth: rightTokens[0].depth }
          : leftCoords;
        if (leftTokens.length === 1 && leftTokens[0].type === TOKEN.IDENT) {
          arg1 = new IdentifierNode(leftTokens[0].value, leftCoords);
        } else {
          arg1 = new LiteralNode(joinTokens(leftTokens), 'RAW_EXPR', leftCoords);
        }
        if (rightTokens.length === 1 && rightTokens[0].type === TOKEN.IDENT) {
          arg2 = new IdentifierNode(rightTokens[0].value, rightCoords);
        } else if (rightTokens.length > 0) {
          arg2 = new LiteralNode(joinTokens(rightTokens), 'RAW_EXPR', rightCoords);
        } else {
          arg2 = new LiteralNode('', 'STRING', rightCoords);
        }
        return new StringOpNode(arg1, arg2, fnName, coords);
      }

      if (fnName === 'SORT' || fnName === 'LEN' || fnName === 'CAP' || fnName === 'COUNT' || fnName === 'SUM' || fnName === 'FIRST' || fnName === 'LAST') {
        const innerCoords = { line: innerTokens[0].line, column: innerTokens[0].column, depth: innerTokens[0].depth };
        let argExpr;
        if (innerTokens.length === 1 && innerTokens[0].type === TOKEN.IDENT) {
          argExpr = new IdentifierNode(innerTokens[0].value, innerCoords);
        } else {
          const innerText = joinTokens(innerTokens);
          argExpr = new LiteralNode(innerText, 'RAW_EXPR', innerCoords);
        }
        if (fnName === 'LEN') return new LenCallNode(argExpr, coords);
        if (fnName === 'CAP') return new CapCallNode(argExpr, coords);
        return new ListOpNode(argExpr, fnName, coords);
      }
    }
    // Check for x[i] indexing pattern: target LBRACK index RBRACK
    if (tokensInSpan.length >= 4 &&
        tokensInSpan.some(t => t.type === TOKEN.PUNCT && t.value === '[') &&
        tokensInSpan[tokensInSpan.length - 1].type === TOKEN.PUNCT && tokensInSpan[tokensInSpan.length - 1].value === ']') {
      const bracketIdx = tokensInSpan.findIndex(t => t.type === TOKEN.PUNCT && t.value === '[');
      const targetTokens = tokensInSpan.slice(0, bracketIdx);
      const indexTokens = tokensInSpan.slice(bracketIdx + 1, tokensInSpan.length - 1);
      const targetCoords = { line: targetTokens[0].line, column: targetTokens[0].column, depth: targetTokens[0].depth };
      const indexCoords = { line: indexTokens[0].line, column: indexTokens[0].column, depth: indexTokens[0].depth };
      let targetExpr;
      if (targetTokens.length === 1 && targetTokens[0].type === TOKEN.IDENT) {
        targetExpr = new IdentifierNode(targetTokens[0].value, targetCoords);
      } else {
        targetExpr = new LiteralNode(joinTokens(targetTokens), 'RAW_EXPR', targetCoords);
      }
      let indexExpr;
      if (indexTokens.length === 1) {
        const it = indexTokens[0];
        if (it.type === TOKEN.NUMBER) indexExpr = new LiteralNode(it.value, 'NUMBER', indexCoords);
        else if (it.type === TOKEN.IDENT) indexExpr = new IdentifierNode(it.value, indexCoords);
        else indexExpr = new LiteralNode(joinTokens(indexTokens), 'RAW_EXPR', indexCoords);
      } else {
        indexExpr = new LiteralNode(joinTokens(indexTokens), 'RAW_EXPR', indexCoords);
      }
      const coords = { line: first.line, column: first.column, depth: first.depth };
      return new IndexAccessNode(targetExpr, indexExpr, coords);
    }
    // Compound expression: reconstruct as text for the legacy evaluator bridge.
    const coords = { line: tokensInSpan[0].line, column: tokensInSpan[0].column, depth: tokensInSpan[0].depth };
    const text = joinTokens(tokensInSpan);
    return new LiteralNode(text, 'RAW_EXPR', coords);
  }

  // ── helper: skip to end of logical line (past the next period) ──
  _skipToEOL() {
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.PUNCT && t.value === '.') { this.advance(); return; }
      if (t.type === TOKEN.DEPTH) return;
      this.advance();
    }
  }

  // ── helper: collect tokens up to next period or DEPTH boundary ──
  _collectLineSpan() {
    const span = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.PUNCT && t.value === '.') { this.advance(); break; }
      if (t.type === TOKEN.DEPTH) break;
      if (t.type === TOKEN.EOF) break;
      span.push(this.advance());
    }
    return joinTokens(span);
  }

  // ── helper: collect nested body until closer keyword or depth drop ──
  _collectBodyUntil(parentDepth, closers) {
    const body = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.EOF) break;
      // Proper DEPTH token
      if (t.type === TOKEN.DEPTH) {
        const d = t.depth;
        if (d <= parentDepth) break;
        this.advance();
        if (this.isAtEnd()) break;
        const st = this.parseStatement();
        if (st) body.push(st);
        continue;
      }
      // "N\" style depth prefix (NUMBER + PUNCT "\")
      if (t.type === TOKEN.NUMBER &&
          this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '\\') {
        this.advance(); this.advance(); // consume N and         if (this.isAtEnd()) break;
        const st = this.parseStatement();
        if (st) body.push(st);
        continue;
      }
      // At parent depth — check closers
      const v = String(t.value || '').toUpperCase();
      if (t.type === TOKEN.KEYWORD && closers.some(c => v === c.toUpperCase())) break;
      if (t.type === TOKEN.PUNCT && (t.value === '\\' || t.value === '/')) break;
      const st = this.parseStatement();
      if (st) body.push(st);
    }
    return body;
  }

  // ── helper: collect tokens until a keyword appears ──
  _collectUntilKeyword(keywords) {
    const span = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.KEYWORD && keywords.map(k => k.toUpperCase()).includes(t.value.toUpperCase())) break;
      if (t.type === TOKEN.PUNCT && t.value === '.') break;
      if (t.type === TOKEN.DEPTH) break;
      span.push(this.advance());
    }
    return joinTokens(span).trim();
  }

  // ── helper: emit a RawStatementNode as fallback ──
  _rawFallback(text, coords) { return new RawStatementNode(text, coords); }

  // IF cond[, inline-action | block-body] [ORIF...] [ELSE...]
  parseIfStatement(coords) {
    const { IfStatementNode } = require('./ast');
    const branches = [];
    this.advance(); // consume IF
    // Collect condition — stops at comma or period (not inside body)
    const condSpan = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.PUNCT && (t.value === ',' || t.value === '.')) break;
      if (t.type === TOKEN.DEPTH) break;
      if (t.type === TOKEN.NUMBER && this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '\\') break;
      if (t.type === TOKEN.EOF) break;
      condSpan.push(this.advance());
    }
    const ifCond = joinTokens(condSpan).trim();

    let ifBody = [];
    if (this.match(TOKEN.PUNCT, ',')) {
      this.advance(); // consume comma
      if (this.match(TOKEN.PUNCT, '.')) {
        this.advance(); // empty body
      } else if (
        this.current().type !== TOKEN.DEPTH &&
        !(this.current().type === TOKEN.NUMBER && this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '\\') &&
        this.current().type !== TOKEN.EOF &&
        !(this.current().type === TOKEN.KEYWORD && ['ORIF','ELSE'].includes(this.current().value))
      ) {
        // Inline single-action: "IF cond, SHOW x." or "IF cond, GIVE y."
        const inlineStmt = this.parseStatement();
        if (inlineStmt) ifBody = [inlineStmt];
      } else {
        if (this.match(TOKEN.PUNCT, '.')) this.advance();
        ifBody = this._collectBodyUntil(coords.depth, ['ORIF', 'ELSE']);
      }
    } else if (this.match(TOKEN.PUNCT, '.')) {
      this.advance();
    } else {
      ifBody = this._collectBodyUntil(coords.depth, ['ORIF', 'ELSE']);
    }
    branches.push({ cond: ifCond, bodyStatements: ifBody });
    while (!this.isAtEnd()) {
      if (this.match(TOKEN.DEPTH)) this.advance();
      if (this.match(TOKEN.KEYWORD, 'ORIF')) {
        this.advance();
        const orCond = [];
        while (!this.isAtEnd()) {
          const t = this.current();
          if (t.type === TOKEN.PUNCT && (t.value === ',' || t.value === '.')) break;
          if (t.type === TOKEN.DEPTH) break;
          if (t.type === TOKEN.NUMBER && this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '\\') break;
          if (t.type === TOKEN.EOF) break;
          orCond.push(this.advance());
        }
        const c = joinTokens(orCond).trim();
        let ob = [];
        if (this.match(TOKEN.PUNCT, ',')) {
          this.advance();
          if (this.current().type !== TOKEN.DEPTH &&
              !(this.current().type === TOKEN.NUMBER && this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '\\') &&
              this.current().type !== TOKEN.EOF &&
              !(this.current().type === TOKEN.KEYWORD && ['ORIF','ELSE'].includes(this.current().value))) {
            const s = this.parseStatement(); if (s) ob = [s];
          } else { ob = this._collectBodyUntil(coords.depth, ['ORIF','ELSE']); }
        } else { ob = this._collectBodyUntil(coords.depth, ['ORIF','ELSE']); }
        branches.push({ cond: c, bodyStatements: ob });
      } else if (this.match(TOKEN.KEYWORD, 'ELSE')) {
        this.advance();
        let eb = [];
        if (this.match(TOKEN.PUNCT, ',')) {
          this.advance();
          if (this.current().type !== TOKEN.DEPTH &&
              !(this.current().type === TOKEN.NUMBER && this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '\\') &&
              this.current().type !== TOKEN.EOF) {
            const s = this.parseStatement(); if (s) eb = [s];
          } else { eb = this._collectBodyUntil(coords.depth, []); }
        } else { if (this.match(TOKEN.PUNCT, '.')) this.advance(); eb = this._collectBodyUntil(coords.depth, []); }
        branches.push({ cond: null, bodyStatements: eb });
        break;
      } else { break; }
    }
    return new IfStatementNode({ branches }, coords);
  }

  // CYCLE var IN expr, body 1\.
  // CYCLE var FROM lo TO hi [STEP n], body 1\.
  parseCycleStatement(coords) {
    const { CycleStatementNode } = require('./ast');
    this.advance(); // CYCLE
    const iterVar = this.current().value; this.advance();
    let sourceExpr = null, fromExpr = null, toExpr = null, stepExpr = null;
    if (this.match(TOKEN.KEYWORD, 'IN')) {
      this.advance();
      sourceExpr = this._collectLineSpan().replace(/,$/, '').trim();
    } else if (this.match(TOKEN.KEYWORD, 'FROM')) {
      this.advance();
      fromExpr = this._collectUntilKeyword(['TO']);
      if (this.match(TOKEN.KEYWORD, 'TO')) this.advance();
      // Collect toExpr manually — STEP is tokenized as IDENT, not KEYWORD,
      // so the generic _collectUntilKeyword() helper won't stop on it.
      const toSpan = [];
      while (!this.isAtEnd()) {
        const t = this.current();
        if (t.type === TOKEN.PUNCT && (t.value === ',' || t.value === '.')) break;
        if (t.type === TOKEN.DEPTH) break;
        if (t.type === TOKEN.IDENT && t.value.toUpperCase() === 'STEP') break;
        toSpan.push(this.advance());
      }
      toExpr = joinTokens(toSpan).trim();
      if (this.current().type === TOKEN.IDENT && this.current().value.toUpperCase() === 'STEP') {
        this.advance(); // consume STEP
        stepExpr = this._collectLineSpan().replace(/,$/, '').trim();
      } else {
        if (this.match(TOKEN.PUNCT, ',')) this.advance();
        if (this.match(TOKEN.PUNCT, '.')) this.advance();
      }
    }
    let body;
    this.currentDepth++;
    try {
      body = this._collectBodyUntil(coords.depth, []);
    } finally {
      this.currentDepth--;
    }
    return new CycleStatementNode({ iterVar, sourceExpr, fromExpr, toExpr, stepExpr, bodyStatements: body }, coords);
  }

  // SEASON cond, body 1\.
  parseSeasonStatement(coords) {
    const { SeasonStatementNode } = require('./ast');
    this.advance();
    // Collect condition — stop at comma, period, or depth marker
    const condSpan = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.PUNCT && (t.value === ',' || t.value === '.')) break;
      if (t.type === TOKEN.DEPTH) break;
      if (t.type === TOKEN.EOF) break;
      if (t.type === TOKEN.NUMBER && this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '\\') break;
      condSpan.push(this.advance());
    }
    const condExpr = joinTokens(condSpan).trim();
    // Consume the comma that opens the body (if present)
    if (this.match(TOKEN.PUNCT, ',')) this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance(); // empty body
    const body = this._collectBodyUntil(coords.depth, []);
    // Consume the /SEASON. closer (left unconsumed by _collectBodyUntil)
    if (this.match(TOKEN.DEPTH)) this.advance();
    if (this.match(TOKEN.PUNCT, '/')) this.advance();
    if (this.match(TOKEN.KEYWORD, 'SEASON')) this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    return new SeasonStatementNode({ condExpr, bodyStatements: body }, coords);
  }

  // FOR item IN collection,
  //   body...
  // .
  parseForInStatement(coords) {
    const { ForInStatementNode } = require('./ast');
    this.advance(); // FOR
    const iterVar = this.current().value; this.advance();
    if (!this.match(TOKEN.KEYWORD, 'IN')) {
      // Provide a clear error
      const { RawStatementNode } = require('./ast');
      this.error(`Expected "IN" after FOR loop variable`, this.current().line, this.current().column);
      return new RawStatementNode({ text: `FOR ${iterVar} ...` }, coords);
    }
    this.advance(); // IN
    const span = [];
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.PUNCT && (t.value === ',' || t.value === '.')) break;
      if (t.type === TOKEN.EOF) break;
      span.push(this.advance());
    }
    const sourceExpr = joinTokens(span).trim();
    if (this.match(TOKEN.PUNCT, ',')) this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance(); // empty body
    const body = this._collectBodyUntil(coords.depth, []);
    // Consume the /FOR. closer (left unconsumed by _collectBodyUntil)
    if (this.match(TOKEN.DEPTH)) this.advance();
    if (this.match(TOKEN.PUNCT, '/')) this.advance();
    if (this.match(TOKEN.KEYWORD, 'FOR')) this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    return new ForInStatementNode({ iterVar, sourceExpr, bodyStatements: body }, coords);
  }

  // Pattern-matching MATCH (new) and legacy MATCH (backward compat):
  //   New: MATCH expr { Variant1(binding) -> { ... } Variant2 -> { ... } }
  //   Legacy: MATCH expr, IS val YIELD action. / ELSE YIELD action. / \\\.
  parseMatchStatement(coords) {
    const { MatchStatementNode } = require('./ast');
    this.consume(TOKEN.KEYWORD, 'MATCH', '"MATCH"');

    // Detect legacy syntax: if next non-depth token is IDENT followed by ','
    const savedPos = this.pos;
    // Skip depth markers
    while (this.current() && this.current().type === TOKEN.DEPTH) this.advance();
    const afterDepth = this.pos;
    const subjectTokens = [];
    // Collect tokens until we see ',' or '{' or '.'
    while (!this.isAtEnd()) {
      const t = this.current();
      if (t.type === TOKEN.PUNCT && t.value === ',') {
        // Legacy syntax: MATCH expr, ...
        this.pos = savedPos; // rewind to right after MATCH (already consumed)
        const subjectExpr = this._collectLineSpan().replace(/,$/, '').trim();
        const clauses = [];
        while (!this.isAtEnd()) {
          if (this.match(TOKEN.DEPTH)) this.advance();
          const t2 = this.current();
          if (!t2 || t2.type === TOKEN.EOF) break;
          if (t2.type === TOKEN.PUNCT && (t2.value === '\\' || t2.value === '/')) { this.advance(); break; }
          const text = this._collectLineSpan().trim();
          if (!text || /^\\+$/.test(text)) break;
          clauses.push({ clauseText: text });
        }
        return new MatchStatementNode({ subjectExpr, clauses }, coords);
      }
      if (t.type === TOKEN.PUNCT && t.value === '{') {
        // New syntax: MATCH expr { ... }
        break;
      }
      if (t.type === TOKEN.PUNCT && t.value === '.') break;
      this.advance();
    }
    this.pos = savedPos; // rewind to right after MATCH (already consumed)

    // New syntax: MATCH expr { Variant1(binding) -> { ... } ... }
    // Collect the subject expression up to (but not including) '{'
    const subjSpan = [];
    while (!this.isAtEnd()) {
      const pk = this.current();
      if (pk.type === TOKEN.PUNCT && pk.value === '{') break;
      if (pk.type === TOKEN.DEPTH) { this.advance(); continue; }
      subjSpan.push(this.advance());
    }
    const subjectExpr = joinTokens(subjSpan).trim();
    this.consume(TOKEN.PUNCT, '{', '"{" to open MATCH body');

    const clauses = [];
    while (!this.isAtEnd() && !this.match(TOKEN.PUNCT, '}')) {
      // Skip leading depth markers
      if (this.match(TOKEN.DEPTH)) { this.advance(); continue; }
      // Check for closing brace
      if (this.match(TOKEN.PUNCT, '}')) break;

      const variantTok = this.current();
      if (variantTok.type !== TOKEN.IDENT && variantTok.type !== TOKEN.KEYWORD) break;
      const variantName = this.advance().value;
      let binding = null;

      // Optional parenthesized binding: Variant(val)
      if (this.match(TOKEN.PUNCT, '(')) {
        this.advance();
        const bindTok = this.current();
        if (bindTok.type === TOKEN.IDENT) {
          binding = this.advance().value;
        }
        this.consume(TOKEN.PUNCT, ')', '")" after MATCH binding');
      }

      // Arrow -> { body }
      if (this.match(TOKEN.PUNCT, '->')) {
        // Single -> token
        this.advance();
      } else if (this.match(TOKEN.PUNCT, '-')) {
        // Separate '-' '>' tokens
        this.advance();
        this.consume(TOKEN.PUNCT, '>', '">" after -> in MATCH clause');
      }

      this.consume(TOKEN.PUNCT, '{', '"{" to open MATCH clause body');

      const bodyStatements = [];
      while (!this.isAtEnd() && !this.match(TOKEN.PUNCT, '}')) {
        if (this.match(TOKEN.DEPTH)) { this.advance(); continue; }
        if (this.match(TOKEN.PUNCT, '}')) break;
        if (this.match(TOKEN.KEYWORD, 'SHOW')) {
          this.advance();
          const exprSpan = [];
          while (!this.isAtEnd()) {
            const t2 = this.current();
            if (t2.type === TOKEN.PUNCT && (t2.value === '}' || t2.value === '.')) break;
            exprSpan.push(this.advance());
          }
          const exprText = joinTokens(exprSpan).trim();
          bodyStatements.push({ type: 'ShowStatement', expr: exprText });
        } else if (this.match(TOKEN.KEYWORD, 'GIVE')) {
          this.advance();
          const exprSpan = [];
          while (!this.isAtEnd()) {
            const t2 = this.current();
            if (t2.type === TOKEN.PUNCT && (t2.value === '}' || t2.value === '.')) break;
            exprSpan.push(this.advance());
          }
          const exprText = joinTokens(exprSpan).trim();
          bodyStatements.push({ type: 'GiveStatement', valueExpr: exprText });
        } else {
          // Any other statement — collect until '}' or '.'
          const bodySpan = [];
          while (!this.isAtEnd()) {
            const t2 = this.current();
            if (t2.type === TOKEN.PUNCT && (t2.value === '}' || t2.value === '.')) break;
            bodySpan.push(this.advance());
          }
          const stmtText = joinTokens(bodySpan).trim();
          if (stmtText) {
            bodyStatements.push({ type: 'GenericStatement', text: stmtText });
          }
        }
      }
      this.consume(TOKEN.PUNCT, '}', '"}" to close MATCH clause body');

      clauses.push({ variantName, binding, bodyStatements });
    }
    this.consume(TOKEN.PUNCT, '}', '"}" to close MATCH');
    if (this.match(TOKEN.PUNCT, '.')) this.advance();

    return new MatchStatementNode({ subjectExpr, clauses }, coords);
  }

  // GIVE expr.
  parseGiveStatement(coords) {
    const { GiveStatementNode } = require('./ast');
    this.advance();
    const valueExpr = this._collectLineSpan().trim();
    return new GiveStatementNode({ valueExpr }, coords);
  }

  // STOP IF cond [, action]
  parseStopIfStatement(coords) {
    const { StopIfStatementNode } = require('./ast');
    this.advance(); // STOP
    if (this.match(TOKEN.KEYWORD, 'IF')) this.advance();
    const rest = this._collectLineSpan().trim();
    const ci = rest.indexOf(',');
    const condExpr = ci >= 0 ? rest.slice(0, ci).trim() : rest;
    const actionExpr = ci >= 0 ? rest.slice(ci + 1).trim() : null;
    return new StopIfStatementNode({ condExpr, actionExpr }, coords);
  }

  // PUT val INTO list|SELF:list
  parsePutStatement(coords) {
    const { PutStatementNode } = require('./ast');
    this.advance();
    const rest = this._collectLineSpan().trim();
    const m = rest.match(/^(.+?)\s+INTO\s+(.+)$/i);
    if (!m) return this._rawFallback('PUT ' + rest, coords);
    return new PutStatementNode({ valueExpr: m[1].trim(), targetExpr: m[2].trim() }, coords);
  }

  // TAKE val FROM list
  parseTakeStatement(coords) {
    const { TakeStatementNode } = require('./ast');
    this.advance();
    const rest = this._collectLineSpan().trim();
    const m = rest.match(/^(.+?)\s+FROM\s+(.+)$/i);
    if (!m) return this._rawFallback('TAKE ' + rest, coords);
    return new TakeStatementNode({ valueExpr: m[1].trim(), listExpr: m[2].trim() }, coords);
  }

  // LINK "key" WITH val IN map
  parseLinkStatement(coords) {
    const { LinkStatementNode } = require('./ast');
    this.advance();
    const rest = this._collectLineSpan().trim();
    const m = rest.match(/^("(?:[^"]*)"|\S+)\s+WITH\s+(.+?)\s+IN\s+(\w+)$/i);
    if (!m) return this._rawFallback('LINK ' + rest, coords);
    return new LinkStatementNode({ keyExpr: m[1], valueExpr: m[2].trim(), mapIdent: m[3] }, coords);
  }

  parseSortStatement(coords) {
    this.advance();
    // Check for SORT(expr) expression syntax
    if (this.match(TOKEN.PUNCT, '(')) {
      const { ListOpNode, IdentifierNode, LiteralNode } = require('./ast');
      this.advance(); // consume (
      const argTokens = [];
      let parenDepth = 1;
      while (!this.isAtEnd() && parenDepth > 0) {
        const t = this.current();
        if (t.type === TOKEN.PUNCT && t.value === '(') { parenDepth++; argTokens.push(this.advance()); }
        else if (t.type === TOKEN.PUNCT && t.value === ')') {
          parenDepth--;
          if (parenDepth === 0) break;
          argTokens.push(this.advance());
        } else {
          argTokens.push(this.advance());
        }
      }
      if (this.isAtEnd() && parenDepth > 0) {
        storm('SYNTAX_STORM', 'Unclosed parenthesis in SORT()', coords.line, coords.column);
      }
      this.advance(); // consume )
      let argExpr;
      if (argTokens.length === 1 && argTokens[0].type === TOKEN.IDENT) {
        argExpr = new IdentifierNode(argTokens[0].value, { line: argTokens[0].line, column: argTokens[0].column, depth: argTokens[0].depth });
      } else {
        argExpr = new LiteralNode(joinTokens(argTokens), 'RAW_EXPR', coords);
      }
      if (this.match(TOKEN.PUNCT, '.')) this.advance();
      return new ListOpNode(argExpr, 'SORT', coords);
    }
    // Legacy: SORT ident.
    const { SortStatementNode } = require('./ast');
    const id = this.current().value; this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    return new SortStatementNode({ listIdent: id }, coords);
  }

  parseShakeStatement(coords) {
    const { ShakeStatementNode } = require('./ast');
    this.advance();
    const id = this.current().value; this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    return new ShakeStatementNode({ listIdent: id }, coords);
  }

  parseEvaporateStatement(coords) {
    const { EvaporateStatementNode } = require('./ast');
    this.advance();
    const id = this.current().value; this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    return new EvaporateStatementNode({ identifier: id }, coords);
  }

  parseLockStatement(coords) {
    const { LockStatementNode } = require('./ast');
    this.advance();
    const id = this.current().value; this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    return new LockStatementNode({ identifier: id }, coords);
  }

  // BRAID a WITH b AS result [MAP]
  parseBraidStatement(coords) {
    const { BraidStatementNode } = require('./ast');
    this.advance();
    const rest = this._collectLineSpan().trim();
    const m = rest.match(/^(\w+)\s+WITH\s+(\w+)\s+AS\s+(\w+)(\s+MAP)?$/i);
    if (!m) return this._rawFallback('BRAID ' + rest, coords);
    return new BraidStatementNode({ list1: m[1], list2: m[2], resultIdent: m[3], asMap: !!m[4] }, coords);
  }

  // HARVEST "url" [opts] AS result
  parseHarvestStatement(coords) {
    const { HarvestStatementNode } = require('./ast');
    this.advance();
    const rest = this._collectLineSpan().trim();
    const m = rest.match(/^(.+?)\s+AS\s+(\w+)$/i);
    if (!m) return this._rawFallback('HARVEST ' + rest, coords);
    const head = m[1], resultIdent = m[2];
    const urlM    = head.match(/^("(?:[^"]*)"|\S+)/);
    const methodM = head.match(/METHOD:(\w+)/i);
    const bodyM   = head.match(/BODY:("(?:[^"]*)"|\w+)/i);
    const headM   = head.match(/HEADERS:(\w+)/i);
    const timeM   = head.match(/TIMEOUT:([\d.]+)/i);
    return new HarvestStatementNode({
      urlExpr: urlM ? urlM[1] : head, resultIdent,
      method:       methodM ? methodM[1].toUpperCase() : 'GET',
      bodyExpr:     bodyM   ? bodyM[1]   : null,
      headersIdent: headM   ? headM[1]   : null,
      timeoutExpr:  timeM   ? timeM[1]   : null,
    }, coords);
  }

  parseAnalyzeStatement(coords) {
    const { AnalyzeStatementNode } = require('./ast');
    this.advance();
    const id = this.current().value; this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    return new AnalyzeStatementNode({ identifier: id }, coords);
  }

  parseWaitStatement(coords) {
    const { WaitStatementNode } = require('./ast');
    this.advance();
    const secsExpr = this._collectLineSpan().trim();
    return new WaitStatementNode({ secsExpr }, coords);
  }

  // VERIFY "label", assertion
  parseVerifyStatement(coords) {
    const { VerifyStatementNode } = require('./ast');
    this.advance(); // VERIFY
    const rest = this._collectLineSpan().trim();
    const m = rest.match(/^"([^"]+)",?\s+(.+)$/);
    if (!m) return this._rawFallback('VERIFY ' + rest, coords);
    return new VerifyStatementNode({ label: m[1], assertion: m[2].trim() }, coords);
  }

  // SUITE "name", body SUITE/.
  parseSuiteStatement(coords) {
    const { SuiteStatementNode } = require('./ast');
    this.advance(); // SUITE
    let name = '';
    if (this.match(TOKEN.STRING)) { name = this.advance().value; }
    else {
      const raw = this._collectLineSpan().trim();
      name = raw.replace(/^"|"$/g, '').replace(/,$/, '').trim();
    }
    if (this.match(TOKEN.PUNCT, ',')) this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    const body = [];
    while (!this.isAtEnd()) {
      // Skip DEPTH tokens
      if (this.match(TOKEN.DEPTH)) { this.advance(); continue; }
      // Skip N\ style depth markers (NUMBER + PUNCT "\")
      if (this.current().type === TOKEN.NUMBER &&
          this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '\\') {
        this.advance(); this.advance(); continue;
      }
      const t = this.current();
      if (!t || t.type === TOKEN.EOF) break;
      // SUITE/ is tokenized as SUITE keyword + "/" punct
      if (t.type === TOKEN.KEYWORD && String(t.value||'').toUpperCase() === 'SUITE' &&
          this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '/') {
        this.advance(); this.advance();
        if (this.match(TOKEN.PUNCT, '.')) this.advance();
        break;
      }
      const st = this.parseStatement();
      if (st) body.push(st);
    }
    return new SuiteStatementNode({ name, bodyStatements: body }, coords);
  }

  parsePlantStatement(coords) {
    const { PlantStatementNode } = require('./ast');
    this.advance();
    const libName = this._collectLineSpan().trim();
    return new PlantStatementNode({ libName }, coords);
  }

  parseMissionStatement(coords) {
    const { MissionStatementNode } = require('./ast');
    this.advance(); // MISSION
    if (this.match(TOKEN.PUNCT, ':')) this.advance();
    const mode = this.current().value; this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    return new MissionStatementNode({ mode }, coords);
  }

  parseRootStatement(coords) {
    const { RootStatementNode } = require('./ast');
    this.advance(); // ROOT
    const identifier = this.current().value; this.advance();
    if (this.match(TOKEN.KEYWORD, 'TO')) this.advance();
    const valueExpr = this._collectLineSpan().trim();
    return new RootStatementNode({ identifier, valueExpr }, coords);
  }

  parseRootScopeStatement(coords) {
    const { RootScopeStatementNode } = require('./ast');
    this.advance(); // ROOT_SCOPE
    const identifier = this.current().value; this.advance();
    if (this.match(TOKEN.PUNCT, ',')) this.advance();
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    const links = [];
    while (!this.isAtEnd()) {
      if (this.match(TOKEN.DEPTH)) this.advance();
      const t = this.current();
      if (!t || t.type === TOKEN.EOF) break;
      // ROOT_SCOPE/ is tokenized as ROOT_SCOPE keyword + "/" punct
      if (t.type === TOKEN.KEYWORD && String(t.value).toUpperCase() === 'ROOT_SCOPE') {
        this.advance(); // consume ROOT_SCOPE
        if (this.match(TOKEN.PUNCT, '/')) this.advance();
        if (this.match(TOKEN.PUNCT, '.')) this.advance();
        break;
      }
      if (this.match(TOKEN.KEYWORD, 'LINK')) {
        this.advance();
        const rest = this._collectLineSpan().trim();
        const m = rest.match(/^("(?:[^"]*)"|\S+)\s+WITH\s+(.+?)\s+IN\s+\w+$/i);
        if (m) links.push({ key: m[1], valueExpr: m[2].trim() });
      } else { this._skipToEOL(); }
    }
    return new RootScopeStatementNode({ identifier, links }, coords);
  }

  // ── IMPORT "filename". ──────────────────────────────────────────
  parseImportStatement(coords) {
    this.consume(TOKEN.KEYWORD, 'IMPORT', '"IMPORT"');
    const pathTok = this.current();
    if (pathTok.type !== TOKEN.STRING) {
      storm('SYNTAX_STORM', `Expected a string literal after IMPORT, found "${pathTok.value}"`,
        pathTok.line, pathTok.column);
    }
    const importPath = this.advance().value; // raw string from tokens (already unquoted)
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    return new ImportStatementNode({ path: importPath, resolvedPath: null, importedStatements: [] }, coords);
  }

  // ── StructName{ arg1, arg2, ... } ─────────────────────────────
  parseStructInstantiation() {
    const nameTok = this.current();
    const structName = this.advance().value;
    const coords = { line: nameTok.line, column: nameTok.column, depth: nameTok.depth };
    this.consume(TOKEN.PUNCT, '{', '"{" after struct name');
    const args = [];
    while (!this.isAtEnd() && !this.match(TOKEN.PUNCT, '}')) {
      // Collect tokens until , or }
      const span = [];
      while (!this.isAtEnd()) {
        if (this.match(TOKEN.PUNCT, ',') || this.match(TOKEN.PUNCT, '}')) break;
        span.push(this.advance());
      }
      if (span.length === 0) break;
      // Parse the span as an expression
      let argExpr;
      if (span.length === 1) {
        const t = span[0];
        const ac = { line: t.line, column: t.column, depth: t.depth };
        if (t.type === TOKEN.NUMBER) argExpr = new LiteralNode(t.value, 'NUMBER', ac);
        else if (t.type === TOKEN.STRING) argExpr = new LiteralNode(t.value, 'STRING', ac);
        else if (t.type === TOKEN.FACT) argExpr = new LiteralNode(t.value, 'FACT', ac);
        else if (t.type === TOKEN.IDENT) argExpr = new IdentifierNode(t.value, ac);
        else argExpr = new LiteralNode(joinTokens(span), 'RAW_EXPR', ac);
      } else {
        // Check for member access: IDENT . IDENT
        if (span.length === 3 &&
            span[0].type === TOKEN.IDENT &&
            span[1].type === TOKEN.PUNCT && span[1].value === '.' &&
            span[2].type === TOKEN.IDENT) {
          const objCoords = { line: span[0].line, column: span[0].column, depth: span[0].depth };
          const obj = new IdentifierNode(span[0].value, objCoords);
          argExpr = new MemberAccessNode({ object: obj, member: span[2].value }, objCoords);
        } else {
          argExpr = new LiteralNode(joinTokens(span), 'RAW_EXPR', { line: span[0].line, column: span[0].column, depth: span[0].depth });
        }
      }
      args.push(argExpr);
      if (this.match(TOKEN.PUNCT, ',')) this.advance();
    }
    this.consume(TOKEN.PUNCT, '}', '"}" to close struct instantiation');
    return new StructInstantiationExpr({ structName, args }, coords);
  }

  // ── { name: expr, ... } anonymous struct literal ─────────────────
  parseStructLiteral() {
    const coords = { line: this.current().line, column: this.current().column, depth: this.current().depth };
    this.consume(TOKEN.PUNCT, '{', '"{" to start struct literal');
    const fields = [];
    while (!this.isAtEnd() && !this.match(TOKEN.PUNCT, '}')) {
      const keyTok = this.current();
      if (keyTok.type !== TOKEN.IDENT && keyTok.type !== TOKEN.KEYWORD) break;
      const name = this.advance().value;
      this.consume(TOKEN.PUNCT, ':', '":" after field name');
      const span = [];
      while (!this.isAtEnd()) {
        if (this.match(TOKEN.PUNCT, ',') || this.match(TOKEN.PUNCT, '}')) break;
        span.push(this.advance());
      }
      let valExpr;
      if (span.length === 1) {
        const t = span[0];
        const ac = { line: t.line, column: t.column, depth: t.depth };
        if (t.type === TOKEN.NUMBER) valExpr = new LiteralNode(t.value, 'NUMBER', ac);
        else if (t.type === TOKEN.STRING) valExpr = new LiteralNode(t.value, 'STRING', ac);
        else if (t.type === TOKEN.FACT) valExpr = new LiteralNode(t.value, 'FACT', ac);
        else if (t.type === TOKEN.IDENT) valExpr = new IdentifierNode(t.value, ac);
        else valExpr = new LiteralNode(joinTokens(span), 'RAW_EXPR', ac);
      } else {
        valExpr = new LiteralNode(joinTokens(span), 'RAW_EXPR',
          { line: span[0].line, column: span[0].column, depth: span[0].depth });
      }
      fields.push({ name, value: valExpr });
      if (this.match(TOKEN.PUNCT, ',')) this.advance();
    }
    this.consume(TOKEN.PUNCT, '}', '"}" to close struct literal');
    return new StructLiteralNode({ structName: null, fields }, coords);
  }

  // ── SHAPE/STRUCT name { field1(TYPE) or field1: TYPE, ... } ───
  parseStructDeclaration(coords) {
    const keyword = ['SHAPE', 'STRUCT'].find(kw => this.match(TOKEN.KEYWORD, kw));
    this.consume(TOKEN.KEYWORD, keyword, '"SHAPE" or "STRUCT"');
    const nameTok = this.current();
    if (nameTok.type !== TOKEN.IDENT && nameTok.type !== TOKEN.KEYWORD) {
      storm('SYNTAX_STORM', `Expected a struct name after ${keyword}, found "${nameTok.value}"`,
        nameTok.line, nameTok.column);
    }
    const name = this.advance().value;
    this.structNames.add(name);
    this.consume(TOKEN.PUNCT, '{', '"{" to open struct body');

    const fields = [];
    while (!this.isAtEnd() && !this.match(TOKEN.PUNCT, '}')) {
      const fNameTok = this.current();
      if (fNameTok.type !== TOKEN.IDENT && fNameTok.type !== TOKEN.KEYWORD) break;
      const fName = this.advance().value;
      let fType;
      // STRUCT uses colon syntax (field: TYPE), SHAPE uses parens (field(TYPE))
      if (this.match(TOKEN.PUNCT, ':')) {
        this.advance();
        const fTypeTok = this.current();
        if (fTypeTok.type === TOKEN.KEYWORD || fTypeTok.type === TOKEN.IDENT) {
          fType = this.advance().value;
        } else {
          storm('SYNTAX_STORM', `Expected a type for field "${fName}", found "${fTypeTok.value}"`,
            fTypeTok.line, fTypeTok.column);
        }
      } else {
        this.consume(TOKEN.PUNCT, '(', '"(" or ":" after field name');
        const fTypeTok = this.current();
        if (fTypeTok.type === TOKEN.KEYWORD && ['NUM','SCL','TX','FACT','LIST','MAP'].includes(fTypeTok.value)) {
          fType = this.advance().value;
        } else if (fTypeTok.type === TOKEN.IDENT) {
          fType = this.advance().value;
        } else {
          storm('SYNTAX_STORM', `Expected a type for field "${fName}", found "${fTypeTok.value}"`,
            fTypeTok.line, fTypeTok.column);
        }
        this.consume(TOKEN.PUNCT, ')', '")" after field type');
      }
      fields.push({ name: fName, varType: fType.toUpperCase() });
      if (this.match(TOKEN.PUNCT, ',')) this.advance();
    }
    this.consume(TOKEN.PUNCT, '}', '"}" to close struct body');
    if (this.match(TOKEN.PUNCT, '.')) this.advance();
    return new StructDeclarationNode({ name, fields }, coords);
  }

  parseVariantDeclaration(coords) {
    this.consume(TOKEN.KEYWORD, 'CHOICE', '"CHOICE"');
    const nameTok = this.current();
    if (nameTok.type !== TOKEN.IDENT && nameTok.type !== TOKEN.KEYWORD) {
      storm('SYNTAX_STORM', `Expected a CHOICE name, found "${nameTok.value}"`,
        nameTok.line, nameTok.column);
    }
    const name = this.advance().value;
    this.consume(TOKEN.PUNCT, '{', '"{" to open CHOICE body');

    const variants = [];
    while (!this.isAtEnd() && !this.match(TOKEN.PUNCT, '}')) {
      const vNameTok = this.current();
      if (vNameTok.type !== TOKEN.IDENT && vNameTok.type !== TOKEN.KEYWORD) break;
      const vName = this.advance().value;
      let vType = null;
      if (this.match(TOKEN.PUNCT, '(')) {
        this.advance();
        const typeTok = this.current();
        if (typeTok.type === TOKEN.KEYWORD && ['NUM','SCL','TX','FACT','LIST'].includes(typeTok.value)) {
          vType = this.advance().value;
        } else if (typeTok.type === TOKEN.IDENT) {
          vType = this.advance().value;
        } else {
          storm('SYNTAX_STORM', `Expected a type for variant "${vName}", found "${typeTok.value}"`,
            typeTok.line, typeTok.column);
        }
        this.consume(TOKEN.PUNCT, ')', '")" after variant type');
      }
      variants.push({ name: vName, type: vType ? vType.toUpperCase() : null });
      if (this.match(TOKEN.PUNCT, ',')) this.advance();
    }
    this.consume(TOKEN.PUNCT, '}', '"}" to close CHOICE body');
    if (this.match(TOKEN.PUNCT, '.')) this.advance();

    const { VariantDeclarationNode } = require('./ast');
    return new VariantDeclarationNode({ name, variants }, coords);
  }

  parseInfuseStatement(coords) {
    this.advance();
    const rest = this._collectLineSpan().trim();
    return this._rawFallback('INFUSE ' + rest, coords);
  }

  parseAbsorbStatement(coords) {
    this.advance();
    const rest = this._collectLineSpan().trim();
    return this._rawFallback('ABSORB ' + rest, coords);
  }

  parseSealStatement(coords) {
    this.advance();
    const rest = this._collectLineSpan().trim();
    return this._rawFallback('SEAL ' + rest, coords);
  }

  parseEmptyStatement(coords) {
    const { EvaporateStatementNode } = require('./ast'); // reuse pattern
    this.advance();
    const rest = this._collectLineSpan().trim();
    return this._rawFallback('EMPTY ' + rest, coords);
  }

  // ── Array literal [a, b, c, ...] ──────────────────────────
  parseArrayLiteral() {
    this.consume(TOKEN.PUNCT, '[', '"[" to start array literal');
    const coords = { line: this.current().line, column: this.current().column, depth: this.current().depth };
    const elements = [];
    while (!this.isAtEnd() && !this.match(TOKEN.PUNCT, ']')) {
      // Detect struct instantiation: IdentName{ ... }
      if (this.peek(0).type === TOKEN.IDENT && this.peek(1).type === TOKEN.PUNCT && this.peek(1).value === '{') {
        elements.push(this.parseStructInstantiation());
        if (this.match(TOKEN.PUNCT, ',')) this.advance();
        continue;
      }
      const span = [];
      while (!this.isAtEnd()) {
        if (this.match(TOKEN.PUNCT, ',') || this.match(TOKEN.PUNCT, ']')) break;
        span.push(this.advance());
      }
      if (span.length === 0) break;
      let el;
      if (span.length === 1) {
        const t = span[0];
        const ac = { line: t.line, column: t.column, depth: t.depth };
        if (t.type === TOKEN.NUMBER) el = new LiteralNode(t.value, 'NUMBER', ac);
        else if (t.type === TOKEN.STRING) el = new LiteralNode(t.value, 'STRING', ac);
        else if (t.type === TOKEN.FACT) el = new LiteralNode(t.value, 'FACT', ac);
        else if (t.type === TOKEN.IDENT) el = new IdentifierNode(t.value, ac);
        else el = new LiteralNode(joinTokens(span), 'RAW_EXPR', ac);
      } else {
        el = new LiteralNode(joinTokens(span), 'RAW_EXPR', { line: span[0].line, column: span[0].column, depth: span[0].depth });
      }
      elements.push(el);
      if (this.match(TOKEN.PUNCT, ',')) this.advance();
    }
    this.consume(TOKEN.PUNCT, ']', '"]" to close array literal');
    return new ArrayLiteralNode(elements, coords);
  }

  // ── Map literal { key: value, key: value, ... } ────────────
  parseMapLiteral() {
    this.consume(TOKEN.PUNCT, '{', '"{" to start map literal');
    const coords = { line: this.current().line, column: this.current().column, depth: this.current().depth };
    const entries = [];
    while (!this.isAtEnd() && !this.match(TOKEN.PUNCT, '}')) {
      // Parse key
      const keySpan = [];
      while (!this.isAtEnd()) {
        if (this.match(TOKEN.PUNCT, ':') || this.match(TOKEN.PUNCT, ',') || this.match(TOKEN.PUNCT, '}')) break;
        keySpan.push(this.advance());
      }
      if (keySpan.length === 0) break;
      let key;
      if (keySpan.length === 1) {
        const t = keySpan[0];
        const ac = { line: t.line, column: t.column, depth: t.depth };
        if (t.type === TOKEN.NUMBER) key = new LiteralNode(t.value, 'NUMBER', ac);
        else if (t.type === TOKEN.STRING) key = new LiteralNode(t.value, 'STRING', ac);
        else if (t.type === TOKEN.IDENT || t.type === TOKEN.KEYWORD) key = new IdentifierNode(t.value, ac);
        else key = new LiteralNode(joinTokens(keySpan), 'RAW_EXPR', ac);
      } else {
        key = new LiteralNode(joinTokens(keySpan), 'RAW_EXPR', { line: keySpan[0].line, column: keySpan[0].column, depth: keySpan[0].depth });
      }
      // Expect colon
      if (!this.match(TOKEN.PUNCT, ':')) {
        storm('SYNTAX_STORM', 'Expected ":" after key in map literal', this.current().line, this.current().column);
      }
      this.advance(); // consume :
      // Parse value
      const valSpan = [];
      while (!this.isAtEnd()) {
        if (this.match(TOKEN.PUNCT, ',') || this.match(TOKEN.PUNCT, '}')) break;
        valSpan.push(this.advance());
      }
      let value;
      if (valSpan.length === 1) {
        const t = valSpan[0];
        const ac = { line: t.line, column: t.column, depth: t.depth };
        if (t.type === TOKEN.NUMBER) value = new LiteralNode(t.value, 'NUMBER', ac);
        else if (t.type === TOKEN.STRING) value = new LiteralNode(t.value, 'STRING', ac);
        else if (t.type === TOKEN.FACT) value = new LiteralNode(t.value, 'FACT', ac);
        else if (t.type === TOKEN.IDENT || t.type === TOKEN.KEYWORD) value = new IdentifierNode(t.value, ac);
        else value = new LiteralNode(joinTokens(valSpan), 'RAW_EXPR', ac);
      } else {
        value = new LiteralNode(joinTokens(valSpan), 'RAW_EXPR', { line: valSpan[0].line, column: valSpan[0].column, depth: valSpan[0].depth });
      }
      const pairCoords = { line: keySpan[0].line, column: keySpan[0].column, depth: keySpan[0].depth };
      entries.push(new (require('./ast').KeyValuePairNode)({ key, value }, pairCoords));
      if (this.match(TOKEN.PUNCT, ',')) this.advance();
    }
    this.consume(TOKEN.PUNCT, '}', '"}" to close map literal');
    return new (require('./ast').MapLiteralNode)({ entries }, coords);
  }

} // end class Parser

/** Convenience: tokenize + parse source in one call (no import resolution). */
function parse(source) {
  const tokens = tokenize(source);
  const parser = new Parser(tokens);
  return parser.parseProgram();
}

/**
 * Resolve IMPORT statements in a ProgramNode recursively.
 * Replaces each ImportStatementNode with the imported file's statements.
 * @param {ProgramNode} program
 * @param {string} baseDir directory of the current file
 * @param {Set<string>} importStack paths currently being resolved (cycle detection)
 * @param {string} currentFilePath the file path of the current program (for cycle tracking)
 * @returns {Array} newStatements array with imports inlined
 */
function resolveImports(program, baseDir, importStack, currentFilePath) {
  importStack = importStack || new Set();
  // Track the current file to detect cycles
  if (currentFilePath) {
    importStack.add(currentFilePath);
  }
  const newStatements = [];
  for (const stmt of program.statements) {
    if (stmt instanceof ImportStatementNode) {
      const importRelPath = stmt.path;
      let resolvedPath;
      // Standard library paths (std/...) resolve to the project's std/ directory
      if (importRelPath.startsWith('std/') || importRelPath.startsWith('std\\')) {
        const relativePart = importRelPath.replace(/^std[/\\]/, '');
        resolvedPath = path.resolve(STD_DIR, relativePart);
      } else if (path.isAbsolute(importRelPath)) {
        resolvedPath = path.normalize(importRelPath);
      } else {
        resolvedPath = path.resolve(baseDir, importRelPath);
      }
      // Check for .plnt extension
      if (!resolvedPath.endsWith('.plnt')) {
        resolvedPath += '.plnt';
      }
      resolvedPath = path.normalize(resolvedPath);

      if (importStack.has(resolvedPath)) {
        storm('SYNTAX_STORM',
          `Circular import detected: "${importRelPath}" (${resolvedPath})`,
          stmt.line, stmt.column);
      }

      if (!fs.existsSync(resolvedPath)) {
        // Try without .plnt extension
        const altPath = resolvedPath.replace(/\.plnt$/, '');
        if (fs.existsSync(altPath)) {
          resolvedPath = altPath;
        } else {
          storm('SYNTAX_STORM',
            `Import file not found: "${importRelPath}" (looked at ${resolvedPath})`,
            stmt.line, stmt.column);
        }
      }

      importStack.add(resolvedPath);
      const source = fs.readFileSync(resolvedPath, 'utf-8');
      const tokens = tokenize(source);
      const parser = new Parser(tokens);
      const importedProgram = parser.parseProgram();
      // Recursively resolve imports in the imported file
      const resolved = resolveImports(importedProgram, path.dirname(resolvedPath), importStack, resolvedPath);
      importStack.delete(resolvedPath);

      stmt.resolvedPath = resolvedPath;
      stmt.importedStatements = resolved;

      // Inline the imported statements
      for (const s of resolved) {
        newStatements.push(s);
      }
      continue;
    }
    newStatements.push(stmt);
  }
  return newStatements;
}

/**
 * Parse a .plnt file from disk, resolving IMPORT statements recursively.
 * @param {string} filePath absolute path to the .plnt file
 * @returns {ProgramNode} fully resolved program
 */
function parseFile(filePath, opts) {
  opts = opts || {};
  const absPath = path.resolve(filePath);
  const source = fs.readFileSync(absPath, 'utf-8');
  const tokens = tokenize(source);
  const parser = new Parser(tokens);
  const program = parser.parseProgram();
  // Auto-inject std/prelude at the AST level (preserves source line numbers)
  const isStdFile = absPath.startsWith(STD_DIR + path.sep) || absPath === STD_DIR;
  if (!isStdFile && !opts.noPrelude) {
    const preludeStmt = new (require('./ast').ImportStatementNode)(
      { path: 'std/prelude', resolvedPath: null, importedStatements: [] },
      { line: 0, column: 0, depth: 0 }
    );
    program.statements.unshift(preludeStmt);
  }
  const resolvedStmts = resolveImports(program, path.dirname(absPath), new Set(), absPath);
  return new ProgramNode(resolvedStmts);
}

module.exports = { Parser, parse, parseFile, resolveImports, RawStatementNode };
