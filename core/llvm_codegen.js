'use strict';
/**
 * core/llvm_codegen.js — PlantLang → LLVM IR Code Generator
 *
 * This is the REAL compiler backend: it emits LLVM IR text (.ll), which is
 * then lowered to native object code via `llc` (LLVM's static compiler) and
 * linked into a binary via `gcc`. This is the same pipeline architecture
 * used by Rust, Swift, Julia, and Zig — PlantLang gets LLVM's decades of
 * optimization work (register allocation, instruction selection, loop
 * vectorization, inlining, dead code elimination) for free.
 *
 * This supersedes core/codegen.js (the direct-to-C generator), which only
 * covered NUM/SCL/TX/FACT + IF/CYCLE/SEASON. This generator additionally
 * handles proper SSA-form value tracking (matching how LLVM itself
 * "thinks") and real string concatenation via runtime buffer building.
 *
 * STILL NOT SUPPORTED in this first LLVM-backend pass (same honesty
 * principle as the old codegen: fail loudly and precisely rather than
 * silently emit broken IR):
 *   LIST, MAP, ACTION/REAP, SPECIES/BLOOM, WEATHER/SHELTER, MATCH, HARVEST,
 *   LISTEN BRANCH, VERIFY/SUITE, PULSE/WHENEVER, BRAID, TAP/ABSORB/INFUSE/SEAL
 *
 * ── Why LLVM IR text instead of an LLVM C++/Node binding ───────────────────
 * Emitting textual .ll and shelling out to `llc` avoids requiring a compiled
 * LLVM binding (llvm-node, etc.) which is fragile to install and version-
 * lock across environments. `llc` itself is a stable, versioned CLI tool
 * present in any LLVM install — this is also literally how `rustc --emit=llvm-ir`
 * and Julia's `code_llvm()` expose their own IR for inspection, so treating
 * LLVM IR as a text format we generate is a well-trodden, legitimate design.
 *
 * Usage:
 *   const { generate } = require('./core/llvm_codegen');
 *   const { parse }    = require('./core/parser');
 *   const prog = parse(source);
 *   const { ir, errors } = generate(prog);
 *   // then: llc -O2 out.ll -o out.s && gcc out.s -o out
 */

class CodegenError extends Error {
  constructor(message, line, column) {
    super(message);
    this.name = 'CodegenError';
    this.line = line || 0;
    this.column = column || 0;
  }
}

// ── PlantLang type → LLVM type ───────────────────────────────────────────────
const LLVM_TYPE = {
  NUM:  'i64',
  SCL:  'double',
  TX:   'i8*',
  FACT: 'i1',
};

function llvmType(plType) {
  return LLVM_TYPE[plType] || null; // null = unsupported (LIST/MAP/INSTANCE/VEIN)
}

const RESERVED = new Set([
  'main', 'printf', 'malloc', 'free', 'realloc', 'strlen', 'strcpy',
  'strcat', 'strcmp', 'sprintf', 'snprintf', 'pow', 'llvm',
]);

function safeName(name) {
  return RESERVED.has(name) ? `pl_${name}` : name;
}

// ── Module-level state ───────────────────────────────────────────────────────
class Module {
  constructor() {
    this.regCounter = 0;
    this.strCounter = 0;
    this.blockCounter = 0;
    this.globals = [];
    this.body = [];
    this.errors = [];
    this.usesMath = false;
    this.scope = new Map();
  }

  freshReg() { return `%r${this.regCounter++}`; }
  freshLabel(prefix) { return `${prefix}${this.blockCounter++}`; }

  emit(instr) { this.body.push('  ' + instr); }
  emitLabel(label) { this.body.push(`${label}:`); }

  error(message, node) {
    this.errors.push(new CodegenError(message, node && node.line, node && node.column));
  }

  unsupported(node, label) {
    this.error(
      `Unsupported construct for LLVM code generation: ${label || node.type}. ` +
      `This works in "chloroplast run" but cannot yet be compiled to native code.`,
      node
    );
  }

  addStringConstant(value) {
    const name = `@.str.${this.strCounter++}`;
    const escaped = llvmEscapeString(value);
    const len = Buffer.byteLength(value, 'utf8') + 1;
    this.globals.push(`${name} = private unnamed_addr constant [${len} x i8] c"${escaped}\\00"`);
    return { name, len };
  }
}

function llvmEscapeString(s) {
  let out = '';
  const bytes = Buffer.from(s, 'utf8');
  for (const b of bytes) {
    if (b === 0x5c) { out += '\\5C'; }
    else if (b === 0x22) { out += '\\22'; }
    else if (b >= 0x20 && b < 0x7f) { out += String.fromCharCode(b); }
    else { out += '\\' + b.toString(16).padStart(2, '0').toUpperCase(); }
  }
  return out;
}

// ── Expression translator ───────────────────────────────────────────────────
class ExprCompiler {
  constructor(mod, node) {
    this.mod = mod;
    this.node = node;
  }

