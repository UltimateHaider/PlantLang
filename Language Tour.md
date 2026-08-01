# 🌿 PlantLang — Chloroplast v0.47.2

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

**Chloroplast** is a pure native compiler (v0.46.4+) — no Node.js required:

```bash
git clone https://github.com/your/plantlang && cd plantlang
make all        # builds bin/Chloroplast (self-hosted)
./bin/Chloroplast myfile.plant myfile.c
gcc -w -O0 -I runtime/c myfile.c runtime/c/plant_runtime.c -o myfile
./myfile
```

## Quick Start

```bash
./bin/Chloroplast file.plant out.c     # compile PlantLang to C
./bin/Chloroplast --help               # usage + options
./bin/Chloroplast --version            # Chloroplast 0.47.2 (pure native)
make test                              # native integration suite
```

---

## Standard Library (v0.47.x) — Practical Examples

The core standard library ships natively in the runtime (`plant_runtime.c` /
`plant_compat.h`) — no interpreter, no imports required. Calls go through
`REAP x FROM fn, args.` or as plain expressions.

### Data Structures: Set, Queue, Stack (v0.47.2)

**Set** — unique unordered collection (identity-based uniqueness; value 0/NULL
is reserved as nil):

```plantlang
REAP s FROM set_create.
REAP r FROM set_add, s, 10.          # "1" added
REAP r FROM set_add, s, 10.          # "0" duplicate
REAP r FROM set_has, s, 10.          # "1" present
REAP r FROM set_remove, s, 10.       # "1" removed
CREATE n(NUM) TO set_size(s).        # unique element count
REAP lst FROM set_to_list, s.        # → LIST for iteration/export
```

**Queue** — FIFO ring buffer:

```plantlang
REAP q FROM queue_create.
REAP _ FROM queue_push, q, "first".
REAP _ FROM queue_push, q, "second".
REAP v FROM queue_pop, q.            # → "first"
REAP v FROM queue_peek, q.           # → "second" (front, kept)
CREATE n(NUM) TO queue_size(q).      # item count
```

**Stack** — LIFO dynamic array:

```plantlang
REAP st FROM stack_create.
REAP _ FROM stack_push, st, "bottom".
REAP _ FROM stack_push, st, "top".
REAP v FROM stack_peek, st.          # → "top"
REAP v FROM stack_pop, st.           # → "top"
REAP v FROM stack_pop, st.           # → "bottom"
```

Empty `pop`/`peek` on a queue or stack return the empty string — never a crash.
Stress workloads (thousands of inserts/lookups/deletes) are covered by the
`std_set` / `std_queue` / `std_stack` native test suites.

### std/json (v0.47.1)

```plantlang
REAP j FROM json_parse, "{\"name\": \"Alice\", \"age\": 30}".
IF j IS NULL,                        # invalid JSON → safe nil, no crash
  SHOW "bad json".
/IF.
REAP nm FROM json_get, j, "name".
SHOW json_val(nm).                   # → Alice
REAP out FROM json_stringify, j.
SHOW out.                            # → {"name":"Alice","age":30}
```

### std/string, std/fs, std/math, std/time (v0.47.1)

```plantlang
REAP s FROM string_repeat, "ab", 3.          # → "ababab"
REAP s FROM string_reverse, "abc".           # → "cba"
REAP s FROM string_pad, "x", 5, ".".         # → "x...."

REAP r FROM file_copy, "a.txt", "b.txt".     # "1" ok
REAP m FROM file_stat, "b.txt".              # MAP: size/mtime/mode
REAP sz FROM _map_get, m, "size".

REAP s FROM math_sqrt, "16".                 # → "4"
REAP s FROM math_pow, "2", "10".             # → "1024"

REAP t FROM time_now.                        # epoch seconds
REAP d FROM time_format, t, "%Y-%m-%d".      # → "2026-08-01"
REAP ok FROM time_sleep, "0.05".             # fractional seconds
```

> Note: the self-hosted compiler subset currently uses `SEASON` (while) for
> iteration; the `CYCLE` statement is tokenized and code-generated for, but not
> yet parsed.

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

### Block-Depth Contract Law

PlantLang enforces a **Block-Depth Contract** at both parse time and type-check time to keep code well-structured:

| Statement | Allowed Depth | Rule |
|---|---|---|
| `ACTION` / `SPECIES` | **Depth 0 only** | Functions and classes must be declared at the top level |
| `REAP` | **Depth ≥ 1** | Function calls must happen inside an ACTION body or CYCLE block |
| `GIVE` | **Depth ≥ 1** | Returns must happen inside an ACTION body |
| `CYCLE` | **Depth ≥ 1** | Loops must happen inside an ACTION body |

**Valid:**
```
1\ ACTION greet(name(TX)),              # ACTION at depth 0 ✓
2\   REAP msg FROM format, name.        # REAP at depth 1 ✓
2\   GIVE msg.                          # GIVE at depth 1 ✓
1\ /ACTION.
```

**Invalid (rejected at parse time):**
```
1\ REAP x FROM f, 5.                    # ✗ REAP at depth 0 — expected depth ≥ 1
1\ CYCLE i FROM 1 TO 10,               # ✗ CYCLE at depth 0 — expected depth ≥ 1
2\   SHOW i.
1\.
```

Violations produce a clear `[DepthContractError]` with the expected depth range and a caret pointing to the offending statement:

```
═══ ⚔ SYNTAX_STORM ═══
  [DepthContractError] REAP is not allowed at depth 0.
  Expected depth 1.
    at line 1, column 3
    |
  1 | REAP x FROM f, 5.
    | ^^^
```

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

**CYCLE...IN with Index Variable (v0.38.0):**

```
1\ CREATE items(LIST) TO 10, 20, 30.
1\ CYCLE val, idx IN items,
2\   SHOW idx.
1\.
# → 0
# → 1
# → 2
```

The index variable (`idx` above) is auto-bound as `NUM` at depth 0, starting at 0 each iteration. It is reset per iteration.

**BREAK and CONTINUE (v0.38.0):**

```
1\ CYCLE x IN items,
2\   IF x = 5,
3\     BREAK.
2\   .
2\   SHOW x.
1\.
# Items before 5 are shown, loop exits at 5.

1\ CYCLE x IN items,
2\   IF x = 5,
3\     CONTINUE.
2\   .
2\   SHOW x.
1\.
# Items before and after 5 are shown, 5 is skipped.
```

`BREAK.` exits the innermost CYCLE immediately. `CONTINUE.` skips to the next iteration. Both produce a `SYNTAX_STORM` if used outside a CYCLE body.

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

### Native String Split/Join via REAP Expressions

`SPLIT(str, delim)` splits a TX string by a delimiter and returns a `[TX]` array. `JOIN(arr, delim)` joins a `[TX]` array with a delimiter and returns a TX string. Both work as REAP expression sources — no ACTION declaration needed:

```
1\ REAP parts FROM SPLIT("apple,banana,cherry", ",").
1\ SHOW COUNT(parts).        # → 3
1\ REAP first FROM parts[0].
1\ SHOW first.               # → apple

1\ REAP joined FROM JOIN(parts, ":").
1\ SHOW joined.              # → apple:banana:cherry
```

Both are compiled natively via LLVM — `SPLIT` calls `plnt_str_split` (two-pass, sret convention), `JOIN` calls `plnt_str_join`. Roundtrips through split-then-join produce identical output. Large strings (>64KB) are supported via `malloc`-backed part arrays.

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

### Multi-field SORT (v0.38.0)

Sort a list of structs by multiple fields with per-field direction:

```
1\ CREATE users(LIST) TO { "name": "Alice", "age": 30 }, { "name": "Bob", "age": 25 }, { "name": "Alice", "age": 20 }.
1\ SORT users BY name ASC, age DESC.
# → Alice,30 → Alice,20 → Bob,25
```

- Fields are compared sequentially — if field N compares equal, field N+1 breaks the tie
- `ASC` (default) or `DESC` per field
- Nulls sort to end regardless of direction
- Simple `SORT list.` and `SORT list ASC|DESC.` syntax continues to work

### BLOOM AS Visual Governance (v0.38.0)

Render data in visual formats:

```
1\ BLOOM users AS TABLE.
1\ BLOOM stats AS GRAPH.
1\ BLOOM series AS CHART.
```

- **TABLE**: column-aligned key-value pairs
- **GRAPH**: horizontal unicode bar chart
- **CHART**: line chart (data points)

Rendering is automatically blocked in restricted environments (piped output, `CODEPLANT_RESTRICTED` env var).

### Nested Struct SHOW (v0.38.0)

`SHOW` on deeply nested struct instances renders an indented tree view:

```
1\ CREATE inner(Inner) TO Inner{ 99, "secret" }.
1\ CREATE outer(Outer) TO Outer{ "top", inner }.
1\ SHOW outer.
# → <Outer>
# →   label (TX): top
# →   child (Inner): <Inner>
# →     id (NUM): 99
# →     role (TX): secret
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
| `SPLIT(str, delim)` / `JOIN(arr, delim)` | ✅ via REAP expression sources — calls `plnt_str_split` / `plnt_str_join` with sret |
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

Chloroplast v0.46.4 is a **pure native, self-hosted compiler** — no JavaScript
engine. Source is compiled to C by the self-hosted compiler
(`src/plantc/*.plant`) and linked against the native C runtime
(`runtime/c/plant_runtime.c`):

```
Source (.plant)
   ↓  src/plantc/lexer.plant        — depth-aware lexer (handles \N prefix)
   ↓  src/plantc/parser.plant       — recursive-descent parser → AST (LISTS)
   ↓  src/plantc/codegen_c.plant    — C code generator
   ↓  bin/Chloroplast               — CLI driver (--help / --version / compile)
   ↓  runtime/c/plant_runtime.c     — native runtime (arenas, LIST/MAP, PLANT modules)
   ↓  gcc
Native executable
```

The compiler is bootstrapped `dist/Chloroplast (v1) → v2 → v3 → v4 → v5` and
`make self` verifies the generations are byte-identical (fixed point).

> The remainder of this section documents the legacy JavaScript-era
> architecture (v0.44.0 and earlier, including `core/*.js` interpreter
> modules, distributed cluster layers, and CodeWords services). It is kept as
> a historical record — none of it ships in the v0.46.4 Pure Native release.

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
    │
    ├── compiler/parallel/parallel_codegen.js  — ParallelCodegenEngine (v0.33.0)
    │   ├── DAG builder: dependence graph for statement-level parallelism
    │   ├── Cycle detection: Tarjan's algorithm rejects cyclic graphs
    │   ├── Weighted load balancer: round-robin bucket assignment per action weight
    │   └── Worker thread pool: `worker_threads` pool, lock-free bitcode assembly
    │
    ├── compiler/distributed/remote_compiler.js  — RemoteCompilerNode (v0.33.0)
    │   ├── zlib (deflate) compression: ≥60% payload reduction
    │   ├── TCP transport: net.Socket connection to remote compiler nodes
    │   ├── 100ms timeout: auto-fallback to local engine on timeout/refusal
    │   └── Transparent failover: caller receives result from local engine
    │
    ├── telemetry/metrics_collector.js  — NonBlockingTelemetry (v0.33.0)
    │   ├── SharedArrayBuffer ring buffer: 128-entry × 64B, lock-free atomic writes
    │   ├── `snapshot()`: zero-allocation structured copy, overflow tracking
    │   ├── `record(name, value)`: O(1) atomic write with overflow detection
    │   └── Background exporter: periodic stream export for external sinks
    │
    ├── runtime/dispatcher.js  — RuntimeDispatcher (v0.33.0)
    │   ├── `enableParallelCodegen()`: enable DAG-split parallel codegen
    │   ├── `disableParallelCodegen()`: revert to sequential compilation
    │   ├── Auto-disable: single-core CPUs skip parallel mode at creation
    │   └── Telemetry bindings: hooks into NonBlockingTelemetry metrics
    │
    ├── security/audit/audit_logger.js  — NonBlockingAuditLogger (v0.34.0)
    │   ├── SharedArrayBuffer ring buffer: 10K entries default, configurable
    │   ├── SHA256 tamper-evident hash chain per entry
    │   ├── `verifyIntegrity()`: detects memory tampering
    │   └── Async Worker flush: background disk/SIEM writer
    │
    ├── security/network/mtls_jwt_guard.js  — mTLSJwtGuard (v0.34.0)
    │   ├── TLS 1.3 mTLS certificate loading from env or file paths
    │   ├── RS256 / Ed25519 JWT verification with key caching
    │   ├── Anti-replay protection via jti tracking table
    │   └── Certificate expiry auto-detection and renewal hooks
    │
    ├── security/sandbox/capability_guard.js  — CapabilityGuard (v0.34.0)
    │
    ├── cluster/discovery/node_registry.js  — NodeRegistry (v0.35.0)
    │   ├── Heartbeat monitor: HEALTHY/DEGRADED/OFFLINE lifecycle
    │   ├── Telemetry: cpuUtil, heapUsage, activeWorkers
    │   └── MISSION CONFIG: HEARTBEAT_INTERVAL, HEARTBEAT_THRESHOLD
    │
    ├── cluster/router/cluster_router.js  — ClusterRouter & CircuitBreaker (v0.35.0)
    │   ├── Weighted least-connections node selection
    │   ├── Per-node circuit breaker (CLOSED/OPEN/HALF-OPEN)
    │   ├── Transparent backup failover on circuit open
    │   └── mTLSJwtGuard integration for dispatch auth
    │
    ├── cluster/memory/distributed_heap.js  — DistributedHeap & ConsistentHashRing (v0.35.0)
    │   ├── SHA-256 → BigInt consistent hash space
    │   ├── Virtual nodes (128 default) for balanced distribution
    │   ├── PERSISTENT store with lease-based expiry
    │   └── Stateful actor ownership with proxy detection
    │
    ├── cluster/config/share_governance.js  — ShareGovernance (v0.36.0)
    │   ├── SHARED_READ: O(1) versioned snapshots, TCP Gossip invalidation
    │   ├── SHARED_WRITE RAFT: single-leader linearizable consensus
    │   ├── SHARED_WRITE CRDT: state-based LWW register convergence
    │   └── MISSION CONFIG: GOSSIP_PROPAGATION_MS, CONSENSUS_ENGINE
    │
    ├── cluster/affinity/call_graph_analyzer.js  — CallGraphAnalyzer (v0.36.0)
    │   ├── Bounded depth CALL_GRAPH_MAX_DEPTH (default 3)
    │   ├── Louvain-inspired community detection for affinity groups
    │   └── Static placement: co-located functions → same cluster node
    │
    ├── cluster/router/smart_execution_router.js  — SmartExecutionRouter (v0.36.0)
    │   ├── LOCAL_CPU / REMOTE_NODE / GPU_ACCELERATED triage
    │   ├── < 0.05ms routing overhead per invocation
    │   └── MISSION CONFIG: SMART_ROUTE_GPU_MIN_BYTES, SMART_ROUTE_MAX_LATENCY_MS
    │
    ├── cluster/replica/replica_manager.js  — ReplicaManager (v0.37.0)
    │   ├── Stateless routing: LEAST_CONNECTIONS, ROUND_ROBIN
    │   ├── Stateful Primary-Backup: assignment, delta replication log
    │   ├── ACK modes: ONE, QUORUM, ALL
    │   └── Failover: NodeRegistry event intercept, backup promotion
    │
    ├── cluster/cycles/distributed_cycle_engine.js  — DistributedCycleEngine (v0.37.0)
    │   ├── Adaptive chunking: max(minChunkSize, ceil(N/(workers×coreFactor)))
    │   ├── scatter/completeChunk/work-stealing (_trySteal)
    │   ├── Straggler detection: checkTimeouts, WORKER_TIMEOUT_MS
    │   └── MISSION CONFIG: CYCLE_CORE_FACTOR, CYCLE_MIN_CHUNK_SIZE, WORKER_TIMEOUT_MS
    │
    ├── cluster/reap/reap_aggregator.js  — ReapAggregator (v0.37.0)
    │   ├── LOCAL_REAP: in-memory collect, reduce, merge, flush
    │   ├── REMOTE_REAP: stream to MEMORY_BUFFER or URI target
    │   └── MISSION CONFIG: REMOTE_REAP_TARGET
    │
    ├── codegen/llvm/llvm_context.js      — Reg counter, string pool, declare headers (v0.39.5)
    ├── codegen/llvm/llvm_type_mapper.js  — NUM→i64, SCL→double, FACT→i1, TX→i8* (v0.39.5)
    ├── codegen/llvm/llvm_symbol_table.js — Variable scope → alloca emission (v0.39.5)
    ├── codegen/llvm/llvm_emitter.js      — AST→LLVM IR visitor + expression parser (v0.39.5)
    │
    ├── interpreter/cycle_evaluator.js  — CycleInStatement (v0.38.0)
    │   ├── CYCLE item [, idx] IN list: per-iteration scope isolation
    │   ├── Index variable auto-binding as NUM at depth 0
    │   ├── BREAK signal: caught by try/catch, exits loop
    │   └── CONTINUE signal: caught and suppressed, next iteration
    │
    ├── interpreter/sort_evaluator.js  — Multi-field SORT (v0.38.0)
    │   ├── _makeChainedComparator: sequential field comparison
    │   ├── Null-to-end semantics regardless of ASC/DESC
    │   └── localeCompare for string fields
    │
    ├── interpreter/bloom_evaluator.js  — BLOOM AS (v0.38.0)
    │   ├── TABLE, GRAPH, CHART target-specific renderers
    │   └── isRestrictedEnvironment: CODEPLANT_RESTRICTED, non-TTY
    │
    ├── interpreter/show_formatter.js  — Nested Struct SHOW (v0.38.0)
    │   ├── formatShowValue: indented JSON-like tree for nested structs
    │   └── Circular reference detection via visited Set
    │
    ├── memory/allocator.js  — ArenaAllocator & ARCHeap (v0.38.0)
    │   ├── ArenaAllocator (FAST): bump allocator, child arena cascading reset
    │   └── ARCHeap (PERSISTENT): cascading reference counting
    │   ├── Zero-trust default: SAFE mode has zero permissions
    │   ├── Granular capability matrix per mission mode
    │   ├── Syscall filtering: blocks execve/ptrace/fork/clone/kill in SAFE
    │   └── Violation enforcement: SIGSYS termination + CRITICAL audit log
    │
    ├── security/codewords_governance.js   — CodeWordsChecker (v0.44.0)
    │   ├── #ALLOW_NETWORK / #ALLOW_HARVEST / #ALLOW_LISTEN / #ALLOW_FILE_READ / #ALLOW_FILE_WRITE / #ALLOW_FILE_DELETE directives
    │   ├── Static AST security pass rejecting HARVEST/LISTEN BRANCH / File I/O without permission
    │   └── #ALLOW_NETWORK implies both HARVEST and LISTEN
    │
    ├── testing/test_runner.js              — TestRunner (v0.44.0)
    │   ├── SUITE/VERIFY block discovery and execution
    │   ├── Truthy/falsy assertion evaluation
    │   ├── Nested suite support with aggregated counts
    │   └── plantc test subcommand integration
    │
    ├── cluster/topology/geo_topology.js  — GeoTopologyManager (v0.40.0)
    │   ├── Dynamic RTT latency matrix with continuous probing
    │   ├── getOptimalNodes(dataLocalityKey, count): lowest-latency node selection
    │   └── MISSION CONFIG: GEO_PROBE_INTERVAL, GEO_PROBE_TIMEOUT
    │
    ├── cluster/reap/stream_compactor.js  — StreamCompactor (v0.40.0)
    │   ├── Binary REAP stream format: magic bytes, version, 48-bit timestamp
    │   ├── zlib deflateRaw compression (60-85% reduction)
    │   ├── Typed header encoding (string, integer, float, boolean, array)
    │   └── MISSION CONFIG: STREAM_COMPRESSION, STREAM_CHUNK_SIZE
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

## Local Runtime & Isolation Layer (v0.32.0)

Starting with v0.32.0, PlantLang ships with a dedicated **Local Runtime & Isolation Layer** — a suite of five runtime modules that implement mission-specific memory management, process isolation, adaptive IPC, and telemetry.

### 1. `BumpAllocator` — FAST Mission Arena Allocator

The `BumpAllocator` provides O(1) linear bump allocation for `FAST` mission mode. It enforces **strict 8-byte alignment** and resets in O(1) time at scope exit — no `free()` or `compact()` needed.

**Default capacity:** 8 MB (configurable, hard cap at 64 MB).

**Automatic escalation:** When the arena runs out of space, execution escalates to `BALANCED` mode transparently:

```
MISSION: FAST.
MISSION CONFIG FAST_HEAP_SIZE = 16MB.

1\ ACTION fast_kernel(data([NUM])) WITH MISSION FAST,
2\   CREATE buf(NUM) TO 0.
2\   CYCLE i FROM 0 TO COUNT(data) - 1,
3\     INCREASE buf BY data[i].
2\   .
2\   GIVE buf.
1\ /ACTION.
```

If `fast_kernel` exhausts the 16 MB FAST heap during execution, the runtime emits:

```
WARN: Fast heap capacity exceeded. Escalated to BALANCED.
```

The program continues correctly — no crash, no data loss.

### 2. `GlobalARCHeap` — PERSISTENT Reference-Counted Heap

The `GlobalARCHeap` provides atomic reference counting for long-lived objects in `PERSISTENT` mission mode. It includes automatic **cycle detection** every 1000 allocations with ≈0.1ms overhead.

**Idle frame utilization with `GC.cycle()`:**

```
1\ ACTION game_loop() WITH MISSION PERSISTENT,
2\   CREATE cache(MAP[TX,SHAPE]).
2\   SEASON TRUE,
3\     # ... game logic ...
3\     REAP _ FROM GC.cycle.     # Manual GC during idle frame time
3\   .
1\ /ACTION.
```

`GC.cycle()` triggers a cycle detection pass that cleans up circular references:

```
INFO: Scheduled GC cycle executed. Circular references cleared.
```

**Finalization callbacks** allow cleanup when an object's reference count drops to zero:

```
1\ ACTION create_resource() WITH MISSION PERSISTENT,
2\   CREATE handle(TX) TO "db-connection".
2\   REAP id FROM arc:register, handle.
2\   GIVE id.
1\ /ACTION.

# When handle is released, its onFinalize callback closes the DB connection.
```

### 3. `WarmProcessPool` — SAFE Process Isolation

The `WarmProcessPool` maintains a pool of **pre-warmed, physically isolated child processes** for `SAFE` mission mode. Each worker is a separate OS process with its own memory space.

**Default pool size:** 4 workers (configurable, ceiling of `min(OS.cpus() × 2, 16)`).

**Heartbeat & zombie recovery:** The pool sends Ping/Pong heartbeats every 5000ms. If a worker fails to respond within 10ms, the runtime:

1. Kills the zombie PID
2. Logs the error
3. Spawns a replacement

```
[SYS-POOL] [ERROR]: Worker_03 heartbeat timed out (15023ms > 10ms). Process killed and respawned.
```

**Queue starvation protection:** Tasks queued longer than 50ms trigger pool expansion (up to the ceiling) or a `BALANCED` fallback:

```
WARN: Process pool starvation and timeout. Fallback to BALANCED.
```

### 4. `SafeChannel` — Adaptive IPC Pipeline

The `SafeChannel` provides an adaptive IPC layer between main thread and worker processes, automatically selecting the optimal transfer mechanism per payload:

| Payload Size | Mechanism | Behavior |
|---|---|---|
| ≤ 1 MB | `Structured Clone` | Deep copy via `structuredClone()` |
| > 1 MB | `Transferable Objects` | Zero-copy `ArrayBuffer` transfer, O(1) |
| Read-only state | `SharedArrayBuffer` | Lock-free shared memory for lookups and tensors |
| Continuous streams | `ReadableStream` / `WritableStream` | Streaming mode for large files and network packets |

```
# SafeChannel auto-selects the best mechanism:
# Small payload → structured clone
# Large array  → transferable (zero-copy)

1\ ACTION process_large(data([NUM])) WITH MISSION SAFE,
2\   REAP result FROM analyze, data.
2\   GIVE result.
1\ /ACTION.

# [TRACE] SafeChannel transferable mode activated for 2MB payload.
```

### 5. `MissionContext` — Unified Telemetry & Diagnostics

`MissionContext` wraps the active allocator, IPC channel, and process pool into a single telemetry interface:

| Method | Purpose | Debug-only? |
|---|---|---|
| `context.diagnostic(msg)` | Runtime escalation/warning logs | No |
| `context.trace(msg)` | Verbose execution trace | Yes (`--debug` flag) |
| `context.getMetrics()` | JSON metrics snapshot | No |

**Metrics output includes:**

```json
{
  "uptimeMs": 12345,
  "allocator": {
    "heapUsed": 262144,
    "heapCapacity": 8388608,
    "heapRemaining": 8126464,
    "fragmentationPct": "96.88",
    "escalated": false
  },
  "arcHeap": {
    "liveObjects": 42,
    "gcCycles": 3,
    "totalAllocations": 3102
  },
  "processPool": {
    "active": 2,
    "idle": 2,
    "dead": 0,
    "ceiling": 8,
    "queueLength": 0
  },
  "diagnosticsCount": 1,
  "tracesCount": 0
}
```

---

### Escalation & Safety Matrix

| Trigger Condition | Automatic Action | Diagnostic Log |
|---|---|---|
| `BumpAllocator` out of memory | Fallback to `BALANCED` | `WARN: Fast heap capacity exceeded` |
| Worker pool starvation (> 50ms) | Fallback to `BALANCED` | `WARN: Process pool starvation` |
| Worker heartbeat timeout (> 10ms) | Kill PID + Respawn | `ERROR: Worker heartbeat timed out` |
| 1000 allocations reached | Run cycle detection | `INFO: Scheduled GC cycle executed` |
| Payload > 1 MB detected | Enable `Transferable` zero-copy | `TRACE: SafeChannel transferable mode` |

---

## Parallel Compilation & Telemetry (v0.33.0)

Starting with v0.33.0, PlantLang ships with a **parallel compilation engine**, **distributed failover**, **lock-free telemetry**, and a **runtime dispatcher** — enabling multi-threaded codegen, remote compiler node fallback, and non-blocking metrics collection.

### 1. `ParallelCodegenEngine` — DAG-based Parallel Compilation

The `ParallelCodegenEngine` splits the AST into a dependence DAG, detects cycles, and distributes independent nodes across a `worker_threads` pool:

```
                     AST
                      │
              DAG Builder (Tarjan)
                      │
              Weighted Load Balancer
                 ┌────┼────┐
             Worker_0 │ Worker_1
                 bitcode assembly
                      │
              Lock-free merge → IR
```

**Cycle detection** rejects programs with circular dependencies:

```
1\ ACTION a(), 2\ REAP b FROM b. 1\ /ACTION.
1\ ACTION b(), 2\ GIVE 1 + a(). 1\ /ACTION.
# → ERROR: Cyclic dependency detected: a → b → a
```

**Load balancing** assigns each action a weight (1 + nested call count) and distributes across buckets for near-optimal CPU utilization.

### 2. `RemoteCompilerNode` — Transparent Distributed Failover

The `RemoteCompilerNode` ships compilation payloads to a remote TCP compiler node. If the remote node is unreachable within **100ms**, it falls back transparently to the local engine:

```
MISSION CONFIG REMOTE_NODE="tcp://192.0.2.1:9473".

1\ ACTION remote_kernel(data([NUM])) WITH MISSION FAST,
2\   REAP result FROM remote:compile, data.
2\   GIVE result.
1\ /ACTION.

# If 192.0.2.1:9473 is unreachable:
# → [WARN] Remote compiler unreachable after 100ms. Using local engine.
```

**Payload compression** uses zlib (deflate) to achieve ≥60% size reduction on the serialized AST before TCP transport.

### 3. `NonBlockingTelemetry` — Lock-free Metrics Ring Buffer

The `NonBlockingTelemetry` collector uses a **SharedArrayBuffer** ring buffer (128 entries × 64 bytes each) with atomic operations — zero GC pressure, zero contention:

```
1\ MISSION CONFIG TELEMETRY_ENABLED = TRUE.

1\ ACTION monitored_kernel() WITH MISSION FAST,
2\   REAP snap FROM telemetry:snapshot.
2\   SHOW snap.metrics[0].name.
1\ /ACTION.
```

Features:
- **`record(name, value)`**: O(1) lock-free atomic write to the ring buffer
- **`snapshot()`**: Zero-allocation structured copy of the current buffer state
- **Overflow detection**: Automatically advances read pointer when buffer is full
- **Background exporter**: Optional periodic export to external sinks (file, network)

### 4. `RuntimeDispatcher` — Parallel Toggle & Auto-Disable

The `RuntimeDispatcher` wraps the parallel engine and telemetry into a single switch:

```
1\ REAP dsp FROM dispatcher:create.
1\ REAP _ FROM dsp:enableParallelCodegen.
# → [DISPATCH] Parallel codegen enabled via API.

1\ ACTION heavy_task(), 2\ REAP result FROM crunch, data. 1\ /ACTION.

1\ REAP _ FROM dsp:disableParallelCodegen.
# → [DISPATCH] Parallel codegen disabled via API.
```

**Single-core auto-disable:** On machines with only 1 logical CPU, `enableParallelCodegen()` is a no-op — the dispatcher detects `os.cpus().length === 1` and skips parallel mode entirely.

---

## Zero-Trust Security & Audit Architecture (v0.34.0)

Starting with v0.34.0, PlantLang ships with a **production-ready Zero-Trust Security & Audit Architecture** — non-blocking audit logging with tamper-evident hash chains, mutual TLS with JWT authentication, and granular capability-based sandboxing for SAFE-mode isolation.

### 1. `NonBlockingAuditLogger` — Tamper-Evident Audit Ring Buffer

The `NonBlockingAuditLogger` provides lock-free, O(1) event logging using a SharedArrayBuffer ring buffer with a cryptographic hash chain:

```
Hash₀ = SHA256("genesis")
Hashₙ = SHA256(EventDataₙ + Hashₙ₋₁)
```

**Configuration:** `MISSION CONFIG AUDIT_RING_SIZE = 10000` or `process.env.AUDIT_RING_SIZE`.

```
1\ MISSION CONFIG AUDIT_RING_SIZE = 5000.

1\ ACTION secure_action() WITH MISSION SAFE,
2\   REAP snap FROM audit:snapshot.
2\   SHOW snap.metrics[0].data.
1\ /ACTION.
```

**Key features:**
- **Atomic ring buffer** — lock-free `record()` via `Atomics.add`, zero GC pressure
- **Tamper-evident hash chain** — each entry stores `SHA256(data + prevHash)`; `verifyIntegrity()` detects any memory tampering
- **Async Worker flush** — offloads disk/SIEM writes to a background thread, keeping FAST path overhead < 0.1ms
- **Overflow handling** — when buffer is full, oldest entry is dropped and a `WARN` is emitted

### 2. `mTLSJwtGuard` — Mutual TLS & JWT Authentication

The `mTLSJwtGuard` provides dual-authentication mTLS 1.3 with signed JWT tokens:

**Configuration via environment variables:**
```bash
export MTLS_CERT=/etc/plantlang/cert.pem
export MTLS_KEY=/etc/plantlang/key.pem
export MTLS_CA=/etc/plantlang/ca.pem
```

```
1\ MISSION CONFIG REMOTE_AUTH = JWT_RS256.

1\ ACTION call_remote() WITH MISSION FAST,
2\   REAP result FROM remote:execute, payload.
2\   GIVE result.
1\ /ACTION.
```

**Enforcement matrix:**

| Trigger Scenario | Action | Log Output |
|---|---|---|
| JWT forgery attempt | Reject immediately | `SECURITY_ALERT: JWT signature verification failed!` |
| Replay attack (jti reused) | Reject request | `SECURITY_ALERT: Replay attack detected for JWT ID` |
| Expired JWT token | Reject, request re-auth | `WARN: Expired JWT token presented.` |
| mTLS handshake failure | Abort TCP connection | `FATAL: mTLS handshake failed. Peer unverified.` |

Supports RS256 and Ed25519 signing algorithms.

### 3. `CapabilityGuard` — Zero-Trust SAFE Sandbox

The `CapabilityGuard` enforces least-privilege access for every mission mode. **By default, SAFE has zero permissions:**

| Mode | Default Permissions |
|---|---|
| **SAFE** | None (zero trust) |
| **BALANCED** | `FILE_READ`, `NET_CONNECT` |
| **FAST** | `FILE_READ`, `FILE_WRITE`, `NET_CONNECT` |
| **SMART** | `FILE_READ`, `FILE_WRITE`, `NET_CONNECT` |
| **PERSISTENT** | `FILE_READ`, `FILE_WRITE`, `NET_CONNECT`, `NET_LISTEN` |

Granular permissions are granted via the `MissionContext`:

```
1\ ACTION read_config() WITH MISSION SAFE,
2\   # Requires explicit FILE_READ grant
2\   REAP data FROM file:read, "/etc/plantlang/config".
2\   GIVE data.
1\ /ACTION.

# Grant: MISSION CONFIG SAFE_PERMISSIONS = FILE_READ, NET_CONNECT.
```

**Violation enforcement:**
- Unauthorized syscalls (`execve`, `ptrace`, `fork`, `clone`, `kill`) in SAFE mode immediately terminate the worker
- All denials emit a `CRITICAL` audit log entry
- Violation hooks allow custom alerting (SIEM, pager, etc.)

---

## Cluster Architecture & Distributed Memory (v0.35.0)

Starting with v0.35.0, PlantLang ships with a **production Cluster Architecture** — decentralized node discovery with heartbeat-based health monitoring, weighted-least-connections cluster routing with circuit-breaker failover, and a distributed hash-ring heap for stateful actors and PERSISTENT data.

Configuration uses `MISSION CONFIG` for all cluster parameters:

```
MISSION CONFIG HEARTBEAT_INTERVAL = 500.
MISSION CONFIG HEARTBEAT_THRESHOLD = 5.
MISSION CONFIG CIRCUIT_BREAKER_THRESHOLD = 0.15.
MISSION CONFIG CIRCUIT_BREAKER_COOLDOWN = 30000.
MISSION CONFIG CONSISTENT_HASH_VNODES = 64.
```

### 1. `NodeRegistry` — Heartbeat-based Node Discovery

The `NodeRegistry` maintains a real-time view of cluster topology. Each node has a lifecycle state — `HEALTHY`, `DEGRADED`, or `OFFLINE` — and carries telemetry (CPU, heap, active workers) updated on each heartbeat:

```
1\ CREATE reg FROM cluster:createRegistry.
1\ REAP id FROM reg:register, "node-A".
# → HEALTHY node registered

1\ REAP _ FROM reg:heartbeat, "node-A", { cpuUtil: 0.3, heapUsage: 0.5 }.

1\ REAP count FROM reg:aliveCount.
1\ SHOW count.        # → 1

1\ REAP _ FROM reg:configure, "HEARTBEAT_INTERVAL", 2000.
1\ REAP _ FROM reg:configure, "HEARTBEAT_THRESHOLD", 5.
```

**Heartbeat monitoring:** A background timer checks each node every `HEARTBEAT_INTERVAL` ms. If `missedBeats` reaches `ceil(threshold / 2)`, the node enters `DEGRADED`. At `threshold`, it becomes `OFFLINE`. A single heartbeat restores it to `HEALTHY`:

```
1\ REAP _ FROM reg:start.           # begin background monitor

# If node-A misses 3 heartbeats with threshold=5:
# → DEGRADED at 3 missed beats
# → OFFLINE at 5 missed beats

# Recovery:
1\ REAP _ FROM reg:heartbeat, "node-A".
# → node-A is HEALTHY again
```

**Topology events:** The registry emits events on state transitions — `node:registered`, `node:healthy`, `node:degraded`, `node:offline` — enabling reactive failover and alerting.

### 2. `ClusterRouter` & `CircuitBreaker` — Weighted Load Balancing with Failover

The `ClusterRouter` selects the optimal target node for each dispatch using **weighted least-connections** — it chooses the alive node with the lowest active connection count, breaking ties by CPU utilization:

```
1\ REAP router FROM cluster:createRouter, reg.

1\ REAP result FROM router:dispatch, "task.action", payload.
# → Result from lowest-connection node
# → On circuit-open, transparently fails over to backup
```

Each node gets a per-node `CircuitBreaker` with three states:

| State | Behavior | Transition |
|---|---|---|
| `CLOSED` | Requests allowed normally | → OPEN when error rate ≥ threshold |
| `OPEN` | All requests rejected (fast-fail) | → HALF-OPEN after `cooldown` ms |
| `HALF-OPEN` | One probe request allowed | → CLOSED on success, → OPEN on failure |

**Circuit breaker configuration via MISSION CONFIG:**

```
MISSION CONFIG CIRCUIT_BREAKER_THRESHOLD = 0.10.
MISSION CONFIG CIRCUIT_BREAKER_COOLDOWN = 15000.
```

When the primary node's circuit is OPEN, the router selects a backup node. If both fail, the error is aggregated and a `router:circuit_open` security alert is emitted:

```
# → SECURITY_ALERT: Node peer disconnected. Executing failover.
# → Cluster dispatch failed on primary and backup: <error>
```

**mTLSJwtGuard integration:** If configured, each dispatch authenticates the target node via JWT before execution. Failed verifications produce a `router:auth_failure` event and reject the dispatch.

### 3. `DistributedHeap` & `ConsistentHashRing` — PERSISTENT Stateful Actor Storage

The `DistributedHeap` wraps a `ConsistentHashRing` using **SHA-256** hashing mapped to BigInt for key distribution — with configurable virtual nodes (default 128 per physical node) for balanced ownership:

```
1\ REAP heap FROM cluster:createHeap, { localNodeId: "server-1" }.

1\ REAP _ FROM heap:addNode, "server-1".
1\ REAP _ FROM heap:addNode, "server-2".

1\ REAP _ FROM heap:put, "config-key", { timeout: 30, retries: 3 }.
1\ REAP val FROM heap:get, "config-key".
```

**Lease-based expiration:** Keys have a configurable lease duration (default 5000ms). Expired keys are lazily evicted on `get()` and actively collected by a background GC timer:

```
MISSION CONFIG LEASE_DURATION = 30000.
```

**Stateful actors:** The heap supports actor-style state management where each actor is owned by exactly one ring node. Mutations from non-owners are transparently proxied:

```
1\ REAP owner FROM heap:registerActor, "session-1".
1\ SHOW owner.     # → "server-2" (deterministic via hash ring)

1\ REAP _ FROM heap:setActorState, "session-1", { count: 42 }, owner.
# → { proxied: false }

1\ REAP _ FROM heap:setActorState, "session-1", { count: 99 }, "server-1".
# If server-1 is not the owner:
# → { proxied: true, owner: "server-2" }
```

**Node addition & migration:** When a new node joins, `computeMigrationStats(newNodeId)` reports how many keys would move. `computeDataKeyMigration(existingKeys)` returns the exact set of keys whose owner changed — enabling zero-downtime rebalancing:

```
1\ REAP stats FROM heap:computeMigrationStats, "server-3".
1\ SHOW stats:totalKeys.      # → 1000
1\ SHOW stats:ratio.          # → 0.25 (25% of keys migrate)
```

**Consistent hash distribution:** With default 128 virtual nodes per physical node, 1000 keys distributed across 2 nodes achieve a near-even split (typical ratio ≥ 0.98).

---

## Geographic Routing & State Governance (v0.36.0)

Starting with v0.36.0, PlantLang ships with a **Geographic Routing & State Governance Engine** — SHARE CONFIG for shared state with dual-path consensus (RAFT / CRDT), TCP Gossip invalidation for immutable versioned snapshots, bounded call-graph affinity analysis for co-located function placement, and adaptive SMART execution routing across LOCAL_CPU, REMOTE_NODE, and GPU_ACCELERATED targets.

Configuration uses `MISSION CONFIG` for all geo-routing and governance parameters:

```
MISSION CONFIG GOSSIP_PROPAGATION_MS = 50.
MISSION CONFIG CONSENSUS_ENGINE = RAFT.
MISSION CONFIG CALL_GRAPH_MAX_DEPTH = 3.
MISSION CONFIG SMART_ROUTE_GPU_MIN_BYTES = 1048576.
MISSION CONFIG SMART_ROUTE_MAX_LATENCY_MS = 15.
```

### 1. `ShareGovernance` — SHARED_READ / SHARED_WRITE State Engine

The `ShareGovernance` module manages global configuration state with two access paths:

**SHARED_READ (Immutable Versioned Snapshot):** High-frequency read variables are distributed via versioned snapshot broadcasts. Read operations execute locally in **O(1)** with zero lock contention:

```
1\ REAP sg FROM governance:create.

# Declare a read-only config key
1\ REAP _ FROM sg:declareReadOnly, "max_connections", 5000.

# O(1) local read — zero lock contention
1\ REAP config FROM sg:read, "max_connections".
1\ SHOW config:value.    # → 5000
1\ SHOW config:version.  # → 1

# Invalidation triggers TCP Gossip across the cluster
1\ REAP _ FROM sg:invalidate, "max_connections", 10000.
1\ REAP config FROM sg:read, "max_connections".
1\ SHOW config:value.    # → 10000
```

**TCP Gossip invalidation:** When a `SHARED_READ` value is updated, the change propagates to peer nodes within `GOSSIP_PROPAGATION_MS`:

```
1\ REAP _ FROM sg:addPeer, "worker-1".
1\ REAP _ FROM sg:addPeer, "worker-2".
1\ REAP _ FROM sg:declareReadOnly, "threshold", 0.9.
1\ REAP _ FROM sg:invalidate, "threshold", 0.95.
# → gossip propagates threshold=0.95 to all peers within 50ms
```

**SHARED_WRITE (Dynamic Consensus Engine):** Mutable state routes through a pluggable consensus engine:

```
# RAFT — single-leader linearizable consensus
1\ REAP _ FROM sg:declareMutable, "cluster_config", "RAFT".
1\ REAP _ FROM sg:write, "cluster_config", { replicas: 3 }.
# → RAFT: replicates to followers, commits on majority

# CRDT — conflict-free convergent state
1\ REAP _ FROM sg:declareMutable, "counter", "CRDT".
1\ REAP _ FROM sg:write, "counter", { count: 42 }.
# → CRDT: LWW register with lamport clock, merges automatically
```

**Directive syntax:** `SHARE CONFIG <KEY> READ_ONLY|MUTABLE [CONSENSUS=RAFT|CRDT]`

```
1\ SHARE CONFIG db_host READ_ONLY.
1\ SHARE CONFIG leader MUTABLE CONSENSUS=RAFT.
1\ SHARE CONFIG distributed_counter MUTABLE CONSENSUS=CRDT.
```

### 2. `CallGraphAnalyzer` — Bounded Static Affinity Analysis

The `CallGraphAnalyzer` inspects function call graphs during AST pre-compilation to co-locate high-density inter-dependent functions onto the same cluster node — eliminating cross-network IPC latency:

```
1\ REAP cga FROM affinity:create, { maxDepth: 3 }.

1\ REAP _ FROM cga:addFunction, "api_handler", ["auth", "validate", "process"].
1\ REAP _ FROM cga:addFunction, "auth", ["db_lookup"].
1\ REAP _ FROM cga:addFunction, "process", ["transform", "enrich"].

# Bounded analysis — maximum depth 3
1\ REAP depth FROM cga:getDepth, "api_handler".
1\ SHOW depth.  # → 3

# Compute affinity groups — Louvain-inspired community detection
1\ REAP groups FROM cga:computeAffinityGroups.
1\ SHOW COUNT(groups).    # → 1 (all functions co-located)

# Static placement — assign each affinity group to a target node
1\ REAP placement FROM cga:computePlacement, ["node-1", "node-2"].
# → api_handler, auth, validate, process → node-1
# → db_lookup, transform, enrich → node-2
```

**Bounded depth guarantee:** The analyzer enforces `CALL_GRAPH_MAX_DEPTH` (default 3, configurable 1–10) during pre-compilation to guarantee O(V·E₍bₒᵤₙdₑd₎) pass times without compiler slowdown.

### 3. `SmartExecutionRouter` — Adaptive Triage (CPU / Remote / GPU)

The `SmartExecutionRouter` dynamically selects the optimal compute target at invocation time with **< 0.05ms routing overhead**:

| Condition | Target | Description |
|---|---|---|
| Default | `LOCAL_CPU` | Normal workloads, or remote latency ≥ `SMART_ROUTE_MAX_LATENCY_MS` |
| Local CPU > 70% + latency < threshold | `REMOTE_NODE` | Offload to lowest-latency alive remote node |
| Vector op + payload ≥ `SMART_ROUTE_GPU_MIN_BYTES` | `GPU_ACCELERATED` | Matrix/vector/tensor operations on registered GPU pipeline |

```
1\ REAP router FROM smart:createRouter, { gpuMinBytes: 1048576, maxLatencyMs: 15 }.
1\ REAP _ FROM router:registerGpuPipeline, "cuda-0".

# Small workload → LOCAL_CPU
1\ REAP decision FROM router:selectTarget, "simple_add", { x: 1, y: 2 }.
1\ SHOW decision:target.  # → LOCAL_CPU

# High CPU load + remote available → REMOTE_NODE
1\ REAP _ FROM router:updateLocalCpuLoad, 0.85.
1\ REAP _ FROM router:setLatency, "worker-1", 10.
1\ REAP decision FROM router:selectTarget, "process_data", items.
1\ SHOW decision:target.  # → REMOTE_NODE

# Large matrix operation → GPU_ACCELERATED
1\ REAP decision FROM router:selectTarget, "matrix_multiply", largeArray.
1\ SHOW decision:target.  # → GPU_ACCELERATED
```

**MISSION CONFIG integration:**
- `SMART_ROUTE_GPU_MIN_BYTES` — minimum payload (default 1MB) to trigger GPU path
- `SMART_ROUTE_MAX_LATENCY_MS` — maximum acceptable remote latency (default 15ms)

---

## Distributed Cycles & Replica Governance (v0.37.0)

Starting with v0.37.0, PlantLang ships with a **Distributed Cycles & Replica Governance Engine** — partitioned loop distribution across cluster workers, stateless/stateful replication strategies, and dual-mode result aggregation for distributed execution.

Configuration uses `MISSION CONFIG` for all distributed cycle and replica parameters:

```
MISSION CONFIG REPLICA_STRATEGY = LEAST_CONNECTIONS.
MISSION CONFIG PRIMARY_BACKUP_ACK = QUORUM.
MISSION CONFIG CYCLE_CORE_FACTOR = 2.0.
MISSION CONFIG CYCLE_MIN_CHUNK_SIZE = 100.
MISSION CONFIG WORKER_TIMEOUT_MS = 5000.
MISSION CONFIG REMOTE_REAP_TARGET = MEMORY_BUFFER.
```

### 1. `ReplicaManager` — Stateless Routing & Stateful Primary-Backup

The `ReplicaManager` provides two replication strategies configurable via `MISSION CONFIG REPLICA_STRATEGY`:

**Stateless Routing** — distributes requests across available nodes:

| Strategy | Behavior |
|---|---|
| `LEAST_CONNECTIONS` (default) | Selects node with fewest active connections — even distribution over many calls |
| `ROUND_ROBIN` | Cycles through nodes in order — deterministic, predictable |

```
1\ REAP rm FROM replica:create, { strategy: "ROUND_ROBIN" }.
1\ REAP target FROM rm:selectStatelessTarget.
1\ SHOW target.   # → "node-A"

1\ REAP target FROM rm:selectStatelessTarget.
1\ SHOW target.   # → "node-B"
```

**Stateful Primary-Backup** — assigns a primary and N backups for stateful actors:

```
1\ REAP result FROM rm:assignPrimary, "session-1".
1\ SHOW result:primary.   # → "node-A"
1\ SHOW COUNT(result:backups).   # → 2
```

**Delta replication log:** Each mutation is recorded as a versioned log entry:

```
1\ REAP log FROM rm:replicate, "session-1", { type: "UPDATE", field: "score", value: 42 }.
1\ SHOW log:version.   # → 1
1\ SHOW log:mode.      # → QUORUM
```

**ACK modes:** `ONE` (single backup ACK), `QUORUM` (majority, default), `ALL` (every backup):

```
MISSION CONFIG PRIMARY_BACKUP_ACK = ALL.
```

**Primary failover:** When the primary fails (detected via NodeRegistry `node:offline`), the highest-priority backup is automatically promoted:

```
1\ REAP _ FROM reg:markOffline, "node-A".
# → ReplicaManager receives node:offline event
1\ REAP newPrimary FROM rm:getPrimary, "session-1".
1\ SHOW newPrimary.   # → "node-B" (promoted from backup)
1\ REAP log FROM rm:readLedger, "session-1".
# → Ledger preserved after failover
```

### 2. `DistributedCycleEngine` — Adaptive Chunked Loop Execution

The `DistributedCycleEngine` partitions large iteration spaces into chunks and distributes them across cluster workers for parallel execution.

**Chunk size formula:**
```
chunkSize = max(CYCLE_MIN_CHUNK_SIZE, ceil(N / (activeWorkers × CYCLE_CORE_FACTOR)))
```

```
1\ REAP dce FROM cycles:create, { coreFactor: 2, minChunkSize: 100 }.
1\ REAP scatter FROM dce:scatter, 50000.
1\ SHOW scatter:totalChunks.   # → 4 (with 2 workers, coreFactor=2)
1\ SHOW scatter:chunkSize.     # → 12500
```

**Work-stealing:** When a worker completes a chunk, the engine automatically steals pending chunks from the queue and assigns them to idle workers:

```
1\ REAP _ FROM dce:completeChunk, "worker-1", 0, result.
# → _trySteal() assigns next pending chunk to worker-1
```

**Timeout recovery:** Straggler workers are detected via `checkTimeouts()` at `WORKER_TIMEOUT_MS` — timed-out chunks are re-queued and reassigned:

```
MISSION CONFIG WORKER_TIMEOUT_MS = 10000.

1\ REAP timedOut FROM dce:checkTimeouts.
1\ SHOW COUNT(timedOut).   # → chunks re-queued and available for reassignment
```

**Completion detection:**

```
1\ REAP done FROM dce:isComplete.
1\ SHOW done.   # → true (all chunks completed)
```

### 3. `ReapAggregator` — LOCAL_REAP / REMOTE_REAP Result Aggregation

The `ReapAggregator` provides dual-mode result collection for distributed cycle outputs:

**LOCAL_REAP** — in-memory deterministic collection with reduce, merge, and flush:

```
1\ REAP ra FROM reap:create, { mode: "LOCAL_REAP" }.

# Collect results
1\ REAP _ FROM ra:collect, [1, 2, 3].
1\ REAP _ FROM ra:collect, [4, 5, 6].

# Reduce by summing
1\ REAP total FROM ra:reduce, (a, b) => a + b, 0.
1\ SHOW total.   # → 21

# Merge by key (deduplication)
1\ REAP merged FROM ra:merge, (r) => r[0], (a, b) => a.concat(b).
1\ SHOW COUNT(merged).   # → 2

# Flush results
1\ REAP flushed FROM ra:flush.
1\ SHOW flushed.   # → 2 (results cleared)
```

**REMOTE_REAP** — streams results to a remote target:

```
1\ REAP ra FROM reap:create, { mode: "REMOTE_REAP", remoteTarget: "MEMORY_BUFFER" }.

# Register a URI handler for custom targets
1\ REAP _ FROM ra:registerHandler, "s3://my-bucket", s3Handler.

# Collect streams to target
1\ REAP _ FROM ra:collect, [10, 20, 30].
1\ REAP _ FROM ra:collect, [40, 50, 60].
1\ REAP state FROM ra:getState.
1\ SHOW state:resultCount.   # → 2
```

**MISSION CONFIG integration:**
- `REPLICA_STRATEGY` — `LEAST_CONNECTIONS` (default) or `ROUND_ROBIN`
- `PRIMARY_BACKUP_ACK` — `ONE`, `QUORUM` (default), or `ALL`
- `CYCLE_CORE_FACTOR` — `1.0` to `10.0` (default `2.0`)
- `CYCLE_MIN_CHUNK_SIZE` — `10` to `100000` (default `100`)
- `WORKER_TIMEOUT_MS` — `1000` to `60000` (default `5000`)
- `REMOTE_REAP_TARGET` — `MEMORY_BUFFER` (default) or a URI with `scheme://`

---

## Integrated Testing & Native Networking (v0.41.0)

Starting with v0.41.0, PlantLang ships with an **Integrated Testing Framework** (`SUITE`/`VERIFY`), **Native Network Primitives** (`HARVEST`/`LISTEN BRANCH`), and **CodeWords Safety Governance** (`#ALLOW_NETWORK`, `#ALLOW_HARVEST`, `#ALLOW_LISTEN`).

### 1. CodeWords Safety Governance

The `CodeWordsChecker` enforces capability-based access control at the AST level. Network-related statements (`HARVEST`, `LISTEN BRANCH`) are rejected unless the source file declares the appropriate directive:

```
#ALLOW_NETWORK
SUITE "Network Tests",
  VERIFY "harvest works", TRUE
SUITE/.
```

**Directive reference:**

| Directive | Implies | Description |
|:---|:---|:---|
| `#ALLOW_NETWORK` | `#ALLOW_HARVEST`, `#ALLOW_LISTEN` | Broad network permission |
| `#ALLOW_HARVEST` | — | Permit outbound HTTP requests |
| `#ALLOW_LISTEN` | — | Permit TCP socket listeners |

### 2. `SUITE` / `VERIFY` — Test Runner (legacy JS-era)

> Removed in v0.46.4 with the JavaScript engine. Kept for historical record.

The legacy `plantc test <file.plant>` subcommand discovers all `SUITE` blocks and executes their `VERIFY` assertions:

```
SUITE "Arithmetic",
  VERIFY "2 + 2 = 4", 2 + 2 IS 4
  VERIFY "zero is falsy", 0
SUITE/.

SUITE "String",
  VERIFY "hello is truthy", "hello"
SUITE/.
```

Each `VERIFY` evaluates its assertion expression; truthy passes, falsy fails. Nested `SUITE` blocks are supported and their counts aggregate upward.

### 3. `HARVEST` — Outbound HTTP Requests

```
#ALLOW_HARVEST
CREATE result(TX).
HARVEST "http://example.com/data" METHOD:"GET" AS result.
SHOW result.
```

`HARVEST` sends an HTTP GET (or custom METHOD) to the target URL and stores the response body in the result variable. At compile time, it emits a call to the C runtime's `plant_net_harvest()` POSIX socket helper.

### 4. `LISTEN BRANCH` — TCP Socket Server

```
#ALLOW_LISTEN
LISTEN 8080 AS req,
  SHOW req.
```

`LISTEN BRANCH` opens a TCP listener on the given port. For each incoming connection, it reads the request data into the bound identifier, executes the handler body, and sends a response. The listener loops until shutdown.

## Geo-Aware Cycles & Dynamic Replica Rebalancing (v0.40.0)

Starting with v0.40.0, PlantLang ships with **Geo-Aware Cycle Execution**, **Stream Compaction** for REMOTE_REAP payloads, and **Dynamic Replica Rebalancing** on node churn.

### 1. `GeoTopologyManager` — Dynamic Latency-Aware Node Selection

The `GeoTopologyManager` maintains a real-time RTT latency matrix between cluster nodes via continuous probing. Each node carries optional topology metadata (region, zone, datacenter) that seeds initial latency estimates:

```
1\ REAP geo FROM topology:create, { probeInterval: 5000 }.
1\ REAP _ FROM geo:registerNode, "node-us-1", { region: "us", zone: "us-east", datacenter: "us-east-1a", localityKey: "us-east" }.
1\ REAP _ FROM geo:registerNode, "node-eu-1", { region: "eu", zone: "eu-west", datacenter: "eu-west-1a", localityKey: "eu-west" }.

# Probe all nodes — establishes RTT matrix
1\ REAP _ FROM geo:probeAll.

# Query latency between two nodes
1\ REAP lat FROM geo:getLatency, "node-us-1", "node-eu-1".
1\ SHOW lat.   # → > 50ms (cross-region)

# Select optimal nodes for a data locality key
1\ REAP nodes FROM geo:getOptimalNodes, "us-east", 2.
1\ SHOW COUNT(nodes).   # → 2 (both us-east nodes)
```

**MISSION CONFIG:**
- `GEO_PROBE_INTERVAL` — probe interval in ms (1000-60000, default 5000)
- `GEO_PROBE_TIMEOUT` — probe timeout in ms (100-10000, default 1000)

### 2. `StreamCompactor` — Binary REAP Payload Compression

The `StreamCompactor` provides binary serialization for `REMOTE_REAP` streams with zlib deflateRaw compression, achieving 60-85% payload reduction vs plaintext JSON:

```
1\ REAP sc FROM stream:create, { compressionLevel: 6 }.

# Compress a REAP stream with typed headers
1\ CREATE payload(TX) TO '{"data": "large payload...", "count": 1000}'.
1\ REAP compressed FROM sc:compress, { type: "CYCLE_RESULT", source: "node-us-1", version: 1 }, payload.
1\ SHOW TYPE compressed.   # → Buffer (binary)

# Decompress at the remote end
1\ REAP result FROM sc:decompress, compressed.
1\ SHOW result:headers:type.    # → CYCLE_RESULT
1\ SHOW result:headers:source.  # → node-us-1
1\ SHOW result:payload.         # → original JSON string
1\ SHOW result:timestamp.       # → Unix timestamp (NUM)
```

**Binary format:**
- 4 bytes: magic bytes `PLRS` (PlantLang REAP Stream)
- 1 byte: format version
- 6 bytes: 48-bit Unix timestamp
- 4 bytes: original (uncompressed) payload size
- 4 bytes: header length (JSON-encoded)
- N bytes: zlib deflateRaw compressed payload
- M bytes: JSON-encoded typed headers

**MISSION CONFIG:**
- `STREAM_COMPRESSION` — zlib level 1-9 (default 6)
- `STREAM_CHUNK_SIZE` — max chunk size in bytes (1024-262144, default 65536)

### 3. Geo-Aware `DistributedCycleEngine` Execution

The `DistributedCycleEngine` now integrates with `GeoTopologyManager` for geo-affine block placement — cycles with a locality affinity are dispatched to the lowest-latency nodes:

```
1\ REAP dce FROM cycles:create, { nodeRegistry: reg }.
1\ REAP _ FROM dce:setGeoTopologyManager, geo.

# Execute a block with geo affinity — dispatched to optimal us-east node
1\ REAP result FROM dce:executeCycleBlock, blockData, "us-east".
1\ SHOW result:executed.      # → true
1\ SHOW result:geoAffinity.   # → us-east
1\ SHOW result:workerId.      # → node-us-1
```

If no `GeoTopologyManager` is configured or no locality key is provided, execution falls back to the `NodeRegistry` worker list.

### 4. Dynamic Replica Rebalancing on Node Churn

The `ReplicaManager` now responds to `NODE_JOIN` and `NODE_LEAVE` gossip events by automatically rebalancing partitions and healing replicas:

```
1\ REAP rm FROM replica:create, { nodeRegistry: reg }.
1\ REAP _ FROM rm:assignPrimary, "actor-1".
1\ REAP _ FROM rm:assignPrimary, "actor-2".
1\ REAP _ FROM rm:assignPrimary, "actor-3".

# A new node joins — partitions rebalance, replicas heal
1\ REAP result FROM rm:handleNodeJoin, "node-D".
1\ SHOW result:rebalanced.   # → true
# Some primaries from overloaded nodes migrate to node-D
# Under-replicated actors get node-D as a backup replica

# A node departs — primaries failover, backup entries cleaned
1\ REAP result FROM rm:handleNodeLeave, "node-A".
1\ SHOW result:affectedActors.   # → number of actors that lost their primary
```

**Rebalancing algorithm:**
1. On join: compute ideal actors-per-node, identify overloaded nodes, migrate excess primaries to the new node
2. On join: scan all replica ledgers for under-replicated actors, assign the new node as a backup
3. On leave: trigger primary failover for affected actors, clean backup references, emit events

All rebalancing operations emit EventEmitter events (`node:join`, `node:leave`, `rebalance:complete`, `partition:moved`, `replica:healed`) for monitoring and observability.

---

## File Extensions

## Choosing the Right Mission Mode

Since v0.32.0, every `ACTION` can declare an execution **mission** using the `WITH MISSION <MODE>` syntax. The mission mode controls memory behavior, optimization paths, and cross-mode call permissions via the **Boundary Handshake Matrix**.

> **Syntax**: `ACTION name(params) WITH MISSION <MODE>,`  
> **Default**: If omitted, the mode is `BALANCED`.  
> **Restriction**: Only valid at Depth 0 (top-level ACTION declarations).

### The Five Modes at a Glance

| Mode | When to Use | Key Restriction |
|---|---|---|
| **BALANCED** | Default general-purpose — safe, typed, predictable | None — can call any mode |
| **FAST** | Performance-critical loops, numeric kernels, hot paths | Cannot invoke SAFE callers |
| **SAFE** | Untrusted input, sandboxed execution, audit boundaries | Cannot invoke FAST, SMART, or PERSISTENT |
| **SMART** | Data-dependent algorithms that adapt to input size | None — can call any mode |
| **PERSISTENT** | Long-lived caches, config stores, persistent state | Cannot invoke SAFE callers |

### Code Examples

**BALANCED (default — no annotation needed):**

```
1\ ACTION add(a(NUM), b(NUM)),
2\   GIVE a + b.
1\ /ACTION.
```

**FAST — explicit performance mode:**

```
1\ ACTION fast_sum(values([NUM])) WITH MISSION FAST,
2\   CREATE total(NUM) TO 0.
2\   CYCLE i FROM 0 TO COUNT(values) - 1,
3\     INCREASE total BY values[i].
2\   .
2\   GIVE total.
1\ /ACTION.
```

**SAFE — sandboxed, isolated execution:**

```
1\ ACTION sanitize(input(TX)) WITH MISSION SAFE,
2\   REAP cleaned FROM strings:TRIM, input.
2\   GIVE cleaned.
1\ /ACTION.
```

SAFE cannot call FAST (would compromise isolation), PERSISTENT (would create objects that escape the sandbox), or SMART (may dynamically route to FAST).

**SMART — adapts execution to data size:**

```
1\ ACTION process_data(items([NUM])) WITH MISSION SMART,
2\   SHOW COUNT(items).
1\ /ACTION.
```

For N < 1000 items, the function runs in scalar inline mode. For N ≥ 1000, it switches to parallel vector mode (chunked element-by-element processing).

**PERSISTENT — long-lived objects:**

```
1\ ACTION create_cache() WITH MISSION PERSISTENT,
2\   CREATE cache(MAP[TX,NUM]).
2\   LINK "version" WITH 1 IN cache.
2\   GIVE cache.
1\ /ACTION.
```

### The Boundary Handshake Matrix

When your action calls another action with a different mission mode, the Boundary Handshake Matrix checks if the call is permitted:

```
BALANCED → any mode:               ✅ Always allowed
FAST → SAFE:                       ❌ Denied
SAFE → FAST, SMART, PERSISTENT:    ❌ Denied
```

Violations produce a clear `BoundaryViolationError` explaining why:

```
═══ ⚔ BoundaryViolationError ═══
  SAFE -> FAST: SAFE is isolated and cannot invoke unguarded FAST code.
```

### Performance Implications

- **BALANCED**: Full type checking, arena-based memory, standard execution — best for most code.
- **FAST**: Skips safety checks, optimized codegen — use for hot inner loops and numeric kernels.
- **SAFE**: ScopedArena sandbox with memory isolation — slight overhead for the isolation guarantee.
- **SMART**: Dynamic routing between scalar and vector paths — ideal for data-processing functions.
- **PERSISTENT**: Allows objects to outlive their creating scope — use for caches and long-lived state.

---

## License

MIT — © 2025 PlantLang Project
