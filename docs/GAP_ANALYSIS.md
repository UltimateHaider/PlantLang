# Legacy JS Interpreter vs Self-Hosted Compiler — Gap Analysis Report

**Scope:** Legacy JavaScript interpreter (PlantLang ≤ v0.45.x, `core/interpreter.js` + associated modules, recovered from git at `7f54eae` v0.45.0) vs the self-hosted native compiler (v0.48.19, `src/plantc/*.plant` → C, `runtime/c/plant_runtime.c`).

---

## 1. Executive Summary

PlantLang transitioned from a **tree-walking JavaScript interpreter** (~9,000 lines of `core/*.js` + `src/interpreter/*` + `std/*.plnt`) to a **self-hosted native compiler** (`src/plantc/*.plant` ≈ 6,700 lines of PlantLang that compiles PlantLang to C, linked against a ~3,500-line C runtime). The compiler achieved full self-hosting convergence (v0.48.19, 257,107 bytes) and ships a richer mission-mode runtime (FAST/SAFE/SMART/PERSISTENT with audit trails, capability checks, an async engine and a reference-counted ARC heap) than the interpreter ever had.

The transition is a **re-implementation with deliberate scope cuts**, not a 1:1 port. High-level counts:

| Area | Legacy (v0.45.0) | Current (v0.48.19) | Gap |
|---|---|---|---|
| Statement keywords (parser dispatch) | ~40 | ~18 + bare calls/closures | ~22 missing |
| Innate/std library functions | ~60 (41 innate + 19 std/.plnt + 5 FFI stubs) | ~20 reachable builtins + ~50 declared runtime helpers | ~40 missing |
| Runtime features | ~90 capabilities | ~70 capabilities | roughly balanced, different sets |
| Network | HTTP client + server (HARVEST/LISTEN) | vestigial POSIX sockets in runtime, **not reachable** | high |
| Error handling | Storm exceptions (WEATHER/SHELTER, 12 storm types) | all 12 kinds + registry defaults (v0.48.23-patch); no `storm()` factory | low |
| Object model | SPECIES classes, BLOOM instantiation, SELF methods, inheritance | none | high |

**Key architectural differences that explain the gaps:**

1. **Dynamic vs static typing.** The interpreter evaluated compound expressions by string-substitution into `new Function()` (arbitrary JavaScript accepted), used JS `Map`/`Array` for aggregates, and allowed runtime type coercion (`CONVERT`, `inferType`, `coerce`). The compiler is statically typed: every expression is rewritten into C with explicit `long`/`tx_t`/struct types. Anything that depended on runtime dynamics (type inspection, dynamic conversions, untyped aggregates, method dispatch) could not survive without a type system the compiler does not yet have.
2. **Interpreted scope chain vs compiled C scopes.** The interpreter resolved variables through a runtime `Soil` scope chain with locks, pulses and storm-watching. Compiled code uses static C scopes; scope-dynamic features (LOCK/EVAPORATE, PULSE watchers, `WHENEVER`, `ROOT`/`ROOT_SCOPE` globals) have no compilation target.
3. **Host capabilities vs portable C runtime.** The interpreter leaned on Node.js (`fs`, `http`, `worker_threads`, `Atomics`, locale formatting, ANSI diagnostics). The C runtime re-implements only a thin slice (stdio via `runtime_bridge.c`, POSIX file ops, and *unwired* POSIX sockets). HTTP, worker-based HARVEST, VEIN file handles, locale-aware time formatting and terminal diagnostics were lost with Node.
4. **Regex + AST dual pipeline vs single AST pipeline.** The interpreter had a legacy regex execution path with many one-off forms (`CONVERT`, `FLOW`, `MATCH … YIELD`, `PICK`, `NOTE`, `STEADY`) that were never migrated into its own AST path; those forms are effectively dead even in the legacy engine and were simply not carried over.
5. **Interpreter-added features post-v0.45 are intentionally dropped.** The prompt excludes intentionally deprecated features; `SPECIES`/`BLOOM`, VEIN file IO and the JS `Function()` escape hatch were explicitly superseded (the Language Tour marks `core/*.js` as "historical only").

