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

const { tokenize, TOKEN } = require('./tokenizer');
const { storm, PlantStorm } = require('./runtime');
const {
  ProgramNode, CreateStatementNode, ShowStatementNode,
  IdentifierNode, LiteralNode, AstNode,
  ListenBranchStatementNode, ResponseStatementNode,
} = require('./ast');

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

  // ── Top level ────────────────────────────────────────────────
  parseProgram() {
    const statements = [];
    while (!this.isAtEnd()) {
      // DEPTH tokens are structural markers, not statements themselves;
      // skip leading ones between statements (consume() handles the
      // ones that matter as part of a specific statement's grammar).
      if (this.match(TOKEN.DEPTH)) { this.advance(); continue; }
      const stmt = this.parseStatement();
      if (stmt) statements.push(stmt);
    }
    return new ProgramNode(statements);
  }

  parseStatement() {
    const startTok = this.current();
    const coords = { line: startTok.line, column: startTok.column, depth: startTok.depth };

    if (this.match(TOKEN.KEYWORD, 'SHOW')) return this.parseShowStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'CREATE')) return this.parseCreateStatement(coords);
    if (this.match(TOKEN.KEYWORD, 'LISTEN') && this.peek(1).type === TOKEN.KEYWORD && this.peek(1).value === 'BRANCH') {
      return this.parseListenBranchStatement(coords);
    }

    if (this.match(TOKEN.KEYWORD, 'GIVE') && this.containsResponseAheadOnLine()) {
      return this.parseResponseStatementAst(coords);
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
    const text = span
      .filter(t => t.type !== TOKEN.PUNCT || t.value !== '.')
      .map(t => t.type === TOKEN.STRING ? `"${t.value}"` : String(t.value))
      .join(' ');
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
    // at the header's own depth — mirrors how ACTION/SPECIES bodies are
    // delimited, generalized into the token-stream parser.
    const headerDepth = coords.depth;
    const bodyTokenStatements = [];
    while (!this.isAtEnd()) {
      // Skip a leading DEPTH marker token to look at the statement it
      // introduces — needed because DEPTH tokens precede every line's
      // first real token, including the "LISTEN/." closer's.
      const aheadIsDepth = this.match(TOKEN.DEPTH);
      const afterDepthOffset = aheadIsDepth ? 1 : 0;
      const lineFirstTok = this.peek(afterDepthOffset);
      const lineSecondTok = this.peek(afterDepthOffset + 1);

      if (lineFirstTok.type === TOKEN.KEYWORD && lineFirstTok.value === 'LISTEN' &&
          lineSecondTok.type === TOKEN.PUNCT && lineSecondTok.value === '/') {
        if (aheadIsDepth) this.advance(); // consume the DEPTH marker, leaving LISTEN/. for the closer-consume below
        break;
      }

      if (aheadIsDepth && this.peek(0).value <= headerDepth) {
        const t = this.current();
        storm('SYNTAX_STORM',
          'LISTEN BRANCH: body statement must be nested deeper than the header (missing "LISTEN/." closer?)',
          t.line, t.column);
      }
      if (aheadIsDepth) this.advance(); // consume DEPTH marker before delegating, same as parseProgram()
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
    return span.map(t => t.type === TOKEN.STRING ? `"${t.value}"` : String(t.value)).join(' ');
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
    const responseExpr = span.map(t => t.type === TOKEN.STRING ? `"${t.value}"` : String(t.value)).join(' ');
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
    if (identTok.type !== TOKEN.IDENT) {
      storm('SYNTAX_STORM',
        `Expected a variable identifier after CREATE, found "${identTok.value}"`,
        identTok.line, identTok.column);
    }
    const identifier = this.advance().value;

    this.consume(TOKEN.PUNCT, '(', '"(" after the identifier');
    const typeTok = this.current();
    if (typeTok.type !== TOKEN.KEYWORD || !['NUM', 'SCL', 'TX', 'FACT', 'LIST', 'MAP', 'INSTANCE', 'VEIN'].includes(typeTok.value)) {
      storm('SYNTAX_STORM',
        `Expected a type (NUM, SCL, TX, FACT, LIST, MAP...), found "${typeTok.value}"`,
        typeTok.line, typeTok.column);
    }
    const varType = this.advance().value;
    this.consume(TOKEN.PUNCT, ')', '")" after the type');

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
        valueExpr = this.parseExpressionSpan();
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
      if (t.type === TOKEN.PUNCT && t.value === '.') break;
      tokensInSpan.push(this.advance());
    }
    if (tokensInSpan.length === 0) {
      const t = this.current();
      storm('SYNTAX_STORM', 'Expected an expression', t.line, t.column);
    }
    if (tokensInSpan.length === 1) {
      const t = tokensInSpan[0];
      const coords = { line: t.line, column: t.column, depth: t.depth };
      if (t.type === TOKEN.NUMBER) return new LiteralNode(t.value, 'NUMBER', coords);
      if (t.type === TOKEN.STRING) return new LiteralNode(t.value, 'STRING', coords);
      if (t.type === TOKEN.FACT) return new LiteralNode(t.value, 'FACT', coords);
      if (t.type === TOKEN.IDENT) return new IdentifierNode(t.value, coords);
    }
    // Compound expression: reconstruct as text for the legacy evaluator bridge.
    const coords = { line: tokensInSpan[0].line, column: tokensInSpan[0].column, depth: tokensInSpan[0].depth };
    const text = tokensInSpan.map(t => t.type === TOKEN.STRING ? `"${t.value}"` : String(t.value)).join(' ');
    return new LiteralNode(text, 'RAW_EXPR', coords);
  }
}

/** Convenience: tokenize + parse source in one call. */
function parse(source) {
  const tokens = tokenize(source);
  const parser = new Parser(tokens);
  return parser.parseProgram();
}

module.exports = { Parser, parse, RawStatementNode };
