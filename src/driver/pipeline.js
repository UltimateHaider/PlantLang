'use strict';

const fs = require('fs');
const path = require('path');
const os = require('os');
const { execFileSync } = require('child_process');
const { ensureRuntimeBuilt } = require('./runtime_builder');
const { parse } = require('../../core/parser');
const { LLVMEmitter } = require('../../src/codegen/llvm/llvm_emitter');

const ROOT = path.resolve(__dirname, '../..');
const UPDATE_PERIOD = 60000;

class CompilerPipeline {
    constructor(opts = {}) {
        this.inputFile   = opts.inputFile;
        this.outputFile  = opts.outputFile;
        this.emitLlvm    = !!opts.emitLlvm;
        this.optLevel    = opts.optLevel || 'O2';
        this.verbose     = !!opts.verbose;
        this.run         = !!opts.run;
        this.tmpDir      = null;
        this.cleanupDirs = [];
    }

    _log(...args) {
        if (this.verbose) console.error('[pipeline]', ...args);
    }

    _checkOutputDir(filePath) {
        const dir = path.dirname(path.resolve(filePath));
        if (!fs.existsSync(dir)) {
            fs.mkdirSync(dir, { recursive: true });
        }
    }

    compile() {
        ensureRuntimeBuilt(this.verbose);

        this._log('parsing', this.inputFile);
        const source = fs.readFileSync(this.inputFile, 'utf8');
        const program = parse(source);

        this._log('generating LLVM IR');
        const emitter = new LLVMEmitter();
        const rawIR = emitter.generate(program);

        const baseName = path.basename(this.inputFile, path.extname(this.inputFile));
        const outDir = path.dirname(path.resolve(this.outputFile || baseName));

        const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'plantc_'));
        this.tmpDir = tmpDir;

        const irPath = path.join(tmpDir, `${baseName}.ll`);
        fs.writeFileSync(irPath, rawIR, 'utf8');
        this._log('wrote', irPath);

        const optPath = path.join(tmpDir, `${baseName}.opt.ll`);
        this._log(`running opt -${this.optLevel}`);
        execFileSync('opt', [
            `-${this.optLevel}`,
            '-S',
            '-o', optPath,
            irPath
        ], { stdio: 'pipe' });
        this._log('optimized →', optPath);

        if (this.emitLlvm) {
            const destLl = path.join(outDir, `${baseName}.ll`);
            this._checkOutputDir(destLl);
            fs.copyFileSync(irPath, destLl);
            this._log('retained IR →', destLl);
        }

        const objPath = path.join(tmpDir, `${baseName}.o`);
        this._log('compiling to object');
        execFileSync('clang', [
            '-c', '-o', objPath, optPath
        ], { stdio: 'pipe' });

        const runtimeObj = path.join(ROOT, 'build', 'plant_runtime.o');
        const binaryPath = path.resolve(this.outputFile || path.join(outDir, baseName));
        this._checkOutputDir(binaryPath);
        this._log('linking →', binaryPath);
        execFileSync('clang', [
            objPath, runtimeObj, '-no-pie', '-lm', '-o', binaryPath
        ], { stdio: 'pipe' });

        if (!this.emitLlvm && !this.verbose) {
            try { fs.unlinkSync(irPath); } catch (_) {}
            try { fs.unlinkSync(optPath); } catch (_) {}
            try { fs.unlinkSync(objPath); } catch (_) {}
        }

        try { fs.rmdirSync(tmpDir); } catch (_) {}

        return binaryPath;
    }

    runBinary(binaryPath) {
        this._log('executing', binaryPath);
        const result = execFileSync(binaryPath, [], {
            encoding: 'utf8',
            stdio: ['pipe', 'pipe', 'pipe'],
        });
        return {
            stdout: result.trim(),
            exitCode: 0,
            raw: result,
        };
    }
}

module.exports = { CompilerPipeline };