  compileExpr(exprStr) {
    const tokens = tokenizeExpr(exprStr);
    const { value } = this.parseOr(tokens, 0);
    return value;
  }

  compileCond(exprStr) {
    const between = exprStr.match(/^(.+?)\s+BETWEEN\s+(-?[\d.]+)\s+(-?[\d.]+)$/i);
    if (between) {
      const [, lhsRaw, lo, hi] = between;
      const lhsVal = this.compileExpr(lhsRaw.trim());
      const loReg = this.emitCompare(lhsVal, { reg: lo, type: 'NUM' }, '>=');
      const hiReg = this.emitCompare(lhsVal, { reg: hi, type: 'NUM' }, '<=');
      const andReg = this.mod.freshReg();
      this.mod.emit(`${andReg} = and i1 ${loReg}, ${hiReg}`);
      return andReg;
    }
    const val = this.compileExpr(exprStr);
    if (val.type !== 'FACT') return this.coerceToBool(val);
    return val.reg;
  }

  coerceToBool(val) {
    const reg = this.mod.freshReg();
    if (val.type === 'NUM') {
      this.mod.emit(`${reg} = icmp ne i64 ${val.reg}, 0`);
    } else if (val.type === 'SCL') {
      this.mod.emit(`${reg} = fcmp one double ${val.reg}, 0.0`);
    } else {
      this.mod.error(`Cannot use a value of type ${val.type} as a condition`, this.node);
      return '0';
    }
    return reg;
  }

  parseOr(tokens, pos) {
    let { value: left, pos: p } = this.parseAnd(tokens, pos);
    while (tokens[p] && tokens[p].type === 'OR') {
      p++;
      const { value: right, pos: p2 } = this.parseAnd(tokens, p);
      const lb = this.ensureBool(left);
      const rb = this.ensureBool(right);
      const reg = this.mod.freshReg();
      this.mod.emit(`${reg} = or i1 ${lb}, ${rb}`);
      left = { reg, type: 'FACT' };
      p = p2;
    }
    return { value: left, pos: p };
  }

  parseAnd(tokens, pos) {
    let { value: left, pos: p } = this.parseNot(tokens, pos);
    while (tokens[p] && tokens[p].type === 'AND') {
      p++;
      const { value: right, pos: p2 } = this.parseNot(tokens, p);
      const lb = this.ensureBool(left);
      const rb = this.ensureBool(right);
      const reg = this.mod.freshReg();
      this.mod.emit(`${reg} = and i1 ${lb}, ${rb}`);
      left = { reg, type: 'FACT' };
      p = p2;
    }
    return { value: left, pos: p };
  }

  parseNot(tokens, pos) {
    if (tokens[pos] && tokens[pos].type === 'NOT') {
      const { value, pos: p2 } = this.parseNot(tokens, pos + 1);
      const b = this.ensureBool(value);
      const reg = this.mod.freshReg();
      this.mod.emit(`${reg} = xor i1 ${b}, true`);
      return { value: { reg, type: 'FACT' }, pos: p2 };
    }
    return this.parseComparison(tokens, pos);
  }

  parseComparison(tokens, pos) {
    const { value: left, pos: p } = this.parseAdditive(tokens, pos);
    const cmpToken = tokens[p];
    if (cmpToken && cmpToken.type === 'CMP') {
      const { value: right, pos: p2 } = this.parseAdditive(tokens, p + 1);
      const reg = this.emitCompare(left, right, cmpToken.op);
      return { value: { reg, type: 'FACT' }, pos: p2 };
    }
    return { value: left, pos: p };
  }

  parseAdditive(tokens, pos) {
    let { value: left, pos: p } = this.parseMultiplicative(tokens, pos);
    while (tokens[p] && (tokens[p].type === 'PLUS' || tokens[p].type === 'MINUS')) {
      const op = tokens[p].type;
      p++;
      const { value: right, pos: p2 } = this.parseMultiplicative(tokens, p);
      left = op === 'PLUS' ? this.emitAdd(left, right) : this.emitSub(left, right);
      p = p2;
    }
    return { value: left, pos: p };
  }

  parseMultiplicative(tokens, pos) {
    let { value: left, pos: p } = this.parseUnary(tokens, pos);
    while (tokens[p] && ['STAR', 'SLASH', 'PERCENT', 'POW'].includes(tokens[p].type)) {
      const op = tokens[p].type;
      p++;
      const { value: right, pos: p2 } = this.parseUnary(tokens, p);
      if (op === 'STAR') left = this.emitMul(left, right);
      else if (op === 'SLASH') left = this.emitDiv(left, right);
      else if (op === 'PERCENT') left = this.emitMod(left, right);
      else if (op === 'POW') left = this.emitPow(left, right);
      p = p2;
    }
    return { value: left, pos: p };
  }

