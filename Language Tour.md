# 🌿 PlantLang — Chloroplast v0.27.0

> **A programming language designed to read like natural prose.**
> Write code the way you write a sentence — not the way you debug a cipher.

```
1\ CREATE name(TX)  TO "Haider".
1\ CREATE score(NUM) TO 94.

1\ IF score GREATER THAN OR EQUAL 90,
2\   SHOW "Welcome, " + name + "! Excellent work.".
1\.
```

---

## Installation

```bash
npm install -g plantlang          # global CLI
# or
git clone https://github.com/your/plantlang && cd plantlang
node chloroplast.js run myfile.plnt
```

## Quick Start

```bash
chloroplast run    file.plnt         # run a program
chloroplast verify tests.plnt        # run VERIFY test suite
chloroplast check  file.plnt         # static type check
chloroplast compile file.plnt --run  # compile to native binary and run it
chloroplast repl                     # interactive REPL
```

---

## Language Tour

### Depth prefix: `N\`
Every statement is prefixed with its nesting depth. Depth 1 = top level, depth 2 = inside a block, etc.

```
1\ CREATE x(NUM) TO 10.
1\ IF x GREATER THAN 5,
2\   SHOW "big".
1\.
```

Each depth level owns a **dedicated 64KB arena slab** (`Arena_N`). Variables at depth N are bump-allocated from Arena_N. When execution leaves a scope (depth decreases), the arena is automatically reset — no garbage collector needed:

```
2\   CYCLE i FROM 1 TO 10,      # Arena_2
3\     CREATE tmp(NUM) TO i.    # Arena_3 — reset on each loop tick
2\   SHOW "done".               # Arena_3 reset, Arena_2 preserved
```

**Four automatic cleanup mechanisms:**
| Mechanism | When | What resets |
|---|---|---|
| Natural Exit | Scope depth decreases | Exited arenas |
| Forced Exit | `GIVE` return from `ACTION` | All arenas > depth 0 |
| Iteration Breath | End of each `CYCLE`/`SEASON` tick | Loop-depth arena + deeper |
| Error Unwinding | `WEATHER` throw → `SHELTER` catch | Arenas between error depth and handler |

### Variables & Types

| Type | Keyword | Example |
|------|---------|---------|
| Integer | `NUM` | `CREATE age(NUM) TO 25.` |
| Decimal | `SCL` | `CREATE pi(SCL) TO 3.14.` |
| Text | `TX` | `CREATE name(TX) TO "Haider".` |
| Boolean | `FACT` | `CREATE active(FACT) TO TRUE.` |
| List | `LIST` | `CREATE fruits(LIST) TO apple, banana.` |
| Map | `MAP` | `CREATE m(MAP[NUM,TX]).` — typed hash table |
| Struct | `SHAPE` | `CREATE p(Point) TO Point{ 10, 20 }.` |
| Tagged Union | `CHOICE` | `CREATE opt TO Option.Some(10).` |

```
1\ SET name TO "World".
1\ INCREASE score BY 5.
1\ DECREASE count BY 1.
1\ LOCK pi.          # make immutable
1\ EVAPORATE temp.   # delete variable
```

### Maps (Hash Tables)

Typed key-value storage with native LLVM compilation — `LINK`, `has()`, and `put()` are all fully compiled:

```
1\ CREATE m(MAP[NUM,TX]).
1\ LINK 1 WITH "hello" IN m.
1\ LINK 2 WITH "world" IN m.
1\ SHOW m.has(1).            # → true (compiled natively)
1\ SHOW m.has(99).           # → false
1\ m.put(3, "foo").          # intrinsic method
```

Internally uses open-addressing with linear probing, djb2 hash for TX keys, automatic capacity doubling when load factor exceeds 0.75, and arena-allocated bucket arrays.

### SHAPE / STRUCT (Struct Types)

Two syntaxes for user-defined aggregate types:

**SHAPE** (classic — `field(TYPE)`):
```
1\ SHAPE Point { x(NUM), y(NUM) }.
1\ CREATE p(Point) TO Point{ 10, 20 }.
1\ SHOW p.x.              # → 10
1\ SET p.y TO 99.
1\ SHOW p.y.              # → 99
```

**STRUCT** (alt — `field: TYPE`):
```
1\ STRUCT Person { name: TX, age: NUM }.
1\ CREATE p(Person) TO { name: "Alice", age: 30 }.
1\ SHOW p.name.           # → Alice
1\ INCREASE p.age BY 1.
1\ SHOW p.age.            # → 31
```

