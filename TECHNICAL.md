# PlantLang Technical Reference

## 0. Module System & Standard Library

See sections 7–8 below for the IMPORT/FFI and Standard Library architecture.

## 1. Rooted Depth System (Memory Architecture)

### 1.1 Depth Prefixes

Every statement carries a compile-time depth prefix (`\N`) that declares its scope level:

```plantlang
1\ CREATE x(NUM) TO 42.     # depth 1 — root-level variable
2\   CYCLE i FROM 1 TO 10,  # depth 2 — loop scope
3\     CREATE y(NUM) TO i.  # depth 3 — inner body scope
```

The depth is set by the tokenizer: `N\` at the start of a line is consumed as a `DEPTH` token before the statement keyword.

### 1.2 Arena Allocation

Each depth level owns a dedicated 64KB arena slab. The runtime state is two globals:

```llvm
@arena_offsets = global [64 x i64] zeroinitializer   ; bump pointer per depth
@arena_memory  = global [64 x [65536 x i8]] zeroinitializer  ; 64KB per depth
```

Allocation (`core/llvm_codegen.js` — `Module.arenaAlloc`):

```llvm
; allocate 8 bytes from Arena_N
%off_ptr = getelementptr [64 x i64], [64 x i64]* @arena_offsets, i64 0, i64 N
%old_off = load i64, i64* %off_ptr
%new_off = add i64 %old_off, 8
store i64 %new_off, i64* %off_ptr
%ptr = getelementptr [64 x [65536 x i8]], [64 x [65536 x i8]]* @arena_memory, i64 0, i64 N, i64 %old_off
```

The returned `i8*` is bitcast to the appropriate typed pointer (`i64*`, `double*`, `i8**`, `i1*`).

### 1.3 Variable Storage Rules

| Site | Arena Depth | Notes |
|---|---|---|
| `CREATE x(NUM) TO val.` | Statement depth (`node.depth`) | Destination must be ≤ current depth (Contract Law) |
| `REAP x FROM fn, ...` auto-create | Statement depth | Implicitly declares the target variable |
| `CYCLE i FROM ...` loop variable | Loop's statement depth | Fixed `%ptr` address survives iteration reset |
| ACTION function params | Depth 0 | Not reset on GIVE — preserved for caller's recursive frames |

### 1.4 Arena Reset

```llvm
; reset Arena_N to zero offset
%off_ptr = getelementptr [64 x i64], [64 x i64]* @arena_offsets, i64 0, i64 N
store i64 0, i64* %off_ptr
```

This does **not** zero the memory — it only resets the bump pointer. Memory at previously-allocated offsets retains its values until overwritten by the next allocation cycle.

---

## 2. The Unwinding Protocol (Article IX)

Four mechanisms guarantee deterministic memory reclamation:

### 2.1 Natural Exit (Scope Depth Decrease)

`trackDepth(node)` in `core/llvm_codegen.js`:

```javascript
trackDepth(node) {
  const nodeDepth = node.depth !== undefined ? node.depth : m.currentDepth;
  if (nodeDepth === m.currentDepth) return;
  if (nodeDepth > m.currentDepth) { m.currentDepth = nodeDepth; return; }
  // Exiting depths: reset each arena from currentDepth down to nodeDepth+1
  for (let d = m.currentDepth; d > nodeDepth; d--) m.arenaResetDepth(d);
  m.currentDepth = nodeDepth;
}
```

Called from `genStatement` before every statement. When a statement at a shallower depth follows a deeper one, all exited arena levels are reset.

### 2.2 Forced Exit (GIVE Return)

`genGiveStatement` emits the cleanup chain before every `ret`:

```javascript
for (let d = m.currentDepth; d >= 1; d--) m.arenaResetDepth(d);
m.currentDepth = 0;
```

Depth 0 is preserved — function parameters live in Arena_0 and must survive for the caller (critical for recursive functions where each call frame shares global arena state).

### 2.3 Loop Iteration Reset (Article VII)

`genCycle` and `genSeason` save the arena offset at the loop's depth before the body, and restore it after each tick:

```javascript
// Before body:
%save_gep = getelementptr ... i64 N
%saved = load i64, i64* %save_gep