  parseUnary(tokens, pos) {
    if (tokens[pos] && tokens[pos].type === 'MINUS') {
      const { value, pos: p2 } = this.parseUnary(tokens, pos + 1);
      return { value: this.emitNeg(value), pos: p2 };
    }
    return this.parseAtom(tokens, pos);
  }

  parseAtom(tokens, pos) {
    const t = tokens[pos];
    if (!t) { this.mod.error('Unexpected end of expression', this.node); return { value: { reg: '0', type: 'NUM' }, pos }; }

    if (t.type === 'LPAREN') {
      const { value, pos: p2 } = this.parseOr(tokens, pos + 1);
      if (tokens[p2] && tokens[p2].type === 'RPAREN') return { value, pos: p2 + 1 };
      this.mod.error('Missing closing ")" in expression', this.node);
      return { value, pos: p2 };
    }
    if (t.type === 'NUMBER') {
      const isFloat = t.value.includes('.');
      return { value: { reg: t.value, type: isFloat ? 'SCL' : 'NUM' }, pos: pos + 1 };
    }
    if (t.type === 'STRING') {
      const { name, len } = this.mod.addStringConstant(t.value);
      const reg = this.mod.freshReg();
      this.mod.emit(`${reg} = getelementptr inbounds [${len} x i8], [${len} x i8]* ${name}, i64 0, i64 0`);
      return { value: { reg, type: 'TX' }, pos: pos + 1 };
    }
    if (t.type === 'BOOL') {
      return { value: { reg: t.value === 'TRUE' ? '1' : '0', type: 'FACT' }, pos: pos + 1 };
    }
    if (t.type === 'IDENT') {
      const info = this.mod.scope.get(t.value);
      if (!info) {
        this.mod.error(`"${t.value}" was not declared`, this.node);
        return { value: { reg: '0', type: 'NUM' }, pos: pos + 1 };
      }
      const reg = this.mod.freshReg();
      const lt = llvmType(info.plType);
      this.mod.emit(`${reg} = load ${lt}, ${lt}* ${info.ptr}`);
      return { value: { reg, type: info.plType }, pos: pos + 1 };
    }

    this.mod.error(`Unexpected token in expression: "${t.raw}"`, this.node);
    return { value: { reg: '0', type: 'NUM' }, pos: pos + 1 };
  }

  ensureBool(val) {
    if (val.type === 'FACT') return val.reg;
    return this.coerceToBool(val);
  }

  promote(left, right) {
    if (left.type === 'SCL' || right.type === 'SCL') {
      const l = left.type === 'NUM' ? this.intToDouble(left.reg) : left.reg;
      const r = right.type === 'NUM' ? this.intToDouble(right.reg) : right.reg;
      return { l, r, type: 'SCL' };
    }
    return { l: left.reg, r: right.reg, type: 'NUM' };
  }

  intToDouble(reg) {
    const out = this.mod.freshReg();
    this.mod.emit(`${out} = sitofp i64 ${reg} to double`);
    return out;
  }

  emitAdd(left, right) {
    if (left.type === 'TX' || right.type === 'TX') return this.emitStringConcat(left, right);
    const { l, r, type } = this.promote(left, right);
    const reg = this.mod.freshReg();
    this.mod.emit(type === 'SCL' ? `${reg} = fadd double ${l}, ${r}` : `${reg} = add i64 ${l}, ${r}`);
    return { reg, type };
  }
  emitSub(left, right) {
    const { l, r, type } = this.promote(left, right);
    const reg = this.mod.freshReg();
    this.mod.emit(type === 'SCL' ? `${reg} = fsub double ${l}, ${r}` : `${reg} = sub i64 ${l}, ${r}`);
    return { reg, type };
  }
  emitMul(left, right) {
    const { l, r, type } = this.promote(left, right);
    const reg = this.mod.freshReg();
    this.mod.emit(type === 'SCL' ? `${reg} = fmul double ${l}, ${r}` : `${reg} = mul i64 ${l}, ${r}`);
    return { reg, type };
  }
  emitDiv(left, right) {
    const l = left.type === 'NUM' ? this.intToDouble(left.reg) : left.reg;
    const r = right.type === 'NUM' ? this.intToDouble(right.reg) : right.reg;
    const reg = this.mod.freshReg();
    this.mod.emit(`${reg} = fdiv double ${l}, ${r}`);
    return { reg, type: 'SCL' };
  }
  emitMod(left, right) {
    if (left.type !== 'NUM' || right.type !== 'NUM') {
      this.mod.error('The % operator requires two NUM operands', this.node);
    }
    const reg = this.mod.freshReg();
    this.mod.emit(`${reg} = srem i64 ${left.reg}, ${right.reg}`);
    return { reg, type: 'NUM' };
  }
  emitPow(left, right) {
    this.mod.usesMath = true;
    const lf = left.type === 'NUM' ? this.intToDouble(left.reg) : left.reg;
    const rf = right.type === 'NUM' ? this.intToDouble(right.reg) : right.reg;
    const reg = this.mod.freshReg();
    this.mod.emit(`${reg} = call double @pow(double ${lf}, double ${rf})`);
    if (left.type === 'NUM' && right.type === 'NUM') {
      const iReg = this.mod.freshReg();
      this.mod.emit(`${iReg} = fptosi double ${reg} to i64`);
      return { reg: iReg, type: 'NUM' };
    }
    return { reg, type: 'SCL' };
  }
  emitNeg(val) {
    const reg = this.mod.freshReg();
    if (val.type === 'SCL') this.mod.emit(`${reg} = fneg double ${val.reg}`);
    else this.mod.emit(`${reg} = sub i64 0, ${val.reg}`);
    return { reg, type: val.type };
  }

