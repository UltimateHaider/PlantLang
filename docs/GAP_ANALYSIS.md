# Legacy JS Interpreter vs Self-Hosted Compiler — Gap Analysis Report

**Scope:** Legacy JavaScript interpreter (PlantLang ≤ v0.45.x, `core/interpreter.js` + associated modules, recovered from git at `7f54eae` v0.45.0) vs the self-hosted native compiler (v0.49.10, `src/plantc/*.plant` → C, `runtime/c/plant_runtime.c`).

> **Status note (v0.49.10):** the legacy side no longer exists in the
> working tree. v0.48.38m removed `core/`, `src/**/*.js`, `service/`,
> `webrepl/`, `std/`, `benchmarks/`, and the editor assets from the
> repository; the legacy engine is now preserved only in git history
> (last shipped state: v0.45.0, commit `7f54eae`). All legacy-side
> line counts and behavior cited below are historical.

---

## 1. Executive Summary

PlantLang transitioned from a **tree-walking JavaScript interpreter** (~9,000 lines of `core/*.js` + `src/interpreter/*` + `std/*.plnt`) to a **self-hosted native compiler** (`src/plantc/*.plant` ≈ 6,700 lines of PlantLang that compiles PlantLang to C, linked against a ~3,500-line C runtime). The compiler achieved full self-hosting convergence (v0.48.19, 257,107 bytes) and ships a richer mission-mode runtime (FAST/SAFE/SMART/PERSISTENT with audit trails, capability checks, an async engine and a reference-counted ARC heap) than the interpreter ever had.

The transition is a **re-implementation with deliberate scope cuts**, not a 1:1 port. High-level counts:

| Area | Legacy (v0.45.0) | Current (v0.49.10) | Gap |
|---|---|---|---|---|
| Statement keywords (parser dispatch) | ~40 | ~40 (incl. storms, CYCLE forms, NOW/ANALYZE/TYPEOF, FREE/ARC/FAST, WAIT/LOCK) | ~10 missing (deliberate cuts + legacy stubs) |
| Innate/std library functions | ~60 (41 innate + 19 std/.plnt + 5 FFI stubs) | ~31 expression builtins (codegen_c.plant:2238-2292) + ~22 FFI module bindings (strings/fs/math) + ~50 declared runtime helpers | ~20 missing |
| Runtime features | ~90 capabilities | ~70 capabilities | roughly balanced, different sets |
| Network | HTTP client + server (HARVEST/LISTEN) | HARVEST client (v0.48.32, MAP-mode v0.48.34, formalized v0.49.0) + LISTEN server (v0.48.33) | low — no TLS (https: parses to port 443 but sends plaintext) |
| Error handling | Storm exceptions (WEATHER/SHELTER, 12 storm types) | 13 kinds (v0.48.25: +STOP_STORM), STOP IF, plant_calm finalization, `storm()` factory (v0.48.38a), location backfill (v0.48.38b) | low |
| Object model | SPECIES classes, BLOOM instantiation, SELF methods, inheritance | none | high |

**Key architectural differences that explain the gaps:**

1. **Dynamic vs static typing.** The interpreter evaluated compound expressions by string-substitution into `new Function()` (arbitrary JavaScript accepted), used JS `Map`/`Array` for aggregates, and allowed runtime type coercion (`CONVERT`, `inferType`, `coerce`). The compiler is statically typed: every expression is rewritten into C with explicit `long`/`tx_t`/struct types. Anything that depended on runtime dynamics (type inspection, dynamic conversions, untyped aggregates, method dispatch) could not survive without a type system the compiler does not yet have.
2. **Interpreted scope chain vs compiled C scopes.** The interpreter resolved variables through a runtime `Soil` scope chain with locks, pulses and storm-watching. Compiled code uses static C scopes; scope-dynamic features (legacy scope locks / EVAPORATE, PULSE watchers, `WHENEVER`, `ROOT`/`ROOT_SCOPE` globals) have no compilation target. (The v0.48.37e `LOCK var.` synchronization statement is a new value-keyed runtime Lock Table, distinct from the legacy per-variable scope lock.)
3. **Host capabilities vs portable C runtime.** The interpreter leaned on Node.js (`fs`, `http`, `worker_threads`, `Atomics`, locale formatting, ANSI diagnostics). The C runtime re-implements only a thin slice (stdio via `runtime_bridge.c`, POSIX file ops, and — since v0.48.32 — POSIX sockets wired as HTTP: HARVEST client with MAP/JSON modes, LISTEN server with TIMEOUT/JSON responses). Worker-thread fan-out, TLS, locale-aware time formatting and terminal diagnostics were lost with Node.
4. **Regex + AST dual pipeline vs single AST pipeline.** The interpreter had a legacy regex execution path with many one-off forms (`CONVERT`, `FLOW`, `MATCH … YIELD`, `NOTE`, `STEADY`) that were never migrated into its own AST path; those forms are effectively dead even in the legacy engine and were simply not carried over. (Exception: `PICK` was re-implemented as a typed built-in in v0.48.38h rather than ported from the regex path.)
5. **Interpreter-added features post-v0.45 are intentionally dropped.** The prompt excludes intentionally deprecated features; `SPECIES`/`BLOOM`, VEIN file IO and the JS `Function()` escape hatch were explicitly superseded (the Language Tour marks `core/*.js` as "historical only").

---

## 2. Methodology & Sources

- **Legacy side** (recovered from git `7f54eae`, the last commit shipping `core/interpreter.js`; removed from the working tree in v0.48.38m — all sources below are historical):
  `core/interpreter.js` (2,828 L), `core/parser.js` (3,033 L), `core/ast.js` (1,040 L), `core/typechecker.js` (1,529 L), `core/tokenizer.js` (259 L), `core/evaluator.js` (173 L), `core/runtime.js` (64 L), `core/innate.js` (67 L), `core/dispatcher.js` (286 L), `core/matrix.js`, `core/harvest.js` + `harvest_worker.js`, `core/diagnostics.js`, `core/lexer.js`, `core/runtime_bridge.c`, `src/interpreter/{cycle_evaluator,sort_evaluator,bloom_evaluator,show_formatter}.js`, `std/{prelude,string,math,io}.plnt`.
- **Current side** (working tree v0.49.10): `src/plantc/{lexer,parser,codegen_c,main}.plant`, `runtime/c/plant_runtime.c` (≈6,000 L), `runtime/c/plant_compat.h` (787 L), `runtime/c/plant_runtime.h`, `tests/native/mock_ffi.{h,c}`, `Language Tour.md`.
- **Verification method:** full keyword-dispatch enumeration of both parsers; complete function inventories of `plant_compat.h` and `plant_runtime.c`; spot-checks for every "missing" claim (all negative).

Legend for gap tables: **S** = supported, **P** = partial (different semantics / only reachable internally), **M** = missing, **D** = intentionally deprecated / dropped by design.

---

## 3. Language Constructs — Legacy vs Current

### 3.1 Declarations & types

