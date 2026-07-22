# PlantLang Language Specification & Ecosystem v0.34.0

**PlantLang** is a human-centric, prose-based programming language engineered for both high-level readability and native-level execution performance. It transforms "prose-like" syntax into highly optimized machine code via the LLVM compiler infrastructure.

---

## 1. Philosophical Foundations (Prose Programming)
PlantLang breaks away from symbolic, bracket-heavy syntax. It is designed to be read as a natural language, prioritizing clarity for both humans and AI models:
* **Human-Readable:** Syntax flows like sentences.
* **AI-First:** Structured for reliable generation by Large Language Models.
* **Prose-Oriented:** Logic is expressed as a narrative flow, moving beyond the "terminal dark-mode" aesthetic.

---

## 2. Language Grammar & Core Types
PlantLang utilizes a strict type system for safety and performance:
* **`NUM`**: 64-bit integer arithmetic.
* **`SCL`**: Double-precision scalar values (produced by division).
* **`TX`**: Text strings (supported by heap-allocated buffers).
* **`FACT`**: Boolean logic (true/false).

### Control Flow Structures:
* **`IF / ORIF / ELSE`**: Contextual branching with multi-branch support.
* **`CYCLE`**: Numeric iteration (`FROM`/`TO`/`STEP`).
* **`SEASON`**: Condition-based while loops.
* **`FOR name IN expr, ... /FOR.`**: Iteration over LIST values, MAP keys, or TX strings.
* **`WEATHER / SHELTER / CALM`**: Deterministic exception handling — captures runtime errors (e.g., division by zero) with typed storm matching and recovery blocks.
* **`MATCH`**: Exhaustive pattern matching on tagged unions (CHOICE) — payload binding with scope-isolated clause bodies.
* **`ACTION / REAP / GIVE`**: Function definitions, calls, and returns — supports recursion, SCL parameters, and TX return values.
* **`IMPORT`**: Multi-file module system — load and merge external `.plnt` files with cycle detection.
* **FFI (`-> external`)**: Declare foreign C functions for direct native interop.

### Data Types:
* **`NUM` / `SCL` / `TX` / `FACT`**: Primitives — integer, double-precision scalar, text string, boolean.
* **`SHAPE`**: User-defined struct types with named, typed fields — `SHAPE Point { x(NUM), y(NUM) }.`
* **`STRUCT`**: Alternative struct syntax with `field: TYPE` — `STRUCT Person { name: TX, age: NUM }.` + anonymous literals `{ name: "Alice", age: 30 }`
* **`CHOICE`**: Tagged unions with optional payload per variant — `CHOICE Option { Some(NUM), None }.`
* **`SPECIES`**: Object-oriented classes with `{ }` body syntax, `FROM` inheritance, BLOOM instantiation — `SPECIES Greeter { msg: TX, ACTION greet() { GIVE SELF:msg. } }`
* **`LIST`**: Dynamic arrays with push/pop growth — `CREATE xs(LIST) TO 1, 2, 3.`
* **`MAP`**: Typed key-value hash table — `CREATE m(MAP[NUM,TX]).` — with native LLVM compilation for `LINK`, `has()`, `put()`

### Standard Library (`std/`)
PlantLang ships with a built-in standard library accessed via `IMPORT "std/..."`:
* **`std/io.plnt`** — `print`, `println` for console output
* **`std/string.plnt`** — `len`, `upper`, `lower`, `trim`, `contains`, `split`, `replace`, `concat`
* **`std/prelude.plnt`** — auto-injected core definitions (TRUE, FALSE, _BOOT)

### Rooted Depth System (Memory Model)
PlantLang uses a compile-time depth prefix (`\N`) before every statement to declare its scope level. Each depth owns a dedicated **64KB arena slab** (`Arena_N`), enabling deterministic bump-allocation with no garbage collector:

```plantlang
1\ CREATE x(NUM) TO 42.     # Arena_1 — root scope
2\   CYCLE i FROM 1 TO 10,  # Arena_2 — loop scope
3\     CREATE y(NUM) TO i.  # Arena_3 — inner scope
```

Arenas are automatically reclaimed:
- **Natural Exit**: Leaving a scope resets its arena.
- **Forced Exit**: Returning from an `ACTION` unwinds all arenas > depth 0.
- **Iteration Breath**: Each loop tick resets the loop's arena.
- **Error Unwinding**: A `WEATHER` throw resets all arenas between the error source and its `SHELTER` handler.

