# 🌿 PlantLang — Chloroplast v0.48.4

> **A programming language designed to read like natural prose.**
> Write code the way you write a sentence — not the way you debug a cipher.
> **Chloroplast** is the pure-native, self-hosted PlantLang compiler:
> `src/plantc/*.plant` compiles to C, which links against a small native
> runtime. No Node.js, no JavaScript, no interpreter.

```
ACTION greet(name(TX)) -> TX,
  GIVE "hello, " + name.
/GIVE greet.

ACTION main(),
  REAP msg FROM greet, "Haider".
  SHOW msg.
  GIVE 0.
/GIVE main.
```

---

## Installation

Requires `gcc` and `make`. No other dependencies (no Node.js, no LLVM).

```bash
make all        # full native build → bin/Chloroplast (v1→v2→v3 chain)
make self       # multi-generation self-hosting + byte-convergence check
make test       # native + generics + closures integration suites
make install    # install to ~/.local (PREFIX=/path/to/prefix to override)
```

## Quick Start

```bash
./bin/Chloroplast hello.plant out.c      # compile PlantLang to C
gcc -w -O0 -I runtime/c out.c runtime/c/plant_runtime.c -lm -o hello
./hello
```

`hello.plant`:

```
ACTION main(),
  SHOW "hello, world".
  GIVE 0.
/GIVE main.
```

CLI:

```bash
./bin/Chloroplast --help        # usage + options
./bin/Chloroplast --version     # Chloroplast 0.48.4 (pure native)
./bin/Chloroplast file.plant [out.c]   # default output: file.c
```

---

## Program Structure

A program is a sequence of **declarations** (actions, externals, structs,
enums, mission config) and **statements**. Every statement ends with a dot
(`.`). `#` starts a line comment to end of line.

Programs conventionally end with a top-level `ACTION main()`; the runtime
driver calls it as the entry point. The compiler itself (in `src/plantc/`)
is written in this language and bootstraps to a byte-identical fixed point.

```
# Functions
ACTION add(a(NUM), b(NUM)),
  GIVE a + b.
/GIVE add.

# Entry point
ACTION main(),
  REAP r FROM add, 10, 25.
  CREATE rn(NUM) TO 0.
  SET rn TO r.            # numeric result → NUM var
  REAP rs FROM _from_long, rn.
  SHOW rs.                # → 35
  GIVE 0.
/GIVE main.
```

> Legacy syntax reference: the pre-v0.46 JS engine's `N\` depth-prefixed
> forms (e.g. `1\ SHOW "x".`), `MISSION: SAFE.` mode declarations, and the
> `core/*.js` architecture are historical only and do not ship in v0.48.4.

---

## Language Tour

### Variables & Types

| Type | Keyword | Example |
|------|---------|---------|
| Integer | `NUM` | `CREATE age(NUM) TO 25.` |
| Decimal | `SCL` | `CREATE pi(SCL) TO 3.14.` |
| Text | `TX` | `CREATE name(TX) TO "Haider".` |
| Boolean | `FACT` | `CREATE active(FACT) TO TRUE.` |
| List | `LIST` | `CREATE parts(LIST) TO plant_list_make(0).` |
| Generic list | `LIST[T]` | `CREATE xs(LIST[NUM]) TO plant_list_make(0).` |
| Struct | `STRUCT` | `STRUCT Point { x: NUM, y: NUM }` |
| Enum | `ENUM` | `ENUM Color { RED, GREEN, BLUE }.` |

Declare a variable with `CREATE` (and re-assign with `SET`):

```
CREATE score(NUM) TO 94.
SET score TO score + 6.
SHOW "score=" + score.   # → 100

CREATE name(TX) TO "Haider".
CREATE pi(SCL) TO 3.14159.
CREATE active(FACT) TO TRUE.
CREATE fruits(LIST) TO plant_list_make(3, "apple", "banana", "kiwi").
```

`LET` is an accepted alias for `CREATE` (same semantics). `SET` requires a
prior `CREATE` — there is no implicit declaration.

### Maps

**Map literals (v0.49.5):** `{k: v}` declares and initializes a
key-value structure in place — keys and values are quoted strings,
numbers, variables, nested lists `[ ... ]`, or nested maps `{ ... }`;
NUM-typed values wrap in `_from_long`:

```
CREATE user(LIST) TO { "name": "Haider", "score": 94 }.
SHOW _map_get(user, "name").            # → Haider

CREATE n(NUM) TO 10.
CREATE cfg(LIST) TO { "tags": ["a", "b"], "limits": { "max": n + 1 } }.
CREATE empty(LIST) TO {}.               # → plant_map_create()
```

The literal compiles to a chain of pair-list MAP setters —
`plant_map_set(plant_map_set(plant_map_create(), "name", "Haider"),
"score", _from_long(94))` — producing the same pair-list MAP form
that `LINK`, `_map_get`, `plant_map_to_string`, `json_stringify` and
the LISTEN/HARVEST request maps all consume. Read with `_map_get`,
serialize with `plant_map_to_string` (→ `{name = Haider, ...}`) or
`json_stringify` (→ `{"name":"Haider",...}`).

Maps can also be built incrementally:

```
CREATE user(LIST) TO plant_map_create().
plant_map_set(user, "name", "Haider").
plant_map_set(user, "score", _from_long(94)).

REAP nm FROM _map_get, user, "name".
SHOW nm.             # → Haider
```

`plant_map_set(...)` as a bare call statement is a first-class statement
(v0.48.4) and upserts (existing keys are replaced). Note: bare NUM
variables as keys or values (e.g. `{n: "x"}`) are not yet wrapped —
use a numeric expression or literal. The C-level hash-table API used
by struct/FFI marshalling is `plant_map_hash_create` /
`plant_map_hash_set` (renamed in v0.49.5).

### ENUM

```
ENUM Color { RED, GREEN, BLUE }.

ACTION describe(c(Color)),
  IF c IS GREEN,
    GIVE "green".
  /IF.
  GIVE "other".
/GIVE describe.

ACTION main(),
  REAP s FROM describe, GREEN.
  SHOW s.            # → green
  GIVE 0.
/GIVE main.
```

Enums compile to native C `typedef enum` blocks; variants are plain
integer identifiers, so compare them with `IF c IS GREEN` (string-
concatenating a variant directly is not supported by the v0.48 generator).

### STRUCT

```
STRUCT Point { x: NUM, y: NUM }
```

`STRUCT` declares a typed aggregate. The codegen emits a C typedef
(`plant_Point`) and conversion helpers (`plant_map_to_Point`,
`plant_Point_to_map`, …). Struct values flow through the FFI as opaque
`tx_t` handles:

```
STRUCT Point { x: NUM, y: NUM }

ACTION ffi_make_point(x(NUM), y(NUM)) -> external.
ACTION ffi_point_sum(p(Point)) -> external.

ACTION main(),
  REAP p FROM ffi_make_point, 3, 4.
  CREATE pp(Point) TO p.
  REAP s FROM ffi_point_sum, pp.
  SHOW "sum=" + s.      # → sum=7
  GIVE 0.
/GIVE main.
```

**Generic structs** (v0.48.1):

```
STRUCT Box[T] { val: T }
STRUCT Pair[T, U] { first: T, second: U }
```

Each instantiation used in the program is monomorphized into a concrete
typedef (`plant_Box_NUM`, `plant_Pair_NUM_TX`, …); uninstantiated templates
emit nothing.

**Field access (v0.49.10):** `a.b.c` on a map-backed LIST reads a key
with `_map_get` — no explicit `_map_get` call needed:

```
CREATE m(LIST) TO { "name": "root", "count": 7,
                    "inner": { "val": "9" },
                    "list": ["a", "b", "c"] }.
SHOW m.name.                # → root          (_map_get(m, "name"))
SHOW m.inner.val.           # → 9             (chained, 3 levels)
SHOW m.count + 1.           # → 8             (numeric coercion)
SHOW "x=" + m.name.         # → x=root        (concatenates)
SHOW m.list[0].             # → a             (index into a field)
CREATE n(NUM) TO m.count.   # numeric field read wraps in _to_long
```

Rules: a trailing bare `IDENT .` binds to that IDENT only; other tails
(chains, `x[0].name`, string expressions) use the whole expression as
the lookup target. The token after `.` must be a non-keyword IDENT not
followed by `(` or `:`. A field next to a numeric literal is coerced
with `_to_long` (so `m.count + 1` is arithmetic); field + field or
field + string has no literal digit, so it concatenates — field values
are text lookups.

**Method calls (v0.49.11):** `obj.method(args)` maps directly onto the
runtime API — no intermediate AST. Recognized methods and their
lowering: `push(x)` → `plant_list_push(obj, x)`, `pop()` →
`plant_list_pop(obj)`, `get(k)` → `plant_map_get(obj, k)`,
`put(k, v)` → `plant_map_set(obj, k, v)`, `has(k)` →
`plant_map_has(obj, k)` (parser-wrapped in `_from_long(...)` so it
prints as text; in arithmetic it coerces numerically). Calls work in
any expression position and chain with field access:

```
CREATE m(LIST) TO { "name": "root", "count": 7 }.
SHOW m.get("name").             # → root          (plant_map_get(m, "name"))
SHOW m.has("name").             # → 1             (plant_map_has)
SHOW m.get("count") + 1.        # → 8             (numeric coercion)
m.put("extra", "9").            # bare mutation statement
SHOW m.get("extra").            # → 9
CREATE l(LIST) TO ["a", "b", "c"].
l.push("d").                    # → l is [a b c d]
SHOW l.pop().                   # → d
SHOW l.push("x").pop().         # → x             (chained method-method)
SHOW nested.get("pt").get("y"). # → 9             (nested lookups)
SHOW m.get("x").name.           # → field into a method result
```

The receiver is chosen like field access: a trailing bare `IDENT .`
binds to that IDENT (`"x=" + m.push(v)` → `_cat("x=", plant_list_push(m, v))`);
other tails take the whole collected expression. `get`/`put`/`has` work
on map-backed lists (key/value pairs via `plant_list_make` — the count
is the element count, so two pairs are `plant_list_make(4, ...)`);
`push`/`pop` work on plain lists. Unknown methods are rejected at
parse time. Statement separation is line-aware: a chain never opens a
new line, so `SHOW m.name.` followed by `m.put(...)` on the next line
stays two statements (lexer marks line-leading tokens).

### Actions (functions)

```
ACTION add(a(NUM), b(NUM)),
  GIVE a + b.
/GIVE add.

ACTION greet(name(TX)) -> TX,
  GIVE "hello, " + name.
/GIVE greet.
```

- Typed parameters (`NUM`, `SCL`, `TX`, `FACT`, `LIST[T]`, structs, enums).
- Optional `-> Type` return annotation (purely informative at this stage).
- `GIVE expr.` returns; bodies may use `IF`/`SEASON`, recursion, closures.
- `REAP target FROM action, args.` calls an action and binds the result.
- `REAP _ FROM action, args.` discards the result (void calls).
- An `ACTION` may also be declared in "expression-call" form directly in a
  statement: `plant_map_set(user, "name", "Haider").`

**Generics** (v0.48.1): actions may carry type parameters, monomorphized
per call site:

```
ACTION echo[T](v(T)),
  GIVE v.
/GIVE echo.

ACTION max2[T](a(T), b(T)),
  IF a > b,
    GIVE a.
  /IF.
  GIVE b.
/GIVE max2.

ACTION main(),
  REAP a FROM echo[TX], "hi".
  REAP m FROM max2[NUM], 9, 4.
  CREATE mn(NUM) TO 0.
  SET mn TO m.           # numeric generic result → NUM var
  REAP ms FROM _from_long, mn.
  SHOW "max=" + ms.       # → max=9
  GIVE 0.
/GIVE main.
```

Each instantiation (e.g. `echo[TX]`, `max2[NUM]`) emits a unique native C
function (`plant_echo_TX`, `plant_max2_NUM`) — zero runtime overhead.
Numeric generic results come back as raw integers: assign them to a `NUM`
(like `max2` above) and convert with `_from_long` before printing. Once the
value is in a `NUM` variable, `SHOW` prints it directly (it is value-aware).

### Calling & Return Values

```
ACTION square(n(NUM)),
  GIVE n * n.
/GIVE square.

ACTION main(),
  CREATE x(NUM) TO 7.
  REAP s FROM square, x.
  CREATE sn(NUM) TO 0.
  SET sn TO s.           # numeric result → NUM var
  REAP ss FROM _from_long, sn.
  SHOW ss.               # → 49
  REAP _ FROM square, 2.          # ignore the result
  GIVE 0.
/GIVE main.
```

> Numeric results: a `GIVE` of a `NUM` comes back from the runtime as a raw
> integer, so copy it into a `NUM` variable (`SET` + `_from_long`) before
> string operations — see the example above. Bare `SHOW` of numeric values
> (vars, arithmetic, `LEN`/`COUNT`) is value-aware (prints the number); the
> remaining case that needs the explicit pattern is a raw return held in a
> `TX`/implicit variable, where `SHOW r.` still reads it as a string pointer.

### REAP Expressions (v0.49.9)

`REAP` also ingests general expressions — translate-time builtins,
arithmetic, indexing, and literals — with no action involved:

```
ACTION main(),
  CREATE lst(LIST) TO plant_list_make(3, "aa", "bb", "cc").
  REAP f FROM FIND("abc", "b").       # builtin → "1"
  REAP j FROM JOIN(lst, "-").         # builtin → "aa-bb-cc"
  REAP s FROM SLICE("abcdef", 1, 3).  # builtin → "bc"
  REAP u FROM UPPER("AbC").           # builtin → "ABC"
  REAP a FROM ABS(-7).                # builtin → "7"
  REAP n FROM 2 + 3.                  # arithmetic → "5"
  REAP x FROM lst[0].                 # indexing → "aa"
  REAP c FROM COUNT lst.              # → "3"
  SHOW f + " " + j + " " + s + " " + u + " " + a + " " + n + " " + x + " " + c.
  GIVE 0.
/GIVE main.
```

The token after `FROM` decides the form: `IDENT ,` action calls, `IDENT :`
module calls, `IDENT [types]` generic calls, and `IDENT (...)` calls to
non-builtin actions keep the legacy forms; everything else — builtin names
(`FIND JOIN SLICE UPPER LOWER ABS ROUND LEN FIRST LAST ...`), arithmetic,
indexing, `COUNT`, and literals — is a general expression translated exactly
like a `SET`/`SHOW` value (so builtins work, e.g. `REAP f FROM FIND(t, s).`
previously emitted a raw `FIND(...)` C call and failed to link). Numeric
results are stored as text (`_from_long`), so concatenation and `SHOW` never
see raw integer bits — matching the behavior of numeric-action REAPs.

### Conditions

```
IF score GREATER THAN OR EQUAL 90,
  SHOW "A".
/IF.

IF score IS 0,
  SHOW "zero".
/IF.
```

Supported comparison keywords: `IS`, `ISNT`, `GREATER THAN`,
`GREATER THAN OR EQUAL`, `LESS THAN`, `LESS THAN OR EQUAL`, `>`, `<`.
Boolean composition: `AND`, `OR`, `NOT`. Constants `TRUE`, `FALSE`,
`NULL` (null). Note: the v0.48 self-hosted parser implements `IF` /
`/IF.` blocks; legacy `ORIF`/`ELSE` branches are not parsed by Chloroplast.

### SEASON (while loop)

```
CREATE count(NUM) TO 5.
SEASON count GREATER THAN 0,
  SHOW "count=" + count.
  SET count TO count - 1.
/SEASON.
```

- `BREAK.` (or `BREAK 0.`) exits the innermost `SEASON` immediately.
- `CONTINUE.` skips to the next iteration.
- Loops must be inside an `ACTION` body.

### CYCLE (numeric and collection loops)

**Collection iteration** — `CYCLE item IN list` visits every element:

```
CREATE lst(LIST) TO ["a", "b", "c"].
CYCLE item IN lst,
  SHOW item.
/CYCLE.
```

**Indexed collection iteration** — `CYCLE item, idx IN list` also binds
the 0-based position:

```
CYCLE item, idx IN lst,
  SHOW idx + ":" + item.
/CYCLE.
```

**Range iteration** — `CYCLE i FROM lo TO hi` counts lo..hi
inclusive (both bounds are expressions; the counter is a `NUM`):

```
CYCLE i FROM 1 TO 5,
  SHOW _from_long(i).
/CYCLE.
```

**Stepped range iteration** — `CYCLE i FROM lo TO hi STEP k` adds a
step value; any spacing around `STEP` is accepted (`STEP 2`,
`STEP  2`, the attached `STEP2`, and negative increments `STEP -2` /
`STEP-2`):

```
CYCLE i FROM 1 TO 9 STEP 2,   # 1 3 5 7 9
  SHOW _from_long(i).
/CYCLE.
CYCLE i FROM 5 TO 1 STEP -2,  # 5 3 1 (descending)
  SHOW _from_long(i).
/CYCLE.
```

- `STEP` defaults to 1; the step may be a literal, a constant
  expression (`STEP 1 + 1`), or a runtime variable. A statically-zero
  step (`STEP 0`, `STEP 2 - 2`) is a compile-time error, and a
  zero-valued runtime step iterates zero times instead of spinning.
- The bound operator follows the step sign: ascending ranges use
  `<=`, descending use `>=`, so a negative step counts down.
- `BREAK.` exits the innermost `CYCLE`; `CONTINUE.` skips to the next
  iteration (the index increment still runs).
- Loops must be inside an `ACTION` body.

### Lists

**List literals (v0.49.4):** `[e1, e2, ...]` declares and initializes a
list in place — integer literals and NUM-typed expressions are stored
as number strings, strings and variables pass through, and brackets
nest recursively. Element access uses `name[expr]`:

```
CREATE a(LIST) TO [1, 2, 3].              # [1, 2, 3]
CREATE b(LIST) TO ["x", ["y", "z"], "w"]. # nested lists
CREATE c(LIST) TO [].                     # empty → plant_list_make(0)
CREATE n(NUM) TO 10.
CREATE d(LIST) TO [n + 1, "var"].         # expressions + variables
SHOW JOIN(d, "-").                        # → 11-var
SHOW a[0].                                # → 1 (name[expr] → plant_list_get)
```

Runtime helpers also build and manipulate lists natively:

```
CREATE parts(LIST) TO plant_list_make(0).
PUT "first" INTO parts.
PUT "second" INTO parts.
REAP r FROM plant_list_get, parts, 1.
SHOW "second=" + r.

IF _at(parts, 0) IS "first",
  SHOW "first-ok".
/IF.
```

- `plant_list_make(count, ...)` — create a list (0..N initial items).
- `plant_list_get(list, i)` / `_at(list, i)` — element access.
- `PUT item INTO list.` — append.
- `COUNT list` — element count (e.g. `SEASON i < COUNT lst`).
- `_map_get(map, key)` — map lookup.
- Note: chained indexing (`b[1][0]`) and string concatenation inside a
  literal are not yet supported.

### List Built-ins (v0.49.15)

Eight list utilities are bare expression built-ins (batch 1) — no
module prefix required, usable in any value position (REAP, SHOW, SET)
and nested inside other calls. List arguments may be literals,
variables, or nested built-in results:

```
REAP r  FROM JOIN(REVERSE(["a", "b", "c"]), " ").   # -> "c b a"
REAP r2 FROM JOIN(RANGE(1, 5), " ").                # -> "1 2 3 4"  (half-open [a, b))
REAP r3 FROM JOIN(RANGE(-2, 2), " ").               # -> "-2 -1 0 1"
REAP s  FROM JOIN(SORT(["cherry", "apple", "b"]), " ").  # -> "apple b cherry"
REAP i  FROM INCLUDES([1, 2, 3], 2).                # -> 1  (element membership)
REAP x  FROM INDEX_OF(["a", "b", "c"], "b").        # -> 1  (first index; -1 absent)
REAP u  FROM JOIN(UNIQUE(["a", "b", "a"]), " ").    # -> "a b"  (first occurrence wins)
REAP a  FROM AVERAGE([1, 2, 3, 4]).                 # -> 2.5
REAP m  FROM MEDIAN([4, 1, 2, 3]).                  # -> 2.5  (even count: mean of middle pair)
REAP n  FROM AVERAGE(UNIQUE([1, 1, 2, 3])).         # -> 2    (nested)
```