// After body (if not terminated by GIVE):
store i64 %saved, i64* %save_gep   ; restore Arena_N to pre-body offset
```

Also resets any deeper arenas entered during the body:

```javascript
for (let d = endDepth; d > loopDepth; d--) m.arenaResetDepth(d);
```

The loop variable survives because it was allocated before the save point using a fixed `%ptr` address.

### 2.4 Error Unwinding (WEATHER/SHELTER)

When a runtime error (e.g., division by zero) occurs inside a `WEATHER` block:

1. The error check (`emitZeroCheck`) emits:
   ```llvm
   %is_zero = fcmp oeq double %divisor, 0.0
   br i1 %is_zero, label %div.err, label %div.ok
   ```
2. In `div.err` block: stores error info in `@_weather_msg`, `@_weather_type`, `@_weather_flag` globals, then branches to the nearest matching SHELTER handler.
3. The SHELTER handler resets all arenas from `currentDepth` down to `unwindDepth + 1`, then runs the recovery body.
4. After SHELTER, the `CALM` block resets `@_weather_flag` to `false`.

---

## 3. Contract Law (Article III — Depth Validation)

### 3.1 CREATE Destination Rule

A variable's destination depth must be ≤ the current execution depth:

```
═══ ⚠ Contract Violation: Illegal Destination ═══
  Operation:  CREATE
  Variable:   "x"
  Destination: depth 3  (Arena_3)
  Current:     depth 0  (Arena_0)
  Rule: "A seed is not allowed to reside in soil (Arena_M) deeper
         than the soil it was born in (Arena_N)."
  Fix: Use CREATE at depth 0 instead, or specify
       a destination ≤ 0.
```

Enforced in `genCreate` before any allocation.

### 3.2 Access Validation (Reserved)

`checkDepthAccess(varName, varDepth, node, operation)` is available at every variable access point (SET, INCREASE, DECREASE, SHOW, REAP, CYCLE) but currently issues only diagnostic errors (disabled by default). When enabled, it produces:

```
═══ ⚠ Contract Violation: Unauthorized Access ═══
  Operation:  SET
  Variable:   "x"
  Declared at: depth 1  (Arena_1)
  Accessed from: depth 3  (Arena_3)
  Fix: Promote using "\N x -> M = ..."
       or declare a local copy at this depth.
```

---

## 4. LLVM IR Generation Pipeline

### 4.1 Module Structure (`core/llvm_codegen.js`)

```
Lexing & Parsing
    ↓
AST (typed node tree with depth annotations)
    ↓
LLVMGenerator.generate(programNode)
    ├── First pass: collect ACTION declarations
    ├── Generate ACTION function definitions (genFnDef)
    │   ├── Arena-allocate parameters at depth 0
    │   ├── Generate body with depth tracking
    │   └── Default ret i64 0 if no explicit GIVE
    ├── Generate main body with depth tracking
    └── Final arena reset at program exit
        ↓
LLVMGenerator.assemble()
    ├── Target triple, printf/malloc declarations
    ├── Arena globals (conditionally emitted)
    ├── String constants
    └── Function definitions → LLVM IR text