Structs can be composed:

```
1\ SHAPE Point { x(NUM), y(NUM) }.
1\ SHAPE Line { start(Point), end(Point) }.
1\ CREATE l(Line) TO Line{ Point{0, 0}, Point{10, 20} }.
1\ SHOW l.start.x.        # → 0
```

STRUCT also supports anonymous struct literals in `CREATE` context and INCREASE/DECREASE on fields.

### Methods on Structs

Actions can receive a typed `SELF` parameter using colon syntax:

```
1\ SHAPE Counter { count(NUM), step(NUM) }.

1\ ACTION (self(Counter)) tick(),
2\   INCREASE SELF:count BY SELF:step.
2\   GIVE SELF:count.
1\ /ACTION.

1\ CREATE c(Counter) TO Counter{ 0, 5 }.
1\ REAP v FROM c:tick.
1\ SHOW v.                # → 5
1\ REAP v FROM c:tick.
1\ SHOW v.                # → 10
```

### CHOICE (Tagged Unions)

```
1\ CHOICE Option { Some(NUM), None }.
1\ CREATE opt TO Option.Some(10).
1\ CREATE empty TO Option.None.
```

Variant names may use keywords like `Num` or `Empty`:

```
1\ CHOICE Value { Num(NUM), Str(TX), Empty }.
1\ CREATE v TO Value.Num(42).
1\ CREATE s TO Value.Str("hello").
1\ CREATE e TO Value.Empty.
```

### MATCH (Pattern Matching)

Exhaustive case analysis on CHOICE values — every variant must have a clause:

```
1\ MATCH opt { Some(v) -> { SHOW v } None -> { SHOW 0 } }.
```

Features:
- **Payload binding**: `Some(v)` binds the inner `NUM` value to `v` inside the clause body
- **Payload-free clause**: `None -> { ... }` — no variable binding
- **Exhaustiveness**: the type checker rejects matches that don't cover all variants of the CHOICE type

### Output

```
1\ SHOW "Hello, " + name + "!".
1\ SHOW score.
1\ SHOW TYPE name.     # → TX
```

### Conditions

```
1\ IF score GREATER THAN OR EQUAL 90,
2\   SHOW "A".
1\ ORIF score GREATER THAN OR EQUAL 80,
2\   SHOW "B".
1\ ELSE,
2\   SHOW "F".
1\.

1\ STOP IF score IS 0, "score cannot be zero".
```

### MATCH (switch)

```
1\ MATCH grade,
2\   IS "A"            YIELD SHOW "Excellent".
2\   IS "B"            YIELD SHOW "Good".
2\   IS BETWEEN 1 5    YIELD SHOW "Number grade".
2\   ELSE              YIELD SHOW "Unknown".
1\ \\.
```

### CYCLE (loops)

```
1\ CYCLE item IN fruits,
2\   SHOW item.
1\.

1\ CYCLE i FROM 1 TO 10,
2\   SHOW i.
1\.

1\ CYCLE i FROM 1 TO 100 STEP 5,
2\   SHOW i.
1\.
```

### FOR...IN (iteration)

Iterate over LIST values, MAP keys, or TX character spans:

```
1\ CREATE items(LIST) TO apple, banana, cherry.
1\ FOR item IN items,
2\   SHOW item.
1\ /FOR.

1\ CREATE m(MAP[NUM,TX]).
1\ LINK 1 WITH "a" IN m.
1\ LINK 2 WITH "b" IN m.
1\ FOR k IN m,
2\   SHOW k.
1\ /FOR.

1\ FOR ch IN "hello",
2\   SHOW ch.
1\ /FOR.
```

Features:
- **STOP IF** works correctly inside `FOR` bodies — `STOP_STORM` is caught and propagated
- **Nested IF** with inline syntax: `IF cond, SHOW x.`
- **DEPTH-aware** — each `FOR` creates its own scope level

### SEASON (while)

```
1\ SEASON count GREATER THAN 0,
2\   DECREASE count BY 1.
1\.
```

### Lists

```
1\ CREATE scores(LIST) TO 85, 92, 78, 96.
1\ PUT 100 INTO scores.
1\ TAKE 85 FROM scores.
1\ SORT scores.
1\ SHAKE scores.             # shuffle
1\ SHOW COUNT scores.
1\ SHOW FIRST scores.
1\ SHOW LAST scores.
1\ SHOW SUM scores.
1\ SHOW MAX scores.
1\ SHOW MIN scores.
```