| Legacy construct | Legacy status | Current | Notes |
|---|---|---|---|
| `ACTION name(a(TYPE), …)` | S | **S** | same core form |
| `ACTION … { body }` (brace form) | S | M | parser only accepts comma-opened bodies |
| `ACTION … -> external.` / `NATIVE ACTION` | S | **S** | externals via compat.h declarations |
| `ACTION (self(Type)) method` receiver binding | S | M | no method model |
| `ACTION … WITH MISSION <mode>.` | S | **S** | FAST/SAFE/SMART/PERSISTENT/BALANCED |
| `ACTION` generics `<T>`, prio, `Result<T,E>` ret | M (legacy had none) | **S** | new in v0.48.1+ |
| `SPECIES name {f: T}` + `BLOOM` | S | **P** | v0.49.28: map-backed records (fields default "", obj.field read + .put write); no VAR syntax (D); ACTION members + SELF shipped v0.49.29; FROM inheritance shipped v0.49.30; ACTION members + SELF v0.49.31; polymorphic dispatch v0.49.32; INTERFACE declarations + runtime conformance + compile-time validation v0.49.39; IMPLEMENTS clause parsing enabled v0.49.37 (compile-time validation D) |
| `STRUCT` / `SHAPE` | S | **P** | STRUCT with typed fields only (map-backed); no SHAPE |
| `CHOICE Variant(TYPE)` + `Option`/`Result` | S | **P** | no CHOICE; `Option`/`Result`/`is_some` etc. exist only as runtime helpers |
| `ENUM` | S | **S** | members as `NAME.MEMBER`, `_to_enum`/`_from_enum` |
| `CONST`, `ROOT`, `ROOT_SCOPE` | S | **S** | v0.48.35 (block-scoped/global string constants; ROOT_SCOPE auto-elevates CONST to ROOT; values must be quoted literals) |
| `TYPE alias = target.` | S | **S** | v0.49.27: parse_type_decl + C typedef emission + CREATE/LET resolution (chained aliases, composite targets); valid at file scope and inside actions |
| `MISSION : mode.` (set mode) | S | **P** | `MISSION CONFIG KEY = VALUE.` only |
| `PLANT lib [AS alias].` | S | M | replaced by direct `module:func` calls |
| `IMPORT "path".` (user programs) | S | **S** | v0.49.25: runtime loader in plant_import_load (relative/absolute paths, .plant inference, dedup, cycle + missing-file errors); compiler self-host still uses build-time concat |
| `LET {a,b} = ...` / `LET [h,t] = ...` destructuring | S | **S** | v0.49.26: object/array/nested/wildcard patterns via let_destructure nodes (_map_get / plant_list_get lowering) |

### 3.2 Statements

| Legacy construct | Legacy status | Current | Notes |
|---|---|---|---|
| `CREATE` (typed, `TO`, empty) | S | **S** | no `PULSE` flag |
| `CREATE x(LIST) TO a, b, c.` comma init | S | M | aggregates built via `plant_list_make`; native alternative: `CREATE x(LIST) TO [a, b, c].` literal (v0.49.4) |
| `LET x = expr.` | S | **S** | |
| `SET t TO e.` | S | **S** | `SELF:prop`/`obj:field` member targets M |
| `INCREASE` / `DECREASE … BY` | S | **S** | `INCREASE x BY n.` / `DECREASE x BY n.` shipped in v0.48.22-patch (numeric targets only, compound C assignment) |
| `EVAPORATE` / `LOCK` / `EMPTY` | S | M | EMPTY was already a parse stub in legacy |
| `IF cond, body [ORIF][ELSE]` | S | **S** | Shipped in v0.48.20 (multi-branch chains) |
| `SEASON cond, body` | S | **S** | |
| `CYCLE x IN list` / `CYCLE i, idx IN` / `CYCLE i FROM lo TO hi` | S | **S** | All three CYCLE forms shipped in v0.48.22 (FROM/TO in v0.48.21); STEP is statically evaluated since v0.48.22-patch3 — zero steps are a compile-time error and expression steps pick direction-aware bounds. v0.49.8: STEP is a registered lexer keyword and the to-expression is collected token-wise (`collect_until_keyword`), replacing the `" STEP "` text split — `STEP 2`, `STEP  2`, attached `STEP2`, and negative increments `STEP -2`/`STEP-2` all parse identically |
| `FOR item IN coll` | S | M | |
| `BREAK` / `CONTINUE` | S | **S** | |
| `STOP IF cond` | S | **S** | v0.48.25 (raises `STOP_STORM`, the 13th storm kind, classified by `plant_storm_match`) |
| `GIVE expr.` | S | **S** | |
| `SHOW expr.` / `SHOW NOW.` / `SHOW TYPE x.` | S | **P** | SHOW expr only; the NOW / TYPE *statements* exist (v0.48.36) but the `SHOW NOW.`/`SHOW TYPE x.` spellings themselves are M |
| `REAP v FROM src, args.` | S | **S** | action/module calls; NOW/TYPEOF available via their v0.48.36 expression forms. v0.49.9: general expressions — REAP accepts translate-time builtins (FIND JOIN SLICE UPPER LOWER ABS ROUND LEN FIRST LAST SUM TRIM REVERSE POW CEIL FLOOR RANDOM SIN COS SQRT HAS ANY ALL PICK COUNT_OF TAP INFUSE ABSORB SEAL TEST COUNT NOW ANALYZE TYPEOF), arithmetic, indexing (a[0]), and literals; numeric results are stored as text (_from_long); legacy IDENT, IDENT:, IDENT[types], and non-builtin IDENT(...) call forms unchanged |
| `REAP … FLOW f1 f2` pipelines | S | M | |
| `PUT val INTO list.` | S | **S** | |
| `TAKE val FROM list.` | S | **S** | v0.48.30 (`plant_list_remove`, first match only) |
| `SORT list [BY f ASC/DESC].` / `SHAKE` / `BRAID` | S | **S** | v0.48.29 (SORT + ASC/DESC + BY multi-field; SHAKE Fisher-Yates); v0.48.31 (BRAID zip + BRAID … AS … MAP) |
| `LINK "k" WITH v IN map.` | S | **S** | v0.48.31 upsert (update in place / append; NULL target instantiated) |
| `WEATHER … SHELTER storm AS e … CALM.` | S | **S** | v0.48.25; THROW + 13 kinds + ANY_STORM catch-all, STOP IF, plant_calm finalization; v0.48.38a `storm()` factory (`THROW storm("TYPE", "msg").` with ARC-managed object binding); v0.48.38b location backfill (`__FILE__`/`__LINE__` injected at the throw site, exposed via `_map_get(e, "file")`/`"line"`) |
| `MATCH expr { Variant(b) -> … }` / `MATCH … IS … YIELD` | S | **S** | v0.49.8: `MATCH subject { pattern -> … }` with literal arms (numbers, strings, TRUE/FALSE), bare-tag arms, `Name(x)` payload bindings and a trailing `_` wildcard; compiles to an if/else-if/else chain over `{ tx_t __mt = <subject>; … }` — literal arms via `_match_eq(__mt, "lit")`, tag arms via `plant_array_length(__mt) == 2 && _match_eq(plant_list_get(__mt, 0), "Tag")` with the payload bound inside the arm block (`tx_t x = _match_extract(__mt, "Tag");`); the wildcard is the unconditional else arm and must come last. `MATCH … IS … YIELD` remains M (regex-path legacy) |
| `TAP/ABSORB/INFUSE/SEAL` (VEIN files) | S | **S** | v0.48.38k (`plant_tap/absorb/infuse/seal` over `FILE*` handles tagged with `VEIN_MAGIC`; legacy INFUSE/ABSORB/SEAL were parse stubs) |
| `HARVEST "url" [METHOD:][BODY:][HEADERS:][TIMEOUT:]` | S | **S** | v0.48.32 (HTTP/1.1 client, response MAP ok/status/body/headers; timeout 0 → 5s; no TLS); v0.48.34 (`… AS … MAP` keeps the connection alive and exposes `sock` for plant_net_read/write/close); v0.49.0 (formalized grammar `HARVEST url AS resp [METHOD m] [BODY b] [HEADERS h] [TIMEOUT t] [MAP]`, any-order options); v0.49.3 (`… AS resp JSON` → `plant_net_harvest_json`, body parsed into a `PlantJson`) |
| `LISTEN BRANCH ON port … LISTEN/.` + `GIVE … AS RESPONSE` | S | **S** | v0.48.33 (LISTEN ON port AS req. one-shot server, request MAP ok/method/path/headers/body, GIVE … AS RESPONSE replies 200 + Content-Length; bind failure → ok FALSE); v0.49.2 (`TIMEOUT t` option → `plant_net_listen_timeout`, accept fails ok FALSE); v0.49.3 (`GIVE … AS RESPONSE JSON` → `plant_net_respond_json`, json_stringify + application/json; plain LISTs serialize as JSON arrays) |
| `WAIT n.` (sync sleep) / `LOCK var.` (sync guard) | S | **S** | v0.48.37e (`WAIT` → `plant_msleep` statement; `LOCK` → centralized runtime Lock Table over variable values, `plant_lock`/`release`/`held`/`status`, `ERR:undefined`/`ERR:full`; zero/negative/bare durations are no-ops) |
| `ANALYZE x.` / `NOW FORMAT:*` / `TYPEOF x.` | S | **S** | v0.48.36 (structural runtime classifier `_plant_val_kind`, uniform `{type = …, size = …, keys = […]}` report, `bad-format:<NAME>` fallback, implicit null for undeclared targets) |
| `FREE` / `ARC LINK|UNLINK` / `FAST RESET.` | S | **S** | v0.48.37a (slab-aware `plant_mem_free` with NULL-back write, ARC edge statements over `plant_arc_link`/`plant_arc_unlink`, mid-scope bump release; `FREE` of a literal is a user error as in C) |
| PERSISTENT GC/lease tuning | S | **S** | v0.48.37b (`PERSIST_GC_INTERVAL` dynamic scheduling, `lease_evict()` pressure-driven reclamation with `PERSIST_PRESSURE`/`PERSIST_LEASE_MS`, deferred-free queue, `plant_persist_status` MAP with `live_objects`/`gc_runs`/`leased_count`/`pending_frees`) |
| `VERIFY "label", assertion.` / `SUITE … SUITE/.` / `STORMS`/`GIVES` | S | M | replaced by test-script harnesses |
| `SHOW_VERIFY_SUMMARY` | S | M | |
| `AWAIT` / `START` / `ASYNC IN` / `CANCEL` / `TRACE` | M (legacy had none) | **S** | new async engine, v0.48.3+ |
| `MISSION CONFIG KEY = VALUE.` | M | **S** | new |
| `TRACE`, bare `call_stmt` | M | **S** | |
| Depth-prefix markers `N\`, closers `N\.`, `/ACTION.` etc. | S | M | **removed outright in v0.48.38l** — the lexer DEPTH block and parser stripping loop were deleted; `N\` now lexes as NUMBER + ERROR and is dropped by the generic unrecognized-token skip |
| `NOTE …` comments, `#` comments | S | **P** | `#` only; NOTE was regex-only legacy |

