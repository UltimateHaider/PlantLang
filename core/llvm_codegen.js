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
  if (LLVM_TYPE[plType]) return LLVM_TYPE[plType];
  // Array types [NUM], [TX], [Point] — use %fat_ptr (i8* ptr + i64 len + i64 cap)
  if (typeof plType === 'string' && plType.startsWith('[') && plType.endsWith(']')) {
    return '%fat_ptr';
  }
  // MAP types are stored as %fat_ptr (same layout: { i8* buckets, i64 len, i64 cap })
  if (isMapTypeStr(plType)) {
    return '%fat_ptr';
  }
  // Check if it's a registered struct type
  if (STRUCT_SIZES.has(`%struct.${plType}`)) return `%struct.${plType}`;
  return null; // unsupported
}

// Get the LLVM element type for an array type string like "[NUM]" → "i64"
function arrayElemLlvmType(arrayPlType) {
  const inner = typeof arrayPlType === 'string' && arrayPlType.startsWith('[') && arrayPlType.endsWith(']')
    ? arrayPlType.slice(1, -1) : null;
  if (!inner) return null;
  return llvmType(inner);
}

// Get the size of an array element type
function arrayElemSize(innerPlType) {
  const lt = llvmType(innerPlType);
  if (!lt) {
    // Try as struct type
    const structType = `%struct.${innerPlType}`;
    return STRUCT_SIZES.get(structType) || 8;
  }
  return llvmTypeSize(lt);
}

// Check if a string is an array type like [NUM], [TX], [Point]
function isArrayTypeStr(s) {
  return typeof s === 'string' && s.startsWith('[') && s.endsWith(']') && s.length >= 3;
}

// Check if a string is a MAP type like MAP[NUM,TX], MAP[TX,NUM]
function isMapTypeStr(s) {
  return typeof s === 'string' && s.startsWith('MAP[') && s.endsWith(']');
}

// Extract key/value types from MAP[NUM,TX] → { keyType, valueType }
function mapInnerTypes(s) {
  if (!isMapTypeStr(s)) return null;
  const inner = s.slice(4, -1);
  const parts = inner.split(',');
  if (parts.length !== 2) return null;
  return { keyType: parts[0].trim(), valueType: parts[1].trim() };
}

// Generate the LLVM bucket struct name for a MAP type
function mapBucketTypeName(keyType, valueType) {
  return `%map_bucket.${keyType}.${valueType}`;
}

// Generate the LLVM bucket struct type definition { i1, keyType, valueType }
function mapBucketLlvmType(keyPlType, valuePlType) {
  const keyLt = llvmType(keyPlType) || 'i64';
  const valLt = llvmType(valuePlType) || 'i64';
  return `{ i1, ${keyLt}, ${valLt} }`;
}

// Compute the size of a single bucket for a MAP type
function mapBucketSize(keyPlType, valuePlType) {
  const keyLt = llvmType(keyPlType) || 'i64';
  const valLt = llvmType(valuePlType) || 'i64';
  // Natural alignment: max(1, align(keyLt), align(valLt))
  const maxAlign = 8; // i64 and %fat_ptr both have 8-byte alignment
  // i1 at offset 0, padded to align of next field
  let off = 1;
  // Align to 8 for key
  off = Math.ceil(off / 8) * 8;
  off += llvmTypeSize(keyLt);
  // Align to 8 for value
  off = Math.ceil(off / 8) * 8;
  off += llvmTypeSize(valLt);
  // Round up to maxAlign
  off = Math.ceil(off / maxAlign) * maxAlign;
  return off;
}

// Compute the LLVM type for the composite map struct: { bucket_ptr, i64, i64 }
function mapPtrLlvmType() { return '%fat_ptr'; }

// Fat pointer field accessors (expressed as types for extractvalue/insertvalue)
const FAT_PTR = { PTR: 0, LEN: 1, CAP: 2 };

// Map field offsets (same layout as fat_ptr: { i8* buckets, i64 len, i64 cap })
const MAP_FIELDS = { BUCKETS: 0, LEN: 1, CAP: 2 };

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
  // Named struct types: store registered size
  if (typeof lt === 'string' && lt.startsWith('%')) {
    return STRUCT_SIZES.get(lt) || 8;
  }
  return 8; // default pointer-sized
}

