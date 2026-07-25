# PlantLang Technical Reference

## 0. Module System & Standard Library

See sections 9–10 below for the IMPORT/FFI and Standard Library architecture.

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

The returned `i8*` is bitcast to the appropriate typed pointer (`i64*`, `double*`, `i8**`, `i1*`, `%fat_ptr*`).

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

### 3.3 Block-Depth Contract Law Enforcement

The Block-Depth Contract extends Contract Law with structural scope validation — enforcing that certain statement types only appear at specific depth levels, independent of variable access.

#### 3.3.1 Depth Tracking (Parser)

The parser maintains `this.currentDepth` throughout the parse:

```javascript
// core/parser.js — constructor
this.currentDepth = 0;
```

Depth increments on scope entry and decrements on scope exit:

- **ACTION declaration**: When entering the body (both `,` comma-body and `{` brace-body forms), `this.currentDepth += 1`. On exit (`/ACTION.` or `}`), `this.currentDepth -= 1`.
- **CYCLE block**: When entering the body, `this.currentDepth += 1`. On exit, `this.currentDepth -= 1`. Wrapped in `try/finally` to guarantee decrement even on parse errors.

```javascript
// Comma-body ACTION entry:
this.currentDepth += 1;
const body = this.parseStatementList('ACTION', closers);
this.currentDepth -= 1;

// CYCLE entry (try/finally for guaranteed cleanup):
this.currentDepth += 1;
try {
  node.bodyStatements = this.parseStatementList('CYCLE', closers);
} finally {
  this.currentDepth -= 1;
}
```

#### 3.3.2 enforceDepthContract Method

```javascript
enforceDepthContract(nodeType, minDepth, maxDepth, token) {
  const depth = this.currentDepth;
  if (depth < minDepth || depth > maxDepth) {
    throw new SyntaxStorm(
      `[DepthContractError] ${nodeType} is not allowed at depth ${depth}. ` +
      `Expected depth ${minDepth}${maxDepth !== undefined ? ` to ${maxDepth}` : ''}.`,
      token.line, token.column
    );
  }
}
```

Called at parse time for restricted statement types:

| Statement | Call |
|---|---|
| `ACTION` declaration | `enforceDepthContract('ACTION', 0, 0, token)` — only allowed at Depth 0 |
| `SPECIES` declaration | `enforceDepthContract('SPECIES', 0, 0, token)` — only allowed at Depth 0 |
| `REAP` statement | `enforceDepthContract('REAP', 1, undefined, token)` — only allowed at Depth ≥ 1 |
| `GIVE` statement | `enforceDepthContract('GIVE', 1, undefined, token)` — only allowed at Depth ≥ 1 |
| `CYCLE` statement | `enforceDepthContract('CYCLE', 1, undefined, token)` — only allowed at Depth ≥ 1 |

#### 3.3.3 Second-Pass Validation (Typechecker)

The typechecker's `validateDepthInvariants(ast)` provides a redundant second pass that catches depth violations that may escape the parser (e.g., in dynamically constructed or imported ASTs):

```javascript
validateDepthInvariants(node, currentDepth = 0) {
  const type = node.type;
  if (type === 'ActionDeclaration' || type === 'SpeciesDeclaration') {
    if (currentDepth !== 0) {
      this.diagnostics.push({ message: `...`, line: node.line, column: node.column });
    }
    // Walk body at currentDepth + 1
    if (node.bodyStatements) this._walkDepth(node.bodyStatements, currentDepth + 1);
  } else if (type === 'ReapStatement' || type === 'GiveStatement' || type === 'CycleStatement') {
    if (currentDepth < 1) {
      this.diagnostics.push({ message: `...`, line: node.line, column: node.column });
    }
    // Walk body at same or incremented depth
    ...
  } else { /* walk children at same depth */ }
}
```

Called as pass 2 in `check()`:

```javascript
check(programNode) {
  // Pass 1: existing type checking
  this._checkProgram(programNode);
  // Pass 2: depth invariants
  this.validateDepthInvariants(programNode);
  return this.diagnostics;
}
```

#### 3.3.4 Error Format

DepthContractError produces a `SYNTAX_STORM` with the `[DepthContractError]` prefix, a human-readable message explaining the allowed depth range, and a caret pointing to the violating token:

```
═══ ⚔ SYNTAX_STORM ═══
  [DepthContractError] REAP is not allowed at depth 0.
  Expected depth 1.
    at line 1, column 3
    |
  1 | REAP x FROM f, 5.
    | ^^^
```

#### 3.3.5 Test Coverage

`tests/test_depth_contract.js` contains 13 tests:

**Valid (8 tests):**
- ACTION at depth 0 with comma body
- ACTION at depth 0 with brace body
- REAP inside ACTION body (depth 1)
- CYCLE inside ACTION body (depth 1)
- GIVE inside ACTION body (depth 1)
- SPECIES at depth 0
- REAP at depth 2 (nested CYCLE inside ACTION)
- GIVE at depth 2

**Invalid (5 tests):**
- REAP at depth 0 (top level)
- CYCLE at depth 0 (top level)
- GIVE at depth 0 (top level)
- ACTION inside ACTION (nested depth 1)
- REAP with `\1` depth prefix at top level (parse-time depth vs currentDepth mismatch)

---

## 4. Species Vtable Dispatch System

### 4.1 Vtable Layout

Every species struct gets a hidden `i8*` vtable pointer as field 0:

```llvm
%species.Animal = type { i8*, i64 }           ; vtable ptr + fields
%species.Dog    = type { i8*, i64, i64 }      ; vtable ptr + inherited + new fields
```

The vtable itself is a constant global array of function pointers:

```llvm
@species.Animal.vtable = constant [1 x i8*] [
  i8* bitcast (i64 (i8*, i64)* @Animal_speak to i8*)
]
```

### 4.2 Method Slot Allocation

`_computeMethodSlots()` walks the parent chain to assign stable slot indices:

1. Parent methods get slots 0..N-1 (preserving parent order)
2. Child overrides reuse the same slot as the parent method
3. New methods (not in parent) get slots N, N+1, ...

This ensures the parent vtable is a prefix of the child vtable — a child instance's vtable can be safely indexed using parent slot numbers, enabling dynamic dispatch upcast.

### 4.3 Uniform Calling Convention

Species method functions use a uniform calling convention with `i8*` receiver:

```llvm
define i64 @Animal_speak(i8* %self) {
  ; bitcast to concrete type for field access
  %typed_self = bitcast i8* %self to %species.Animal*
  ...
}
```

This allows any species method to be called through the vtable without type mismatches.

### 4.4 Dispatch Path

```
genMethodCallStatement:
  1. Load vtable pointer from instance field 0
  2. Bitcast i8* to [N x i8*]*
  3. GEP to method slot index
  4. Load function pointer (i8*)
  5. Bitcast to i64 (i8*, i64, ...)*
  6. Call with receiver bitcast to i8*
```

## 5. CHOICE/MATCH LLVM Codegen

### 5.1 Choice Value Layout

CHOICE values are stored as a fixed-size struct:

```llvm
%choice.Option = type { i64, i64 }  ; { tag, payload }
```

- `tag`: variant index (0-based, in declaration order)
- `payload`: variant's value, stored as i64 (all PlantLang types fit in i64: NUM→i64, SCL→bitcast, TX→ptrtoint, FACT→zext)

### 5.2 Variant Construction

**No-payload variant** (`Option.None`):
```llvm
%tagged = insertvalue %choice.Option zeroinitializer, i64 1, 0  ; tag=1
%val    = insertvalue %choice.Option %tagged, i64 0, 1          ; payload=0
```

**Payload-bearing variant** (`Option.Some(10)`):
```llvm
%tagged = insertvalue %choice.Option zeroinitializer, i64 0, 0  ; tag=0
%val    = insertvalue %choice.Option %tagged, i64 10, 1         ; payload=10
```

### 5.3 MATCH Switch Chain

```
genMatchStatement:
  1. Compile subject expression → get CHOICE struct value
  2. Extract tag with extractvalue
  3. For each clause: icmp eq tag, variant_idx
  4. Branch to matching clause body
  5. If binding exists: extractvalue payload, store in arena, register in scope
  6. Execute clause body statements
  7. Branch to merge block
```

### 5.4 MAP `get()` → Option

MAP `get()` returns an `Option<V>` value instead of erroring. The implementation:

1. Probes the map with `genMapHas()` to check key existence
2. If found: probes again with `_emitMapGetValue()`, wraps value in `Option.Some(v)`
3. If not found: returns `Option.None`
4. Both branches store the result in a common arena slot, merged after conditional

The `Option` CHOICE type is registered lazily on first `get()` call:
```javascript
choiceTypes.set('Option', [
  { name: 'Some', type: null },
  { name: 'None', type: null },
]);
```

---

## 6. LLVM IR Generation Pipeline

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

## 7. MAP Hash Table Implementation

### 7.1 Data Layout

MAPs are stored as `%fat_ptr` structs: `{ i8* buckets, i64 len, i64 cap }` — same layout as arrays and TX fat pointers.

Each bucket is a padded struct:
```
{ i1 is_occupied, key_type, value_type }
```

For `MAP[NUM,TX]` this is `{ i1, i64, %fat_ptr }` with ABI alignment:
- Offset 0: i1 (1 byte) + 7 bytes padding
- Offset 8: i64 key (8 bytes)
- Offset 16: %fat_ptr value (24 bytes)
- **Total stride: 40 bytes**

The bucket size is computed by `mapBucketSize()` which accounts for natural alignment padding. Initial capacity is 8 buckets; growth doubles capacity when load factor exceeds 0.75.

### 7.2 Hash Functions

| Key Type | Hash | Implementation |
|---|---|---|
| `NUM` | Identity | Key value used directly (modulo capacity) |
| `TX` | djb2 | Inline LLVM IR loop: `hash = hash * 33 + byte[i]` over string bytes |

### 7.3 Linear Probing

Both `genMapPut` and `genMapHas` implement open-addressing with linear probing:

1. Compute `idx = hash % cap`
2. **Occupied + key matches** → found (overwrite in put, success in has)
3. **Occupied + key differs** → collision → `idx = (idx + 1) % cap` → goto 2
4. **Empty** → not found (insert in put, fail in has)

### 7.4 Growth

When `len >= cap * 3 / 4` during a put:
1. Allocate new bucket array at 2× capacity in the current arena
2. Zero-initialize with `@llvm.memset.p0i8.i64`
3. Loop over old buckets, rehash each occupied entry into the new array using the same probing logic
4. Update the map `%fat_ptr` to point to the new bucket array

### 7.5 Codegen Functions

| Function | Purpose |
|---|---|
| `genCreateMap(node)` | Allocate 8-bucket array, zero-init, build `%fat_ptr`, store and scope-register |
| `emitTxHash(fpReg, node)` | Emit djb2 hash loop over TX buffer → `i64` |
| `genMapPut(node, mapType, mapPtr, key, value)` | Check load factor, grow if needed, probe, insert/overwrite |
| `emitMapGrow(...)` | Allocate 2× array, rehash all entries, update map pointer |
| `genMapHas(node, mapType, mapReg, key)` | Probe loop, return `icmp ne i64 result, 0` as `FACT` |
| `mapBucketSize()` | Compute padded bucket stride (accounts for ABI alignment) |
| `mapBucketLlvmType()` | Return anonymous struct type `{ i1, keyLt, valLt }` |

---

## 8. AST Node Types

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
| `ReapStatement` | `variable`, `source`, `args` — `source.kind` can be `'ACTION'`, `'EXPR'`, `'NOW'`, `'TYPEOF'`, `'SELF'`, `'INSTANCE_OR_LIBRARY'`, `'LITERAL'` |
| `ActionDeclaration` | `name`, `params`, `bodyStatements` |
| `LockStatement` | — |
| `RawStatement` | `text` (fallback for unmigrated constructs) |

### Expression Nodes
| Type | Fields |
|---|---|
| `IdentifierNode` | `name` |
| `LiteralNode` | `value`, `plType` |

---

## 9. Error Handling Architecture

### 9.1 Compile-Time Errors

The LLVM backend produces structured `CodegenError` objects for:
- Unsupported constructs (LIST, MAP without explicit key/value type, SPECIES, etc.)
- Undeclared variable references
- Contract Law violations (CREATE destination depth)
- Type errors in operations

Errors are collected in `Module.errors` and returned alongside the partial IR for inspection.

### 9.2 Runtime Errors (WEATHER/SHELTER)

The only runtime error currently detected by the LLVM backend is **division by zero** (ZERO_STORM). Detection mechanism:

1. `ExprCompiler.emitDiv` calls `generator.emitZeroCheck(r)` after converting the divisor to double
2. `emitZeroCheck` looks up the nearest ZERO_STORM handler on the `shelterStack`
3. If a handler is found, emits an `fcmp oeq` check that branches to the error setup block
4. The error setup block stores the error message in `@_weather_msg`, sets `@_weather_flag = true`, and branches to the handler
5. If no handler is found (no active WEATHER block), the check is skipped and division proceeds normally (producing ±Infinity per IEEE 754)

### 9.3 Error Globals

```llvm
@_weather_flag = global i1 false
@_weather_type = global i64 0    ; 1 = ZERO_STORM
@_weather_msg  = global i8* null
```

Emitted lazily only when a WEATHER block is present.

---

## 10. REAP Expression Sources (Universal REAP)

### 10.1 Background

In v0.29.0 and earlier, `REAP` only accepted `ACTION` sources:
```plantlang
REAP r FROM my_action, arg1, arg2.
```

Native expressions like `SPLIT(str, delim)` or `parts[0]` could not be used directly with REAP — they required wrapping in an FFI `ACTION` declaration.

### 10.2 Parser Changes

`parseReapStatement` in `core/parser.js` now detects expression sources by checking the token after `FROM`:
- If `IDENT/KW` followed by `(` → function call expression (SPLIT, JOIN, COUNT, etc.)
- If `IDENT/KW` followed by `[` → index access expression (`parts[0]`)
- Otherwise → existing ACTION parsing

