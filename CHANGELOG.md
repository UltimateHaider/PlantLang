# Changelog — PlantLang / Chloroplast

## v0.25.0 — 2026 (current)

### New Features
- **SHAPE (Struct Types)** — user-defined aggregate types:
  - Syntax: `SHAPE Point { x(NUM), y(NUM) }.` with named, typed fields
  - Instantiation: `CREATE p(Point) TO Point{ 10, 20 }.`
  - Field access: `p.x`, `p.y`
  - Field mutation: `SET p.x TO 99.`
  - Support for zero-field structs (`SHAPE Unit { }.`)
  - Struct-of-struct composition and deep field access
- **Methods on Structs** — ACTIONs that receive a typed `SELF` parameter:
  - Syntax: `ACTION (self(Point)) move(x(NUM), y(NUM)), SET self.x TO x. SET self.y TO y. /ACTION.`
  - Method call: `REAP _ FROM p:move, 5, 10.` via colon dispatch
  - `SelfReferenceNode` in AST for `SELF` keyword
  - Type checker validates receiver type on method calls
- **Dynamic Arrays (push/pop)** — runtime array growth:
  - `PUT val INTO xs.` — amortized-O(1) appends with capacity doubling
  - `TAKE val FROM xs.` — pop from end with shrink
  - `CREATE xs(LIST) TO.` — empty list declaration
  - Type checker validates list operations
- **Tagged Unions via CHOICE** — type-safe sum types:
  - Syntax: `CHOICE Option { Some(NUM), None }.`
  - Variant construction: `Option.Some(10)`, `Option.None`
  - Variant names may collide with keywords (`Num`, `Empty`), handled by parser
  - Static type checker validates variant existence, payload types, arity
- **Pattern Matching via MATCH** — exhaustive case analysis on CHOICE values:
  - Syntax: `MATCH opt { Some(v) -> { SHOW v } None -> { SHOW 0 } }.`
  - Payload binding: `Some(v)` binds the inner value to `v` in the clause body
  - Payload-free clause: `None -> { SHOW "none" }`
  - Non-exhaustive match detection (missing variant = compile-time error)
  - `MatchStatementNode` in AST with `{variantName, binding, bodyStatements}` clause array
- **Member Access / Method Call Parsing** — full expression-level member access:
  - `Option.Some(10)` parsed as `MethodCallNode` with target `Option`, method `Some`
  - `Option.None` parsed as `MemberAccessNode` with object `Option`, member `None`
  - KEYWORD field names (e.g. `Num`, `Empty`) accepted after `.` in member-access position, with same-line + statement-avoidance heuristics to prevent false matches
- **Inferred CREATE types** — `(TYPE)` annotation optional when the value expression type can be inferred from a CHOICE variant construction or array literal

### Test Suite
- Added **Phase 9 — Structs** (`tests/test_phase9_structs.js`) — 70 tests covering SHAPE declaration, instantiation, field access/mutation, struct-of-struct, type checking, LLVM codegen parity
- Added **Phase 10 — Dynamic Arrays** (`tests/test_phase10_arrays.js`) — 58 tests covering push/pop, capacity growth, type checking, LLVM codegen, interpreter parity
- Added **Phase 11 — Methods** (`tests/test_phase11_methods.js`) — 47 tests covering method declaration, call dispatch, SELF receiver, type checking, LLVM codegen
- Added **Phase 12 — Array Growth** (`tests/test_phase12_arrays_growth.js`) — 64 tests covering amortized capacity doubling, shrink-on-pop, edge cases
- Added **Phase 13 — CHOICE & Pattern Matching** (`tests/test_phase13_choices_matching.js`) — 64 tests covering variant declaration, construction, member access, MATCH exhaustiveness, payload binding, type checking, interpreter execution
- Total test suites expanded to **12 files, ~613 total assertions** — all green

### Documentation
- `README.md` updated to v0.25.0 with SHAPE, Methods, Dynamic Arrays, CHOICE/MATCH sections
- `ROADMAP.md` — v0.25.0 objectives marked complete, v0.26.0 roadmap drafted
- `Language Tour.md` — added SHAPE, CHOICE/MATCH, method call syntax
- `TECHNICAL.md` — updated test counts and architecture diagrams