---

## 3. Engineering Architecture (The v0.34.0 Stack)
The ecosystem is built on a modular, industrial-grade pipeline:

1.  **Core Interpreter (Chloroplast Engine):**
    * High-fidelity Abstract Syntax Tree (AST) evaluator.
    * Static Type Checker (v0.18+) to ensure type safety before execution.
    * Full `WEATHER`/`SHELTER`/`CALM` exception handling with scope sandboxing.
2.  **Native Compiler (LLVM Backend):**
    * Emits standard `LLVM IR` in **SSA (Static Single Assignment)** form.
    * Arena-based memory allocation (replaces `alloca`).
    * Full function support (`ACTION`/`REAP`/`GIVE`) with recursive safety.
    * Deterministic unwind chain for exception handling.
    * Integrates with `llc` (LLVM) and `gcc` for final object linking.
3.  **MAP Hash Tables:**
    * Open-addressing with linear probing — fully compiled via LLVM IR.
    * djb2 hash for TX keys, identity hash for NUM keys.
    * Automatic growth at load factor > 0.75 with full rehash.
    * Arena-allocated bucket arrays within the Rooted Depth System.
4.  **CodeWords Service API:**
    * A sandboxed HTTP environment using process-level isolation for safe, distributed execution.
5.  **Web REPL UI:**
    * A single-page, browser-native IDE featuring a "settling ink" visualization to reflect the language's prose-based identity.
6.  **Module System & FFI:**
    * `IMPORT` statement with relative/absolute/std path resolution and cycle detection.
    * FFI declarations (`ACTION ... -> external.`) for calling native C functions.
    * AST merging at parse time — imported files are spliced into the importing program.
7.  **Standard Library (`std/`):**
    * Three core modules: `io.plnt`, `string.plnt`, `prelude.plnt`.
    * Auto-injected prelude on every parse.
    * Runtime C bridge (`core/runtime_bridge.c`) implementing FFI targets for I/O and string operations.
8.  **Struct Types & Methods:**
    * `SHAPE` declarations with compile-time field offset computation.
    * Methods via `ACTION (self(Point)) ...` with colon-dispatch call syntax.
    * SELF parameter, field access/mutation in LLVM codegen.
9.  **Tagged Unions & Pattern Matching:**
    * `CHOICE` declarations with typed payloads and keyword-compatible variant names.
    * `MATCH` with exhaustive clause validation and payload binding.
    * Variant construction via dot-notation: `Option.Some(10)`.
10. **Species / Bloom (Object-Oriented):**
    * `SPECIES` declarations with `{ }` body syntax, typed fields, and ACTION methods.
    * `FROM` / `PARENT` inheritance with deep-clone field merging.
    * `BLOOM` expression in `CREATE` — instance allocation with field defaults.
    * Colon-dispatch method calls via `REAP result FROM obj:method.`
    * `SELF:field` access (read/write) in species action bodies.
    * LLVM codegen — species registered as LLVM struct types with static method dispatch.

---

## 4. Compilation Pipeline
PlantLang employs a state-of-the-art compilation chain:
1. **Lexing & Parsing**: AST construction with depth-aware tokenization.
2. **Type Checking**: Semantic validation including Contract Law (depth ≤ destination) and Block-Depth Contract Law (validateDepthInvariants — ACTION/SPECIES at Depth 0, REAP/GIVE/CYCLE at Depth ≥ 1).
3. **LLVM IR Generation**: Conversion to LLVM SSA-based IR:
   - Arena allocation (`arenaAlloc`) for all variable storage.
   - Depth tracking with automatic arena reset on scope boundaries.
   - Zero-cost division-by-zero checks when inside `WEATHER` blocks.
   - MAP hash tables with inline djb2, linear probing, and automatic growth.
4. **Optimization**: `llc -O2` production-grade optimization passes.
5. **Native Linking**: Object code linked via `gcc` into a standalone binary.

---

## 5. Quick Reference Guide

