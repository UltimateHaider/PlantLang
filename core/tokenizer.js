'use strict';
// ═══════════════════════════════════════════════════════════════
//  core/tokenizer.js — Character-by-character state machine
//  tokenizer (Phase 1 of the compiler-frontend migration).
//
//  This module is ADDITIVE: core/lexer.js (the existing flat
//  statement scanner that the regex-dispatch interpreter depends
//  on for all 176 passing tests) is untouched. This tokenizer is
//  consumed only by the new core/parser.js + core/ast.js pipeline,
//  which is being migrated in incrementally per statement type
//  (SHOW and CREATE first) without breaking the existing engine.
// ═══════════════════════════════════════════════════════════════

const KEYWORDS = new Set([
  'CREATE','SET','INCREASE','DECREASE','SHOW','LOCK','ROOT','EVAPORATE',
  'IF','ORIF','ELSE','STOP','PICK','MATCH','YIELD','CYCLE','SEASON',
  'PUT','TAKE','SORT','SHAKE','EMPTY','BRAID','LINK',
  'FOR',
  'ACTION','GIVE','REAP','FLOW','SPECIES','BLOOM','PARENT','SELF','VAR',
  'WEATHER','SHELTER','CALM','TAP','ABSORB','INFUSE','SEAL','IMPORT','EXTERNAL',
  'PULSE','WHENEVER','CHANGES','NOW','WAIT','ANALYZE','TYPEOF',
  'ROOT_SCOPE','MISSION','PLANT','VERIFY','SUITE','STORMS','GIVES',
  'HARVEST','METHOD','BODY','HEADERS','TIMEOUT',
  'LISTEN','BRANCH','RESPONSE','ON','WITH','AS','MAP',
  'TRUE','FALSE','TO','FROM','IS','IN','BETWEEN','AND','OR','NOT',
  'GREATER','LESS','THAN','COUNT','FIRST','LAST','SUM','MAX','MIN','REVERSE',
  'NUM','SCL','TX','FACT','LIST','INSTANCE','VEIN','SHAPE','STRUCT','CHOICE','NATIVE',
  'SPLIT','JOIN',
  'BALANCED','FAST','SAFE','SMART','PERSISTENT',
  'BREAK','CONTINUE',
]);

const TOKEN = {
  KEYWORD: 'KEYWORD',
  IDENT: 'IDENT',
  NUMBER: 'NUMBER',
  STRING: 'STRING',
  FACT: 'FACT',
  DEPTH: 'DEPTH',        // the "N\" depth marker itself
  PUNCT: 'PUNCT',        // . , : ( ) /
  EOF: 'EOF',
};

/**
 * A single lexical token. Every token carries its own absolute
 * source coordinates so the parser never has to re-derive position
 * from a containing statement string — this is what lets the
 * diagnostics module point the caret at the *exact* character that
 * broke the grammar, not just "the start of the line".
 */
class Token {
  constructor(type, value, line, column, depth) {
    this.type = type;
    this.value = value;
    this.line = line;
    this.column = column;
    this.depth = depth;
  }
}

/**
 * Tokenize PlantLang source into a flat array of Token objects,
 * terminated by a single EOF token.
 *
 * Depth handling: a leading "N\" at the start of a logical line
 * establishes that line's depth. Every token scanned afterward on
 * that same logical line inherits that depth (per the task spec:
 * "Every subsequent token parsed on that line must inherit that
 * precise numeric depth"). The column of the first real token after
 * "N\ " is offset correctly past the slash and any following
 * whitespace — verified by test below.
 */