---

## 2. Methodology & Sources

- **Legacy side** (recovered from git `7f54eae`, the last commit shipping `core/interpreter.js`):
  `core/interpreter.js` (2,828 L), `core/parser.js` (3,033 L), `core/ast.js` (1,040 L), `core/typechecker.js` (1,529 L), `core/tokenizer.js` (259 L), `core/evaluator.js` (173 L), `core/runtime.js` (64 L), `core/innate.js` (67 L), `core/dispatcher.js` (286 L), `core/matrix.js`, `core/harvest.js` + `harvest_worker.js`, `core/diagnostics.js`, `core/lexer.js`, `core/runtime_bridge.c`, `src/interpreter/{cycle_evaluator,sort_evaluator,bloom_evaluator,show_formatter}.js`, `std/{prelude,string,math,io}.plnt`.
- **Current side** (working tree v0.48.19): `src/plantc/{tokens,ast,lexer,parser,codegen_c,main}.plant`, `runtime/c/plant_runtime.c` (3,527 L), `runtime/c/plant_compat.h` (549 L), `runtime/c/plant_runtime.h`, `tests/native/mock_ffi.{h,c}`, `Language Tour.md`.
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
| `SPECIES name [FROM base], VAR f, ACTION…` | S | M | **D** (object model dropped) |
| `STRUCT` / `SHAPE` | S | **P** | STRUCT with typed fields only (map-backed); no SHAPE |
| `CHOICE Variant(TYPE)` + `Option`/`Result` | S | **P** | no CHOICE; `Option`/`Result`/`is_some` etc. exist only as runtime helpers |
| `ENUM` | S | **S** | members as `NAME.MEMBER`, `_to_enum`/`_from_enum` |
| `CONST`, `ROOT`, `ROOT_SCOPE` | S | M | no immutable declaration |
| `TYPE alias = target.` | S | M | |
| `MISSION : mode.` (set mode) | S | **P** | `MISSION CONFIG KEY = VALUE.` only |
| `PLANT lib [AS alias].` | S | M | replaced by direct `module:func` calls |
| `IMPORT "path".` (user programs) | S | **P** | IMPORT only for the compiler's own sources; no runtime program imports |
| `LET {a,b} = …` / `LET [h,t] = …` destructuring | S | M | |

### 3.2 Statements