| Feature | Command / Syntax | Description |
| :--- | :--- | :--- |
| **Declaration** | `CREATE x(NUM) TO 42.` | Initialize memory for variables. |
| **Assignment** | `SET x TO 10.` | Update existing variables. |
| **Arithmetic** | `INCREASE x BY 1.` | Perform operations on primitives. |
| **Output** | `SHOW x.` | Print values to standard output. |
| **Function** | `ACTION f(n(NUM)),` ... `GIVE n.` | Define and return from functions. |
| **Function Call** | `REAP r FROM f, x.` | Call a function and store the result. |
| **FFI Declaration** | `ACTION my_fn(n(NUM)) -> external.` | Declare a native C function for FFI. |
| **Import Module** | `IMPORT "std/io".` | Load and merge an external `.plnt` file. |
| **Try/Catch** | `WEATHER,` ... `SHELTER ZERO_STORM AS err,` ... `CALM.` | Catch runtime errors with typed handlers. |
| **Loop** | `CYCLE i FROM 1 TO 10,` ... | Numeric iteration. |
| **While** | `SEASON condition,` ... | Condition-based loop. |
| **Branch** | `IF cond,` ... `ORIF cond2,` ... `ELSE,` ... | Multi-branch conditional. |
| **Depth** | `\N` before statement | Declare scope level for arena allocation. |
| **Depth Contract** | Automatic enforcement | ACTION/SPECIES restricted to Depth 0; REAP/GIVE/CYCLE restricted to Depth ≥ 1. |
| **Struct (SHAPE)** | `SHAPE Point { x(NUM), y(NUM) }.` | User-defined aggregate type (classic syntax). |
| **Struct (STRUCT)** | `STRUCT Person { name: TX, age: NUM }.` | Alt. struct syntax with `field: TYPE`. |
| **Struct Literal** | `CREATE p(Person) TO { name: "A", age: 30 }.` | Anonymous struct literal in CREATE context. |
| **FOR...IN Loop** | `FOR x IN items, SHOW x. /FOR.` | Iterate over LIST, MAP keys, or TX string. |
| **Method** | `ACTION (self(Point)) move(x(NUM), y(NUM)),` ... `/ACTION.` | Typed method with SELF receiver. |
| **Method Call** | `REAP _ FROM p:move, 5, 10.` | Colon-dispatch method invocation. |
| **SPECIES Decl** | `SPECIES Greeter { msg: TX, ACTION greet() { GIVE SELF:msg. } }` | Class/object declaration with `{ }` body syntax. |
| **SPECIES Inherit** | `SPECIES Dog FROM Animal { ... }` | Inheritance via `FROM`/`PARENT` keyword. |
| **BLOOM Instantiation** | `CREATE g TO BLOOM Greeter.` | Create species instance in CREATE expression. |
| **SELF-field Access** | `SET SELF:count TO SELF:count + 1.` | Read/write instance fields in action body. |
| **Tagged Union** | `CHOICE Option { Some(NUM), None }.` | Sum type with payload variants. |
| **Variant Construction** | `Option.Some(10)` / `Option.None` | Create tagged union values. |
| **Pattern Match** | `MATCH opt { Some(v) -> { SHOW v } None -> { SHOW 0 } }.` | Exhaustive case analysis with binding. |
| **Array Push** | `PUT val INTO xs.` | Amortized-O(1) append. |
| **Array Pop** | `TAKE val FROM xs.` | Pop from end with shrink. |
| **MAP Create** | `CREATE m(MAP[NUM,TX]).` | Typed key-value hash table. |
| **MAP Insert** | `LINK key WITH value IN m.` | Insert key-value pair (compiled natively). |
| **MAP Has** | `SHOW m.has(key).` | Key existence check (compiled natively). |
| **LIST Count** | `SHOW COUNT(xs).` | O(1) array length — compiled via `extractvalue` on `%fat_ptr`. |
| **LIST First** | `SHOW FIRST(xs).` | O(1) first element — compiled via GEP + load. |
| **LIST Last** | `SHOW LAST(xs).` | O(1) last element — compiled via GEP to `len-1`. |
| **LIST Sum** | `SHOW SUM(xs).` | O(n) inline accumulation — compiled as LLVM phi loop. |
| **SPLIT String** | `REAP parts FROM SPLIT(str, delim).` | Split string by delimiter — returns `[TX]` array (native REAP expression). |
| **JOIN Strings** | `REAP joined FROM JOIN(parts, delim).` | Join `[TX]` array with delimiter — returns `TX` (native REAP expression). |