  emitCompare(left, right, op) {
    if (left.type === 'TX' || right.type === 'TX') {
      if (op !== '==' && op !== '!=') {
        this.mod.error(`Cannot use ${op} on TX values — only IS / IS NOT supported for text`, this.node);
      }
      const cmpReg = this.mod.freshReg();
      this.mod.emit(`${cmpReg} = call i32 @strcmp(i8* ${left.reg}, i8* ${right.reg})`);
      const reg = this.mod.freshReg();
      this.mod.emit(`${reg} = icmp ${op === '==' ? 'eq' : 'ne'} i32 ${cmpReg}, 0`);
      return reg;
    }
    const { l, r, type } = this.promote(left, right);
    const reg = this.mod.freshReg();
    const iop = { '==': 'eq', '!=': 'ne', '>': 'sgt', '<': 'slt', '>=': 'sge', '<=': 'sle' }[op];
    const fop = { '==': 'oeq', '!=': 'one', '>': 'ogt', '<': 'olt', '>=': 'oge', '<=': 'ole' }[op];
    this.mod.emit(type === 'SCL' ? `${reg} = fcmp ${fop} double ${l}, ${r}` : `${reg} = icmp ${iop} i64 ${l}, ${r}`);
    return reg;
  }

  emitStringConcat(left, right) {
    const leftStr = this.toTXValue(left);
    const rightStr = this.toTXValue(right);

    const lenL = this.mod.freshReg();
    this.mod.emit(`${lenL} = call i64 @strlen(i8* ${leftStr})`);
    const lenR = this.mod.freshReg();
    this.mod.emit(`${lenR} = call i64 @strlen(i8* ${rightStr})`);
    const total = this.mod.freshReg();
    this.mod.emit(`${total} = add i64 ${lenL}, ${lenR}`);
    const totalPlus1 = this.mod.freshReg();
    this.mod.emit(`${totalPlus1} = add i64 ${total}, 1`);
    const buf = this.mod.freshReg();
    this.mod.emit(`${buf} = call i8* @malloc(i64 ${totalPlus1})`);
    this.mod.emit(`call i8* @strcpy(i8* ${buf}, i8* ${leftStr})`);
    this.mod.emit(`call i8* @strcat(i8* ${buf}, i8* ${rightStr})`);
    return { reg: buf, type: 'TX' };
  }

  toTXValue(val) {
    if (val.type === 'TX') return val.reg;
    this.mod.usesMath = val.type === 'SCL' || this.mod.usesMath;
    const buf = this.mod.freshReg();
    this.mod.emit(`${buf} = call i8* @malloc(i64 64)`);
    if (val.type === 'NUM') {
      const { name, len } = this.mod.addStringConstant('%ld');
      const fmt = this.mod.freshReg();
      this.mod.emit(`${fmt} = getelementptr inbounds [${len} x i8], [${len} x i8]* ${name}, i64 0, i64 0`);
      this.mod.emit(`call i32 (i8*, i8*, ...) @sprintf(i8* ${buf}, i8* ${fmt}, i64 ${val.reg})`);
    } else if (val.type === 'SCL') {
      const { name, len } = this.mod.addStringConstant('%.15g');
      const fmt = this.mod.freshReg();
      this.mod.emit(`${fmt} = getelementptr inbounds [${len} x i8], [${len} x i8]* ${name}, i64 0, i64 0`);
      this.mod.emit(`call i32 (i8*, i8*, ...) @sprintf(i8* ${buf}, i8* ${fmt}, double ${val.reg})`);
    } else if (val.type === 'FACT') {
      const { name: tName, len: tLen } = this.mod.addStringConstant('true');
      const { name: fName, len: fLen } = this.mod.addStringConstant('false');
      const tPtr = this.mod.freshReg();
      this.mod.emit(`${tPtr} = getelementptr inbounds [${tLen} x i8], [${tLen} x i8]* ${tName}, i64 0, i64 0`);
      const fPtr = this.mod.freshReg();
      this.mod.emit(`${fPtr} = getelementptr inbounds [${fLen} x i8], [${fLen} x i8]* ${fName}, i64 0, i64 0`);
      const chosen = this.mod.freshReg();
      this.mod.emit(`${chosen} = select i1 ${val.reg}, i8* ${tPtr}, i8* ${fPtr}`);
      return chosen;
    } else {
      this.mod.error(`Cannot convert type ${val.type} to text for concatenation`, this.node);
    }
    return buf;
  }
}

