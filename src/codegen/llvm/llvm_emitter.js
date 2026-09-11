'use strict';

const { LLVMContext } = require('./llvm_context');
const { LLVMSymbolTable } = require('./llvm_symbol_table');
const { toLLVMType, getPrintFunction, isIntegerType, isFloatType, llvmTypeOf } = require('./llvm_type_mapper');

class LLVMEmitter {
    constructor() {
        this.ctx = new LLVMContext();
        this.symbols = new LLVMSymbolTable(null, this.ctx);
        this._funcTable = new Map();
        this._funcDefs = [];
    }

    // ═══════════════════════════════════════════════════════════════════
    //  Public entry point  —  single pass: declare + emit simultaneously
    // ═══════════════════════════════════════════════════════════════════
    generate(programNode) {
        this.ctx.addDeclare('void', 'plnt_print_int', ['i64']);
        this.ctx.addDeclare('void', 'plnt_print_decimal', ['double']);
        this.ctx.addDeclare('void', 'plnt_print_bool', ['i1']);
        this.ctx.addDeclare('void', 'plnt_print_text', ['i8*']);

        this.ctx.addDeclare('i8*', 'plant_str_concat', ['i8*', 'i8*']);
        this.ctx.addDeclare('i64*', 'plant_array_create', ['i64']);
        this.ctx.addDeclare('i64', 'plant_array_get', ['i64*', 'i64']);
        this.ctx.addDeclare('void', 'plant_array_set', ['i64*', 'i64', 'i64']);
        this.ctx.addDeclare('void', 'plant_free', ['i8*']);

        /* ── v0.42.0: Map / Iterator / Domain ── */
        this.ctx.addDeclare('i8*', 'plant_map_create', ['i64']);
        this.ctx.addDeclare('void', 'plant_map_set', ['i8*', 'i8*', 'i8*']);
        this.ctx.addDeclare('i8*', 'plant_map_get', ['i8*', 'i8*']);
        this.ctx.addDeclare('i8*', 'plant_map_keys', ['i8*', 'i64*']);
        this.ctx.addDeclare('void', 'plant_map_free', ['i8*']);
        this.ctx.addDeclare('void', 'plant_sys_action', ['i8*', 'i8*']);
        this.ctx.addDeclare('void', 'plant_env_set_weather', ['i8*']);
        this.ctx.addDeclare('i8*', 'plant_env_get_weather', []);
        this.ctx.addDeclare('void', 'plant_entity_set_species', ['i8*', 'i8*']);

        this.ctx.resetRegs();
        this._bodyBuffer = [];
        this.symbols = new LLVMSymbolTable(null, this.ctx);
        this._funcTable = new Map();
        this._funcDefs = [];

        const funcDecls = programNode.statements.filter(s => s.type === 'ActionDeclaration');
        const mainStmts = programNode.statements.filter(s => s.type !== 'ActionDeclaration');

        for (const decl of funcDecls) {
            this._registerFunction(decl);
        }

        this._emitBody(mainStmts);

        if (!this.ctx.isTerminated) {
            this._emitLine('  ret i32 0');
        }

        for (const decl of funcDecls) {
            const fib = this._emitFunctionDefinition(decl);
            if (fib) this._funcDefs.push(fib);
        }

        const prologue = this.ctx.emitPrologue();
        const allocas = this.ctx.emitAllocas();
        const body = this._bodyBuffer.join('\n');

        let result = `${prologue}

define i32 @main() {
${allocas}
${body}
}
`;
        for (const fd of this._funcDefs) {
            result += '\n' + fd;
        }

        return result;
    }

    _registerFunction(decl) {
        if (decl.isExternal) return;
        const paramTypes = decl.params.map(p => toLLVMType(p.type || 'NUM'));
        this._funcTable.set(decl.name, { retType: 'i64', paramTypes });
    }

    // ═══════════════════════════════════════════════════════════════════
    //  Helpers — body buffer & label emission
    // ═══════════════════════════════════════════════════════════════════
    _emitLine(line) {
        if (this.ctx.isTerminated) return;
        this._bodyBuffer.push(line);
    }

    _emitLabel(label) {
        this._bodyBuffer.push(`${label}:`);
        this.ctx.resetTerminated();
    }

    _emitBr(label) {
        this._emitLine(`  br label %${label}`);
        this.ctx.markTerminated();
    }

    _emitCondBr(cond, trueLabel, falseLabel) {
        const condStr = typeof cond === 'object' ? cond.reg : cond;
        this._emitLine(`  br i1 ${condStr}, label %${trueLabel}, label %${falseLabel}`);
        this.ctx.markTerminated();
    }

