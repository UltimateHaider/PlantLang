'use strict';

// Lines whose stripped form (no trailing comma) open a block
// These should NEVER be joined with the next line via comma-continuation
const BLOCK_OPENER_RE = [
  /^ACTION\s+\w+/i,
  /^SPECIES\s+\w+/i,
  /^WEATHER$/i,
  /^SHELTER\b/i,
  /^CALM$/i,
  /^CYCLE\s+/i,
  /^SEASON\s+/i,
  /^WHENEVER\s+/i,
  /^MATCH\s+/i,
  /^ROOT_SCOPE\s+/i,
  /^IF\s+/i,
  /^ORIF\s+/i,
  /^ELSE$/i,
  /^SUITE\s+"/i,
  /^SHOW_VERIFY_SUMMARY$/i,
];

function isBlockOpener(stripped) {
  // Remove depth prefix before checking
  const text = stripped.replace(/^\d+\\\s*/, '').trim();
  return BLOCK_OPENER_RE.some(re => re.test(text));
}

function lex(source) {
  const rawLines = source.split('\n');
  const statements = [];
  let buffer = '', bufferLine = 0, bufferCol = 1;

  for (let i = 0; i < rawLines.length; i++) {
    const original = rawLines[i];
    const raw = original.trim();
    const lineNum = i + 1;
    // Column where the trimmed content starts (1-indexed)
    const leadingWs = original.length - original.trimStart().length;
    const colNum = leadingWs + 1;
    if (!raw || raw.startsWith('#')) continue;

    // Determine if this raw line (stripped of trailing comma) is a block opener
    const strippedRaw = raw.endsWith(',') ? raw.slice(0, -1).trim() : (raw.endsWith('.') ? raw.slice(0, -1).trim() : raw);

    if (!buffer) {
      // Starting fresh
      if (raw.endsWith('.')) {
        statements.push(parseStatement(strippedRaw, lineNum, colNum));
      } else if (raw.endsWith(',') && isBlockOpener(strippedRaw)) {
        // Block opener with trailing comma = emit as statement, comma is syntax not continuation
        statements.push(parseStatement(strippedRaw, lineNum, colNum));
      } else if (raw.endsWith(',')) {
        // True continuation — start buffering
        buffer = raw;
        bufferLine = lineNum;
        bufferCol = colNum;
      } else {
        // No punctuation — emit as-is
        statements.push(parseStatement(raw, lineNum, colNum));
      }
    } else {
      // We're in a continuation buffer
      buffer += ' ' + raw;
      if (raw.endsWith('.')) {
        const text = buffer.slice(0, -1).trim();
        statements.push(parseStatement(text, bufferLine, bufferCol));
        buffer = ''; bufferLine = 0; bufferCol = 1;
      } else if (!raw.endsWith(',')) {
        statements.push(parseStatement(buffer.trim(), bufferLine, bufferCol));
        buffer = ''; bufferLine = 0; bufferCol = 1;
      }
      // else keep buffering
    }
  }

  if (buffer.trim()) {
    const text = buffer.endsWith(',') ? buffer.slice(0, -1).trim() : buffer.trim();
    statements.push(parseStatement(text, bufferLine, bufferCol));
  }

  return statements;
}

function parseStatement(text, line, column) {
  const col = column || 1;
  const m = text.match(/^(\d+)\\\s*/);
  if (m) {
    // Depth prefix consumed (e.g. "2\ ") — the real statement text starts
    // after it, so its column shifts right by the prefix length.
    return { depth: parseInt(m[1]), text: text.slice(m[0].length).trim(), line, column: col + m[0].length };
  }
  return { depth: 0, text, line, column: col };
}

module.exports = { lex };
