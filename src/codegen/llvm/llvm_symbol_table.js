'use strict';

const { toLLVMType } = require('./llvm_type_mapper');

class LLVMSymbolTable {
    constructor(parent = null, context = null) {
        this.parent = parent;
        this.context = context;
        this.variables = new Map();
    }

    declare(name, plantType) {
        if (this.variables.has(name)) {
            throw new Error(`Variable '${name}' already declared in this scope`);
        }
        const llvmType = toLLVMType(plantType);
        const alloca = this.context ? this.context.nextAllocaName(name) : `%${name}`;
        const entry = { type: plantType, llvmType, alloca };
        this.variables.set(name, entry);
        if (this.context) {
            this.context.addEntryAlloca(alloca, llvmType);
        }
        return alloca;
    }

    lookup(name) {
        const entry = this.variables.get(name);
        if (entry) return entry;
        if (this.parent) return this.parent.lookup(name);
        throw new Error(`Undefined variable '${name}'`);
    }

    has(name) {
        if (this.variables.has(name)) return true;
        if (this.parent) return this.parent.has(name);
        return false;
    }

    pushScope() {
        const child = new LLVMSymbolTable(this, this.context);
        return child;
    }

    popScope() {
        return this.parent;
    }
}

module.exports = { LLVMSymbolTable };