```

### 4.2 Expression Compilation (`ExprCompiler`)

Recursive-descent expression parser with operator precedence:

```
OR        (lowest)
AND
NOT (unary)
comparison  (IS, GREATER THAN, BETWEEN, ...)
additive    (+ -)
multiplicative  (* / % **)
unary       (-)
atom        (numbers, strings, identifiers, parenthesized)
```

Each operation emits LLVM IR directly to `Module.body`. String concatenation uses `@malloc`/`@strcpy`/`@strcat` runtime calls. Type promotion follows: `NUM + SCL → SCL` (int → double via `sitofp`).

### 4.3 Type Coercions (Return Register)

All function return values pass through `i64`:
- `NUM`: direct (i64 → i64)
- `SCL`: `bitcast double %val to i64`
- `TX`: `ptrtoint i8* %val to i64`
- `FACT`: `zext i1 %val to i64`

Caller reverses via the target variable's declared type.

---

## 5. AST Node Types

All nodes inherit from `AstNode` (`core/ast.js`) with `type`, `line`, `column`, `depth`.

### Statement Nodes
| Type | Constructor Fields |
|---|---|
| `CreateStatement` | `identifier`, `varType`, `valueExpr`, `plType` |
| `SetStatement` | `target`, `valueExpr` |
| `ShowStatement` | `valueExpr` |
| `IfStatement` | `branches` (array of `{cond, bodyStatements}`) |
| `CycleStatement` | `iterVar`, `fromExpr`, `toExpr`, `stepExpr`, `sourceExpr`, `bodyStatements` |
| `SeasonStatement` | `condExpr`, `bodyStatements` |
| `WeatherStatement` | `conditionExpr`, `bodyStatements`, `shelterClauses`, `calmClause` |
| `ShelterStatement` | `stormType`, `errVar`, `bodyStatements` |
| `CalmStatement` | `bodyStatements` |
| `GiveStatement` | `valueExpr` |
| `ReapStatement` | `variable`, `source`, `args` |
| `ActionDeclaration` | `name`, `params`, `bodyStatements` |
| `LockStatement` | — |
| `RawStatement` | `text` (fallback for unmigrated constructs) |

### Expression Nodes
| Type | Fields |
|---|---|
| `IdentifierNode` | `name` |
| `LiteralNode` | `value`, `plType` |

---

## 6. Error Handling Architecture

### 6.1 Compile-Time Errors

The LLVM backend produces structured `CodegenError` objects for:
- Unsupported constructs (LIST, MAP, SPECIES, etc.)
- Undeclared variable references
- Contract Law violations (CREATE destination depth)
- Type errors in operations

Errors are collected in `Module.errors` and returned alongside the partial IR for inspection.

### 6.2 Runtime Errors (WEATHER/SHELTER)

The only runtime error currently detected by the LLVM backend is **division by zero** (ZERO_STORM). Detection mechanism:

1. `ExprCompiler.emitDiv` calls `generator.emitZeroCheck(r)` after converting the divisor to double
2. `emitZeroCheck` looks up the nearest ZERO_STORM handler on the `shelterStack`
3. If a handler is found, emits an `fcmp oeq` check that branches to the error setup block
4. The error setup block stores the error message in `@_weather_msg`, sets `@_weather_flag = true`, and branches to the handler
5. If no handler is found (no active WEATHER block), the check is skipped and division proceeds normally (producing ±Infinity per IEEE 754)

### 6.3 Error Globals

```llvm
@_weather_flag = global i1 false
@_weather_type = global i64 0    ; 1 = ZERO_STORM
@_weather_msg  = global i8* null
```

Emitted lazily only when a WEATHER block is present.

---

## 7. Module System (IMPORT) & FFI

### 7.1 IMPORT Resolution Algorithm

The `resolveImports` function in `core/parser.js` handles multi-file program loading:

```
resolveImports(programNode, baseDir, visited = new Set())
  1. Check for cycle: if programNode's absolute path is in `visited`, emit error
  2. Add current path to `visited`
  3. For each ImportStatement in programNode.statements:
     a. Resolve the import path:
        - If starts with "std/" → look in `${PLANTLANG_STD}/std/` or `../std/`
        - If relative → resolve against baseDir
        - If absolute → use as-is
     b. Append ".plnt" if no extension present
     c. Stat the resolved path — if not found, emit file-not-found error
     d. Parse the resolved file (recursive-descent parse)
     e. Recursively call resolveImports on the child's AST
     f. Replace the ImportStatement with the child's parsed statements (AST merge)
  4. Return the modified programNode
```

### 7.2 Cycle Detection

Import cycles are detected during the recursive `resolveImports` pass:

```plantlang
IMPORT "a".   → resolveImports enters "a" → visited = {"a"}
  ↳ IMPORT "b". → resolveImports enters "b" → visited = {"a", "b"}
    ↳ IMPORT "a". → "a" is in visited!
      → ERROR: "IMPORT cycle detected: a → b → a"
```

The `visited` set tracks absolute file paths, so the same file imported from different relative paths is still caught.

### 7.3 AST Merging

After resolution, each `ImportStatement` node is replaced in-place with the imported file's `statements` array:

```javascript
// Before merging:
[ImportStatement("helpers"), ShowStatement]