Expression sources call `parseExpressionSpan()` to build full AST nodes (`StringOpNode`, `IndexAccessNode`, `ListOpNode`) and store them as `source = { kind: 'EXPR', expr: astNode }`.

### 10.3 Typechecker

`_checkReap` handles `EXPR` kind by calling `_inferExprNode(source.expr, scope)` to derive the return type dynamically and set the target variable's type.

### 10.4 Interpreter

`evaluateReapStatement` handles `EXPR` kind:
```javascript
if (kind === 'EXPR') {
  const val = this.evaluateExpressionNode(node.source.expr, soil);
  store(val);
  return { next: 1 };
}
```

### 10.5 LLVM Codegen

`genReapStatement` evaluates the expression via `compileAstExpr`, then auto-creates or stores into the target variable:
```javascript
if (src.kind === 'EXPR') {
  const val = this.compileAstExpr(src.expr);
  // auto-create target variable with inferred type
  const inferType = val.type;
  const ptr = m.arenaAllocTyped(inferType, depth);
  m.scope.set(node.variable, { ptr, plType: inferType, depth });
  // store
  m.emit(`store ${lt} ${val.reg}, ${lt}* ${targetInfo.ptr}`);
}
```

The expression itself handles `sret` allocation internally (e.g., `StringOpNode` allocates an sret slot, calls `@plnt_str_split`, and loads the result). No sret handling is needed in `genReapStatement` for EXPR sources.

### 10.6 Supported Expressions

| Expression | Return Type | LLVM Path |
|---|---|---|
| `SPLIT(str, delim)` | `[TX]` | `%fat_ptr` via `plnt_str_split` sret call |
| `JOIN(arr, delim)` | `TX` | `%fat_ptr` via `plnt_str_join` sret call |
| `COUNT(arr)` | `NUM` | `extractvalue` on `%fat_ptr` length field |
| `arr[index]` | Element type | GEP + load on array pointer |
| `SORT(arr)` | `NUM` (statement) | `plnt_sort_i64` / `plnt_sort_double` call |

---

## 11. Module System (IMPORT) & FFI

### 11.1 IMPORT Resolution Algorithm

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

### 11.2 Cycle Detection

Import cycles are detected during the recursive `resolveImports` pass:

```plantlang
IMPORT "a".   → resolveImports enters "a" → visited = {"a"}
  ↳ IMPORT "b". → resolveImports enters "b" → visited = {"a", "b"}
    ↳ IMPORT "a". → "a" is in visited!
      → ERROR: "IMPORT cycle detected: a → b → a"
```

The `visited` set tracks absolute file paths, so the same file imported from different relative paths is still caught.

### 11.3 AST Merging

After resolution, each `ImportStatement` node is replaced in-place with the imported file's `statements` array:

```javascript
// Before merging:
[ImportStatement("helpers"), ShowStatement]

// After resolving import of helpers.plnt (which contains CreateStatement):
[CreateStatement, ShowStatement]
```

This means depth tracking, type checking, and code generation operate on a flat, merged AST — imported code is indistinguishable from inline code.

### 11.4 FFI Declaration & Stub Mechanism

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

### 11.5 FFI Stub Registration

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

## 12. Standard Library Architecture

### 12.1 Directory Layout

```
std/
├── prelude.plnt    # Auto-injected — TRUE, FALSE, _BOOT
├── io.plnt         # I/O functions — print, println, plant_printf, plant_puts
└── string.plnt     # String functions — len, upper, lower, trim, contains, split, replace, concat
```

### 12.2 Auto-Prelude Injection

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

### 12.3 std/ Path Resolution

When `IMPORT "std/io"` is encountered:
1. The resolver detects the `std/` prefix
2. Searches for `std/io.plnt` relative to the PlantLang std library root
3. If `PLANTLANG_STD` env var is set, uses that; otherwise defaults to `../std/` from the parser directory

### 12.4 Runtime C Bridge (`core/runtime_bridge.c`)

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

---

## 13. Five-Mission Execution Architecture (v0.33.0)

### 13.1 Overview

The Five-Mission Architecture introduces per-action execution modes that alter memory behavior, optimization paths, and isolation levels. Each `ACTION` can be annotated with a mission mode via the `WITH MISSION <MODE>` syntax:

```plantlang
1\ ACTION compute() WITH MISSION FAST,
2\   ...body...
1\ /ACTION.
```

If omitted, the mode defaults to `BALANCED`. Mission declarations are restricted to Depth 0 (top-level ACTION blocks).

### 13.2 The Five Mission Modes

| Mode | Tagline | Behavior |
|---|---|---|
| **BALANCED** | Default — safe and general-purpose | Full type checking, arena-based memory, standard execution |
| **FAST** | Performance-first | Skips safety checks, optimized LLVM codegen path, no boundary validation overhead |
| **SAFE** | Maximum isolation | Sandboxed memory arena (ScopedArena), cannot invoke FAST, SMART, or PERSISTENT callees |
| **SMART** | Adaptive routing | Routes execution based on input size: scalar inline (N < 1000) or parallel vector (N ≥ 1000) |
| **PERSISTENT** | Long-lived objects | Allows creating persistent objects that outlive their creating scope; cannot be invoked from SAFE |

### 13.3 The 5x5 Boundary Handshake Matrix

A function executing under a `FromMode` must validate call permission when invoking a function under a `ToMode`:

| From \\ To | BALANCED | FAST | SAFE | SMART | PERSISTENT |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **BALANCED** | ALLOW | ALLOW | ALLOW | ALLOW | ALLOW |
| **FAST** | ALLOW | ALLOW | **DENY** | ALLOW | ALLOW |
| **SAFE** | ALLOW | **DENY** | ALLOW | **DENY** | **DENY** |
| **SMART** | ALLOW | ALLOW | ALLOW | ALLOW | ALLOW |
| **PERSISTENT** | ALLOW | ALLOW | **DENY** | ALLOW | ALLOW |

**The four forbidden paths and their rationale:**

1. **SAFE → FAST** — `"SAFE is isolated and cannot invoke unguarded FAST code."` — SAFE mode provides maximum isolation; FAST code skips safety checks, which would violate SAFE's contract.
2. **SAFE → PERSISTENT** — `"SAFE cannot create persistent objects that outlive the isolated scope."` — Persistent objects would escape the isolated arena, breaking SAFE's memory containment guarantee.
3. **FAST → SAFE** — `"FAST cannot invoke SAFE due to conflicting performance/safety requirements."` — FAST mode explicitly trades safety for performance; calling SAFE code would reintroduce the safety overhead that FAST was chosen to avoid.
4. **SAFE → SMART** — `"SAFE cannot invoke SMART as it may dynamically route to FAST."` — SMART may route execution to the FAST path for large inputs (N ≥ 1000), which SAFE is not permitted to invoke.

### 13.4 Implementation Architecture

#### 13.4.1 Core Modules

| Module | File | Purpose |
|---|---|---|
| Error Hierarchy | `core/errors.js` | `BoundaryViolationError` with `fromMode`, `toMode`, `scopeId`, `lineContext` |
| Matrix Module | `core/matrix.js` | 5x5 `PERMISSION_MATRIX`, `validateBoundary()`, `formatMatrix()` |
| Mission Dispatcher | `core/dispatcher.js` | `MissionStack`, `ScopedArena`, `MissionDispatcher`, SMART router |
| Parser Extension | `core/parser.js` | `WITH MISSION <MODE>` parsing in `parseActionDeclaration`, `MissionBlockNode` |
| AST Node | `core/ast.js` | `MissionBlockNode` with `mode`, `scopeId`, `action` |
| Typechecker | `core/typechecker.js` | Static boundary validation in `_checkReap` via `validateBoundary()` |

#### 13.4.2 MissionStack

The `MissionStack` maintains the active execution context during call trees:

```
Initial state:          [BALANCED]
Call FAST action:       [BALANCED, FAST]      (push)
Return from FAST:       [BALANCED]             (pop)
```

```javascript
stack.push('FAST');       // enter mode
stack.current();           // → 'FAST'
stack.pop();              // exit mode
```

Guarantees the root `BALANCED` entry can never be popped, ensuring there is always a valid default mode.

#### 13.4.3 ScopedArena

`ScopedArena` provides temporally-scoped memory allocation tied to a `scopeId`, mirroring PlantLang's Rooted Depth System semantics:

```
ScopedArena constructor(scopeId=99, capacity=65536)
  ├── alloc(16)   → returns offset 0, bump to 16
  ├── alloc(32)   → returns offset 16, bump to 48
  ├── write(ptr, data)  → stores bytes at offset
  ├── read(ptr, len)    → retrieves bytes
  ├── reset()    → bumps offset back to 0 (no GC)
  ├── used       → current offset
  └── remaining  → capacity - offset
```

Arenas are isolated by `scopeId` — resetting one does not affect others, enabling SAFE mode's memory containment guarantee.

#### 13.4.4 Adaptive SMART Router

The SMART router selects execution strategy based on input size:

```
routeSMART(actionFn, data):
  if Array.isArray(data) AND data.length >= 1000:
    return executeParallelVector(actionFn, data)   // chunked element-by-element
  else:
    return executeScalarInline(actionFn, data)      // single call with full data
```

#### 13.4.5 MissionDispatcher

The `MissionDispatcher` orchestrates cross-mode calls with three steps:

1. **Validate boundary** via `validateBoundary(fromMode, toMode)` — throws `BoundaryViolationError` on DENY
2. **Push callee mode** onto `MissionStack`
3. **Execute** — uses SMART routing for `SMART` target mode; direct call otherwise
4. **Pop callee mode** in `finally` block (guaranteed cleanup)

### 13.5 Dual Enforcement Points

Boundary validation is enforced at two levels:

1. **Static Check** (`core/typechecker.js`): During `_checkReap`, before registering a REAP call, verifies the caller's current mission mode against the callee's registered mode. Produces `BOUNDARY_VIOLATION` diagnostic errors.

2. **Dynamic Check** (`core/dispatcher.js`): At runtime, `MissionDispatcher.dispatch()` calls `validateBoundary()` before executing the function, catching violations that cannot be statically determined (e.g., dynamic dispatch or indirect calls).

### 13.6 Parser Grammar

```
ACTION name(params) WITH MISSION mode [,|{|->]
```

- `WITH MISSION <mode>` is optional; defaults to `BALANCED`
- If `MISSION` appears without `WITH`, a clear syntax error guides the user
- Only valid at Depth 0 (top-level ACTION declarations)
- Produces a `MissionBlockNode` wrapping the `ActionDeclarationNode`

### 13.7 Test Coverage

| Test File | Tests | Coverage |
|---|---|---|
| `tests/matrix.test.js` | 28 | All 25 matrix transitions (21 ALLOW, 4 DENY) + unknown mode errors + context propagation |
| `tests/dispatcher.test.js` | 47 | MissionStack, ScopedArena alloc/write/read/reset, MissionDispatcher boundary enforcement, SMART threshold (N=999 vs N=1000), multi-hop chains (BALANCED→FAST→SAFE fails, BALANCED→PERSISTENT→FAST succeeds), memory isolation |

---

## 15. Parallel Compilation & Telemetry (v0.33.0)

### 15.1 Overview

v0.33.0 adds four modules enabling parallel code generation, distributed compilation failover, lock-free telemetry, and runtime dispatch orchestration:

| Module | File | Purpose |
|---|---|---|
| ParallelCodegenEngine | `src/compiler/parallel/parallel_codegen.js` | AST DAG splitting, Tarjan cycle detection, weighted load balancing, worker_threads pool |
| RemoteCompilerNode | `src/compiler/distributed/remote_compiler.js` | zlib compression, TCP transport, 100ms timeout → local fallback |
| NonBlockingTelemetry | `src/telemetry/metrics_collector.js` | SharedArrayBuffer ring buffer, lock-free atomic record(), zero-allocation snapshot() |
| RuntimeDispatcher | `src/runtime/dispatcher.js` | enableParallelCodegen() toggle, single-core auto-disable, telemetry integration |

### 15.2 ParallelCodegenEngine

The engine builds a directed acyclic graph (DAG) from the AST to identify independent actions that can be compiled in parallel:

```
buildDag(program):
  for each top-level ACTION:
    add node(name, weight=1 + nestedCallCount)
    if action calls another top-level action:
      add edge(caller → callee)
  detectCycles()  // Tarjan's algorithm
  if cycle found: throw CycleDetectedError
```

**Cycle detection** rejects programs with circular dependencies:
```
1\ ACTION a(), 2\ REAP b FROM b. 1\ /ACTION.
1\ ACTION b(), 2\ GIVE 1 + a(). 1\ /ACTION.
# → ERROR: Cyclic dependency detected: a → b → a
```

**Weighted load balancing** computes a `balance(nodes, k)` that assigns nodes to `k` buckets via round-robin over nodes sorted by weight descending, minimizing the max bucket weight.

**Worker pool** uses `worker_threads` for parallel bitcode assembly. Workers receive a serialized sub-DAG, compile independently, and return their IR chunk for lock-free merge.

### 15.3 RemoteCompilerNode

The remote node compresses compilation payloads and ships them via TCP:

```
1. Serialize AST → JSON buffer
2. zlib.deflateSync(buffer) → compressed
3. net.createConnection(port, host, { timeout: 100 })
4. If connect succeeds:
     send(compressed) → receive(result)
     return result
   Else (timeout or error):
     log warning → fallback to local compile → return local result
```

**Compression**: zlib (deflate) consistently achieves ≥60% reduction on serialized AST payloads. Verified: a 1202-byte AST payload compresses to 480 bytes (60.1% reduction).

**Timeout mechanism**: A 100ms connect timeout (configurable) triggers `socket.destroy()` on expiry. The `Promise.race` pattern ensures the caller receives either the remote result or the local fallback result within 100ms.

### 15.4 NonBlockingTelemetry

The metrics collector uses a **SharedArrayBuffer** ring buffer with lock-free atomic operations:

```
Buffer layout: 128 entries × 64 bytes = 8192 bytes per ring
Entry format (64 bytes):
  bytes 0-7:   timestamp (ms, BigInt)
  bytes 8-15:  value (float64)
  bytes 16-47: name (32 bytes, null-padded)
  bytes 48-63: reserved
```