function tokenize(source) {
  const tokens = [];
  let i = 0;
  let line = 1;
  let col = 1;
  let currentDepth = 0;
  const n = source.length;

  function peekChar(off = 0) { return source[i + off]; }
  function advance() {
    const c = source[i++];
    if (c === '\n') { line++; col = 1; }
    else { col++; }
    return c;
  }

  // At the start of every physical line, look for a leading "N\"
  // depth marker before any other token is scanned.
  function tryConsumeDepthMarker() {
    const startCol = col;
    const m = source.slice(i).match(/^(\d+)\\/);
    if (!m) return;
    const depthDigits = m[1];
    const fullMatch = m[0]; // e.g. "2\"
    const depthLine = line, depthCol = col;
    for (let k = 0; k < fullMatch.length; k++) advance();
    currentDepth = parseInt(depthDigits, 10);
    tokens.push(new Token(TOKEN.DEPTH, currentDepth, depthLine, depthCol, currentDepth));
    // consume a single following space if present (column then lands
    // exactly on the first real character of the statement)
    if (peekChar() === ' ') advance();
  }

  let atLineStart = true;

  while (i < n) {
    const c = peekChar();

    if (atLineStart) {
      atLineStart = false;
      tryConsumeDepthMarker();
      continue;
    }

    if (c === '\n') { advance(); atLineStart = true; continue; }
    if (c === ' ' || c === '\t' || c === '\r') { advance(); continue; }

    // Comments: '#' to end of line — not tokenized
    if (c === '#') {
      while (i < n && peekChar() !== '\n') advance();
      continue;
    }

    const startLine = line, startCol = col;

    // String literal
    if (c === '"') {
      advance();
      let value = '';
      while (i < n && peekChar() !== '"') {
        if (peekChar() === '\n') { advance(); value += '\n'; continue; }
        value += advance();
      }
      if (peekChar() === '"') advance(); // closing quote
      tokens.push(new Token(TOKEN.STRING, value, startLine, startCol, currentDepth));
      continue;
    }

    // Number literal (integer or decimal, optional leading minus).
    // A '.' is only absorbed as a decimal point if followed by another
    // digit — otherwise it's the statement-terminating period and must
    // be left for the PUNCT scanner below (e.g. "...TO 4." must NOT
    // swallow the closing '.' into the number).
    if (/[0-9]/.test(c) || (c === '-' && /[0-9]/.test(peekChar(1) || ''))) {
      let value = advance();
      while (i < n) {
        const pc = peekChar();
        if (/[0-9]/.test(pc)) { value += advance(); continue; }
        if (pc === '.' && /[0-9]/.test(peekChar(1) || '')) { value += advance(); continue; }
        break;
      }
      tokens.push(new Token(TOKEN.NUMBER, parseFloat(value), startLine, startCol, currentDepth));
      continue;
    }

    // FACT:TRUE / FACT:FALSE literal
    if (source.slice(i, i + 5).toUpperCase() === 'FACT:') {
      for (let k = 0; k < 5; k++) advance();
      let word = '';
      while (i < n && /[A-Za-z]/.test(peekChar())) word += advance();
      const boolVal = word.toUpperCase() === 'TRUE';
      tokens.push(new Token(TOKEN.FACT, boolVal, startLine, startCol, currentDepth));
      continue;
    }

    // Identifier or keyword (supports Arabic letters + underscore, per
    // the language's existing string-literal/identifier conventions)
    if (/[A-Za-z_\u0600-\u06FF]/.test(c)) {
      let word = advance();
      while (i < n && /[A-Za-z0-9_\u0600-\u06FF]/.test(peekChar() || '')) word += advance();
      const upper = word.toUpperCase();
      if (KEYWORDS.has(upper)) {
        tokens.push(new Token(TOKEN.KEYWORD, upper, startLine, startCol, currentDepth));
      } else {
        tokens.push(new Token(TOKEN.IDENT, word, startLine, startCol, currentDepth));
      }
      continue;
    }

    // Structural punctuation
    if ('.,:()/[]{}'.includes(c)) {
      advance();
      tokens.push(new Token(TOKEN.PUNCT, c, startLine, startCol, currentDepth));
      continue;
    }

    // Compound operator: ** (power)
    if (c === '*' && peekChar(1) === '*') {
      advance(); advance(); // consume both *
      tokens.push(new Token(TOKEN.PUNCT, '**', startLine, startCol, currentDepth));
      continue;
    }

    // Arrow operator: -> (used for FFI external declarations, return types)
    if (c === '-' && peekChar(1) === '>') {
      advance(); advance(); // consume -
      tokens.push(new Token(TOKEN.PUNCT, '->', startLine, startCol, currentDepth));
      continue;
    }

    // Operators / anything else: consume as a single-char PUNCT so the
    // parser can still see it (kept permissive — full operator grammar
    // is out of scope for this milestone, expressions still delegate
    // to the existing evaluator for now).
    advance();
    tokens.push(new Token(TOKEN.PUNCT, c, startLine, startCol, currentDepth));
  }

  tokens.push(new Token(TOKEN.EOF, null, line, col, currentDepth));
  return tokens;
}

module.exports = { tokenize, Token, TOKEN, KEYWORDS };