| Legacy construct | Legacy status | Current | Notes |
|---|---|---|---|
| `CREATE` (typed, `TO`, empty) | S | **S** | no `PULSE` flag |
| `CREATE x(LIST) TO a, b, c.` comma init | S | M | aggregates built via `plant_list_make` |
| `LET x = expr.` | S | **S** | |
| `SET t TO e.` | S | **S** | `SELF:prop`/`obj:field` member targets M |
| `INCREASE` / `DECREASE … BY` | S | **S** | `INCREASE x BY n.` / `DECREASE x BY n.` shipped in v0.48.22-patch (numeric targets only, compound C assignment) |
| `EVAPORATE` / `LOCK` / `EMPTY` | S | M | EMPTY was already a parse stub in legacy |
| `IF cond, body [ORIF][ELSE]` | S | **S** | Shipped in v0.48.20 (multi-branch chains) |
| `SEASON cond, body` | S | **S** | |
| `CYCLE x IN list` / `CYCLE i, idx IN` / `CYCLE i FROM lo TO hi` | S | **S** | All three CYCLE forms shipped in v0.48.22 (FROM/TO in v0.48.21); STEP is statically evaluated since v0.48.22-patch3 — zero steps are a compile-time error and expression steps pick direction-aware bounds |
| `FOR item IN coll` | S | M | |
| `BREAK` / `CONTINUE` | S | **S** | |
| `STOP IF cond` | S | M | |
| `GIVE expr.` | S | **S** | |
| `SHOW expr.` / `SHOW NOW.` / `SHOW TYPE x.` | S | **P** | SHOW expr only; NOW/TYPE forms M |
| `REAP v FROM src, args.` | S | **S** | only action calls; `NOW`, `TYPEOF`, string/expr sources M |
| `REAP … FLOW f1 f2` pipelines | S | M | |
| `PUT val INTO list.` | S | **S** | |
| `TAKE val FROM list.` | S | M | |
| `SORT list [BY f ASC/DESC].` / `SHAKE` / `BRAID` | S | M | |
| `LINK "k" WITH v IN map.` | S | M | map ops via `_map_get`/`plant_map_get` only |
| `WEATHER … SHELTER storm AS e … CALM.` | S | **S** | v0.48.23-patch; THROW + all 12 kinds + ANY_STORM catch-all |
| `MATCH expr { Variant(b) -> … }` / `MATCH … IS … YIELD` | S | M | |
| `TAP/ABSORB/INFUSE/SEAL` (VEIN files) | S | M | INFUSE/ABSORB/SEAL were parse stubs in legacy too |
| `HARVEST "url" [METHOD:][BODY:][HEADERS:][TIMEOUT:]` | S | M | sockets exist in C runtime, unwired |
| `LISTEN BRANCH ON port … LISTEN/.` + `GIVE … AS RESPONSE` | S | M | |
| `WAIT n.` (sync sleep) | S | **P** | only `plant_msleep` via external; no statement |
| `ANALYZE x.` | S | M | |
| `VERIFY "label", assertion.` / `SUITE … SUITE/.` / `STORMS`/`GIVES` | S | M | replaced by test-script harnesses |
| `SHOW_VERIFY_SUMMARY` | S | M | |
| `AWAIT` / `START` / `ASYNC IN` / `CANCEL` / `TRACE` | M (legacy had none) | **S** | new async engine, v0.48.3+ |
| `MISSION CONFIG KEY = VALUE.` | M | **S** | new |
| `TRACE`, bare `call_stmt` | M | **S** | |
| Depth-prefix markers `N\`, closers `N\.`, `/ACTION.` etc. | S | **P** | tokenized as DEPTH and stripped; legacy `N\ SHOW "x".` unsupported |
| `NOTE …` comments, `#` comments | S | **P** | `#` only; NOTE was regex-only legacy |

### 3.3 Expression forms & operators

| Legacy form | Current | Notes |
|---|---|---|
| Integer/decimal literals, negative numbers | **S** | |
| `"str"` with `\n \t \r \" \' \\` escapes | **S** | |
| `"str {expr}"` string interpolation | **S** | shipped as `"str ${expr}"` (v0.48.22-patch2) with nesting, numeric/enum wrapping; `\${` stays literal and concat chains flatten to `_cat3`/`_cat4` with a single-digit fast path (v0.48.22-patch4) |
| `'str'` single-quoted | M | regex-path only in legacy too |
| `TRUE`/`FALSE`/`NULL`/`VOID` | **P** | TRUE/FALSE/NULL; VOID M |
| `[a, b, c]` array literal | M | use `plant_list_make(n, …)` |
| `{ k: v }` map literal | M | use `_map_get`/`plant_map_get` style |
| `StructName{ args }` / `{field: value}` struct literals | M | structs are map-backed, created via action returns |
| `a..b` range expression | M | runtime `plant_range` helper exists but not as syntax |
| `x[i]` index | **S** | rewritten to `plant_list_get`/array access |
| `x[s:e]` slices | M | |
| `obj:prop`, `obj:"k1":"k2"`, `a.b.c`, `SELF:prop` | M | colon-call only for `module:func` |
| `a.method(args)` (push/pop/put/get/has) | M | |
| `PICK (c) a b` ternary | M | |
| `COUNT(x)`, `SUM(x)`, `FIRST(x)`, `LAST(x)`, `SORT(x)`, `LEN(x)` | **P** | COUNT→`plant_array_length`, LEN→`strlen`; SUM/FIRST/LAST/SORT M |
| `SPLIT(s, d)`, `JOIN(a, d)` | **P** | SPLIT via `strings:SPLIT`; JOIN M |
| `Option.Some/None`, `Result.Ok/Err` | M | runtime helpers `plant_option_*`/`plant_result_*` unwired |
| `BLOOM Species` | M | |
| Binary ops `+ - * / % **` | **S** | |
| Floor division `//`, comparisons `== != > < >= <=` | **P** | `//` M; comparisons S |
| Keyword ops `IS`, `IS NOT`, `GREATER THAN [OR EQUAL]`, `LESS THAN [OR EQUAL]`, `AND`, `OR`, `NOT` | **S** | translated to C operators |
| `ANY`/`ALL` list quantifiers, `HAS`, `IS_A`, `EMPTY`, `TEST` conditions | M | |
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
→ current: runtime has `math_sqrt cos sin tan floor ceil round abs pow min max random` (compat.h `std/math`), reachable via externals, **not** as `PLANT math` module calls. Missing by name: `LOG PI E SIGN CLAMP RANDOM` present? `math_random` exists ✓; `LOG/PI/E/SIGN/CLAMP` M.