// ── Tokenizer for the small expression sub-language ─────────────────────────
function tokenizeExpr(str) {
  const tokens = [];
  let s = String(str).trim();
  const KEYWORD_OPS = [
    [/^GREATER THAN OR EQUAL\b/i, { type: 'CMP', op: '>=' }],
    [/^LESS THAN OR EQUAL\b/i,    { type: 'CMP', op: '<=' }],
    [/^GREATER THAN\b/i,          { type: 'CMP', op: '>' }],
    [/^LESS THAN\b/i,             { type: 'CMP', op: '<' }],
    [/^IS NOT\b/i,                { type: 'CMP', op: '!=' }],
    [/^IS\b/i,                    { type: 'CMP', op: '==' }],
    [/^AND\b/i,                   { type: 'AND' }],
    [/^OR\b/i,                    { type: 'OR' }],
    [/^NOT\b/i,                   { type: 'NOT' }],
    [/^TRUE\b/,                   { type: 'BOOL', value: 'TRUE' }],
    [/^FALSE\b/,                  { type: 'BOOL', value: 'FALSE' }],
  ];

  while (s.length > 0) {
    s = s.replace(/^\s+/, '');
    if (s.length === 0) break;

    if (s[0] === '"') {
      const m = s.match(/^"([^"]*)"/);
      if (m) { tokens.push({ type: 'STRING', value: m[1], raw: m[0] }); s = s.slice(m[0].length); continue; }
    }

    let matched = false;
    for (const [re, tok] of KEYWORD_OPS) {
      const m = s.match(re);
      if (m) { tokens.push({ ...tok, raw: m[0] }); s = s.slice(m[0].length); matched = true; break; }
    }
    if (matched) continue;

    const numMatch = s.match(/^-?\d+(\.\d+)?/);
    if (numMatch && (tokens.length === 0 || ['CMP','AND','OR','NOT','PLUS','MINUS','STAR','SLASH','PERCENT','POW','LPAREN'].includes(tokens[tokens.length-1].type))) {
      tokens.push({ type: 'NUMBER', value: numMatch[0], raw: numMatch[0] });
      s = s.slice(numMatch[0].length);
      continue;
    }

    if (s.startsWith('**')) { tokens.push({ type: 'POW', raw: '**' }); s = s.slice(2); continue; }
    if (s[0] === '+') { tokens.push({ type: 'PLUS', raw: '+' }); s = s.slice(1); continue; }
    if (s[0] === '-') { tokens.push({ type: 'MINUS', raw: '-' }); s = s.slice(1); continue; }
    if (s[0] === '*') { tokens.push({ type: 'STAR', raw: '*' }); s = s.slice(1); continue; }
    if (s[0] === '/') { tokens.push({ type: 'SLASH', raw: '/' }); s = s.slice(1); continue; }
    if (s[0] === '%') { tokens.push({ type: 'PERCENT', raw: '%' }); s = s.slice(1); continue; }
    if (s[0] === '(') { tokens.push({ type: 'LPAREN', raw: '(' }); s = s.slice(1); continue; }
    if (s[0] === ')') { tokens.push({ type: 'RPAREN', raw: ')' }); s = s.slice(1); continue; }

    const identMatch = s.match(/^[a-zA-Z_][a-zA-Z0-9_]*/);
    if (identMatch) {
      const word = identMatch[0];
      tokens.push({ type: 'IDENT', value: word, raw: word });
      s = s.slice(word.length);
      continue;
    }

    tokens.push({ type: 'UNKNOWN', raw: s[0] });
    s = s.slice(1);
  }
  return tokens;
}

// ── Main generator ──────────────────────────────────────────────────────────
class LLVMGenerator {
  constructor() {
    this.mod = new Module();
  }

  generate(programNode) {
    for (const node of (programNode.statements || [])) {
      this.genStatement(node);
    }
    return this.assemble();
  }

  assemble() {
    const m = this.mod;
    const lines = [];

    lines.push('; Generated by PlantLang LLVM Code Generator v1.0.0');
    lines.push('target triple = "x86_64-pc-linux-gnu"');
    lines.push('');

    lines.push('declare i32 @printf(i8*, ...)');
    lines.push('declare i32 @sprintf(i8*, i8*, ...)');
    lines.push('declare i8* @malloc(i64)');
    lines.push('declare void @free(i8*)');
    lines.push('declare i64 @strlen(i8*)');
    lines.push('declare i8* @strcpy(i8*, i8*)');
    lines.push('declare i8* @strcat(i8*, i8*)');
    lines.push('declare i32 @strcmp(i8*, i8*)');
    if (m.usesMath) lines.push('declare double @pow(double, double)');
    lines.push('');

    for (const g of m.globals) lines.push(g);
    if (m.globals.length) lines.push('');

    lines.push('define i32 @main() {');
    lines.push('entry:');
    for (const line of m.body) lines.push(line);
    lines.push('  ret i32 0');
    lines.push('}');
    lines.push('');

    return { ir: lines.join('\n'), errors: m.errors };
  }