Semantics notes:

- `RANGE(a, b)` produces the integers `[a, b)` — `b <= a` yields `[]`;
  negative bounds work (`RANGE(-2, 2)` → `-2 -1 0 1`).
- `REVERSE` and `INCLUDES` dispatch on the first argument's type:
  lists get element-wise reversal / membership; strings keep the
  string built-in behavior (`REVERSE("abc")` → `"cba"`, substring
  `INCLUDES`).
- `SORT(lst)` sorts ascending (string/lexicographic order) and returns
  the same list (in place) — the v0.48.29 `SORT lst [ASC|DESC]`
  statement remains available for explicit order control.
- `AVERAGE`/`MEDIAN` parse numeric elements (non-numeric elements and
  nested lists/maps are skipped); an empty list yields `"0"`. Averages
  can be fractional (`AVERAGE([1, 2])` → `1.5`).
- `INDEX_OF` returns the first matching index, or `-1` when absent
  (and for non-list inputs). `UNIQUE` keeps first occurrences in order.
- Counts/lengths are numeric literals or numeric helpers — pass numeric
  *values* from `TX` variables through the FFI forms if needed.

#### Advanced List Built-ins (v0.49.16)

Batch 2 completes the list toolkit — the full legacy `lists` (14)
surface is now expression built-ins:

```
REAP f  FROM JOIN(FLATTEN([["a", "b"], ["c"]]), " ").  # -> "a b c"
REAP f2 FROM JOIN(FLATTEN([[1, [2]]]), " ").           # -> "1 [2]"  (single-level)
REAP c  FROM JOIN(CHUNK([1, 2, 3, 4, 5], 2), " ").     # -> "[1, 2] [3, 4] [5]"
REAP z  FROM JOIN(ZIP(["a", "b"], [1, 2]), " ").       # -> "[a, 1] [b, 2]"
REAP z2 FROM JOIN(ZIP([1, 2, 3], ["x"]), " ").         # -> "[1, x]"  (truncated)
REAP g  FROM JOIN(FILTER_GT([1, 2, 3, 4], 2), " ").    # -> "3 4"
REAP l  FROM JOIN(FILTER_LT([1, 2, 3, 4], 3), " ").    # -> "1 2"
REAP n  FROM JOIN(SORT(FILTER_GT([5, 1, 4, 2], 2)), " ").  # -> "4 5"  (nested)
```

Semantics notes:

- `FLATTEN` unnests one level: direct sub-lists are spliced in, deeper
  nesting and maps pass through (`FLATTEN([[1, [2]]])` → `[1, [2]]`).
- `CHUNK(lst, n)` subdivides into sub-lists of at most `n` elements
  (`n < 1` or an empty input yields `[]`); `CHUNK([1,2,3], 5)` → one
  sub-list.
- `ZIP(a, b)` pairs elements element-wise into `[a_i, b_i]` lists,
  truncating to the shorter input; a non-list argument yields `[]`.
- `FILTER_GT`/`FILTER_LT` keep elements strictly greater/less than the
  numeric threshold; non-numeric elements are dropped (mirroring
  `AVERAGE`/`MEDIAN`). Thresholds and lengths are integer literals
  (`FILTER_GT(lst, 0)` works — literal `0` is supported).

### String Operations

Strings are immutable `TX` values; `+` concatenates. Concatenating a
number into a string works automatically (v0.48.3a): `"x=" + i` emits
`_cat("x=", _from_long(i))`. Pure-numeric `+` stays plain C arithmetic.

```
CREATE x(NUM) TO 41.
CREATE msg(TX) TO "n=" + x.
SHOW msg.            # → n=41
SHOW "len " + LEN(msg).     # → len 3
```

Runtime helpers: `_from_long(n)` (number → text), `_to_long(s)` (text →
number), `LEN(s)` (string length), `_cat(a, b)` (concat). Module-style
calls also exist: `strings:LENGTH`, `strings:REPLACE`, `strings:SPLIT`.

#### Bare String Built-ins (v0.49.13)

Six string utilities are first-class expression built-ins — no
`strings:` module prefix required. They accept string literals,
variables, and nested built-in calls in any value position (REAP, SHOW,
SET):

```
REAP i  FROM INCLUDES("hello world", "lo wo").   # → 1  (substring present)
REAP i2 FROM INCLUDES("hello", "xyz").           # → 0  (absent)
REAP s  FROM STARTS_WITH("hello", "he").         # → 1  (prefix match)
REAP s2 FROM STARTS_WITH("hello", "lo").         # → 0
REAP e  FROM ENDS_WITH("hello", "llo").          # → 1  (suffix match)
REAP e2 FROM ENDS_WITH("hello", "hel").          # → 0
REAP r  FROM REPEAT("ab", 3).                    # → "ababab"
REAP r2 FROM REPEAT("x", 0).                     # → ""   (count <= 0)
REAP p  FROM PAD("x", 5, ".").                   # → "x...."  (right-pad)
REAP p2 FROM PAD("hello", 3, ".").               # → "hello"  (at/over length)
REAP q  FROM PAD_LEFT("ab", 4, "-").             # → "--ab"   (left-pad)
SHOW PAD_LEFT(REPEAT("y", 2), 4, "_").           # → "__yy"   (nested)
```

All six return text: the three predicates (`INCLUDES`, `STARTS_WITH`,
`ENDS_WITH`) return `"1"`/`"0"`; `REPEAT` with `count <= 0` (or an empty
input) returns `""`; `PAD`/`PAD_LEFT` pass the input through unchanged
when it already meets the target length. Counts and lengths are numeric
literals or numeric helpers — pass numeric *values* through the
`strings:` module forms (`strings:REPEAT, s, n`) when the count comes
from a `TX`-typed variable. The `strings:` forms remain fully supported.

---

## Closures (v0.48.2)

Anonymous functions with explicit capture lists. `MOVE` copies a value
into the closure environment (the outer variable is cleared); `REF` tracks
a variable live via pointer so changes are visible inside.

```
ACTION counter(start(NUM)),          # e.g. counter(3)
  CREATE f TO [MOVE start](step(NUM)) -> step + start.
  REAP a FROM f, 5.
  REAP b FROM f, 5.
  SET a1 TO a.
  SET a2 TO b.
  REAP da FROM _from_long, a1.
  REAP db FROM _from_long, a2.
  SHOW "state=" + da + "," + db.      # e.g. → state=8,8 (env persists)
  SHOW "moved=" + _from_long(start).  # → moved=0 (MOVE cleared outer var)
/GIVE counter.

ACTION tracer(v(NUM)),                # called with 0
  CREATE t TO [REF v](d(NUM)) -> d + v.
  SET v TO 100.            # visible inside t via REF
  REAP r FROM t, 1.
  CREATE rn(NUM) TO 0.
  SET rn TO r.
  SHOW "ref=" + _from_long(rn).       # → ref=101
/GIVE tracer.
```