### 3.3 Expression forms & operators

| Legacy form | Current | Notes |
|---|---|---|
| Integer/decimal literals, negative numbers | **S** | decimal spans lexed since v0.49.6 (`match_number` consumes `1.5`/`2.7`); decimals inside `[ … ]`/`{ … }` literals wrap in `_from_double`; bare decimals in plain assignments stay raw (`SET d TO 1.5.` → `d = 1.5;`, truncating for `long` targets — pre-existing behavior) |
| `"str"` with `\n \t \r \" \' \\` escapes | **S** | |
| `"str {expr}"` string interpolation | **S** | shipped as `"str ${expr}"` (v0.48.22-patch2) with nesting, numeric/enum wrapping; `\${` stays literal and concat chains flatten to `_cat3`/`_cat4` with a single-digit fast path (v0.48.22-patch4) |
| `'str'` single-quoted | **S** | v0.49.20: shared scanner parameterized over the opening quote (escapes incl. `\'`); same STRING token, no `${...}` interpolation |
| `TRUE`/`FALSE`/`NULL`/`VOID` | **P** | TRUE/FALSE/NULL; VOID M |
| `[a, b, c]` array literal | M | **S** (v0.49.4: native literals — `[1, 2, 3]` → `plant_list_make(3, _from_long(1), …)`, integer literals and NUM-typed expressions wrap in `_from_long` (`[x + 1, y]` → `_from_long(x + 1)`), nested `["a", ["b", "c"], "w"]` recurse, `[]` → `plant_list_make(0)`, bare TRUE/FALSE quote as strings; v0.49.6: decimal elements wrap in `_from_double` (`[1.5, 2.7]` → `plant_list_make(3, _from_double(1.5), _from_double(2.7))`); strings/list variables pass through as tx_t; still unsupported: string concatenation inside a literal) |
| `{ k: v }` map literal | M | **S** (v0.49.5: native literals — `{ "name": "plant", "year": 2026 }` → `plant_map_set(plant_map_set(plant_map_create(), "name", "plant"), "year", _from_long(2026))`; keys/values are quoted strings, numbers (→ `_from_long`), variables, nested `[ … ]` and `{ … }`; `{}` → `plant_map_create()`; produces the pair-list MAP form — readable via `_map_get`, serializable via `plant_map_to_string`/`json_stringify`; v0.49.6: decimal values wrap in `_from_double`; bare NUM variables as keys/values still unwrapped) |
| `StructName{ args }` / `{field: value}` struct literals | M | structs are map-backed, created via action returns |
| `a..b` range expression | **S** | v0.49.20: infix shorthand lowers to `plant_range_list(a, b)` (same half-open semantics as the v0.49.15 built-in; works at any bracket depth, string-aware splice) |
| `x[i]` index | **S** | rewritten to `plant_list_get`/array access; `name[expr]` → `plant_list_get(name, expr)` via C-side `handle_brackets` — expression position (v0.49.4-documented; v0.49.6: multi-pass rightmost rewrite adds chained `b[1][0]` → `plant_list_get(plant_list_get(b, 1), 0)` and call-result indexing `f(x)[0]`, `plant_map_get(m, "k")[0]`). `REAP … FROM x[i].` supported since v0.49.6 (reap_expr_stmt) — the target-declaration wiring shipped in v0.49.9 (`collect_used_walk` declares the target `tx_t ""`), and indexed values combine with operators/concat in REAP position (`REAP p FROM lst[1] + "!".`). v0.49.10: indexing into a field-access result works — `m.list[0]` → `plant_list_get(_map_get(m, "list"), 0)` |
| `x[s:e]` slices | M | `SLICE(seq, start, end)` built-in **S** (v0.48.38i, half-open bounds with -1 bound expansion); `x[s:e]` syntax M |
| `obj:prop`, `obj:"k1":"k2"`, `a.b.c`, `SELF:prop` | M | colon-call only for `module:func`; **`a.b.c` field access S (v0.49.10)**: `IDENT . IDENT` lowers to `_map_get(target, "field")` on map-backed lists (chained: `a.b.c` → `_map_get(_map_get(a, "b"), "c")`; field + numeric literal → `_to_long` coercion; `m.list[0]` indexing into a field works; trailing bare `IDENT.` binds to that IDENT, other tails use the whole expression). FFI-struct handles (PlantMap hash maps) still read via explicit `plant_map_get` + `_from_long` |
| `a.method(args)` (push/pop/put/get/has) | **S** | v0.49.11: `IDENT . IDENT (` lowers directly to the runtime API — `push(x)`→`plant_list_push`, `pop()`→`plant_list_pop`, `get(k)`→`plant_map_get`, `put(k,v)`→`plant_map_set`, `has(k)`→`plant_map_has` (parser-wrapped in `_from_long(...)`, numeric coercion in arithmetic via `is_lookup_prefix`). Chained with field access: `l.push("x").pop()`, `nested.get("pt").get("y")`, `m.get("x").name`. Receiver selection mirrors field access (trailing bare IDENT binds, other tails take the whole expression). Bare `m.put(...).` / `l.push(...).` statements emit raw (emit_stmt). Lexer marks line-leading tokens so a chain never merges into the next statement (`SHOW m.name.` + newline + `m.put(...).` stay separate). Unknown methods rejected. Works on map-backed lists (pair-lists) and plain lists |
| `PICK (c) a b` ternary | **S** | v0.48.38h: `PICK(cond, a, b)` built-in with truthiness rules (`"0"`/`"false"` falsy) |
| `COUNT(x)`, `SUM(x)`, `FIRST(x)`, `LAST(x)`, `SORT(x)`, `LEN(x)` | **S** | COUNT→`plant_array_length`, LEN→`strlen`; FIRST/LAST/SUM **S** (v0.48.38d: `plant_first`/`plant_last` boundary elements, `plant_sum` numeric aggregation — empty/NULL → `"0"`, non-parsable elements skipped); SORT(x) expression form **S** (v0.49.15: `plant_list_sort` → `plant_sort(x, "")` — ascending qsort, non-list input passes through; the v0.48.29 SORT statement remains) |
| `UPPER(x)`, `LOWER(x)`, `TRIM(x)`, `REVERSE(x)`, `FIND(t, s)`, `COUNT_OF(t, s)` | **S** | expression built-ins: UPPER/LOWER/TRIM/REVERSE **S** (v0.48.38e, ASCII-safe case ops; TRIM strips space/tab/NL/CR; NULL/empty → `""`; REVERSE **S** for lists too since v0.49.15 via `plant_list_reverse` dispatch — element-wise reversal of list arguments, string behavior preserved), FIND/COUNT_OF **S** (v0.48.38j, `strstr`-based: FIND → 0-based index, `"0"` for empty sub, `"-1"` absent; COUNT_OF → non-overlapping count). Callable in any expression position without REAP or module prefix |
| `INCLUDES(t, s)`, `STARTS_WITH(t, p)`, `ENDS_WITH(t, s)`, `REPEAT(s, n)`, `PAD(s, n, c)`, `PAD_LEFT(s, n, c)` | **S** | v0.49.13: bare expression built-ins mapped through the paren mapper to `string_includes`/`string_starts_with`/`string_ends_with`/`string_repeat`/`string_pad`/`string_pad_left` — no `strings:` module prefix needed; predicates return `"1"`/`"0"`, `REPEAT` with `count <= 0` → `""`, `PAD`/`PAD_LEFT` pass through at/over target length; usable in REAP/SHOW/SET positions and nested; the `strings:` module forms remain fully supported. v0.49.15: `INCLUDES` with a list first argument does element membership via `plant_list_includes` (substring semantics preserved for strings) |
| `REVERSE(lst)`, `RANGE(a, b)`, `SORT(lst)`, `INDEX_OF(lst, item)`, `UNIQUE(lst)`, `AVERAGE(lst)`, `MEDIAN(lst)` | **S** | v0.49.15 list built-ins (batch 1): `plant_list_reverse` (reversed copy; string dispatch), `plant_range_list` (half-open `[a, b)` integer list, `b <= a` → `[]`), `plant_list_sort` (ascending; SORT statement still available), `plant_list_index_of` (first index, `"-1"` when absent/non-list), `plant_list_unique` (first-occurrence dedupe copy), `plant_list_average`/`plant_list_median` (numeric elements via strtod, non-parsable/MAP/LIST elements skipped, empty → `"0"`; average of `[1,2]` → `1.5`). Nested in any expression position |
| `FLATTEN(lst)`, `CHUNK(lst, n)`, `ZIP(a, b)`, `FILTER_GT(lst, t)`, `FILTER_LT(lst, t)` | **S** | v0.49.16 list built-ins (batch 2) — completes the legacy `lists` (14) surface at 100%: `plant_list_flatten` (single-level unnest of kind-0 sub-lists; maps/non-lists pass through), `plant_list_chunk` (max-size sub-lists, `n < 1` or empty → `[]`), `plant_list_zip` (element-wise pair lists, truncated to the shorter list; non-list either side → `[]`), `plant_list_filter_gt`/`plant_list_filter_lt` (strict numeric comparison via tagged-int/strtod parsing, non-numeric elements dropped, threshold `0` literal supported). Nested in any expression position |
| `SPLIT(s, d)`, `JOIN(a, d)` | **P** | SPLIT via `strings:SPLIT`; JOIN **S** (v0.48.38c: `JOIN(list, delim)` → `plant_join` in expression position, empty/NULL guards, serializer for nested MAP/LIST elements) |
| `Option.Some/None`, `Result.Ok/Err` | M | runtime helpers `plant_option_*`/`plant_result_*` unwired |
| `BLOOM Species` | M | |
| Binary ops `+ - * / % **` | **S** | |
| Floor division `//`, comparisons `== != > < >= <=` | **P** | `//` M; comparisons S |
| Keyword ops `IS`, `IS NOT`, `GREATER THAN [OR EQUAL]`, `LESS THAN [OR EQUAL]`, `AND`, `OR`, `NOT` | **S** | translated to C operators |
| `ANY`/`ALL` list quantifiers, `HAS`, `IS_A`, `EMPTY`, `TEST` conditions | **P** | ANY/ALL/HAS **S** (v0.48.38g, substring-aware `_find_substr` matcher); `IS_A`/`EMPTY`/`TEST` M |
| Arbitrary JS expressions in compound expressions | M | **D** — the escape hatch, gone with JS |