**write path** (`record(name, value)`):
1. `Atomics.add(writeIndex, 0, 1) % 128` → slot index
2. Detect overflow: `writeBefore - readIndex >= 128` → advance readIndex atomically, increment overflow counter
3. Store timestamp, value, and name via `DataView.setBigInt64` / `setFloat64` / atomics per byte

**read path** (`snapshot()`):
1. Load current `writeIndex` and `readIndex` atomically
2. Compute entry count: `min(writeIdx - readIdx, 128)`
3. Iterate from `readIdx` to `writeIdx`, decoding each entry
4. Return `{ metrics: [...], overflowCount, uptimeNs }`

All operations are O(1) and incur zero GC pressure — no allocations on the write path, and the snapshot allocates exactly one array per call.

### 15.5 RuntimeDispatcher

The `RuntimeDispatcher` provides a single entry point for enabling/disabling parallel codegen:

```javascript
class RuntimeDispatcher {
  constructor() {
    this.cpuCount = os.cpus().length;
    this.enabled = false;
    if (this.cpuCount === 1) {
      console.log('[DISPATCH] Single-core CPU detected. Parallel codegen disabled.');
    }
  }

  enableParallelCodegen() {
    if (this.cpuCount < 2) return;  // no-op on single core
    this.enabled = true;
    console.log('[DISPATCH] Parallel codegen enabled via API.');
  }

  disableParallelCodegen() {
    this.enabled = false;
    console.log('[DISPATCH] Parallel codegen disabled via API.');
  }
}
```

**Single-core auto-disable**: On machines with 1 logical CPU, `enableParallelCodegen()` is silently ignored — there are no cores to parallelize across.

**Telemetry integration**: When enabled, the dispatcher wires `NonBlockingTelemetry.record()` calls into the compilation pipeline to capture per-action compile times and node counts.

### 15.6 Test Coverage

| Test File | Tests | Coverage |
|---|---|---|
| `tests/v0.33.0_parallel.test.js` | 60 | DAG empty/single/independent/cycle, weighted balance buckets/assignments/uniqueness, compression ratio ≥60% / small payload, 100ms timeout fallback, telemetry snapshot entries/values/overflow/1000-write stress, dispatcher create/enable/disable/single-core, 20-node benchmark with 2/4/8 worker balance ratios |

---

## 16. Zero-Trust Security & Audit Architecture (v0.34.0)

### 17.1 Overview

v0.34.0 adds three security modules implementing non-blocking audit logging, mutual TLS with JWT authentication, and capability-based sandboxing:

| Module | File | Purpose |
|---|---|---|
| NonBlockingAuditLogger | `src/security/audit/audit_logger.js` | SAB ring buffer, SHA256 hash chain, async Worker flush, verifyIntegrity() |
| mTLSJwtGuard | `src/security/network/mtls_jwt_guard.js` | TLS 1.3 mTLS cert loading, RS256/Ed25519 JWT verification, anti-replay |
| CapabilityGuard | `src/security/sandbox/capability_guard.js` | Zero-trust defaults, granular capability matrix, syscall filtering |

### 17.2 NonBlockingAuditLogger

**Ring buffer layout:** SharedArrayBuffer with 4 Int32 header slots (writeIndex, readIndex, overflowCount, committedIndex) followed by N entries of 256 bytes each.

Entry layout (256 bytes):
```
bytes 0-7:    timestamp (ms, BigInt LE)
byte 8:       eventType (ASCII char code)
bytes 9-191:  data (183 bytes, UTF-8 null-padded)
bytes 192-223: prevHash (SHA256 of previous entry, 32 bytes)
bytes 224-255: hash (SHA256 of this entry, 32 bytes)
```

**Hash chain:** `hash_n = SHA256(timestamp_n + eventType_n + data_n + hash_{n-1})`. The first entry uses `hash_{0} = 32 zero bytes` as the previous hash. `verifyIntegrity()` walks the chain from the current read cursor, verifying each entry's stored hash against a recomputation and checking `prevHash` linkage.

**Overflow detection:** When `writeBefore - readIndex >= entryCount`, the read index is advanced atomically and an overflow counter is incremented. A synchronous fallback flush is triggered.

**Async worker:** A background `Worker` thread receives flush batches via `postMessage`. Batch size is 50 entries; a `shutdown` message triggers final flush.

### 17.3 mTLSJwtGuard

**Certificate loading:** Reads PEM files from `process.env.MTLS_CERT`, `process.env.MTLS_KEY`, `process.env.MTLS_CA` or explicit file paths. Provides `getTLSOptions()` returning a `tls.createServer`/`tls.connect`-compatible options object with `minVersion: 'TLSv1.3'`.

**JWT verification pipeline:**
1. Decode header (algorithm detection: RS256 or Ed25519)
2. Decode payload (expiry check, jti extraction)
3. Verify signature using `crypto.createVerify('RSA-SHA256')` for RS256 or `crypto.verify(null, ...)` for Ed25519
4. Check jti against in-memory Set for replay detection

**Error differentiation:**

| Condition | Error Code | Log Level |
|---|---|---|
| Expired `exp` claim | `EXPIRED` | WARN |
| Signature mismatch | `FORGERY` | SECURITY_ALERT |
| jti reuse | `REPLAY` | SECURITY_ALERT |
| mTLS peer cert null/invalid | `MTLS_FAILURE` | FATAL |

### 17.4 CapabilityGuard

**Default capability matrix:**

| Mode | Permissions |
|---|---|
| SAFE | (none) |
| BALANCED | FILE_READ, NET_CONNECT |
| FAST | FILE_READ, FILE_WRITE, NET_CONNECT |
| SMART | FILE_READ, FILE_WRITE, NET_CONNECT |
| PERSISTENT | FILE_READ, FILE_WRITE, NET_CONNECT, NET_LISTEN |

**API:**
- `grantPermission(mode, capability)` — add capability to a mode
- `revokePermission(mode, capability)` — remove capability
- `hasPermission(mode, capability)` — check without throwing
- `checkPermission(mode, capability, resource?)` — throws `CapabilityViolationError` on denial
- `enforceSandbox(mode, action, resource?)` — syscall-level filter for SAFE mode; blocks `execve`, `ptrace`, `fork`, `clone`, `kill`
- `onViolation(callback)` — register hook for CRITICAL audit events
- `resetToDefaults()` — restore initial permission sets

### 17.5 Test Coverage

| Test File | Tests | Coverage |
|---|---|---|
| `tests/v0.34.0_security.test.js` | 91 | Audit logger: record/snapshot/integrity/overflow/fast-path benchmark/hash chain verification/prev hash chaining; mTLS JWT: RS256 valid/expired/forged/replay/Ed25519/malformed/cert expiry/peer verification; Capability: SAFE zero-default/grant/revoke/syscall blocking/violation hooks/reset/per-mode defaults/duplicate idempotence; Integration: violation detail capture/benchmark throughput |

---

## 17. Test Suite Architecture

### Test Files
- `tests/test_llvm_codegen.js` — 50 parity tests: each fixture runs via interpreter AND compiled via `llc` + `gcc`, asserts identical stdout
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
- `tests/test_phase14_maps.js` — 17 tests for MAP hash tables
- `tests/test_phase15_for_in.js` — 19 tests for FOR...IN loops
- `tests/test_phase16_structs.js` — 16 tests for STRUCT types
- `tests/test_phase17_species.js` — 10 tests for SPECIES/BLOOM OOP
- `tests/test_phase18_lists.js` — 15 tests for native LIST operations (COUNT, FIRST, LAST, SUM)
- `tests/test_phase21_runtime.js` — 20 tests for C runtime FFI: math, sort, string split/join (FFI and native via REAP), 70KB large-string stress test
- `tests/test_depth_contract.js` — 13 tests for Block-Depth Contract Law Enforcement (valid: ACTION/SPECIES at depth 0, REAP/GIVE/CYCLE inside ACTION; invalid: REAP/CYCLE/GIVE at depth 0, nested ACTION, depth prefix mismatch)
- `tests/matrix.test.js` — 28 tests for the 5x5 Boundary Handshake Matrix (all 25 transitions + error paths + context propagation)
- `tests/dispatcher.test.js` — 47 tests for MissionStack, ScopedArena, MissionDispatcher, SMART routing, multi-hop call chains, memory isolation
- `tests/runtime.test.js` — 70 tests for Local Runtime & Isolation Layer (BumpAllocator alignment/overflow/escalation, ARCHeap retain/release/cycle detection/GC.cycle, SafeChannel all 4 mechanisms, MissionContext diagnostics/metrics/tracing, ProcessPool heartbeat simulation)
- `tests/v0.33.0_parallel.test.js` — 60 tests for Parallel Compilation & Telemetry (DAG/cycle detection, weighted load balancing, network compression ≥60%, 100ms timeout fallback, telemetry ring buffer/snapshot, dispatcher auto-disable, 20-node speedup benchmark suite)
- `tests/v0.34.0_security.test.js` — 91 tests for Zero-Trust Security & Audit (audit logger hash chain/integrity/overflow, mTLS JWT valid/expired/forged/replay/Ed25519, capability sandboxing SAFE zero-default/grant/revoke/syscall blocking/violation hooks, benchmark suite)
- `tests/v0.40.0_distributed.test.js` — 34 tests for Geo-Aware Cycles, Dynamic Replica Rebalancing & Stream Compaction (GeoTopologyManager latency matrix/optimal nodes, StreamCompactor binary compress/decompress round-trip, DistributedCycleEngine geo-aware executeCycleBlock, ReplicaManager handleNodeJoin/handleNodeLeave rebalancing)
- `tests/v0.41.0_native_net_governance.test.js` — 69 tests for Integrated Testing, Native Networking & CodeWords Governance (CodeWordsChecker directive parsing/permission/AST security, TestRunner SUITE/VERIFY/nested/runAll, CodeWords+TestRunner integration in plantc test pipeline)
- `tests/v0.42.0_c_backend_parity.test.js` — 31 tests for C Backend Parity & Legacy Realignment (PlantMap create/set/get IR, LINK/ForInStatement/WeatherStatement/SpeciesDeclaration codegen, CodeWords zero false-positives, pipeline integration)
- `tests/v0.43.0_file_io_types_const.test.js` — 81 tests for Native File I/O, Constant Folding & Type Infrastructure (file read/write/exists/delete, string split/trim/index_of, AST constant folding, ENUM/TYPE/CONST declarations, CodeWords file I/O directives)
- **Total: 30+ test files, ~1000+ tests, all green**

### Test Methodology
Each test:
1. Parses the source with the real parser (resolving any `IMPORT` statements)
2. For LLVM/C backend tests: generates IR/C code
3. For LLVM tests: compiles to a native binary via `llc -O2` + `gcc -no-pie -lm` (FFI functions resolved via `core/runtime_bridge.c`)
4. Runs the binary and captures stdout
5. Compares against the interpreter's output — must match exactly

Phase 7 and Phase 8 tests use a custom `check(label, condition)` harness that tests specific parser, resolver, and runtime behaviors without requiring LLVM compilation.

## 18. Cluster Architecture & Distributed Memory (v0.35.0)

The v0.35.0 cluster architecture provides production-grade distributed runtime capabilities: decentralized node discovery, circuit-breaker-backed request routing, and consistent-hash-based distributed data storage.

### 18.1 NodeRegistry — Heartbeat-based Node Lifecycle

**File:** `src/cluster/discovery/node_registry.js`

The `NodeRegistry` implements a topology manager with three health states:

| State | Meaning | Entry Condition |
|---|---|---|
| `HEALTHY` | Node responding normally | Initial registration or heartbeat received |
| `DEGRADED` | Node missed ≥ ceil(threshold/2) heartbeats | Missed beat threshold halfway |
| `OFFLINE` | Node missed ≥ threshold heartbeats | Missed beat threshold reached |

**Architecture:**
```
NodeRegistry (extends EventEmitter)
├── Map<nodeId, NodeState>
│   ├── id, state, firstSeen, lastHeartbeat, missedBeats
│   └── cpuUtil, heapUsage, activeWorkers (telemetry)
├── setInterval(_checkHeartbeats, heartbeatInterval)
│   └── each tick: increment missedBeats if (now - lastHeartbeat) ≥ interval
│       → DEGRADED at ceil(threshold/2)
│       → OFFLINE at threshold
├── configure(key, value) — MISSION CONFIG overrides
│   ├── HEARTBEAT_INTERVAL (100-10000ms)
│   └── HEARTBEAT_THRESHOLD (2-10)
└── Events: node:registered, node:healthy, node:degraded, node:offline
```

**MISSION CONFIG integration:**
```javascript
this._heartbeatInterval = options.heartbeatInterval
    || parseInt(process.env.HEARTBEAT_INTERVAL, 10) || 1000;
this._failureThreshold = options.heartbeatThreshold
    || parseInt(process.env.HEARTBEAT_THRESHOLD, 10) || 3;
```

### 18.2 ClusterRouter & CircuitBreaker — Weighted Least-Connections Routing

**File:** `src/cluster/router/cluster_router.js`

The `ClusterRouter` implements weighted least-connections load balancing with per-node circuit breakers:

```
dispatch(action, payload)
├── _selectTarget()
│   └── alive nodes → sort by activeConnections ASC, cpuUtil ASC → lowest wins
├── execute on target via _executeOnNode()
│   └── on success: cb.recordSuccess()
│   └── on failure: cb.recordFailure() → _selectBackup() failover
│       └── no backup available: throw original error
│       └── backup fails too: throw aggregated error
└── mTLSJwtGuard.verifyRequest() integration on each dispatch
```

**CircuitBreaker** (per-node, not shared):
```
┌─────────┐  errorRate ≥ threshold  ┌────────┐  cooldown expired  ┌──────────┐
│ CLOSED  │ ──────────────────────→ │  OPEN  │ ─────────────────→ │ HALF-OPEN │
│ (normal)│                         │ (reject)│                  │ (probing)│
└─────────┘                         └────────┘                  └──────────┘
      ↑                                │                              │
      └────────────────────────────────┘──────────────────────────────┘
         recordSuccess() on success      recordFailure() on probe fail
```

