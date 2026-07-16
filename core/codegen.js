'use strict';
/**
 * core/codegen.js — PlantLang → C Code Generator
 *
 * Translates a parsed PlantLang AST (core/parser.js) into standalone C99
 * source code that can be compiled with gcc into a native binary.
 *
 * SUPPORTED SUBSET (v0.19.0 — first generator pass):
 *   - CREATE / SET / INCREASE / DECREASE          (NUM, SCL, TX, FACT)
 *   - SHOW (string literals, identifiers, +concatenation of NUM/TX)
 *   - IF / ORIF / ELSE  (block form)
 *   - CYCLE var FROM lo TO hi [STEP k]
 *   - SEASON cond (while loop)
 *   - Basic arithmetic: + - * / % **
 *   - Basic comparisons: IS, IS NOT, GREATER THAN, LESS THAN,
 *     GREATER THAN OR EQUAL, LESS THAN OR EQUAL, BETWEEN
 *   - AND / OR / NOT
 *
 * NOT YET SUPPORTED (falls back to a compile-time error naming the
 * unsupported construct, so the caller can report it clearly instead
 * of silently producing broken C):
 *   LIST, MAP, ACTION/REAP, SPECIES/BLOOM, WEATHER/SHELTER,
 *   MATCH, HARVEST, LISTEN BRANCH, VERIFY/SUITE, PULSE/WHENEVER,
 *   BRAID, TAP/ABSORB/INFUSE/SEAL, library calls (math:/strings:/lists:)
 *
 * Usage:
 *   const { generate } = require('./core/codegen');
 *   const { parse }    = require('./core/parser');
 *   const prog = parse(source);
 *   const { code, errors } = generate(prog);
 *   if (errors.length) { ...report... } else { fs.writeFileSync('out.c', code); }
 */

class CodegenError extends Error {
  constructor(message, line, column) {
    super(message);
    this.name = 'CodegenError';
    this.line = line || 0;
    this.column = column || 0;
  }
}

// ── C type mapping ──────────────────────────────────────────────────────────
const C_TYPE = {
  NUM:  'long',
  SCL:  'double',
  TX:   'char*',
  FACT: 'int',        // 0/1, C has no native bool without <stdbool.h> — keep it simple
};

function cType(plType) {
  return C_TYPE[plType] || null; // null = unsupported (LIST/MAP/INSTANCE/VEIN)
}

// ── Identifier sanitizing ───────────────────────────────────────────────────
// PlantLang identifiers are already C-safe ([a-zA-Z_][a-zA-Z0-9_]*) except
// for a small set of C reserved words we must rename to avoid collisions.
const C_RESERVED = new Set([
  'int','long','double','char','float','void','return','if','else','while',
  'for','do','switch','case','break','continue','struct','union','typedef',
  'static','const','sizeof','goto','default','enum','extern','register',
  'signed','unsigned','volatile','auto','short','main','printf','scanf',
]);

function cIdent(name) {
  if (C_RESERVED.has(name)) return `pl_${name}`;
  return name;
}

// ── Expression translator ───────────────────────────────────────────────────
// PlantLang stores most expressions as raw strings (e.g. "x GREATER THAN 50",
// "a + b"). We translate the recognized operator keywords to C operators and
// leave identifiers/numbers/strings as-is (after identifier sanitizing).

const OP_MAP = [
  // multi-word operators must come first (longest match)
  [/\bGREATER THAN OR EQUAL\b/gi, '>='],
  [/\bLESS THAN OR EQUAL\b/gi,    '<='],
  [/\bGREATER THAN\b/gi,          '>'],
  [/\bLESS THAN\b/gi,             '<'],
  [/\bIS NOT\b/gi,                '!='],
  [/\bIS\b/gi,                    '=='],
  [/\bAND\b/gi,                   '&&'],
  [/\bOR\b/gi,                    '||'],
  [/\bNOT\b/gi,                   '!'],
  [/\bTRUE\b/g,                   '1'],
  [/\bFALSE\b/g,                  '0'],
];

class ExprTranslator {
  constructor(scope, errors) {
    this.scope  = scope;   // Map<name, plType>
    this.errors = errors;  // shared error list
  }