### 3.4 Closures

| Legacy form | Current |
|---|---|
| Long form `[MOVE x, REF y](v(NUM)) -> expr` | **S** (v0.47+) |
| Block bodies `-> ( stmts )` | **S** |
| Short form `[MOVE x]`, `[MOVE x y z]`, `[p q]` | **S** (v0.48.19) |
| `MOVE` capture (copy + clear), `REF` capture (aliasing) | **S** |
| Nested closures | **S** |

---

## 4. Standard Library Modules

### 4.1 Legacy innate `PLANT` libraries (innate.js) — status in current compiler

**`math` (14):** `SQRT ABS CEIL FLOOR ROUND RANDOM POW LOG SIN COS PI E SIGN CLAMP`
→ current — expression built-ins: `ABS ROUND POW CEIL FLOOR RANDOM SIN COS SQRT` **S** (v0.48.38f: `translate_expr` maps them through `_handle_func_paren` to `plant_abs/round/pow/ceil/floor/random/sin/cos/sqrt`, plant_runtime.c:2523-2571; `POW(x, y)` dual-arg, `RANDOM()` parameterless, tx_t → double coercion). FFI module: `math:LOG/PI/E/SIGN/CLAMP` (v0.48.27, compat.h:423-425). **v0.49.17 — Extended math library shipped, completing the math module at 100%: 17 new built-ins (MIN MAX TAN ATAN COT ASIN ACOS ATAN2 SINH COSH TANH EXP EXPM1 LOG10 LOG2 LOG1P HYPOT) as bare expression built-ins AND FFI module bindings.** `MIN`/`MAX` (bounds) patched to coerce tagged ints via `_plant_math_num` (previously crashed on numeric literal args); `TAN`/`COT`/`ASIN`/`ACOS`/`ATAN`/`ATAN2` (trig + inverse, domain validation returns `-nan` for out-of-range inputs); `SINH`/`COSH`/`TANH` (hyperbolics); `EXP`/`EXPM1`/`LOG10`/`LOG2`/`LOG1P` (exponential/logarithmic, stability near 0); `HYPOT` (Pythagorean). All 17 are lexer keywords, `_handle_func_paren` mappings, `is_reap_builtin` entries, runtime helpers in `plant_runtime.c` (declared in `plant_compat.h` std/math block + extern wrappers), and `math:` FFI module bindings (`REAP r FROM math:TAN, 3.`). The legacy `math:MIN MAX TAN` gap is closed — the math module is now complete.
  **v0.49.18 — advanced tier (11): `SEC CSC ASINH ACOSH ATANH ERF ERFC GAMMA LGAMMA EXP2 LOG_BASE`** — reciprocal trig (zero divisors → `-nan`), inverse hyperbolics (`ACOSH` x ≥ 1, `ATANH` |x| < 1), error/gamma special functions (`GAMMA`=tgamma, `LGAMMA`, x > 0), `EXP2`, and `LOG_BASE(x, b)` (x > 0 ∧ b > 0 ∧ b ≠ 1 → `-nan`, no silent ln fallback). Math subsystem is feature-complete with uniform `_plant_math_num` coercion and `_plant_math_result` rendering. **v0.49.19 — gap closure:** the legacy eight (`ABS ROUND POW CEIL FLOOR SIN COS SQRT`) migrated to `_math_func_paren` so decimal literal arguments compile (previously raw-double-vs-tx_t compile errors); bare `LOG` shipped end-to-end (keyword + codegen row + REAP ingestion, routing to hardened `math_log`, which now decodes tagged ints via `_plant_math_num`); the `math:` module namespace completed with wrappers for every endpoint. The whole subsystem now accepts numeric literals in every call form.