**Error rate computation (sliding window):**
```javascript
get errorRate() {
    const total = this._successes + this._failures;
    return total === 0 ? 0 : this._failures / total;
}
// Breaker trips when: total ≥ 10 AND errorRate ≥ threshold
```

**MISSION CONFIG:**
- `CIRCUIT_BREAKER_THRESHOLD` → `this._errorThreshold`
- `CIRCUIT_BREAKER_COOLDOWN` → `this._cooldownMs`

### 18.3 DistributedHeap & ConsistentHashRing — SHA-256 Ring Storage

**File:** `src/cluster/memory/distributed_heap.js`

The `DistributedHeap` wraps a `ConsistentHashRing` for deterministic key-to-node mapping:

```
┌─────────────────────────────────────────────────┐
│ DistributedHeap                                  │
│ ├── ConsistentHashRing                           │
│ │   ├── SHA-256(key) → BigInt (hash space)       │
│ │   ├── virtual nodes (default 128 per node)     │
│ │   ├── sorted entry ring (binary search lookup) │
│ │   └── configure("CONSISTENT_HASH_VNODES", N)   │
│ ├── Map<key, { value, owner, leaseExpiry }>      │
│ ├── Map<actorId, owner>  (stateful actors)       │
│ ├── get(key), put(key, val), delete(key)         │
│ ├── registerActor(id), setActorState(id, state, caller) │
│ ├── computeDataKeyMigration(existingKeys)        │
│ ├── computeMigrationStats(newNodeId)             │
│ ├── startGC(intervalMs) — lazy + periodic GC     │
│ └── removeNode(nodeId) — re-owns entries via ring│
└─────────────────────────────────────────────────┘
```

**Consistent hash ring implementation:**
```javascript
_hashKey(key) {
    const h = crypto.createHash('sha256').update(key).digest();
    // First 8 bytes → BigInt
    let val = 0n;
    for (let i = 0; i < 8; i++) val = (val << 8n) + BigInt(h[i]);
    return val;
}
```

**Virtual node distribution:** Each physical node is placed on the ring `vnodes` times. `getNode(key)` finds the nearest clockwise virtual node, then returns the owning physical node. With 128 virtual nodes per physical node, 1000 keys distribute with a ratio ≥ 0.98 between 2 nodes.

**Lease-based GC:**
```javascript
put(key, value) {
    const owner = this._ring.getNode(key);
    const entry = { value, owner, leaseExpiry: Date.now() + this._leaseDuration };
    this._store.set(key, entry);
}
// get() checks leaseExpiry; expired keys return null + are deleted
// startGC() runs periodic sweeps via setInterval
```

**Actor ownership & proxy detection:** `registerActor(id)` assigns an owner via `_ring.getNode(id)`. `setActorState(id, state, callerId)` checks ownership:
```javascript
if (callerId !== owner) {
    return { proxied: true, owner };
}
return { proxied: false };
```

**Node removal:** `removeNode(nodeId)` removes the node from the ring and re-owns all stored entries by re-querying `_ring.getNode(storeKey)`, ensuring zero data loss.

### 18.4 Integration Points

| Component | Integration |
|---|---|
| ClusterRouter ← NodeRegistry | `_selectTarget()` queries `reg.getAliveNodes()` |
| ClusterRouter ← CircuitBreaker | Per-node breaker via `getCircuitBreaker(nodeId)` |
| ClusterRouter ← mTLSJwtGuard | Optional JWT verification before `_executeOnNode()` |
| DistributedHeap ← ConsistentHashRing | `_ring.getNode(key)` for all data placement |
| MISSION CONFIG | `configure()` on NodeRegistry, CircuitBreaker, ConsistentHashRing |

### 18.5 Test Coverage

- `tests/v0.35.0_cluster.test.js` — 88 tests:
  - NodeRegistry: register/unregister, heartbeat, telemetry, DEGRADED/OFFLINE state machine, MISSION CONFIG
  - CircuitBreaker: CLOSED/OPEN/HALF-OPEN transitions, error rate, cooldown, reset, MISSION CONFIG
  - ClusterRouter: least-connections selection, CPU-weighted ties, dispatch, failover, aggregated errors, benchmark
  - ConsistentHashRing: add/remove node, distribution ratio (≥0.98), stability, migration, vnode config
  - DistributedHeap: put/get/delete, actors (owner, proxied), lease expiry GC, removeNode re-own, migration stats, benchmarks
  - Total: 88 tests, all green

## 19. Geographic Routing & State Governance Engine (v0.36.0)

The v0.36.0 geo-routing engine provides shared state governance with dual-path consensus, bounded static call-graph affinity analysis, and adaptive SMART execution routing across local CPU, remote nodes, and GPU pipelines.

### 19.1 ShareGovernance — SHARED_READ / SHARED_WRITE State Engine

**File:** `src/cluster/config/share_governance.js`

```
ShareGovernance (extends EventEmitter)
├── SHARED_READ (Read Store)
│   ├── declareReadOnly(key, value) — versioned snapshot
│   ├── read(key) — O(1) local lookup, zero contention { value, version, source }
│   ├── invalidate(key, newValue) — bumps version, enqueues TCP Gossip
│   └── receiveGossip(data) — applies remote invalidation if version > local
├── SHARED_WRITE (Write Store)
│   ├── declareMutable(key, consensusMode) — RAFT or CRDT
│   ├── write(key, value) — consensus-dependent write path
│   ├── readWrite(key) — { value, consensus, committed }
│   ├── RAFT path:
│   │   ├── Append entry to log, replicate to followers
│   │   ├── Commit on majority (≥ floor((peers+1)/2))
│   │   └── Emit write:committed / write:replication_failure
│   ├── CRDT path:
│   │   ├── LWW Register with lamport clock + nodeId tiebreak
│   │   ├── Local writes always succeed (same-nodeId override)
│   │   └── crdtMerge(key, delta) — remote delta merge
│   └── RAFT _replicateToFollower() — pluggable transport
├── TCP Gossip Layer
│   ├── _gossipQueue — batched outbound invalidation/delta messages
│   ├── _flushGossip() — periodic send to peers via gossip:send event
│   ├── receiveGossip(data) — process read-only, raft_commit, crdt_delta
│   └── _startGossipFlush() — setInterval at GOSSIP_PROPAGATION_MS
├── Directive Parsing
│   └── parseDirective("SHARE CONFIG <KEY> READ_ONLY|MUTABLE [CONSENSUS=RAFT|CRDT]")
└── MISSION CONFIG
    ├── GOSSIP_PROPAGATION_MS (10-1000, default 50)
    └── CONSENSUS_ENGINE (RAFT|CRDT, default RAFT)
```

**O(1) local read guarantee:**
```javascript
read(key) {
    const entry = this._readStore.get(key);
    if (!entry) return null;
    return { value: entry.value, version: entry.version, source: 'local' };
}
```
No locks, no consensus involvement, no network calls — pure Map lookup.

**TCP Gossip invalidation propagation:**
```javascript
_flushGossip() {
    const batch = this._gossipQueue.splice(0, 50);
    for (const peerId of this._peers) {
        this.emit('gossip:send', { target: peerId, batch, origin: this._nodeId });
    }
}
receiveGossip(data) {
    for (const msg of data.batch) {
        if ((msg.type === 'read_only') && (!existing || msg.version > existing.version)) {
            this._readStore.set(msg.key, { value: msg.value, version: msg.version, ... });
        }
    }
}
```

### 19.2 CallGraphAnalyzer — Bounded Static Affinity Analysis

**File:** `src/cluster/affinity/call_graph_analyzer.js`

```
CallGraphAnalyzer
├── addFunction(name, calls[]) — build adjacency matrix
├── setEdgeWeight(caller, callee, weight) — weighted graph
├── getDepth(name, visited, depth) — bounded depth traversal
│   └── hard cap at CALL_GRAPH_MAX_DEPTH (default 3, range 1-10)
├── computeAffinityGroups()
│   └── Louvain-inspired community detection:
│       └── for each unassigned node:
│           └── BFS to neighbors within depth limit
│           └── modularity gain ≈ (internalWeight / totalWeight) - (depth / maxDepth) × 0.1
│           └── gain ≥ 0.1 → add to group
├── computePlacement(nodeIds[]) — static affinity group → node mapping
├── getGroupForFunction(name) — lookup runtime
├── buildFromAST(astFunctions) — factory from parsed AST
└── MISSION CONFIG: CALL_GRAPH_MAX_DEPTH (1-10, default 3)
```

**Bounded depth guarantee:** `getDepth()` stops recursion at `this._maxDepth`:
```javascript
getDepth(name, visited = new Set(), depth = 0) {
    if (depth > this._maxDepth) return this._maxDepth;
    if (visited.has(name)) return depth;
    // ... traverse up to maxDepth
}
```

**Louvain-inspired clustering:**
```javascript
_louvainCommunityDetect(startNode, adjacency, assigned) {
    for each node in BFS from startNode:
        internalWeight = Σ weighted edges to already-grouped nodes
        totalWeight = Σ all weighted edges
        modularityGain = (internalWeight / totalWeight) - (depth / maxDepth) × 0.1
        if gain ≥ 0.1 → add to current affinity group
    return group
}
```

### 19.3 SmartExecutionRouter — Adaptive Triage Router

**File:** `src/cluster/router/smart_execution_router.js`

```
SmartExecutionRouter (extends EventEmitter)
├── selectTarget(action, payload) → { target, reason, ... }
│   ├── LOCAL_CPU: default — no offload conditions met
│   ├── REMOTE_NODE: localCpuLoad > 0.7 AND remote latency < maxLatencyMs
│   └── GPU_ACCELERATED: isMatrixOrVectorOp() AND payloadSize ≥ gpuMinBytes AND GPU pipeline registered
├── route(action, payload) → async dispatch to selected target
├── Decision Metrics:
│   ├── estimatePayloadSize(payload) — NUM=8B, TX=length, Array=N×element
│   ├── isMatrixOrVectorOp(action) — keyword match (mat, vec, tensor, fft, ...)
│   ├── measureLatency(nodeId) — cached multi-tap measurement (5s TTL)
│   └── updateLocalCpuLoad(load) — from telemetry
├── GPU Pipeline Management
│   ├── registerGpuPipeline(id), unregisterGpuPipeline(id)
│   └── hasGpuPipeline()
└── MISSION CONFIG
    ├── SMART_ROUTE_GPU_MIN_BYTES (65536 - 1073741824, default 1048576)
    └── SMART_ROUTE_MAX_LATENCY_MS (1-100, default 15)
```

**Routing triage decision tree:**
```javascript
selectTarget(action, payload) {
    payloadSize = estimatePayloadSize(payload)
    isVectorOp = isMatrixOrVectorOp(action)
    if (isVectorOp && payloadSize ≥ gpuMinBytes && hasGpuPipeline())
        return GPU_ACCELERATED
    if (alive nodes exist && localCpuLoad > 0.7)
        candidates = nodes with latency < maxLatencyMs
        if (candidates.length > 0) return REMOTE_NODE (lowest latency)
    return LOCAL_CPU
}
```

**Overhead guarantee:** `selectTarget()` measures `Date.now()` diff and emits `router:overhead_warning` if > 0.05ms. Benchmark: 1000 calls in < 50ms average.

### 19.4 Integration Points

| Component | Integration |
|---|---|
| ShareGovernance ← NodeRegistry | `_peers` from `reg.getAliveNodes()` + `node:registered`/`node:offline` events |
| ShareGovernance → TCP Gossip | `gossip:send` events consumed by transport layer |
| CallGraphAnalyzer → ShareGovernance | Affinity placement can feed `SHARE CONFIG` directives |
| SmartExecutionRouter ← NodeRegistry | `_selectTarget()` queries `reg.getAliveNodes()` for REMOTE_NODE candidates |
| SmartExecutionRouter → GPU | `registerGpuPipeline()` / event-based pipeline management |
| MISSION CONFIG | `configure()` on ShareGovernance, CallGraphAnalyzer, SmartExecutionRouter |

### 19.5 Test Coverage

- `tests/v0.36.0_geo_routing.test.js` — 125 tests:
  - ShareGovernance SHARED_READ: declare, O(1) read, invalidate, version bump, benchmark (100K reads < 100ms), directive parsing
  - ShareGovernance Gossip: peer propagation, invalidation broadcast, within GOSSIP_PROPAGATION_MS timing
  - ShareGovernance SHARED_WRITE RAFT: declareMutable, write with/without followers, commit index, sequential convergence
  - ShareGovernance SHARED_WRITE CRDT: LWW write, bidirectional convergence, sequential convergence
  - CallGraphAnalyzer: addFunction, edge weights, getDepth bounded at maxDepth, affinity group detection, computePlacement, buildFromAST
  - SmartExecutionRouter: GPU pipeline registration, payload estimation, matrix/vector detection, triage transitions (LOCAL/REMOTE/GPU), latency measurement, benchmark (1000 calls < 50ms)
  - MISSION CONFIG: all 5 directives validated (range enforcement, roundtrip)
  - Integration: end-to-end scenarios combining governance + affinity + routing
  - Total: 125 tests, all green

## 20. Distributed Cycles & Replica Governance Engine (v0.37.0)

The v0.37.0 distributed cycles engine provides partitioned loop distribution across cluster workers, stateless/stateful replication strategies via ReplicaManager, and dual-mode result aggregation via ReapAggregator.

### 20.1 ReplicaManager — Stateless Routing & Stateful Primary-Backup

**File:** `src/cluster/replica/replica_manager.js`

