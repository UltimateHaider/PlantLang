'use strict';

class LLVMContext {
    constructor() {
        this.regCounter = 1;
        this.stringPool = new Map();
        this.stringIndex = 0;
        this.declarations = new Set();
        this.emitDeclarations = [];
    }

    nextReg() {
        return `%${this.regCounter++}`;
    }

    peekReg() {
        return `%${this.regCounter}`;
    }

    getOrCreateStringConstant(str) {
        if (this.stringPool.has(str)) return this.stringPool.get(str);
        const idx = this.stringIndex++;
        const name = `@.str.${idx}`;
        this.stringPool.set(str, name);
        return name;
    }

    addDeclare(retType, name, paramTypes) {
        const sig = `declare ${retType} @${name}(${paramTypes.join(', ')})`;
        if (!this.declarations.has(sig)) {
            this.declarations.add(sig);
            this.emitDeclarations.push(sig);
        }
    }

    emitPrologue() {
        const parts = [
            '; ModuleID = \'PlantLang_Module\'',
            'target triple = "x86_64-pc-linux-gnu"',
            'target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"',
        ];
        if (this.emitDeclarations.length > 0) {
            parts.push('');
            parts.push(this.emitDeclarations.join('\n'));
        }
        if (this.stringPool.size > 0) {
            parts.push('');
            for (const [str, name] of this.stringPool) {
                const len = Buffer.byteLength(str, 'utf8');
                const escaped = str
                    .replace(/\\/g, '\\\\')
                    .replace(/"/g, '\\22')
                    .replace(/\n/g, '\\0a')
                    .replace(/\r/g, '\\0d')
                    .replace(/\t/g, '\\09');
                parts.push(`${name} = private unnamed_addr constant [${len + 1} x i8] c"${escaped}\\00"`);
            }
        }
        return parts.join('\n');
    }

    resetRegs() {
        this.regCounter = 1;
    }
}

module.exports = { LLVMContext };