**`strings` (17):** `UPPER LOWER TRIM LENGTH REVERSE REPEAT PAD_LEFT PAD_RIGHT INCLUDES STARTS_WITH ENDS_WITH SPLIT REPLACE SLICE FIND COUNT_OF JOIN`
→ current — two routes. **Expression built-ins** (callable in any expression, no module prefix): `JOIN(list, delim)` (v0.48.38c, `plant_join` — element concatenation with delimiter control and object-serializer conversion for nested MAP/LIST elements, empty/NULL → `""`), `FIND`/`COUNT_OF(text, sub)` (v0.48.38j, `strstr`-based, non-overlapping counting), `SLICE(data, start, end)` (v0.48.38i, `plant_slice` dual-type dispatch with −1 bound expansion), and `UPPER`/`LOWER`/`TRIM`/`REVERSE` (v0.48.38e, ASCII-safe). Since v0.49.9 all of these are also reachable in REAP position (`REAP r FROM FIND(t, s).`, `REAP j FROM JOIN(lst, "-").`, `REAP u FROM UPPER("x").`, …) without declaring an action, alongside arithmetic, indexing, and literals. **FFI module calls** (REAP from `strings:FUNC`): the v0.48.26 `strings:` bindings `LENGTH REPLACE SPLIT UPPER LOWER TRIM INCLUDES STARTS_WITH ENDS_WITH REVERSE REPEAT PAD PAD_LEFT` (plant_compat.h:128-129, 328, 384-393 — thin wrappers over `string_*`). Still missing as expression built-ins: none — `INCLUDES STARTS_WITH ENDS_WITH REPEAT PAD PAD_LEFT` shipped as bare expression built-ins in v0.49.13 (`string_includes`/`string_starts_with`/`string_ends_with`/`string_repeat`/`string_pad`/`string_pad_left`), while the `strings:` module forms remain supported.

**`lists` (14):** `UNIQUE REVERSE FLATTEN SORT CHUNK ZIP AVERAGE MEDIAN FILTER_GT FILTER_LT INCLUDES INDEX_OF RANGE`
→ current: `JOIN(list, delim)` built-in (v0.48.38c, `plant_join`) covers the legacy `strings:JOIN` surface and the aggregation family is partially covered by `FIRST`/`LAST`/`SUM` (v0.48.38d) and `HAS`/`ANY`/`ALL` (v0.48.38g); `plant_iterator_*`, `plant_range`, Set/Queue/Stack helpers exist in runtime but have no language surface. v0.49.15 (batch 1) shipped as bare expression built-ins: `REVERSE RANGE SORT INCLUDES INDEX_OF UNIQUE AVERAGE MEDIAN` (plant_list_reverse / plant_range_list / plant_list_sort / plant_list_includes / plant_list_index_of / plant_list_unique / plant_list_average / plant_list_median). v0.49.16 (batch 2) closed the gap — `FLATTEN CHUNK ZIP FILTER_GT FILTER_LT` (plant_list_flatten single-level / plant_list_chunk / plant_list_zip truncated to shorter / plant_list_filter_gt / plant_list_filter_lt) — the full legacy `lists` (14) surface is now 100% expression built-ins. (`plant_iterator_*`/Set/Queue/Stack helpers remain unsurfaced — not part of the legacy `lists` module.)

**`io` (5):** `NOW STAMP DATE TIME YEAR` (ar-IQ localized)
→ current: `time_now format parse sleep` runtime helpers (compat.h `std/time`); the language-level `NOW` statement/expression shipped v0.48.36, `io:SHOWLN`/`io:FLUSH` shipped v0.48.28. ar-IQ localization lost.

**`fs` (4):** `READ WRITE APPEND EXISTS`
→ current: `fs:READ fs:WRITE fs:EXISTS` **S** (compiler uses them); v0.48.28 added `fs:APPEND` **S** (`fs_append` in `plant_runtime.c`, "ab" mode auto-creates missing targets, returns `"1"`/`"0"`); plus runtime `file_copy/move/stat/start/dump/end` (compat.h `std/fs`) reachable via externals.

### 4.2 Legacy `std/*.plnt` modules

| Module | Functions | Current |
|---|---|---|
| `std/io.plnt` | `print println flush` (+ externals `plant_printf plant_puts plant_flush`) | **P** — no `print` builtin; SHOW prints; `plant_print` exists |
| `std/string.plnt` | `concat substring` (+ externals `plnt_string_concat plnt_string_len`) | **P** — `+` concat and `substring`-style helpers exist |
| `std/math.plnt` | `sqrt sin cos tan floor ceil abs` (externals) | **S** — runtime `std/math` covers sqrt/sin/cos/tan/floor/ceil/abs; v0.49.17 adds MIN MAX ATAN COT ASIN ACOS ATAN2 SINH COSH TANH EXP EXPM1 LOG10 LOG2 LOG1P HYPOT */
| `std/prelude.plnt` | auto-import of io/string/math | M — no prelude injection in native compiler |

### 4.3 JSON / other runtime std (new, no legacy equivalent)

`std/json` (`json_parse stringify get at len kind val`), `std/time`, `std/fs`, `std/string`, `std/math` — declared in `plant_compat.h`; usable from programs that declare matching externals. This is the current replacement strategy for innate libraries.

---

## 5. Builtin Functions & FFI Bridges

### 5.1 Legacy bridge surface
- **C bridge (`core/runtime_bridge.c`):** `plant_printf` `plant_puts` `plant_flush` (x86-64 fat-pointer ABI).
- **JS external stubs (`_registerStdStubs`):** `plant_printf`, `plant_puts`, `plant_flush`, `get_cli_arg`, `_map_get`.
- **10 std externals** (`sqrt`…`abs`, `plnt_string_concat`, `plnt_string_len`) were declared but had **no JS fallback** (MISSING_STORM at call time) — effectively unusable in interpreted mode.