```
ReplicaManager (extends EventEmitter)
├── Stateless Routing
│   ├── ROUND_ROBIN: sequential target rotation
│   │   └── selectStatelessTarget() → next node in round-robin order
│   └── LEAST_CONNECTIONS: node with fewest active connections
│       └── selectStatelessTarget() → lowest-connection alive node
├── Stateful Primary-Backup
│   ├── assignPrimary(actorId) → { primary, backups }
│   ├── getPrimary(actorId) → nodeId | null
│   ├── getBackups(actorId) → nodeId[]
│   ├── replicate(actorId, mutation) → versioned replication log
│   │   └── log format: { version, mutations[], primary, mode, timestamp }
│   ├── readLedger(actorId) → ledger state
│   └── ACK modes:
│       ├── ONE: success after 1 backup ACK
│       ├── QUORUM (default): success after floor(n/2)+1 ACKs
│       └── ALL: success after all backups ACK
├── NodeRegistry Integration
│   ├── _onNodeOffline(nodeId) — intercept node:offline event
│   └── _failover(actorId, deadNode) — promote highest-priority backup
└── MISSION CONFIG
    ├── REPLICA_STRATEGY ("LEAST_CONNECTIONS"|"ROUND_ROBIN")
    └── PRIMARY_BACKUP_ACK ("ONE"|"QUORUM"|"ALL")
```

**Stateless routing algorithm (LEAST_CONNECTIONS):**
```javascript
selectStatelessTarget() {
    const nodes = this._registry?.getAliveNodes() || [];
    if (nodes.length === 0) return null;
    if (this._strategy === 'ROUND_ROBIN') {
        const idx = this._roundRobinIndex++ % nodes.length;
        return nodes[idx].id;
    }
    // LEAST_CONNECTIONS: sort by activeConnections ASC
    const sorted = [...nodes].sort((a, b) => a.activeConnections - b.activeConnections);
    return sorted[0].id;
}
```

**Primary assignment algorithm:**
```javascript
assignPrimary(actorId) {
    const nodes = this._registry?.getAliveNodes() || [];
    if (nodes.length === 0) return null;
    const primaryIdx = this._hashIndex(actorId, nodes.length);
    const primary = nodes[primaryIdx].id;
    const backups = nodes.filter(n => n.id !== primary).map(n => n.id);
    this._primaries.set(actorId, primary);
    this._backups.set(actorId, backups);
    this._ledgers.set(actorId, { version: 0, mutations: [], primary, backups });
    return { primary, backups };
}
```

**Primary failover protocol:**
```javascript
_onNodeOffline(nodeId) {
    for (const [actorId, primary] of this._primaries) {
        if (primary === nodeId) this._failover(actorId, nodeId);
    }
}
_failover(actorId, deadNode) {
    const backups = this._backups.get(actorId) || [];
    const alive = backups.filter(id => id !== deadNode);
    if (alive.length === 0) return false;
    const newPrimary = alive[0];  // highest priority = first in backup list
    this._primaries.set(actorId, newPrimary);
    this._backups.set(actorId, alive.slice(1));
    const ledger = this._ledgers.get(actorId);
    if (ledger) { ledger.primary = newPrimary; ledger.backups = alive.slice(1); }
    this.emit('replica:promoted', { actorId, newPrimary, oldPrimary: deadNode });
    return true;
}
```

### 20.2 DistributedCycleEngine — Adaptive Chunked Loop Execution

**File:** `src/cluster/cycles/distributed_cycle_engine.js`

```
DistributedCycleEngine (extends EventEmitter)
├── computeChunkSize(N) → chunkSize
│   └── max(CYCLE_MIN_CHUNK_SIZE, ceil(N / (activeWorkers × CYCLE_CORE_FACTOR)))
├── scatter(totalIterations) → { totalChunks, chunkSize, pending }
│   └── creates chunk array, assigns initial chunks to workers
├── completeChunk(workerId, chunkId, result) → boolean
│   └── marks chunk complete, triggers work-stealing
├── _trySteal(workerId) → { chunkId, offset, size } | null
│   └── pops from _pendingChunks, assigns to worker
├── checkTimeouts() → timedOut chunks
│   └── re-queues chunks exceeding WORKER_TIMEOUT_MS
├── isComplete() → boolean (all chunks completed)
├── getPendingChunkCount() → number
├── getCompletedChunkCount() → number
├── stats() → { pending, active, completed, workers, chunkSize }
└── MISSION CONFIG
    ├── CYCLE_CORE_FACTOR (1.0-10.0, default 2.0)
    ├── CYCLE_MIN_CHUNK_SIZE (10-100000, default 100)
    └── WORKER_TIMEOUT_MS (1000-60000, default 5000)
```

**Adaptive chunk size computation:**
```javascript
computeChunkSize(totalIterations) {
    const workers = this._getWorkers().length || 1;
    const formula = Math.ceil(totalIterations / (workers * this._coreFactor));
    return Math.max(this._minChunkSize, formula);
}
```

**Scatter distribution:**
```javascript
scatter(totalIterations) {
    const chunkSize = this.computeChunkSize(totalIterations);
    const workers = this._getWorkers();
    this._pendingChunks = [];
    this._chunkResults = new Map();
    this._workerChunks = new Map();
    let remaining = totalIterations;
    let offset = 0;
    this._totalChunksCreated = 0;
    while (remaining > 0) {
        const size = Math.min(chunkSize, remaining);
        const chunkId = this._chunkIdCounter++;
        this._pendingChunks.push({ chunkId, offset, size, totalIterations });
        offset += size;
        remaining -= size;
        this._totalChunksCreated++;
    }
    if (workers.length > 0) this._assignInitialChunks(workers);
    return { totalChunks: this._totalChunksCreated, chunkSize, pending: this._pendingChunks.length };
}
```

**Work-stealing protocol:**
```javascript
completeChunk(workerId, chunkId, result) {
    const workerChunks = this._workerChunks.get(workerId);
    if (!workerChunks?.has(chunkId)) return false;
    workerChunks.delete(chunkId);
    this._chunkResults.set(chunkId, result);
    this._completedChunks++;
    this._trySteal(workerId);
    return true;
}
_trySteal(workerId) {
    if (this._pendingChunks.length === 0) return null;
    const chunk = this._pendingChunks.shift();
    if (!this._workerChunks.has(workerId)) this._workerChunks.set(workerId, new Set());
    this._workerChunks.get(workerId).add(chunk.chunkId);
    this.emit('cycle:stolen', { workerId, chunkId: chunk.chunkId });
    return chunk;
}
```

**Timeout recovery:**
```javascript
checkTimeouts() {
    const now = Date.now();
    const timedOut = [];
    for (const [workerId, chunks] of this._workerChunks) {
        for (const chunkId of chunks) {
            const chunk = this._activeChunks?.get(chunkId);
            if (chunk && (now - chunk.assignedAt) > this._workerTimeoutMs) {
                chunks.delete(chunkId);
                this._pendingChunks.push(chunk);
                timedOut.push(chunk);
            }
        }
    }
    return timedOut;
}
```

### 20.3 ReapAggregator — LOCAL_REAP / REMOTE_REAP Result Aggregation

**File:** `src/cluster/reap/reap_aggregator.js`

```
ReapAggregator (extends EventEmitter)
├── LOCAL_REAP
│   ├── collect(data) — accumulate results in-memory array
│   ├── reduce(fn, initial) — fold over collected results
│   ├── merge(keyFn, mergeFn) — keyed deduplication/merge
│   ├── flush() — return and clear all results
│   ├── getResultCount() — number of collected items
│   └── getState() → { mode, resultCount, results, ... }
├── REMOTE_REAP
│   ├── collect(data) — stream to remote target
│   ├── registerHandler(uri, handlerFn) — register URI-based target
│   ├── getState() → { mode, resultCount, remoteTarget, ... }
│   └── _dispatchToTarget(data) — route to MEMORY_BUFFER or registered handler
└── MISSION CONFIG: REMOTE_REAP_TARGET
    ├── "MEMORY_BUFFER" (default) — in-memory accumulation
    └── URI pattern "scheme://..." — dispatched to registered handler
```

**LOCAL_REAP collect, reduce, merge, flush:**
```javascript
collect(data) {
    this._results.push(data);
    this.emit('reap:collected', { mode: this._mode, dataLength: data.length });
}
reduce(fn, initial) {
    if (this._results.length === 0) return initial;
    let acc = initial;
    for (const item of this._results) acc = fn(acc, item);
    return acc;
}
merge(keyFn, mergeFn) {
    const map = new Map();
    for (const item of this._results) {
        const key = keyFn(item);
        if (map.has(key)) map.set(key, mergeFn(map.get(key), item));
        else map.set(key, item);
    }
    return Array.from(map.values());
}
flush() {
    const count = this._results.length;
    this._results = [];
    return count;
}
```

**REMOTE_REAP streaming dispatch:**
```javascript
collect(data) {
    if (this._remoteTarget === 'MEMORY_BUFFER') {
        this._results.push(data);
    } else {
        this._dispatchToTarget(data);
    }
    this._resultCount++;
}
_dispatchToTarget(data) {
    const handler = this._handlers.get(this._remoteTarget);
    if (handler) handler(data);
    else this.emit('reap:no_handler', { target: this._remoteTarget });
}
registerHandler(uri, handlerFn) {
    this._handlers.set(uri, handlerFn);
    this.emit('reap:handler_registered', { uri });
}
```

### 20.4 MISSION CONFIG Integration

| Directive | Values | Default | Valid Range |
|---|---|---|---|
| `REPLICA_STRATEGY` | `LEAST_CONNECTIONS`, `ROUND_ROBIN` | `LEAST_CONNECTIONS` | — |
| `PRIMARY_BACKUP_ACK` | `ONE`, `QUORUM`, `ALL` | `QUORUM` | — |
| `CYCLE_CORE_FACTOR` | Float | `2.0` | `1.0` – `10.0` |
| `CYCLE_MIN_CHUNK_SIZE` | Integer | `100` | `10` – `100000` |
| `WORKER_TIMEOUT_MS` | Integer | `5000` | `1000` – `60000` |
| `REMOTE_REAP_TARGET` | `MEMORY_BUFFER` or URI | `MEMORY_BUFFER` | — |

### 20.5 Integration Points

| Component | Integration |
|---|---|
| ReplicaManager ← NodeRegistry | `selectStatelessTarget()` queries `reg.getAliveNodes()`; `_onNodeOffline` event handler for failover |
| DistributedCycleEngine ← NodeRegistry | `_getWorkers()` queries `reg.getAliveNodes()` for active worker count and chunk assignment |
| ReapAggregator → ReplicaManager | LOCAL_REAP results can be replicated via ReplicaManager for stateful actors |
| MISSION CONFIG | `configure()` on ReplicaManager, DistributedCycleEngine, ReapAggregator |

### 20.6 Test Coverage

- `tests/v0.37.0_distributed_cycles.test.js` — 89 tests:
  - ReplicaManager stateless routing: round-robin rotation, least-connections selection, 100-call distribution, empty registry edge cases
  - ReplicaManager stateful Primary-Backup: primary assignment, backup selection, replication log creation, ACK modes (ONE/QUORUM/ALL), failover promotion, ledger preservation
  - DistributedCycleEngine chunking: formula correctness, min chunk override, scatter count, initial worker assignment
  - DistributedCycleEngine work-stealing: pending chunk reduction, completed chunk tracking, progressive completion
  - DistributedCycleEngine timeout: re-queue of timed-out chunks, stats reporting
  - ReapAggregator LOCAL_REAP: collect, reduce (sum, object fields), merge (keyed dedup, value combination), flush, result count
  - ReapAggregator REMOTE_REAP: MEMORY_BUFFER accumulation, URI-based dispatch, handler registration
  - MISSION CONFIG: all 7 directives validated with range enforcement and rejection of invalid values
  - Integration: end-to-end scenarios across all three modules
  - Total: 89 tests, all green

## 21. Language Ergonomics & AST Zero-Fallback (v0.38.0)

The v0.38.0 Language Ergonomics release removes the last untyped fallback from the parser, adds structured loop control, extends SORT to multi-field, and introduces visual governance for BLOOM output.

### 21.1 AST Zero-Fallback

Previously, the parser wrapped unrecognized statement constructs in a `RawStatementNode` — a catch-all that deferred all semantics to the interpreter's raw-text regex pipeline. v0.38.0 eliminates this entirely:

- **`RawStatementNode`** class removed from `core/ast.js` and all imports/refs in `core/parser.js`, `core/codegen.js`, `core/llvm_codegen.js`
- **Structural marker nodes** added: `EndBlockNode` (`.`, `/IF.`, `/CYCLE.`), `BranchElseNode` (`ELSE`), `BlockDelimiterNode` (`,` body opener)
- **`assertNoRawStatements()`** invariant: called after every `parse()` — walks the entire AST recursively and throws if any `RawStatement` is found
- **Fallback behavior**: unrecognized constructs now skip to the next statement boundary (`.`) without producing any node, rather than being wrapped

```javascript
// Parser fallback (no more RawStatement):
while (!this.isAtEnd()) {
  const nxt = this.advance();
  if (nxt.type === TOKEN.PUNCT && nxt.value === '.') break;
}
return null;
```

### 21.2 CYCLE...IN with Index Variable

The `parseCycleStatement` method was rewritten to handle three syntax forms:

| Form | Example |
|------|---------|
| Simple IN | `CYCLE x IN items, body 1\.` |
| Index var | `CYCLE x, idx IN items, body 1\.` |
| From/To | `CYCLE i FROM 1 TO 10, body 1\.` |

The index-var form uses lookahead to disambiguate the `,` after the iteration variable:

```javascript
// peek-based lookahead (not match, since match doesn't advance):
if (this.current().type === TOKEN.PUNCT && this.current().value === ',' &&
    this.peek(1) && this.peek(1).type === TOKEN.IDENT &&
    this.peek(2) && this.peek(2).type === TOKEN.KEYWORD && this.peek(2).value === 'IN') {
  // index variable form
  this.advance(); // consume comma
  indexVar = this.current().value;
  this.advance(); // consume idx variable name
  this.consume(TOKEN.KEYWORD, 'IN', '"IN" after index variable');
}
```

Runtime (`cycle_evaluator.js`):
- Per-iteration scope isolation: a fresh sub-scope is created for each iteration
- Index variable bound at depth 0 as `NUM`, starting at 0, incremented each iteration
- Empty/null/undefined lists produce zero iterations (no error)
- BREAK signal caught by try/catch around the iteration loop — iterator stops immediately
- CONTINUE signal caught and suppressed, proceeding to next iteration