---

## v0.24.0 — 2026

### New Features
- **IMPORT Statement & Module System** — multi-file PlantLang programs:
  - `IMPORT "path".` syntax for loading external `.plnt` files
  - Relative, absolute, and `std/`-prefixed path resolution
  - Cycle detection with clear error messages (`IMPORT cycle detected`)
  - AST merging — imported statements are spliced into the importing program's AST at parse time
  - Type checker resolves and validates all imports before semantic analysis
  - Interpreter resolves `IMPORT` at runtime with file-system lookup
  - `ImportStatementNode` in AST with `path`, `statements`, `resolvedAbsPath` fields
- **FFI (Foreign Function Interface)** — call native C functions from PlantLang:
  - Syntax: `ACTION name(params) -> external.` — marks an ACTION as an external C function
  - `isExternal` property on `ActionDeclaration` nodes
  - LLVM backend emits `declare` IR for FFI functions with proper type signatures
  - FFI stubs pre-registered in the interpreter for all `runtime_bridge` functions
  - Type checker validates FFI signatures against known bridge functions
- **Standard Library Foundation** (`std/`):
  - `std/io.plnt` — `print`, `println`, `plant_printf`, `plant_puts` (FFI-bridged I/O)
  - `std/string.plnt` — `len`, `upper`, `lower`, `trim`, `contains`, `split`, `replace`, `concat`
  - `std/prelude.plnt` — auto-injected core definitions (TRUE, FALSE, _BOOT)
  - Auto-prelude injection — every program implicitly imports `std/prelude.plnt`
  - `IMPORT "std/io"` and `IMPORT "std/string"` — resolve via `std/` path prefix
- **Core Runtime Bridge** (`core/runtime_bridge.c`):
  - C bridge implementing FFI targets: `plant_printf`, `plant_puts`, `plant_len`, `plant_upper`, `plant_lower`, `plant_trim`, `plant_contains`, `plant_split`, `plant_replace`, `plant_concat`
  - Linked into compiled binaries via the LLVM backend's `declare` + extern resolution

### Test Suite
- Added **Phase 7 — Module System & FFI** (30 test groups)
- Added **Phase 8 — Standard Library** (8 integration test groups)
- LLVM backend expanded from 37 → **46 smoke tests**
- All test suites green — 7 test files, ~300+ total assertions

---

## v0.23.0 — 2026

### New Features
- **ACTION/REAP/GIVE** in the LLVM backend (`core/llvm_codegen.js`) — full function support with:
  - Multiple typed parameters (NUM, SCL, TX, FACT)
  - Recursive calls (factorial, Fibonacci — verified via llc + gcc)
  - IF/ELSE bodies with multiple GIVE statements (terminator-aware branching)
  - SCL (double) params via bitcast through i64 return register
  - TX (string) returns via ptrtoint/inttoptr
  - Void actions (no GIVE) default to `ret i64 0`
- **Rooted Depth System** — deterministic arena-based memory management:
  - 64 depth levels, each backed by a 64KB arena slab (`@arena_offsets[64 x i64]`, `@arena_memory[64 x [65536 x i8]]`)
  - Bump allocation (`arenaAlloc`/`arenaAllocTyped`) replaces all `alloca` across all variable creation sites (CREATE, REAP auto-create, CYCLE iter, function params)
  - Depth tracking (`trackDepth`) injects automatic `arenaResetDepth` on scope exit
  - Arena globals emitted lazily only when `m.usesArena` is set
- **Contract Law Validation** (Article III) — compile-time depth enforcement:
  - CREATE destination must be ≤ current depth, with educational error messages citing the violated rule and suggesting fixes
  - `checkDepthAccess()` helper infrastructure for future cross-depth read/write enforcement
  - Scope entries track `{ptr, plType, depth}` for all variables