// Register struct type sizes for LLVM codegen
const STRUCT_SIZES = new Map();
function registerStructType(name, fields) {
  let totalSize = 0;
  const llvmFields = fields.map(f => {
    const lt = llvmType(f.varType) || 'i64';
    const sz = llvmTypeSize(lt);
    // Align to 8 bytes for struct field
    const aligned = Math.ceil(totalSize / 8) * 8;
    const padding = aligned - totalSize;
    totalSize += padding + sz;
    return lt;
  });
  // Total struct alignment to 8 bytes
  totalSize = Math.ceil(totalSize / 8) * 8;
  const structType = `%struct.${name}`;
  STRUCT_SIZES.set(structType, totalSize);
  // Store field name -> index mapping
  const fieldIndex = {};
  fields.forEach((f, i) => { fieldIndex[f.name] = i; });
  return { structType, llvmFields, totalSize, fieldIndex, rawFields: fields };
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
    this.usesMemset = false;
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
    this.structTypes = new Map(); // name -> { structType, llvmFields, totalSize }
    this.structDeclLines = []; // LLVM IR lines for type declarations
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

  /** Look up the field index for a struct type by field name. */
  _getStructFieldIndex(structName, fieldName) {
    const info = this.structTypes.get(structName);
    if (!info || !info.fieldIndex) return -1;
    return info.fieldIndex[fieldName] !== undefined ? info.fieldIndex[fieldName] : -1;
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
    const errMsg = m.addStringConstant('division by zero');
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
    // First pass: collect ACTION declarations and SHAPE definitions
    for (const node of (programNode.statements || [])) {
      if (node.type === 'StructDeclaration') {
        const reg = registerStructType(node.name, node.fields);
        this.structTypes.set(node.name, reg);
        this.structDeclLines.push(`${reg.structType} = type { ${reg.llvmFields.join(', ')} }`);
        continue;
      }
      if (node.type === 'ActionDeclaration') {
        const fnName = node.receiver ? `${node.receiver.type}_${node.name}` : node.name;
        this.fnInfos.set(fnName, {
          params: node.params,
          bodyStatements: node.bodyStatements,
          line: node.line,
          column: node.column,
          isExternal: !!node.isExternal,
          receiver: node.receiver,
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
    // For receiver methods, the first parameter is the receiver pointer
    if (info.receiver) {
      const recvLt = llvmType(info.receiver.type);
      if (recvLt) {
        llvmParams.push(`${recvLt}* %${safeName(info.receiver.name)}`);
      } else {
        m.error(`Unsupported receiver type "${info.receiver.type}" in ACTION "${name}"`, info);
      }
    }
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
    // Handle receiver first if present
    if (info.receiver) {
      const recvLt = llvmType(info.receiver.type);
      if (recvLt) {
        const sName = safeName(info.receiver.name);
        // Receiver is already a pointer — store it directly in scope
        m.scope.set(info.receiver.name, { ptr: `%${sName}`, plType: info.receiver.type, depth: 0 });
      }
    }
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

  // ── CREATE with struct type ────────────────────────────────────────────────
  genCreateStruct(node, structInfo) {
    const m = this.mod;
    const depth = node.depth !== undefined ? node.depth : m.currentDepth;
    if (depth > m.currentDepth) {
      m.error(`Contract violation: struct CREATE at depth ${depth} > current ${m.currentDepth}`, node);
      return;
    }
    // Allocate arena space for the struct
    const rawPtr = m.arenaAlloc(depth, structInfo.totalSize);
    const structPtr = m.freshReg();
    m.emit(`${structPtr} = bitcast i8* ${rawPtr} to ${structInfo.structType}*`);
    m.scope.set(node.identifier, { ptr: structPtr, plType: node.varType, depth });

    // If there's a struct instantiation expression, store each field
    const ve = node.valueExpr;
    if (ve && ve.type === 'StructInstantiation') {
      const args = ve.args || [];
      for (let i = 0; i < args.length; i++) {
        const fieldInfo = structInfo.llvmFields[i];
        const fieldType = fieldInfo;
        const fieldPtr = m.freshReg();
        m.emit(`${fieldPtr} = getelementptr inbounds ${structInfo.structType}, ${structInfo.structType}* ${structPtr}, i32 0, i32 ${i}`);
        let val;
        const arg = args[i];
        if (arg.type === 'StructInstantiation') {
          m.error(`Nested struct not yet supported in LLVM codegen`, arg);
          continue;
        } else if (arg.type === 'Literal' || arg.type === 'Identifier') {
          val = this.compileAstExpr(arg);
        } else if (typeof arg === 'string') {
          const ec = new ExprCompiler(m, node, this);
          val = ec.compileExpr(arg);
        } else if (arg && arg.type === 'LenCall' || arg && arg.type === 'CapCall' || arg && arg.type === 'IndexAccess') {
          val = this.compileAstExpr(arg);
        } else {
          val = { reg: '0', type: 'NUM' };
        }
        const fieldPlType = structInfo.llvmFields[i] === '%fat_ptr' ? 'TX' :
                            structInfo.llvmFields[i] === 'double' ? 'SCL' :
                            structInfo.llvmFields[i] === 'i1' ? 'FACT' : 'NUM';
        const coerced = this.coerce(val, fieldPlType, node);
        const storeType = structInfo.llvmFields[i];
        m.emit(`store ${storeType} ${coerced}, ${storeType}* ${fieldPtr}`);
      }
    } else if (ve && ve.type === 'StructLiteral') {
      for (const fv of ve.fields) {
        const fieldIdx = structInfo.fieldIndex[fv.name];
        if (fieldIdx === undefined) {
          m.error(`Struct "${node.varType}" has no field "${fv.name}"`, node);
          continue;
        }
        const fieldPtr = m.freshReg();
        m.emit(`${fieldPtr} = getelementptr inbounds ${structInfo.structType}, ${structInfo.structType}* ${structPtr}, i32 0, i32 ${fieldIdx}`);
        let val;
        if (fv.value.type === 'Literal' || fv.value.type === 'Identifier') {
          val = this.compileAstExpr(fv.value);
        } else if (fv.value.type === 'LenCall' || fv.value.type === 'CapCall' || fv.value.type === 'IndexAccess') {
          val = this.compileAstExpr(fv.value);
        } else {
          val = { reg: '0', type: 'NUM' };
        }
        const fieldPlType = structInfo.llvmFields[fieldIdx] === '%fat_ptr' ? 'TX' :
                            structInfo.llvmFields[fieldIdx] === 'double' ? 'SCL' :
                            structInfo.llvmFields[fieldIdx] === 'i1' ? 'FACT' : 'NUM';
        const coerced = this.coerce(val, fieldPlType, node);
        const storeType = structInfo.llvmFields[fieldIdx];
        m.emit(`store ${storeType} ${coerced}, ${storeType}* ${fieldPtr}`);
      }
    }
  }

  // ── CREATE array ([NUM], [TX], [Point], etc.) ──────────────────────────────
  genCreateArray(node) {
    const m = this.mod;
    const depth = node.depth !== undefined ? node.depth : m.currentDepth;
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

    const innerType = node.varType.slice(1, -1); // "[NUM]" → "NUM"
    const elemLt = llvmType(innerType);
    if (!elemLt) { m.unsupported(node, `array element type ${innerType}`); return; }
    const elemSize = arrayElemSize(innerType);

    const ve = node.valueExpr;
    let len = 0;
    let cap = 0;
    let arrVal; // { reg: fatPtrReg, type: node.varType }

    if (ve && ve.type === 'ArrayLiteral') {
      len = ve.elements.length;
      cap = len;
      const bufSize = cap * elemSize;
      const rawBuf = m.arenaAlloc(depth, bufSize);
      // Bitcast to element type pointer
      const typedBuf = m.freshReg();
      m.emit(`${typedBuf} = bitcast i8* ${rawBuf} to ${elemLt}*`);
      // Store each element
      for (let i = 0; i < ve.elements.length; i++) {
        const elExpr = this.compileAstExpr(ve.elements[i]);
        const elPtr = m.freshReg();
        m.emit(`${elPtr} = getelementptr inbounds ${elemLt}, ${elemLt}* ${typedBuf}, i64 ${i}`);
        const elPlType = innerType === 'TX' ? 'TX' : innerType === 'SCL' ? 'SCL' : innerType === 'FACT' ? 'FACT' : 'NUM';
        const coerced = this.coerce(elExpr, elPlType, node);
        m.emit(`store ${elemLt} ${coerced}, ${elemLt}* ${elPtr}`);
      }
      const fpReg = m.buildFatPtr(rawBuf, String(len), String(cap));
      arrVal = { reg: fpReg, type: node.varType };
    } else {
      // Empty array
      const rawBuf = m.arenaAlloc(depth, 1);
      m.emit(`store i8 0, i8* ${rawBuf}`);
      const fpReg = m.buildFatPtr(rawBuf, '0', '1');
      arrVal = { reg: fpReg, type: node.varType };
    }

    const ptr = m.arenaAllocTyped(node.varType, depth);
    const lt = llvmType(node.varType);
    m.emit(`store ${lt} ${arrVal.reg}, ${lt}* ${ptr}`);
    m.scope.set(node.identifier, { ptr, plType: node.varType, depth });
  }

  // ── MAP[NUM,TX] codegen helpers ──────────────────────────────────

  genCreateMap(node) {
    const m = this.mod;
    const depth = node.depth !== undefined ? node.depth : m.currentDepth;
    if (depth > m.currentDepth) {
      m.error(
        `═══ ⚠ Contract Violation: Illegal Destination ═══\n` +
        `  Operation:  CREATE\n` +
        `  Variable:   "${node.identifier}"\n` +
        `  Destination: depth ${depth}\n` +
        `  Current:     depth ${m.currentDepth}\n`,
        node
      );
      return;
    }

    // Initial capacity: 8 buckets
    const initialCap = 8;
    const mt = mapInnerTypes(node.varType);
    const bucketType = mapBucketLlvmType(mt.keyType, mt.valueType);
    const bktSize = mapBucketSize(mt.keyType, mt.valueType);
    const initBytes = initialCap * bktSize;

    // Allocate bucket array
    const rawBuckets = m.arenaAlloc(depth, initBytes);
    // Zero-initialize: set all bytes to 0 (is_occupied = false for all buckets)
    m.usesMemset = true;
    m.emit(`call void @llvm.memset.p0i8.i64(i8* ${rawBuckets}, i8 0, i64 ${initBytes}, i1 false)`);

    // Build the map struct (reuse %fat_ptr layout: { i8* buckets, i64 len, i64 cap })
    const mapReg = m.buildFatPtr(rawBuckets, '0', String(initialCap));
    const ptr = m.arenaAllocTyped(node.varType, depth);
    m.emit(`store %fat_ptr ${mapReg}, %fat_ptr* ${ptr}`);
    m.scope.set(node.identifier, { ptr, plType: node.varType, depth });
  }

  // Emit a djb2 hash of a TX fat pointer value → i64 register
  emitTxHash(txFpReg, node) {
    const m = this.mod;
    const ptrReg = m.extractPtr(txFpReg);
    const lenReg = m.extractLen(txFpReg);

    // hash = 5381; i = 0
    const hashReg = m.freshReg();
    m.emit(`${hashReg} = alloca i64`);
    m.emit(`store i64 5381, i64* ${hashReg}`);

    const entryL = m.freshLabel('txhash.entry');
    const loopL = m.freshLabel('txhash.loop');
    const doneL = m.freshLabel('txhash.done');
    m.emit(`br label %${entryL}`);

    // entry: initialize i = 0
    m.emitLabel(entryL);
    const iReg = m.freshReg();
    m.emit(`${iReg} = alloca i64`);
    m.emit(`store i64 0, i64* ${iReg}`);
    m.emit(`br label %${loopL}`);

    // loop: check i < len
    m.emitLabel(loopL);
    const curI = m.freshReg();
    m.emit(`${curI} = load i64, i64* ${iReg}`);
    const cmp = m.freshReg();
    m.emit(`${cmp} = icmp ult i64 ${curI}, ${lenReg}`);
    m.emit(`br i1 ${cmp}, label %${doneL}, label %${doneL}`);

    // Read byte at ptr[i]
    const charPtr = m.freshReg();
    m.emit(`${charPtr} = getelementptr inbounds i8, i8* ${ptrReg}, i64 ${curI}`);
    const ch = m.freshReg();
    m.emit(`${ch} = load i8, i8* ${charPtr}`);
    const chZext = m.freshReg();
    m.emit(`${chZext} = zext i8 ${ch} to i64`);

    // hash = hash * 33 + ch
    const oldHash = m.freshReg();
    m.emit(`${oldHash} = load i64, i64* ${hashReg}`);
    const mult = m.freshReg();
    m.emit(`${mult} = mul i64 ${oldHash}, 33`);
    const newHash = m.freshReg();
    m.emit(`${newHash} = add i64 ${mult}, ${chZext}`);
    m.emit(`store i64 ${newHash}, i64* ${hashReg}`);

    // i++
    const nextI = m.freshReg();
    m.emit(`${nextI} = add i64 ${curI}, 1`);
    m.emit(`store i64 ${nextI}, i64* ${iReg}`);
    m.emit(`br label %${loopL}`);

    // done
    m.emitLabel(doneL);
    const finalHash = m.freshReg();
    m.emit(`${finalHash} = load i64, i64* ${hashReg}`);
    return finalHash;
  }

  // Emit the map put operation inline — modifies the map through its fat_ptr pointer
  // Returns nothing (void-like, but returns a dummy i64 0 for consistency)
  genMapPut(node, mapTypeStr, mapPtr, keyVal, valueVal) {
    const m = this.mod;
    const mt = mapInnerTypes(mapTypeStr);
    const bucketType = mapBucketLlvmType(mt.keyType, mt.valueType);
    const bktSize = mapBucketSize(mt.keyType, mt.valueType);
    const keyLt = llvmType(mt.keyType) || 'i64';
    const valLt = llvmType(mt.valueType) || 'i64';

    // Load the map struct (fat_ptr)
    const fpReg = m.freshReg();
    m.emit(`${fpReg} = load %fat_ptr, %fat_ptr* ${mapPtr}`);
    const bucketsPtr = m.extractPtr(fpReg);
    const lenReg = m.extractLen(fpReg);
    const capReg = m.extractCap(fpReg);

    // Check load factor: len >= cap * 3 / 4
    const cap3 = m.freshReg();
    m.emit(`${cap3} = mul i64 ${capReg}, 3`);
    const threshold = m.freshReg();
    m.emit(`${threshold} = udiv i64 ${cap3}, 4`);
    const doGrow = m.freshReg();
    m.emit(`${doGrow} = icmp uge i64 ${lenReg}, ${threshold}`);
    const growL = m.freshLabel('map.put.grow');
    const skipGrowL = m.freshLabel('map.put.skipgrow');
    m.emit(`br i1 ${doGrow}, label %${growL}, label %${skipGrowL}`);

    // ── Grow block ──
    m.emitLabel(growL);
    const newCapG = m.freshReg();
    m.emit(`${newCapG} = mul i64 ${capReg}, 2`);
    this.emitMapGrow(node, mapTypeStr, bucketsPtr, lenReg, capReg, newCapG, mapPtr);
    // Reload the map struct after grow
    const fpG = m.freshReg();
    m.emit(`${fpG} = load %fat_ptr, %fat_ptr* ${mapPtr}`);
    const newBucketsG = m.extractPtr(fpG);
    const newCapG2 = m.extractCap(fpG);
    m.emit(`br label %${skipGrowL}`);

    // ── Common tail after potential grow ──
    m.emitLabel(skipGrowL);
    // Reload fresh from pointer (may have been updated by grow)
    const fp2 = m.freshReg();
    m.emit(`${fp2} = load %fat_ptr, %fat_ptr* ${mapPtr}`);
    const bucketsFinal = m.extractPtr(fp2);
    const capFinal = m.extractCap(fp2);
    const lenFinal = m.extractLen(fp2);

    // Compute hash: for NUM keys, just use the key value; for TX, emit djb2
    let hashReg;
    let keyReg = keyVal.reg;
    if (mt.keyType === 'TX') {
      hashReg = this.emitTxHash(keyReg, node);
    } else if (mt.keyType === 'NUM') {
      hashReg = keyReg; // identity hash
    } else {
      // Fallback: just use the key as-is (unsafe but allows compilation)
      hashReg = keyReg;
    }

    // start_index = hash % cap
    const startIdx = m.freshReg();
    m.emit(`${startIdx} = urem i64 ${hashReg}, ${capFinal}`);

    // Probe loop labels
    const probeL = m.freshLabel('map.put.probe');
    const foundL = m.freshLabel('map.put.found');
    const emptyL = m.freshLabel('map.put.empty');
    const exitL = m.freshLabel('map.put.exit');
    const idxAlloca = m.freshReg();
    m.emit(`${idxAlloca} = alloca i64`);
    m.emit(`store i64 ${startIdx}, i64* ${idxAlloca}`);
    m.emit(`br label %${probeL}`);

    // ── Probe loop ──
    m.emitLabel(probeL);
    const curIdx = m.freshReg();
    m.emit(`${curIdx} = load i64, i64* ${idxAlloca}`);

    // bucket_ptr = buckets + curIdx * bucketSize
    const byteOff = m.freshReg();
    m.emit(`${byteOff} = mul i64 ${curIdx}, ${bktSize}`);
    const bucketPtr = m.freshReg();
    m.emit(`${bucketPtr} = getelementptr inbounds i8, i8* ${bucketsFinal}, i64 ${byteOff}`);
    const bktTyped = m.freshReg();
    m.emit(`${bktTyped} = bitcast i8* ${bucketPtr} to ${bucketType}*`);

    // Check is_occupied (at offset 0)
    const occPtr = m.freshReg();
    m.emit(`${occPtr} = getelementptr inbounds ${bucketType}, ${bucketType}* ${bktTyped}, i64 0, i32 0`);
    const isOcc = m.freshReg();
    m.emit(`${isOcc} = load i1, i1* ${occPtr}`);

    // If empty: branch to emptyL
    m.emit(`br i1 ${isOcc}, label %${foundL}, label %${emptyL}`);

    // ── Found (is_occupied == 1): check key match ──
    m.emitLabel(foundL);
    const keyPtr = m.freshReg();
    m.emit(`${keyPtr} = getelementptr inbounds ${bucketType}, ${bucketType}* ${bktTyped}, i64 0, i32 1`);
    const existingKey = m.freshReg();
    m.emit(`${existingKey} = load ${keyLt}, ${keyLt}* ${keyPtr}`);

    // Compare keys
    let keysMatch;
    if (keyLt === 'i64') {
      const km = m.freshReg();
      m.emit(`${km} = icmp eq i64 ${existingKey}, ${keyReg}`);
      keysMatch = km;
    } else if (keyLt === '%fat_ptr') {
      // TX key comparison: use strcmp
      const existingPtr = m.extractPtr(existingKey);
      const keyPtr2 = m.extractPtr(keyReg);
      const scmp = m.freshReg();
      m.emit(`${scmp} = call i32 @strcmp(i8* ${existingPtr}, i8* ${keyPtr2})`);
      const sm = m.freshReg();
      m.emit(`${sm} = icmp eq i32 ${scmp}, 0`);
      keysMatch = sm;
    } else {
      // fallback: use bitcast to i64 comparison
      const km = m.freshReg();
      m.emit(`${km} = icmp eq i64 ${existingKey}, ${keyReg}`);
      keysMatch = km;
    }

    m.emit(`br i1 ${keysMatch}, label %${exitL}, label %${emptyL}`);

    // ── Empty/Not-Matched: write if empty, else continue probing ──
    m.emitLabel(emptyL);
    // If is_occupied == 0: this is an empty slot → write
    // If is_occupied == 1: this is a collision → probe next
    const collisionLabel = m.freshLabel('map.put.collision');
    m.emit(`br i1 ${isOcc}, label %${collisionLabel}, label %${exitL}`);

    // ── Collision: index = (index + 1) % cap ──
    m.emitLabel(collisionLabel);
    const nextIdx = m.freshReg();
    m.emit(`${nextIdx} = add i64 ${curIdx}, 1`);
    const wrappedIdx = m.freshReg();
    m.emit(`${wrappedIdx} = urem i64 ${nextIdx}, ${capFinal}`);
    m.emit(`store i64 ${wrappedIdx}, i64* ${idxAlloca}`);
    m.emit(`br label %${probeL}`);

    // ── Exit: store key and value, update is_occupied, increment len ──
    m.emitLabel(exitL);
    // Set is_occupied = true (only if was empty — idempotent for found case)
    m.emit(`store i1 true, i1* ${occPtr}`);
    // Store key
    const keyPtr2 = m.freshReg();
    m.emit(`${keyPtr2} = getelementptr inbounds ${bucketType}, ${bucketType}* ${bktTyped}, i64 0, i32 1`);
    m.emit(`store ${keyLt} ${keyReg}, ${keyLt}* ${keyPtr2}`);
    // Store value
    const valPtr = m.freshReg();
    m.emit(`${valPtr} = getelementptr inbounds ${bucketType}, ${bucketType}* ${bktTyped}, i64 0, i32 2`);
    m.emit(`store ${valLt} ${valueVal.reg}, ${valLt}* ${valPtr}`);

    // Increment len (only if was empty — but idempotent)
    const newLen = m.freshReg();
    m.emit(`${newLen} = add i64 ${lenFinal}, 1`);
    // Store updated map struct back
    const updatedFp = m.buildFatPtr(bucketsFinal, newLen, capFinal);
    m.emit(`store %fat_ptr ${updatedFp}, %fat_ptr* ${mapPtr}`);
  }

  // Emit map grow: allocate new bucket array, rehash all entries, update map pointer
  emitMapGrow(node, mapTypeStr, oldBuckets, oldLen, oldCap, newCapReg, mapPtr) {
    const m = this.mod;
    const mt = mapInnerTypes(mapTypeStr);
    const bucketType = mapBucketLlvmType(mt.keyType, mt.valueType);
    const bktSize = mapBucketSize(mt.keyType, mt.valueType);
    const keyLt = llvmType(mt.keyType) || 'i64';

    const depth = node.depth !== undefined ? node.depth : m.currentDepth;

    // newSize = newCap * bucketSize
    const newSize = m.freshReg();
    m.emit(`${newSize} = mul i64 ${newCapReg}, ${bktSize}`);
    const newBuckets = m.arenaAlloc(depth, { reg: newSize, type: 'i64' });
    // Zero-initialize
    m.usesMemset = true;
    m.emit(`call void @llvm.memset.p0i8.i64(i8* ${newBuckets}, i8 0, i64 ${newSize}, i1 false)`);

    // Loop over old buckets, rehash occupied ones
    const loopL = m.freshLabel('map.grow.loop');
    const doneL = m.freshLabel('map.grow.done');
    const iAlloca = m.freshReg();
    m.emit(`${iAlloca} = alloca i64`);
    m.emit(`store i64 0, i64* ${iAlloca}`);
    m.emit(`br label %${loopL}`);

    m.emitLabel(loopL);
    const i = m.freshReg();
    m.emit(`${i} = load i64, i64* ${iAlloca}`);
    const icmp = m.freshReg();
    m.emit(`${icmp} = icmp ult i64 ${i}, ${oldLen}`);
    const bodyL = m.freshLabel('map.grow.body');
    m.emit(`br i1 ${icmp}, label %${bodyL}, label %${doneL}`);
    m.emitLabel(bodyL);

    // Read old bucket at index i
    const byteOff = m.freshReg();
    m.emit(`${byteOff} = mul i64 ${i}, ${bktSize}`);
    const oldBktRaw = m.freshReg();
    m.emit(`${oldBktRaw} = getelementptr inbounds i8, i8* ${oldBuckets}, i64 ${byteOff}`);
    const oldBkt = m.freshReg();
    m.emit(`${oldBkt} = bitcast i8* ${oldBktRaw} to ${bucketType}*`);

    const occOld = m.freshReg();
    m.emit(`${occOld} = getelementptr inbounds ${bucketType}, ${bucketType}* ${oldBkt}, i64 0, i32 0`);
    const isOccOld = m.freshReg();
    m.emit(`${isOccOld} = load i1, i1* ${occOld}`);

    const skipL = m.freshLabel('map.grow.skip');
    const rehashL = m.freshLabel('map.grow.rehash');
    m.emit(`br i1 ${isOccOld}, label %${rehashL}, label %${skipL}`);

    // ── Rehash this entry ──
    m.emitLabel(rehashL);
    const oldKeyPtr = m.freshReg();
    m.emit(`${oldKeyPtr} = getelementptr inbounds ${bucketType}, ${bucketType}* ${oldBkt}, i64 0, i32 1`);
    const oldKey = m.freshReg();
    m.emit(`${oldKey} = load ${keyLt}, ${keyLt}* ${oldKeyPtr}`);

    const oldValPtr = m.freshReg();
    m.emit(`${oldValPtr} = getelementptr inbounds ${bucketType}, ${bucketType}* ${oldBkt}, i64 0, i32 2`);
    const oldVal = m.freshReg();
    const valLt = llvmType(mt.valueType) || 'i64';
    m.emit(`${oldVal} = load ${valLt}, ${valLt}* ${oldValPtr}`);

    // Hash the key
    let hashG;
    if (mt.keyType === 'NUM') {
      hashG = oldKey;
    } else if (mt.keyType === 'TX') {
      hashG = this.emitTxHash(oldKey, node);
    } else {
      hashG = oldKey;
    }
    const newIdx = m.freshReg();
    m.emit(`${newIdx} = urem i64 ${hashG}, ${newCapReg}`);

    // Probe for empty slot in new array
    const probeGL = m.freshLabel('map.grow.probe');
    const foundGL = m.freshLabel('map.grow.found');
    const storeL = m.freshLabel('map.grow.store');
    const idxAllocaG = m.freshReg();
    m.emit(`${idxAllocaG} = alloca i64`);
    m.emit(`store i64 ${newIdx}, i64* ${idxAllocaG}`);
    m.emit(`br label %${probeGL}`);

    m.emitLabel(probeGL);
    const curIdxG = m.freshReg();
    m.emit(`${curIdxG} = load i64, i64* ${idxAllocaG}`);
    const byteOffG = m.freshReg();
    m.emit(`${byteOffG} = mul i64 ${curIdxG}, ${bktSize}`);
    const newBktRaw = m.freshReg();
    m.emit(`${newBktRaw} = getelementptr inbounds i8, i8* ${newBuckets}, i64 ${byteOffG}`);
    const newBkt = m.freshReg();
    m.emit(`${newBkt} = bitcast i8* ${newBktRaw} to ${bucketType}*`);

    const occNew = m.freshReg();
    m.emit(`${occNew} = getelementptr inbounds ${bucketType}, ${bucketType}* ${newBkt}, i64 0, i32 0`);
    const isOccNew = m.freshReg();
    m.emit(`${isOccNew} = load i1, i1* ${occNew}`);

    m.emit(`br i1 ${isOccNew}, label %${foundGL}, label %${storeL}`);
    // foundGL: collision in new array → probe next
    m.emitLabel(foundGL);
    const nextG = m.freshReg();
    m.emit(`${nextG} = add i64 ${curIdxG}, 1`);
    const wrapG = m.freshReg();
    m.emit(`${wrapG} = urem i64 ${nextG}, ${newCapReg}`);
    m.emit(`store i64 ${wrapG}, i64* ${idxAllocaG}`);
    m.emit(`br label %${probeGL}`);

    // storeL: found empty slot in new array
    m.emitLabel(storeL);
    m.emit(`store i1 true, i1* ${occNew}`);
    const newKeyPtr = m.freshReg();
    m.emit(`${newKeyPtr} = getelementptr inbounds ${bucketType}, ${bucketType}* ${newBkt}, i64 0, i32 1`);
    m.emit(`store ${keyLt} ${oldKey}, ${keyLt}* ${newKeyPtr}`);
    const newValPtr = m.freshReg();
    m.emit(`${newValPtr} = getelementptr inbounds ${bucketType}, ${bucketType}* ${newBkt}, i64 0, i32 2`);
    m.emit(`store ${valLt} ${oldVal}, ${valLt}* ${newValPtr}`);
    m.emit(`br label %${skipL}`);

    // skipL: old bucket was empty, or done storing rehashed entry
    m.emitLabel(skipL);
    // i++
    const nextI = m.freshReg();
    m.emit(`${nextI} = add i64 ${i}, 1`);
    m.emit(`store i64 ${nextI}, i64* ${iAlloca}`);
    m.emit(`br label %${loopL}`);

    m.emitLabel(doneL);
    // Update map pointer with new bucket array and capacity (len stays same)
    const newFp = m.buildFatPtr(newBuckets, oldLen, newCapReg);
    m.emit(`store %fat_ptr ${newFp}, %fat_ptr* ${mapPtr}`);
  }

  // Emit map has check: returns i64 (0 = not found, 1 = found)
  genMapHas(node, mapTypeStr, mapReg, keyVal) {
    const m = this.mod;
    const mt = mapInnerTypes(mapTypeStr);
    const bucketType = mapBucketLlvmType(mt.keyType, mt.valueType);
    const bktSize = mapBucketSize(mt.keyType, mt.valueType);
    const keyLt = llvmType(mt.keyType) || 'i64';

    const bucketsPtr = m.extractPtr(mapReg);
    const capReg = m.extractCap(mapReg);

    // Compute hash
    let hashReg;
    let keyReg = keyVal.reg;
    if (mt.keyType === 'TX') {
      hashReg = this.emitTxHash(keyReg, node);
    } else if (mt.keyType === 'NUM') {
      hashReg = keyReg;
    } else {
      hashReg = keyReg;
    }

    const startIdx = m.freshReg();
    m.emit(`${startIdx} = urem i64 ${hashReg}, ${capReg}`);

    // Result flag (alloca so we can set it from any block)
    const resAlloca = m.freshReg();
    m.emit(`${resAlloca} = alloca i64`);
    m.emit(`store i64 0, i64* ${resAlloca}`);

    const probeL = m.freshLabel('map.has.probe');
    const foundL = m.freshLabel('map.has.found');
    const emptyL = m.freshLabel('map.has.empty');
    const exitL = m.freshLabel('map.has.exit');
    const idxAlloca = m.freshReg();
    m.emit(`${idxAlloca} = alloca i64`);
    m.emit(`store i64 ${startIdx}, i64* ${idxAlloca}`);
    m.emit(`br label %${probeL}`);

    m.emitLabel(probeL);
    const curIdx = m.freshReg();
    m.emit(`${curIdx} = load i64, i64* ${idxAlloca}`);
    const byteOff = m.freshReg();
    m.emit(`${byteOff} = mul i64 ${curIdx}, ${bktSize}`);
    const bktRaw = m.freshReg();
    m.emit(`${bktRaw} = getelementptr inbounds i8, i8* ${bucketsPtr}, i64 ${byteOff}`);
    const bkt = m.freshReg();
    m.emit(`${bkt} = bitcast i8* ${bktRaw} to ${bucketType}*`);

    const occPtr = m.freshReg();
    m.emit(`${occPtr} = getelementptr inbounds ${bucketType}, ${bucketType}* ${bkt}, i64 0, i32 0`);
    const isOcc = m.freshReg();
    m.emit(`${isOcc} = load i1, i1* ${occPtr}`);

    m.emit(`br i1 ${isOcc}, label %${foundL}, label %${emptyL}`);

    // ── Found: check key match ──
    m.emitLabel(foundL);
    const keyPtr = m.freshReg();
    m.emit(`${keyPtr} = getelementptr inbounds ${bucketType}, ${bucketType}* ${bkt}, i64 0, i32 1`);
    const existingKey = m.freshReg();
    m.emit(`${existingKey} = load ${keyLt}, ${keyLt}* ${keyPtr}`);

    let keysMatch;
    if (keyLt === 'i64') {
      const km = m.freshReg();
      m.emit(`${km} = icmp eq i64 ${existingKey}, ${keyReg}`);
      keysMatch = km;
    } else if (keyLt === '%fat_ptr') {
      const existingPtr = m.extractPtr(existingKey);
      const keyPtr2 = m.extractPtr(keyReg);
      const scmp = m.freshReg();
      m.emit(`${scmp} = call i32 @strcmp(i8* ${existingPtr}, i8* ${keyPtr2})`);
      const sm = m.freshReg();
      m.emit(`${sm} = icmp eq i32 ${scmp}, 0`);
      keysMatch = sm;
    } else {
      const km = m.freshReg();
      m.emit(`${km} = icmp eq i64 ${existingKey}, ${keyReg}`);
      keysMatch = km;
    }

    // If match: set result=1, exit. Otherwise: treat as collision → probe next
    const matchExit = m.freshLabel('map.has.matchexit');
    const collisionL = m.freshLabel('map.has.collision');
    m.emit(`br i1 ${keysMatch}, label %${matchExit}, label %${collisionL}`);

    m.emitLabel(matchExit);
    m.emit(`store i64 1, i64* ${resAlloca}`);
    m.emit(`br label %${exitL}`);

    // ── Collision: probe next slot ──
    m.emitLabel(collisionL);
    const nextIdx = m.freshReg();
    m.emit(`${nextIdx} = add i64 ${curIdx}, 1`);
    const wrappedIdx = m.freshReg();
    m.emit(`${wrappedIdx} = urem i64 ${nextIdx}, ${capReg}`);
    m.emit(`store i64 ${wrappedIdx}, i64* ${idxAlloca}`);
    m.emit(`br label %${probeL}`);

    // ── Empty slot: not found ──
    m.emitLabel(emptyL);
    m.emit(`br label %${exitL}`);

    // ── Exit ──
    m.emitLabel(exitL);
    const finalRes = m.freshReg();
    m.emit(`${finalRes} = load i64, i64* ${resAlloca}`);
    const boolRes = m.freshReg();
    m.emit(`${boolRes} = icmp ne i64 ${finalRes}, 0`);
    return { reg: boolRes, type: 'FACT' };
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
    if (m.usesMemset) lines.push('declare void @llvm.memset.p0i8.i64(i8*, i8, i64, i1)');
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

    // User-defined struct type declarations
    for (const declLine of this.structDeclLines) {
      lines.push(declLine);
    }
    if (this.structDeclLines.length) lines.push('');

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
      const lt = llvmType(info.plType);
      // For struct types, return the pointer itself (structs are pass-by-reference)
      if (lt && lt.startsWith('%struct.')) {
        return { reg: info.ptr, type: info.plType, ptr: info.ptr };
      }
      const reg = m.freshReg();
      m.emit(`${reg} = load ${lt}, ${lt}* ${info.ptr}`);
      // For array types, also pass through the memory pointer for mutation
      if (lt === '%fat_ptr') {
        return { reg, type: info.plType, ptr: info.ptr };
      }
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

    if (node.type === 'ArrayLiteral') {
      // Build array in arena and return a fat pointer
      const innerType = 'NUM'; // default inner type
      // For now, compile as RAW_EXPR since we need type info from variable
      // Instead, we handle arrays primarily through CREATE with genCreateArray
      return { reg: '0', type: 'NUM' };
    }

    if (node.type === 'LenCall') {
      const arg = this.compileAstExpr(node.arg);
      if (isArrayTypeStr(arg.type)) {
        const lenReg = m.extractLen(arg.reg);
        return { reg: lenReg, type: 'NUM' };
      }
      if (arg.type !== 'TX') {
        m.error(`len() requires a TX or array argument, got ${arg.type}`, node);
        return { reg: '0', type: 'NUM' };
      }
      const lenReg = m.extractLen(arg.reg);
      return { reg: lenReg, type: 'NUM' };
    }

    if (node.type === 'CapCall') {
      const arg = this.compileAstExpr(node.arg);
      if (isArrayTypeStr(arg.type)) {
        const capReg = m.extractCap(arg.reg);
        return { reg: capReg, type: 'NUM' };
      }
      if (arg.type !== 'TX') {
        m.error(`cap() requires a TX or array argument, got ${arg.type}`, node);
        return { reg: '0', type: 'NUM' };
      }
      const capReg = m.extractCap(arg.reg);
      return { reg: capReg, type: 'NUM' };
    }

    if (node.type === 'MethodCall') {
      const target = this.compileAstExpr(node.target);
      const targetPtr = target.ptr || target.reg;
      const targetType = target.type;

      // ── Intrinsic array methods: push and pop ───────────────────────
      if (isArrayTypeStr(targetType)) {
        const innerType = targetType.slice(1, -1);
        const innerLt = llvmType(innerType) || `%struct.${innerType}`;
        const innerSize = arrayElemSize(innerType);

        if (node.methodName === 'push') {
          // Compile the new element argument
          const ec = new ExprCompiler(m, node, this);
          let itemVal;
          if (node.args && node.args.length > 0) {
            if (typeof node.args[0] === 'string') {
              itemVal = ec.compileExpr(node.args[0]);
            } else {
              itemVal = this.compileAstExpr(node.args[0]);
            }
            itemVal = { reg: this.coerce(itemVal, innerType, node), type: innerType };
          } else {
            m.error('push requires an argument', node);
            return { reg: '0', type: targetType };
          }

          // target.reg is the loaded fat pointer value, target.ptr is %fat_ptr* in arena
          const fpReg = target.reg;
          const fpPtr = target.ptr; // %fat_ptr* for storing back
          const ptrReg = m.extractPtr(fpReg);
          const lenReg = m.extractLen(fpReg);
          const capReg = m.extractCap(fpReg);

          // Check if len == cap → need reallocation
          const needRealloc = m.freshReg();
          m.emit(`${needRealloc} = icmp eq i64 ${lenReg}, ${capReg}`);
          const reallocLabel = m.freshLabel('array.push.realloc');
          const skipLabel = m.freshLabel('array.push.append');
          m.emit(`br i1 ${needRealloc}, label %${reallocLabel}, label %${skipLabel}`);

          // ── Reallocation block ──
          m.emitLabel(reallocLabel);
          const isZero = m.freshReg();
          m.emit(`${isZero} = icmp eq i64 ${capReg}, 0`);
          const doubleCap = m.freshReg();
          m.emit(`${doubleCap} = mul i64 ${capReg}, 2`);
          const newCap = m.freshReg();
          m.emit(`${newCap} = select i1 ${isZero}, i64 8, i64 ${doubleCap}`);
          const newSize = m.freshReg();
          m.emit(`${newSize} = mul i64 ${newCap}, ${String(innerSize)}`);
          const newBuf = m.arenaAlloc(m.currentDepth, { reg: newSize, type: 'i64' });
          const oldSize = m.freshReg();
          m.emit(`${oldSize} = mul i64 ${lenReg}, ${String(innerSize)}`);
          m.emit(`call void @llvm.memcpy.p0i8.p0i8.i64(i8* ${newBuf}, i8* ${ptrReg}, i64 ${oldSize}, i1 false)`);
          const reallocFp = m.buildFatPtr(newBuf, lenReg, newCap);
          m.emit(`store %fat_ptr ${reallocFp}, %fat_ptr* ${fpPtr}`);
          m.emit(`br label %${skipLabel}`);

          // ── Append (common tail) ──
          m.emitLabel(skipLabel);
          // Reload fat pointer from memory (it may have been updated by realloc)
          const curFp = m.freshReg();
          m.emit(`${curFp} = load %fat_ptr, %fat_ptr* ${fpPtr}`);
          const finalPtr = m.extractPtr(curFp);
          const finalLen = m.extractLen(curFp);
          const finalCap = m.extractCap(curFp);
          const typedBuf = m.freshReg();
          m.emit(`${typedBuf} = bitcast i8* ${finalPtr} to ${innerLt}*`);
          const elemPtr = m.freshReg();
          m.emit(`${elemPtr} = getelementptr inbounds ${innerLt}, ${innerLt}* ${typedBuf}, i64 ${finalLen}`);
          m.emit(`store ${innerLt} ${itemVal.reg}, ${innerLt}* ${elemPtr}`);
          const newLen = m.freshReg();
          m.emit(`${newLen} = add i64 ${finalLen}, 1`);
          const resultFp = m.buildFatPtr(finalPtr, newLen, finalCap);
          m.emit(`store %fat_ptr ${resultFp}, %fat_ptr* ${fpPtr}`);
          return { reg: resultFp, type: targetType };
        }

        if (node.methodName === 'pop') {
          const fpReg = target.reg;
          const fpPtr = target.ptr;
          const lenReg = m.extractLen(fpReg);
          const ptrReg = m.extractPtr(fpReg);
          // Bounds check: len must be > 0
          const isZero = m.freshReg();
          m.emit(`${isZero} = icmp eq i64 ${lenReg}, 0`);
          const okLabel = m.freshLabel('array.pop.ok');
          const errLabel = m.freshLabel('array.pop.err');
          m.emit(`br i1 ${isZero}, label %${errLabel}, label %${okLabel}`);
          m.emitLabel(errLabel);
          m.ensureWeatherGlobals();
          const errMsg = m.addStringConstant('pop on empty array');
          const msgGep = m.freshReg();
          m.emit(`${msgGep} = getelementptr inbounds [${errMsg.len} x i8], [${errMsg.len} x i8]* ${errMsg.name}, i64 0, i64 0`);
          m.emit(`store i8* ${msgGep}, i8** @_weather_msg`);
          m.emit(`store i64 6, i64* @_weather_type`);
          m.emit(`store i1 true, i1* @_weather_flag`);
          m.emit(`br label %${okLabel}`);
          m.emitLabel(okLabel);
          const newLen = m.freshReg();
          m.emit(`${newLen} = sub i64 ${lenReg}, 1`);
          const typedBuf = m.freshReg();
          m.emit(`${typedBuf} = bitcast i8* ${ptrReg} to ${innerLt}*`);
          const elemPtr = m.freshReg();
          m.emit(`${elemPtr} = getelementptr inbounds ${innerLt}, ${innerLt}* ${typedBuf}, i64 ${newLen}`);
          const elemVal = m.freshReg();
          m.emit(`${elemVal} = load ${innerLt}, ${innerLt}* ${elemPtr}`);
          const capReg = m.extractCap(fpReg);
          const newFp = m.buildFatPtr(ptrReg, newLen, capReg);
          m.emit(`store %fat_ptr ${newFp}, %fat_ptr* ${fpPtr}`);
          const retLt = innerLt === 'double' ? 'double' : innerLt === 'i1' ? 'i1' : 'i64';
          const retType = innerLt === 'double' ? 'SCL' : innerLt === 'i1' ? 'FACT' : 'NUM';
          return { reg: elemVal, type: retType };
        }

        m.error(`Array type has no method "${node.methodName}"`, node);
        return { reg: '0', type: targetType };
      }

      // ── Intrinsic MAP methods: put, get, has ──────────────────────
      if (isMapTypeStr(targetType)) {
        if (node.methodName === 'put') {
          const ec = new ExprCompiler(m, node, this);
          let keyVal, valueVal;
          if (node.args && node.args.length >= 2) {
            if (typeof node.args[0] === 'string') keyVal = ec.compileExpr(node.args[0]);
            else keyVal = this.compileAstExpr(node.args[0]);
            if (typeof node.args[1] === 'string') valueVal = ec.compileExpr(node.args[1]);
            else valueVal = this.compileAstExpr(node.args[1]);
          } else {
            m.error('MAP put expects 2 arguments (key, value)', node);
            return { reg: '0', type: targetType };
          }
          this.genMapPut(node, targetType, targetPtr, keyVal, valueVal);
          return { reg: '0', type: 'NUM' };
        }
        if (node.methodName === 'has') {
          const ec = new ExprCompiler(m, node, this);
          let keyVal;
          if (node.args && node.args.length >= 1) {
            if (typeof node.args[0] === 'string') keyVal = ec.compileExpr(node.args[0]);
            else keyVal = this.compileAstExpr(node.args[0]);
          } else {
            m.error('MAP has expects 1 argument (key)', node);
            return { reg: '0', type: 'NUM' };
          }
          return this.genMapHas(node, targetType, target.reg, keyVal);
        }
        if (node.methodName === 'get') {
          // get returns Option<V> — not yet supported in compiled mode (needs MATCH)
          m.unsupported(node, `MAP get() not yet supported in compiled mode — use has() instead`);
          return { reg: '0', type: targetType };
        }
        m.error(`MAP type has no method "${node.methodName}"`, node);
        return { reg: '0', type: targetType };
      }

      // ── User-defined struct methods ─────────────────────────────
      const mangled = `${targetType}_${node.methodName}`;
      const fnInfo = this.fnInfos.get(mangled);
      if (!fnInfo) {
        m.error(`Method "${node.methodName}" not defined for type "${targetType}"`, node);
        return { reg: '0', type: 'NUM' };
      }
      // Compile arguments
      const ec = new ExprCompiler(m, node, this);
      const argVals = [];
      for (let i = 0; i < (node.args || []).length; i++) {
        let argVal;
        if (typeof node.args[i] === 'string') {
          argVal = ec.compileExpr(node.args[i]);
        } else {
          argVal = this.compileAstExpr(node.args[i]);
        }
        if (i < fnInfo.params.length) {
          const paramType = fnInfo.params[i].type;
          argVal = { reg: this.coerce(argVal, paramType, node), type: paramType };
        }
        argVals.push(argVal);
      }
      // Build call args: receiver ptr first, then explicit args
      const receiverLt = llvmType(targetType);
      const callArgs = [`${receiverLt}* ${targetPtr}`];
      for (let i = 0; i < argVals.length; i++) {
        const pt = i < fnInfo.params.length ? fnInfo.params[i].type : argVals[i].type;
        const lt = llvmType(pt) || 'i64';
        callArgs.push(`${lt} ${argVals[i].reg}`);
      }
      const resultReg = m.freshReg();
      m.emit(`${resultReg} = call i64 @${safeName(mangled)}(${callArgs.join(', ')})`);
      return { reg: resultReg, type: 'NUM' };
    }

    if (node.type === 'MemberAccess') {
      const objExpr = node.object;
      const objName = objExpr.name || objExpr.value;
      const objInfo = m.scope.get(objName);
      if (!objInfo) { m.error(`Cannot access member of unknown variable "${objName}"`, node); return { reg: '0', type: 'NUM' }; }
      const structName = objInfo.plType;
      const sInfo = this.structTypes.get(structName);
      if (!sInfo) { m.error(`"${structName}" is not a struct type`, node); return { reg: '0', type: 'NUM' }; }
      const actualIdx = this._getStructFieldIndex(structName, node.member);
      if (actualIdx === -1) { m.error(`Struct "${structName}" has no field "${node.member}"`, node); return { reg: '0', type: 'NUM' }; }
      const fieldType = sInfo.llvmFields[actualIdx];
      const fieldPtr = m.freshReg();
      m.emit(`${fieldPtr} = getelementptr inbounds ${sInfo.structType}, ${sInfo.structType}* ${objInfo.ptr}, i32 0, i32 ${actualIdx}`);
      const valReg = m.freshReg();
      m.emit(`${valReg} = load ${fieldType}, ${fieldType}* ${fieldPtr}`);
      const plType = fieldType === '%fat_ptr' ? 'TX' : fieldType === 'double' ? 'SCL' : fieldType === 'i1' ? 'FACT' : 'NUM';
      return { reg: valReg, type: plType };
    }

    if (node.type === 'IndexAccess') {
      const target = this.compileAstExpr(node.target);
      const idx = this.compileAstExpr(node.index);
      if (idx.type !== 'NUM') {
        m.error(`Index must be a NUM, got ${idx.type}`, node);
        return { reg: target.reg, type: target.type === 'TX' ? 'TX' : 'NUM' };
      }

      // Handle array type index access: arr[0] on [NUM], [TX], [Point], etc.
      if (isArrayTypeStr(target.type)) {
        const innerType = target.type.slice(1, -1);
        const innerLt = llvmType(innerType);
        if (!innerLt) {
          m.error(`Index access: unknown element type ${innerType}`, node);
          return { reg: '0', type: 'NUM' };
        }
        const ptr = m.extractPtr(target.reg);
        const lenReg = m.extractLen(target.reg);
        m.emitBoundsCheck(ptr, lenReg, idx.reg, node, 'array index access');
        const typedBuf = m.freshReg();
        m.emit(`${typedBuf} = bitcast i8* ${ptr} to ${innerLt}*`);
        const elemPtr = m.freshReg();
        m.emit(`${elemPtr} = getelementptr inbounds ${innerLt}, ${innerLt}* ${typedBuf}, i64 ${idx.reg}`);
        const elemVal = m.freshReg();
        m.emit(`${elemVal} = load ${innerLt}, ${innerLt}* ${elemPtr}`);
        const plType = innerType;
        return { reg: elemVal, type: plType };
      }

      if (target.type !== 'TX') {
        m.error(`Index access requires a TX or array target, got ${target.type}`, node);
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
      case 'StructDeclaration':
      case 'VariantDeclaration':
        return false;

      case 'CreateStatement': this.genCreate(node); return false;
      case 'SetStatement':     this.genSet(node);   return false;
      case 'IncreaseStatement':this.genIncDec(node, '+'); return false;
      case 'DecreaseStatement':this.genIncDec(node, '-'); return false;
      case 'ShowStatement':    this.genShow(node);  return false;
      case 'IfStatement':      this.genIf(node);    return false;
      case 'CycleStatement':   this.genCycle(node); return false;
      case 'SeasonStatement':  this.genSeason(node);return false;
      case 'ForInStatement':   this.genForIn(node); return false;
      case 'LockStatement':    return false;

      case 'LinkStatement': {
        const mapInfo = m.scope.get(node.mapIdent);
        if (!mapInfo) { m.error(`"${node.mapIdent}" is not defined`, node); return false; }
        if (!isMapTypeStr(mapInfo.plType)) { m.error(`"${node.mapIdent}" is not a MAP`, node); return false; }
        const ec = new ExprCompiler(m, node, this);
        const keyVal = ec.compileExpr(node.keyExpr);
        const valueVal = ec.compileExpr(node.valueExpr);
        // Coerce key to map key type
        const mt = mapInnerTypes(mapInfo.plType);
        const coercedKey = { reg: this.coerce(keyVal, mt.keyType, node), type: mt.keyType };
        const coercedVal = { reg: this.coerce(valueVal, mt.valueType, node), type: mt.valueType };
        this.genMapPut(node, mapInfo.plType, mapInfo.ptr, coercedKey, coercedVal);
        return false;
      }

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
    // Check if this is a struct type
    const structInfo = this.structTypes.get(node.varType);
    if (structInfo) {
      this.genCreateStruct(node, structInfo);
      return;
    }

    // Check if this is an array type [NUM], [TX], [Point], etc.
    if (isArrayTypeStr(node.varType)) {
      this.genCreateArray(node);
      return;
    }

    // Check if this is a MAP type MAP[NUM,TX], etc.
    if (isMapTypeStr(node.varType)) {
      this.genCreateMap(node);
      return;
    }

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
    } else if (ve && ve.type === 'ArrayLiteral') {
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
    // Struct member access: SET obj.field TO val
    if (node.isMemberAccess) {
      const objInfo = m.scope.get(node.memberObject);
      if (!objInfo) { m.error(`SET: "${node.memberObject}" was not declared`, node); return; }
      const structName = objInfo.plType;
      const sInfo = this.structTypes.get(structName);
      if (!sInfo) { m.error(`SET: "${node.memberObject}" is not a struct`, node); return; }
      const fieldIdx = this._getStructFieldIndex(structName, node.memberField);
      if (fieldIdx === -1) { m.error(`SET: struct "${structName}" has no field "${node.memberField}"`, node); return; }
      const fieldType = sInfo.llvmFields[fieldIdx];
      const fieldPtr = m.freshReg();
      m.emit(`${fieldPtr} = getelementptr inbounds ${sInfo.structType}, ${sInfo.structType}* ${objInfo.ptr}, i32 0, i32 ${fieldIdx}`);
      let val;
      const ve = node.valueExpr;
      if (typeof ve === 'string') {
        const ec = new ExprCompiler(m, node, this);
        val = ec.compileExpr(ve);
      } else {
        val = this.compileAstExpr(ve);
      }
      const fieldPlType = fieldType === '%fat_ptr' ? 'TX' : fieldType === 'double' ? 'SCL' : fieldType === 'i1' ? 'FACT' : 'NUM';
      const coerced = this.coerce(val, fieldPlType, node);
      m.emit(`store ${fieldType} ${coerced}, ${fieldType}* ${fieldPtr}`);
      return;
    }

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
      // Struct types — show as unsupported for now (LLVM can't easily print arbitrary structs)
      if (lt && lt.startsWith('%struct.')) {
        m.unsupported(node, `SHOW on struct type "${info.plType}"`);
        return;
      }
      const reg = m.freshReg();
      m.emit(`${reg} = load ${lt}, ${lt}* ${info.ptr}`);
      this.emitPrintValue({ reg, type: info.plType });
      return;
    }
    if (expr.type === 'MemberAccess') {
      const val = this.compileAstExpr(expr);
      this.emitPrintValue(val);
      return;
    }
    if (expr.type === 'Literal' && expr.literalType === 'RAW_EXPR') {
      const ec = new ExprCompiler(m, node, this);
      const val = ec.compileExpr(expr.value);
      this.emitPrintValue(val);
      return;
    }
    if (expr.type === 'LenCall' || expr.type === 'CapCall' || expr.type === 'IndexAccess' || expr.type === 'MethodCall') {
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

  // ── FOR ... IN (unified iteration) ─────────────────────────────────
  genForIn(node) {
    const m = this.mod;
    const srcInfo = m.scope.get(node.sourceExpr);
    if (!srcInfo) { m.error(`"${node.sourceExpr}" is not defined`, node); return; }
    const plType = srcInfo.plType;

    let elemPlType;
    if (isArrayTypeStr(plType)) {
      elemPlType = arrayInnerType(plType);
    } else if (isMapTypeStr(plType)) {
      elemPlType = mapInnerTypes(plType).valueType;
    } else {
      m.error(`FOR IN: "${node.sourceExpr}" must be [T] or MAP[K,V]`, node);
      return;
    }
    const elemLt = llvmType(elemPlType);

    const loopVarDepth = node.depth !== undefined ? node.depth : m.currentDepth;
    const loopVarPtr = m.arenaAllocTyped(elemPlType, loopVarDepth);
    m.scope.set(node.iterVar, { ptr: loopVarPtr, plType: elemPlType, depth: loopVarDepth });

    const fpReg = m.freshReg();
    m.emit(`${fpReg} = load %fat_ptr, %fat_ptr* ${srcInfo.ptr}`);

    // Iterator state allocas: type_id, state_ptr, index, limit, value
    const itTypeA = m.freshReg(); m.emit(`${itTypeA} = alloca i64`);
    const itPtrA  = m.freshReg(); m.emit(`${itPtrA} = alloca i8*`);
    const itIdxA  = m.freshReg(); m.emit(`${itIdxA} = alloca i64`);
    const itLimA  = m.freshReg(); m.emit(`${itLimA} = alloca i64`);
    const itValA  = m.freshReg(); m.emit(`${itValA} = alloca ${elemLt}`);

    this.genIteratorInit(node, plType, fpReg, { typeA: itTypeA, ptrA: itPtrA, idxA: itIdxA, limA: itLimA });

    const condL = m.freshLabel('forin.cond');
    const bodyL = m.freshLabel('forin.body');
    const incL  = m.freshLabel('forin.inc');
    const endL  = m.freshLabel('forin.end');

    m.emit(`br label %${condL}`);
    m.emitLabel(condL);

    const doneReg = this.genIteratorNext(node, plType, { typeA: itTypeA, ptrA: itPtrA, idxA: itIdxA, limA: itLimA, valA: itValA, elemPlType });
    m.emit(`br i1 ${doneReg}, label %${endL}, label %${bodyL}`);

    // ── Body ──
    m.emitLabel(bodyL);
    const loadedVal = m.freshReg();
    m.emit(`${loadedVal} = load ${elemLt}, ${elemLt}* ${itValA}`);
    m.emit(`store ${elemLt} ${loadedVal}, ${elemLt}* ${loopVarPtr}`);

    // Iteration Breath: save arena offset before body
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
      const endDepth = m.currentDepth;
      for (let d = endDepth; d > loopVarDepth; d--) m.arenaResetDepth(d);
      m.emit(`store i64 ${savedOff}, i64* ${saveGep}`);
      m.currentDepth = loopVarDepth;
      m.emit(`br label %${incL}`);
    }

    m.emitLabel(incL);
    m.emit(`br label %${condL}`);
    m.emitLabel(endL);
  }

  genIteratorInit(node, plType, fpReg, iter) {
    const m = this.mod;
    const { typeA, ptrA, idxA, limA } = iter;
    if (isArrayTypeStr(plType)) {
      m.emit(`store i64 0, i64* ${typeA}`);
      const dataPtr = m.extractPtr(fpReg);
      m.emit(`store i8* ${dataPtr}, i8** ${ptrA}`);
      m.emit(`store i64 0, i64* ${idxA}`);
      const len = m.extractLen(fpReg);
      m.emit(`store i64 ${len}, i64* ${limA}`);
    } else {
      m.emit(`store i64 1, i64* ${typeA}`);
      const bktPtr = m.extractPtr(fpReg);
      m.emit(`store i8* ${bktPtr}, i8** ${ptrA}`);
      m.emit(`store i64 0, i64* ${idxA}`);
      const cap = m.extractCap(fpReg);
      m.emit(`store i64 ${cap}, i64* ${limA}`);
    }
  }

  genIteratorNext(node, plType, iter) {
    const m = this.mod;
    const { typeA, ptrA, idxA, limA, valA, elemPlType } = iter;
    const elemLt = llvmType(elemPlType);

    const doneA = m.freshReg();
    m.emit(`${doneA} = alloca i1`);
    m.emit(`store i1 true, i1* ${doneA}`);

    const itT = m.freshReg();
    m.emit(`${itT} = load i64, i64* ${typeA}`);
    const isArr = m.freshReg();
    m.emit(`${isArr} = icmp eq i64 ${itT}, 0`);

    const arrL  = m.freshLabel('forin.next.arr');
    const mapL  = m.freshLabel('forin.next.map');
    const contL = m.freshLabel('forin.next.cont');
    m.emit(`br i1 ${isArr}, label %${arrL}, label %${mapL}`);

    // ── Array path ──
    m.emitLabel(arrL);
    const ai = m.freshReg();
    m.emit(`${ai} = load i64, i64* ${idxA}`);
    const al = m.freshReg();
    m.emit(`${al} = load i64, i64* ${limA}`);
    const aDone = m.freshReg();
    m.emit(`${aDone} = icmp slt i64 ${ai}, ${al}`);
    const aLoadL = m.freshLabel('forin.next.arrload');
    const aSkipL = m.freshLabel('forin.next.arrskip');
    m.emit(`br i1 ${aDone}, label %${aLoadL}, label %${aSkipL}`);

    m.emitLabel(aLoadL);
    const ap = m.freshReg();
    m.emit(`${ap} = load i8*, i8** ${ptrA}`);
    const aBuf = m.freshReg();
    m.emit(`${aBuf} = bitcast i8* ${ap} to ${elemLt}*`);
    const aEp = m.freshReg();
    m.emit(`${aEp} = getelementptr inbounds ${elemLt}, ${elemLt}* ${aBuf}, i64 ${ai}`);
    const aEv = m.freshReg();
    m.emit(`${aEv} = load ${elemLt}, ${elemLt}* ${aEp}`);
    m.emit(`store ${elemLt} ${aEv}, ${elemLt}* ${valA}`);
    // Increment index for next call
    const aNi = m.freshReg();
    m.emit(`${aNi} = add i64 ${ai}, 1`);
    m.emit(`store i64 ${aNi}, i64* ${idxA}`);
    m.emit(`store i1 false, i1* ${doneA}`);
    m.emit(`br label %${contL}`);

    m.emitLabel(aSkipL);
    m.emit(`br label %${contL}`);

    // ── MAP path ──
    m.emitLabel(mapL);
    const mt = mapInnerTypes(plType);
    const bktType = mapBucketLlvmType(mt.keyType, mt.valueType);
    const bktSz = mapBucketSize(mt.keyType, mt.valueType);
    const vLt = llvmType(mt.valueType);

    const scanL  = m.freshLabel('forin.next.map.scan');
    const checkL = m.freshLabel('forin.next.map.check');
    const foundL = m.freshLabel('forin.next.map.found');
    const advL   = m.freshLabel('forin.next.map.advance');
    const exhL   = m.freshLabel('forin.next.map.exhausted');
    m.emit(`br label %${scanL}`);

    m.emitLabel(scanL);
    const mi = m.freshReg();
    m.emit(`${mi} = load i64, i64* ${idxA}`);
    const ml = m.freshReg();
    m.emit(`${ml} = load i64, i64* ${limA}`);
    const mScanDone = m.freshReg();
    m.emit(`${mScanDone} = icmp slt i64 ${mi}, ${ml}`);
    m.emit(`br i1 ${mScanDone}, label %${checkL}, label %${exhL}`);

    m.emitLabel(checkL);
    const mp = m.freshReg();
    m.emit(`${mp} = load i8*, i8** ${ptrA}`);
    const mOff = m.freshReg();
    m.emit(`${mOff} = mul i64 ${mi}, ${bktSz}`);
    const mRaw = m.freshReg();
    m.emit(`${mRaw} = getelementptr inbounds i8, i8* ${mp}, i64 ${mOff}`);
    const mBkt = m.freshReg();
    m.emit(`${mBkt} = bitcast i8* ${mRaw} to ${bktType}*`);
    const mOccP = m.freshReg();
    m.emit(`${mOccP} = getelementptr inbounds ${bktType}, ${bktType}* ${mBkt}, i64 0, i32 0`);
    const mOcc = m.freshReg();
    m.emit(`${mOcc} = load i1, i1* ${mOccP}`);
    m.emit(`br i1 ${mOcc}, label %${foundL}, label %${advL}`);

    m.emitLabel(foundL);
    const mValP = m.freshReg();
    m.emit(`${mValP} = getelementptr inbounds ${bktType}, ${bktType}* ${mBkt}, i64 0, i32 2`);
    const mVal = m.freshReg();
    m.emit(`${mVal} = load ${vLt}, ${vLt}* ${mValP}`);
    m.emit(`store ${vLt} ${mVal}, ${vLt}* ${valA}`);
    // Advance index past this bucket
    const mNi = m.freshReg();
    m.emit(`${mNi} = add i64 ${mi}, 1`);
    m.emit(`store i64 ${mNi}, i64* ${idxA}`);
    m.emit(`store i1 false, i1* ${doneA}`);
    m.emit(`br label %${contL}`);

    // Unoccupied: advance index, continue scan
    m.emitLabel(advL);
    const mNext = m.freshReg();
    m.emit(`${mNext} = add i64 ${mi}, 1`);
    m.emit(`store i64 ${mNext}, i64* ${idxA}`);
    m.emit(`br label %${scanL}`);

    // Exhausted: all buckets scanned
    m.emitLabel(exhL);
    m.emit(`br label %${contL}`);

    // ── Continue ──
    m.emitLabel(contL);
    const done = m.freshReg();
    m.emit(`${done} = load i1, i1* ${doneA}`);
    return done;
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
