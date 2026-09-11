'use strict';
// ═══════════════════════════════════════════════════════════════
//  Diagnostics — centralized Storm error formatter with a visual
//  caret (^) pointer aimed at the exact source line/column.
// ═══════════════════════════════════════════════════════════════
const fs = require('fs');

const C = {
  reset: '\x1b[0m', bold: '\x1b[1m', dim: '\x1b[2m',
  red: '\x1b[31m', yellow: '\x1b[33m', cyan: '\x1b[36m', gray: '\x1b[90m'
};

/**
 * Read a specific 1-indexed line out of a source file, returning '' if
 * the file can't be read or the line is out of range. Cached per-call
 * (cheap enough for CLI/REPL use — not meant for hot loops).
 */
function readSourceLine(filePath, lineNum) {
  if (!filePath || !lineNum) return null;
  try {
    const text = fs.readFileSync(filePath, 'utf8');
    const lines = text.split('\n');
    if (lineNum < 1 || lineNum > lines.length) return null;
    return lines[lineNum - 1];
  } catch (_) {
    return null;
  }
}

/**
 * Build the full multi-line diagnostic panel for a PlantStorm.
 *
 * @param {Error} err          The thrown PlantStorm (or generic Error)
 * @param {string} [filePath]  Path to the .plnt source file, if known
 * @param {string} [sourceText] Raw source text, used instead of re-reading
 *                              the file when running from a string (REPL).
 * @returns {string} ANSI-colored, ready-to-print diagnostic block
 */
function formatStormDiagnostic(err, filePath, sourceText) {
  const stormType = err.stormType || err.name || 'ANY_STORM';
  const line = err.line;
  const column = err.column;
  const out = [];

  out.push(`${C.red}${C.bold}⛈️  Atmospheric Storm Panic: ${stormType}${C.reset}`);

  if (line) {
    const loc = `${filePath || '<source>'}:${line}${column ? ':' + column : ''}`;
    out.push(`${C.cyan}  --> ${loc}${C.reset}`);

    let lineText = null;
    if (sourceText) {
      const lines = sourceText.split('\n');
      lineText = (line >= 1 && line <= lines.length) ? lines[line - 1] : null;
    } else if (filePath) {
      lineText = readSourceLine(filePath, line);
    }

    if (lineText !== null) {
      const gutter = String(line);
      const gutterPad = ' '.repeat(gutter.length);
      out.push('');
      out.push(`${C.gray}  ${gutter} \\ ${C.reset}${lineText}`);

      // Build the caret line: pad to `column` (1-indexed), then '^'
      const col = Math.max(1, column || 1);
      const pointerPad = ' '.repeat(col - 1);
      out.push(`${C.gray}  ${gutterPad}   ${C.reset}${pointerPad}${C.red}${C.bold}^${C.reset}`);
    }
  }

  out.push('');
  out.push(`${C.red}Error: ${err.message}${C.reset}`);

  return out.join('\n');
}

module.exports = { formatStormDiagnostic, readSourceLine };
