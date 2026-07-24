'use strict';

const path = require('path');
const fs = require('fs');
const { CompilerPipeline } = require('../driver/pipeline');

const VERSION = '1.0.0-phase5';

function printUsage() {
    console.error(`PlantLang Compiler (plantc) v${VERSION}

Usage:
  plantc [options] <input.plant>

Options:
  -o, --output <file>     Output binary path (default: <input basename>)
  --emit-llvm             Keep the generated .ll LLVM IR file
  -O0, -O1, -O2, -O3     Optimization level (default: -O2)
  --run                   Compile and run the binary immediately
  --verbose               Print pipeline debug information
  -h, --help              Show this help message
  -v, --version           Show version

Examples:
  plantc program.plant
  plantc -O0 --run program.plant
  plantc -o myapp --emit-llvm program.plant
`);
}

function parseArgs(argv) {
    const args = { optLevel: 'O2' };
    let i = 2;
    while (i < argv.length) {
        const arg = argv[i];
        if (arg === '-h' || arg === '--help') { args.help = true; i++; continue; }
        if (arg === '-v' || arg === '--version') { args.version = true; i++; continue; }
        if (arg === '--emit-llvm') { args.emitLlvm = true; i++; continue; }
        if (arg === '--run') { args.run = true; i++; continue; }
        if (arg === '--verbose') { args.verbose = true; i++; continue; }
        if (arg === '-O0' || arg === '-O1' || arg === '-O2' || arg === '-O3') {
            args.optLevel = arg.slice(1);
            i++; continue;
        }
        if (arg === '-o' || arg === '--output') {
            if (i + 1 >= argv.length) {
                console.error('error: --output requires a file argument');
                process.exit(1);
            }
            args.outputFile = argv[i + 1];
            i += 2; continue;
        }
        if (arg.startsWith('-')) {
            console.error(`error: unknown option "${arg}"`);
            process.exit(1);
        }
        args.inputFile = arg;
        i++;
    }
    return args;
}

function run() {
    const args = parseArgs(process.argv);

    if (args.help) { printUsage(); process.exit(0); }
    if (args.version) { console.log(VERSION); process.exit(0); }

    if (!args.inputFile) {
        console.error('error: no input file specified');
        printUsage();
        process.exit(1);
    }

    if (!fs.existsSync(args.inputFile)) {
        console.error(`error: input file not found: ${args.inputFile}`);
        process.exit(1);
    }

    const pipeline = new CompilerPipeline({
        inputFile: args.inputFile,
        outputFile: args.outputFile,
        emitLlvm: args.emitLlvm,
        optLevel: args.optLevel,
        verbose: args.verbose,
        run: args.run,
    });

    try {
        const binaryPath = pipeline.compile();
        if (args.run) {
            const result = pipeline.runBinary(binaryPath);
            if (result.stdout) process.stdout.write(result.stdout + '\n');
            process.exit(result.exitCode);
        } else {
            console.error(`compiled → ${binaryPath}`);
        }
    } catch (e) {
        console.error(`error: ${e.message}`);
        process.exit(1);
    }
}

if (require.main === module) {
    run();
}

module.exports = { run, parseArgs, VERSION };