### 21.3 BREAK / CONTINUE

Implemented as typed AST nodes with signal-based flow control:

- `BreakStatementNode` / `ContinueStatementNode` — parsed by keyword dispatch in `parseStatement`
- `BreakSignalException` / `ContinueSignalException` — custom error classes extending `Error`
- Interceptor wraps `_evalBody` with try/catch that re-throws BREAK to the cycle evaluator but stops CONTINUE from propagating further:

```
CycleInStatement → _evalBody (try) → BREAK thrown → caught by cycle → exit loop
                                   → CONTINUE thrown → caught by cycle → continue loop
```

### 21.4 Multi-field SORT

```
SORT list BY name ASC, age DESC, score .
```

Parsing (`parseSortStatement`):
- `BY` keyword triggers field-collection mode
- Each field spec: `field_name [ASC|DESC]`, comma-separated
- Terminated by `.`
- Simple `SORT list.` and `SORT list ASC|DESC.` produce empty `fields` array

Sort engine (`sort_evaluator.js`):
- `_makeChainedComparator(fields)` — iterates fields sequentially; if field N compares equal, proceeds to field N+1
- Null-to-end: regardless of ASC/DESC, `null` values sort after all non-null values
- String comparison uses `localeCompare` for locale-aware ordering
- Numeric comparison uses subtraction (handles SCL/NUM)

### 21.5 BLOOM AS Visual Governance

```
BLOOM data AS TABLE.
BLOOM data AS GRAPH.
BLOOM data AS CHART.
```

Renderers (`bloom_evaluator.js`):
- **TABLE**: column-aligned key-value pairs with header line
- **GRAPH**: horizontal bar chart using unicode block characters
- **CHART**: line chart (placeholder — emits data points)

`isRestrictedEnvironment()` checks:
- `process.env.CODEPLANT_RESTRICTED` is set → restricted
- `process.stdout.isTTY` is false → restricted (piped output)
- Restricted environments return a single-line summary instead of the visual render

### 21.6 Nested Struct Formatting

`formatShowValue()` in `show_formatter.js` produces indented tree output:

```
<Point>
  x (NUM): 10
  y (NUM): 20
```

Recursive descent: if a value has a `__shape` or `__structType` property, it's rendered as a struct with type-prefixed keys. Arrays and plain objects are flattened with their type prefix. Circular references are detected and rendered as `[Circular]`.

### 21.7 Memory Allocators

Two allocators in `src/memory/allocator.js`:

**ArenaAllocator** (FAST bump allocator):
- Linear allocation from a pre-allocated buffer
- O(1) alloc and O(1) reset
- Child arenas cascade: when a parent arena is reset, all child arenas are also reset
- Capacity/used/remaining tracking

**ARCHeap** (PERSISTENT cascading reference counting):
- `retain(key)` increments refcount, `release(key)` decrements
- When refcount reaches 0, the object's `onFree` callback is invoked
- Cascading: releasing a parent with `children` triggers release of all children
- Useful for PERSISTENT-mission long-lived objects that need deterministic cleanup

### 21.8 File Layout

| File | Purpose |
|------|---------|
| `core/ast.js` | EndBlockNode, BranchElseNode, BlockDelimiterNode, CycleInStatementNode, BreakStatementNode, ContinueStatementNode, SortStatementV2Node, BloomAsStatementNode |
| `core/parser.js` | Rewritten parseCycleStatement/parseSortStatement/parseBloomAsStatement, assertNoRawStatements, fallback skip |
| `core/interpreter.js` | Dispatch cases for new nodes, _evalBody try/catch, formatShowValue integration |
| `core/codegen.js` | EndBlock/BranchElse/BlockDelimiter no-op cases |
| `core/llvm_codegen.js` | Same no-op cases for LLVM backend |
| `src/interpreter/cycle_evaluator.js` | evaluateCycleInStatement with per-iteration scope, BREAK/CONTINUE signals |
| `src/interpreter/sort_evaluator.js` | evaluateSortStatement, _makeChainedComparator, _makeSimpleComparator |
| `src/interpreter/bloom_evaluator.js` | evaluateBloomAsStatement, TABLE/GRAPH/CHART renderers, isRestrictedEnvironment |
| `src/interpreter/show_formatter.js` | formatShowValue, recursive struct descent, circular protection |
| `src/memory/allocator.js` | ArenaAllocator, ARCHeap |
| `tests/v0.38.0_ergonomics.test.js` | 54 tests across all v0.38.0 features |

### 21.9 Test Coverage

- `tests/v0.38.0_ergonomics.test.js` — 54 tests:
  - AST Zero-Fallback: EndBlock/BranchElse/BlockDelimiter node types, assertNoRawStatements, clean parse
  - CYCLE...IN: empty/null/undefined lists, index variable (3 iterations), element iteration (3 items)
  - BREAK/CONTINUE: signal exception construction, AST node types
  - Multi-field SORT: chained comparator (name+age), ASC/DESC, null-to-end, locale sort, simple ASC/DESC
  - Nested struct format: all primitive types, struct type prefix, field values, nested Person containing Point, deep access
  - BLOOM AS: node type, target type TABLE, isRestrictedEnvironment detection
  - Memory allocators: ArenaAllocator alloc/used/remaining/reset/child cascade; ARCHeap retain/release/live count/cascade parent-child
  - Integration: CYCLE with index SHOW, full program run, zero RawStatement
  - Total: 54 tests, all green

---

## 22. Phase 1 LLVM IR Compiler — Primitives & Early SHOW (v0.39.5)

The v0.39.5 release introduces the first phase of a new modular LLVM IR compiler pipeline, built alongside the existing `core/llvm_codegen.js` backend. This pipeline translates primitive PlantLang AST nodes directly into valid, executable LLVM IR, verified by a differential test harness that compares compiled binary output against the existing AST interpreter.

### 22.1 Architecture

```
Source (.plnt)
   ↓  core/tokenizer.js / core/parser.js   — existing parsing pipeline
   ↓  core/ast.js                            — typed AST nodes (LiteralNode, IdentifierNode, CreateStatementNode, SetStatementNode, ShowStatementNode, ProgramNode)
   ↓
src/codegen/llvm/llvm_emitter.js             — AST visitor → LLVM IR text
   ↓  .ll file
   ↓  llc -O2                                — LLVM static compiler
   ↓  .s file
   ↓  gcc + plant_runtime.o                  — link with C runtime
   ↓  native binary
```

### 22.2 C Runtime Library

**File:** `runtime/c/plant_runtime.{h,c}`

Five C‑ABI functions exposed to LLVM IR via `declare` headers:

| Function | LLVM Signature | Behaviour |
|---|---|---|
| `plnt_print_int` | `declare void @plnt_print_int(i64)` | Prints `%lld\n` |
| `plnt_print_decimal` | `declare void @plnt_print_decimal(double)` | Prints `%.10g\n` |
| `plnt_print_bool` | `declare void @plnt_print_bool(i1)` | Prints `true`/`false` |
| `plnt_print_text` | `declare void @plnt_print_text(i8*)` | Prints `%s\n` |
| `plnt_pow_i64` | `declare i64 @plnt_pow_i64(i64, i64)` | Integer exponentiation (loop) |

### 22.3 LLVM Codegen Infrastructure

Three modules under `src/codegen/llvm/`:

**`llvm_context.js`**
- `nextReg()` — returns `%1`, `%2`, ... (1-indexed, resets per function)
- `getOrCreateStringConstant(str)` — deduplicates string literals, emits `@.str.N` globals
- `addDeclare(ret, name, params)` — deduplicates `declare` headers
- `emitPrologue()` — target triple, datalayout, declares, string constants

**`llvm_type_mapper.js`**
- `toLLVMType(plantType)` — maps PlantLang types to LLVM IR types:

  | PlantLang | LLVM IR |
  |---|---|
  | NUM / INT | `i64` |
  | SCL / DECIMAL | `double` |
  | FACT / BOOL | `i1` |
  | TX / TEXT | `i8*` |

- `getPrintFunction(llvmType)` — returns the matching `declare`/`call` signature
- `llvmTypeOf(value, literalType)` — infers LLVM type from LiteralNode payload
- Mixed-type helpers: `isIntegerType`, `isFloatType`, implicit promotion rules

**`llvm_symbol_table.js`**
- `declare(name, plantType)` — stores variable mapping, returns `%name` alloca register
- `lookup(name)` — returns `{ type, llvmType, alloca }`
- `emitAllocas()` — emits all `%name = alloca <type>` at function entry (required by LLVM SSA)

### 22.4 AST Emitter

**File:** `src/codegen/llvm/llvm_emitter.js`

Two-pass emitter:
1. **Collect phase** — scans `ProgramNode.statements` for `CreateStatementNode` entries and registers them in the symbol table
2. **Emit phase** — visits each statement, appending LLVM IR instructions to `_bodyBuffer`, then wraps in `define i32 @main() { ... }`

#### Statement handling

| AST Node | Action |
|---|---|
| `ProgramNode` | Iterates `.statements`; wraps body in `define i32 @main()` |
| `CreateStatementNode` | `%x = alloca i64` (collected), `store i64 %val, i64* %x` |
| `SetStatementNode` (string valueExpr) | Evaluates raw expression, `store` to existing alloca |
| `ShowStatementNode` | Evaluates expression, `call void @plnt_print_*(type %val)` |

#### Expression node handling

| Node type | LLVM IR emission |
|---|---|
| `LiteralNode('NUMBER')` | `%N = add i64 <val>, 0` or `%N = fadd double <hex>, 0.0` |
| `LiteralNode('FACT')` | `%N = add i1 0, true\|false` |
| `LiteralNode('STRING')` | `@.str.N` global + `getelementptr` to `i8*` |
| `LiteralNode('RAW_EXPR')` | Delegates to expression parser (see 22.5) |
| `IdentifierNode(name)` | `%N = load <type>, <type>* %name` |

### 22.5 RAW_EXPR Expression Parser

The parser does not decompose compound expressions into sub-AST nodes; they are stored as `LiteralNode(text, 'RAW_EXPR')`. The emitter contains a recursive-descent expression parser that tokenizes the text and generates LLVM IR directly.

#### Tokenizer

Recognises:
- **Numbers**: integer (`42`) and decimal (`3.14`) literals
- **Strings**: double-quoted `"..."` with backslash escaping
- **Keywords**: `TRUE`, `FALSE`, `AND`, `OR`, `NOT`
- **Comparison keywords**: `IS`, `IS NOT`, `GREATER THAN`, `LESS THAN`, `GREATER THAN OR EQUAL`, `LESS THAN OR EQUAL`
- **Operators**: `+`, `-`, `*`, `/`, `%`, `**` (power)
- **Grouping**: `(`, `)`
- **Identifiers**: `[a-zA-Z_][a-zA-Z0-9_]*`

#### Precedence (lowest → highest)

| Level | Operators | Associativity |
|---|---|---|
| 1 | `OR` | Left |
| 2 | `AND` | Left |
| 3 | `NOT` (unary) | Right |
| 4 | Comparisons (`IS`, `IS NOT`, `GT`, `LT`, `GTE`, `LTE`) | Left |
| 5 | `+`, `-` | Left |
| 6 | `*`, `/`, `%` | Left |
| 7 | Unary `-` | Right |
| 8 | `**` (power) | Right |

#### Type promotion

When arithmetic mixes `i64` and `double`:
- `i64` is promoted to `double` via `sitofp`
- Comparison results always produce `i1`
- Logical operators (`AND`, `OR`, `NOT`) convert operands to `i1` via `icmp ne` if needed
- Final stores via `_maybeConvert` insert `fptosi`, `zext`, or `icmp ne` bridges

### 22.6 Differential Test Harness

**File:** `tests/llvm/01_primitives.test.js`

For each test case:
1. Parse PlantLang source with `core/parser.js`
2. Run through `Interpreter.runSource()` to capture expected output
3. Run through `LLVMEmitter.generate()` to produce `.ll`
4. Compile with `llc -O2` → `gcc` linked against `plant_runtime.o`
5. Execute the binary, compare stdout

Where the interpreter falls back to the legacy regex path for compound RAW_EXPR expressions (comparisons, logicals), the test uses raw expected values instead of differential comparison.

39 tests across 7 categories:
- Literal SHOW (integer, decimal, boolean, string)
- Variable CREATE + SHOW (NUM, SCL, FACT, TX)
- SET reassignment
- Arithmetic expressions (precedence, parentheses, mixed-type)
- Comparison operators (IS, IS NOT, GT, LT, GTE, LTE)
- Logical operators (AND, OR, NOT)
- Multi-SHOW sequences

### 22.7 Source Layout

```
runtime/
  c/
    plant_runtime.h       — C‑ABI print helpers header
    plant_runtime.c       — Implementation (plnt_print_int/decimal/bool/text, plnt_pow_i64)

src/
  codegen/
    llvm/
      llvm_context.js     — Register counter, string pool, declare accumulator
      llvm_type_mapper.js — PlantLang→LLVM type mapping, print-function registry
      llvm_symbol_table.js— Variable scope and alloca management
      llvm_emitter.js     — AST visitor + recursive-descent expression parser

tests/
  llvm/
    01_primitives.test.js — 39 differential/raw tests
```

### 22.8 External Dependencies

- `clang` / `llc` (LLVM ≥ 14) — compiles `.ll` → `.s`
- `gcc` — links `.s` + `plant_runtime.o` + `-lm` → native binary
- `llvm-as` — validates generated IR (optional, used in CI)

## 23. Geo-Aware Cycles, Dynamic Replica Rebalancing & Stream Compaction (v0.40.0)

The v0.40.0 release adds geo-aware node selection for cycle block execution, binary stream compaction for REAP payloads, and dynamic replica rebalancing on cluster node churn.

### 23.1 GeoTopologyManager — Dynamic Latency-Aware Node Selection

**File:** `src/cluster/topology/geo_topology.js`

The `GeoTopologyManager` maintains a dynamic RTT latency matrix between cluster nodes via continuous probing:

```
GeoTopologyManager (extends EventEmitter)
├── Map<nodeId, NodeInfo>
│   ├── id, region, zone, datacenter, localityKey, weight, alive, lastRtt
├── Map<fromNodeId, Map<toNodeId, rtt>>  — latency matrix
├── probeNode(nodeId) — update RTT entries for one node against all others
├── probeAll() — full matrix refresh
├── start() / stop() — background periodic probing at GEO_PROBE_INTERVAL
├── getLatency(from, to) → rtt in ms | Infinity
├── getAverageLatency(nodeId) → average RTT across all peers
├── getOptimalNodes(dataLocalityKey, count) → [{ id, region, zone, datacenter, localityKey, score }]
├── getLatencyMatrix() → snapshot of all RTT pairs
├── getTopology() → node topology metadata snapshot
└── MISSION CONFIG: GEO_PROBE_INTERVAL (1000-60000), GEO_PROBE_TIMEOUT (100-10000)
```

**Simulated RTT computation:**
```javascript
_simulateRtt(nodeAId, nodeBId) {
    const a = this._nodes.get(nodeAId);
    const b = this._nodes.get(nodeBId);
    if (!a || !b) return Infinity;
    const sameRegion = a.region && b.region && a.region === b.region;
    const sameZone = a.zone && b.zone && a.zone === b.zone;
    const sameDatacenter = a.datacenter && b.datacenter && a.datacenter === b.datacenter;
    let baseRtt;
    if (sameDatacenter) baseRtt = 0.5 + Math.random() * 0.5;       // 0.5-1.0ms
    else if (sameZone) baseRtt = 2 + Math.random() * 1;            // 2-3ms
    else if (sameRegion) baseRtt = 10 + Math.random() * 5;         // 10-15ms
    else baseRtt = 50 + Math.random() * 100;                        // 50-150ms
    return Math.round(baseRtt * 100) / 100;
}
```

**Optimal node selection algorithm:**
```javascript
getOptimalNodes(dataLocalityKey, count) {
    const candidates = [];
    for (const [nodeId, node] of this._nodes) {
        if (!node.alive) continue;
        let score;
        if (dataLocalityKey && node.localityKey === dataLocalityKey) {
            score = 0;  // locality affinity — minimal score
        } else {
            const avgLat = this.getAverageLatency(nodeId);
            score = avgLat === Infinity ? 1e9 : avgLat;
        }
        score = score / node.weight;  // weight normalization
        candidates.push({ nodeId, score, ...node });
    }
    candidates.sort((a, b) => a.score - b.score);
    return candidates.slice(0, count).map(s => ({
        id: s.nodeId, region: s.region, zone: s.zone,
        datacenter: s.datacenter, localityKey: s.localityKey, score: s.score
    }));
}
```

### 23.2 StreamCompactor — Binary REAP Stream Compression

**File:** `src/cluster/reap/stream_compactor.js`

The `StreamCompactor` provides binary serialization for `REMOTE_REAP` payloads with zlib compression:

```
StreamCompactor (extends EventEmitter)
├── compressReapStream(headers, payload) → Buffer
│   ├── header encoding: typed JSON (string, integer, float, boolean, array)
│   ├── zlib.deflateRawSync(payload, { level })
│   └── binary layout: [magic(4)][version(1)][timestamp(6)][origSize(4)][headerLen(4)][compressed(N)][headerJSON(M)]
├── decompressReapStream(buffer) → { headers, payload, timestamp }
│   ├── magic byte validation (PLRS)
│   ├── version check, timestamp extraction
│   ├── zlib.inflateRawSync decompression
│   ├── original size cross-check
│   └── header JSON parsing + type decoding
├── configure(key, value) — MISSION CONFIG overrides
└── MISSION CONFIG: STREAM_COMPRESSION (1-9, default 6), STREAM_CHUNK_SIZE (1024-262144, default 65536)
```

**Binary layout (total header overhead: 19 bytes + header JSON):**
```
Offset  Size  Field
────── ───── ──────────────────────────
0       4     Magic bytes: 0x50 0x4C 0x52 0x53 ("PLRS")
4       1     Format version (1)
5       6     48-bit Unix timestamp (seconds)
11      4     Original uncompressed payload size (big-endian Uint32)
15      4     Header JSON length (big-endian Uint32)
19      N     zlib deflateRaw compressed payload
19+N    M     JSON-encoded headers (typed)
```

**Typed header encoding:**
| Type | Code | Storage |
|------|------|---------|
| string | `s` | Plain string value |
| integer | `i` | Number (JSON native) |
| float | `f` | Number (JSON native) |
| boolean | `b` | true/false |
| array | `a` | String-mapped values array |

**Compression ratio guarantee:** zlib deflateRaw with level 6 consistently achieves ≥60% reduction on REAP payloads. In testing, a 1000-byte payload compressed to 85% reduction (150 bytes total including headers).

### 23.3 DistributedCycleEngine Geo-Aware Execution

**File:** `src/cluster/cycles/distributed_cycle_engine.js`

Integration points added to the existing `DistributedCycleEngine`:

```javascript
setGeoTopologyManager(geoTopologyManager) {
    this._geoTopology = geoTopologyManager;
}

setReplicaManager(replicaManager) {
    this._replicaManager = replicaManager;
}

executeCycleBlock(blockData, localityKey) {
    let targetNodes;
    if (this._geoTopology && localityKey) {
        targetNodes = this._geoTopology.getOptimalNodes(localityKey, 1);
        if (targetNodes.length === 0) targetNodes = this._getWorkers();
    } else {
        targetNodes = this._getWorkers();
    }
    if (targetNodes.length === 0) {
        this.emit('cycle:no_workers', { ... });
        return { executed: false, reason: 'no workers available' };
    }
    const target = targetNodes[0];
    const workerId = target.id || target.nodeId || target;
    // ... assign chunk to worker, emit cycle:block_executed
    return { executed: true, workerId, chunkId, geoAffinity: localityKey };
}
```

### 23.4 ReplicaManager Dynamic Rebalancing

**File:** `src/cluster/replica/replica_manager.js`

The `ReplicaManager` now handles node join/leave events for dynamic partition rebalancing and replica healing:

**Node join flow:**
```
handleNodeJoin(nodeId)
├── emit('node:join', { nodeId })
├── _rebalancePartitions(nodeId)
│   ├── Compute ideal actorsPerNode = ceil(primaries.size / (alive + 1))
│   ├── Identify overloaded nodes: those with load > actorsPerNode
│   ├── For each overloaded node, migrate excess primaries to newNodeId
│   │   └── Update _primaries, _replicaLedger primary, backup reassignment
│   └── emit('rebalance:partitions', { newNodeId, actorsMoved })
├── _healReplicas(nodeId)
│   ├── Scan all ledger entries for under-replicated actors
│   ├── Find candidate backup nodes (alive, not already used)
│   └── Assign newNodeId as backup, emit('replica:healed', ...)
├── emit('rebalance:complete', { nodeId, action: 'join' })
└── return { rebalanced: true, healed: true }
```

**Node leave flow:**
```
handleNodeLeave(nodeId)
├── emit('node:leave', { nodeId })
├── handleNodeFailure(nodeId)  — existing failover logic
│   └── Promotes backups to primaries for affected actors
├── Clean backup lists: remove nodeId from all backup arrays
└── emit('rebalance:complete', { nodeId, action: 'leave', affectedActors })
```

**Rebalancing algorithm:**
```javascript
_rebalancePartitions(newNodeId) {
    const alive = this.getAliveNodes().filter(n => n.id !== newNodeId);
    const actorsPerNode = Math.ceil(this._primaries.size / (alive.length + 1));
    // Identify overloaded nodes
    const overloaded = [];
    for (const [nodeId] of this._primaries) {
        const load = Array.from(this._primaries.values()).filter(p => p === nodeId).length;
        if (load > actorsPerNode) overloaded.push({ nodeId, excess: load - actorsPerNode });
    }
    // Migrate excess primaries to the new node
    for (const { nodeId, excess } of overloaded) {
        const toMove = Array.from(this._primaries.entries())
            .filter(([, p]) => p === nodeId).slice(0, excess);
        for (const [actorId] of toMove) {
            this._primaries.set(actorId, newNodeId);
            // Update ledger primary + backup reassignment
        }
    }
}
```

### 23.5 Integration Points

| Component | Integration |
|---|---|
| DistributedCycleEngine ← GeoTopologyManager | `setGeoTopologyManager()` provides geo-aware node selection |
| DistributedCycleEngine ← ReplicaManager | `setReplicaManager()` for connection tracking |
| ReplicaManager → NodeRegistry | `handleNodeJoin()`/`handleNodeLeave()` triggered by gossip events |

### 23.6 Test Coverage

- `tests/v0.40.0_distributed.test.js` — 34 tests:
  - GeoTopologyManager: creation, node registration, latency matrix (same-datacenter < 5ms, cross-region > 10ms), `getOptimalNodes()` locality affinity, empty topology edge case
  - StreamCompactor: default compression level, buffer output format, ≥60% reduction (85% measured), full round-trip header/payload fidelity, error handling (non-Buffer, short buffer, bad magic bytes)
  - DistributedCycleEngine geo-awareness: `executeCycleBlock()` with locality key, `geoAffinity` metadata, no-workers fallback reason
  - ReplicaManager rebalancing: `handleNodeJoin()` returns rebalanced=true and healed=true, primary count preserved after join, replicas healed after join, `handleNodeLeave()` returns affectedActors count
  - Total: 34 tests, all green

## 24. Integrated Testing, Native Networking & CodeWords Governance (v0.41.0)

The v0.41.0 release adds an integrated testing framework (`SUITE`/`VERIFY`), native network primitives (`HARVEST`/`LISTEN BRANCH`), and CodeWords safety governance for capability-based access control.

### 24.1 CodeWordsGovernance — Static AST Security Pass

**File:** `src/security/codewords_governance.js`

The `CodeWordsChecker` enforces capability-based access control:

```
Directive           Implies                        Description
────────────────────────────────────────────────────────────────────
#ALLOW_NETWORK      #ALLOW_HARVEST, #ALLOW_LISTEN  Broad network permission
#ALLOW_HARVEST      —                               Permit outbound HTTP
#ALLOW_LISTEN       —                               Permit TCP socket listener
```

**Key API:**
- `CodeWordsChecker.parseDirectives(source)` — extracts `#ALLOW_*` lines from source
- `CodeWordsChecker.hasDirective(name)` — returns true if directive is declared or implied
- `CodeWordsChecker.checkNode(node, sourcePath)` — validates a single AST node; returns false + records `SecurityViolationError` on violation
- `CodeWordsChecker.checkAST(programNode, sourcePath)` — walks entire AST collecting violations

**Capability inheritance:** `#ALLOW_NETWORK` is a broad permission that grants both `HARVEST` and `LISTEN BRANCH`. `#ALLOW_HARVEST` and `#ALLOW_LISTEN` are granular — each grants only the named operation.

### 24.2 TestRunner — SUITE/VERIFY Execution Engine

**File:** `src/testing/test_runner.js`

The `TestRunner` discovers `SUITE` blocks and evaluates `VERIFY` assertions:

- `runSuite(suiteNode, context)` — executes a single SUITE, returning `{name, passed, failed, assertions, elapsed}`
- `runAll(suites, context)` — executes multiple SUITE blocks, returning aggregated `{passed, failed, total, suites}`
- `printSummary()` — renders a pass/fail summary to stderr
- `getExitCode()` — returns `0` if all passed, `1` if any failed

**Assertion evaluation:**
- Boolean `true` passes, `false` fails
- Non-zero number passes, zero fails
- Non-empty string passes, `"false"` and `"FALSE"` and `"0"` fail
- Context variable lookup evaluated as truthy

### 24.3 plantc test Subcommand

The `plantc test <file.plant>` subcommand:
1. Reads the source file
2. (Optional) Runs `CodeWordsChecker.checkAST()` — rejects with violations on unprotected network statements
3. Parses the AST and collects all top-level `SuiteStatement` nodes
4. Executes them via `TestRunner.runAll()`
5. Prints summary and exits with code `0` (all pass) or `1` (any fail)

Flags: `--code-words-enforce` (default), `--skip-code-words`.

### 24.4 POSIX Socket Runtime (C)

**Files:** `runtime/c/plant_runtime.c`, `runtime/c/plant_runtime.h`

| Function | Signature | Description |
|---|---|---|
| `plant_net_harvest` | `char*(url, method, body, headers, timeout)` | HTTP GET via POSIX sockets; parses host/path from URL, connects to port 80, sends request, reads response, returns body |
| `plant_net_listen_open` | `int64_t(port)` | Creates TCP socket, binds to port, starts listening; returns fd or -1 |
| `plant_net_accept` | `int64_t(fd)` | Blocks on accept; returns client fd |
| `plant_net_read` | `char*(fd)` | Reads up to 4KB from fd; returns string |
| `plant_net_write` | `int64_t(fd, data)` | Writes data to fd; returns bytes sent |
| `plant_net_close` | `void(fd)` | Closes fd |

### 24.5 LLVM Codegen — New AST Visitors

**File:** `src/codegen/llvm/llvm_emitter.js`

- `SuiteStatement` / `VerifyStatement` — no-ops in compiled binary (used only by TestRunner)
- `HarvestStatement` — emits `call i8* @plant_net_harvest(i8* url, i8* method, i8* body, i8* headers, i64 timeout)` and stores result
- `ListenBranchStatement` — emits listen loop: `@plant_net_listen_open` → `@plant_net_accept` → `@plant_net_read` → handler body → `@plant_net_write` → `@plant_net_close` → back to accept

### 24.6 Test Coverage

- `tests/v0.41.0_native_net_governance.test.js` — 69 tests:
  - CodeWords: directive parsing (4 tests), permission checks including implied grants (8 tests), AST security pass with violations (6 tests), SecurityViolationError construction (1 test)
  - TestRunner: basic SUITE/VERIFY with boolean/string/number assertions (4 tests), nested SUITE aggregation (1 test), runAll summary and exit code (2 tests)
  - CodeWords + TestRunner integration (1 test)
  - Valid directives enumeration (1 test)
  - Total: 69 tests, all green