---

## 6. QA & Quality Assurance
The **v0.34.0** release is verified by an automated regression suite:
* **~856+ Total Tests** across twenty-six test suites (LLVM backend, C codegen, parser migration, diagnostics, tokenizer, Phase 7—21, depth contract, matrix, dispatcher, runtime, parallel, security).
* **LLVM Backend**: 50 smoke tests covering CREATE/SHOW, arithmetic, strings, comparisons, IF/CYCLE/SEASON, ACTION/REAP/GIVE (recursion, SCL params, TX returns), WEATHER/SHELTER exception handling, TX fat-pointer operations, and MAP hash tables (LINK, has(), growth, overwrite).
* **Native LIST Ops**: 15 tests covering COUNT, FIRST, LAST, SUM on empty/populated arrays, type-checker validation.
* **MAP Types**: 17 tests covering empty map create, map literals, LINK/put semantics, has/get, overwrite, growth (10 entries), SHOW display, type-checker validation.
* **SPECIES/BLOOM**: 10 tests covering `{ }` body syntax, BLOOM instantiation, method dispatch, inheritance, SELF mutation, type checking.
* **Module System**: 30 test groups covering IMPORT parsing, cycle detection, path resolution, error messages, and FFI syntax.
* **Standard Library**: 8 integration tests covering std/ path resolution, I/O and string module parsing, prelude injection, and end-to-end FFI calls.
* **Structs & Methods**: 133 tests covering SHAPE and STRUCT declaration, instantiation, field access/mutation, method dispatch, SELF receiver, anonymous struct literals, type checking, LLVM codegen parity.
* **Dynamic Arrays**: 122 tests covering push/pop, capacity growth, type checking, LLVM codegen, interpreter parity.
* **CHOICE & MATCH**: 64 tests covering variant declaration, construction, member access, MATCH exhaustiveness, payload binding, type checking, interpreter execution.
* **Parity Guarantee**: Exact output matching between the Interpreter and Native Compiler via `llc` + `gcc`.
* **Performance**: ~15,000x execution speedup on iterative loops via LLVM optimization compared to the JS interpreter.
* **Parity Guarantee**: Exact output matching between the Interpreter and Native Compiler via `llc` + `gcc`.
* **Performance**: ~15,000x execution speedup on iterative loops via LLVM optimization compared to the JS interpreter.

---

## 7. Roadmap & Future Scope

### ✅ Complete (v0.28.0)
- Primitives (`NUM`, `SCL`, `TX`, `FACT`)
- Control Flow (`IF`, `CYCLE`, `SEASON`, `FOR...IN`)
- Functions (`ACTION`, `REAP`, `GIVE` with recursion)
- Exception Handling (`WEATHER`, `SHELTER`, `CALM`)
- Rooted Depth System (arena-based deterministic memory)
- LLVM native compilation with full depth tracking
- Division-by-zero detection with error propagation
- Module System (`IMPORT` with cycle detection and AST merging)
- FFI (`ACTION ... -> external.` for native C interop)
- Standard Library Foundation (`std/io`, `std/string`, `std/prelude`)
- Core Runtime Bridge (`core/runtime_bridge.c` — 10 FFI targets)
- **Struct Types** (`SHAPE` and `STRUCT` with fields, instantiation, access/mutation, anonymous literals)
- **Methods** on structs (typed `SELF` receiver, colon-dispatch call syntax)
- **Dynamic Arrays** (`PUT`/`TAKE` push/pop with amortized capacity doubling)
- **Tagged Unions** (`CHOICE` with payload variants)
- **Pattern Matching** (`MATCH` with exhaustive validation and binding)
- **MAP Hash Tables** (typed key-value store with native LLVM compilation, open-addressing, linear probing, djb2 hash, automatic growth)
- **FOR...IN loops** (iterate over LIST values, MAP keys, or TX strings)
- **English-Language Cleanup** (all Arabic strings translated to English)
- **SPECIES / BLOOM OOP** (`{ }` body syntax, `FROM` inheritance, BLOOM instantiation, colon-dispatch method calls, SELF:field access)
- **Native LIST Operations** (`COUNT`, `FIRST`, `LAST` — O(1) via GEP; `SUM` — O(n) via inline LLVM loop; full interpreter and LLVM codegen)