- **Forced Exit Arena Cleanup** (Article IX) — `genGiveStatement` emits the Unwinding Chain before every `ret`, resetting all arenas from `currentDepth` down to depth 1 (depth 0 preserved for caller's function params — critical for recursive correctness)
- **Loop Iteration Reset** (Article VII) — `genCycle` and `genSeason` save the arena offset at the loop's depth before the body and restore it after each tick, preventing temporary-variable accumulation across iterations. Deeper arenas entered during the body are also reset.
- **Error Unwinding Protocol** (WEATHER/SHELTER/CALM) — deterministic exception handling in LLVM:
  - `genWeatherStatement` generates the try/catch block structure with body, typed SHELTER handlers, and CALM continuation
  - Division-by-zero detection (`emitZeroCheck`) emits a `fcmp oeq` + conditional branch that sets `@_weather_msg`/`@_weather_type`/`@_weather_flag` globals and transfers control to the matching ZERO_STORM (or ANY_STORM) shelter handler
  - Unwind chain in each handler resets arenas from error depth to shelter depth
  - `errVar` binding loads the error message from `@_weather_msg` into a TX-typed arena slot
  - `shelterStack` enables proper nesting — inner errors are caught by inner handlers; handler body errors propagate outward
- **`core/Module`** gains `ensureWeatherGlobals()` and `weatherGlobalsEmitted` flag for lazy global emission

### Test Suite
- LLVM backend tests expanded from 34 → **37 smoke tests** (3 new: WEATHER body without error, ZERO_STORM caught via division by zero, post-error scope integrity)
- All four test suites green: LLVM backend 37/37, C codegen 9/9, parser migration 108/109 (1 pre-existing RESPONSE resolution discrepancy), diagnostics 44/45 (1 pre-existing)

### Documentation
- `README.md` updated to v0.23.0 with new sections on the Rooted Depth System, complete Quick Reference (ACTION/REAP/GIVE, WEATHER/SHELTER/CALM, depth syntax), updated test counts, and reorganized Roadmap

---

## v0.22.0 — 2026

### New Features
- **LLVM IR Backend** (`core/llvm_codegen.js`) — a real compiler backend emitting LLVM IR text, using the same pipeline architecture as **Rust, Swift, Julia, and Zig**:
  - Proper SSA-form code generation (`alloca`/`load`/`store` per variable, exactly matching clang's own `-O0` output shape, which LLVM's `mem2reg` pass then promotes to true SSA registers)
  - Full expression compiler with correct operator precedence (`OR` → `AND` → `NOT` → comparison → additive → multiplicative → unary → atom), matching the interpreter's evaluation semantics exactly
  - NUM/SCL type promotion, string concatenation via runtime `malloc`/`strcpy`/`strcat`, `BETWEEN` conditions, boolean short-circuit-free `AND`/`OR`/`NOT`
  - Same supported/unsupported subset as the C backend (CREATE/SET/INCREASE/DECREASE/SHOW/IF/ORIF/ELSE/CYCLE/SEASON) — unsupported constructs (LIST, MAP, ACTION, SPECIES, ...) fail with a clear compile-time error, never silently miscompiled
- **`chloroplast compile --backend llvm|c`** — auto-detects an installed LLVM toolchain (`llc`/`llc-14` through `llc-18`) and prefers it by default; falls back to the C backend automatically if no LLVM install is found
- **`opt -O2` integration** — the generated IR is run through LLVM's real optimizer (mem2reg, GVN, loop optimizations, inlining) before `llc` lowers it to object code. Without this step, hand-emitted alloca-heavy IR only gets `llc`'s backend-level optimizations and misses most of what makes LLVM fast — with it, compiled PlantLang matches gcc's own optimizer on equivalent code (verified: a 50-million-iteration accumulation loop drops from 88.7s interpreted to 6ms compiled — a ~14,700× speedup)
- **`tests/test_llvm_codegen.js`** — 27 parity tests (interpreted vs LLVM-compiled output must match exactly) covering arithmetic, SCL/NUM promotion, string concatenation, all comparison operators, BETWEEN, boolean logic, IF/ORIF/ELSE, CYCLE (with and without STEP), SEASON, nested loops, and unsupported-construct error handling
- **`examples/10_performance.plnt`** — a runnable interpreter-vs-compiled benchmark demonstrating the real-world speedup

### Fixes (found by building and testing the LLVM backend against the interpreter)
- **`core/evaluator.js`**: string literals containing text that happens to match a variable name (e.g. `"pi=" + pi` where `pi` is also a variable) were being corrupted — the identifier-substitution regex used plain word-boundaries (`\b...\b`) with no awareness of quote context, so it matched and replaced the `pi` substring *inside* the string literal `"pi="` itself, producing `"3.14=3.14"` instead of `"pi=3.14"`. Fixed by having the substitution regex skip over quoted string-literal spans entirely. This is a real interpreter bug, not just an LLVM-codegen quirk — it affected `chloroplast run` too, and was only caught because the LLVM backend's independent implementation disagreed with the interpreter's on the exact same test case.
- **`core/llvm_codegen.js`**: `CREATE x(NUM) TO someOtherVariable.` was compiling to a hardcoded `0` instead of loading the referenced variable — the parser produces a plain `Identifier` AST node (not a `RAW_EXPR` `Literal`) for single bare-identifier value expressions, and `genCreate()`'s type dispatch had no case for it, silently falling through to a zero default. Caught by a nested-loop test (`SEASON` inside `CYCLE`, where the loop-local variable was initialized from the outer loop's counter) producing empty output instead of a countdown.