### Native List Operations (COUNT / FIRST / LAST / SUM)

The recommended syntax for aggregate list operations uses typed arrays `[NUM]`:

```
1\ CREATE nums([NUM]) TO [10, 20, 30, 40].
1\ SHOW COUNT(nums).    # → 4 (O(1) — extractvalue on %fat_ptr length)
1\ SHOW SUM(nums).      # → 100 (O(n) — inline LLVM phi loop)
1\ SHOW FIRST(nums).    # → 10 (O(1) — GEP to index 0)
1\ SHOW LAST(nums).     # → 40 (O(1) — GEP to len-1)
```

All four operations are compiled to native LLVM IR — no external calls. COUNT, FIRST, and LAST are O(1). SUM is O(n) with inline emission. Type checker enforces: SUM requires `[NUM]`; COUNT/FIRST/LAST work on any `[T]` array.

### LINK (MAP insert)

```
1\ CREATE user(MAP[NUM,TX]).
1\ LINK 1 WITH "Haider" IN user.
1\ LINK 2 WITH 94        IN user.
1\ SHOW user.has(2).     # → true (compiled natively)
```

### BRAID (zip two lists)

```
1\ CREATE names(LIST)  TO Alice, Bob, Carol.
1\ CREATE grades(LIST) TO 92, 87, 78.

# Pairs: [[Alice,92], [Bob,87], [Carol,78]]
1\ BRAID names WITH grades AS report.

# Or directly into a MAP: {Alice:92, Bob:87, Carol:78}
1\ BRAID names WITH grades AS score_map MAP.
```

### Actions (functions)

Supported natively in the LLVM backend — compiled to real function calls, not inlined.

```
1\ ACTION add(a(NUM), b(NUM)),
2\   GIVE a + b.
1\ /ACTION.

1\ REAP result FROM add, 10, 25.
1\ SHOW result.          # → 35

# Ignore return value
1\ REAP _ FROM add, 1, 2.
```

Features:
- Multiple typed params (`NUM`, `SCL`, `TX`, `FACT`)
- Recursion (factorial, Fibonacci — verified via `llc` + `gcc`)
- `IF`/`ELSE` bodies with multiple `GIVE` statements
- `SCL` (double) params preserve bits through `i64` return register
- `TX` (string) returns via pointer encoding
- Void actions (no `GIVE`) default to `ret i64 0`

**Memory safety**: Function parameters are arena-allocated at depth 0 and preserved across recursive calls. On `GIVE`, all arenas > depth 0 are automatically reset (Forced Exit cleanup).

### FLOW (pipeline)

```
1\ ACTION trim(s(TX)),
2\   REAP r FROM strings:TRIM, s.
2\   GIVE r.
1\ /ACTION.

1\ ACTION upper(s(TX)),
2\   REAP r FROM strings:UPPER, s.
2\   GIVE r.
1\ /ACTION.

1\ REAP clean FROM "  hello world  ",
     FLOW trim,
     FLOW upper.
1\ SHOW clean.           # → HELLO WORLD
```

### SPECIES / BLOOM (classes & instances)

Two syntax styles — new `{ }` body style and legacy `,`/`/SPECIES.` style:

**New `{ }` body syntax:**
```
1\ SPECIES Counter {
2\   count: NUM
3\   step: NUM
4\   ACTION tick(),
5\     INCREASE SELF:count BY SELF:step.
6\     GIVE SELF:count.
7\   /ACTION.
8\ }
```

**Legacy `,`/`/SPECIES.` syntax:**
```
1\ SPECIES Counter,
2\   VAR count(NUM) TO 0.
2\   VAR step(NUM)  TO 1.
2\   ACTION tick(),
3\     INCREASE SELF:count BY SELF:step.
3\     GIVE SELF:count.
2\   /ACTION.
1\ /SPECIES.
```

**Instantiation & method call:**
```
1\ CREATE c TO BLOOM Counter.
1\ SET c:step TO 5.
1\ REAP v FROM c:tick.
1\ SHOW v.              # → 5
```

### Inheritance (PARENT / FROM)

