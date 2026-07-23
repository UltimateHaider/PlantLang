'use strict';

const { LLVMContext } = require('./llvm_context');
const { LLVMSymbolTable } = require('./llvm_symbol_table');
const { toLLVMType, getPrintFunction, isIntegerType, isFloatType, llvmTypeOf } = require('./llvm_type_mapper');

class LLVMEmitter {
    constructor() {
        this.ctx = new LLVMContext();
        this.symbols = new LLVMSymbolTable();
    }

    generate(programNode) {
        this._collectDeclarations(programNode);

        this.ctx.addDeclare('void', 'plnt_print_int', ['i64']);
        this.ctx.addDeclare('void', 'plnt_print_decimal', ['double']);
        this.ctx.addDeclare('void', 'plnt_print_bool', ['i1']);
        this.ctx.addDeclare('void', 'plnt_print_text', ['i8*']);

        this.ctx.resetRegs();
        this._bodyBuffer = [];

        for (const stmt of programNode.statements) {
            this._emitStatement(stmt);
        }

        const prologue = this.ctx.emitPrologue();
        const allocas = this.symbols.emitAllocas();
        const body = this._bodyBuffer.join('\n');

        return `${prologue}

define i32 @main() {
${allocas}
${body}
  ret i32 0
}
`;
    }

    _collectDeclarations(programNode) {
        for (const stmt of programNode.statements) {
            if (stmt.type === 'CreateStatement') {
                this.symbols.declare(stmt.identifier, stmt.varType || 'NUM');
            }
        }
    }

    _emitStatement(stmt) {
        switch (stmt.type) {
            case 'CreateStatement': return this._emitCreate(stmt);
            case 'SetStatement':    return this._emitSet(stmt);
            case 'ShowStatement':   return this._emitShow(stmt);
        }
    }

    _emitCreate(stmt) {
        const info = this.symbols.lookup(stmt.identifier);
        const llvmType = info.llvmType;
        const alloca = info.alloca;
        if (!stmt.valueExpr) return;
        const val = this._emitExpressionNode(stmt.valueExpr);
        if (!val) {
            this._bodyBuffer.push(`  ; CREATE ${stmt.identifier} — unsupported expr`);
            return;
        }
        const converted = this._maybeConvert(val, llvmType);
        this._bodyBuffer.push(`  store ${converted.llvmType} ${converted.reg}, ${llvmType}* ${alloca}`);
    }

    _emitSet(stmt) {
        const info = this.symbols.lookup(stmt.identifier);
        const llvmType = info.llvmType;
        const alloca = info.alloca;
        if (!stmt.valueExpr) return;
        const val = this._emitExpressionNode(stmt.valueExpr);
        if (!val) {
            this._bodyBuffer.push(`  ; SET ${stmt.identifier} — unsupported expr`);
            return;
        }
        const converted = this._maybeConvert(val, llvmType);
        this._bodyBuffer.push(`  store ${converted.llvmType} ${converted.reg}, ${llvmType}* ${alloca}`);
    }

    _emitShow(stmt) {
        const val = this._emitExpressionNode(stmt.expr);
        if (!val) {
            this._bodyBuffer.push('  ; SHOW — unsupported expr');
            return;
        }
        const printFn = getPrintFunction(val.llvmType);
        if (!printFn) {
            this._bodyBuffer.push(`  ; SHOW — no print fn for ${val.llvmType}`);
            return;
        }
        this._bodyBuffer.push(`  call ${printFn.ret} @${printFn.name}(${printFn.args[0]} ${val.reg})`);
    }

    _emitExpressionNode(node) {
        if (!node) return null;
        if (typeof node === 'string') return this._emitRawExpr(node);
        switch (node.type) {
            case 'Literal':   return this._emitLiteral(node);
            case 'Identifier': return this._emitIdentifier(node);
            default:           return null;
        }
    }