### 5.2 Current bridge surface
- `plant_compat.h` declares **~103 compiler-facing functions**: `_S/_P/_L/_POS` macros, `_from_long/_to_long/_from_enum/_to_enum/_from_ffi_num`, `_cat`, `_map_get`, `plant_print`, `plant_init_cli`, `get_cli_arg`, `fs_READ/WRITE/EXISTS`, `strings_LENGTH/REPLACE/SPLIT`, `char_at/substring/find_any`, `plant_list_create/get/push`, `plant_array_length`, `plant_map_*`, `plant_string_*`, `plant_math_*`, `plant_time_*`, `plant_file_*`, `plant_json_*`, `plant_option/result/is_*`, `plant_range/array_slice`, `plant_iterator_*`, `plant_cb_*` callbacks, `plant_struct_free`, `plant_profile_*`, `plant_msleep`, `plant_ffi_errno/ffi_last_error`, mission-mode + audit + async + ARC APIs.
- **Expression built-in chain** (`src/plantc/codegen_c.plant:2238-2292`): `translate_expr` rewrites **~48 language-level names** to runtime functions via `_handle_func`/`_handle_func_paren`: `COUNT LEN JOIN FIRST LAST SUM UPPER LOWER TRIM REVERSE ABS ROUND POW CEIL FLOOR RANDOM SIN COS SQRT HAS ANY ALL PICK FIND COUNT_OF SLICE TAP INFUSE ABSORB SEAL` (plus `TEST` → `!` and `NOW/ANALYZE/TYPEOF` via `_ni_replace`). Callable in any expression position with no declaration. v0.49.9: also callable directly in REAP — `REAP f FROM FIND(t, s).` etc. — via the parser's `is_reap_builtin` classification (previously emitted a raw `FIND(...)` C call and failed at link time). v0.49.15 (batch 1): `REVERSE RANGE SORT INCLUDES INDEX_OF UNIQUE AVERAGE MEDIAN` (list built-ins). v0.49.16 (batch 2): `FLATTEN CHUNK ZIP FILTER_GT FILTER_LT` — the legacy `lists` (14) surface is 100% expression built-ins. v0.49.17: `MIN MAX TAN ATAN COT ASIN ACOS ATAN2 SINH COSH TANH EXP EXPM1 LOG10 LOG2 LOG1P HYPOT` (17 math built-ins) — the `math` module is now complete. v0.49.18: `SEC CSC ASINH ACOSH ATANH ERF ERFC GAMMA LGAMMA EXP2 LOG_BASE` (11 more) — the math subsystem is fully mature. v0.49.21: statistical/array layer - `VARIANCE STDDEV PRODUCT MODE` as new keywords; `MIN`/`MAX`/`RANGE` gained single-list-argument forms (spread = max-min) via the arity-dispatching `_stat_paren` mapper while keeping their scalar semantics; defaults "0"/"1"/"" per function with non-parsable elements filtered. v0.49.22: linear algebra engine - `DOT CROSS NORM TRANSPOSE MATRIX_MULT INVERSE DET` over nested lists with an "ERR" error contract for dimension/rectangularity/singularity violations; shared Gaussian-elimination core powers DET and INVERSE. v0.49.23: numerical analysis layer - `LU` (row-pivoted PA=LU), `EIGEN` (Jacobi, symmetric matrices, values descending + eigenvector columns), `SVD` (via A^T A eigenproblem, Gram-Schmidt null-space fill), `SOLVE` (linear systems, singular -> "ERR"), `COND` (Euclidean condition number). Compiler hardening shipped alongside: `_wrap_math_args` wraps numeric literal args in `_from_double` (fixes decimal-literal compile crashes for ALL 28 math built-ins), tagged-safe index arithmetic via `plant_rw_arg_long`, and a keyword-shadow order guard so ASIN/ASINH-class names cannot be carved by shorter needles (SIN/COS/TAN/SINH/COSH/TANH/GAMMA).
- **FFI module bindings** (`plant_compat.h:125-129, 328, 384-393, 423-425`): `strings:` (13: LENGTH REPLACE SPLIT UPPER LOWER TRIM INCLUDES STARTS_WITH ENDS_WITH REVERSE REPEAT PAD PAD_LEFT), `fs:` (4: READ WRITE EXISTS APPEND), `math:` (42 module endpoints after v0.49.19: LOG SIGN CLAMP PI E + wrappers for every v0.48.38f/v0.49.17/v0.49.18 endpoint — ABS ROUND POW CEIL FLOOR RANDOM SIN COS SQRT TAN ATAN COT ASIN ACOS ATAN2 SINH COSH TANH EXP EXPM1 EXP2 LOG10 LOG2 LOG1P HYPOT MIN MAX SEC CSC ASINH ACOSH ATANH ERF ERFC GAMMA LGAMMA LOG_BASE; legacy-eight wrappers route to tagged-int-safe plant_* helpers) — used via `REAP … FROM module:FUNC, …`.
- **Test FFI (`mock_ffi.{h,c}`):** ~76 `ffi_*` functions (sleep, smart/arc/persist status, audit dump, weather memory, now/lock, etc.) — test-provided, not language builtins.
- The 3 legacy stdio bridge functions are covered by `plant_print`/runtime.

**Net:** the current surface is *larger* but structured around the mission/async runtime; the legacy *convenience* library (math/strings/lists/io) was largely not re-exposed.

---

## 6. Runtime Features

### 6.1 Present in both (different implementations)
| Feature | Legacy | Current |
|---|---|---|
| Mission modes BALANCED/FAST/SAFE/SMART/PERSISTENT | MissionStack + 5×5 matrix + boundary validation | **Richer**: `plant_*_enter/exit`, capability checks, audit trail with hash chain, pools, SMART router, PERSISTENT ARC heap + tri-color GC |
| Bump allocator (FAST) | JS `ArenaAllocator` | C `plant_fast_alloc/reset/status` |
| SMART routing | JS `AdaptiveSMARTRouter` (scalar/parallel) | C `plant_smart_route` (chunked vec pool) |
| ARC (PERSISTENT) | JS `ARCHeap` | C `plant_arc_*` + finalizers + leases + cycle detection |
| File IO | Node fs + VeinFS | POSIX `plant_file_*` + `fs:READ/WRITE/EXISTS` |