**`strings` (17):** `UPPER LOWER TRIM LENGTH REVERSE REPEAT PAD_LEFT PAD_RIGHT INCLUDES STARTS_WITH ENDS_WITH SPLIT REPLACE SLICE FIND COUNT_OF JOIN`
→ current: `strings:LENGTH REPLACE SPLIT` only (compiler + compat.h). `string_repeat/reverse/pad` exist in runtime (compat.h `std/string`) — reachable via externals. Missing: `UPPER LOWER TRIM INCLUDES STARTS_WITH ENDS_WITH SLICE FIND COUNT_OF JOIN`.

**`lists` (14):** `UNIQUE REVERSE FLATTEN SORT CHUNK ZIP AVERAGE MEDIAN FILTER_GT FILTER_LT INCLUDES INDEX_OF RANGE`
→ current: none as language-level; `plant_iterator_*`, `plant_range`, Set/Queue/Stack helpers exist in runtime but no list-comprehension surface. Missing: all 14.

**`io` (5):** `NOW STAMP DATE TIME YEAR` (ar-IQ localized)
→ current: `time_now format parse sleep` runtime helpers (compat.h `std/time`); language has no `NOW` form. Localization lost.

**`fs` (4):** `READ WRITE APPEND EXISTS`
→ current: `fs:READ fs:WRITE fs:EXISTS` **S** (compiler uses them); `APPEND` M; plus runtime `file_copy/move/stat/start/dump/end` (compat.h `std/fs`) reachable via externals.

### 4.2 Legacy `std/*.plnt` modules