Legacy `PARENT` syntax:
```
1\ SPECIES Animal,
2\   VAR name(TX) TO "?".
2\   ACTION speak(),
3\     GIVE SELF:name + " speaks.".
2\   /ACTION.
1\ /SPECIES.

1\ SPECIES Dog PARENT Animal,
2\   ACTION fetch(),
3\     GIVE SELF:name + " fetches!".
2\   /ACTION.
1\ /SPECIES.
```

New `FROM` syntax with `{ }` body:
```
1\ SPECIES Animal {
2\   name: TX
3\   ACTION speak(),
4\     GIVE SELF:name + " speaks.".
5\   /ACTION.
6\ }

1\ SPECIES Dog FROM Animal {
2\   ACTION fetch(),
3\     GIVE SELF:name + " fetches!".
4\   /ACTION.
5\ }
```

Usage:
```
1\ BLOOM Dog AS d.
1\ SET d:name TO "Rex".
1\ REAP s FROM d:speak.
1\ SHOW s.              # → Rex speaks.
```

### IMPORT (Module System)

Load external `.plnt` files and merge their statements into the current program:

```
IMPORT "std/io".
IMPORT "std/string".

# Relative paths resolve from the importing file's directory:
IMPORT "lib/helpers.plnt".
IMPORT "../shared/utils".
```

Features:
- Cycle detection — circular imports produce a clear error at parse time
- AST merging — imported statements are spliced directly into the importing program
- Path resolution — relative, absolute, and `std/`-prefixed paths are supported
- Auto-prelude — every program implicitly imports `std/prelude.plnt`

**Cycle detection:**
```
IMPORT "a".
  ↳ IMPORT "b".
      ↳ IMPORT "a".   # ERROR: IMPORT cycle detected
```

### FFI (Foreign Function Interface)

Declare native C functions and call them directly from PlantLang using the `-> external` syntax:

```
ACTION plant_printf(fmt(TX)) -> external.
ACTION plant_puts(s(TX)) -> external.
ACTION plant_len(s(TX)) -> external.

1\ REAP result FROM plant_puts, "Hello from C!".
```

FFI functions:
- Are declared with `ACTION name(params) -> external.` (no body, no `/ACTION.`)
- Must be backed by a matching C function in `core/runtime_bridge.c` (or linked separately)
- Are treated as external `declare` in LLVM IR — linked at compile time
- Are pre-registered in the interpreter with stub implementations

### Standard Library

PlantLang's standard library lives in `std/` and is accessed via `IMPORT "std/..."`:

```
IMPORT "std/io".
IMPORT "std/string".

1\ REAP _ FROM print, "Hello from std/io!".
1\ REAP _ FROM println, "This adds a newline".

1\ REAP upp FROM strings:UPPER, "hello".
1\ SHOW upp.                          # → HELLO

1\ REAP joined FROM strings:CONCAT, "a", "b", "c".
1\ SHOW joined.                       # → abc
```

Available modules:

| Module | Key Functions |
|--------|--------------|
| `std/io` | `print`, `println`, `plant_printf`, `plant_puts` |
| `std/string` | `len`, `upper`, `lower`, `trim`, `contains`, `split`, `replace`, `concat` |
| `std/prelude` | Auto-injected: TRUE, FALSE, _BOOT |

### WEATHER / SHELTER / CALM (try / catch / finally)

Supported natively in the LLVM backend — division-by-zero detection branches directly to the matching handler.

```
1\ WEATHER,
2\   REAP val FROM risky_action, 0.
1\ SHELTER ZERO_STORM AS err,
2\   SHOW "Caught: " + err.
1\ SHELTER ANY_STORM,
2\   SHOW "Unknown error".
1\ CALM.

# Division by zero caught at compile-time-checked runtime:
1\ WEATHER,
2\   SHOW 10 / 0.
1\ SHELTER ZERO_STORM AS err,
2\   SHOW "zero! " + err.
1\ CALM.
```

**How it works**: Before every division, the LLVM backend emits a `fcmp oeq %divisor, 0.0` check. If zero, error globals (`@_weather_msg`, `@_weather_type`, `@_weather_flag`) are set and control transfers to the matching SHELTER handler. All arenas between the error source depth and the handler depth are automatically reset (Error Unwinding).

### VERIFY (built-in testing)

```
SUITE "Math",
  1\ ACTION add(a(NUM), b(NUM)),
  2\   GIVE a + b.
  1\ /ACTION.

  VERIFY "addition works",       FROM add, 2, 3 GIVES 5.
  VERIFY "result type",          TYPE result IS NUM.
  VERIFY "count check",          COUNT fruits IS 4.
  VERIFY "storm fires",          STORMS ZERO_STORM FROM 1 / 0.
  VERIFY "value in range",       score BETWEEN (0, 100).
SUITE/.
```