  // Translate a raw PlantLang expression string into a C expression string.
  // Returns { code, type } where type is a best-effort PlantLang type guess.
  translate(expr, line, column) {
    if (expr === null || expr === undefined) return { code: '0', type: 'NUM' };
    let s = String(expr).trim();

    // BETWEEN lo hi  →  handled specially by caller (cond context) but also
    // supported standalone as "(x) >= lo && (x) <= hi" is NOT valid without
    // knowing the left operand, so BETWEEN is only translated inside cond().
    // Here we just translate operators/booleans and pass through identifiers,
    // numbers, and string literals unchanged.

    // Protect string literals from operator substitution by temporarily
    // extracting them.
    const strings = [];
    s = s.replace(/"([^"]*)"/g, (m) => {
      strings.push(m);
      return `\u0001${strings.length - 1}\u0001`;
    });

    for (const [re, rep] of OP_MAP) s = s.replace(re, rep);

    // Restore strings
    s = s.replace(/\u0001(\d+)\u0001/g, (_, i) => strings[i]);

    // Sanitize bare identifiers (skip inside string literals, which are
    // now restored — re-protect them for this pass)
    const strings2 = [];
    s = s.replace(/"([^"]*)"/g, (m) => { strings2.push(m); return `\u0002${strings2.length-1}\u0002`; });
    s = s.replace(/\b([a-zA-Z_][a-zA-Z0-9_]*)\b/g, (m) => {
      if (/^(int|long|double|char|void)$/.test(m)) return m;
      return cIdent(m);
    });
    s = s.replace(/\u0002(\d+)\u0002/g, (_, i) => strings2[i]);

    // ** power operator → needs pow() from <math.h>; translate a ** b to
    // (long)pow(a,b) for NUM context (best-effort, no nested-expression parsing)
    const powMatch = s.match(/^(.+?)\s*\*\*\s*(.+)$/);
    if (powMatch) {
      s = `(long)pow(${powMatch[1].trim()}, ${powMatch[2].trim()})`;
    }

    return { code: s, type: 'UNKNOWN' };
  }

  // Translate a condition (used in IF/SEASON). Adds BETWEEN support.
  cond(expr, line, column) {
    let s = String(expr).trim();
    const between = s.match(/^(.+?)\s+BETWEEN\s+(-?[\d.]+)\s+(-?[\d.]+)$/i);
    if (between) {
      const [, lhsRaw, lo, hi] = between;
      const lhs = this.translate(lhsRaw.trim(), line, column).code;
      return `(${lhs} >= ${lo} && ${lhs} <= ${hi})`;
    }
    return this.translate(s, line, column).code;
  }
}

// ── Main generator ──────────────────────────────────────────────────────────
class CodeGenerator {
  constructor() {
    this.errors  = [];       // CodegenError[]
    this.lines   = [];       // output C source lines (function body)
    this.decls   = [];       // top-level declarations (none needed — all in main for now)
    this.indent  = 1;
    this.scope   = new Map(); // name → PlantLang type, flat (single-function subset)
    this.usesMath = false;
    this.tempCounter = 0;
  }

  emit(line) {
    this.lines.push('    '.repeat(this.indent) + line);
  }

  error(message, node) {
    this.errors.push(new CodegenError(message, node && node.line, node && node.column));
  }

  unsupported(node, label) {
    this.error(`Unsupported construct for C code generation: ${label || node.type}. ` +
      `This feature works in the interpreter but cannot yet be compiled to native code.`, node);
  }

  // ── Entry point ────────────────────────────────────────────────────────────
  generate(programNode) {
    this.emit('// Generated by PlantLang Chloroplast C Code Generator v0.19.0');
    for (const node of (programNode.statements || [])) {
      this._genStatement(node);
    }
    return this._assemble();
  }

  _assemble() {
    const header = [
      '#include <stdio.h>',
      '#include <stdlib.h>',
      '#include <string.h>',
    ];
    if (this.usesMath) header.push('#include <math.h>');
    header.push('', 'int main(void) {');
    const footer = ['    return 0;', '}', ''];
    const code = [...header, ...this.lines, ...footer].join('\n');
    return { code, errors: this.errors };
  }

  // ── Statement dispatch ──────────────────────────────────────────────────────
  _genStatement(node) {
    if (!node || !node.type) return;
    switch (node.type) {
      case 'MissionStatement':
      case 'PlantStatement':      /* no-op for native codegen */ return;
      case 'CreateStatement':     return this._genCreate(node);
      case 'SetStatement':        return this._genSet(node);
      case 'IncreaseStatement':   return this._genIncDec(node, '+=');
      case 'DecreaseStatement':   return this._genIncDec(node, '-=');
      case 'ShowStatement':       return this._genShow(node);
      case 'IfStatement':         return this._genIf(node);
      case 'CycleStatement':      return this._genCycle(node);
      case 'SeasonStatement':     return this._genSeason(node);
      case 'LockStatement':       return; // no runtime effect needed in native code
      case 'RawStatement':
        if (node.text && node.text.trim()) {
          // Empty/closer raw statements are fine to skip silently;
          // non-empty ones mean something wasn't migrated to a typed node.
          if (!/^\\+$/.test(node.text.trim())) {
            this.unsupported(node, `"${node.text.trim().slice(0, 40)}"`);
          }
        }
        return;
      default:
        this.unsupported(node);
        return;
    }
  }