### ✅ Completed (v0.30.0)
- Runtime C library (`libplantlang.so`) — math FFI, sort, string split/join
- `NATIVE ACTION ... -> external.` syntax for FFI declarations
- Compiled `SORT` on `[NUM]` and `[SCL]` arrays via `plnt_sort_i64` / `plnt_sort_double`
- String `SPLIT(str, delim)` / `JOIN(arr, delim)` — native syntax with REAP expression support
- Universal REAP expression sources — `REAP x FROM SPLIT(...)`, `REAP x FROM JOIN(...)`, `REAP x FROM expr[index]`
- 70KB large-string stress test for split/join roundtrip
- **Block-Depth Contract Law Enforcement** — semantic depth validation:
  - Parser: `enforceDepthContract(nodeType, minDepth, maxDepth, token)` with `this.currentDepth` tracking
  - Typechecker: `validateDepthInvariants(ast)` second-pass AST walker verifying depth invariants
  - ACTION/SPECIES restricted to Depth 0; REAP/GIVE/CYCLE restricted to Depth ≥ 1; nested ACTION rejected
  - 13 new tests in `test_depth_contract.js`

### ✅ Completed (v0.31.0)
- **Five-Mission Execution Architecture** — five distinct mission modes:
  - `MISSION: BALANCED/FAST/SAFE/SMART/PERSISTENT.` with per-mode memory, optimization, and boundary policies
  - `MissionBlockNode` AST, `MissionStack` runtime, `ScopedArena` depth-level memory
  - `MissionDispatcher` with SMART router for mission-aware ACTION dispatch
  - `BoundaryHandshakeMatrix` — 5×5 permission table governing cross-mode calls
  - LLVM codegen: `@_mission_mode` global, mode-check guards in `genReapStatement`
  - Typechecker: `_checkMissionStatement` validates mode string and matrix permissions
  - 75 new tests (28 matrix, 47 dispatcher) — all green
- **100% Test Pass Rate** — all pre-existing failures fixed:
  - Diagnostics column assertion corrected
  - RESPONSE emission + errVar binding fixed in parser migration tests
  - LLVM codegen ACTION test updated to reflect full support

### ✅ Completed (v0.32.0)
- **Local Runtime & Isolation Layer** — five runtime modules:
  - `BumpAllocator` (FAST) — O(1) linear bump allocator with 8-byte alignment, 8MB default, automatic BALANCED escalation
  - `GlobalARCHeap` (PERSISTENT) — atomic reference counting, auto cycle detection every 1000 allocs, `GC.cycle()` manual trigger
  - `WarmProcessPool` (SAFE) — 4 pre-warmed isolated workers, Ping/Pong heartbeat, zombie kill+respawn, 50ms queue timeout
  - `SafeChannel` — adaptive IPC: Structured Clone (≤1MB), Transferable (>1MB), SharedArrayBuffer, streaming
  - `MissionContext` — unified telemetry with `diagnostic()`, `trace()`, `getMetrics()` JSON output
  - **Escalation & Safety Matrix** — 5 automatic fallback rules with diagnostic logging
  - 70 new tests in `tests/runtime.test.js` — all green

### ✅ Completed (v0.34.0)
- **Zero-Trust Security & Audit Architecture** — three security modules:
  - `NonBlockingAuditLogger` — SAB ring buffer, SHA256 hash chain, async Worker flush, `verifyIntegrity()`
  - `mTLSJwtGuard` — TLS 1.3 mTLS cert loading, RS256/Ed25519 JWT verification, anti-replay via jti
  - `CapabilityGuard` — zero-trust SAFE defaults, granular capability matrix, syscall filtering
  - 91 new tests in `tests/v0.34.0_security.test.js` — all green

### 🔜 In Progress / Planned (v0.35.0)
- `PULSE` / `WHENEVER` reactive programming
- `VERIFY` / `SUITE` native compilation
- `TAP` file I/O in LLVM backend
- `HARVEST` networking in LLVM backend
- Expanded standard library (math, lists, maps modules)

---

*PlantLang v0.33.0 — Parallel Compilation & Telemetry. ParallelCodegenEngine, RemoteCompilerNode, NonBlockingTelemetry, RuntimeDispatcher. 60 new tests. 765+ total tests. All green.*