  genStatement(node) {
    if (!node || !node.type) return;
    const m = this.mod;
    switch (node.type) {
      case 'MissionStatement':
      case 'PlantStatement':
        return;

      case 'CreateStatement': return this.genCreate(node);
      case 'SetStatement':     return this.genSet(node);
      case 'IncreaseStatement':return this.genIncDec(node, '+');
      case 'DecreaseStatement':return this.genIncDec(node, '-');
      case 'ShowStatement':    return this.genShow(node);
      case 'IfStatement':      return this.genIf(node);
      case 'CycleStatement':   return this.genCycle(node);
      case 'SeasonStatement':  return this.genSeason(node);
      case 'LockStatement':    return;

      case 'RawStatement':
        if (node.text && node.text.trim() && !/^\\+$/.test(node.text.trim())) {
          m.unsupported(node, `"${node.text.trim().slice(0, 40)}"`);
        }
        return;

      default:
        m.unsupported(node);
        return;
    }
  }

  genCreate(node) {
    const m = this.mod;
    const lt = llvmType(node.varType);
    if (!lt) { m.unsupported(node, `CREATE with type ${node.varType}`); return; }

    const name = safeName(node.identifier);
    const ptr = `%${name}.addr`;
    m.emit(`${ptr} = alloca ${lt}`);

    const ec = new ExprCompiler(m, node);
    let val;
    const ve = node.valueExpr;
    if (ve && ve.type === 'Literal') {
      if (ve.literalType === 'STRING') {
        const { name: gname, len } = m.addStringConstant(ve.value);
        const reg = m.freshReg();
        m.emit(`${reg} = getelementptr inbounds [${len} x i8], [${len} x i8]* ${gname}, i64 0, i64 0`);
        val = { reg, type: 'TX' };
      } else if (ve.literalType === 'NUMBER') {
        val = { reg: String(ve.value), type: Number.isInteger(ve.value) ? 'NUM' : 'SCL' };
      } else if (ve.literalType === 'BOOLEAN') {
        val = { reg: ve.value ? '1' : '0', type: 'FACT' };
      } else if (ve.literalType === 'RAW_EXPR') {
        val = ec.compileExpr(ve.value);
      } else {
        val = { reg: '0', type: node.varType };
      }
    } else if (ve && ve.type === 'Identifier') {
      // e.g. "CREATE countdown(NUM) TO i." where the parser produced a
      // plain Identifier node (single bare identifier) instead of wrapping
      // it in a RAW_EXPR Literal — must still resolve it via the variable
      // scope, exactly like the RAW_EXPR/compileExpr path does.
      val = ec.compileExpr(ve.name || ve.identifier || ve.value);
    } else if (typeof ve === 'string') {
      val = ec.compileExpr(ve);
    } else {
      val = { reg: node.varType === 'TX' ? 'null' : '0', type: node.varType };
    }

    const coerced = this.coerce(val, node.varType, node);
    m.emit(`store ${lt} ${coerced}, ${lt}* ${ptr}`);
    m.scope.set(node.identifier, { ptr, plType: node.varType });
  }

  coerce(val, declaredType, node) {
    if (val.type === declaredType) return val.reg;
    if (declaredType === 'SCL' && val.type === 'NUM') {
      const reg = this.mod.freshReg();
      this.mod.emit(`${reg} = sitofp i64 ${val.reg} to double`);
      return reg;
    }
    if (declaredType === 'NUM' && val.type === 'SCL') {
      const reg = this.mod.freshReg();
      this.mod.emit(`${reg} = fptosi double ${val.reg} to i64`);
      return reg;
    }
    this.mod.error(`Type mismatch: declared ${declaredType} but value is ${val.type}`, node);
    return declaredType === 'TX' ? 'null' : '0';
  }

  genSet(node) {
    const m = this.mod;
    const info = m.scope.get(node.identifier);
    if (!info) { m.error(`SET: "${node.identifier}" was not declared with CREATE`, node); return; }
    const ec = new ExprCompiler(m, node);
    const val = ec.compileExpr(node.valueExpr);
    const coerced = this.coerce(val, info.plType, node);
    const lt = llvmType(info.plType);
    m.emit(`store ${lt} ${coerced}, ${lt}* ${info.ptr}`);
  }