  // ── CREATE ───────────────────────────────────────────────────────────────────
  _genCreate(node) {
    const ct = cType(node.varType);
    if (!ct) {
      this.unsupported(node, `CREATE with type ${node.varType}`);
      return;
    }
    const name = cIdent(node.identifier);
    const et = new ExprTranslator(this.scope, this.errors);
    const valNode = node.valueExpr;

    let valueC;
    if (valNode && valNode.type === 'Literal') {
      if (valNode.literalType === 'STRING') {
        valueC = JSON.stringify(valNode.value);
      } else if (valNode.literalType === 'NUMBER') {
        valueC = String(valNode.value);
      } else if (valNode.literalType === 'BOOLEAN') {
        valueC = valNode.value ? '1' : '0';
      } else if (valNode.literalType === 'RAW_EXPR') {
        valueC = et.translate(valNode.value, node.line, node.column).code;
      } else {
        valueC = '0';
      }
    } else if (typeof valNode === 'string') {
      valueC = et.translate(valNode, node.line, node.column).code;
    } else {
      valueC = node.varType === 'TX' ? '""' : '0';
    }

    this.scope.set(node.identifier, node.varType);

    if (node.varType === 'TX') {
      // Use a fixed buffer for simple string handling (no dynamic growth yet)
      this.emit(`char ${name}[256]; strncpy(${name}, ${valueC}, 255); ${name}[255]=0;`);
    } else {
      this.emit(`${ct} ${name} = ${valueC};`);
    }
  }

  // ── SET ──────────────────────────────────────────────────────────────────────
  _genSet(node) {
    const plType = this.scope.get(node.identifier);
    if (!plType) {
      this.error(`SET: "${node.identifier}" was not declared with CREATE`, node);
      return;
    }
    const name = cIdent(node.identifier);
    const et = new ExprTranslator(this.scope, this.errors);
    const valueC = et.translate(node.valueExpr, node.line, node.column).code;

    if (plType === 'TX') {
      this.emit(`strncpy(${name}, ${valueC}, 255); ${name}[255]=0;`);
    } else {
      this.emit(`${name} = ${valueC};`);
    }
  }

  // ── INCREASE / DECREASE ─────────────────────────────────────────────────────
  _genIncDec(node, op) {
    const plType = this.scope.get(node.identifier);
    if (!plType) {
      this.error(`${op === '+=' ? 'INCREASE' : 'DECREASE'}: "${node.identifier}" was not declared`, node);
      return;
    }
    if (plType !== 'NUM' && plType !== 'SCL') {
      this.error(`${op === '+=' ? 'INCREASE' : 'DECREASE'}: "${node.identifier}" is ${plType} — only NUM/SCL supported`, node);
      return;
    }
    const name = cIdent(node.identifier);
    const et = new ExprTranslator(this.scope, this.errors);
    const amountC = et.translate(node.amountExpr, node.line, node.column).code;
    this.emit(`${name} ${op} ${amountC};`);
  }

  // ── SHOW ─────────────────────────────────────────────────────────────────────
  _genShow(node) {
    const expr = node.expr;
    if (!expr) { this.emit('printf("\\n");'); return; }

    if (expr.type === 'Literal' && expr.literalType === 'STRING') {
      this.emit(`printf("%s\\n", ${JSON.stringify(expr.value)});`);
      return;
    }

    if (expr.type === 'Identifier') {
      const name = expr.name || expr.identifier || expr.value;
      const plType = this.scope.get(name);
      if (!plType) { this.error(`SHOW: "${name}" was not declared`, node); return; }
      this.emit(this._printfFor(plType, cIdent(name)));
      return;
    }

    if (expr.type === 'Literal' && expr.literalType === 'RAW_EXPR') {
      this._genShowExpr(expr.value, node);
      return;
    }

    this.unsupported(node, 'SHOW with this expression form');
  }

  // SHOW of a compound expression: try to detect "a + b" string concatenation
  // vs numeric arithmetic, and pick the right printf format.
  _genShowExpr(raw, node) {
    const s = String(raw).trim();

    // Simple identifier?
    if (/^[a-zA-Z_]\w*$/.test(s)) {
      const plType = this.scope.get(s);
      if (!plType) { this.error(`SHOW: "${s}" was not declared`, node); return; }
      this.emit(this._printfFor(plType, cIdent(s)));
      return;
    }

    // "a" + b + "c" style concatenation — only supported when all operands
    // are known TX variables or string literals (uses snprintf into a buffer).
    if (s.includes(' + ') && (s.includes('"') || this._allOperandsAreTX(s))) {
      this._genShowConcat(s, node);
      return;
    }

    // Otherwise assume it's a numeric expression
    const et = new ExprTranslator(this.scope, this.errors);
    const translated = et.translate(s, node.line, node.column).code;
    // Guess NUM vs SCL by presence of a decimal point or SCL operands
    const looksDecimal = /\.\d/.test(s) || this._anyOperandIsSCL(s);
    this.emit(`printf("${looksDecimal ? '%g' : '%ld'}\\n", ${looksDecimal ? '(double)(' : '(long)('}${translated}));`);
  }