    _emitLiteral(node) {
        if (node.literalType === 'NUMBER') {
            const isDecimal = typeof node.value === 'number' && !Number.isInteger(node.value);
            if (isDecimal) {
                const reg = this.ctx.nextReg();
                this._bodyBuffer.push(`  ${reg} = fadd double ${doubleToLLVM(node.value)}, 0.0`);
                return { reg, llvmType: 'double' };
            }
            const reg = this.ctx.nextReg();
            this._bodyBuffer.push(`  ${reg} = add i64 ${Math.trunc(Number(node.value))}, 0`);
            return { reg, llvmType: 'i64' };
        }

        if (node.literalType === 'FACT') {
            const reg = this.ctx.nextReg();
            this._bodyBuffer.push(`  ${reg} = add i1 0, ${node.value ? 'true' : 'false'}`);
            return { reg, llvmType: 'i1' };
        }

        if (node.literalType === 'STRING') {
            return this._emitStringConstant(String(node.value));
        }

        if (node.literalType === 'RAW_EXPR') {
            return this._emitRawExpr(String(node.value));
        }

        return null;
    }

    _emitStringConstant(str) {
        const globalName = this.ctx.getOrCreateStringConstant(str);
        const len = Buffer.byteLength(str, 'utf8') + 1;
        const reg = this.ctx.nextReg();
        this._bodyBuffer.push(`  ${reg} = getelementptr [${len} x i8], [${len} x i8]* ${globalName}, i64 0, i64 0`);
        return { reg, llvmType: 'i8*' };
    }

    _emitIdentifier(node) {
        if (!this.symbols.has(node.name)) {
            throw new Error(`Undefined variable '${node.name}'`);
        }
        const info = this.symbols.lookup(node.name);
        const reg = this.ctx.nextReg();
        this._bodyBuffer.push(`  ${reg} = load ${info.llvmType}, ${info.llvmType}* ${info.alloca}`);
        return { reg, llvmType: info.llvmType };
    }

    // ── RAW_EXPR expression parser ──────────────────────────────────
    _emitRawExpr(text) {
        if (!text || text.trim() === '') return null;
        const trimmed = text.trim();

        if (/^-?\d+\.\d+$/.test(trimmed)) {
            const reg = this.ctx.nextReg();
            this._bodyBuffer.push(`  ${reg} = fadd double ${doubleToLLVM(parseFloat(trimmed))}, 0.0`);
            return { reg, llvmType: 'double' };
        }
        if (/^-?\d+$/.test(trimmed)) {
            const reg = this.ctx.nextReg();
            this._bodyBuffer.push(`  ${reg} = add i64 ${trimmed}, 0`);
            return { reg, llvmType: 'i64' };
        }
        if (/^(TRUE|FALSE)$/i.test(trimmed)) {
            const reg = this.ctx.nextReg();
            const val = trimmed.toUpperCase() === 'TRUE' ? 'true' : 'false';
            this._bodyBuffer.push(`  ${reg} = add i1 0, ${val}`);
            return { reg, llvmType: 'i1' };
        }
        if (/^"[^"]*"$/.test(trimmed)) {
            return this._emitStringConstant(trimmed.slice(1, -1));
        }
        if (/^[a-zA-Z_]\w*$/.test(trimmed)) {
            return this._emitIdentifier({ type: 'Identifier', name: trimmed });
        }

        const tokens = this._tokenizeExpr(text);
        if (tokens.length === 0) return null;