  genIncDec(node, sign) {
    const m = this.mod;
    const info = m.scope.get(node.identifier);
    if (!info) { m.error(`${sign === '+' ? 'INCREASE' : 'DECREASE'}: "${node.identifier}" was not declared`, node); return; }
    if (info.plType !== 'NUM' && info.plType !== 'SCL') {
      m.error(`${sign === '+' ? 'INCREASE' : 'DECREASE'}: "${node.identifier}" is ${info.plType} — only NUM/SCL supported`, node);
      return;
    }
    const lt = llvmType(info.plType);
    const cur = m.freshReg();
    m.emit(`${cur} = load ${lt}, ${lt}* ${info.ptr}`);
    const ec = new ExprCompiler(m, node);
    const amount = ec.compileExpr(node.amountExpr);
    const amountCoerced = this.coerce(amount, info.plType, node);
    const result = m.freshReg();
    if (info.plType === 'SCL') {
      m.emit(`${result} = f${sign === '+' ? 'add' : 'sub'} double ${cur}, ${amountCoerced}`);
    } else {
      m.emit(`${result} = ${sign === '+' ? 'add' : 'sub'} i64 ${cur}, ${amountCoerced}`);
    }
    m.emit(`store ${lt} ${result}, ${lt}* ${info.ptr}`);
  }

  genShow(node) {
    const m = this.mod;
    const expr = node.expr;
    if (!expr) { this.emitPrintNewlineOnly(); return; }

    if (expr.type === 'Literal' && expr.literalType === 'STRING') {
      this.emitPrintString(expr.value);
      return;
    }
    if (expr.type === 'Identifier') {
      const name = expr.name || expr.identifier || expr.value;
      const info = m.scope.get(name);
      if (!info) { m.error(`SHOW: "${name}" was not declared`, node); return; }
      const lt = llvmType(info.plType);
      const reg = m.freshReg();
      m.emit(`${reg} = load ${lt}, ${lt}* ${info.ptr}`);
      this.emitPrintValue({ reg, type: info.plType });
      return;
    }
    if (expr.type === 'Literal' && expr.literalType === 'RAW_EXPR') {
      const ec = new ExprCompiler(m, node);
      const val = ec.compileExpr(expr.value);
      this.emitPrintValue(val);
      return;
    }
    m.unsupported(node, 'SHOW with this expression form');
  }

  emitPrintNewlineOnly() {
    const { name, len } = this.mod.addStringConstant('\n');
    const ptr = this.mod.freshReg();
    this.mod.emit(`${ptr} = getelementptr inbounds [${len} x i8], [${len} x i8]* ${name}, i64 0, i64 0`);
    this.mod.emit(`call i32 (i8*, ...) @printf(i8* ${ptr})`);
  }

  emitPrintString(str) {
    const { name, len } = this.mod.addStringConstant(str + '\n');
    const fmtName2 = this.mod.addStringConstant('%s');
    const ptr = this.mod.freshReg();
    this.mod.emit(`${ptr} = getelementptr inbounds [${len} x i8], [${len} x i8]* ${name}, i64 0, i64 0`);
    const fmtPtr = this.mod.freshReg();
    this.mod.emit(`${fmtPtr} = getelementptr inbounds [${fmtName2.len} x i8], [${fmtName2.len} x i8]* ${fmtName2.name}, i64 0, i64 0`);
    this.mod.emit(`call i32 (i8*, ...) @printf(i8* ${fmtPtr}, i8* ${ptr})`);
  }

  emitPrintValue(val) {
    const m = this.mod;
    let fmtStr, castVal = val.reg;
    if (val.type === 'TX') { fmtStr = '%s\n'; }
    else if (val.type === 'NUM') { fmtStr = '%ld\n'; }
    else if (val.type === 'SCL') { fmtStr = '%.15g\n'; m.usesMath = true; }
    else if (val.type === 'FACT') {
      const { name: tName, len: tLen } = m.addStringConstant('true');
      const { name: fName, len: fLen } = m.addStringConstant('false');
      const tPtr = m.freshReg();
      m.emit(`${tPtr} = getelementptr inbounds [${tLen} x i8], [${tLen} x i8]* ${tName}, i64 0, i64 0`);
      const fPtr = m.freshReg();
      m.emit(`${fPtr} = getelementptr inbounds [${fLen} x i8], [${fLen} x i8]* ${fName}, i64 0, i64 0`);
      const chosen = m.freshReg();
      m.emit(`${chosen} = select i1 ${val.reg}, i8* ${tPtr}, i8* ${fPtr}`);
      castVal = chosen;
      fmtStr = '%s\n';
    } else {
      m.error(`Cannot SHOW a value of type ${val.type}`, {});
      return;
    }

    const { name: fmtName, len: fmtLen } = m.addStringConstant(fmtStr);
    const fmtPtr = m.freshReg();
    m.emit(`${fmtPtr} = getelementptr inbounds [${fmtLen} x i8], [${fmtLen} x i8]* ${fmtName}, i64 0, i64 0`);
    const llt = val.type === 'SCL' ? 'double' : (val.type === 'FACT' ? 'i8*' : (val.type === 'TX' ? 'i8*' : 'i64'));
    m.emit(`call i32 (i8*, ...) @printf(i8* ${fmtPtr}, ${llt} ${castVal})`);
  }