  _allOperandsAreTX(s) {
    const parts = s.split(' + ').map(p => p.trim());
    return parts.every(p => {
      if (/^".*"$/.test(p)) return true;
      return this.scope.get(p) === 'TX';
    });
  }

  _anyOperandIsSCL(s) {
    const parts = s.split(/[+\-*/]/).map(p => p.trim());
    return parts.some(p => this.scope.get(p) === 'SCL');
  }

  _genShowConcat(s, node) {
    const parts = s.split(' + ').map(p => p.trim());
    const fmtParts = [];
    const args = [];
    for (const p of parts) {
      if (/^".*"$/.test(p)) {
        fmtParts.push('%s');
        args.push(p);
      } else {
        const plType = this.scope.get(p);
        if (!plType) { this.error(`SHOW: "${p}" was not declared`, node); return; }
        if (plType === 'TX') { fmtParts.push('%s'); args.push(cIdent(p)); }
        else if (plType === 'NUM') { fmtParts.push('%ld'); args.push(cIdent(p)); }
        else if (plType === 'SCL') { fmtParts.push('%g'); args.push(cIdent(p)); }
        else if (plType === 'FACT') { fmtParts.push('%d'); args.push(cIdent(p)); }
        else { this.error(`SHOW: cannot concatenate type ${plType}`, node); return; }
      }
    }
    this.emit(`printf("${fmtParts.join('')}\\n", ${args.join(', ')});`);
  }

  _printfFor(plType, cExpr) {
    if (plType === 'TX')   return `printf("%s\\n", ${cExpr});`;
    if (plType === 'NUM')  return `printf("%ld\\n", ${cExpr});`;
    if (plType === 'SCL')  return `printf("%g\\n", ${cExpr});`;
    if (plType === 'FACT') return `printf("%s\\n", (${cExpr}) ? "true" : "false");`;
    return `printf("%ld\\n", (long)(${cExpr}));`;
  }

  // ── IF / ORIF / ELSE ─────────────────────────────────────────────────────────
  _genIf(node) {
    const et = new ExprTranslator(this.scope, this.errors);
    (node.branches || []).forEach((branch, i) => {
      if (branch.cond === null) {
        this.emit('else {');
      } else {
        const condC = et.cond(branch.cond, node.line, node.column);
        this.emit(`${i === 0 ? 'if' : 'else if'} (${condC}) {`);
      }
      this.indent++;
      for (const stmt of (branch.bodyStatements || [])) this._genStatement(stmt);
      this.indent--;
      this.emit('}');
    });
  }

  // ── CYCLE (numeric range only — LIST iteration not yet supported) ───────────
  _genCycle(node) {
    if (node.sourceExpr !== null) {
      this.unsupported(node, 'CYCLE ... IN list (LIST iteration not yet supported in codegen)');
      return;
    }
    const et = new ExprTranslator(this.scope, this.errors);
    const varName = cIdent(node.iterVar);
    const fromC = et.translate(node.fromExpr, node.line, node.column).code;
    const toC   = et.translate(node.toExpr,   node.line, node.column).code;
    const stepC = node.stepExpr ? et.translate(node.stepExpr, node.line, node.column).code : '1';

    this.scope.set(node.iterVar, 'NUM');
    this.emit(`for (long ${varName} = ${fromC}; ${varName} <= ${toC}; ${varName} += ${stepC}) {`);
    this.indent++;
    for (const stmt of (node.bodyStatements || [])) this._genStatement(stmt);
    this.indent--;
    this.emit('}');
  }

  // ── SEASON (while loop) ──────────────────────────────────────────────────────
  _genSeason(node) {
    const et = new ExprTranslator(this.scope, this.errors);
    const condC = et.cond(node.condExpr, node.line, node.column);
    this.emit(`while (${condC}) {`);
    this.indent++;
    for (const stmt of (node.bodyStatements || [])) this._genStatement(stmt);
    this.indent--;
    this.emit('}');
  }
}

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * generate(programNode) → { code, errors }
 *
 * Translates a parsed PlantLang AST into C99 source code.
 * `code` is the full C source (always produced, even with errors, for
 * partial inspection); `errors` is an array of CodegenError — if
 * non-empty, the code should NOT be compiled (it references unsupported
 * constructs or type errors).
 */
function generate(programNode) {
  const gen = new CodeGenerator();
  return gen.generate(programNode);
}

module.exports = { generate, CodegenError, CodeGenerator };