Numeric closure results and outer `NUM` variables are raw integers, so the
`SET`+`_from_long` conversion pattern applies here too (see the example).
`SHOW` of any numeric variable/expression itself is value-aware and prints
the number directly; the explicit conversion is only needed when converting
a raw return into a string context.

Block-form bodies run full statements and may nest closures. The closure
must declare a parameter list; the body runs between `( … )`:

```
ACTION main(),
  CREATE x(NUM) TO 5.
  CREATE outer TO [MOVE x](a(NUM)) -> (
    CREATE inner TO [MOVE a](b(NUM)) -> b + a + 1.
    REAP ri FROM inner, 10.
    SHOW "inner=" + _from_long(ri).
    GIVE ri
  ).
  REAP r FROM outer, 2.
  SHOW "outer=" + _from_long(r).
  GIVE 0.
/GIVE main.
```

Each closure lowers to a heap-allocated env struct (`plant_Env_N`) plus a
plain native function (`plant_Closure_N_fn`) — no runtime dispatch. Closures
are invocable anywhere in an ACTION body, including `SEASON`/`IF` bodies.

---

## FFI (Foreign Function Interface)

Declare native C functions and call them directly. An external is an
`ACTION` with no body whose return type is `external`:

```
ACTION ffi_add(a(NUM), b(NUM)) -> external.
ACTION ffi_swap_ref(a(REF NUM), b(REF NUM)) -> external.
ACTION ffi_open(mode(NUM)) -> Result<NUM, TX>.
```

- **Plain externals** — `ACTION name(args) -> external.` must be backed by
  a matching C function (declare it in `plant_compat.h` or link a library).
- **`REF` parameters** — passed by pointer; the call site emits `&var`
  automatically.
- **`Result<T, E>` returns** — the C ABI returns the value on success and a
  sentinel + `errno` on failure. Check with `ffi_last_error()` /
  `ffi_last_error_msg()` (`dlerror()` on loader failure, else
  `strerror(errno)`).
- **`ffi_free(ptr)`** — release `malloc`'d handles; `ffi_free(NULL)` sets
  `EINVAL` (guarded, safe).
- **Full signature space (v0.48.4)** — struct-by-value params and returns
  (`STRUCT X`, mapped via `plant_map_to_X` / `plant_X_to_map`), `REF STRUCT`
  (`plant_map_to_ref_X`), `void*` handles, varargs (`..., ...`), and
  `CALLBACK` parameters (auto-generated `plant_cbw_<name>` adapters +
  `plant_cb_ensure`).

Example (from `tests/native/ffi.plant`):

```
ACTION ffi_open_mock(mode(NUM)) -> Result<NUM, TX>.

ACTION main(),
  REAP h1 FROM ffi_open_mock, 0.      # "" on failure, errno set
  CREATE e(NUM) TO 0.
  REAP e FROM ffi_last_error.         # errno (2 = ENOENT)
  REAP m FROM ffi_last_error_msg.
  SHOW m.                             # "No such file or directory"
  REAP h2 FROM ffi_open_mock, 1.      # success → errno cleared
  REAP buf FROM ffi_make_buf, 100.
  REAP _ FROM ffi_free, buf.          # lifecycle
  GIVE 0.
/GIVE main.
```

The generated C ships a `/*__PLANT_TYPES_BEGIN__*/ … __END__` block with
topologically ordered struct typedefs and extension prototypes.

---

## Standard Library (v0.47.x+)

The core standard library ships natively in the runtime
(`runtime/c/plant_runtime.c` + `plant_compat.h`) — no imports, no
interpreter. Calls go through `REAP x FROM fn, args.` or bare expressions.

### std/json

```
REAP j FROM json_parse, "{\"name\": \"Alice\", \"age\": 30}".
IF j IS NULL,                        # invalid JSON → safe nil, no crash
  SHOW "bad json".
/IF.
REAP nm FROM json_get, j, "name".
SHOW json_val(nm).                   # → Alice
REAP out FROM json_stringify, j.
SHOW out.                            # → {"name":"Alice","age":30}
REAP tags FROM json_get, j, "tags".
CREATE tl(NUM) TO json_len(tags).    # array/object element count
REAP t0 FROM json_at, tags, 0.
SHOW json_val(t0).                   # first element
```

### std/string

```
REAP r1 FROM string_repeat, "ab", 3.     # → "ababab"
REAP r2 FROM string_reverse, "abc".      # → "cba"
REAP r3 FROM string_pad, "x", 5, ".".    # → "x...."
REAP r  FROM strings:LENGTH, "abcd".     # → 4
REAP s  FROM strings:REPLACE, "a-b-c", "-", "+".   # → "a+b+c"
```

### std/fs

```
REAP w FROM fs_WRITE, "/tmp/f.txt", "hello fs".     # "1" ok
REAP e FROM fs_EXISTS, "/tmp/f.txt".                # "1" / "0"
REAP c FROM file_copy, "a.txt", "b.txt".            # "1" ok
REAP m FROM file_move, "b.txt", "c.txt".            # "1" ok
REAP st FROM file_stat, "c.txt".                    # MAP: size/mtime/mode
REAP sz FROM _map_get, st, "size".
```

### std/math

```
REAP s FROM math_sqrt, "16".       # → "4"
REAP p FROM math_pow, "2", "10".   # → "1024"
REAP f FROM math_floor, "3.7".     # → 3
REAP c FROM math_ceil, "3.2".      # → 4
REAP r FROM math_round, "2.5".     # → 3
REAP si FROM math_sin, "0".        # → 0
REAP mn FROM math_min, "3", "7".   # → 3
REAP mx FROM math_max, "3", "7".   # → 7
REAP rd FROM math_random.          # uniform [0,1) as text
```

### Extended Math Library (v0.49.17)

17 new math built-ins as bare expression built-ins and FFI module
bindings — all dispatch through `_plant_math_num` for safe numeric
coercion (tagged small ints, numeric strings, `_from_long` NUM vars).
Results render as a long integer when integral, otherwise "%.10g"
("nan" / "-nan" for domain errors).

```
# Bounds / combinators
SHOW MIN(3, 5).                     # → 3
SHOW MAX(3, 5).                     # → 5

# Trigonometric & inverse
SHOW TAN(0).                        # → 0
SHOW ATAN(0).                       # → 0
SHOW COT(0.5).                      # → 1.83048772172
SHOW ASIN(0).                       # → 0
SHOW ASIN(2).                       # → -nan  (out of range)
SHOW ACOS(0.5).                     # → 1.0471975512
SHOW ATAN2(1, 1).                   # → 0.785398163397

# Hyperbolic
SHOW SINH(0).                       # → 0
SHOW COSH(1).                       # → 1.54308063482
SHOW TANH(1).                       # → 0.76159415596

# Exponential & logarithmic
SHOW EXP(0).                        # → 1
SHOW EXPM1(0).                      # → 0
SHOW LOG10(10).                     # → 1
SHOW LOG2(8).                       # → 3
SHOW LOG1P(0).                      # → 0

# Pythagorean
SHOW HYPOT(3, 4).                   # → 5
```