### 6.2 Legacy-only runtime features (M in current)
- **Storms**: 13 typed exceptions (`ZERO_STORM` … `ANY_STORM`), `WEATHER/SHELTER/CALM`, `storm()` factory, location backfill. **v0.48.25 ships all 13 kinds**: the six core kinds (`ZERO_STORM`, `LOCK_STORM`, `MISSING_STORM`, `NETWORK_STORM`, `LOST_STORM`, `ANY_STORM`) plus the six additive classifications (`RANGE_STORM`, `TYPE_STORM`, `PARSE_STORM`, `HANDLE_STORM`, `HARVEST_STORM`, `FALL_STORM`) and `STOP_STORM` (the `STOP IF` classification), routed by the runtime's `plant_storm_match` against `AS e`-bound shelters with per-kind default messages, plus the `plant_calm` finalization pipeline (CALM runs on normal, caught, unmatched, and `GIVE`/`BREAK`/`CONTINUE` exits); **v0.48.38a ships the `storm()` factory** (`THROW storm("TYPE", "msg").` / `CALL storm(...).`): an ARC-managed `{type, message}` object that survives unwinding, binds as the SHELTER `AS e` value, and is released to the ARC heap after the handler body runs (`plant_storm_release`), with empty-type/empty-message normalization to `ANY_STORM` / registry defaults; **v0.48.38b ships location backfill**: `storm(...)` compiles to `plant_storm("TYPE", "msg", __FILE__, __LINE__, 0)` — the factory packs the compile-site source path and line into the object (column omitted), and SHELTER `AS e` bindings expose `_map_get(e, "file")` / `_map_get(e, "line")` metadata. Nothing missing.
- **Soil scope chain**: locked vars (`LOCK_STORM`), PULSE flags, `WHENEVER … CHANGES` watchers.
- **HTTP server** (LISTEN, request MAPs, JSON bodies, `GIVE … AS RESPONSE`, SIGINT/SIGTERM lifecycle) — `LISTEN ON port AS req.` + `GIVE … AS RESPONSE` shipped in v0.48.33 (one-shot server, plain-text bodies), timeout option v0.49.2, **JSON bodies shipped v0.49.3** (`GIVE … AS RESPONSE JSON` → `plant_net_respond_json`, json_stringify with `application/json`; `HARVEST … AS resp JSON` → `plant_net_harvest_json` parses the body into a `PlantJson`). Still missing: a signal-driven request loop.
- **HTTP client** (HARVEST via worker threads, NETWORK_STORM, `{ok,status,body,headers}` result) — **shipped and formalized v0.49.0**: `HARVEST url AS resp [METHOD m] [BODY b] [HEADERS h] [TIMEOUT t] [MAP].` maps to `plant_net_harvest` / `plant_net_harvest_map` (parser `parse_harvest_stmt`, codegen `harvest_stmt`); MAP-mode exposes the live `sock` for `plant_net_read/write/close`; **v0.49.3 adds `… AS resp JSON.`** → `plant_net_harvest_json` (body parsed into a `PlantJson`, nested access via json_get/json_at). Missing vs legacy: worker-thread fan-out and WebSocket server (listen/accept/handshake) v0.49.47; VERIFY/SUITE keyword recognition v0.49.49; full codegen deferred to v0.49.50; WebSocket client RFC 6455 v0.49.46; TLS/HTTPS client via curl v0.49.43 (verified); HTTPS LISTEN server D (requires OpenSSL headers not in build env); HTTPS LISTEN server requires OpenSSL headers (D).
- **VEIN file handles** (TAP/ABSORB/INFUSE/SEAL) — **shipped v0.48.38k**: `plant_tap/absorb/infuse/seal` over `FILE*` handles tagged with `VEIN_MAGIC` (invalid handles return falsy/`""`; `SEAL` closes and frees). The legacy forms were parse stubs.
- **VERIFY/SUITE** test framework + `SHOW_VERIFY_SUMMARY` (replaced by shell harnesses).
- **WAIT** synchronous sleep statement (Atomics.wait capped at 10 s) — **shipped v0.48.37e** as `WAIT n.` over POSIX `nanosleep` (`plant_msleep`); bare/zero/negative durations are no-ops. The legacy `Atomics.wait`-style capped variant is N/A in the native runtime.
- **ANALYZE**, `NOW FORMAT:*`, `TYPEOF`, `CONVERT` in-place coercion.
- **JS escape hatch** (`new Function` expression evaluation), `inferType`, `coerce`, `_splitArgs` runtime helpers.
- **Diagnostics**: ANSI "Atmospheric Storm Panic" panels with source carets.
- **PULSE/BLOOM rendering** (`GRAPH/TABLE/CHART`), species inheritance.
- Locale-aware (`ar-IQ`) time strings.

### 6.3 Current-only runtime features (new since v0.46)
- **Audit log** (`plant_audit_log/dump`, FNV-1a chain, tamper detection) — every mission-mode event hash-chained.
- **Capability masks** (`plant_cap_check`, zero-perm SAFE, NET_LISTEN v0.48.18) and **boundary blocking** (`plant_boundary_block`).
- **Async engine**: contexts, tokens, `AWAIT/START/ASYNC IN/CANCEL/TRACE`, suspension, steal, work-stealing stats, timer heap.
- **PERSISTENT data-integrity gate** (SAFE-taint, `plant_arc_persist` validation).
- **Callbacks** (`plant_cb_*`), profiling (`plant_profile_*`), `plant_trace`, `plant_msleep`, worker heartbeat/pools (`plant_vec_init`), priority queue, task trees (`plant_task`, teardown).
- Generics monomorphization, generic structs, varargs, `CALLBACK` FFI, `PLANT_STRUCT_/ENUM_/FFI_HAS_*` generated header contracts.

---

## 7. Architecture Drivers of the Gaps

1. **Static typing vs dynamic evaluation.** The legacy evaluator's `new Function()` compilation and `inferType/coerce` make dynamic forms cheap to implement in JS but impossible to express in typed C without a full type-inference pass. This explains the loss of: string interpolation, untyped aggregates, `CONVERT`, `TYPEOF`, `ANY/ALL/HAS/IS_A` conditions, and type-method dispatch.
2. **C codegen vs JS runtime objects.** Maps/lists/structs are now opaque `tx_t` pointers with explicit helper calls. Language-level convenience (literals, slices, SORT/SHAKE/BRAID, JOIN) needs codegen rewrites — SORT/SHAKE (v0.48.29), BRAID/LINK (v0.48.31), JOIN (v0.48.38c, `JOIN(list, delim)` → `plant_join`) and list literals (v0.49.4, `[ … ]` → `plant_list_make` with `_from_long` wrapping) and map literals (v0.49.5, `{ k: v }` → `plant_map_create` + chained `plant_map_set`) have them; the runtime helpers (`plant_array_length`, `plant_range`, `plant_iterator_*`) partially exist for the rest.
3. **No exception machinery.** WEATHER/SHELTER and storms are the biggest *semantic* loss: compiled C has no unwinding; the compiler instead returns errno/`ffi_last_error` strings. Implementing storms would require setjmp/longjmp or explicit propagation — a major roadmap item.
4. **Node dependency removal.** HTTP (client+server), VEIN FS, worker threads, locale formatting, ANSI diagnostics all died with Node. The C runtime retains v0.41-era POSIX sockets (`plant_net_harvest`, `plant_net_listen_*`) that predate the gap and are **unreachable from the language** — re-wiring them is cheap compared to writing them.
5. **Scope model.** PULSE watchers, LOCK/EVAPORATE, ROOT globals and `SELF` bindings need runtime scope objects; compiled scopes are static, so these features would need runtime scope tables.
6. **Test philosophy.** VERIFY/SUITE moved out of the language into shell harnesses (`.expected` diffs), which is why the statements were not ported.
7. **Legacy dead-ends.** Several "legacy" features were already stubs or regex-only in v0.45 (INFUSE/ABSORB/SEAL/EMPTY parse throws, CONVERT/FLOW/MATCH-YIELD/PICK regex-only, missing `require`d modules in the regex path). These are counted as unsupported but were not first-class even at v0.45.

---

## 8. Migration Reference (legacy → current)

| Legacy idiom | Current equivalent |
|---|---|
| `CREATE x(LIST) TO 1, 2, 3.` | `CREATE x(LIST) TO plant_list_make(3, 1, 2, 3).` (or, v0.49.4: `CREATE x(LIST) TO [1, 2, 3].`) |
| `x[i]` / slices | `plant_list_get(x, i)` / `plant_array_slice` (v0.49.4: `x[i]` works directly in expression position; v0.49.6: chained `b[1][0]` and `f(x)[0]` work too) |
| `PUT v INTO l.` | `CALL plant_list_push(l, v).` (or `CALL ffi_…`) |
| Map access `m:"k"` | `_map_get(m, "k")` |
| `{k: v}` map literal | `{ "k": v }` (v0.49.5) — or `plant_map_create()` + `plant_map_set(m, k, v)` |
| `SORT l.` / `SHAKE l.` | `l = plant_sort(l, spec)` / `l = plant_shuffle(l)` (v0.48.29) |
| `math:SQRT(x)` | declare `ACTION sqrt_(v(SCL)) -> external.` against `math_sqrt`, or call `std/math` externals |
| `strings:UPPER(s)` | `UPPER(s)` expression built-in (v0.48.38e) or `REAP r FROM strings:UPPER, s.` |
| `strings:FIND(s, sub)` / `COUNT_OF(s, sub)` / `SLICE(s, a, b)` / `JOIN(l, d)` | `FIND(s, sub)` / `COUNT_OF(s, sub)` / `SLICE(x, a, b)` / `JOIN(l, d)` expression built-ins (v0.48.38j/i/c) |
| `fs:READ(p)` | `REAP r FROM fs:READ, p.` (works) |
| `WEATHER … SHELTER` | errno checks: `REAP e FROM ffi_last_error, ….` + `IF` |
| `HARVEST "url" …` | `HARVEST url AS resp [METHOD m] [BODY b] [HEADERS h] [TIMEOUT t] [MAP|JSON].` (v0.48.32/34, formalized v0.49.0, JSON v0.49.3) |
| `WAIT n.` | `WAIT n.` statement (v0.48.37e) / `CALL ffi_sleep(n).` (test FFI) / `plant_msleep` external |
| `VERIFY`/`SUITE` | regression harness `.plant` + `.expected` files |
| `IMPORT "std/io".` | direct `plant_print`/SHOW; externals for the rest |
| `MISSION : SAFE.` | `ACTION x() WITH MISSION SAFE,` or `MISSION CONFIG` keys |