    _emitHeapCleanup() {
        for (const name of this.symbols.heapVars) {
            const info = this.symbols.variables.get(name);
            if (!info) continue;
            const tmp = this.ctx.nextReg();
            this._emitLine(`  ${tmp} = load ${info.llvmType}, ${info.llvmType}* ${info.alloca}`);
            if (info.llvmType === 'i64*') {
                const bc = this.ctx.nextReg();
                this._emitLine(`  ${bc} = bitcast i64* ${tmp} to i8*`);
                this._emitLine(`  call void @plant_free(i8* ${bc})`);
            } else {
                this._emitLine(`  call void @plant_free(i8* ${tmp})`);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  Statement emission
    // ═══════════════════════════════════════════════════════════════════
    _emitBody(stmts) {
        for (const stmt of stmts) {
            if (this.ctx.isTerminated) break;
            this._emitStatement(stmt);
        }
    }

    _emitStatement(stmt) {
        if (!stmt || this.ctx.isTerminated) return;
        switch (stmt.type) {
            case 'CreateStatement':   this._emitCreate(stmt); break;
            case 'SetStatement':      this._emitSet(stmt); break;
            case 'ShowStatement':     this._emitShow(stmt); break;
            case 'IfStatement':       this._emitIfStatement(stmt); break;
            case 'CycleStatement':    this._emitCycleStatement(stmt); break;
            case 'BreakStatement':    this._emitBreakStatement(); break;
            case 'ContinueStatement': this._emitContinueStatement(); break;
            case 'GiveStatement':     this._emitGiveStatement(stmt); break;
            case 'ReapStatement':     this._emitReapStatement(stmt); break;
            case 'SuiteStatement':    /* no-op in compiled output */ break;
            case 'VerifyStatement':   /* no-op in compiled output */ break;
            case 'HarvestStatement':  this._emitHarvestStatement(stmt); break;
            case 'ListenBranchStatement': this._emitListenBranchStatement(stmt); break;
            case 'LinkStatement':     this._emitLinkStatement(stmt); break;
            case 'ForInStatement':    this._emitForInStatement(stmt); break;
            case 'WeatherStatement':  this._emitWeatherStatement(stmt); break;
            case 'SpeciesDeclaration': this._emitSpeciesDeclaration(stmt); break;
        }
    }

    // ── CREATE — declare in current scope, register entry alloca, store initial value ──
    _emitCreate(stmt) {
        this.symbols.declare(stmt.identifier, stmt.varType || 'NUM');
        const info = this.symbols.lookup(stmt.identifier);
        if (!stmt.valueExpr) return;
        const val = this._emitExpressionNode(stmt.valueExpr);
        if (!val) { this._emitLine(`  ; CREATE ${stmt.identifier} — unsupported`); return; }
        const converted = this._maybeConvert(val, info.llvmType);
        this._emitLine(`  store ${converted.llvmType} ${converted.reg}, ${info.llvmType}* ${info.alloca}`);
        if (val.isHeap) {
            this.symbols.registerHeapVar(stmt.identifier);
        }
    }

    // ── SET ─────────────────────────────────────────────────────────
    _emitSet(stmt) {
        const info = this.symbols.lookup(stmt.identifier);
        if (!stmt.valueExpr) return;
        const val = this._emitExpressionNode(stmt.valueExpr);
        if (!val) { this._emitLine(`  ; SET ${stmt.identifier} — unsupported`); return; }
        if (val.isHeap && this.symbols.hasHeapVar(stmt.identifier)) {
            const tmp = this.ctx.nextReg();
            this._emitLine(`  ${tmp} = load ${info.llvmType}, ${info.llvmType}* ${info.alloca}`);
            if (info.llvmType === 'i64*') {
                const bc = this.ctx.nextReg();
                this._emitLine(`  ${bc} = bitcast i64* ${tmp} to i8*`);
                this._emitLine(`  call void @plant_free(i8* ${bc})`);
            } else {
                this._emitLine(`  call void @plant_free(i8* ${tmp})`);
            }
        }
        const converted = this._maybeConvert(val, info.llvmType);
        this._emitLine(`  store ${converted.llvmType} ${converted.reg}, ${info.llvmType}* ${info.alloca}`);
        if (val.isHeap) {
            this.symbols.registerHeapVar(stmt.identifier);
        }
    }

    // ── SHOW ────────────────────────────────────────────────────────
    _emitShow(stmt) {
        const val = this._emitExpressionNode(stmt.expr);
        if (!val) { this._emitLine('  ; SHOW — unsupported'); return; }
        const printFn = getPrintFunction(val.llvmType);
        if (!printFn) { this._emitLine(`  ; SHOW — no print fn for ${val.llvmType}`); return; }
        this._emitLine(`  call ${printFn.ret} @${printFn.name}(${printFn.args[0]} ${val.reg})`);
    }

    // ═══════════════════════════════════════════════════════════════════
    //  IF / ELSE IF / ELSE
    // ═══════════════════════════════════════════════════════════════════
    _emitIfStatement(stmt) {
        const branches = stmt.branches;
        const n = branches.length;
        const finalEndLabel = this.ctx.nextLabel('if.end');

        const entryLabels = branches.map((b, i) => {
            const prefix = (b.cond !== null && b.cond !== undefined && b.cond !== '') ? 'if.cond' : 'if.else';
            return this.ctx.nextLabel(`${prefix}.${i}`);
        });
        const bodyLabels = branches.map((b, i) => {
            if (b.cond !== null && b.cond !== undefined && b.cond !== '') return this.ctx.nextLabel(`if.then.${i}`);
            return null;
        });

        for (let i = 0; i < n; i++) {
            const branch = branches[i];
            const hasCond = branch.cond !== null && branch.cond !== undefined && branch.cond !== '';

            if (i > 0) this._emitLabel(entryLabels[i]);

            if (hasCond) {
                const falseLabel = i < n - 1 ? entryLabels[i + 1] : finalEndLabel;
                const condVal = this._emitExpressionNode(branch.cond);
                if (!condVal) { this._emitLine('  ; IF condition — unsupported'); break; }
                const boolCond = this._maybeConvert(condVal, 'i1');
                this._emitCondBr(boolCond, bodyLabels[i], falseLabel);

                this._emitLabel(bodyLabels[i]);
            }

            this.symbols = this.symbols.pushScope();
            this._emitBody(branch.bodyStatements);
            this._emitHeapCleanup();
            this.symbols = this.symbols.popScope();

            if (!this.ctx.isTerminated) {
                this._emitBr(finalEndLabel);
            }
        }

        this._emitLabel(finalEndLabel);
    }

    // ═══════════════════════════════════════════════════════════════════
    //  CYCLE (numeric FROM/TO)
    // ═══════════════════════════════════════════════════════════════════
    _emitCycleStatement(stmt) {
        if (stmt.fromExpr === null || stmt.toExpr === null) {
            this._emitLine('  ; CYCLE — only numeric FROM/TO supported');
            return;
        }

        const condLabel = this.ctx.nextLabel('loop.cond');
        const bodyLabel = this.ctx.nextLabel('loop.body');
        const incLabel = this.ctx.nextLabel('loop.inc');
        const endLabel = this.ctx.nextLabel('loop.end');

        if (!this.symbols.has(stmt.iterVar)) {
            this.symbols.declare(stmt.iterVar, 'NUM');
        }
        const iterInfo = this.symbols.lookup(stmt.iterVar);
        const iterAlloca = iterInfo.alloca;

        const fromVal = this._emitExpressionNode(stmt.fromExpr);
        if (!fromVal) { this._emitLine('  ; CYCLE FROM — unsupported'); return; }
        const fromConv = this._maybeConvert(fromVal, 'i64');
        this._emitLine(`  store i64 ${fromConv.reg}, i64* ${iterAlloca}`);
        this._emitBr(condLabel);

        this._emitLabel(condLabel);
        const currentReg = this.ctx.nextReg();
        this._emitLine(`  ${currentReg} = load i64, i64* ${iterAlloca}`);
        const toVal = this._emitExpressionNode(stmt.toExpr);
        if (!toVal) { this._emitLine('  ; CYCLE TO — unsupported'); return; }
        const toConv = this._maybeConvert(toVal, 'i64');
        const condReg = this.ctx.nextReg();
        this._emitLine(`  ${condReg} = icmp sle i64 ${currentReg}, ${toConv.reg}`);
        this._emitCondBr(condReg, bodyLabel, endLabel);

        this._emitLabel(bodyLabel);
        this.ctx.pushLoop(incLabel, endLabel);
        this.symbols = this.symbols.pushScope();
        this._emitBody(stmt.bodyStatements);
        this._emitHeapCleanup();
        this.symbols = this.symbols.popScope();
        this.ctx.popLoop();

        if (!this.ctx.isTerminated) {
            this._emitBr(incLabel);
        }

        this._emitLabel(incLabel);
        let stepReg;
        if (stmt.stepExpr) {
            const stepVal = this._emitExpressionNode(stmt.stepExpr);
            if (stepVal) {
                const stepConv = this._maybeConvert(stepVal, 'i64');
                stepReg = stepConv.reg;
            }
        }
        const incReg = this.ctx.nextReg();
        if (stepReg) {
            this._emitLine(`  ${incReg} = add i64 ${currentReg}, ${stepReg}`);
        } else {
            this._emitLine(`  ${incReg} = add i64 ${currentReg}, 1`);
        }
        this._emitLine(`  store i64 ${incReg}, i64* ${iterAlloca}`);
        this._emitBr(condLabel);

        this._emitLabel(endLabel);
    }

    // ── BREAK / CONTINUE ────────────────────────────────────────────
    _emitBreakStatement() {
        const loop = this.ctx.getCurrentLoop();
        if (!loop) { this._emitLine('  ; BREAK outside loop — ignored'); return; }
        this._emitBr(loop.endLabel);
    }

    _emitContinueStatement() {
        const loop = this.ctx.getCurrentLoop();
        if (!loop) { this._emitLine('  ; CONTINUE outside loop — ignored'); return; }
        this._emitBr(loop.condLabel);
    }

    // ═══════════════════════════════════════════════════════════════════
    //  GIVE — return from function
    // ═══════════════════════════════════════════════════════════════════
    _emitGiveStatement(stmt) {
        this._emitFunctionScopeCleanup();
        if (!stmt.valueExpr) {
            this._emitLine('  ret i64 0');
            this.ctx.markTerminated();
            return;
        }
        const val = this._emitExpressionNode(stmt.valueExpr);
        if (!val) { this._emitLine('  ret i64 0'); this.ctx.markTerminated(); return; }
        const conv = this._maybeConvert(val, 'i64');
        this._emitLine(`  ret i64 ${conv.reg}`);
        this.ctx.markTerminated();
    }

    _emitFunctionScopeCleanup() {
        const heapVars = this.symbols.collectHeapVars();
        for (const info of heapVars) {
            const tmp = this.ctx.nextReg();
            this._emitLine(`  ${tmp} = load ${info.llvmType}, ${info.llvmType}* ${info.alloca}`);
            if (info.llvmType === 'i64*') {
                const bc = this.ctx.nextReg();
                this._emitLine(`  ${bc} = bitcast i64* ${tmp} to i8*`);
                this._emitLine(`  call void @plant_free(i8* ${bc})`);
            } else {
                this._emitLine(`  call void @plant_free(i8* ${tmp})`);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  REAP — call a function (ACTION)
    // ═══════════════════════════════════════════════════════════════════
    _emitReapStatement(stmt) {
        if (stmt.source.kind !== 'ACTION') {
            this._emitLine(`  ; REAP — only ACTION calls supported`);
            return;
        }
        const funcInfo = this._funcTable.get(stmt.source.name);
        if (!funcInfo) {
            this._emitLine(`  ; REAP — unknown function "${stmt.source.name}"`);
            return;
        }
        const expectedCount = funcInfo.paramTypes.length;
        if (stmt.args.length !== expectedCount) {
            this._emitLine(`  ; REAP — "${stmt.source.name}" expects ${expectedCount} args, got ${stmt.args.length}`);
            return;
        }

        const argRegs = [];
        for (let i = 0; i < stmt.args.length; i++) {
            const argVal = this._emitExpressionNode(stmt.args[i]);
            if (!argVal) { this._emitLine(`  ; REAP arg ${i} — unsupported`); return; }
            const conv = this._maybeConvert(argVal, funcInfo.paramTypes[i]);
            argRegs.push(conv.reg);
        }

        const argList = argRegs.map((reg, i) => `${funcInfo.paramTypes[i]} ${reg}`).join(', ');
        const callReg = this.ctx.nextReg();
        this._emitLine(`  ${callReg} = call i64 @${stmt.source.name}(${argList})`);

        if (stmt.variable !== '_') {
            if (!this.symbols.has(stmt.variable)) {
                this.symbols.declare(stmt.variable, 'NUM');
            }
            const info = this.symbols.lookup(stmt.variable);
            this._emitLine(`  store i64 ${callReg}, i64* ${info.alloca}`);
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  Function definition (ACTION)
    // ═══════════════════════════════════════════════════════════════════
    _emitFunctionDefinition(decl) {
        if (decl.isExternal) return null;

        const savedState = this.ctx.saveState();
        const savedBuffer = this._bodyBuffer;
        const savedSymbols = this.symbols;

        this.ctx.resetFunctionState();
        this._bodyBuffer = [];
        this.symbols = new LLVMSymbolTable(null, this.ctx);

        const paramTypes = decl.params.map(p => toLLVMType(p.type || 'NUM'));
        const paramNames = decl.params.map(p => `%${p.name}`);
        const sigParams = paramNames.map((n, i) => `${paramTypes[i]} ${n}`).join(', ');
        this._emitLine(`define i64 @${decl.name}(${sigParams}) {`);

        for (let i = 0; i < decl.params.length; i++) {
            const param = decl.params[i];
            this.symbols.declare(param.name, param.type || 'NUM');
            const info = this.symbols.lookup(param.name);
            this._emitLine(`  store ${paramTypes[i]} ${paramNames[i]}, ${info.llvmType}* ${info.alloca}`);
        }

        this._emitBody(decl.bodyStatements);

        if (!this.ctx.isTerminated) {
            this._emitFunctionScopeCleanup();
            this._emitLine('  ret i64 0');
        }

        this._bodyBuffer.push('}');

        const funcEntryAllocas = this.ctx.emitAllocas();
        const funcBody = this._bodyBuffer.join('\n');

        let funcIR;
        if (funcEntryAllocas) {
            const lines = funcBody.split('\n');
            lines.splice(1, 0, funcEntryAllocas);
            funcIR = lines.join('\n');
        } else {
            funcIR = funcBody;
        }

        this._bodyBuffer = savedBuffer;
        this.symbols = savedSymbols;
        this.ctx.restoreState(savedState);

        return funcIR;
    }

    // ═══════════════════════════════════════════════════════════════════
    //  Expression nodes
    // ═══════════════════════════════════════════════════════════════════
    _emitExpressionNode(node) {
        if (!node) return null;
        if (typeof node === 'string') return this._emitRawExpr(node);
        switch (node.type) {
            case 'Literal':      return this._emitLiteral(node);
            case 'Identifier':   return this._emitIdentifier(node);
            case 'ArrayLiteral': return this._emitArrayLiteral(node);
            case 'MapLiteral':   return this._emitMapLiteral(node);
            case 'IndexAccess':  return this._emitIndexAccess(node);
            default:             return null;
        }
    }

    _emitLiteral(node) {
        if (node.literalType === 'NUMBER') {
            const isDecimal = typeof node.value === 'number' && !Number.isInteger(node.value);
            if (isDecimal) {
                const reg = this.ctx.nextReg();
                this._emitLine(`  ${reg} = fadd double ${doubleToLLVM(node.value)}, 0.0`);
                return { reg, llvmType: 'double' };
            }
            const reg = this.ctx.nextReg();
            this._emitLine(`  ${reg} = add i64 ${Math.trunc(Number(node.value))}, 0`);
            return { reg, llvmType: 'i64' };
        }
        if (node.literalType === 'FACT') {
            const reg = this.ctx.nextReg();
            this._emitLine(`  ${reg} = add i1 0, ${node.value ? 'true' : 'false'}`);
            return { reg, llvmType: 'i1' };
        }
        if (node.literalType === 'STRING') return this._emitStringConstant(String(node.value));
        if (node.literalType === 'RAW_EXPR') return this._emitRawExpr(String(node.value));
        return null;
    }

    _emitStringConstant(str) {
        const globalName = this.ctx.getOrCreateStringConstant(str);
        const len = Buffer.byteLength(str, 'utf8') + 1;
        const reg = this.ctx.nextReg();
        this._emitLine(`  ${reg} = getelementptr [${len} x i8], [${len} x i8]* ${globalName}, i64 0, i64 0`);
        return { reg, llvmType: 'i8*' };
    }

    _emitIdentifier(node) {
        if (!this.symbols.has(node.name)) {
            return null;
        }
        const info = this.symbols.lookup(node.name);
        const reg = this.ctx.nextReg();
        this._emitLine(`  ${reg} = load ${info.llvmType}, ${info.llvmType}* ${info.alloca}`);
        return { reg, llvmType: info.llvmType };
    }

    // ── Array literal ──────────────────────────────────────────────
    _emitArrayLiteral(node) {
        const capacity = node.elements.length;
        const capReg = this.ctx.nextReg();
        this._emitLine(`  ${capReg} = add i64 ${capacity}, 0`);
        const arrReg = this.ctx.nextReg();
        this._emitLine(`  ${arrReg} = call i64* @plant_array_create(i64 ${capReg})`);
        for (let i = 0; i < node.elements.length; i++) {
            const elem = this._emitExpressionNode(node.elements[i]);
            if (!elem) continue;
            const conv = this._maybeConvert(elem, 'i64');
            const idxReg = this.ctx.nextReg();
            this._emitLine(`  ${idxReg} = add i64 ${i}, 0`);
            this._emitLine(`  call void @plant_array_set(i64* ${arrReg}, i64 ${idxReg}, i64 ${conv.reg})`);
        }
        return { reg: arrReg, llvmType: 'i64*', isHeap: true };
    }

    // ── Index access (read) ────────────────────────────────────────
    _emitIndexAccess(node) {
        const target = this._emitExpressionNode(node.target);
        if (!target) return null;
        const idx = this._emitExpressionNode(node.index);
        if (!idx) return null;
        const convIdx = this._maybeConvert(idx, 'i64');
        const reg = this.ctx.nextReg();
        if (target.llvmType === 'i64*') {
            this._emitLine(`  ${reg} = call i64 @plant_array_get(i64* ${target.reg}, i64 ${convIdx.reg})`);
        } else if (target.llvmType === 'i8*') {
            const charReg = this.ctx.nextReg();
            this._emitLine(`  ${charReg} = getelementptr i8, i8* ${target.reg}, i64 ${convIdx.reg}`);
            this._emitLine(`  ${reg} = load i8, i8* ${charReg}`);
            const zextReg = this.ctx.nextReg();
            this._emitLine(`  ${zextReg} = zext i8 ${reg} to i64`);
            return { reg: zextReg, llvmType: 'i64' };
        } else {
            return null;
        }
        return { reg, llvmType: 'i64' };
    }

    // ═══════════════════════════════════════════════════════════════════
    //  RAW_EXPR expression parser
    // ═══════════════════════════════════════════════════════════════════
    _emitRawExpr(text) {
        if (!text || text.trim() === '') return null;
        const trimmed = text.trim();

        if (/^-?\d+\.\d+$/.test(trimmed)) {
            const reg = this.ctx.nextReg();
            this._emitLine(`  ${reg} = fadd double ${doubleToLLVM(parseFloat(trimmed))}, 0.0`);
            return { reg, llvmType: 'double' };
        }
        if (/^-?\d+$/.test(trimmed)) {
            const reg = this.ctx.nextReg();
            this._emitLine(`  ${reg} = add i64 ${trimmed}, 0`);
            return { reg, llvmType: 'i64' };
        }
        if (/^(TRUE|FALSE)$/i.test(trimmed)) {
            const reg = this.ctx.nextReg();
            const val = trimmed.toUpperCase() === 'TRUE' ? 'true' : 'false';
            this._emitLine(`  ${reg} = add i1 0, ${val}`);
            return { reg, llvmType: 'i1' };
        }
        if (/^"[^"]*"$/.test(trimmed)) return this._emitStringConstant(trimmed.slice(1, -1));
        if (/^[a-zA-Z_]\w*$/.test(trimmed)) return this._emitIdentifier({ type: 'Identifier', name: trimmed });

        const tokens = this._tokenizeExpr(text);
        if (tokens.length === 0) return null;
        return this._parseExpr(tokens);
    }

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
                if (rest.startsWith(mw.kw)) { tokens.push({ type: mw.tok }); i += mw.kw.length; matched = true; break; }
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
                        tokens.push({ type: kw.tok }); i = nextIdx; kwMatched = true; break;
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

    // ── Recursive descent expression parser ─────────────────────────
    _parseExpr(tokens) {
        this._tp = tokens;
        this._tpos = 0;
        return this._parseOr();
    }

    _peek() { return this._tp[this._tpos] || { type: 'EOF' }; }
    _consume() { return this._tp[this._tpos++] || { type: 'EOF' }; }
    _expect(type) { const t = this._consume(); if (t.type !== type) throw new Error(`Expected ${type}, got ${t.type}`); return t; }

    // ── OR (short-circuit) ──────────────────────────────────────────
    _parseOr() {
        let left = this._parseAnd();
        while (this._peek().type === 'OR') {
            this._consume();
            const l = this._maybeConvert(left, 'i1');
            left = this._emitShortCircuit(l, () => {
                const r = this._parseAnd();
                return this._maybeConvert(r, 'i1');
            }, 'or');
        }
        return left;
    }

    // ── AND (short-circuit) ─────────────────────────────────────────
    _parseAnd() {
        let left = this._parseNot();
        while (this._peek().type === 'AND') {
            this._consume();
            const l = this._maybeConvert(left, 'i1');
            left = this._emitShortCircuit(l, () => {
                const r = this._parseNot();
                return this._maybeConvert(r, 'i1');
            }, 'and');
        }
        return left;
    }

    // ── Short-circuit AND/OR core ───────────────────────────────────
    _emitShortCircuit(lhs, rhsThunk, op) {
        const isAnd = op === 'and';
        const labelRhs = this.ctx.nextLabel(isAnd ? 'and.rhs' : 'or.rhs');
        const labelEnd = this.ctx.nextLabel(isAnd ? 'and.end' : 'or.end');

        const tempAlloca = this.ctx.nextAllocaName('sc_tmp');
        this.ctx.addEntryAlloca(tempAlloca, 'i1');

        this._emitLine(`  store i1 ${lhs.reg}, i1* ${tempAlloca}`);
        if (isAnd) {
            this._emitLine(`  br i1 ${lhs.reg}, label %${labelRhs}, label %${labelEnd}`);
        } else {
            this._emitLine(`  br i1 ${lhs.reg}, label %${labelEnd}, label %${labelRhs}`);
        }
        this.ctx.markTerminated();

        this._emitLabel(labelRhs);
        const rhs = rhsThunk();
        const rhsBool = this._maybeConvert(rhs, 'i1');
        this._emitLine(`  store i1 ${rhsBool.reg}, i1* ${tempAlloca}`);
        this._emitLine(`  br label %${labelEnd}`);
        this.ctx.markTerminated();

        this._emitLabel(labelEnd);
        const resultReg = this.ctx.nextReg();
        this._emitLine(`  ${resultReg} = load i1, i1* ${tempAlloca}`);
        return { reg: resultReg, llvmType: 'i1' };
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
                this._emitLine(`  ${reg} = fadd double ${doubleToLLVM(parseFloat(tok.value))}, 0.0`);
                return { reg, llvmType: 'double' };
            }
            this._emitLine(`  ${reg} = add i64 ${tok.value}, 0`);
            return { reg, llvmType: 'i64' };
        }
        if (tok.type === 'TRUE') { this._consume(); const reg = this.ctx.nextReg(); this._emitLine(`  ${reg} = add i1 0, true`); return { reg, llvmType: 'i1' }; }
        if (tok.type === 'FALSE') { this._consume(); const reg = this.ctx.nextReg(); this._emitLine(`  ${reg} = add i1 0, false`); return { reg, llvmType: 'i1' }; }
        if (tok.type === 'STRING') { this._consume(); return this._emitStringConstant(tok.value); }
        if (tok.type === 'IDENT') { this._consume(); return this._emitIdentifier({ type: 'Identifier', name: tok.value }); }
        throw new Error(`Unexpected token ${tok.type}`);
    }

    // ═══════════════════════════════════════════════════════════════════
    //  LLVM IR operation emission
    // ═══════════════════════════════════════════════════════════════════
    _emitArithOp(left, right, op) {
        if (op === 'PLUS' && left.llvmType === 'i8*' && right.llvmType === 'i8*') {
            const reg = this.ctx.nextReg();
            this._emitLine(`  ${reg} = call i8* @plant_str_concat(i8* ${left.reg}, i8* ${right.reg})`);
            return { reg, llvmType: 'i8*', isHeap: true };
        }
        const p = this._promoteTypes(left, right);
        const l = p.left, r = p.right;
        const t = l.llvmType;
        const reg = this.ctx.nextReg();
        if (isIntegerType(t)) {
            const map = { PLUS: 'add', MINUS: 'sub', STAR: 'mul', SLASH: 'sdiv', PERCENT: 'srem' };
            this._emitLine(`  ${reg} = ${map[op] || 'add'} ${t} ${l.reg}, ${r.reg}`);
        } else {
            const map = { PLUS: 'fadd', MINUS: 'fsub', STAR: 'fmul', SLASH: 'fdiv', PERCENT: 'frem' };
            this._emitLine(`  ${reg} = ${map[op] || 'fadd'} ${t} ${l.reg}, ${r.reg}`);
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
            this._emitLine(`  ${reg} = icmp ${map[op] || 'eq'} ${t} ${l.reg}, ${r.reg}`);
        } else {
            const map = { IS: 'oeq', IS_NOT: 'one', GT: 'ogt', LT: 'olt', GTE: 'oge', LTE: 'ole' };
            this._emitLine(`  ${reg} = fcmp ${map[op] || 'oeq'} ${t} ${l.reg}, ${r.reg}`);
        }
        return { reg, llvmType: 'i1' };
    }

    _emitUnaryNot(val) {
        const v = this._maybeConvert(val, 'i1');
        const reg = this.ctx.nextReg();
        this._emitLine(`  ${reg} = xor i1 ${v.reg}, true`);
        return { reg, llvmType: 'i1' };
    }

    _emitUnaryMinus(val) {
        const t = val.llvmType;
        const reg = this.ctx.nextReg();
        if (isIntegerType(t)) {
            this._emitLine(`  ${reg} = sub ${t} 0, ${val.reg}`);
        } else {
            this._emitLine(`  ${reg} = fsub ${t} -0.0, ${val.reg}`);
        }
        return { reg, llvmType: t };
    }

    _emitPower(left, right) {
        const p = this._promoteTypes(left, right);
        const l = p.left, r = p.right;
        if (isIntegerType(l.llvmType)) {
            this.ctx.addDeclare('i64', 'plnt_pow_i64', ['i64', 'i64']);
            const reg = this.ctx.nextReg();
            this._emitLine(`  ${reg} = call i64 @plnt_pow_i64(i64 ${l.reg}, i64 ${r.reg})`);
            return { reg, llvmType: 'i64' };
        }
        this.ctx.addDeclare('double', 'pow', ['double', 'double']);
        const reg = this.ctx.nextReg();
        this._emitLine(`  ${reg} = call double @pow(double ${l.reg}, double ${r.reg})`);
        return { reg, llvmType: 'double' };
    }

    _promoteTypes(a, b) {
        if (a.llvmType === 'double' && b.llvmType === 'i64') return { left: a, right: this._intToDouble(b) };
        if (a.llvmType === 'i64' && b.llvmType === 'double') return { left: this._intToDouble(a), right: b };
        return { left: a, right: b };
    }

    _intToDouble(val) {
        const reg = this.ctx.nextReg();
        this._emitLine(`  ${reg} = sitofp i64 ${val.reg} to double`);
        return { reg, llvmType: 'double' };
    }

    _maybeConvert(val, targetType) {
        if (val.llvmType === targetType) return val;
        if (val.llvmType === 'i64' && targetType === 'double') return this._intToDouble(val);
        if (val.llvmType === 'double' && targetType === 'i64') {
            const reg = this.ctx.nextReg();
            this._emitLine(`  ${reg} = fptosi double ${val.reg} to i64`);
            return { reg, llvmType: 'i64' };
        }
        if (val.llvmType === 'i1' && targetType === 'i64') {
            const reg = this.ctx.nextReg();
            this._emitLine(`  ${reg} = zext i1 ${val.reg} to i64`);
            return { reg, llvmType: 'i64' };
        }
        if (val.llvmType === 'i64' && targetType === 'i1') {
            const reg = this.ctx.nextReg();
            this._emitLine(`  ${reg} = icmp ne i64 ${val.reg}, 0`);
            return { reg, llvmType: 'i1' };
        }
        return val;
    }

    // ═══════════════════════════════════════════════════════════════════
    //  HARVEST — v0.41.0 outbound HTTP request
    // ═══════════════════════════════════════════════════════════════════
    _emitHarvestStatement(stmt) {
        this.ctx.addDeclare('i8*', 'plant_net_harvest', ['i8*', 'i8*', 'i8*', 'i8*', 'i64']);

        const urlVal = this._emitExpressionNode(stmt.urlExpr);
        if (!urlVal) { this._emitLine('  ; HARVEST URL — unsupported'); return; }
        const urlStr = this._maybeConvert(urlVal, 'i8*');

        const methodStr = stmt.method
            ? this._emitStringConstant(stmt.method)
            : this._emitStringConstant('GET');

        let bodyStr = { reg: 'null', llvmType: 'i8*' };
        if (stmt.bodyExpr) {
            const bv = this._emitExpressionNode(stmt.bodyExpr);
            if (bv) bodyStr = this._maybeConvert(bv, 'i8*');
        }

        let headersStr = { reg: 'null', llvmType: 'i8*' };
        if (stmt.headersIdent) {
            const hv = this._emitIdentifier({ type: 'Identifier', name: stmt.headersIdent });
            if (hv) headersStr = this._maybeConvert(hv, 'i8*');
        }

        const timeoutVal = stmt.timeoutExpr
            ? this._maybeConvert(this._emitExpressionNode(stmt.timeoutExpr), 'i64')
            : { reg: '5', llvmType: 'i64' };

        const callReg = this.ctx.nextReg();
        this._emitLine(`  ${callReg} = call i8* @plant_net_harvest(i8* ${urlStr.reg}, i8* ${methodStr.reg}, i8* ${bodyStr.reg}, i8* ${headersStr.reg}, i64 ${timeoutVal.reg})`);

        if (stmt.resultIdent) {
            if (!this.symbols.has(stmt.resultIdent)) {
                this.symbols.declare(stmt.resultIdent, 'TX');
            }
            const info = this.symbols.lookup(stmt.resultIdent);
            this._emitLine(`  store i8* ${callReg}, i8** ${info.alloca}`);
            this.symbols.registerHeapVar(stmt.resultIdent);
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  LISTEN BRANCH — v0.41.0 TCP server listener
    // ═══════════════════════════════════════════════════════════════════
    _emitListenBranchStatement(stmt) {
        this.ctx.addDeclare('i64', 'plant_net_listen_open', ['i64']);
        this.ctx.addDeclare('i64', 'plant_net_accept', ['i64']);
        this.ctx.addDeclare('i8*', 'plant_net_read', ['i64']);
        this.ctx.addDeclare('i64', 'plant_net_write', ['i64', 'i8*']);
        this.ctx.addDeclare('void', 'plant_net_close', ['i64']);

        const portVal = this._emitExpressionNode(stmt.portExpr);
        if (!portVal) { this._emitLine('  ; LISTEN — port unsupported'); return; }
        const portI64 = this._maybeConvert(portVal, 'i64');

        const listenFd = this.ctx.nextReg();
        this._emitLine(`  ${listenFd} = call i64 @plant_net_listen_open(i64 ${portI64.reg})`);

        const loopLabel = this.ctx.nextLabel('listen.loop');
        const acceptLabel = this.ctx.nextLabel('listen.accept');
        const doneLabel = this.ctx.nextLabel('listen.done');

        this._emitLabel(loopLabel);
        const clientFd = this.ctx.nextReg();
        this._emitLine(`  ${clientFd} = call i64 @plant_net_accept(i64 ${listenFd})`);
        const cmpReg = this.ctx.nextReg();
        this._emitLine(`  ${cmpReg} = icmp slt i64 ${clientFd}, 0`);
        this._emitCondBr(cmpReg, doneLabel, acceptLabel);

        this._emitLabel(acceptLabel);
        if (stmt.requestIdent) {
            if (!this.symbols.has(stmt.requestIdent)) {
                this.symbols.declare(stmt.requestIdent, 'TX');
            }
            const reqInfo = this.symbols.lookup(stmt.requestIdent);
            const reqData = this.ctx.nextReg();
            this._emitLine(`  ${reqData} = call i8* @plant_net_read(i64 ${clientFd})`);
            this._emitLine(`  store i8* ${reqData}, i8** ${reqInfo.alloca}`);
            this.symbols.registerHeapVar(stmt.requestIdent);
        }

        this.symbols = this.symbols.pushScope();
        this._emitBody(stmt.bodyStatements);
        this._emitHeapCleanup();
        this.symbols = this.symbols.popScope();

        this._emitLine(`  call void @plant_net_close(i64 ${clientFd})`);
        this._emitBr(loopLabel);

        this._emitLabel(doneLabel);
        this._emitLine(`  call void @plant_net_close(i64 ${listenFd})`);
    }

    // ═══════════════════════════════════════════════════════════════════
    //  LINK — v0.42.0 map key-value insertion
    // ═══════════════════════════════════════════════════════════════════
    _emitLinkStatement(stmt) {
        const mapInfo = this.symbols.lookup(stmt.mapIdent);
        if (!mapInfo) { this._emitLine(`  ; LINK — unknown map "${stmt.mapIdent}"`); return; }
        const mapReg = this.ctx.nextReg();
        this._emitLine(`  ${mapReg} = load i8*, i8** ${mapInfo.alloca}`);

        const keyVal = this._emitExpressionNode(stmt.keyExpr);
        if (!keyVal) { this._emitLine('  ; LINK KEY — unsupported'); return; }
        const keyStr = this._maybeConvert(keyVal, 'i8*');

        const valExpr = this._emitExpressionNode(stmt.valueExpr);
        if (!valExpr) { this._emitLine('  ; LINK VALUE — unsupported'); return; }
        const valStr = this._maybeConvert(valExpr, 'i8*');

        this._emitLine(`  call void @plant_map_set(i8* ${mapReg}, i8* ${keyStr.reg}, i8* ${valStr.reg})`);
    }

    // ═══════════════════════════════════════════════════════════════════
    //  FOR...IN — v0.42.0 iteration over maps / arrays
    // ═══════════════════════════════════════════════════════════════════
    _emitForInStatement(stmt) {
        const sourceVal = this._emitExpressionNode(stmt.sourceExpr);
        if (!sourceVal) { this._emitLine('  ; FOR...IN source — unsupported'); return; }

        const loopLabel = this.ctx.nextLabel('forin.loop');
        const bodyLabel = this.ctx.nextLabel('forin.body');
        const endLabel  = this.ctx.nextLabel('forin.end');

        if (!this.symbols.has(stmt.iterVar)) {
            this.symbols.declare(stmt.iterVar, 'NUM');
        }
        const iterInfo = this.symbols.lookup(stmt.iterVar);

        /* Allocate index variable and initialize to 0 */
        const idxName = this.ctx.nextAllocaName('forin_idx');
        this.ctx.addEntryAlloca(idxName, 'i64');
        this._emitLine(`  store i64 0, i64* ${idxName}`);

        /* Get capacity */
        let capReg;
        if (sourceVal.llvmType === 'i64*') {
            capReg = this.ctx.nextReg();
            this._emitLine(`  ${capReg} = load i64, i64* ${sourceVal.reg}`);
        } else if (sourceVal.llvmType === 'i8*') {
            /* For map, we use a fixed iteration limit */
            capReg = { reg: '100', llvmType: 'i64' };
            this._emitLine(`  ; FOR...IN over MAP — using cap 100`);
        } else {
            this._emitLine('  ; FOR...IN — unsupported container type');
            return;
        }
        const capI64 = this._maybeConvert(capReg, 'i64');

        this._emitBr(loopLabel);
        this._emitLabel(loopLabel);
        const idxReg = this.ctx.nextReg();
        this._emitLine(`  ${idxReg} = load i64, i64* ${idxName}`);
        const condReg = this.ctx.nextReg();
        this._emitLine(`  ${condReg} = icmp slt i64 ${idxReg}, ${capI64.reg}`);
        this._emitCondBr(condReg, bodyLabel, endLabel);

        this._emitLabel(bodyLabel);
        /* Load element and store into iterVar */
        if (sourceVal.llvmType === 'i64*') {
            const elemReg = this.ctx.nextReg();
            this._emitLine(`  ${elemReg} = call i64 @plant_array_get(i64* ${sourceVal.reg}, i64 ${idxReg})`);
            this._emitLine(`  store i64 ${elemReg}, i64* ${iterInfo.alloca}`);
        } else if (sourceVal.llvmType === 'i8*') {
            const elemReg = this.ctx.nextReg();
            this._emitLine(`  ${elemReg} = call i8* @plant_map_get(i8* ${sourceVal.reg}, i8* null)`);
            this._emitLine(`  store i8* ${elemReg}, i8** ${iterInfo.alloca}`);
        }

        this.ctx.pushLoop(loopLabel, endLabel);
        this.symbols = this.symbols.pushScope();
        this._emitBody(stmt.bodyStatements);
        this._emitHeapCleanup();
        this.symbols = this.symbols.popScope();
        this.ctx.popLoop();

        if (!this.ctx.isTerminated) {
            const nextReg = this.ctx.nextReg();
            this._emitLine(`  ${nextReg} = add i64 ${idxReg}, 1`);
            this._emitLine(`  store i64 ${nextReg}, i64* ${idxName}`);
            this._emitBr(loopLabel);
        }

        this._emitLabel(endLabel);
    }

    // ═══════════════════════════════════════════════════════════════════
    //  WEATHER — v0.42.0 error-handling / environment set
    // ═══════════════════════════════════════════════════════════════════
    _emitWeatherStatement(stmt) {
        const bodyLabel = this.ctx.nextLabel('weather.body');
        const endLabel  = this.ctx.nextLabel('weather.end');

        /* Set weather type if condition expr is provided */
        if (stmt.conditionExpr) {
            const condVal = this._emitExpressionNode(stmt.conditionExpr);
            if (condVal) {
                const condStr = this._maybeConvert(condVal, 'i8*');
                this._emitLine(`  call void @plant_env_set_weather(i8* ${condStr.reg})`);
            }
        }

        this._emitBr(bodyLabel);
        this._emitLabel(bodyLabel);
        this.symbols = this.symbols.pushScope();
        this._emitBody(stmt.bodyStatements);
        this._emitHeapCleanup();
        this.symbols = this.symbols.popScope();
        this._emitBr(endLabel);
        this._emitLabel(endLabel);

        /* Process shelter clauses */
        if (stmt.shelterClauses) {
            for (const shelter of stmt.shelterClauses) {
                const shelterLabel = this.ctx.nextLabel('weather.shelter');
                this._emitLabel(shelterLabel);
                this.symbols = this.symbols.pushScope();
                this._emitBody(shelter.bodyStatements);
                this._emitHeapCleanup();
                this.symbols = this.symbols.popScope();
                this._emitBr(endLabel);
            }
        }

        /* Calm clause */
        if (stmt.calmClause && stmt.calmClause.bodyStatements) {
            const calmLabel = this.ctx.nextLabel('weather.calm');
            this._emitLabel(calmLabel);
            this.symbols = this.symbols.pushScope();
            this._emitBody(stmt.calmClause.bodyStatements);
            this._emitHeapCleanup();
            this.symbols = this.symbols.popScope();
            this._emitBr(endLabel);
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  SPECIES — v0.42.0 entity species declaration
    // ═══════════════════════════════════════════════════════════════════
    _emitSpeciesDeclaration(stmt) {
        const nameStr = this._emitStringConstant(stmt.name);
        this._emitLine(`  call void @plant_entity_set_species(i8* null, i8* ${nameStr.reg})`);
    }

    // ═══════════════════════════════════════════════════════════════════
    //  MapLiteral — v0.42.0 inline MAP construction
    // ═══════════════════════════════════════════════════════════════════
    _emitMapLiteral(node) {
        const initCap = node.entries ? node.entries.length : 0;
        const capReg = this.ctx.nextReg();
        const capVal = Math.max(initCap, 8);
        this._emitLine(`  ${capReg} = add i64 ${capVal}, 0`);
        const mapReg = this.ctx.nextReg();
        this._emitLine(`  ${mapReg} = call i8* @plant_map_create(i64 ${capReg})`);

        if (node.entries) {
            for (const entry of node.entries) {
                const keyVal = this._emitExpressionNode(entry.key);
                if (!keyVal) continue;
                const keyStr = this._maybeConvert(keyVal, 'i8*');

                const valExpr = this._emitExpressionNode(entry.value);
                if (!valExpr) continue;
                const valStr = this._maybeConvert(valExpr, 'i8*');

                this._emitLine(`  call void @plant_map_set(i8* ${mapReg}, i8* ${keyStr.reg}, i8* ${valStr.reg})`);
            }
        }
        return { reg: mapReg, llvmType: 'i8*', isHeap: true };
    }
}

function doubleToLLVM(val) {
    const buf = Buffer.alloc(8);
    buf.writeDoubleBE(val, 0);
    return `0x${buf.toString('hex').toUpperCase()}`;
}

module.exports = { LLVMEmitter };
