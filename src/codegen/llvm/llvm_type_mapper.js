'use strict';

const TYPE_MAP = {
    NUM: 'i64',
    INT: 'i64',
    SCL: 'double',
    DECIMAL: 'double',
    FACT: 'i1',
    BOOL: 'i1',
    TX: 'i8*',
    TEXT: 'i8*',
};

const PRINT_FUNCTIONS = {
    i64:     { name: 'plnt_print_int',     ret: 'void', args: ['i64'] },
    double:  { name: 'plnt_print_decimal',  ret: 'void', args: ['double'] },
    i1:      { name: 'plnt_print_bool',     ret: 'void', args: ['i1'] },
    'i8*':   { name: 'plnt_print_text',     ret: 'void', args: ['i8*'] },
};

function toLLVMType(plantType) {
    const t = TYPE_MAP[plantType];
    if (!t) {
        throw new Error(`Unknown PlantLang type: ${plantType}`);
    }
    return t;
}

function getPrintFunction(llvmType) {
    return PRINT_FUNCTIONS[llvmType];
}

function isIntegerType(llvmType) {
    return llvmType === 'i64' || llvmType === 'i1';
}

function isFloatType(llvmType) {
    return llvmType === 'double';
}

function llvmTypeOf(value, literalType) {
    if (literalType === 'NUMBER') {
        if (typeof value === 'number' && !Number.isInteger(value)) return 'double';
        if (typeof value === 'number' && Number.isInteger(value)) return 'i64';
        return 'i64';
    }
    if (literalType === 'FACT') return 'i1';
    if (literalType === 'STRING') return 'i8*';
    return 'i64';
}

module.exports = { toLLVMType, getPrintFunction, isIntegerType, isFloatType, llvmTypeOf, TYPE_MAP };