// After resolving import of helpers.plnt (which contains CreateStatement):
[CreateStatement, ShowStatement]
```

This means depth tracking, type checking, and code generation operate on a flat, merged AST — imported code is indistinguishable from inline code.

### 7.4 FFI Declaration & Stub Mechanism

FFI functions are declared with `-> external` syntax:

```plantlang
ACTION plant_printf(fmt(TX)) -> external.
```

**Parser**: Sets `isExternal = true` on the `ActionDeclaration` node. No body is parsed.

**Type Checker**: Validates that the FFI function's signature matches a known bridge function in the runtime registry. Produces `UNDEFINED_ACTION` warning if no matching external is found.

**LLVM Codegen**: Emits LLVM `declare` IR:

```llvm
declare i64 @plant_printf(i64)
```

The function name is mangled from the PlantLang identifier. Parameters use the standard type coercion (TX → i64 via ptrtoint, NUM → i64, SCL → i64 via bitcast).

**Interpreter**: FFI stubs are pre-registered in the interpreter's runtime. Each stub wraps the corresponding `runtime_bridge.c` function via a JS implementation. When the interpreter encounters a call to an external ACTION, it dispatches to the registered stub instead of looking for a body.

### 7.5 FFI Stub Registration

```javascript
// core/interpreter.js
const FFI_STUBS = {
  plant_printf: (fmt, ...args) => { /* JS printf equivalent */ },
  plant_puts: (s) => { /* JS puts equivalent */ },
  plant_len: (s) => s.length,
  plant_upper: (s) => s.toUpperCase(),
  // ...
};
```

Stubs are registered at interpreter construction time. All 10 `runtime_bridge.c` functions have matching JS stubs.

---

## 8. Standard Library Architecture

### 8.1 Directory Layout

```
std/
├── prelude.plnt    # Auto-injected — TRUE, FALSE, _BOOT
├── io.plnt         # I/O functions — print, println, plant_printf, plant_puts
└── string.plnt     # String functions — len, upper, lower, trim, contains, split, replace, concat
```

### 8.2 Auto-Prelude Injection

Every program automatically imports `std/prelude.plnt` at parse time. This happens in `parser.js` before the main parse:

```javascript
function prelude() {
  const preludePath = path.join(__dirname, '..', 'std', 'prelude.plnt');
  return parseFile(preludePath);
}
```

The prelude provides:
- `TRUE` / `FALSE` — boolean constants
- `_BOOT` — bootstrap marker for runtime initialization
- Core type aliases and utility definitions

### 8.3 std/ Path Resolution

When `IMPORT "std/io"` is encountered:
1. The resolver detects the `std/` prefix
2. Searches for `std/io.plnt` relative to the PlantLang std library root
3. If `PLANTLANG_STD` env var is set, uses that; otherwise defaults to `../std/` from the parser directory

### 8.4 Runtime C Bridge (`core/runtime_bridge.c`)

The C bridge implements 10 FFI targets that compiled PlantLang programs link against:

| FFI Function | C Implementation | Purpose |
|---|---|---|
| `plant_printf` | `int64_t plant_printf(int64_t fmt)` | Formatted output via `printf` |
| `plant_puts` | `int64_t plant_puts(int64_t s)` | String output via `puts` |
| `plant_len` | `int64_t plant_len(int64_t s)` | String length via `strlen` |
| `plant_upper` | `int64_t plant_upper(int64_t s)` | Uppercase (alloc + toupper) |
| `plant_lower` | `int64_t plant_lower(int64_t s)` | Lowercase (alloc + tolower) |
| `plant_trim` | `int64_t plant_trim(int64_t s)` | Trim whitespace |
| `plant_contains` | `int64_t plant_contains(int64_t s, int64_t sub)` | Substring check via `strstr` |
| `plant_split` | `int64_t plant_split(int64_t s, int64_t delim)` | String split |
| `plant_replace` | `int64_t plant_replace(int64_t s, int64_t old, int64_t new)` | String replace |
| `plant_concat` | `int64_t plant_concat(int64_t a, int64_t b)` | String concatenation via `strcat` |

All functions receive and return `int64_t` (TX pointers as `int64_t` via ptrtoint/inttoptr). String operations use `malloc`/`strdup` for heap-allocated results.

---

## 9. Test Suite Architecture

### Test Files
- `tests/test_llvm_codegen.js` — 46 parity tests: each fixture runs via interpreter AND compiled via `llc` + `gcc`, asserts identical stdout
- `tests/test_codegen.js` — 10 parity tests for the C backend
- `tests/test_parser_migration.js` — 109 tests covering AST node construction, execution, error messages, and legacy interpreter compatibility
- `tests/test_diagnostics.js` — 45 tests for error panel rendering with visual caret
- `tests/test_tokenizer.js` — 33 character-level tokenizer verification tests
- `tests/test_phase7_import_ffi.js` — 40 test groups for Module System & FFI
- `tests/test_phase8_stdlib.js` — 28 integration tests for Standard Library
- `tests/test_phase9_structs.js` — 70 tests for SHAPE struct types
- `tests/test_phase10_arrays.js` — 58 tests for dynamic arrays
- `tests/test_phase11_methods.js` — 47 tests for method dispatch
- `tests/test_phase12_arrays_growth.js` — 64 tests for dynamic array growth
- `tests/test_phase13_choices_matching.js` — 64 tests for CHOICE/MATCH
- **Total: 12 test files, ~613 assertions, all green**

### Test Methodology
Each test:
1. Parses the source with the real parser (resolving any `IMPORT` statements)
2. For LLVM/C backend tests: generates IR/C code
3. For LLVM tests: compiles to a native binary via `llc -O2` + `gcc -no-pie -lm` (FFI functions resolved via `core/runtime_bridge.c`)
4. Runs the binary and captures stdout
5. Compares against the interpreter's output — must match exactly

Phase 7 and Phase 8 tests use a custom `check(label, condition)` harness that tests specific parser, resolver, and runtime behaviors without requiring LLVM compilation.