---

## v0.21.0 — 2025 (current)

### New Features
- **Web REPL UI** (`webrepl/`) — a single-page, no-build-step browser interface for writing and running PlantLang:
  - Four modes matching the CodeWords Compiler Service's endpoints: **Run it**, **Check it**, **Verify it**, **Compile it**
  - Live connection indicator polling `/health`
  - Eight curated example programs loadable from a dropdown (generated from real `examples/*.plnt` files)
  - Collapsible generated-C-source view for Compile mode
  - `Cmd/Ctrl+Enter` keyboard shortcut, persistent editor content and server URL via `localStorage`
  - Visual design built around PlantLang's "reads like prose" identity — Fraunces serif for the mode tabs/wordmark, output lines settle in individually rather than appearing all at once
- **`webrepl/test_webrepl.js`** — 13 integration tests using jsdom to load the *real* HTML/JS files and drive actual clicks/keyboard events/fetch calls against a *live* CodeWords Compiler Service (not mocked)

### Milestone
This completes the full compiler pipeline originally planned:
```
Tokenizer → Parser/AST → Type Checker → Standard Library
    → C Generator → GCC Binary → Compiler Service → Web REPL UI
```
Every stage is implemented, tested, and documented.

---

## v0.20.0 — 2025 (current)

### New Features
- **CodeWords Compiler Service** (`service/codewords-server.js`) — standalone HTTP API exposing the interpreter, typechecker, and C code generator to external clients:
  - `GET /health` — service status
  - `POST /run` — execute a program, return captured output
  - `POST /check` — static type-check, return diagnostics
  - `POST /verify` — run VERIFY/SUITE tests, return pass/fail counts
  - `POST /compile` — generate C, compile with gcc, run the binary, return output
- **`service/sandbox-runner.js`** — per-request forked child process worker. Untrusted PlantLang code never runs in the main server process, so:
  - Infinite loops are killed cleanly after a 5s timeout without affecting other in-flight requests
  - `LISTEN BRANCH` and `HARVEST` are rejected before execution (no port-binding or outbound network access from submitted code)
  - Output is capped at 64KB; request bodies at 128KB
- **`service/test_service.js`** — 15 automated tests covering every endpoint, error handling, timeout enforcement, network blocking, and true concurrent-request isolation
- **`service/README.md`** — full API reference and safety model documentation

---

## v0.19.0 — 2025 (current)