```bash
chloroplast verify tests/suite.plnt    # exits 1 if any test fails
```

### HARVEST (HTTP client)

```
1\ HARVEST "https://api.example.com/users" AS resp.
1\ SHOW resp:"status".                   # → 200
1\ SHOW resp:"body":"name".

1\ CREATE headers(MAP).
1\ LINK "Authorization" WITH "Bearer token123" IN headers.

1\ HARVEST "https://api.example.com/create",
2\   METHOD: POST,
2\   BODY:    payload,
2\   HEADERS: headers,
2\   TIMEOUT: 10,
2\   AS result.
```

Response MAP: `{ ok:FACT, status:NUM, body:MAP|LIST|TX, headers:MAP }`

### LISTEN BRANCH (HTTP server)

```
1\ CREATE cfg(MAP).
1\ LINK "timeout" WITH 30 IN cfg.

1\ LISTEN BRANCH ON 3000 WITH cfg AS req MAP,
2\   CREATE path(TX) TO req:"path".
2\   IF path IS "/",
3\     CREATE body(MAP).
3\     LINK "status" WITH "ok" IN body.
3\     GIVE body AS RESPONSE.
2\   ELSE,
3\     GIVE "Not Found" AS RESPONSE.
1\ LISTEN/.
```

Request MAP: `{ method:TX, path:TX, query:MAP, headers:MAP, body:MAP|TX }`

### ANALYZE / TYPEOF / NOW / WAIT

```
1\ ANALYZE scores.          # detailed inspection: sum, avg, min, max, median
1\ REAP t  FROM TYPEOF scores.   # → LIST
1\ REAP ts FROM NOW FORMAT:STAMP.  # Unix timestamp (NUM)
1\ REAP dt FROM NOW FORMAT:DATE.   # locale date (TX)
1\ WAIT 1.5.                       # sleep 1.5 seconds (max 10)
```

### ROOT / ROOT_SCOPE (constants)

```
ROOT VERSION TO "1.0.0".
ROOT MAX_RETRIES TO 3.

ROOT_SCOPE CONFIG,
  LINK "host"  WITH "localhost" IN CONFIG.
  LINK "port"  WITH 8080        IN CONFIG.
  LINK "debug" WITH FALSE       IN CONFIG.
ROOT_SCOPE/.

1\ SHOW CONFIG:"host".   # → localhost
# Attempting SET CONFIG:"host" throws LOCK_STORM
```

### PULSE / WHENEVER (reactive variables)

```
1\ CREATE temp(NUM) PULSE TO 20.
1\ WHENEVER temp CHANGES,
2\   SHOW "Temperature changed to " + temp.
1\.

1\ SET temp TO 35.   # → "Temperature changed to 35"
```

### Libraries (PLANT)

```
PLANT math.
PLANT strings.
PLANT lists.

1\ REAP r  FROM math:SQRT, 144.        # → 12
1\ REAP up FROM strings:UPPER, "hi".  # → HI
1\ REAP av FROM lists:AVERAGE, scores. # → mean
```

**Available libraries:**

| Library | Key functions |
|---------|---------------|
| `math`    | SQRT ABS ROUND FLOOR CEIL POW LOG |
| `strings` | TRIM UPPER LOWER LENGTH SPLIT REPLACE INCLUDES CONCAT |
| `lists`   | AVERAGE MEDIAN UNIQUE FLATTEN CHUNK RANGE FILTER_GT FILTER_LT |

---

## Storm Types (Errors)

| Storm | Cause |
|-------|-------|
| `ZERO_STORM` | Division by zero |
| `TYPE_STORM` | Wrong operation for type |
| `MISSING_STORM` | Undefined variable or action |
| `SEED_STORM` | Unknown statement syntax |
| `LOST_STORM` | Index / key not found |
| `BOUND_STORM` | List index out of range |
| `LOCK_STORM` | Attempt to modify ROOT or locked variable |
| `STOP_STORM` | Raised by STOP IF |
| `PERM_STORM` | Permission error |
| `NETWORK_STORM` | HARVEST / LISTEN network failure |
| `SYNTAX_STORM` | Malformed LISTEN BRANCH grammar |
| `ANY_STORM` | Catch-all in SHELTER |

---

