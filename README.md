# PlantLang Language Specification & Ecosystem v0.25.0

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
* **`WEATHER / SHELTER / CALM`**: Deterministic exception handling — captures runtime errors (e.g., division by zero) with typed storm matching and recovery blocks.
* **`MATCH`**: Exhaustive pattern matching on tagged unions (CHOICE) — payload binding with scope-isolated clause bodies.
* **`ACTION / REAP / GIVE`**: Function definitions, calls, and returns — supports recursion, SCL parameters, and TX return values.
* **`IMPORT`**: Multi-file module system — load and merge external `.plnt` files with cycle detection.
* **FFI (`-> external`)**: Declare foreign C functions for direct native interop.

### Data Types:
* **`NUM` / `SCL` / `TX` / `FACT`**: Primitives — integer, double-precision scalar, text string, boolean.
* **`SHAPE`**: User-defined struct types with named, typed fields — `SHAPE Point { x(NUM), y(NUM) }.`
* **`CHOICE`**: Tagged unions with optional payload per variant — `CHOICE Option { Some(NUM), None }.`
* **`LIST`**: Dynamic arrays with push/pop growth — `CREATE xs(LIST) TO 1, 2, 3.`
* **`MAP`**: Key-value storage — `CREATE cfg(MAP).`

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

## 3. Engineering Architecture (The v0.25.0 Stack)
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
3.  **CodeWords Service API:**
    * A sandboxed HTTP environment using process-level isolation for safe, distributed execution.
 4.  **Web REPL UI:**
     * A single-page, browser-native IDE featuring a "settling ink" visualization to reflect the language's prose-based identity.
 5.  **Module System & FFI:**
     * `IMPORT` statement with relative/absolute/std path resolution and cycle detection.
     * FFI declarations (`ACTION ... -> external.`) for calling native C functions.
     * AST merging at parse time — imported files are spliced into the importing program.
 6.  **Standard Library (`std/`):**
     * Three core modules: `io.plnt`, `string.plnt`, `prelude.plnt`.
     * Auto-injected prelude on every parse.
     * Runtime C bridge (`core/runtime_bridge.c`) implementing FFI targets for I/O and string operations.
 7.  **Struct Types & Methods:**
     * `SHAPE` declarations with compile-time field offset computation.
     * Methods via `ACTION (self(Point)) ...` with colon-dispatch call syntax.
     * SELF parameter, field access/mutation in LLVM codegen.
 8.  **Tagged Unions & Pattern Matching:**
     * `CHOICE` declarations with typed payloads and keyword-compatible variant names.
     * `MATCH` with exhaustive clause validation and payload binding.
     * Variant construction via dot-notation: `Option.Some(10)`.

---

## 4. Compilation Pipeline
PlantLang employs a state-of-the-art compilation chain:
1. **Lexing & Parsing**: AST construction with depth-aware tokenization.
2. **Type Checking**: Semantic validation including Contract Law (depth ≤ destination).
3. **LLVM IR Generation**: Conversion to LLVM SSA-based IR:
   - Arena allocation (`arenaAlloc`) for all variable storage.
   - Depth tracking with automatic arena reset on scope boundaries.
   - Zero-cost division-by-zero checks when inside `WEATHER` blocks.
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
| **Struct** | `SHAPE Point { x(NUM), y(NUM) }.` | User-defined aggregate type. |
| **Method** | `ACTION (self(Point)) move(x(NUM), y(NUM)),` ... `/ACTION.` | Typed method with SELF receiver. |
| **Method Call** | `REAP _ FROM p:move, 5, 10.` | Colon-dispatch method invocation. |
| **Tagged Union** | `CHOICE Option { Some(NUM), None }.` | Sum type with payload variants. |
| **Variant Construction** | `Option.Some(10)` / `Option.None` | Create tagged union values. |
| **Pattern Match** | `MATCH opt { Some(v) -> { SHOW v } None -> { SHOW 0 } }.` | Exhaustive case analysis with binding. |
| **Array Push** | `PUT val INTO xs.` | Amortized-O(1) append. |
| **Array Pop** | `TAKE val FROM xs.` | Pop from end with shrink. |

---

## 6. QA & Quality Assurance
The **v0.25.0** release is verified by an automated regression suite:
* **~613 Total Assertions** across twelve test suites (LLVM backend, C codegen, parser migration, diagnostics, tokenizer, Phase 7 — Module System & FFI, Phase 8 — Standard Library, Phase 9 — Structs, Phase 10 — Arrays, Phase 11 — Methods, Phase 12 — Array Growth, Phase 13 — CHOICE & Pattern Matching).
* **LLVM Backend**: 46 smoke tests covering CREATE/SHOW, arithmetic, strings, comparisons, IF/CYCLE/SEASON, ACTION/REAP/GIVE (recursion, SCL params, TX returns), WEATHER/SHELTER exception handling, and TX fat-pointer operations.
* **Module System**: 30 test groups covering IMPORT parsing, cycle detection, path resolution, error messages, and FFI syntax.
* **Standard Library**: 8 integration tests covering std/ path resolution, I/O and string module parsing, prelude injection, and end-to-end FFI calls.
* **Structs & Methods**: 117 tests covering SHAPE declaration, instantiation, field access/mutation, method dispatch, SELF receiver, type checking, LLVM codegen parity.
* **Dynamic Arrays**: 122 tests covering push/pop, capacity growth, type checking, LLVM codegen, interpreter parity.
* **CHOICE & MATCH**: 64 tests covering variant declaration, construction, member access, MATCH exhaustiveness, payload binding, type checking, interpreter execution.
* **Parity Guarantee**: Exact output matching between the Interpreter and Native Compiler via `llc` + `gcc`.
* **Performance**: ~15,000x execution speedup on iterative loops via LLVM optimization compared to the JS interpreter.

---

## 7. Roadmap & Future Scope

### ✅ Complete (v0.25.0)
- Primitives (`NUM`, `SCL`, `TX`, `FACT`)
- Control Flow (`IF`, `CYCLE`, `SEASON`)
- Functions (`ACTION`, `REAP`, `GIVE` with recursion)
- Exception Handling (`WEATHER`, `SHELTER`, `CALM`)
- Rooted Depth System (arena-based deterministic memory)
- LLVM native compilation with full depth tracking
- Division-by-zero detection with error propagation
- Module System (`IMPORT` with cycle detection and AST merging)
- FFI (`ACTION ... -> external.` for native C interop)
- Standard Library Foundation (`std/io`, `std/string`, `std/prelude`)
- Core Runtime Bridge (`core/runtime_bridge.c` — 10 FFI targets)
- **Struct Types** (`SHAPE` with fields, instantiation, access/mutation)
- **Methods** on structs (typed `SELF` receiver, colon-dispatch call syntax)
- **Dynamic Arrays** (`PUT`/`TAKE` push/pop with amortized capacity doubling)
- **Tagged Unions** (`CHOICE` with payload variants)
- **Pattern Matching** (`MATCH` with exhaustive validation and binding)

### 🔜 In Progress / Planned (v0.26.0)
- `MAP` hash table type with LINK/key access
- `SPECIES` / `BLOOM` object-oriented constructs in LLVM backend
- `LIST` operations (`SORT`, `SHAKE`, `COUNT`, ...) in LLVM backend
- Expanded standard library (math, lists, maps modules)
- `TAP` file I/O and `HARVEST` networking in LLVM backend

---

*PlantLang v0.25.0 — Compiled via LLVM. Struct types. Tagged unions. Pattern matching.*