### New Features
- **C Code Generator** (`core/codegen.js`) — translates a supported subset of PlantLang AST into standalone, compilable C99 source:
  - `CREATE` / `SET` / `INCREASE` / `DECREASE` for `NUM`, `SCL`, `TX`, `FACT`
  - `SHOW` — string literals, identifiers, and `+` concatenation (auto-selects `printf` format per type)
  - `IF` / `ORIF` / `ELSE`
  - `CYCLE var FROM lo TO hi [STEP k]` (numeric ranges)
  - `SEASON` (while loop)
  - Arithmetic (`+ - * / % **`) and comparisons (`IS`, `GREATER THAN`, `BETWEEN`, etc.) translated to native C operators
  - Unsupported constructs (LIST, MAP, ACTION/REAP, SPECIES, WEATHER, HARVEST, LISTEN BRANCH, ...) are reported as clear compile-time errors naming the exact construct and line — never silently miscompiled
- **`chloroplast compile`** — new CLI command:
  ```bash
  chloroplast compile file.plnt --run        # compile + execute immediately
  chloroplast compile file.plnt -o mybinary  # custom output path
  chloroplast compile file.plnt --keep-c     # keep generated .c file for inspection
  ```
  Pipes generated C through `gcc -O2 -lm` to produce a native binary.
- **`tests/test_codegen.js`** — parity smoke tests: runs each fixture both interpreted and compiled, asserts identical stdout (9/9 passing)
- **`examples/09_compile.plnt`** — FizzBuzz-style demo covering CYCLE, IF/ORIF/ELSE, SEASON, SCL comparisons

### Fixes
- `CYCLE var FROM lo TO hi STEP k` — `STEP` is tokenized as `IDENT` (not `KEYWORD`), so the parser's generic `_collectUntilKeyword()` never stopped at it. `toExpr` was swallowing `"20 STEP 5"` whole and `stepExpr` was always null. Fixed with an explicit IDENT-aware scan in `parseCycleStatement`. This was a **pre-existing interpreter bug**, not just a codegen limitation — `CYCLE ... STEP` was silently broken in `chloroplast run` too.

---

## v0.18.0 — 2025 (current)