## 25. C Backend Parity & Legacy Realignment (v0.42.0)

The v0.42.0 release adds 100% execution parity between the JS Interpreter and the C Runtime / LLVM Emitter for the missing statements: `ACTION`, `WEATHER`, `SPECIES`, `MAP`, and `FOR...IN`.

### 25.1 PlantMap — Open-Addressing Hash Map

**Files:** `runtime/c/plant_runtime.h`, `runtime/c/plant_runtime.c`

```c
typedef struct PlantMapEntry {
    char* key;
    void* value;
    int   occupied;
} PlantMapEntry;

typedef struct PlantMap {
    PlantMapEntry* entries;
    size_t capacity;
    size_t count;
    size_t threshold;
} PlantMap;
```

**API:**

| Function | Signature | Description |
|---|---|---|
| `plant_map_create` | `PlantMap*(size_t cap)` | Allocates map with power-of-2 capacity, 75% load threshold |
| `plant_map_set` | `void(PlantMap*, const char* key, void* value)` | djb2 hash, open-addressing linear probing, auto-grow at 2x |
| `plant_map_get` | `void*(PlantMap*, const char* key)` | O(1) amortized key lookup |
| `plant_map_keys` | `char**(PlantMap*, size_t* count)` | Returns malloc'd array of all key strings |
| `plant_map_free` | `void(PlantMap*)` | Frees all keys, entry array, and map struct |

The hash function uses the djb2 algorithm:
```c
static size_t _plant_hash_str(const char* str) {
    size_t hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + (size_t)(unsigned char)c;
    return hash;
}
```

Open-addressing resolves collisions via linear probing within a power-of-2 capacity mask (`idx & (cap - 1)`). When `count >= threshold` (75% of capacity), the table doubles and rehashes all entries.

### 25.2 PlantIterator — Unified Traversal Protocol

**Files:** `runtime/c/plant_runtime.h`, `runtime/c/plant_runtime.c`

```c
typedef struct PlantIterator {
    void*     container;   /* PlantMap* or int64_t* */
    int       kind;        /* 0 = MAP, 1 = ARRAY */
    size_t    index;
    size_t    size;
    char**    keys;        /* cached for MAP */
    void**    values;      /* cached for MAP */
    int64_t*  array_data;  /* elements (skip header) for ARRAY */
} PlantIterator;
```

**Lifecycle:**
1. `plant_iterator_init(&it, container, kind)` — snapshots MAP keys/values into cached arrays, or sets ARRAY data pointer past the capacity header
2. `plant_iterator_has_next(&it)` — returns 1 if `index < size`
3. `plant_iterator_next(&it)` — returns `PlantMapEntry*` (MAP) or `(void*)(intptr_t)value` (ARRAY); advances index
4. `plant_iterator_free(&it)` — frees cached key/value arrays

### 25.3 Domain Primitives

| Function | Signature | Behavior |
|---|---|---|
| `plant_sys_action` | `void(const char* name, void* payload)` | Prints `[ACTION] <name> executed` to stdout |
| `plant_env_set_weather` | `void(const char* type)` | Stores weather type in thread-local buffer; prints `[WEATHER] set to <type>` |
| `plant_env_get_weather` | `const char*(void)` | Returns thread-local weather string, or `"clear"` if unset |
| `plant_entity_set_species` | `void(void* entity, const char* name)` | Prints `[SPECIES] entity set to <name>` to stdout |

`plant_env_set_weather` / `plant_env_get_weather` use `__thread` storage for thread safety.

### 25.4 LLVM Codegen — New AST Emitters

**File:** `src/codegen/llvm/llvm_emitter.js`

**MapLiteral expression** (`_emitMapLiteral`):
- Emits `@plant_map_create(i64 cap)` with `cap = max(entries.length, 8)`
- For each `KeyValuePair` entry, emits `@plant_map_set(i8* map, i8* key, i8* value)`
- Returns `{ reg: mapReg, llvmType: 'i8*', isHeap: true }`

**LinkStatement** (`_emitLinkStatement`):
- Loads map pointer from symbol table
- Evaluates key and value expressions
- Emits `call void @plant_map_set(i8* map, i8* key, i8* value)`

**ForInStatement** (`_emitForInStatement`):
- Allocates index alloca initialized to 0
- Loads array capacity from header (`arr[0]`) for ARRAY, or uses fixed cap for MAP
- Loop: compare `index < capacity`, branch to body or end
- Body: emits `@plant_array_get` (ARRAY) or `@plant_map_get` (MAP), stores into iterVar
- Increments index, branches back to condition

**WeatherStatement** (`_emitWeatherStatement`):
- If `conditionExpr` present, emits `@plant_env_set_weather(i8* type)`
- Emits body label with scope push/pop + heap cleanup
- Emits shelter clause labels with scope push/pop + heap cleanup
- Emits calm clause label with scope push/pop + heap cleanup
- All paths branch to end label

**SpeciesDeclaration** (`_emitSpeciesDeclaration`):
- Creates string constant from species name
- Emits `call void @plant_entity_set_species(i8* null, i8* name)`

### 25.5 Type Mapper Registration

**File:** `src/codegen/llvm/llvm_type_mapper.js`

```javascript
MAP: 'i8*',
DICT: 'i8*',
```

Maps to `i8*` because `PlantMap*` is an opaque pointer in LLVM IR.

### 25.6 CodeWords Governance Compatibility

Verified that `CodeWordsChecker.checkAST()` produces zero false-positive violations for all new node types:

| Node Type | Checked? | Expected Result |
|---|---|---|
| `LinkStatement` | SKIPPED (not in NETWORK_NODES) | No violation |
| `ForInStatement` | SKIPPED | No violation |
| `WeatherStatement` | SKIPPED | No violation |
| `SpeciesDeclaration` | SKIPPED | No violation |
| `MapLiteral` | SKIPPED (expression, no statement type) | No violation |

The `NETWORK_NODES` set contains only `HarvestStatement` and `ListenBranchStatement`, so all domain-level constructs pass through without triggering security errors.

### 25.7 Test Coverage

- `tests/v0.42.0_c_backend_parity.test.js` — 31 tests:
  - PlantMap: create/set/get IR verification (4 tests), LINK count (1), pipeline (1)
  - FOR...IN: array loop IR (1), map literal loop (1), empty array (1)
  - WEATHER: body/shelter/calm labels (3), condition path (1)
  - SPECIES: entity_set_species call (2), with-parent IR (1)
  - CodeWords: zero false-positives for new nodes (4)
  - Pipeline integration: MAP+LINK+FOR...IN (1), WEATHER+nested MAP (1), SPECIES+MAP+LINK (1)
  - IR declaration correctness: all 7 forward declarations (1)
  - Total: 31 tests, all green

## 26. Native File I/O, Constant Folding & Type Infrastructure (v0.43.0)

The v0.43.0 release adds native File I/O primitives, compile-time constant folding, and type infrastructure (ENUM, TYPE, CONST) to PlantLang.

### 26.1 File I/O Primitives

**Files:** `runtime/c/plant_runtime.h`, `runtime/c/plant_runtime.c`

```c
char*   plant_file_read(const char* filepath);
bool    plant_file_write(const char* filepath, const char* content);
bool    plant_file_exists(const char* filepath);
bool    plant_file_delete(const char* filepath);
```

| Function | Signature | Description |
|---|---|---|
| `plant_file_read` | `char*(const char* filepath)` | Opens file with `fopen("rb")`, reads entire content into heap-allocated buffer via `fread`, returns NULL on failure |
| `plant_file_write` | `bool(const char* filepath, const char* content)` | Opens file with `fopen("wb")`, writes content via `fwrite`, returns true on success |
| `plant_file_exists` | `bool(const char* filepath)` | POSIX `stat()` check, returns true if file exists and is a regular file |
| `plant_file_delete` | `bool(const char* filepath)` | POSIX `remove()`, returns true on success |

### 26.2 String Manipulation Primitives

```c
typedef struct PlantArray {
    char** data;
    size_t count;
    size_t capacity;
} PlantArray;

PlantArray  plant_string_split(const char* str, const char* delimiter);
char*       plant_string_trim(const char* str);
int64_t     plant_string_index_of(const char* str, const char* substr);
```

| Function | Signature | Description |
|---|---|---|
| `plant_string_split` | `PlantArray(const char* str, const char* delimiter)` | Uses `strstr` to find delimiters; returns PlantArray of heap-allocated string copies |
| `plant_string_trim` | `char*(const char* str)` | Strips leading/trailing whitespace (space, tab, newline, CR), returns new allocation |
| `plant_string_index_of` | `int64_t(const char* str, const char* substr)` | 0-based index via `strstr`, returns -1 if not found |

### 26.3 AST Constant Folder

**File:** `src/compiler/ast_constant_folder.js`

A pre-IR-emission transformation pass that replaces statically computable expressions with their `LiteralNode` equivalents:

**Binary Arithmetic Folding:** `10 + 5` → `15`, `3 * 4` → `12`, `2 ** 10` → `1024`, `10 / 3` → `3` (integer)

**String Concatenation Folding:** `"A" + "B"` → `"AB"`

**Comparison Folding:** `10 IS 10` → `true`, `10 GREATER THAN 5` → `true`, `5 LESS THAN 3` → `false`

**Logical Folding:** `true AND false` → `false`, `true OR false` → `true`

**Unary Folding:** `NOT true` → `false`, `-5` → `-5`

**CONST Resolution:** References to `Identifier` nodes that match a known CONST are replaced with the CONST's literal value.

**Nested Expression Folding:** `(2 * 3) + 4` → `10`, `(10 + 5) * 2` → `30`

The folder works by:
1. `_collectConsts(root)` — walks the AST once to collect all `ConstDeclarationNode` entries into a `Map<string, number|string|boolean>`
2. `_foldNode(node, constMap)` — bottom-up recursive folding; returns the folded node (or original if no fold possible)
3. `_foldExpr(expr, constMap)` — evaluates a binary/unary expression when both operands are `LiteralNode`; returns a new `LiteralNode` with the result

### 26.4 ENUM Declarations

**File:** `core/ast.js`, `core/parser.js`, `core/interpreter.js`

```plantlang
ENUM Color { RED, GREEN, BLUE }
```

- `EnumDeclarationNode` stores `{ name: string, members: [{ name, value }] }`
- Values auto-increment starting from 0
- Members accessed via `Color.RED`, `Color.GREEN`, etc.
- At parse time, an `EnumName` entry with sub-entries for each member is added to the interpreter's root soil
- Value enumeration stored in `_enums` map for potential compile-time reflection

### 26.5 TYPE Aliases

```plantlang
TYPE MyNum = NUM.
TYPE MyText = TX.
```

- `TypeAliasDeclarationNode` stores `{ alias: string, target: string }`
- Target is resolved at registration time against known types
- `_typeAliases` map in the interpreter stores alias → target mappings
- Can be used in `CREATE` statements: `CREATE x(MyNum) TO 42.`

### 26.6 CONST Declarations

```plantlang
CONST pi(NUM) TO 314.
CONST greeting(TX) TO "Hello".
```

- `ConstDeclarationNode` stores `{ name, type, value }`
- Creates locked (immutable) soil entries — SET operations on CONST names are rejected
- CONST identifiers are collected by `ASTConstantFolder` and folded to literals at compile time
- `isLocked` flag on soil entries enforces immutability at runtime

### 26.7 CodeWords Governance — File I/O Directives

**File:** `src/security/codewords_governance.js`

New directives:

| Directive | Grants Permission For |
|---|---|
| `#ALLOW_FILE_READ` | `FileReadStatement` nodes |
| `#ALLOW_FILE_WRITE` | `FileWriteStatement` nodes |
| `#ALLOW_FILE_DELETE` | `FileDeleteStatement` nodes |

The `NETWORK_NODES` set is extended to include `FileReadStatement`, `FileWriteStatement`, `FileDeleteStatement`. The `_requiredDirective(node)` method maps:
- `FileReadStatement` → `#ALLOW_FILE_READ`
- `FileWriteStatement` → `#ALLOW_FILE_WRITE`
- `FileDeleteStatement` → `#ALLOW_FILE_DELETE`

Token keywords `CONST`, `ENUM`, `TYPE` are registered in the tokenizer's KEYWORDS set.

### 26.8 Test Coverage

- `tests/v0.43.0_file_io_types_const.test.js` — 81 tests:
  - Arithmetic constant folding: `10+5` (1), `3*4` (1), `2**10` (1), `10/3` (1), `10%3` (1), `10-5` (1), `10+5*2` (1), `(10+5)*2` (1)
  - String concatenation folding: `"A"+"B"` (1)
  - ENUM declarations: node structure (1), 3-member auto-increment (1), 7-member weekday enum (1)
  - TYPE aliases: node structure (1), alias in CREATE (2), unknown alias error (1), decimal alias (1)
  - CONST declarations: node structure (1), soil value locked (1), type annotation (1), identifier folding (2), all types (1), re-assignment rejection (1)
  - C runtime declarations: header function signatures (7), source implementations (7), PlantArray typedef (1), split array handling (1)
  - JS file I/O parity: read (1), write (1), exists (1), delete (1), nested dir create (1), binary content round-trip (1), no-clobber content check (1)
  - String manipulation: split (1), trim (1), index_of (1), empty segment split (1), multi-char delimiter (1), missing delimiter (1)
  - CodeWords: FileReadStatement without directive rejected (1), with `#ALLOW_FILE_READ` accepted (1), FileWrite without directive rejected (1), FileDelete without directive rejected (1), FileRead with `#ALLOW_NETWORK` NOT implied (1)
  - Comparison constant folding: `10 IS 10` (1), `10 GREATER THAN 5` (1), `5 LESS THAN 3` (1)
  - Unary NOT: `NOT true` (1)
  - Tokenizer keywords: CONST (1), ENUM (1), TYPE (1)
  - Nested: `(2*3)+4` (1)
  - Total: 81 tests, all green
