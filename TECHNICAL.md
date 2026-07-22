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

## 13. Five-Mission Execution Architecture (v0.32.0)

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

## 14. Test Suite Architecture

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
- **Total: 22 test files, ~635+ tests, all green**

### Test Methodology
Each test:
1. Parses the source with the real parser (resolving any `IMPORT` statements)
2. For LLVM/C backend tests: generates IR/C code
3. For LLVM tests: compiles to a native binary via `llc -O2` + `gcc -no-pie -lm` (FFI functions resolved via `core/runtime_bridge.c`)
4. Runs the binary and captures stdout
5. Compares against the interpreter's output — must match exactly

Phase 7 and Phase 8 tests use a custom `check(label, condition)` harness that tests specific parser, resolver, and runtime behaviors without requiring LLVM compilation.
