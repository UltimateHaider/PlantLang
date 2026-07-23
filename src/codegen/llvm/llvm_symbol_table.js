'use strict';

const { toLLVMType } = require('./llvm_type_mapper');

class LLVMSymbolTable {
    constructor() {
        this.variables = new Map();
        this.allocas = [];
    }

    declare(name, plantType) {
        if (this.variables.has(name)) {
            throw new Error(`Variable '${name}' already declared`);
        }
        const llvmType = toLLVMType(plantType);
        const alloca = `%${name}`;
        this.variables.set(name, { type: plantType, llvmType, alloca });
        this.allocas.push({ name, alloca, llvmType });
        return alloca;
    }

    lookup(name) {
        const entry = this.variables.get(name);
        if (!entry) {
            throw new Error(`Undefined variable '${name}'`);
        }
        return entry;
    }

    has(name) {
        return this.variables.has(name);
    }

    emitAllocas() {
        return this.allocas
            .map(e => `  ${e.alloca} = alloca ${e.llvmType}`)
            .join('\n');
    }
}

module.exports = { LLVMSymbolTable };