## MISSION Modes

```
MISSION: SAFE.    # default — full type checking, immutability enforcement
MISSION: FAST.    # performance target (Rust-style)
MISSION: SMART.   # AI/ML target (Python-style)
MISSION: BALANCED. # general purpose
```

---

## CLI Reference

```bash
chloroplast run    <file.plnt> [--mission SAFE|FAST|SMART] [--verbose]
chloroplast verify <file.plnt> [--mission SAFE|FAST|SMART]
chloroplast repl   [--mission SAFE|FAST|SMART]
chloroplast check  <file.plnt>
chloroplast compile <file.plnt> [--output <path>] [--run] [--keep-c]
```

## Compiling to Native Code

`chloroplast compile` translates a supported subset of PlantLang into a real native binary — no interpreter needed at runtime.

**Two backends**, auto-selected (LLVM preferred, C as fallback):

```bash
chloroplast compile app.plnt --run                # compile and execute immediately
chloroplast compile app.plnt --output myapp        # custom binary name
chloroplast compile app.plnt --keep-c              # keep the generated .ll/.opt.ll/.s (or .c) files
chloroplast compile app.plnt --backend llvm         # force LLVM (fails if no llc found)
chloroplast compile app.plnt --backend c            # force the direct-to-C backend
```

### LLVM backend (default when `llc` is available)