All 17 are also available via the `math:` FFI module
(`REAP r FROM math:TAN, 3.`) and support REAP ingestion
(`REAP r FROM MIN(1, 2).`). Numeric literals arrive as tagged small
ints and are decoded safely — `MIN(3, 5)` with integer literals no
longer segfaults (the legacy `math_min`/`math_max` helpers were patched
in v0.49.17 to use `_plant_math_num`).
### Advanced Math Library (v0.49.18)

11 further built-ins complete the math subsystem — same coercion and
rendering rules as the extended library; domain violations report
`-nan`.

```
# Reciprocal trig
SHOW SEC(0).                         # → 1   (1/cos)
SHOW CSC(1).                         # → 1.188395106

# Inverse hyperbolics (domain-guarded)
SHOW ASINH(1).                       # → 0.881373587
SHOW ACOSH(2).                       # → 1.316957897
SHOW ACOSH(0.5).                     # → -nan (x >= 1 required)
SHOW ATANH(0.5).                     # → 0.5493061443
SHOW ATANH(1).                       # → -nan (|x| < 1 required)

# Special functions
SHOW ERF(1).                         # → 0.8427007929
SHOW ERFC(1).                        # → 0.1572992071
SHOW GAMMA(5).                       # → 24  (tgamma)
SHOW LGAMMA(5).                      # → 3.17805383

# Utilities
SHOW EXP2(3).                        # → 8   (2^x)
SHOW LOG_BASE(8, 2).                 # → 3   (x > 0, b > 0, b != 1)
SHOW LOG_BASE(8, 1).                 # → -nan

# Nesting works anywhere an expression does
SHOW EXP2(LOG_BASE(8, 2)).           # → 8
```
All of these also accept decimal literals directly (`ROUND(3.7)`,
`POW(2.5, 2)`), and since v0.49.19 the same is true for the older
built-ins (`ABS ROUND POW CEIL FLOOR SIN COS SQRT`). `LOG(x)` is the
natural-log built-in (`LOG(10)` → 2.302585093; x <= 0 yields the
"ERR:" diagnostic). Every math function is additionally reachable
through the `math:` module form (`REAP s FROM math:SIN, "0.5".`).

### Single Quotes, Ranges, Option/Result (v0.49.20)

Single-quoted strings are interchangeable with double quotes
(`'it\'s'` needs the `\'` escape; no `${...}` interpolation inside
single quotes). Infix ranges are shorthand for the RANGE built-in:

```
SHOW 'hello'.                  # same STRING token as "hello"
SHOW JOIN(1..4, ",").          # 1,2,3   (== JOIN(RANGE(1, 4), ","))
SHOW JOIN(-2..2, " ").         # -2 -1 0 1

REAP t FROM Option_Some('v7'). # monadic wrappers over the runtime
REAP u FROM plant_unwrap, t.   # tagged unions (v0.44.0 helpers)
SHOW u.                        # v7
REAP e FROM Result_Err('boom').
REAP ue FROM plant_unwrap_err, e.
SHOW ue.                       # boom
CREATE n(NUM) TO plant_is_some(t).   # predicates are numeric
```

Variants use underscore compounds (`Option_Some`, `Option_None`,
`Result_Ok`, `Result_Err`) so they never collide with dot-notation
field access.

### Statistical Aggregations (v0.49.21)

```
CREATE nums(LIST) TO [1, 2, 3, 4].
SHOW VARIANCE(nums).           # 1.25     (population)
SHOW STDDEV(nums).             # 1.118033989
SHOW PRODUCT(nums).            # 24
SHOW MIN(nums).                # 1        (single arg = list form)
SHOW MAX(nums).                # 4
SHOW RANGE(nums).              # 3        (max - min spread)
SHOW MODE([7, 2, 7, 9, 7]).    # 7

SHOW MIN(3, 5).                # 3        (two args = scalar form)
SHOW JOIN(RANGE(2, 6), ",").   # 2,3,4,5  (scalar form builds lists)
```

Empty inputs default to `"0"` (product `"1"`, mode `""`);
non-parsable elements are filtered before aggregating.

### Matrix & Linear Algebra (v0.49.22)

Matrices are nested lists (LIST of LIST); vectors are flat lists.

```
SHOW DOT([2, 3], [4, 1]).              # 11
SHOW NORM([3, 4]).                     # 5
SHOW CROSS([1, 0, 0], [0, 1, 0]).      # [0, 0, 1]
CREATE M(LIST) TO [[1, 2], [3, 4]].
SHOW JOIN(FLATTEN(TRANSPOSE(M)), ","). # 1,3,2,4
SHOW DET(M).                           # -2
SHOW JOIN(FLATTEN(INVERSE(M)), ",").   # -2,1,1.5,-0.5
SHOW JOIN(FLATTEN(MATRIX_MULT(M, M)), ",").  # 7,10,15,22
```

Dimension violations (ragged rows, mismatched dot lengths, cross on
non-3-vectors, MATRIX_MULT inner-dimension mismatch, non-square
DET/INVERSE input, singular inverse) return the string "ERR";
determinants of singular matrices return "0".

### Numerical Analysis (v0.49.23)

```
CREATE A(LIST) TO [[4, 3], [6, 3]].
CREATE f(LIST) TO LU(A).               # [L, U]; PA = LU (row-pivoted)
SHOW JOIN(FLATTEN(f[0]), ",").         # L = 1,0,0.6666666667,1
CREATE S(LIST) TO [[2, 1], [1, 2]].
CREATE eg(LIST) TO EIGEN(S).           # symmetric only; values desc
SHOW JOIN(eg[0], ",").                 # 3,1
CREATE sd(LIST) TO SVD([[3, 0], [0, 2]]).
SHOW JOIN(sd[1], ",").                 # singular values: 3,2
SHOW JOIN(SOLVE([[2, 1], [1, 3]], [5, 10]), ",").   # 1,3
SHOW COND([[2, 0], [0, 2]]).           # 2
```

EIGEN accepts symmetric matrices (Jacobi rotations); SVD decomposes
any shape via A^T A; SOLVE rejects singular systems ("ERR").

### std/time

```
REAP t  FROM time_now.                      # epoch seconds
REAP d  FROM time_format, t, "%Y-%m-%d".    # → "2026-08-01"
REAP t2 FROM time_parse, "2026-08-01 12:00:00", "%Y-%m-%d %H:%M:%S".
REAP ok FROM time_sleep, "0.05".            # fractional seconds
```