---

## 9. Roadmap Priorities

**High effort / high value**
1. **Storms / exception handling** (WEATHER/SHELTER/CALM + `LOST_STORM`/`ZERO_STORM`): **shipped** — v0.48.23 added `THROW` + `WEATHER/SHELTER/CALM` with mandatory `CALM`, unmatched re-propagation, and `GIVE`/`BREAK`/`CONTINUE` frame popping via `setjmp`/`longjmp` frames; v0.48.23-patch completed the 12-kind registry (with per-kind default messages) and the runtime `plant_storm_match` catch-all matcher; v0.48.25 adds `STOP IF` (raises `STOP_STORM`, the 13th kind) and the `plant_calm` finalization pipeline that runs `CALM` on every exit path, including `GIVE`/`BREAK`/`CONTINUE` interruptions. **Weather memory management shipped v0.48.37d**: every WEATHER frame binds a dedicated exit-list, `plant_weather_leave` frees registered handles (ARC-aware) and drains deferred deallocations on every exit path, SHELTER handler temporaries are purged on handler exit, and `plant_weather_status` reports `active_frames`/`live_objects`/`pending_frees`/`storm_handlers` telemetry. **`storm()` factory shipped v0.48.38a**: `THROW storm("TYPE", "msg").` builds an ARC-managed `{type, message}` exception object that survives unwinding, binds as the SHELTER `AS e` value, and is released after the handler body runs; `CALL storm(...).` and bare `storm(...).` statements work too. **Location backfill shipped v0.48.38b**: the factory takes `(file, line, column)`, the codegen injects `__FILE__`, `__LINE__`, `0` at every `storm(...)` call, and the fields are conditionally packed so `AS e` bindings expose `_map_get(e, "file")` / `_map_get(e, "line")`. Shipped in full.
2. **List/map/std library surface**: array & map literals (v0.49.4 `[ … ]`, v0.49.5 `{ k: v }`, v0.49.6 decimal elements `_from_double`), slices, `FIRST/LAST/SUM/AVERAGE/MEDIAN/UNIQUE/REVERSE/FLATTEN/CHUNK/ZIP/RANGE` — **complete v0.49.15 + v0.49.16** (batch 1: `REVERSE RANGE SORT INCLUDES INDEX_OF UNIQUE AVERAGE MEDIAN` shipped v0.49.15; batch 2: `FLATTEN CHUNK ZIP FILTER_GT FILTER_LT` shipped v0.49.16 — the legacy `lists` (14) surface is 100% expression built-ins), string `FIND/COUNT_OF` (**shipped v0.48.38j**), `SLICE` (**shipped v0.48.38i**), `JOIN` (**shipped v0.48.38c**), `UPPER/LOWER/TRIM/INCLUDES/STARTS_WITH/ENDS_WITH/REVERSE/REPEAT/PAD/PAD_LEFT` (v0.48.26 shipped via the `strings:` FFI module), `math` extras (`LOG PI E SIGN CLAMP` shipped v0.48.27; `ABS ROUND POW CEIL FLOOR RANDOM SIN COS SQRT` bindings), `io:SHOWLN io:FLUSH` (shipped v0.48.28), `fs:APPEND` (shipped v0.48.28).
3. **String interpolation** — shipped as `"str ${expr}"` (v0.48.22-patch2); ORIF/ELSE (v0.48.20), CYCLE 3 forms (v0.48.21-22) and INCREASE/DECREASE (v0.48.22-patch) are shipped; numeric CYCLE hardened with static STEP evaluation in v0.48.22-patch3 (zero-step compile error, direction-aware expression steps, runtime nonzero guard); v0.48.22-patch4 adds literal `\${` escape markers, `_cat3`/`_cat4` chain flattening, and the single-digit `_from_digit` fast path.

**Medium effort**
4. **Re-wire the dormant POSIX sockets**: `HARVEST` (HTTP GET/POST via `plant_net_harvest`) and a minimal `LISTEN` server (or defer).
5. **VEIN file handles** (TAP/ABSORB/INFUSE/SEAL) — **shipped v0.48.38k** (`plant_tap/absorb/infuse/seal`).
6. **`NOW`/`ANALYZE`/`TYPEOF` statements** over `std/time` + type tags — shipped v0.48.36 (statement + expression forms).
7. **Memory safety layer (EVAPORATE)** — shipped v0.48.37a: `FREE x.` /
   `ARC LINK/UNLINK` / `FAST RESET.` statements, fixed-size string slabs
   for `_cat` reuse, `plant_mem_report` / `plant_mem_scan` accounting,
   DistributedHeap with a consistent-hash ring over ARC segments and
   lease-based eviction (`MISSION CONFIG DIST_NODES`/`DIST_NODE_CAP`),
    and RAII `plant_fast_reset` on FAST action exit. Architecture in
    `docs/EVAPORATE.md`. **True SAFE isolation across real worker
    processes shipped v0.48.37c**: SAFE actions run in forked workers
    with a typed wire codec (`'N'/'I'/'S'/'A'/'F'`; `'F'` = memfd +
    SCM_RIGHTS for payloads > 1MB) and generated adapters; numeric
    SAFE args cross the wire as TX strings (the `< 4096` small-int
    heuristic would otherwise misread large raw literals as pointers)
    and are parsed back via `plant_rw_arg_long`.
8. **`CONST`/`ROOT` immutables** — shipped v0.48.35 (`CONST`/`ROOT`/`ROOT_SCOPE`).

**Low effort / niche**
8. **SPLIT/JOIN statement forms** (SORT/SHAKE shipped v0.48.29; BRAID/LINK shipped v0.48.31; **`JOIN(list, delim)` function shipped v0.48.38c**), `PICK` (**shipped v0.48.38h**), `STOP IF` (shipped v0.48.25), `WAIT n.` statement (**shipped v0.48.37e**; `LOCK var.` synchronization also shipped v0.48.37e), `ANY/ALL/HAS` conditions (**shipped v0.48.38g**; `IS_A` remains M), `LOCATE`/`NOTE` comments, brace-form ACTION bodies (**shipped v0.49.24**), TYPE aliases, single-quoted strings (**shipped v0.49.20**).
9. **Legacy `N\` depth-prefixed syntax** — **resolved by removal**: the DEPTH token machinery was deleted in v0.48.38l, closing this item permanently; no drop-in compatibility is planned.

**Intentionally out of scope (D)**
- SPECIES/BLOOM object model, `SELF:`/method dispatch, `PLANT` library statements, PULSE/WHENEVER watchers, JS `Function()` escape hatch, locale-specific IO formatting, VERIFY/SUITE language framework.

*Report generated for v0.48.19 (commit b2b2705) against legacy v0.45.0 (git 7f54eae); continuously updated through v0.49.10. As of v0.49.10 the legacy side exists only in git history — `core/`, `src/**/*.js`, `std/`, `service/`, `webrepl/`, and `benchmarks/` were removed from the working tree (commit `c17de62`).*