`core/llvm_codegen.js` emits LLVM IR text (SSA form) with arena-based deterministic memory management, which is optimized with `opt -O2` (mem2reg, GVN, loop optimizations, inlining — LLVM's real optimization pipeline) and lowered to native object code with `llc -O2`, then linked with `gcc`. This is the same pipeline architecture used by **Rust, Swift, Julia, and Zig** — PlantLang gets decades of LLVM optimization work for free.

**Key technology: Rooted Depth System**
Rather than using LLVM's `alloca` (which couples variable lifetime to the LLVM stack frame), PlantLang allocates all variables from per-depth arena slabs. This enables:
- Deterministic bulk deallocation without GC
- Automatic cleanup on scope exit, function return, loop iteration, and exception unwind
- Zero fragmentation — simple bump-pointer allocation

Requires LLVM's `llc` (and ideally `opt`) on `PATH` — e.g. `apt install llvm` on Debian/Ubuntu. Auto-detects `llc`/`llc-14` through `llc-18`.

### C backend (fallback, no LLVM required)

`core/codegen.js` emits plain C99, compiled with `gcc -O2`. Used automatically when no LLVM toolchain is found, or explicitly via `--backend c`. Supports `CREATE`/`SET`/`SHOW`/`IF`/`CYCLE`/`SEASON` for NUM/SCL/TX/FACT (no ACTION, no WEATHER).

### Supported subset (LLVM backend)

| Feature | Support |
|---------|---------|
| `CREATE` / `SET` / `INCREASE` / `DECREASE` | ✅ `NUM`, `SCL`, `TX`, `FACT` |
| `SHOW` | ✅ literals, identifiers, `+` concatenation, method calls |
| `IF` / `ORIF` / `ELSE` | ✅ |
| `CYCLE var FROM lo TO hi [STEP k]` | ✅ numeric ranges |
| `SEASON` | ✅ while loop |
| Arithmetic & comparisons | ✅ `+ - * / % **`, `IS`, `GREATER THAN`, `BETWEEN`, `AND`/`OR`/`NOT` |
| `ACTION` / `REAP` / `GIVE` | ✅ recursion, SCL params, TX returns, void actions |
| `WEATHER` / `SHELTER` / `CALM` | ✅ ZERO_STORM detection, errVar binding, nested shelters |
| `MAP` (hash table) | ✅ `LINK`, `has()`, `put()` — open-addressing, linear probing, djb2 hash, auto-growth |
| `FOR...IN` | ✅ iterate over LIST, MAP, TX |
| `LIST` (dynamic array) | ❌ use `chloroplast run` |
| `COUNT(xs)` / `FIRST(xs)` / `LAST(xs)` | ✅ O(1) via GEP/extractvalue — compiled natively |
| `SUM(xs)` | ✅ O(n) inline phi loop — compiled natively |
| `SPECIES` / `BLOOM` | ✅ interpreter: `{ }` body syntax, FROM inheritance, BLOOM instantiation, method dispatch, SELF:field access |
| `CHOICE` / `MATCH` | ❌ not yet (interpreter-only) |
| `HARVEST` / `LISTEN BRANCH` | ❌ not yet |
| `VERIFY` / `SUITE` | ❌ not yet |

Unsupported constructs produce a clear compile-time error naming the exact line and feature — programs are never silently miscompiled.

### Performance

For compute-heavy code, compiled binaries measure **thousands of times faster** than the interpreter — a 50-million-iteration accumulation loop runs in ~6ms compiled vs ~89s interpreted (~14,700× speedup). See `examples/10_performance.plnt` for a runnable comparison.

## CodeWords Compiler Service

A standalone HTTP API for running PlantLang remotely — powers the Web REPL and lets any client (curl, CI, other tools) execute, type-check, verify, or compile PlantLang without installing anything locally.

```bash
node service/codewords-server.js --port 8420
```

```bash
curl -X POST http://localhost:8420/run \
  -H "Content-Type: application/json" \
  -d '{"source": "MISSION: SAFE.\n1\\ SHOW \"Hello!\"."}'
# → {"ok":true,"output":"Hello!","elapsedMs":42}
```

Every request runs in a disposable forked process with a 5s timeout, output/body size caps, and network access (`HARVEST`/`LISTEN BRANCH`) disabled — full details in [`service/README.md`](service/README.md).

## Web REPL

A single-page browser UI for writing and running PlantLang — no build step, no dependencies.

```bash
node service/codewords-server.js --port 8420   # in one terminal
cd webrepl && python3 -m http.server 8850       # in another
# open http://localhost:8850
```

Four modes (Run / Check / Verify / Compile), a live connection indicator, curated example programs, and a collapsible generated-C-source view for Compile mode. Details in [`webrepl/README.md`](webrepl/README.md).

---

## Architecture

Chloroplast v0.28.0 uses a dual-engine architecture — an AST interpreter for development and an LLVM compiler for production:

```
Source (.plnt)
   ↓  core/tokenizer.js      — depth-aware lexer (handles \N prefix)
   ↓  core/parser.js         — recursive-descent, 40+ typed node types, IMPORT resolution
   ↓  core/ast.js            — typed AST node classes (all carry depth)
   ↓
    ├── core/interpretm.js   — evaluateNode() dispatcher (dev/`chloroplast run`)
    │   └── core/evaluator.js — expression evaluator
    │   └── core/runtime.js   — Soil scope chain, PlantStorm
    │   └── core/innate.js    — built-in libs (math/strings/lists)
    │   └── FFI stubs         — pre-registered runtime_bridge wrappers
    │
    ├── std/                   — Standard Library (.plnt modules)
    │   ├── io.plnt           — print, println (FFI-bridged)
    │   ├── string.plnt       — len, upper, lower, trim, contains, split, replace, concat
    │   └── prelude.plnt      — auto-injected core definitions
    │
    ├── core/llvm_codegen.js  — LLVM IR generator (production/`chloroplast compile`)
    │   └── Rooted Depth System: arena-based deterministic memory
    │       ├── Arena allocation (bump-alloc from per-depth 64KB slabs)
    │       ├── Depth tracking with automatic arena reset
    │       ├── Contract Law validation (CREATE destination ≤ current depth)
    │       ├── Unwinding Protocol (Natural/Forced Exit, Loop Reset, Error Unwind)
    │       ├── MAP hash tables: open-addressing, linear probing, djb2 hash, auto-growth
    │       ├── SPECIES struct types: %species.Name with parent-field prefixing
    │       ├── SPECIES method dispatch: static call with bitcast for inheritance
    │       └── FFI declare IR — external function declarations for runtime_bridge
```

### Memory Architecture

```
Arena_0  [████████████████░░░░░░░░░░░░░░░░░░]  ← function params
Arena_1  [████████████████████░░░░░░░░░░░░░░]  ← root scope variables
Arena_2  [████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░]  ← loop scope (reset each iteration)
Arena_3  [░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░]  ← inner scope
...up to Arena_63
```

Each arena is 64KB (65536 bytes). The bump pointer (`@arena_offsets[N]`) tracks the next free byte. When a scope exits, the pointer is reset to zero — no free-list, no GC, no fragmentation.

96% of all statements are parsed to typed AST nodes. The remaining 4% (empty block closers) use a safe RawStatement fallback.

---

## File Extensions

| Extension | Description |
|-----------|-------------|
| `.plnt` | PlantLang source file |

---

## License

MIT — © 2025 PlantLang Project