### New Features
- **Static Type Checker** (`core/typechecker.js`) — full AST-based static analysis:
  - `TYPE_MISMATCH` — wrong type passed to action parameter or arithmetic on non-NUM
  - `ARITY_MISMATCH` — wrong number of arguments to an action
  - `UNDEFINED_ACTION` — calling an action that was never defined
  - `UNDEFINED_VAR` — using a variable that was never declared (warning)
  - `UNDEFINED_SPECIES` — BLOOM of an undefined SPECIES (warning)
  - `LOCK_VIOLATION` — attempt to SET a ROOT or locked variable
  - Errors inside WEATHER body are demoted to info (they're intentionally risky)
  - Two-pass architecture: declaration hoisting then full type checking
  - Library return types pre-declared (`math:SQRT → SCL`, `strings:UPPER → TX`, etc.)
  - Polymorphic action detection (all-TX params treated as ANY)
- **`chloroplast check`** now runs the static type checker with visual caret output
  (previously just counted statements)

### Fixes
- `chloroplast check` shows file/line/column with source preview and error arrow

---

## v0.7 — 2025 (current)

### New Features
- **LISTEN BRANCH** — real Node.js HTTP server with routing (`IF/ORIF/ELSE` on `req:"path"`)
- **HARVEST** — synchronous-style HTTP/HTTPS client (Worker + SharedArrayBuffer)
- **BRAID** — zip two lists into pairs or a MAP
- **VERIFY / SUITE** — built-in testing framework written in PlantLang itself
- **ANALYZE** — type-aware data inspection (sum/avg/min/max for NUM lists, key listing for MAPs)
- **NOW** — current date/time (`FORMAT:DATE`, `FORMAT:TIME`, `FORMAT:STAMP`, `FORMAT:YEAR`)
- **WAIT** — synchronous sleep (capped at 10s)
- **ROOT_SCOPE** — locked config MAP built at program start
- **TYPEOF** — `REAP t FROM TYPEOF x` returns type as TX

### Language Fixes
- `GREATER THAN OR EQUAL` / `LESS THAN OR EQUAL` now work correctly (were parsed as `GREATER THAN OR` + `EQUAL`)
- `IF/ORIF/ELSE` chain now runs only the first matching branch (was running all branches)
- `obj:"key"` quoted MAP access works inside string concatenation
- Chained access `obj:"k1":"k2":"k3"` works in expressions and compound strings
- `**` (power) tokenized as single token — `2 ** 8` now evaluates correctly
- `SET obj:"key" TO val` added with LOCK_STORM guard
- `LOCK_STORM` on `SET` to any locked MAP or ROOT variable

### Architecture
- Full compiler frontend: Tokenizer → Parser → AST (40+ typed node classes) → evaluateNode
- 96% AST coverage — all statements except empty block closers parsed to typed nodes
- CLI (`chloroplast run/verify`) now uses AST pipeline (`runSource`) exclusively
- symbolPass handles typed PlantStatement/RootStatement/RootScopeStatement/MissionStatement
- VERIFY dry-run mode for LISTEN BRANCH inside SUITE blocks

---

## v0.6 — 2025

### New Features
- **VERIFY / SUITE** initial implementation
- **BRAID** initial implementation
- **HARVEST** initial implementation (HTTP via Worker + SAB)
- **ANALYZE / TYPEOF / NOW / WAIT / ROOT_SCOPE** implemented
- **PULSE / WHENEVER** confirmed working with MATCH bodies
- Precision line/column diagnostics with visual caret pointer
- `SHOW_VERIFY_SUMMARY` CLI command, `chloroplast verify` subcommand

### Architecture
- Compiler frontend migration started (Tokenizer, Parser, AST as additive layer)
- SHOW / CREATE / LISTEN BRANCH / RESPONSE migrated to AST
- WEATHER / SHELTER / CALM migrated to AST

---

## v0.5 — 2025

### New Features
- **SPECIES / BLOOM / PARENT** — class-based OOP with inheritance
- **SELF** — instance self-reference in all contexts (read/write/INCREASE/PUT/REAP)
- **FLOW** — action pipeline chaining
- **MATCH / YIELD** — pattern matching with BETWEEN, range, ELSE
- **SEASON** — while-style loop

---

## v0.4 — 2025

### New Features
- **WEATHER / SHELTER / CALM** — try/catch/finally error handling
- **TAP / ABSORB / INFUSE / SEAL** — file I/O (VeinFS virtual filesystem)
- **PULSE / WHENEVER / CHANGES** — reactive variable observation
- **ROOT / LOCK** — immutable constants
- Multiple SHELTER clauses per WEATHER block
- `ANY_STORM` catch-all

---

## v0.3 — 2025

### New Features
- **ACTION / GIVE / REAP** — user-defined functions with return values
- **Recursion** — verified with Fibonacci and factorial
- **REAP _ FROM** — discard return value
- **PLANT** — load built-in libraries (math, strings, lists)
- **math** library: SQRT, ABS, ROUND, FLOOR, CEIL, POW, LOG, SIN, COS
- **strings** library: TRIM, UPPER, LOWER, LENGTH, SPLIT, REPLACE, INCLUDES
- **lists** library: AVERAGE, MEDIAN, UNIQUE, FLATTEN, CHUNK, RANGE

---

## v0.2 — 2025

### New Features
- **CYCLE** — list iteration and numeric range loops (`FROM n TO m STEP k`)
- **SORT / SHAKE** — list sorting and shuffling
- **PUT / TAKE** — list mutation
- **LINK** — MAP key assignment
- **MAP** type with `obj:prop` and `obj:"key"` access
- **STOP IF** — conditional abort with optional message
- Comma continuation for multi-line statements

---

## v0.1 — 2025 (initial)

- Core interpreter in Node.js (Chloroplast)
- Depth prefix `N\` syntax
- Types: NUM, SCL, TX, FACT, LIST
- CREATE / SET / INCREASE / DECREASE / SHOW / LOCK / EVAPORATE
- IF / ORIF / ELSE (single-line and block forms)
- MISSION: SAFE / FAST / SMART / BALANCED
- Arabic-readable design with English-only keywords