### Set / Queue / Stack (v0.47.2)

```plantlang
# Set — unique collection (identity-based uniqueness; 0/NULL reserved as nil)
REAP s FROM set_create.
REAP r FROM set_add, s, 10.          # "1" added
REAP r FROM set_add, s, 10.          # "0" duplicate
REAP r FROM set_has, s, 10.          # "1" present
REAP r FROM set_remove, s, 10.       # "1" removed
CREATE n(NUM) TO set_size(s).        # unique element count
REAP lst FROM set_to_list, s.        # → LIST for iteration/export

# Queue — FIFO ring buffer
REAP q FROM queue_create.
REAP _ FROM queue_push, q, "first".
REAP _ FROM queue_push, q, "second".
REAP v FROM queue_pop, q.            # → "first"
REAP v FROM queue_peek, q.           # → "second" (front, kept)
CREATE n(NUM) TO queue_size(q).      # item count

# Stack — LIFO dynamic array
REAP st FROM stack_create.
REAP _ FROM stack_push, st, "bottom".
REAP _ FROM stack_push, st, "top".
REAP v FROM stack_peek, st.          # → "top"
REAP v FROM stack_pop, st.           # → "top"
REAP v FROM stack_pop, st.           # → "bottom"
```

Empty `pop`/`peek` on a queue or stack return the empty string — never a
crash. Stress workloads (thousands of inserts/lookups/deletes) are covered
by the `std_set` / `std_queue` / `std_stack` native test suites.

---

## Async Engine (v0.48.3+)

`ASYNC ACTION` declares a cooperative, single-threaded async action. It
compiles to a C state machine (no threads, no locks) with suspension and
resume across awaits:

```
ASYNC ACTION phase2(tag(TX)),
  GIVE "p2-" + tag.
/GIVE phase2.

ASYNC ACTION worker(tag(TX), n(NUM)),
  CREATE i(NUM) TO 0.
  CREATE sum(NUM) TO 0.
  SEASON i < n,
    SET sum TO sum + i.
    SET i TO i + 1.
  /SEASON.
  AWAIT phase2, tag.
  GIVE sum.
/GIVE worker.

ACTION main(),
  CREATE i(NUM) TO 0.
  SEASON i < 20,
    START worker, "w" + i, 1000.
    SET i TO i + 1.
  /SEASON.
  GIVE 0.
/GIVE main.
```

Async statements:

| Statement | Meaning |
|---|---|
| `AWAIT action, args.` | suspend current task until `action` completes |
| `START action, args.` | fire-and-forget spawn of an async action |
| `START action, args IN ctx.` | spawn into a named context |
| `ASYNC IN ctx, action, args.` | spawn into a structured context |
| `CANCEL value.` | cancel a task token or an entire context |
| `TRACE LEVEL msg.` | emit a scoped trace event (level: INFO/DEBUG/PERF) |

- `PRIORITY HIGH \| NORMAL \| LOW` may follow the `-> Type` annotation of an
  async action (default NORMAL).
- A top-level `ACTION main` that spawns async work automatically ends with
  `plant_async_drain()` so every worker completes before the program exits
  (v0.48.3a).
- `MISSION CONFIG` directives tune the engine at runtime.

### MISSION CONFIG (runtime directives)

`MISSION CONFIG` directives configure the async engine at program startup.
They compile to `plant_async_config("<key>", "<value>")` calls and are
emitted before any other top-level statements.

```
MISSION CONFIG ADAPTIVE_THRESHOLD = 1000.
MISSION CONFIG SAMPLING_MODE = CPU.
MISSION CONFIG TRACE_LEVEL = DEBUG.
MISSION CONFIG METRICS = ON.
MISSION CONFIG TRACE = ON.
MISSION CONFIG TRACE_FILE = trace.log.

SHOW "starting".
```

Supported keys: `ADAPTIVE_THRESHOLD` (queue threshold, ≥1),
`SAMPLING_MODE` (`CPU`/other), `TRACE_LEVEL` (`DEBUG`/`PERF`/off),
`METRICS` (`ON`/`OFF`), `TRACE` (`ON`/`OFF`) and `TRACE_FILE` (path or
`OFF`). Environment: `PLANT_TRACE=1` enables trace output;
`PLANT_TRACE_FILE` writes it to a named file.

> Note: a program mixes `MISSION CONFIG` with either an `ACTION main`
> **or** bare top-level statements — the driver emits a single `main`,
> so defining `ACTION main()` alongside `MISSION CONFIG` would produce a
> duplicate `main` at link time. Keep config directives in statement-based
> programs.

---

## Networking (v0.48.32+)

The native runtime speaks HTTP/1.1 over POSIX sockets: **HARVEST** is
the client (v0.48.32, MAP mode v0.48.34, formalized v0.49.0) and
**LISTEN** is the server (v0.48.33, timeout option v0.49.2). There is no
TLS — `https://` URLs parse to port 443 but the socket stays plaintext.

### HTTP Client — HARVEST

```
HARVEST "http://127.0.0.1:41234/get" AS res.
REAP ok     FROM _map_get, res, "ok".       # "TRUE" / "FALSE"
REAP status FROM _map_get, res, "status".   # "200", "0" on failure
REAP body   FROM _map_get, res, "body".
REAP hd     FROM _map_get, res, "headers".  # response header MAP
```

Optional modifiers (any order, commas allowed):

```
HARVEST "http://127.0.0.1:41234/post" AS r
        METHOD POST BODY "hello=world" HEADERS h TIMEOUT 5.
```

- `METHOD m` — defaults to `GET`; `BODY b` — request payload (sent on
  POST); `HEADERS h` — a MAP built with `LINK "Name" WITH "value" IN h`;
  `TIMEOUT t` — seconds, `0`/absent means 5 s.
- `MAP` mode keeps the connection alive and adds a `sock` key: the
  descriptor as a decimal string for `plant_net_read`, `plant_net_write`,
  and `plant_net_close` (buffered read, send-all, idempotent close).
- `JSON` mode (v0.49.3) parses the response body into a `PlantJson`
  structure in `resp["body"]` — nested access via `json_get`, array
  elements via `json_at`, scalar leaves via `json_val`:

```
HARVEST "http://127.0.0.1:41234/json" AS r JSON.
REAP body FROM _map_get, r, "body".
REAP key FROM json_get, body, "key".
SHOW json_val(key).                    # → value
REAP arr FROM json_get, body, "list".
REAP el0 FROM json_at, arr, 0.         # → "1"
REAP out FROM json_stringify, body.    # canonical JSON text back
```

  A body that is not valid JSON becomes the empty string (falsy). The
  plain form keeps the raw text body.

### HTTP Server — LISTEN