| Module | Functions | Current |
|---|---|---|
| `std/io.plnt` | `print println flush` (+ externals `plant_printf plant_puts plant_flush`) | **P** — no `print` builtin; SHOW prints; `plant_print` exists |
| `std/string.plnt` | `concat substring` (+ externals `plnt_string_concat plnt_string_len`) | **P** — `+` concat and `substring`-style helpers exist |
| `std/math.plnt` | `sqrt sin cos tan floor ceil abs` (externals) | **P** — runtime `std/math` covers sqrt/sin/cos/floor/ceil/abs; tan M |
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
- **Test FFI (`mock_ffi.{h,c}`):** ~40 `ffi_*` functions (sleep, smart/arc/persist status, audit dump, etc.) — test-provided, not language builtins.
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
- **Storms**: 12 typed exceptions (`ZERO_STORM` … `ANY_STORM`), `WEATHER/SHELTER/CALM`, `storm()` factory, location backfill. **v0.48.23-patch ships all 12 kinds**: the six core kinds (`ZERO_STORM`, `LOCK_STORM`, `MISSING_STORM`, `NETWORK_STORM`, `LOST_STORM`, `ANY_STORM`) plus the six additive classifications (`RANGE_STORM`, `TYPE_STORM`, `PARSE_STORM`, `HANDLE_STORM`, `HARVEST_STORM`, `FALL_STORM`), routed by the runtime's `plant_storm_match` against `AS e`-bound shelters with per-kind default messages for message-less `THROW`s; still missing: the `storm()` factory and location backfill.
- **Soil scope chain**: locked vars (`LOCK_STORM`), PULSE flags, `WHENEVER … CHANGES` watchers.
- **HTTP server** (LISTEN, request MAPs, JSON bodies, `GIVE … AS RESPONSE`, SIGINT/SIGTERM lifecycle) — the C runtime's `plant_net_listen_open/accept/read/write/close` (v0.41-era POSIX sockets) are **not wired into the compiler**.
- **HTTP client** (HARVEST via worker threads, NETWORK_STORM, `{ok,status,body,headers}` result) — `plant_net_harvest` exists in the C runtime but is also **unwired**.
- **VEIN file handles** (TAP/ABSORB/INFUSE/SEAL) — partially stub-inherited from legacy itself.
- **VERIFY/SUITE** test framework + `SHOW_VERIFY_SUMMARY` (replaced by shell harnesses).
- **WAIT** synchronous sleep statement (Atomics.wait capped at 10 s).
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
2. **C codegen vs JS runtime objects.** Maps/lists/structs are now opaque `tx_t` pointers with explicit helper calls. Language-level convenience (literals, slices, SORT/SHAKE/BRAID, JOIN) needs codegen rewrites that haven't been written, though the runtime helpers (`plant_array_length`, `plant_range`, `plant_iterator_*`) partially exist.
3. **No exception machinery.** WEATHER/SHELTER and storms are the biggest *semantic* loss: compiled C has no unwinding; the compiler instead returns errno/`ffi_last_error` strings. Implementing storms would require setjmp/longjmp or explicit propagation — a major roadmap item.
4. **Node dependency removal.** HTTP (client+server), VEIN FS, worker threads, locale formatting, ANSI diagnostics all died with Node. The C runtime retains v0.41-era POSIX sockets (`plant_net_harvest`, `plant_net_listen_*`) that predate the gap and are **unreachable from the language** — re-wiring them is cheap compared to writing them.
5. **Scope model.** PULSE watchers, LOCK/EVAPORATE, ROOT globals and `SELF` bindings need runtime scope objects; compiled scopes are static, so these features would need runtime scope tables.
6. **Test philosophy.** VERIFY/SUITE moved out of the language into shell harnesses (`.expected` diffs), which is why the statements were not ported.
7. **Legacy dead-ends.** Several "legacy" features were already stubs or regex-only in v0.45 (INFUSE/ABSORB/SEAL/EMPTY parse throws, CONVERT/FLOW/MATCH-YIELD/PICK regex-only, missing `require`d modules in the regex path). These are counted as unsupported but were not first-class even at v0.45.

---

## 8. Migration Reference (legacy → current)

| Legacy idiom | Current equivalent |
|---|---|
| `CREATE x(LIST) TO 1, 2, 3.` | `CREATE x(LIST) TO plant_list_make(3, 1, 2, 3).` |
| `x[i]` / slices | `plant_list_get(x, i)` / `plant_array_slice` |
| `PUT v INTO l.` | `CALL plant_list_push(l, v).` (or `CALL ffi_…`) |
| Map access `m:"k"` | `_map_get(m, "k")` |
| `SORT l.` / `SHAKE l.` | none (sort via `plant_iterator_*` + custom loop, or runtime helpers) |
| `math:SQRT(x)` | declare `ACTION sqrt_(v(SCL)) -> external.` against `math_sqrt`, or call `std/math` externals |
| `strings:UPPER(s)` | none — implement with loop + `char_at` |
| `fs:READ(p)` | `REAP r FROM fs:READ, p.` (works) |
| `WEATHER … SHELTER` | errno checks: `REAP e FROM ffi_last_error, ….` + `IF` |
| `HARVEST "url" …` | none (C socket code unwired) |
| `WAIT n.` | `CALL ffi_sleep(n).` (test FFI) / `plant_msleep` external |
| `VERIFY`/`SUITE` | regression harness `.plant` + `.expected` files |
| `IMPORT "std/io".` | direct `plant_print`/SHOW; externals for the rest |
| `MISSION : SAFE.` | `ACTION x() WITH MISSION SAFE,` or `MISSION CONFIG` keys |

