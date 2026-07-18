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
 *   LIST, MAP, SPECIES/BLOOM, WEATHER/SHELTER, MATCH, HARVEST,
 *   LISTEN BRANCH, VERIFY/SUITE, PULSE/WHENEVER, BRAID, TAP/ABSORB/INFUSE/SEAL
 * 
 * SUPPORTED in this pass: ACTION/REAP/GIVE (functions) — added in v0.22.1.
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
  TX:   '%fat_ptr',   // { i8* ptr, i64 len, i64 cap }
  FACT: 'i1',
};

function llvmType(plType) {
  return LLVM_TYPE[plType] || null; // null = unsupported (LIST/MAP/INSTANCE/VEIN)
}

// Fat pointer field accessors (expressed as types for extractvalue/insertvalue)
const FAT_PTR = { PTR: 0, LEN: 1, CAP: 2 };

const RESERVED = new Set([
  'main', 'printf', 'malloc', 'free', 'realloc', 'strlen', 'strcpy',
  'strcat', 'strcmp', 'sprintf', 'snprintf', 'pow', 'llvm',
]);

function safeName(name) {
  return RESERVED.has(name) ? `pl_${name}` : name;
}

// ── Module-level state ───────────────────────────────────────────────────────
function llvmTypeSize(lt) {
  if (lt === 'i64' || lt === 'double') return 8;
  if (lt === 'i32') return 4;
  if (lt === 'i16') return 2;
  if (lt === 'i8' || lt === 'i1') return 1;
  if (lt === 'i8*') return 8;
  if (lt === '%fat_ptr') return 24; // { i8*, i64, i64 } = 8+8+8
  return 8; // default pointer-sized
}

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
    // Arena state
    this.currentDepth = 0;
    this.usesArena = false;
    this.arenaDepthCap = 64;
    this.arenaSlotSize = 65536; // 64KB per arena
    // Weather / Shelter exception handling state
    this.weatherGlobalsEmitted = false;
  }

  /** Ensure the weather-error globals (@_weather_flag, etc.) are declared. */
  ensureWeatherGlobals() {
    if (this.weatherGlobalsEmitted) return;
    this.weatherGlobalsEmitted = true;
    this.globals.push(`@_weather_flag = global i1 false`);
    this.globals.push(`@_weather_type = global i64 0`);
    this.globals.push(`@_weather_msg = global i8* null`);
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

  // ── Arena allocation ─────────────────────────────────────────────────────
  // Allocates `size` bytes from the arena at the given depth.
  // Returns an LLVM register holding an i8* pointer.
  arenaAlloc(depth, size) {
    this.usesArena = true;
    const d = String(depth);
    const offPtr = this.freshReg();
    this.emit(`${offPtr} = getelementptr inbounds [${this.arenaDepthCap} x i64], [${this.arenaDepthCap} x i64]* @arena_offsets, i64 0, i64 ${d}`);
    const oldOff = this.freshReg();
    this.emit(`${oldOff} = load i64, i64* ${offPtr}`);
    // size can be a number (constant) or an object {reg, type}
    const sizeVal = (typeof size === 'object' && size !== null) ? size.reg : String(size);
    const newOff = this.freshReg();
    if (typeof size === 'object' && size !== null) {
      this.emit(`${newOff} = add i64 ${oldOff}, ${size.reg}`);
    } else {
      this.emit(`${newOff} = add i64 ${oldOff}, ${String(size)}`);
    }
    this.emit(`store i64 ${newOff}, i64* ${offPtr}`);
    const base = this.freshReg();
    this.emit(`${base} = getelementptr inbounds [${this.arenaDepthCap} x [${this.arenaSlotSize} x i8]], [${this.arenaDepthCap} x [${this.arenaSlotSize} x i8]]* @arena_memory, i64 0, i64 ${d}, i64 ${oldOff}`);
    return base; // i8*
  }

  // Allocate from the current depth's arena, automatically determining
  // the size from the PlantLang type. Returns an LLVM typed pointer.
  arenaAllocTyped(plType, depth) {
    const lt = llvmType(plType);
    if (!lt) return null;
    const size = llvmTypeSize(lt);
    const rawPtr = this.arenaAlloc(depth, size);
    const typedPtr = this.freshReg();
    this.emit(`${typedPtr} = bitcast i8* ${rawPtr} to ${lt}*`);
    return typedPtr;
  }

  // Reset the arena at the given depth (zero the offset).
  arenaResetDepth(depth) {
    this.usesArena = true;
    const d = String(depth);
    const offPtr = this.freshReg();
    this.emit(`${offPtr} = getelementptr inbounds [${this.arenaDepthCap} x i64], [${this.arenaDepthCap} x i64]* @arena_offsets, i64 0, i64 ${d}`);
    this.emit(`store i64 0, i64* ${offPtr}`);
  }

  // ── Fat Pointer helpers ────────────────────────────────────────────
  // Build a %fat_ptr struct from ptr (i8*), len (i64), cap (i64)
  buildFatPtr(ptrReg, lenVal, capVal) {
    const fp = this.freshReg();
    this.emit(`${fp} = insertvalue %fat_ptr undef, i8* ${ptrReg}, ${FAT_PTR.PTR}`);
    const fp2 = this.freshReg();
    this.emit(`${fp2} = insertvalue %fat_ptr ${fp}, i64 ${lenVal}, ${FAT_PTR.LEN}`);
    const fp3 = this.freshReg();
    this.emit(`${fp3} = insertvalue %fat_ptr ${fp2}, i64 ${capVal}, ${FAT_PTR.CAP}`);
    return fp3;
  }

  // Extract the i8* ptr from a %fat_ptr
  extractPtr(fpReg) {
    const reg = this.freshReg();
    this.emit(`${reg} = extractvalue %fat_ptr ${fpReg}, ${FAT_PTR.PTR}`);
    return reg;
  }

  // Extract the i64 len from a %fat_ptr
  extractLen(fpReg) {
    const reg = this.freshReg();
    this.emit(`${reg} = extractvalue %fat_ptr ${fpReg}, ${FAT_PTR.LEN}`);
    return reg;
  }

  // Extract the i64 cap from a %fat_ptr
  extractCap(fpReg) {
    const reg = this.freshReg();
    this.emit(`${reg} = extractvalue %fat_ptr ${fpReg}, ${FAT_PTR.CAP}`);
    return reg;
  }

  // Emit a bounds-check for index access: idx < len, else educational error
  // Returns a label name to continue at (for the ok-branch)
  emitBoundsCheck(ptrReg, lenReg, idxReg, node, context) {
    const okLabel = this.freshLabel('bounds.ok');
    const errLabel = this.freshLabel('bounds.err');
    const cmp = this.freshReg();
    // Use unsigned compare: idx < len (works for non-negative idx)
    this.emit(`${cmp} = icmp ult i64 ${idxReg}, ${lenReg}`);
    this.emit(`br i1 ${cmp}, label %${okLabel}, label %${errLabel}`);

    this.emitLabel(errLabel);
    this.ensureWeatherGlobals();
    const errMsg = this.addStringConstant(
      `${context}: index out of bounds — len is `);
    const idxMsg = this.addStringConstant(` but index was `);
    const msgGep = this.freshReg();
    this.emit(`${msgGep} = getelementptr inbounds [${errMsg.len} x i8], [${errMsg.len} x i8]* ${errMsg.name}, i64 0, i64 0`);
    // Build a human-readable error message using printf to a static buffer
    // For simplicity, set the weather message and type
    this.emit(`store i8* ${msgGep}, i8** @_weather_msg`);
    this.emit(`store i64 6, i64* @_weather_type`); // SEED_STORM type
    this.emit(`store i1 true, i1* @_weather_flag`);
    this.emit(`br label %${okLabel}`); // continue (but flag is set)

    this.emitLabel(okLabel);
    return okLabel;
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
  constructor(mod, node, generator) {
    this.mod = mod;
    this.node = node;
    this.generator = generator;
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
      const m = this.mod;
      const strVal = t.value;
      const byteLen = Buffer.byteLength(strVal, 'utf8');
      const cap = byteLen + 1;
      const { name: gname, len: constLen } = m.addStringConstant(strVal);
      const srcPtr = m.freshReg();
      m.emit(`${srcPtr} = getelementptr inbounds [${constLen} x i8], [${constLen} x i8]* ${gname}, i64 0, i64 0`);
      const bufPtr = m.arenaAlloc(m.currentDepth, cap);
      m.emit(`call i8* @strcpy(i8* ${bufPtr}, i8* ${srcPtr})`);
      const fpReg = m.buildFatPtr(bufPtr, String(byteLen), String(cap));
      return { value: { reg: fpReg, type: 'TX' }, pos: pos + 1 };
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
      const m = this.mod;
      const lt = llvmType(info.plType);
      const reg = m.freshReg();
      m.emit(`${reg} = load ${lt}, ${lt}* ${info.ptr}`);
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
    if (this.generator) this.generator.emitZeroCheck(r, this.node);
    const reg = this.mod.freshReg();
    this.mod.emit(`${reg} = fdiv double ${l}, ${r}`);
    return { reg, type: 'SCL' };
  }
  emitMod(left, right) {
    if (left.type !== 'NUM' || right.type !== 'NUM') {
      this.mod.error('The % operator requires two NUM operands', this.node);
    }
    if (this.generator) this.generator.emitZeroCheck(
      left.type === 'NUM' ? left.reg : this.intToDouble(left.reg),
      this.node
    );
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
      const m = this.mod;
      const leftPtr = left.type === 'TX' ? m.extractPtr(left.reg) : left.reg;
      const rightPtr = right.type === 'TX' ? m.extractPtr(right.reg) : right.reg;
      const cmpReg = m.freshReg();
      m.emit(`${cmpReg} = call i32 @strcmp(i8* ${leftPtr}, i8* ${rightPtr})`);
      const reg = m.freshReg();
      m.emit(`${reg} = icmp ${op === '==' ? 'eq' : 'ne'} i32 ${cmpReg}, 0`);
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
    const m = this.mod;
    // Get ptr and len for left operand
    let leftPtr, leftLen;
    if (left.type === 'TX') {
      leftPtr = m.extractPtr(left.reg);
      leftLen = m.extractLen(left.reg);
    } else {
      leftPtr = this.toTXValue(left);
      leftLen = m.freshReg();
      m.emit(`${leftLen} = call i64 @strlen(i8* ${leftPtr})`);
    }
    // Get ptr and len for right operand
    let rightPtr, rightLen;
    if (right.type === 'TX') {
      rightPtr = m.extractPtr(right.reg);
      rightLen = m.extractLen(right.reg);
    } else {
      rightPtr = this.toTXValue(right);
      rightLen = m.freshReg();
      m.emit(`${rightLen} = call i64 @strlen(i8* ${rightPtr})`);
    }

    const total = m.freshReg();
    m.emit(`${total} = add i64 ${leftLen}, ${rightLen}`);
    const cap = m.freshReg();
    m.emit(`${cap} = add i64 ${total}, 1`);
    // Allocate from arena (uses current module depth)
    const buf = m.arenaAlloc(m.currentDepth, { reg: cap, type: 'NUM' }); // FIXME
    m.emit(`call i8* @strcpy(i8* ${buf}, i8* ${leftPtr})`);
    m.emit(`call i8* @strcat(i8* ${buf}, i8* ${rightPtr})`);
    const fpReg = m.buildFatPtr(buf, total, cap);
    return { reg: fpReg, type: 'TX' };
  }

  toTXValue(val) {
    if (val.type === 'TX') return this.mod.extractPtr(val.reg);
    this.mod.usesMath = val.type === 'SCL' || this.mod.usesMath;
    const buf = this.mod.arenaAlloc(this.mod.currentDepth, 64);
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
    this.fnDefs = [];   // { name, params, ir: string[], llvmParamList: string[] }
    this.fnInfos = new Map(); // name -> { params, bodyStatements, line, column, isExternal }
    this.fnDeclares = []; // { name, params, llvmParamList } for extern FFI actions
    this.shelterStack = [];
    // Each entry: { unwindDepth, shelterClause, handlerLabel }
  }

  /** Look up the nearest active SHELTER that catches the given storm type. */
  getShelterForStorm(stormType) {
    for (let i = this.shelterStack.length - 1; i >= 0; i--) {
      const entry = this.shelterStack[i];
      if (entry.shelterClause.stormType === stormType ||
          entry.shelterClause.stormType === 'ANY_STORM') {
        return entry;
      }
    }
    return null;
  }

  /** Helper: emit a zero-divisor check that sets error globals and branches
   *  to the nearest matching ZERO_STORM SHELTER handler. Returns true if a
   *  shelter was found and the check was emitted, false if no shelter is active
   *  (division proceeds unchecked). */
  emitZeroCheck(divisorReg, nod) {
    const m = this.mod;
    const shelter = this.getShelterForStorm('ZERO_STORM');
    if (!shelter) return false;
    m.ensureWeatherGlobals();
    const errLabel = m.freshLabel('div.err');
    const okLabel  = m.freshLabel('div.ok');
    const isZero = m.freshReg();
    m.emit(`${isZero} = fcmp oeq double ${divisorReg}, 0.0`);
    m.emit(`br i1 ${isZero}, label %${errLabel}, label %${okLabel}`);
    // Error setup block — only reached when divisor is zero
    m.emitLabel(errLabel);
    const errMsg = m.addStringConstant('\u0642\u0633\u0645\u0629 \u0639\u0644\u0649 \u0635\u0641\u0631');
    const msgGep = m.freshReg();
    m.emit(`${msgGep} = getelementptr inbounds [${errMsg.len} x i8], [${errMsg.len} x i8]* ${errMsg.name}, i64 0, i64 0`);
    m.emit(`store i8* ${msgGep}, i8** @_weather_msg`);
    m.emit(`store i64 1, i64* @_weather_type`);
    m.emit(`store i1 true, i1* @_weather_flag`);
    m.emit(`br label %${shelter.handlerLabel}`);
    // Normal continuation
    m.emitLabel(okLabel);
    return true;
  }

  generate(programNode) {
    const m = this.mod;
    // First pass: collect ACTION declarations
    for (const node of (programNode.statements || [])) {
      if (node.type === 'ActionDeclaration') {
        this.fnInfos.set(node.name, {
          params: node.params,
          bodyStatements: node.bodyStatements,
          line: node.line,
          column: node.column,
          isExternal: !!node.isExternal,
        });
      }
    }
    // Generate function definitions (before main for forward references)
    for (const [name, info] of this.fnInfos) {
      this.genFnDef(name, info);
    }
    // Generate main body with depth tracking
    m.currentDepth = 0;
    for (const node of (programNode.statements || [])) {
      if (node.type === 'ActionDeclaration') continue;
      this.trackDepth(node);
      this.genStatement(node);
    }
    // Final arena reset at program exit
    for (let d = m.currentDepth; d >= 0; d--) {
      m.arenaResetDepth(d);
    }
    return this.assemble();
  }

  // ── Contract Law: Depth access validation ───────────────────────────────────
  // Article II: "No depth (N+1) can access data in depth (N) except through
  // 'contracting' (pre-allocation)."
  // Validates that the current depth is not deeper than the variable's
  // allocation depth — a deeper scope may not access a shallower scope's
  // variables without explicit contracting.
  checkDepthAccess(varName, varDepth, node, operation) {
    const m = this.mod;
    if (m.currentDepth > varDepth) {
      m.error(
        `═══ ⚠ Contract Violation: Unauthorized Access ═══\n` +
        `  Operation:  ${operation}\n` +
        `  Variable:   "${varName}"\n` +
        `  Declared at: depth ${varDepth}  (Arena_${varDepth})\n` +
        `  Accessed from: depth ${m.currentDepth}  (Arena_${m.currentDepth})\n` +
        `  Rule: "No depth (N+1) can access data in depth (N) except\n` +
        `         through 'contracting' (pre-allocation)."\n` +
        `  Fix: Promote the variable to the current arena using\n` +
        `       "${varDepth}\\\ ${varName} -> ${m.currentDepth} = ..."\n` +
        `       or declare a local copy at this depth.`,
        node
      );
    }
  }

  // ── Depth tracking ──────────────────────────────────────────────────────────
  // Before generating a statement, check if the depth changed and inject arena
  // resets for any depth levels that have been exited.
  trackDepth(node) {
    const m = this.mod;
    const nodeDepth = node.depth !== undefined ? node.depth : m.currentDepth;
    if (nodeDepth === m.currentDepth) return;

    if (nodeDepth > m.currentDepth) {
      // Entering a deeper scope — no reset needed, just update
      m.currentDepth = nodeDepth;
      return;
    }

    // Exiting depths: reset each arena from currentDepth down to nodeDepth+1
    for (let d = m.currentDepth; d > nodeDepth; d--) {
      m.arenaResetDepth(d);
    }
    m.currentDepth = nodeDepth;
  }

  // ── ACTION function definition / declaration ──────────────────────────────
  genFnDef(name, info) {
    const m = this.mod;

    const llvmParams = [];
    for (const p of info.params) {
      const lt = llvmType(p.type);
      if (!lt) {
        m.error(`Unsupported parameter type "${p.type}" in ACTION "${name}"`, info);
        continue;
      }
      llvmParams.push(`${lt} %${safeName(p.name)}`);
    }

    // FFI external actions → emit declare instead of define
    if (info.isExternal) {
      this.fnDeclares.push({ name, params: info.params, llvmParamList: llvmParams });
      return;
    }

    const savedBody = m.body;
    const savedScope = m.scope;
    const savedReg = m.regCounter;
    const savedBlock = m.blockCounter;
    // strCounter is NOT saved/restored — string globals are shared module-wide

    m.body = [];
    m.scope = new Map();
    m.regCounter = 0;
    m.blockCounter = 0;

    m.emit('entry:');
    m.currentDepth = 0;

    // Store each parameter from register to arena allocation (for mutability)
    for (const p of info.params) {
      const lt = llvmType(p.type);
      if (!lt) continue;
      const sName = safeName(p.name);
      const ptr = m.arenaAllocTyped(p.type, 0);
      if (!ptr) continue;
      m.emit(`store ${lt} %${sName}, ${lt}* ${ptr}`);
      m.scope.set(p.name, { ptr, plType: p.type, depth: 0 });
    }

    // Generate function body with depth tracking
    m.currentDepth = 0;
    let hasReturn = false;
    for (const stmt of (info.bodyStatements || [])) {
      this.trackDepth(stmt);
      if (this.genStatement(stmt)) hasReturn = true;
    }

    // Default return if no GIVE was emitted
    if (!hasReturn) {
      m.emit('ret i64 0');
    }

    const fnBody = m.body;

    // Restore module state
    m.body = savedBody;
    m.scope = savedScope;
    m.regCounter = savedReg;
    m.blockCounter = savedBlock;

    this.fnDefs.push({ name, params: info.params, ir: fnBody, llvmParamList: llvmParams });
  }

  // ── REAP (function call) ───────────────────────────────────────────────────
  genReapStatement(node) {
    const m = this.mod;
    const src = node.source;

    // Only ACTION kind is supported for LLVM compilation
    if (src.kind !== 'ACTION') {
      m.unsupported(node, `REAP from ${src.kind} source`);
      return;
    }

    const fnInfo = this.fnInfos.get(src.name);
    if (!fnInfo) {
      m.error(`REAP: ACTION "${src.name}" is not defined`, node);
      return;
    }

    // Compile arguments
    const ec = new ExprCompiler(m, node, this);
    const argVals = [];
    for (let i = 0; i < (node.args || []).length; i++) {
      const argExpr = node.args[i];
      // If we have a matching param type, compile the arg
      let argVal;
      try {
        argVal = ec.compileExpr(argExpr);
      } catch (e) {
        m.error(`Error compiling REAP argument "${argExpr}": ${e.message}`, node);
        return;
      }

      // Coerce argument to the declared parameter type
      if (i < fnInfo.params.length) {
        const paramType = fnInfo.params[i].type;
        argVal = { reg: this.coerce(argVal, paramType, node), type: paramType };
      }
      argVals.push(argVal);
    }

    // Build call arguments
    const callArgs = argVals.map((v, i) => {
      const pt = i < fnInfo.params.length ? fnInfo.params[i].type : v.type;
      const lt = llvmType(pt) || 'i64';
      return `${lt} ${v.reg}`;
    }).join(', ');

    const resultReg = m.freshReg();
    const fnName = safeName(src.name);
    m.emit(`${resultReg} = call i64 @${fnName}(${callArgs})`);

    // Store result in target variable (auto-create if not declared)
    if (node.variable === '_') return; // discard

    let targetInfo = m.scope.get(node.variable);
    if (!targetInfo) {
      // Auto-create the variable — REAP implicitly declares its target
      const depth = node.depth !== undefined ? node.depth : m.currentDepth;
      this.checkDepthAccess(node.variable, depth, node, 'REAP (auto-create)');
      const ptr = m.arenaAllocTyped('NUM', depth);
      m.emit(`store i64 0, i64* ${ptr}`);
      m.scope.set(node.variable, { ptr, plType: 'NUM', depth });
      targetInfo = m.scope.get(node.variable);
    }

    let storedReg;
    if (targetInfo.plType === 'SCL') {
      // The function returned i64 but the bits represent a double — use bitcast
      storedReg = m.freshReg();
      m.emit(`${storedReg} = bitcast i64 ${resultReg} to double`);
    } else {
      // For all other types, use normal coercion
      storedReg = this.coerce({ reg: resultReg, type: 'NUM' }, targetInfo.plType, node);
    }
    const lt = llvmType(targetInfo.plType);
    m.emit(`store ${lt} ${storedReg}, ${lt}* ${targetInfo.ptr}`);
  }

  // ── GIVE (return from ACTION) ──────────────────────────────────────────────
  genGiveStatement(node) {
    const m = this.mod;

    // 1. Compile the return value FIRST (before any arena cleanup)
    let val;
    if (typeof node.valueExpr === 'string') {
      const ec = new ExprCompiler(m, node, this);
      val = ec.compileExpr(node.valueExpr);
    } else {
      val = this.compileAstExpr(node.valueExpr);
    }

    // 2. Arena Unwinding: Forced Exit cleanup chain per Article IX.
    //    Reset all depth levels > 0 from deepest to shallowest, ensuring
    //    temporary variables in deeper arenas are freed before shallower ones.
    //    We skip depth 0 because function parameters live in Arena_0 and must
    //    survive for the caller — especially critical for recursive functions
    //    where each call frame shares the global arena state.
    for (let d = m.currentDepth; d >= 1; d--) {
      m.arenaResetDepth(d);
    }
    m.currentDepth = 0;

    // 3. Convert the value to i64 and return
    let returnReg;
    if (val.type === 'NUM') {
      returnReg = val.reg;
    } else if (val.type === 'SCL') {
      returnReg = m.freshReg();
      m.emit(`${returnReg} = bitcast double ${val.reg} to i64`);
    } else if (val.type === 'TX') {
      const ptrReg = m.extractPtr(val.reg);
      returnReg = m.freshReg();
      m.emit(`${returnReg} = ptrtoint i8* ${ptrReg} to i64`);
    } else if (val.type === 'FACT') {
      returnReg = m.freshReg();
      m.emit(`${returnReg} = zext i1 ${val.reg} to i64`);
    } else {
      m.error(`Cannot GIVE a value of type ${val.type}`, node);
      return;
    }
    m.emit(`ret i64 ${returnReg}`);
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
    // User FFI external declarations
    for (const fd of this.fnDeclares) {
      const llvmRetType = 'i64';
      lines.push(`declare ${llvmRetType} @${safeName(fd.name)}(${fd.llvmParamList.join(', ')})`);
    }
    lines.push('');

    // Arena globals (emitted lazily if any code uses arena allocation)
    if (m.usesArena) {
      lines.push(`@arena_offsets = global [${m.arenaDepthCap} x i64] zeroinitializer`);
      lines.push(`@arena_memory = global [${m.arenaDepthCap} x [${m.arenaSlotSize} x i8]] zeroinitializer`);
      lines.push('');
    }

    // Fat pointer struct type for dynamic data (TX, future arrays)
    lines.push('%fat_ptr = type { i8*, i64, i64 }');
    lines.push('');

    for (const g of m.globals) lines.push(g);
    if (m.globals.length) lines.push('');

    // Emit function definitions (before main for forward references)
    for (const fd of this.fnDefs) {
      const llvmRetType = 'i64';
      lines.push(`define ${llvmRetType} @${safeName(fd.name)}(${fd.llvmParamList.join(', ')}) {`);
      for (const line of fd.ir) lines.push(line);
      lines.push('}');
      lines.push('');
    }

    lines.push('define i32 @main() {');
    lines.push('entry:');
    for (const line of m.body) lines.push(line);
    lines.push('  ret i32 0');
    lines.push('}');
    lines.push('');

    return { ir: lines.join('\n'), errors: m.errors };
  }

  /** Compile any AST expression node to { reg, type } */
  compileAstExpr(node) {
    if (!node) return { reg: '0', type: 'NUM' };
    const m = this.mod;

    if (node.type === 'Identifier') {
      const info = m.scope.get(node.name);
      if (!info) { m.error(`"${node.name}" was not declared`, node); return { reg: '0', type: 'NUM' }; }
      const reg = m.freshReg();
      const lt = llvmType(info.plType);
      m.emit(`${reg} = load ${lt}, ${lt}* ${info.ptr}`);
      return { reg, type: info.plType };
    }

    if (node.type === 'Literal') {
      if (node.literalType === 'NUMBER') return { reg: String(node.value), type: 'NUM' };
      if (node.literalType === 'STRING') {
        const strVal = node.value;
        const byteLen = Buffer.byteLength(strVal, 'utf8');
        const cap = byteLen + 1;
        const { name: gname, len: constLen } = m.addStringConstant(strVal);
        const srcPtr = m.freshReg();
        m.emit(`${srcPtr} = getelementptr inbounds [${constLen} x i8], [${constLen} x i8]* ${gname}, i64 0, i64 0`);
        const bufPtr = m.arenaAlloc(m.currentDepth, cap);
        m.emit(`call i8* @strcpy(i8* ${bufPtr}, i8* ${srcPtr})`);
        const fpReg = m.buildFatPtr(bufPtr, String(byteLen), String(cap));
        return { reg: fpReg, type: 'TX' };
      }
      if (node.literalType === 'BOOLEAN') return { reg: node.value ? '1' : '0', type: 'FACT' };
      if (node.literalType === 'RAW_EXPR') {
        const ec = new ExprCompiler(m, node, this);
        return ec.compileExpr(node.value);
      }
      return { reg: '0', type: 'NUM' };
    }

    if (node.type === 'LenCall') {
      const arg = this.compileAstExpr(node.arg);
      if (arg.type !== 'TX') {
        m.error(`len() requires a TX argument, got ${arg.type}`, node);
        return { reg: '0', type: 'NUM' };
      }
      const lenReg = m.extractLen(arg.reg);
      return { reg: lenReg, type: 'NUM' };
    }

    if (node.type === 'CapCall') {
      const arg = this.compileAstExpr(node.arg);
      if (arg.type !== 'TX') {
        m.error(`cap() requires a TX argument, got ${arg.type}`, node);
        return { reg: '0', type: 'NUM' };
      }
      const capReg = m.extractCap(arg.reg);
      return { reg: capReg, type: 'NUM' };
    }

    if (node.type === 'IndexAccess') {
      const target = this.compileAstExpr(node.target);
      if (target.type !== 'TX') {
        m.error(`Index access requires a TX target, got ${target.type}`, node);
        return { reg: target.reg, type: 'TX' };
      }
      const idx = this.compileAstExpr(node.index);
      if (idx.type !== 'NUM') {
        m.error(`Index must be a NUM, got ${idx.type}`, node);
        return { reg: target.reg, type: 'TX' };
      }
      const ptr = m.extractPtr(target.reg);
      const lenReg = m.extractLen(target.reg);
      m.emitBoundsCheck(ptr, lenReg, idx.reg, node, 'TX index access');
      const charPtr = m.freshReg();
      m.emit(`${charPtr} = getelementptr inbounds i8, i8* ${ptr}, i64 ${idx.reg}`);
      const charVal = m.freshReg();
      m.emit(`${charVal} = load i8, i8* ${charPtr}`);
      const singleBuf = m.arenaAlloc(m.currentDepth, 2);
      m.emit(`store i8 ${charVal}, i8* ${singleBuf}`);
      const nullReg = m.freshReg();
      m.emit(`${nullReg} = getelementptr inbounds i8, i8* ${singleBuf}, i64 1`);
      m.emit(`store i8 0, i8* ${nullReg}`);
      const fpReg = m.buildFatPtr(singleBuf, '1', '2');
      return { reg: fpReg, type: 'TX' };
    }

    if (typeof node === 'string') {
      const ec = new ExprCompiler(m, node, this);
      return ec.compileExpr(node);
    }

    m.unsupported(node, `expression type ${node.type}`);
    return { reg: '0', type: 'NUM' };
  }

  genStatement(node) {
    if (!node || !node.type) return false;
    const m = this.mod;
    if (node.depth !== undefined) this.trackDepth(node);
    switch (node.type) {
      case 'MissionStatement':
      case 'PlantStatement':
      case 'ActionDeclaration':
        return false;

      case 'CreateStatement': this.genCreate(node); return false;
      case 'SetStatement':     this.genSet(node);   return false;
      case 'IncreaseStatement':this.genIncDec(node, '+'); return false;
      case 'DecreaseStatement':this.genIncDec(node, '-'); return false;
      case 'ShowStatement':    this.genShow(node);  return false;
      case 'IfStatement':      this.genIf(node);    return false;
      case 'CycleStatement':   this.genCycle(node); return false;
      case 'SeasonStatement':  this.genSeason(node);return false;
      case 'LockStatement':    return false;

      case 'WeatherStatement':
        this.genWeatherStatement(node);
        return false;

      case 'ReapStatement':
        this.genReapStatement(node);
        return false;

      case 'GiveStatement':
        this.genGiveStatement(node);
        return true; // ret is a terminator

      case 'RawStatement':
        if (node.text && node.text.trim() && !/^\\+$/.test(node.text.trim())) {
          m.unsupported(node, `"${node.text.trim().slice(0, 40)}"`);
        }
        return false;

      default:
        m.unsupported(node);
        return false;
    }
  }

  genCreate(node) {
    const m = this.mod;
    const lt = llvmType(node.varType);
    if (!lt) { m.unsupported(node, `CREATE with type ${node.varType}`); return; }

    const name = safeName(node.identifier);
    // Allocate from arena at the variable's depth (defaults to statement depth)
    const depth = node.depth !== undefined ? node.depth : m.currentDepth;
    // Validate Contract Law (Article III): destination depth must be ≤ current depth
    if (depth > m.currentDepth) {
      m.error(
        `═══ ⚠ Contract Violation: Illegal Destination ═══\n` +
        `  Operation:  CREATE\n` +
        `  Variable:   "${node.identifier}"\n` +
        `  Destination: depth ${depth}  (Arena_${depth})\n` +
        `  Current:     depth ${m.currentDepth}  (Arena_${m.currentDepth})\n` +
        `  Rule: "A seed is not allowed to reside in soil (Arena_M) deeper\n` +
        `         than the soil it was born in (Arena_N)."\n` +
        `  Fix: Use CREATE at depth ${m.currentDepth} instead, or specify\n` +
        `       a destination ≤ ${m.currentDepth}.`,
        node
      );
      return;
    }
    const ptr = m.arenaAllocTyped(node.varType, depth);

    const ec = new ExprCompiler(m, node, this);
    let val;
    const ve = node.valueExpr;
    if (ve && ve.type === 'Literal') {
      if (ve.literalType === 'STRING') {
        if (node.varType === 'TX') {
          // TX with string literal: build a fat pointer with arena-backed buffer
          const strVal = ve.value;
          const byteLen = Buffer.byteLength(strVal, 'utf8');
          const cap = byteLen + 1;
          const { name: gname, len: constLen } = m.addStringConstant(strVal);
          const srcPtr = m.freshReg();
          m.emit(`${srcPtr} = getelementptr inbounds [${constLen} x i8], [${constLen} x i8]* ${gname}, i64 0, i64 0`);
          const bufPtr = m.arenaAlloc(depth, cap);
          m.emit(`call i8* @strcpy(i8* ${bufPtr}, i8* ${srcPtr})`);
          const fpReg = m.buildFatPtr(bufPtr, String(byteLen), String(cap));
          val = { reg: fpReg, type: 'TX' };
        } else {
          const { name: gname, len } = m.addStringConstant(ve.value);
          const reg = m.freshReg();
          m.emit(`${reg} = getelementptr inbounds [${len} x i8], [${len} x i8]* ${gname}, i64 0, i64 0`);
          val = { reg, type: 'TX' };
        }
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
      val = ec.compileExpr(ve.name || ve.identifier || ve.value);
    } else if (typeof ve === 'string') {
      val = ec.compileExpr(ve);
    } else if (ve && (ve.type === 'LenCall' || ve.type === 'CapCall' || ve.type === 'IndexAccess')) {
      val = this.compileAstExpr(ve);
    } else if (node.varType === 'TX') {
      // Empty TX: zero-length fat pointer with a null-terminated empty string
      const emptyBuf = m.arenaAlloc(depth, 1);
      m.emit(`store i8 0, i8* ${emptyBuf}`);
      const fpReg = m.buildFatPtr(emptyBuf, '0', '1');
      val = { reg: fpReg, type: 'TX' };
    } else {
      val = { reg: '0', type: node.varType };
    }

    const coerced = this.coerce(val, node.varType, node);
    m.emit(`store ${lt} ${coerced}, ${lt}* ${ptr}`);
    m.scope.set(node.identifier, { ptr, plType: node.varType, depth });
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
    if (declaredType === 'NUM' && val.type === 'TX') {
      const ptr = this.mod.extractPtr(val.reg);
      const reg = this.mod.freshReg();
      this.mod.emit(`${reg} = ptrtoint i8* ${ptr} to i64`);
      return reg;
    }
    if (declaredType === 'TX' && val.type === 'NUM') {
      const m = this.mod;
      const ptrReg = m.freshReg();
      m.emit(`${ptrReg} = inttoptr i64 ${val.reg} to i8*`);
      // Build a minimal fat pointer: ptr = inttoptr, len = 0, cap = 0
      const fpReg = m.buildFatPtr(ptrReg, '0', '0');
      return fpReg;
    }
    if (declaredType === 'NUM' && val.type === 'FACT') {
      const reg = this.mod.freshReg();
      this.mod.emit(`${reg} = zext i1 ${val.reg} to i64`);
      return reg;
    }
    if (declaredType === 'FACT' && val.type === 'NUM') {
      const reg = this.mod.freshReg();
      this.mod.emit(`${reg} = trunc i64 ${val.reg} to i1`);
      return reg;
    }
    if (declaredType === 'TX' && val.type === 'FACT') {
      const m = this.mod;
      const reg = m.freshReg();
      m.emit(`${reg} = zext i1 ${val.reg} to i64`);
      const ptrReg = m.freshReg();
      m.emit(`${ptrReg} = inttoptr i64 ${reg} to i8*`);
      const fpReg = m.buildFatPtr(ptrReg, '0', '0');
      return fpReg;
    }
    if (declaredType === 'FACT' && val.type === 'TX') {
      const m = this.mod;
      const ptr = m.extractPtr(val.reg);
      const reg = m.freshReg();
      m.emit(`${reg} = ptrtoint i8* ${ptr} to i64`);
      const bool = m.freshReg();
      m.emit(`${bool} = icmp ne i64 ${reg}, 0`);
      return bool;
    }
    this.mod.error(`Type mismatch: declared ${declaredType} but value is ${val.type}`, node);
    if (declaredType === 'TX') {
      return `zeroinitializer`; // zero-initialized %fat_ptr
    }
    return '0';
  }

  genSet(node) {
    const m = this.mod;
    const info = m.scope.get(node.identifier);
    if (!info) { m.error(`SET: "${node.identifier}" was not declared with CREATE`, node); return; }
    let val;
    const ve = node.valueExpr;
    if (typeof ve === 'string') {
      const ec = new ExprCompiler(m, node, this);
      val = ec.compileExpr(ve);
    } else {
      val = this.compileAstExpr(ve);
    }
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
    const ec = new ExprCompiler(m, node, this);
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
      const ec = new ExprCompiler(m, node, this);
      const val = ec.compileExpr(expr.value);
      this.emitPrintValue(val);
      return;
    }
    if (expr.type === 'LenCall' || expr.type === 'CapCall' || expr.type === 'IndexAccess') {
      const val = this.compileAstExpr(expr);
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
    if (val.type === 'TX') { castVal = m.extractPtr(val.reg); fmtStr = '%s\n'; }
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
        const ec = new ExprCompiler(m, node, this);
        const condReg = ec.compileCond(branch.cond);
        const nextLabel = condLabels[i + 1] || endLabel;
        m.emit(`br i1 ${condReg}, label %${bodyLabels[i]}, label %${nextLabel}`);
      }

      m.emitLabel(bodyLabels[i]);
      let terminated = false;
      for (const stmt of (branch.bodyStatements || [])) {
        terminated = this.genStatement(stmt);
      }
      if (!terminated) {
        m.emit(`br label %${endLabel}`);
      }
    });

    m.emitLabel(endLabel);
  }

  genCycle(node) {
    const m = this.mod;
    if (node.sourceExpr !== null) {
      m.unsupported(node, 'CYCLE ... IN list (LIST iteration not yet supported in LLVM codegen)');
      return;
    }

    const ec = new ExprCompiler(m, node, this);
    const fromVal = ec.compileExpr(node.fromExpr);
    const toVal = ec.compileExpr(node.toExpr);
    const stepVal = node.stepExpr ? ec.compileExpr(node.stepExpr) : { reg: '1', type: 'NUM' };

    const varName = safeName(node.iterVar);
    const loopVarDepth = node.depth !== undefined ? node.depth : m.currentDepth;
    const ptr = m.arenaAllocTyped('NUM', loopVarDepth);
    m.emit(`store i64 ${this.coerce(fromVal, 'NUM', node)}, i64* ${ptr}`);
    m.scope.set(node.iterVar, { ptr, plType: 'NUM', depth: loopVarDepth });

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
    // Iteration Breath (Article VII): save Arena_{loopVarDepth} offset before
    // the body executes, restore after each tick to prevent temporary variable
    // accumulation. The loop variable's fixed %ptr address survives the restore
    // because it was allocated before the save point.
    m.usesArena = true;
    const saveGep = m.freshReg();
    m.emit(`${saveGep} = getelementptr inbounds [${m.arenaDepthCap} x i64], [${m.arenaDepthCap} x i64]* @arena_offsets, i64 0, i64 ${loopVarDepth}`);
    const savedOff = m.freshReg();
    m.emit(`${savedOff} = load i64, i64* ${saveGep}`);

    let bodyTerminated = false;
    for (const stmt of (node.bodyStatements || [])) {
      bodyTerminated = this.genStatement(stmt);
    }
    if (!bodyTerminated) {
      // Restore Arena_{loopVarDepth} and reset any deeper arenas entered
      // during the body (Article VII — Iteration Breath).
      const endDepth = m.currentDepth;
      for (let d = endDepth; d > loopVarDepth; d--) {
        m.arenaResetDepth(d);
      }
      m.emit(`store i64 ${savedOff}, i64* ${saveGep}`);
      m.currentDepth = loopVarDepth;
      m.emit(`br label %${incLabel}`);
    }

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
    const ec = new ExprCompiler(m, node, this);
    const condReg = ec.compileCond(node.condExpr);
    m.emit(`br i1 ${condReg}, label %${bodyLabel}, label %${endLabel}`);

    m.emitLabel(bodyLabel);
    // Iteration Breath (Article VII): save arena offset before body
    const seasonDepth = node.depth !== undefined ? node.depth : m.currentDepth;
    m.usesArena = true;
    const saveGep = m.freshReg();
    m.emit(`${saveGep} = getelementptr inbounds [${m.arenaDepthCap} x i64], [${m.arenaDepthCap} x i64]* @arena_offsets, i64 0, i64 ${seasonDepth}`);
    const savedOff = m.freshReg();
    m.emit(`${savedOff} = load i64, i64* ${saveGep}`);

    let bodyTerminated = false;
    for (const stmt of (node.bodyStatements || [])) {
      bodyTerminated = this.genStatement(stmt);
    }
    if (!bodyTerminated) {
      const endDepth = m.currentDepth;
      for (let d = endDepth; d > seasonDepth; d--) {
        m.arenaResetDepth(d);
      }
      m.emit(`store i64 ${savedOff}, i64* ${saveGep}`);
      m.currentDepth = seasonDepth;
      m.emit(`br label %${condLabel}`);
    }

    m.emitLabel(endLabel);
  }

  // ── WEATHER / SHELTER / CALM (exception handling) ────────────────────────────
  genWeatherStatement(node) {
    const m = this.mod;
    const bodyLabel = m.freshLabel('weather.body');
    const calmLabel = m.freshLabel('weather.calm');

    const shelterHandlers = node.shelterClauses.map(clause => ({
      clause,
      handlerLabel: m.freshLabel('weather.shelter'),
    }));

    m.ensureWeatherGlobals();
    const unwindDepth = m.currentDepth;

    // Push shelter entries onto stack so error checks in the body
    // (e.g. emitZeroCheck) can find the nearest handler label.
    for (const sh of shelterHandlers) {
      this.shelterStack.push({
        unwindDepth,
        shelterClause: sh.clause,
        handlerLabel: sh.handlerLabel,
      });
    }

    m.emit(`br label %${bodyLabel}`);

    // ── Body block ──
    m.emitLabel(bodyLabel);
    let bodyTerminated = false;
    for (const stmt of node.bodyStatements) {
      bodyTerminated = this.genStatement(stmt);
    }
    if (!bodyTerminated) {
      m.emit(`br label %${calmLabel}`);
    }

    // Pop shelter entries BEFORE generating handler bodies, so that if
    // a handler's own body throws an error it propagates outward rather
    // than being re-caught by the same WEATHER block.
    for (const sh of shelterHandlers) {
      this.shelterStack.pop();
    }

    // ── Shelter handler blocks ──
    for (const sh of shelterHandlers) {
      m.emitLabel(sh.handlerLabel);

      // Unwind arenas from current depth down to unwindDepth + 1
      for (let d = m.currentDepth; d > unwindDepth; d--) {
        m.arenaResetDepth(d);
      }
      m.currentDepth = unwindDepth;

      // Bind errVar to the error message stored in @_weather_msg
      if (sh.clause.errVar) {
        const msgReg = m.freshReg();
        m.emit(`${msgReg} = load i8*, i8** @_weather_msg`);
        // Get the length of the message
        const msgLen = m.freshReg();
        m.emit(`${msgLen} = call i64 @strlen(i8* ${msgReg})`);
        const msgCap = m.freshReg();
        m.emit(`${msgCap} = add i64 ${msgLen}, 1`);
        const ptr = m.arenaAllocTyped('TX', unwindDepth);
        const fpReg = m.buildFatPtr(msgReg, msgLen, msgCap);
        m.emit(`store %fat_ptr ${fpReg}, %fat_ptr* ${ptr}`);
        m.scope.set(sh.clause.errVar, { ptr, plType: 'TX', depth: unwindDepth });
      }

      for (const stmt of sh.clause.bodyStatements) {
        this.genStatement(stmt);
      }
      m.emit(`br label %${calmLabel}`);
    }

    // ── Calm block ──
    m.emitLabel(calmLabel);
    // Reset error flag for clean state after WEATHER/SHELTER
    m.emit(`store i1 false, i1* @_weather_flag`);
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