```
LISTEN ON 41235 AS req.
REAP method FROM _map_get, req, "method".   # "GET", "POST", ...
REAP path   FROM _map_get, req, "path".     # "/hello?q=1"
REAP body   FROM _map_get, req, "body".     # per Content-Length
REAP hd     FROM _map_get, req, "headers".  # request header MAP
GIVE "Hello from Chloroplast" AS RESPONSE.
```

- `LISTEN ON <port> AS <req>.` blocks for ONE client connection, parses
  the request into a MAP with `ok`/`method`/`path`/`headers`/`body`/`sock`
  keys, then closes the listener (single-request server).
- `LISTEN ON <port> AS <req> TIMEOUT <t>.` (v0.49.2) gives up on the
  accept after `t` seconds and returns a MAP with `ok = "FALSE"`.
- `GIVE <body> AS RESPONSE.` replies to the most recent `LISTEN` in the
  same block with `HTTP/1.1 200 OK` + `Content-Length`, then closes the
  connection. Without a bound request (no LISTEN in scope) it is a safe
  no-op; bind failure and malformed requests surface as `ok = "FALSE"`.
- `GIVE <body> AS RESPONSE JSON.` (v0.49.3) serializes the body with
  `json_stringify` and replies with `Content-Type: application/json`.
  The body may be a pair-list MAP (→ JSON object), a plain LIST
  (odd element count → JSON array), or a `PlantJson` from `json_parse`;
  scalar values follow JSON rules (true/false/numbers raw, strings
  quoted). The non-JSON form stays `text/plain`.
- The regression suite drives these one-shot servers with
  `tests/regression/listen_client.py`.

---

## Self-Hosting & Build

The compiler is a single pipeline written in PlantLang itself:

```
Source (.plant)
   ↓  src/plantc/lexer.plant       — tokenizer
   ↓  src/plantc/parser.plant      — recursive-descent parser → AST (LISTS)
   ↓  src/plantc/codegen_c.plant   — C code generator
   ↓  bin/Chloroplast              — CLI driver (--help / --version / compile)
   ↓  runtime/c/plant_runtime.c    — native runtime (lists, json, fs, async …)
   ↓  gcc
Native executable
```

Bootstrapping: `dist/Chloroplast` (v1) compiles the sources to `v2`, which
compiles them again to `v3`, and so on; `make self` verifies the
generations are byte-identical (fixed point). The Makefile also runs the
native, generics, and closures integration suites (`make test`) and the
benchmark suite (`make perf`, results in `perf_results.md`).

### File Extensions

| Extension | Meaning |
|---|---|
| `.plant` | modern native source (this tour) |
| `.plnt` | legacy pre-v0.46 source (not parsed by Chloroplast) |

---

## License

MIT — see the repository root. Chloroplast is self-hosted, pure native,
and MIT licensed.


### Brace-form Action Bodies (v0.49.24)

Actions may use curly-brace blocks instead of the comma/`/ACTION.`
form; both coexist freely:

```
ACTION main() {
  SHOW "hello".
  SHOW double(21).
  GIVE 0.
}

ACTION double(n(NUM)) -> NUM {
  GIVE n * 2.
}

ACTION noop() {
}
```

Inner statements keep the standard forms (`IF cond,` ... `/IF.`);
empty blocks return ""; numeric returns need `-> NUM` as usual.


### File Imports (v0.49.25)

```
IMPORT "lib/helpers.plnt".   # relative to this file
IMPORT "/abs/path/mod.plant".
IMPORT "stdlib/stringutil".  # .plant appended when missing

# then use imported ACTIONs directly
```

Each file is expanded exactly once per compilation; circular imports
abort with "Error: import cycle detected: <path>", unreadable targets
with "Error: cannot read import: <path>". The compiler's own sources
keep using build-time concatenation.


### LET Destructuring (v0.49.26)

```
CREATE m(LIST) TO { "x": 10, "y": 20 }.
LET {x, y} = m.            # binds x=10, y=20
LET {x: rx} = m.           # rename: key x -> var rx
CREATE l(LIST) TO [7, 8, 9].
LET [a, b, c] = l.         # positional
LET [_, s] = l.            # "_" skips a position
LET {pt: {px, py}} = nested.   # nested patterns recurse
LET [[d1, d2], [d3, d4]] = deep.
```

Objects extract via map lookups, lists via indices; "_" ignores a
binding; nesting is unlimited.


### TYPE Aliases (v0.49.27)

```
TYPE MyInt = NUM.
TYPE Name = TX.
TYPE IntList = LIST[NUM].
TYPE SameList = IntList.      # chains resolve transitively

CREATE n(MyInt) TO 7.         # behaves exactly like NUM
CREATE xs(IntList) TO [1, 2].
```

Aliases may be declared at file scope or inside ACTION bodies; they
lower to C typedefs and resolve in CREATE/LET type positions.


### SPECIES + BLOOM (v0.49.28)

```
SPECIES Person {
  name: TX
  age: NUM
}

BLOOM Person AS p.
SHOW "[" + p.name + "]".      # [] - fields default to ""
CALL p.put("name", "Ada").
SHOW p.name.                  # Ada
```

Species are map-backed records; fields default to "" and work with
the existing obj.field selector and .put(...) method.


### Species Methods + SELF (v0.49.29)

```
SPECIES Person {
  name: TX
  age: NUM

  ACTION greet() -> TX {
    GIVE "Hello, " + self.name + "!".
  }
  ACTION haveBirthday() {
    CALL self.put("age", "37").
  }
}

BLOOM Person AS p.
CALL p.put("name", "Ada").
SHOW p.greet().            # Hello, Ada!
CALL p.haveBirthday().
SHOW p.age.                # 37
```

Methods lift to functions whose first parameter is the instance;
`SELF` (case-exact) inside bodies becomes that parameter. Use the dot
form for field/method access. Method names are global-unique.


### Species Inheritance (v0.49.30)

```
SPECIES Animal {
  name: TX
  ACTION speak() -> TX { GIVE self.name + " makes a sound". }
}
SPECIES Dog FROM Animal {
  breed: TX
  ACTION fetch() -> TX { GIVE self.name + " fetches!". }
}

BLOOM Dog AS d.
CALL d.put("name", "Rex").
SHOW d.speak().     # inherited from Animal
SHOW d.fetch().     # Dog's own method
```

Child fields merge with parent fields at creation time.


### Species Inheritance + Methods + SELF (v0.49.29-31)

```
SPECIES Animal { name: TX }
SPECIES Dog FROM Animal {
  breed: TX
}
ACTION speak(self(TX)) -> TX {
  GIVE self.name + " speaks".
}

BLOOM Dog AS d.
CALL d.put("name", "Rex").
SHOW COUNT d.       # 2 (name + breed merged)
SHOW speak(d).      # Rex speaks
```

Methods can also be embedded in SPECIES bodies; they lift to
top-level functions with an implicit self first parameter.