        return this._parseExpr(tokens);
    }

    // ── Expression tokenizer ────────────────────────────────────────
    _tokenizeExpr(text) {
        const tokens = [];
        let i = 0;
        while (i < text.length) {
            if (/\s/.test(text[i])) { i++; continue; }
            if (/[0-9]/.test(text[i])) {
                let num = '';
                while (i < text.length && /[0-9.]/.test(text[i])) { num += text[i++]; }
                const dots = (num.match(/\./g) || []).length;
                if (dots > 1) throw new Error(`Invalid number: ${num}`);
                tokens.push({ type: 'NUMBER', value: num });
                continue;
            }
            if (text[i] === '"') {
                i++;
                let str = '';
                while (i < text.length && text[i] !== '"') {
                    if (text[i] === '\\' && i + 1 < text.length) { i++; str += text[i++]; }
                    else { str += text[i++]; }
                }
                if (i < text.length) i++;
                tokens.push({ type: 'STRING', value: str });
                continue;
            }

            const rest = text.slice(i).toUpperCase();
            const multiWord = [
                { kw: 'GREATER THAN OR EQUAL', tok: 'GTE' },
                { kw: 'LESS THAN OR EQUAL',    tok: 'LTE' },
                { kw: 'GREATER THAN', tok: 'GT' },
                { kw: 'LESS THAN',    tok: 'LT' },
                { kw: 'IS NOT',      tok: 'IS_NOT' },
                { kw: 'IS',           tok: 'IS' },
            ];
            let matched = false;
            for (const mw of multiWord) {
                if (rest.startsWith(mw.kw)) {
                    tokens.push({ type: mw.tok });
                    i += mw.kw.length;
                    matched = true;
                    break;
                }
            }
            if (matched) continue;

            const ch = text[i];
            if (ch === '(') { tokens.push({ type: 'LPAREN' }); i++; continue; }
            if (ch === ')') { tokens.push({ type: 'RPAREN' }); i++; continue; }
            if (ch === '+') { tokens.push({ type: 'PLUS' }); i++; continue; }
            if (ch === '-') { tokens.push({ type: 'MINUS' }); i++; continue; }
            if (ch === '*') {
                if (i + 1 < text.length && text[i + 1] === '*') { tokens.push({ type: 'POW' }); i += 2; continue; }
                tokens.push({ type: 'STAR' }); i++; continue;
            }
            if (ch === '/') { tokens.push({ type: 'SLASH' }); i++; continue; }
            if (ch === '%') { tokens.push({ type: 'PERCENT' }); i++; continue; }

            const singleKW = [
                { kw: 'AND', tok: 'AND' }, { kw: 'OR', tok: 'OR' }, { kw: 'NOT', tok: 'NOT' },
                { kw: 'TRUE', tok: 'TRUE' }, { kw: 'FALSE', tok: 'FALSE' },
            ];
            let kwMatched = false;
            for (const kw of singleKW) {
                if (rest.startsWith(kw.kw)) {
                    const nextIdx = i + kw.kw.length;
                    if (nextIdx >= text.length || /\s|[+\-*/().]/.test(text[nextIdx])) {
                        tokens.push({ type: kw.tok });
                        i = nextIdx;
                        kwMatched = true;
                        break;
                    }
                }
            }
            if (kwMatched) continue;

            if (/[a-zA-Z_]/.test(ch)) {
                let ident = '';
                while (i < text.length && /[a-zA-Z0-9_]/.test(text[i])) { ident += text[i++]; }
                tokens.push({ type: 'IDENT', value: ident });
                continue;
            }

            throw new Error(`Unexpected char '${ch}' at position ${i}`);
        }
        return tokens;
    }

    // ── Recursive descent parser ────────────────────────────────────
    _parseExpr(tokens) {
        this._tp = tokens;
        this._tpos = 0;
        return this._parseOr();
    }

    _peek() { return this._tp[this._tpos] || { type: 'EOF' }; }
    _consume() { return this._tp[this._tpos++] || { type: 'EOF' }; }
    _expect(type) {
        const t = this._consume();
        if (t.type !== type) throw new Error(`Expected ${type}, got ${t.type}`);
        return t;
    }

    _parseOr() {
        let left = this._parseAnd();
        while (this._peek().type === 'OR') { this._consume(); left = this._emitLogicOp(left, this._parseAnd(), 'or'); }
        return left;
    }

    _parseAnd() {
        let left = this._parseNot();
        while (this._peek().type === 'AND') { this._consume(); left = this._emitLogicOp(left, this._parseNot(), 'and'); }
        return left;
    }

    _parseNot() {
        if (this._peek().type === 'NOT') { this._consume(); return this._emitUnaryNot(this._parseNot()); }
        return this._parseComparison();
    }

    _parseComparison() {
        let left = this._parseAddSub();
        const cmpOps = new Set(['IS', 'IS_NOT', 'GT', 'LT', 'GTE', 'LTE']);
        if (cmpOps.has(this._peek().type)) {
            const op = this._consume().type;
            return this._emitComparison(left, this._parseAddSub(), op);
        }
        return left;
    }

    _parseAddSub() {
        let left = this._parseMulDiv();
        while (this._peek().type === 'PLUS' || this._peek().type === 'MINUS') {
            const op = this._consume().type;
            left = this._emitArithOp(left, this._parseMulDiv(), op);
        }
        return left;
    }

    _parseMulDiv() {
        let left = this._parseUnary();
        while (['STAR', 'SLASH', 'PERCENT'].includes(this._peek().type)) {
            const op = this._consume().type;
            left = this._emitArithOp(left, this._parseUnary(), op);
        }
        return left;
    }

    _parseUnary() {
        if (this._peek().type === 'MINUS') { this._consume(); return this._emitUnaryMinus(this._parseUnary()); }
        return this._parsePower();
    }

    _parsePower() {
        let left = this._parsePrimary();
        if (this._peek().type === 'POW') { this._consume(); left = this._emitPower(left, this._parseUnary()); }
        return left;
    }

    _parsePrimary() {
        const tok = this._peek();
        if (tok.type === 'LPAREN') { this._consume(); const v = this._parseOr(); this._expect('RPAREN'); return v; }
        if (tok.type === 'NUMBER') {
            this._consume();
            const reg = this.ctx.nextReg();
            if (tok.value.includes('.')) {
                this._bodyBuffer.push(`  ${reg} = fadd double ${doubleToLLVM(parseFloat(tok.value))}, 0.0`);
                return { reg, llvmType: 'double' };
            }
            this._bodyBuffer.push(`  ${reg} = add i64 ${tok.value}, 0`);
            return { reg, llvmType: 'i64' };
        }
        if (tok.type === 'TRUE') { this._consume(); const reg = this.ctx.nextReg(); this._bodyBuffer.push(`  ${reg} = add i1 0, true`); return { reg, llvmType: 'i1' }; }
        if (tok.type === 'FALSE') { this._consume(); const reg = this.ctx.nextReg(); this._bodyBuffer.push(`  ${reg} = add i1 0, false`); return { reg, llvmType: 'i1' }; }
        if (tok.type === 'STRING') { this._consume(); return this._emitStringConstant(tok.value); }
        if (tok.type === 'IDENT') { this._consume(); return this._emitIdentifier({ type: 'Identifier', name: tok.value }); }
        throw new Error(`Unexpected token ${tok.type}`);
    }

    // ── LLVM IR emission ───────────────────────────────────────────
    _emitArithOp(left, right, op) {
        const p = this._promoteTypes(left, right);
        const l = p.left, r = p.right;
        const t = l.llvmType;
        const reg = this.ctx.nextReg();

        if (isIntegerType(t)) {
            const map = { PLUS: 'add', MINUS: 'sub', STAR: 'mul', SLASH: 'sdiv', PERCENT: 'srem' };
            this._bodyBuffer.push(`  ${reg} = ${map[op] || 'add'} ${t} ${l.reg}, ${r.reg}`);
        } else {
            const map = { PLUS: 'fadd', MINUS: 'fsub', STAR: 'fmul', SLASH: 'fdiv', PERCENT: 'frem' };
            this._bodyBuffer.push(`  ${reg} = ${map[op] || 'fadd'} ${t} ${l.reg}, ${r.reg}`);
        }
        return { reg, llvmType: t };
    }

    _emitComparison(left, right, op) {
        const p = this._promoteTypes(left, right);
        const l = p.left, r = p.right;
        const t = l.llvmType;
        const reg = this.ctx.nextReg();

        if (isIntegerType(t)) {
            const map = { IS: 'eq', IS_NOT: 'ne', GT: 'sgt', LT: 'slt', GTE: 'sge', LTE: 'sle' };
            this._bodyBuffer.push(`  ${reg} = icmp ${map[op] || 'eq'} ${t} ${l.reg}, ${r.reg}`);
        } else {
            const map = { IS: 'oeq', IS_NOT: 'one', GT: 'ogt', LT: 'olt', GTE: 'oge', LTE: 'ole' };
            this._bodyBuffer.push(`  ${reg} = fcmp ${map[op] || 'oeq'} ${t} ${l.reg}, ${r.reg}`);
        }
        return { reg, llvmType: 'i1' };
    }

    _emitLogicOp(left, right, op) {
        const l = this._maybeConvert(left, 'i1');
        const r = this._maybeConvert(right, 'i1');
        const reg = this.ctx.nextReg();
        this._bodyBuffer.push(`  ${reg} = ${op} i1 ${l.reg}, ${r.reg}`);
        return { reg, llvmType: 'i1' };
    }

    _emitUnaryNot(val) {
        const v = this._maybeConvert(val, 'i1');
        const reg = this.ctx.nextReg();
        this._bodyBuffer.push(`  ${reg} = xor i1 ${v.reg}, true`);
        return { reg, llvmType: 'i1' };
    }

    _emitUnaryMinus(val) {
        const t = val.llvmType;
        const reg = this.ctx.nextReg();
        if (isIntegerType(t)) {
            this._bodyBuffer.push(`  ${reg} = sub ${t} 0, ${val.reg}`);
        } else {
            this._bodyBuffer.push(`  ${reg} = fsub ${t} -0.0, ${val.reg}`);
        }
        return { reg, llvmType: t };
    }

    _emitPower(left, right) {
        const p = this._promoteTypes(left, right);
        const l = p.left, r = p.right;
        if (isIntegerType(l.llvmType)) {
            this.ctx.addDeclare('i64', 'plnt_pow_i64', ['i64', 'i64']);
            const reg = this.ctx.nextReg();
            this._bodyBuffer.push(`  ${reg} = call i64 @plnt_pow_i64(i64 ${l.reg}, i64 ${r.reg})`);
            return { reg, llvmType: 'i64' };
        }
        this.ctx.addDeclare('double', 'pow', ['double', 'double']);
        const reg = this.ctx.nextReg();
        this._bodyBuffer.push(`  ${reg} = call double @pow(double ${l.reg}, double ${r.reg})`);
        return { reg, llvmType: 'double' };
    }

    _promoteTypes(a, b) {
        if (a.llvmType === 'double' && b.llvmType === 'i64') return { left: a, right: this._intToDouble(b) };
        if (a.llvmType === 'i64' && b.llvmType === 'double') return { left: this._intToDouble(a), right: b };
        return { left: a, right: b };
    }

    _intToDouble(val) {
        const reg = this.ctx.nextReg();
        this._bodyBuffer.push(`  ${reg} = sitofp i64 ${val.reg} to double`);
        return { reg, llvmType: 'double' };
    }

    _maybeConvert(val, targetType) {
        if (val.llvmType === targetType) return val;
        if (val.llvmType === 'i64' && targetType === 'double') return this._intToDouble(val);
        if (val.llvmType === 'double' && targetType === 'i64') {
            const reg = this.ctx.nextReg();
            this._bodyBuffer.push(`  ${reg} = fptosi double ${val.reg} to i64`);
            return { reg, llvmType: 'i64' };
        }
        if (val.llvmType === 'i1' && targetType === 'i64') {
            const reg = this.ctx.nextReg();
            this._bodyBuffer.push(`  ${reg} = zext i1 ${val.reg} to i64`);
            return { reg, llvmType: 'i64' };
        }
        if (val.llvmType === 'i64' && targetType === 'i1') {
            const reg = this.ctx.nextReg();
            this._bodyBuffer.push(`  ${reg} = icmp ne i64 ${val.reg}, 0`);
            return { reg, llvmType: 'i1' };
        }
        return val;
    }
}

function doubleToLLVM(val) {
    const buf = Buffer.alloc(8);
    buf.writeDoubleBE(val, 0);
    return `0x${buf.toString('hex').toUpperCase()}`;
}

module.exports = { LLVMEmitter };
