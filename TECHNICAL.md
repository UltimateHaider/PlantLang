# PlantLang Technical Reference

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

## 7. Test Suite Architecture

### Test Files
- `tests/test_llvm_codegen.js` — 37 parity tests: each fixture runs via interpreter AND compiled via `llc` + `gcc`, asserts identical stdout
- `tests/test_codegen.js` — 9 parity tests for the C backend
- `tests/test_parser_migration.js` — 109 tests covering AST node construction, execution, error messages, and legacy interpreter compatibility
- `tests/test_diagnostics.js` — 45 tests for error panel rendering with visual caret

### Test Methodology
Each test:
1. Parses the source with the real parser
2. Generates LLVM IR (or C)
3. Compiles to a native binary via `llc -O2` + `gcc -no-pie -lm`
4. Runs the binary and captures stdout
5. Compares against the interpreter's output — must match exactly
