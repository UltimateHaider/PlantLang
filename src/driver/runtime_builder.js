'use strict';

const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '../..');
const RUNTIME_SRC = path.join(ROOT, 'runtime', 'c', 'plant_runtime.c');
const RUNTIME_H   = path.join(ROOT, 'runtime', 'c', 'plant_runtime.h');
const BUILD_DIR   = path.join(ROOT, 'build');
const RUNTIME_OBJ = path.join(BUILD_DIR, 'plant_runtime.o');

function toolExists(tool) {
    try {
        execFileSync('which', [tool], { stdio: 'pipe' });
        return true;
    } catch (_) {
        return false;
    }
}

function ensureToolchain() {
    const needed = ['clang', 'opt'];
    for (const t of needed) {
        if (!toolExists(t)) {
            throw new Error(`Required tool not found: "${t}". Install LLVM/Clang toolchain.`);
        }
    }
}

function buildRuntime() {
    if (!fs.existsSync(BUILD_DIR)) {
        fs.mkdirSync(BUILD_DIR, { recursive: true });
    }
    execFileSync('clang', [
        '-c',
        '-o', RUNTIME_OBJ,
        RUNTIME_SRC
    ], { stdio: 'pipe' });
}

function ensureRuntimeBuilt(verbose) {
    ensureToolchain();

    if (fs.existsSync(RUNTIME_OBJ)) {
        const objMtime = fs.statSync(RUNTIME_OBJ).mtimeMs;
        const srcMtime = Math.max(
            fs.statSync(RUNTIME_SRC).mtimeMs,
            fs.statSync(RUNTIME_H).mtimeMs
        );
        if (objMtime >= srcMtime) {
            if (verbose) console.error('[runtime] build/plant_runtime.o is up to date');
            return;
        }
        if (verbose) console.error('[runtime] source changed, recompiling...');
    } else {
        if (verbose) console.error('[runtime] build/plant_runtime.o not found, compiling...');
    }

    buildRuntime();
    if (verbose) console.error('[runtime] compiled → build/plant_runtime.o');
}

module.exports = { ensureRuntimeBuilt, buildRuntime, ensureToolchain };