---

## 9. Roadmap Priorities

**High effort / high value**
1. **Storms / exception handling** (WEATHER/SHELTER/CALM + `LOST_STORM`/`ZERO_STORM`): **shipped** — v0.48.23 added `THROW` + `WEATHER/SHELTER/CALM` with mandatory `CALM`, unmatched re-propagation, and `GIVE`/`BREAK`/`CONTINUE` frame popping via `setjmp`/`longjmp` frames; v0.48.23-patch completes the 12-kind registry (`RANGE`/`TYPE`/`PARSE`/`HANDLE`/`HARVEST`/`FALL` join the six core kinds) with per-kind default messages and the runtime `plant_storm_match` catch-all matcher. Remaining (low priority): the `storm()` factory and location backfill.
2. **List/map/std library surface**: array & map literals, slices, `FIRST/LAST/SUM/AVERAGE/MEDIAN/UNIQUE/REVERSE/FLATTEN/CHUNK/ZIP/RANGE`, string `UPPER/LOWER/TRIM/INCLUDES/STARTS_WITH/ENDS_WITH/FIND/COUNT_OF/JOIN/REPLACE/SPLIT/SLICE/REPEAT/PAD_*`, `math` extras (`LOG PI E SIGN CLAMP`), `fs:APPEND`.
3. **String interpolation** — shipped as `"str ${expr}"` (v0.48.22-patch2); ORIF/ELSE (v0.48.20), CYCLE 3 forms (v0.48.21-22) and INCREASE/DECREASE (v0.48.22-patch) are shipped; numeric CYCLE hardened with static STEP evaluation in v0.48.22-patch3 (zero-step compile error, direction-aware expression steps, runtime nonzero guard); v0.48.22-patch4 adds literal `\${` escape markers, `_cat3`/`_cat4` chain flattening, and the single-digit `_from_digit` fast path.

**Medium effort**
4. **Re-wire the dormant POSIX sockets**: `HARVEST` (HTTP GET/POST via `plant_net_harvest`) and a minimal `LISTEN` server (or defer).
5. **VEIN file handles** (TAP/ABSORB/INFUSE/SEAL) over `plant_file_*`.
6. **`NOW`/`ANALYZE`/`TYPEOF` statements** over `std/time` + type tags.
7. **`CONST`/`ROOT` immutables** (compile-time locking).

**Low effort / niche**
8. **SPLIT/JOIN/SORT/SHAKE/BRAID statement forms**, `PICK`, `STOP IF`, `WAIT n.` statement, `ANY/ALL/HAS/IS_A` conditions, `LOCATE`/`NOTE` comments, brace-form ACTION bodies, TYPE aliases, single-quoted strings.
9. **Legacy `N\` depth-prefixed syntax** (tokenized already; only needs codegen acceptance) for drop-in legacy source compatibility.

**Intentionally out of scope (D)**
- SPECIES/BLOOM object model, `SELF:`/method dispatch, `PLANT` library statements, PULSE/WHENEVER watchers, JS `Function()` escape hatch, locale-specific IO formatting, VERIFY/SUITE language framework.

*Report generated for v0.48.19 (commit b2b2705) against legacy v0.45.0 (git 7f54eae).*
