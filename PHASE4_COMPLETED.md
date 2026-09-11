# Phase 4 Completion — Pure Native & Chloroplast Transition (v0.46.4)

Status: **COMPLETE** — 2026-08-01

## Summary

Phase 4 executes the **total purge of Node.js/JavaScript legacy components**
and the **project-wide rebranding from plantc to Chloroplast**, transitioning
PlantLang to a **100% Pure Native architecture** and declaring the **v0.46.4
Pure Native release** ready.

## 1. Migration & Rebranding: plantc → Chloroplast

Chloroplast is now the definitive compiler binary for PlantLang:

| Aspect | Before (plantc) | After (Chloroplast) |
|---|---|---|
| Native build output | `bin/plantc` | `bin/Chloroplast` |
| v1 bootstrap compiler | `dist/plantc` | `dist/Chloroplast` |
| Install path | `~/.local/bin/plantc` | `~/.local/bin/Chloroplast` |
| CLI usage | `usage: plantc <source.plant> [out.c]` | `usage: Chloroplast <source.plant> [out.c]` |
| Version string | `plantc 1.0.0-phase5 (self-hosted)` | `Chloroplast 0.46.4 (pure native)` |
| package.json | `name: plantlang` | `name: chloroplast` |
| Makefile version | `VERSION = 1.0.0-phase5` | `VERSION = 0.46.4` |

The compiler sources (`src/plantc/*.plant`) were re-bootstrapped through the
five-generation chain against the rebranded bootstrap and re-converged to a
new fixed point: `v3 == v4 == v5` at **69 668 bytes** of generated C
(verified byte-for-byte by `make self`).

## 2. Total Removal of Legacy JavaScript

Deleted from the repository:

- **`core/interpreter.js`** — the legacy JavaScript interpreter (per checklist)
- **All legacy JS test suites** — reviewed and confirmed obsolete (every file
  exercised the purged JS engine via `require('chloroplast.js')` /
  `require('core/...')`), then deleted:
  - `tests/dispatcher.test.js`, `tests/matrix.test.js`, `tests/runtime.test.js`
  - `tests/test_*.js` — tokenizer, parser migration, codegen, diagnostics,
    LLVM codegen, depth contract, phases 7–21
  - `tests/llvm/` and `tests/parity/` suites
  - versioned suites `tests/v0.33.0…v0.44.0.*.test.js`
- **Legacy JS CLI/drivers** — `src/cli/plantc.js`, root `chloroplast.js`
  (Node CLI driver), `trace_parse.js`

Consequently the `make test-js` target was removed from the Makefile.
`package.json` was reduced to strict metadata (name/version/description/
license) with no scripts, no devDependencies, and no build hooks.

The remaining `core/*.js` sources are inert legacy files outside this phase's
deletion checklist; the compiler pipeline itself is 100% native.

## 3. Documentation & Versioning (v0.46.4)

All markdown documentation was updated per its structural role:

- **README.md** — rewritten for programmers coming from other languages:
  philosophy, why PlantLang, standout features, installation and usage
  (`./bin/Chloroplast` strictly), pointers to the other docs; **"Pure Native 🚀"**
  badge added
- **Language Tour.md** — installation/quick-start now native-only; architecture
  section re-written for the pure native pipeline; legacy JS-era architecture
  retained as an explicitly-marked historical record
- **ROADMAP.md** — v0.46.4 marked **fully completed**; upcoming milestones
  outlined precisely: v0.47.0 (std/json, std/math, std/time, Collections,
  Native C FFI direct calls), v0.48.0 (Generics `[T]`, Closures MOVE/REF,
  ASYNC/AWAIT), v0.49.0 (plantm, plantfmt, plantlang init/build), v0.50.0
  (Distributed GC, Raft Consensus for SHARED_WRITE + Paxos abstraction),
  v0.51.0 (LSP + CodeWords inspection, DWARF/GDB debugger), v0.52.0
  (`--target=wasm`, plant bench), v1.0.0 (`--target=morphon` Carbon Paradigm
  v5.1 QPU Dataflow, Macros, plantdoc)
- **TECHNICAL.md** — Pure Native architecture banner; self-hosting pipeline
  section updated to the native runtime (`plant_compat.h` FFI) and native CLI
  invocation
- **CHANGELOG.md** — v0.46.4 entry documenting the transition
- **docs/BUILD.md** — full Chloroplast build guide (no Node.js requirement)
- **PHASE3_COMPLETED.md** — updated to reference Chloroplast and v0.46.4
- **package.json** — `chloroplast` / `0.46.4` / MIT

## 4. v0.46.4 Pure Native Release Declaration

The v0.46.4 release is formally declared **Pure Native release ready**:

- ✅ `make all` — full native build → `bin/Chloroplast` (v1→v2→v3 chain)
- ✅ `make self` — self-hosting converged, 69 668 bytes (`v3 == v4 == v5`)
- ✅ `make test` — native integration suite 9/9 passing
- ✅ `make dist` — `release/plantlang-0.46.4.tar.gz`, DISTCHECK OK
  (unpack → build → test inside the tarball)
- ✅ `make install` — `~/.local/bin/Chloroplast` + runtime headers, version
  verified: `Chloroplast 0.46.4 (pure native)`
- ✅ Zero Node.js/JavaScript in the build, test, or runtime path

Git operations (staging/committing the renamed bootstrap, deletions, and doc
updates) are left to VS Code.