  genIf(node) {
    const m = this.mod;
    const endLabel = m.freshLabel('if.end');
    const branches = node.branches || [];

    const bodyLabels = branches.map(() => m.freshLabel('if.then'));
    const condLabels  = branches.map(() => m.freshLabel('if.cond'));

    m.emit(`br label %${condLabels[0]}`);

    branches.forEach((branch, i) => {
      m.emitLabel(condLabels[i]);
      if (branch.cond === null) {
        m.emit(`br label %${bodyLabels[i]}`);
      } else {
        const ec = new ExprCompiler(m, node);
        const condReg = ec.compileCond(branch.cond);
        const nextLabel = condLabels[i + 1] || endLabel;
        m.emit(`br i1 ${condReg}, label %${bodyLabels[i]}, label %${nextLabel}`);
      }

      m.emitLabel(bodyLabels[i]);
      for (const stmt of (branch.bodyStatements || [])) this.genStatement(stmt);
      m.emit(`br label %${endLabel}`);
    });

    m.emitLabel(endLabel);
  }

  genCycle(node) {
    const m = this.mod;
    if (node.sourceExpr !== null) {
      m.unsupported(node, 'CYCLE ... IN list (LIST iteration not yet supported in LLVM codegen)');
      return;
    }

    const ec = new ExprCompiler(m, node);
    const fromVal = ec.compileExpr(node.fromExpr);
    const toVal = ec.compileExpr(node.toExpr);
    const stepVal = node.stepExpr ? ec.compileExpr(node.stepExpr) : { reg: '1', type: 'NUM' };

    const varName = safeName(node.iterVar);
    const ptr = `%${varName}.addr`;
    m.emit(`${ptr} = alloca i64`);
    m.emit(`store i64 ${this.coerce(fromVal, 'NUM', node)}, i64* ${ptr}`);
    m.scope.set(node.iterVar, { ptr, plType: 'NUM' });

    const condLabel = m.freshLabel('cycle.cond');
    const bodyLabel = m.freshLabel('cycle.body');
    const incLabel  = m.freshLabel('cycle.inc');
    const endLabel  = m.freshLabel('cycle.end');

    m.emit(`br label %${condLabel}`);
    m.emitLabel(condLabel);
    const cur = m.freshReg();
    m.emit(`${cur} = load i64, i64* ${ptr}`);
    const cmp = m.freshReg();
    m.emit(`${cmp} = icmp sle i64 ${cur}, ${this.coerce(toVal, 'NUM', node)}`);
    m.emit(`br i1 ${cmp}, label %${bodyLabel}, label %${endLabel}`);

    m.emitLabel(bodyLabel);
    for (const stmt of (node.bodyStatements || [])) this.genStatement(stmt);
    m.emit(`br label %${incLabel}`);

    m.emitLabel(incLabel);
    const curInc = m.freshReg();
    m.emit(`${curInc} = load i64, i64* ${ptr}`);
    const next = m.freshReg();
    m.emit(`${next} = add i64 ${curInc}, ${this.coerce(stepVal, 'NUM', node)}`);
    m.emit(`store i64 ${next}, i64* ${ptr}`);
    m.emit(`br label %${condLabel}`);

    m.emitLabel(endLabel);
  }

  genSeason(node) {
    const m = this.mod;
    const condLabel = m.freshLabel('season.cond');
    const bodyLabel = m.freshLabel('season.body');
    const endLabel  = m.freshLabel('season.end');

    m.emit(`br label %${condLabel}`);
    m.emitLabel(condLabel);
    const ec = new ExprCompiler(m, node);
    const condReg = ec.compileCond(node.condExpr);
    m.emit(`br i1 ${condReg}, label %${bodyLabel}, label %${endLabel}`);

    m.emitLabel(bodyLabel);
    for (const stmt of (node.bodyStatements || [])) this.genStatement(stmt);
    m.emit(`br label %${condLabel}`);

    m.emitLabel(endLabel);
  }
}

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * generate(programNode) → { ir, errors }
 *
 * Translates a parsed PlantLang AST into LLVM IR text (.ll format).
 * `ir` is always produced (even with errors, for partial inspection);
 * `errors` is an array of CodegenError — if non-empty, the IR references
 * unsupported constructs or has type errors and should not be lowered
 * to a binary.
 */
function generate(programNode) {
  const gen = new LLVMGenerator();
  return gen.generate(programNode);
}

module.exports = { generate, CodegenError, LLVMGenerator, tokenizeExpr };
