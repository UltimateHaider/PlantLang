'use strict';

// ═══════════════════════════════════════════════════════════════
//  src/interpreter/show_formatter.js
//  v0.38.0 — Nested Struct Renderer for SHOW()
//  Formats struct instances as indented JSON-like tree views.
// ═══════════════════════════════════════════════════════════════

/**
 * Format a value for SHOW display, with nested struct tree rendering.
 * @param {any} val - the value to format
 * @param {number} indent - current indentation level
 * @returns {string} formatted output
 */
function formatShowValue(val, indent = 0) {
  const pad = '  '.repeat(indent);

  if (val === null || val === undefined) {
    return val === null ? 'NULL' : 'VOID';
  }

  // Detect struct instances (both SHAPE __structType and SPECIES __species)
  if (typeof val === 'object' && !Array.isArray(val)) {
    const structType = val.__structType || val.__species;
    if (structType) {
      return _formatStructInstance(val, structType, indent);
    }
    // Plain object (MAP-like)
    if (Object.keys(val).length > 0 && !val.__actions) {
      const keys = Object.keys(val);
      if (keys.length <= 3 && indent === 0) {
        // Inline for small objects at root
        const pairs = keys.map(k => `${k}: ${formatShowValue(val[k], indent + 1)}`).join(', ');
        return `{ ${pairs} }`;
      }
      const lines = ['{'];
      for (const k of keys) {
        lines.push(`  ${pad}  ${k}: ${formatShowValue(val[k], indent + 1)}`);
      }
      lines.push(pad + '}');
      return lines.join('\n');
    }
  }

  // Arrays
  if (Array.isArray(val)) {
    if (val.length === 0) return '[]';
    if (val.length <= 5 && indent === 0) {
      return '[' + val.map(v => formatShowValue(v, indent + 1)).join(', ') + ']';
    }
    const lines = ['['];
    for (const item of val) {
      lines.push(`  ${pad}${formatShowValue(item, indent + 1)}`);
    }
    lines.push(pad + ']');
    return lines.join('\n');
  }

  // Strings
  if (typeof val === 'string') return val;

  // Numbers, booleans
  return String(val);
}

/**
 * Format a struct instance as an indented tree view.
 */
function _formatStructInstance(val, typeName, indent) {
  const pad = '  '.repeat(indent);
  const lines = [`${typeName} {`];

  for (const [key, value] of Object.entries(val)) {
    // Skip internal fields
    if (key.startsWith('__')) continue;
    const formatted = formatShowValue(value, indent + 1);
    // Check if the value is a multi-line string (nested struct)
    if (formatted.includes('\n')) {
      lines.push(`  ${pad}${key}:`);
      // Add indented lines for the nested value
      const nestedLines = formatted.split('\n');
      for (const nl of nestedLines) {
        lines.push(`    ${pad}${nl}`);
      }
    } else {
      lines.push(`  ${pad}${key}: ${formatted}`);
    }
  }

  lines.push(pad + '}');
  return lines.join('\n');
}

module.exports = { formatShowValue };
