# Changelog — PlantLang / Chloroplast

## v0.48.28 — 2026 (IO and File System Extensions)

### New Features
- **`io:` FFI module.** Two bindings expose runtime output control to
  scripts: `io:SHOWLN` and `io:FLUSH` (thin static wrappers in
  `plant_compat.h`, same `module:FUNC` → `module_FUNC` rewrite as the
  `strings:`/`math:` bindings; zero-argument REAP for `FLUSH`).
- **Runtime implementations** (`plant_runtime.c`):
  - `io_showln` — prints the text followed by a trailing newline.
    NULL and empty inputs degrade to a bare newline (no pointer or
    formatting faults). Returns `"1"`.
  - `io_flush` — forces immediate stdout buffer clearance via
    `fflush(stdout)` so printed data is observable without buffering
    delays when stdout is piped. Returns `"1"`.
  - `fs_append` — appends text content to a target path opened in
    `"ab"` (append mode): a pre-existing file keeps its prior content
    and receives the new text after it, and a non-existent target is
    dynamically created. NULL paths, open errors, and close errors
    return `"0"`; empty content is a no-op. Reliable `fclose` on all
    paths prevents resource leaks.
- **`fs:APPEND` FFI binding** — closes the last gap in the legacy
  `fs` module (`READ WRITE APPEND EXISTS` now all callable from
  scripts).

### Regression Tests
- `io_full`: `SHOWLN` across plain text, empty input (bare newline),
  a multi-line string (literal newlines from the source), and
  whitespace-padded text; `FLUSH` between outputs with return-code
  verification.
- `fs_append`: append to a non-existent target (created), append to a
  pre-existing target (prior content preserved, exact concatenation
  verified by `fs:READ`-back), empty-content append (no-op), a second
  independent file, and an unwritable path (`"0"`). The regression
  runner removes stale `/tmp/plantlang_fs_append_*.txt` scratch files
  before each run so appends always start from empty.

## v0.48.27 — 2026 (Mathematical Library)

### New Features
- **`math:` FFI module.** Five bindings expose the std/math library
  to scripts: `math:LOG`, `math:PI`, `math:E`, `math:SIGN`, and
  `math:CLAMP` (thin static wrappers in `plant_compat.h` over the
  runtime, same `module:FUNC` → `module_FUNC` codegen rewrite as the
  `strings:` bindings).
- **Runtime implementations** (`plant_runtime.c`, std/math):
  - `math_log` — natural logarithm via `log()`; non-positive inputs
    (≤ 0) return an explicit error message
    (`ERR: math_log(x): x must be > 0 (x = …)`) instead of NaN.
  - `math_sign` — sign determination returning `"-1"` for negative
    values, `"0"` for zero (including `-0.0`), `"1"` for positive.
  - `math_clamp` — boundary restriction of `x` into `[lo, hi]`;
    both bounds are inclusive, and values already inside pass through
    unchanged (decimal formatting via `%.10g`).
  - `math_pi` / `math_e` — constant accessors formatted to 15
    significant digits, with explicit `M_PI` / `M_E` fallback
    definitions when the standard library macros are unavailable
    (strict feature-test builds), guarded by `#ifndef`.
- **Zero-argument REAP.** `REAP r FROM math:PI.` (no argument list)
  emits `r = math_PI();`, following the `math_random` precedent.

### Regression Tests
- `math_full`: all five bindings with edge cases — `LOG` of `1`, `e`,
  `10`, `0.5`, `0` (error message) and `-5` (error message); `PI`/`E`
  constants; exhaustive `SIGN` states (`-7.5 → -1`, `0 → 0`,
  `-0.0 → 0`, `42/3.99 → 1`); `CLAMP` below, inside, and above the
  bounds, inclusive boundary hits, and a fractional in-range value.

## v0.48.26 — 2026 (Complete String Library)

### New Features
- **`strings:` FFI module.** Ten bindings expose the std/string
  library to scripts: `strings:UPPER`, `strings:LOWER`,
  `strings:TRIM`, `strings:INCLUDES`, `strings:STARTS_WITH`,
  `strings:ENDS_WITH`, `strings:REVERSE`, `strings:REPEAT`,
  `strings:PAD` (right-aligned), and `strings:PAD_LEFT`.
  `module:FUNC` calls rewrite to `module_FUNC` at codegen, so the
  bindings are thin static wrappers over the runtime.
- **Runtime implementations** (`plant_runtime.c`, std/string):
  - `string_upper` / `string_lower` — character-level case
    conversion (ASCII a–z / A–Z).
  - `string_trim` — strips leading and trailing whitespace
    (`<= ' '`), sharing `plant_string_trim` semantics.
  - `string_includes` — substring containment via `strstr`; the
    empty substring is contained.
  - `string_starts_with` / `string_ends_with` — prefix/suffix
    boundary checks; the empty prefix/suffix matches.
  - `string_reverse` — symmetric character inversion (already
    shipped, now also exposed as `strings:REVERSE`).
  - `string_repeat` — duplicates a string `count` times
    (`count <= 0` or empty input → `""`).
  - `string_pad` / `string_pad_left` — pad to a target length with
    the first character of the pad string; lengths at or below the
    input length return the input unchanged.
- **Boolean convention.** Containment/boundary functions return
  `"1"`/`"0"` strings, matching `str_eq` and the FFI truthiness
  convention.

### Regression Tests
- `string_full`: all ten `strings:` bindings with edge cases — empty
  strings and single characters, mixed-case and punctuation, all-
  whitespace and already-trimmed inputs, absent and empty
  substrings/prefixes/suffixes, zero- and one-count repetitions,
  palindromes, and padding where the target length falls below the
  input length.

### Notes
- The compiler's `--version` string (previously stuck at 0.48.22)
  now reports 0.48.26; the native suite's version check follows.

## v0.48.25 — 2026 (STOP IF + plant_calm Finalization)

### New Features
- **Conditional execution halting.** `STOP IF cond.` evaluates the
  condition exactly like an `IF` header and raises the `STOP_STORM`
  classification through the standard exception pipeline when it is
  true; a false condition passes through. A missing or empty
  condition after the directive keywords is a compile-time
  `syntax_error`. `STOP_STORM` joins the storm registry (13th kind)
  with the default message `conditional stop requested`, so a
  message-less halt is descriptive in handlers and at the top level.
- **`plant_calm` runtime finalization.** `runtime/c/plant_runtime.c`
  gains `plant_calm(w)`: the unconditional finalization point called
  after every `CALM` body. It pops the weather frame and re-raises
  any storm the shelters did not handle (`raised && !handled`),
  centralizing the unmatched-storm propagation that was previously
  emitted as generated guards. The `handled` flag moved from a C
  local (`__wmN`) into the `PlantWeather` frame struct.
- **CALM on every exit path.** The `CALM` body is now embedded in the
  threaded `mexit`/`wexit` exit lists, so `GIVE`, `BREAK`, and
  `CONTINUE` inside a protected body or a shelter still run the
  finalizer before leaving the frame (generated once with clean
  frame-pop exits to prevent re-entry, then shared by the inline
  path and the exit chains). Nested `WEATHER` hierarchies finalize
  frame by frame through the same lists.

### Regression Tests
- `stop_if`: true/false conditions, numeric conditions (`n >= 7`),
  and `STOP IF` inside `WEATHER` blocks, verifying the registry
  default message via `SHELTER STOP_STORM AS err`.
- `calm`: `CALM` runs unconditionally — normal completion, caught
  storms, unmatched storms re-propagating to an enclosing shelter —
  plus `BREAK` and `GIVE` inside `WEATHER` blocks still finalizing.
- `calm_nested`: three-level nested `WEATHER` hierarchies with
  multi-level `CALM` handlers and an empty finalization block.

### Notes
- `STOP` is now a reserved keyword; `STOP IF` is the only form.

## v0.48.23 — 2026 (WEATHER/SHELTER/CALM Storm Exceptions)

### New Features
- **Storm exceptions.** `THROW storm "msg".` raises a typed storm from
  any statement position (functions, actions, handlers, missions);
  `WEATHER, <body> SHELTER <storm> AS err, <handler> CALM, <final>
  /WEATHER.` installs exception handling. Mandatory `CALM` runs on both
  the normal and the exceptional exit path (syntax error if omitted).
- **Shelter matching.** Shelters match by string on the storm name
  (`ZERO_STORM`, `LOCK_STORM`, `MISSING_STORM`, `NETWORK_STORM`,
  `LOST_STORM`, `ANY_STORM` — the six core legacy kinds; other names are
  free-form identifiers). `SHELTER ANY_STORM` is the catch-all, and an
  untyped shelter is treated as `ANY_STORM`. `AS err` binds the storm
  message to a text variable in the handler.
- **Unmatched propagation.** If no shelter matches, `CALM` still runs,
  then the storm is re-raised so an enclosing `WEATHER` can catch it;
  a handler that itself `THROW`s escalates the storm past its own
  `CALM`. A storm that reaches the top level prints
  `[WEATHER] unhandled storm: <TYPE> <msg>` and aborts.
- **Non-local exits.** `GIVE`, `BREAK`, and `CONTINUE` inside a
  `WEATHER` pop the weathering frame via threaded exit lists, so no
  stale frame leaks to an enclosing scope.

### Runtime
- `runtime/c/plant_runtime.h/.c` gain `plant_weather_enter`,
  `plant_weather_leave`, `plant_throw`, `plant_exc_type`,
  `plant_exc_msg` over an internal storm-frame stack; `WEATHER` bodies
  compile to `setjmp`/`longjmp` regions guarded by volatile frame
  state, and shelter dispatch emits an ordered `if`/`else if` chain.

### Regression Tests
- `weather_basic`: typed shelter with `AS err`, `ANY_STORM` catch-all,
  calm, and post-`WEATHER` continuation.
- `weather_nested`: inner `WEATHER` catches its own storm; the outer
  `WEATHER` never sees it.
- `weather_unmatched`: inner block's `CALM` runs, then the storm
  re-propagates to the outer shelter.
- `weather_multi`: two `WEATHER` blocks with typed + catch-all
  shelters and distinct `CALM`s.
- `weather_empty_calm`: empty `CALM` bodies (plain `CALM, /WEATHER.`).
- `weather_missing_calm`: missing `CALM` is a compile-time error
  (`.invalid` negative test).

### Notes
- Six legacy storm kinds are recognized; the `storm()` factory, the
  remaining kinds, and location backfill remain future work.

### Patch (v0.48.23-patch) — 12-Storm Registry (additive)
- **Six new classifications.** `RANGE_STORM` (index/range bounds),
  `TYPE_STORM` (type/conversion mismatch), `PARSE_STORM` (malformed
  input), `HANDLE_STORM` (invalid resource handle), `HARVEST_STORM`
  (HTTP harvest failure), and `FALL_STORM` (requested abort) join the
  six core kinds (`ZERO_STORM`, `LOCK_STORM`, `MISSING_STORM`,
  `NETWORK_STORM`, `LOST_STORM`, `ANY_STORM`) for a cumulative 12.
- **Runtime registry.** `plant_runtime.c` maps all 12 kinds in
  `_plant_storm_registry` with per-kind default messages; `THROW X.`
  (no message) now raises the classification's default text, and
  free-form identifiers fall back to `(unclassified storm)`.
- **Runtime matcher.** `plant_storm_match(thrown, shelter)` — exact
  name equality with `ANY_STORM` as the universal catch-all —
  replaces the codegen `strcmp`/`if (1)` chains, so the routing
  branch chain is identical for all 12 kinds and free-form names.

### Regression Tests (v0.48.23-patch)
- `weather_storms12`: each of the 11 concrete kinds thrown with an
  explicit message and caught by its exact-name shelter, plus a
  free-form `GHOST_STORM` routed to the `ANY_STORM` catch-all.
- `weather_storm_defaults`: all 12 kinds plus an unknown name thrown
  without messages, verifying the registry's default messages
  (`division by zero` … `requested abort or termination`,
  `(unclassified storm)` fallback).

## v0.48.22 — 2026 (List Iteration CYCLE)

### New Features
- **List iteration.** `CYCLE item IN list, body /CYCLE.` iterates the
  elements of any list expression, and `CYCLE item, idx IN list,`
  additionally binds the element's runtime index. `parse_cycle_stmt`
  now dispatches on the header token after the loop variable: `FROM`
  (numeric range, v0.48.21), `IN` (element-only), or `,` (element +
  index, followed by `IN`); a missing `IN` raises a `syntax_error`.
- **Idiomatic C `for` loop.** List iteration compiles to
  `for (long idx = 0; idx < plant_array_length(list); idx++) {
  tx_t item = plant_list_get(list, idx); … }`. The increment lives
  in the `for` header so `CONTINUE` cannot skip it — the previous
  `while` + trailing `idx++` form looped forever on CONTINUE.
- **Correct list accessor.** Iteration reads elements with
  `plant_list_get` (the text-list accessor) instead of
  `plant_array_get` (which is the legacy int64-array reader and
  crashed on PlantArray lists).
- **Index is numeric.** The index variable is registered in the
  action's numeric-var table, so `SET t TO t + idx.` stays raw C
  `+` and `"idx=" + idx` wraps via `_from_long`.

### Regression Tests
- `cycle_list`: element-only iteration over multi-element, empty, and
  single-element lists, nested IN loops (list of lists), numeric
  summing via `_to_long`, and BREAK/CONTINUE inside the loop.
- `cycle_index`: element + index over empty/single/multi lists,
  index arithmetic, index-based branching (`idx % 2`), and
  `_to_long(item) + idx` sums.

### Notes
- Element variables are text (`tx_t`): numeric list payloads convert
  with `_to_long(item)` (or `_from_long` for output).

### Patch (v0.48.22-patch)
Shipped `INCREASE x BY n.` and `DECREASE x BY n.` (legacy gap 3.2):
- Parser: `parse_incdec_stmt` consumes the keyword, target identifier,
  and a mandatory `BY` clause (missing `BY` is a syntax error) and
  builds a dedicated `increase_stmt`/`decrease_stmt` node carrying the
  target name and evaluated expression; dispatched from
  `parse_statement`.
- Codegen: compound C assignment `x += expr;` / `x -= expr;`. The RHS
  goes through `_handle_cat` so numeric literals, negative offsets,
  zero values, and composite arithmetic (`a * 2`, `idx + 1`) keep raw
  numeric C operators. Targets must be registered numeric variables
  (in `nums` — NUM/FACT CREATE/LET targets, CYCLE FROM/TO counters,
  and list-iteration index vars); a non-numeric target is rejected
  with a compile-time `#error` line, preserving type invariants.
- Loops: works inside CYCLE bodies alongside BREAK/CONTINUE, with
  negative STEP ranges, and against list-iteration index variables.
- Two regression tests (`incdec_basic`, `incdec_loop`): 69 regression
  / 101 total. Self-host converged 274255 B; DISTCHECK OK.

### Patch (v0.48.22-patch2)
Shipped native string interpolation `"Hello ${name}"` (legacy gap 3.2,
roadmap item 3):
- Lexer: `match_string_i` supersedes `match_string` in `scan_tokens`.
  It returns a triple `[value, endpos, has_interp]` and tracks two
  states: `idepth` counts `${...}` nesting and `instr` marks quoted
  regions inside embedded expressions, so `${a + "${b}"}` and nested
  `${n + ${n}}` scan correctly. Escaped quotes and backslashes skip
  the closing-quote scan; `\n \t \r` sequences are normalized. The
  string branch now pushes `INTERP` tokens (value carries the full
  raw text including `${...}` delimiters) alongside `STRING`, and the
  plain-string push condition was fixed to `si[2] ISNT 1` — the
  runtime maps a stored `0` to `""`, so the original `== 0` test
  never fired and silently dropped every plain string literal
  (found by building the self-host chain: `v2` stripped strings,
  which corrupted the `v2→v3` stage).
- Parser: `collect_value` and `collect_until` treat `INTERP` like
  `STRING` (escape + quote-wrap); three `atype IS "STRING"` sites in
  `collect_args`/create/await argument handling accept both.
- Codegen: `_handle_cat` now first runs `_interp_to_cat`, which
  rebuilds each quoted string containing `${...}` into a nested
  `_cat(...)` chain. `_interp_expand` splits the string into literal
  and expression segments; expressions go through unescape → bare-
  `${}` expansion (`_expand_bare`, quote-aware) → nested interpolation
  → `translate_expr` → `_handle_cat`. Numerically typed results wrap
  in `_from_long(...)` (via `seg_is_numeric`), enum variables in
  `_from_enum(...)` (via `enum_expr_of`); pure-numeric expressions
  such as `${a + b}` emit `_from_long(a + b)` with raw C arithmetic.
- Error safeguards: an unterminated `${` (no closing `}`) raises a
  compile-time `#error unterminated string interpolation: missing }`
  line in the generated C, rejecting the program at compile time.
- Self-host discipline: with the stale `dist/Chloroplast` bootstrap,
  pure var+var `+` concatenation of digit-bearing identifiers emits
  raw `tx_t + tx_t` C, so new helpers use digit-free names
  (`_unescape`: `rr/ii/nn/cc/nx`; `_expand_bare`: `res/i/n/c/c2`)
  and quote a literal on concatenations (`res + c2 + ""`). Runtime
  `_cat` hardened with `if(!sa) sa=""; if(!sb) sb="";`.
- Two regression tests (`interp_basic`, `interp_composite`) covering
  foundational interpolation, numeric positive/negative/zero,
  embedded-digit identifiers (`eb2`), empty `${}`, mixed vars,
  composite arithmetic, nested expressions, `LEN`, escapes, and
  interpolation inside `SET`. 71 regression / 103 total. Self-host
  converged 284951 B; DISTCHECK OK.

### Patch (v0.48.22-patch3)
Hardened numeric `CYCLE` loops with static `STEP` evaluation (legacy
gap 3.2):
- **Static STEP evaluator.** New codegen helpers (`_is_digit`,
  `_st_num`, `_st_factor`, `_st_term`, `_st_expr`, `_step_sign`)
  constant-fold a pure-literal integer expression (`+ - * /`, parens,
  unary minus, spaces stripped) down to its sign: `+`, `-`, `0`, or
  `?` when the step is a runtime expression. The parser
  (`parse_cycle_stmt`) and the codegen both classify the resolved
  step, so interception happens before any code is emitted.
- **Zero-step compile-time error.** A statically-zero `STEP` — literal
  `STEP 0` or a folding expression like `STEP 2 - 2` / `STEP 1 * 0` —
  is rejected at parse time with `Error: #error STEP cannot be 0.`
  (exit 1); the codegen carries the same guard (`#error STEP cannot
  be 0`) as a backstop. A zero step can never compile into a `for`
  loop that spins forever.
- **Direction-aware bounds by folded sign.** The bound operator is
  chosen from the *evaluated* step sign instead of the first
  character: `STEP 0 - 2` now descends (`i >= hi`) and `STEP 1 + 1`
  ascends (`i <= hi`); plain literals behave exactly as before
  (`STEP -1` descends, `STEP 2` ascends). Previously the first-char
  heuristic sent `STEP 0 - 2` up a wrong-direction loop and `STEP
  2 * k` into a fixed ascending bound.
- **Runtime-step nonzero guard.** A runtime step expression compiles
  to a sign-aware middle clause plus a `(step != 0)` guard, so a
  zero-valued step variable skips the loop body instead of iterating
  forever; nonzero runtime steps behave identically to before.
- **Edge semantics verified.** `lo == hi` executes exactly once under
  both positive and negative steps; mismatched range/step directions
  (`1 TO 5 STEP -1`, `5 TO 1 STEP 1`) run zero times; nested
  BREAK/CONTINUE work inside descending loops.
- Three regression tests (`cycle_neg_edge`, and the negative
  `cycle_zero_step` / `cycle_zero_expr` with `.invalid` markers): 74
  regression / 106 total. Self-host converged 289912 B; DISTCHECK OK.

### Patch (v0.48.22-patch4)
Faster string concatenation and escaped interpolation markers:
- **Flattened concatenation.** The codegen previously emitted
  left-deep `_cat(_cat(a, b), c)` chains — one malloc per pair. New
  runtime helpers `_cat3`/`_cat4` (plant_compat.h) concatenate 3-4
  segments with a single allocation and copy. The codegen helper
  `_emit_cat_chain` groups segments into `_cat4` calls (the chain is
  argument 1, so each subsequent group carries 3 new parts) with
  `_cat3` for 3-part chains and `_cat` for pairs — applied both to
  `_handle_cat`'s `"a" + "b" + …` emission and to
  `_interp_expand`'s interpolation chain, so interpolation-heavy
  strings flatten too. Output text is unchanged; only the C shape
  and allocation count differ.
- **Single-digit numeric fast path.** `_from_digit` uses a static
  0-9 lookup table; `_from_long` funnels through it, and the codegen
  emits `_from_digit(...)` directly at both `_from_long` wrap sites
  when the segment is exactly one literal digit (`${0}`…`${9}`,
  `"d=" + 5`). Returns are static strings, safe because the runtime
  never writes into `tx_t`.
- **Escaped interpolation markers.** `\${` in a string is now a
  literal marker: the lexer (`match_string_i`) preserves the
  backslash and does not open a `${...}` region, and the codegen
  (`_interp_expand`'s first-`${` scan, `_expand_bare`) treats a `${`
  preceded by a backslash in the content as literal text. `"\${x}"`
  prints `\${x}`; `"a \${x} and ${1 + 2}"` prints `a \${x} and 3`.
  Note the escape is consumed at the lexer, so `\\${x}` (a literal
  backslash followed by a real interpolation) also renders as the
  literal `\${x}` — a source `\${` always wins.
- Three regression tests (`interp_flatten` — 10-part and 6-segment
  chains; `interp_fastnum` — all ten single digits plus `5 + 4`,
  `10`, and a variable; `interp_escape` — literal, mixed, doubled
  markers and `\\` before a real interpolation): 77 regression /
  109 total. Self-host converged 289982 B; DISTCHECK OK.

## v0.48.21 — 2026 (Numeric Range CYCLE)

### New Features
- **Numeric range loops.** `CYCLE i FROM lo TO hi, body /CYCLE.`
  compiles to an idiomatic C `for` loop with a `long` loop variable:
  `for (long i = lo; i <= hi; i += 1) { … }`. Loop bounds are full
  expressions (`CYCLE i FROM b TO b + 3,`), loops nest, and
  BREAK/CONTINUE work inside the body.
- **STEP clause.** An optional `STEP k` modifier sets the increment;
  the default is 1. Step sizes may be negative for reverse
  iteration (`CYCLE i FROM 5 TO 1 STEP -1,` iterates 5..1), and the
  generated bound operator flips (`i >= hi`) when the step is a
  negative literal. A runtime step expression emits a sign-aware
  bound clause so both directions behave correctly.
- **Parser wiring.** `CYCLE` was already a lexer keyword and the
  code generator had cycle_stmt support, but the parser never
  dispatched it — `parse_cycle_stmt` now parses the header
  (`iterVar`, `fromExpr`, `toExpr`, optional `stepExpr`) and the
  body, mirroring `parse_season_stmt`; a missing `FROM` raises a
  `syntax_error`.
- **Walker coverage.** `cycle_stmt` bodies are now walked by the
  implicit-declaration passes, and the FROM/TO loop variable is
  registered as a numeric scalar (in `nums`) so arithmetic on the
  loop variable (`SET s TO s + i.`) stays raw C `+` instead of
  routing to `_cat`.

### Regression Tests
- `cycle_num`: default-step ascending loops, expression bounds,
  `lo == hi`, `lo > hi` (no iterations), nested loops, BREAK,
  CONTINUE.
- `cycle_step`: custom positive steps, negative steps descending to
  and including `hi`, runtime step variables, and `lo > hi` with a
  negative step (no iterations).

### Notes
- The ` : ` ternary separator is avoided in generated code because
  `translate_expr` rewrites `" : "` to `"_"` (legacy dialect
  transform); runtime-step loops use a `&&`/`||` bound clause.

## v0.48.20 — 2026 (IF/ORIF/ELSE Chains)

### New Features
- **Multi-branch conditionals.** `IF cond, body` can now be followed by
  any number of `ORIF cond, body` arms and an optional `ELSE, body`
  fallback. The parser chains every arm into a single `if_stmt` node
  (flat `elif` list of cond/body pairs plus an `else` list), and code
  generation emits idiomatic C `if/else if/else` chains — no nested
  hoisting, no mode change, matching the classic 2015 PlantLang
  dialect documented in the gap analysis.
- **Uniform branch walking.** A new `_if_bodies` helper flattens the
  main body, every `ORIF` body, and the `ELSE` body so all
  statement-walking passes (implicit declarations, used-variable
  collection, enums, nums, async, structs, templates, stvars, callback
  uses, callee closure discovery, and closure scope walking) treat
  every branch identically; variables declared in one branch are seen
  by the others the same way they are in `season_stmt`.
- **No-else fallthrough.** An `ORIF` chain without `ELSE` simply falls
  through when no arm matches.

### Regression Tests
- `if_else`: plain IF/ELSE, nested IF inside a branch, branch-local
  `CREATE`/`SET`, string conditions via strcmp.
- `if_orif`: 2-4 arm chains with and without ELSE, string conditions on
  `ORIF` arms, and an all-false no-else chain.

### Notes
- `ORIF` tokenizes as an identifier (only `ELSE` is a lexer keyword);
  it is consumed by `parse_if_stmt` so no lexer change was needed.

### Patch (v0.48.20-patch)
Patched `_handle_cat` to correctly distinguish identifiers with digits
from numeric literals. Prevents raw C + errors in concat contexts.
`is_identifier` and `seg_has_literal_digit` now classify each
top-level `+` segment: only a genuine numeric literal (digit outside
an identifier, e.g. `1` in `p5 + 1`) keeps the raw C `+` arithmetic
shortcut that REAP'd index vars rely on; identifiers that merely
contain digits (`eb2`, `n2`) route to `_cat` instead of emitting
`void* + void*`. `seg_is_numeric` was refactored onto `is_identifier`
so identifier tokens are always verified against the numeric-var
table. Regression tests: `cat_var_digit`, `cat_var_digit_pair`,
`cat_num_literal`, `cat_mixed`.

## v0.48.19 — 2026 (Closures Syntax Repair)

### New Features
- **Parenthesis-free closure parameter lists (short form).** A closure
  header can now declare bare parameters directly in the bracket:
  `[MOVE x] -> x + 1` (one parameter), `[MOVE x y z] -> x + y + z`
  (several), and a mode-less list is accepted too (`[p q] -> p - q`).
  Grammar: `closure_param_list → IDENT+` alongside the existing
  `(IDENT, …)` form; the token following `]` selects the form — `(` is
  the long form (bracket = captures), anything else is the short form
  (bracket = parameters).
- **Unified AST.** Both syntax variants produce the identical closure
  node shape (`params` = list of `{name, type}` entries; short-form
  parameters default to `NUM` so concise closures can be used in
  arithmetic directly). Downstream code generation is untouched.
- **`Mixed closure parameter syntax` diagnostic.** Mixing the forms is
  a compile error that aborts the build with a clear message, e.g.
  `[MOVE (x y)]` (parenthesized list inside the bracket) or a bare
  multi-entry bracket followed by a parenthesized list
  (`[MOVE x y](v)`). The parser raises a `syntax_error` node that
  bubbles through statement/body/program parsing to the driver, which
  prints `Error: Mixed closure parameter syntax.` and exits 1.
- **Regression harness negative tests.** A `*.invalid` marker turns a
  regression test into a negative test: the source must fail to
  compile and the compiler log must contain every expected diagnostic
  line (dist tarball now ships the markers).

### Regression Tests
- `closure_short` — single bare parameter (`[MOVE x]`) with expression
  and block bodies.
- `closure_short_multi` — `[MOVE x y z]` and mode-less `[p q]`.
- `closure_mixed` — short form vs long form side by side: parameters
  vs captures-plus-parameters, including MOVE-consumed outer values.
- `closure_invalid` — negative test locking the `Mixed closure
  parameter syntax` diagnostic.

### Perf
- Zero syntax-parser overhead: the self-hosting compiler converges at
  257107 bytes (parser grew ~4.5 KB for the new grammar); the full
  perf suite reruns clean with no regression vs the v0.48.18 rows in
  `perf_results.md`.

## v0.48.18 — 2026 (Mission Mode PERSISTENT)

### New Features
- **`WITH MISSION PERSISTENT` binds the GlobalARCHeap** — the codegen
  emits the boundary guard plus `plant_persist_enter`/`plant_persist_exit`
  (mode-stack `P`, `MODE_ENTER PERSISTENT` audit) around the body.
- **GlobalARCHeap from scratch** — reference-counted tracked objects:
  `arc_alloc(size)` (refs=1), `arc_retain` (refs++), `arc_release`
  (refs--; finalize + free at zero), `arc_finalize` (named callbacks
  from a built-in registry, invoked at deallocation), `arc_link`/
  `arc_unlink` (reference edges with internal retains), `arc_lease`
  (keep an object alive past refs=0 — the persistent-cache path).
- **Tri-color cycle detection** — automatic every
  `PERSIST_GC_INTERVAL` (default 1000) allocations (sub-millisecond
  linear mark-sweep over the live set) plus manual `GC.cycle()`
  (`plant_arc_gc`): objects with no external references are reclaimed
  with their finalizers (`ARC_RECLAIM` accounting, `ARC_GC` events).
- **`NET_LISTEN` default capability** — PERSISTENT actions hold
  `FILE_READ`/`FILE_WRITE`/`NET_CONNECT`/`NET_LISTEN` (new
  `PLANT_CAP_NET_LISTEN`); `plant_cap_check` consults the mask while a
  PERSISTENT action is active.
- **Data-integrity gate** — objects allocated inside a SAFE context are
  tainted; `arc_persist` refuses to persist them
  (`ARC_PERSIST … blocked (untrusted SAFE data)`) so untrusted data
  cannot cross the boundary without validation.
- **BoundaryViolationError: PERSISTENT→SAFE blocked** — the boundary
  table gains `PERSISTENT→SAFE` (callee-side guard); SMART and FAST
  callers still pass everywhere. Hash-chained audit covers every
  lifecycle event (`ARC_ALLOC`/`ARC_RETAIN`/`ARC_RELEASE`/`ARC_LINK`/
  `ARC_UNLINK`/`ARC_LEASE`/`ARC_FINALIZE_REG`/`ARC_FINALIZE`/`ARC_FREE`/
  `ARC_GC`/`ARC_PERSIST`).
- **`MISSION CONFIG` keys** — `PERSIST_GC_INTERVAL` (allocation
  trigger), `PERSIST_LEASE_MS` (default lease duration).
- **Codegen fixes landed on the way** — mission-mode exit lines
  (`plant_*_exit`) are now threaded into the body generator so GIVE
  returns pop the mode stack too (previously dead code behind the
  return); zero-arg `REAP r FROM f().` parses correctly (empty parens
  no longer emitted as the invalid `f(( ))`).
- **Deferred (out of scope)** — DistributedHeap and the consistent
  hash ring.

### Regression Tests
- `persistent_cache` — a leased object survives refs=0
  (`release2=2`, `cached … leased=1`) and is reclaimed by `GC.cycle()`
  after its lease expires (`after … reclaimed=1`).
- `persistent_cycle` — manual `GC.cycle()` reclaims a 2-object cycle
  (`manual=2`); 2500 further allocations trigger 2 automatic sweeps
  (`gc_runs=3`) without touching live objects (`live=2500`).
- `persistent_finalization` — registered finalizers run on the
  refs-hit-0 path and on cycle reclamation (`finalized=1` → `2`), with
  the audit chain verified `OK`.
- `persistent_boundary` — PERSISTENT→SAFE blocked at the callee
  (`BOUNDARY PERSISTENT->SAFE blocked sa`; body never runs) while FAST
  stays reachable.
- `persistent_permissions` — `read=1 write=1 net=1 listen=1` inside
  PERSISTENT; a SAFE-created object is refused by the persist gate
  (`persist=0`, `ARC_PERSIST … blocked (untrusted SAFE data)`).

### Perf
- `persistent_bench` — 200k fully-audited ARC lifecycles (5 hash-chained
  events each, ~1M events total) with automatic cycle detection every
  1000 allocations: ~2µs per lifecycle (~400ms total), recorded against
  the BALANCED `numeric_bench` baseline in `perf_results.md`.

## v0.48.17 — 2026 (Mission Mode SMART)

### New Features
- **`WITH MISSION SMART` adaptive execution routing** — the codegen
  binds the SmartExecutionRouter at the action entry
  (`plant_smart_enter(name, size)` with `size` taken from the first
  NUM parameter; `plant_smart_exit(name)` before the tail return).
  Datasets below the scalar limit (default 1000, `SMART_SCALAR_LIMIT`)
  run **Scalar Inline** on the caller thread; datasets at/above the
  limit run **Parallel Vector Mode**.
- **Parallel Vector Mode + dynamic vec pool** — the pool is sized from
  the CPU core count (capped at 16, `SMART_POOL_CAPACITY`/`SMART_POOL_MAX`
  to pin it for deterministic runs, `SMART_CHUNK_SIZE` to control the
  partition). The router partitions the dataset into chunks and
  dispatches them round-robin across the pool workers
  (`SMART_CHUNK,<lo>-><hi>,vec<w>` events), tracking spawns, served
  chunks and the pending queue.
- **Starvation prevention** — the router monitors the queue; when
  pending chunks exceed 2x the live worker count the pool grows
  toward the hard cap (`SMART_EXPAND,<a>-><b>` events), and at the cap
  it **falls back safely to BALANCED execution** (`SMART_FALLBACK`
  event) — work always completes.
- **Cross-mode freedom** — SMART actions may invoke actions of every
  other mission mode: BALANCED, FAST, SAFE and PERSISTENT all execute
  from inside the SMART context (the mode stack marks `M`; the SAFE
  Boundary Handshake still blocks SAFE→SMART callers).
- **Broad operational defaults** — SMART actions initialize with
  `FILE_READ`, `FILE_WRITE` and `NET_CONNECT` granted (cap-check
  consults the SMART mask; `PLANT_CAP_FILE_WRITE` added).
- **Comprehensive routing audit** — every routing decision
  (`SMART_ROUTE,scalar|parallel,<name>,<size>[,workers]`), chunk
  dispatch, pool expansion, fallback and mode entry goes through the
  hash-chained audit logger; `plant_smart_status` exposes
  `workers/queue/spawns/served/expands/fallback` telemetry.

### Regression Tests
- `smart_scalar` — N=0/500/999 all route Scalar Inline: bodies run,
  the vec pool is never touched (`served=0`).
- `smart_parallel` — N=2500 with chunk size 1000 dispatches 3 chunks
  across a pinned 4-worker pool (`served=3`); the same action then
  routes scalar again for N=999 — adaptive in both directions.
- `smart_permissions` — SMART main holds `FILE_READ`/`FILE_WRITE`/
  `NET_CONNECT` (`read=1 write=1 net=1`) and invokes SAFE, FAST,
  PERSISTENT and BALANCED actions — all execute.
- `smart_audit` — a 40-chunk workload on a 2-worker pool (max 4)
  grows the pool twice then falls back to BALANCED (`expands=2
  fallback=1`), with every event recorded and the hash chain verified
  `OK`.

### Perf
- `smart_bench` — 200k routing decisions (100k Scalar Inline + 100k
  Parallel Vector Mode with chunk dispatch and per-chunk auditing,
  ~1.9M audit events) in ~1.06s on the reference box, recorded against
  the BALANCED `numeric_bench` and FAST `fast_loop` baselines in
  `perf_results.md`.

## v0.48.16 — 2026 (Mission Mode SAFE)

### New Features
- **`WITH MISSION SAFE` executes actions on the WarmProcessPool** — the
  codegen emits the boundary guard, `plant_safe_enter` (worker acquire,
  heartbeat bind, zero-permission context) and `plant_safe_channel_init`
  at the action entry, with `plant_safe_exit` (worker + channel release)
  before the tail return. Workers are in-process isolated-process
  emulations: default pool of 4, expandable to 16
  (`SAFE_POOL_CAPACITY`/`SAFE_POOL_EXPAND`).
- **Heartbeat health monitoring** — workers renew heartbeats per call;
  the pool tick (`plant_pool_tick`, run at every acquire) terminates
  workers that miss the heartbeat interval (default 5000ms,
  `SAFE_HEARTBEAT_MS`) and stay past the response window (default 10ms,
  `SAFE_HEARTBEAT_RESPONSE_MS`), then respawns them (restart counter +
  `plant_safe_status` telemetry).
- **Starvation protection** — when every worker is busy and the queue
  wait exceeds `SAFE_STARVATION_MS` (default 50ms), the pool grows
  toward the expand cap; at the cap it falls back gracefully to
  BALANCED inline execution (`fallback=1` in status) and SAFE work
  still completes.
- **SafeChannel IPC transport** — payloads up to 1MB are
  structured-cloned (deep copy, sender keeps its buffer); larger
  payloads are transferable (zero-copy adoption, `plant_safe_send_big`).
  `plant_safe_stats` reports `copies=`/`transfers=` counters;
  `SAFE_CHANNEL_THRESHOLD` (default 1048576) sets the routing cutoff.
- **BoundaryViolationError enforcement** — `plant_boundary_block` is now
  mode-aware (callee mode passed from the codegen): SAFE callers are
  blocked at FAST/SMART/PERSISTENT entries and FAST callers at SAFE
  entries, each logged as a `BOUNDARY` event (`SAFE->X blocked <name>` /
  `FAST->SAFE blocked <name>`); BALANCED calls pass everywhere.
  `WITH MISSION SMART` and `WITH MISSION PERSISTENT` now parse as
  guarded mission modes.
- **Mission-mode stack** — FAST/SAFE entries push their mode, exits pop
  it, so boundary decisions reflect the innermost active action instead
  of a sticky flag (fixes a latent v0.48.15 bug where any earlier FAST
  call would block later SAFE calls).
- **SAFE governance & zero trust** — SAFE actions start with **zero
  permissions**; only `FILE_READ`/`NET_CONNECT` can be granted, solely
  through the MissionContext (`plant_safe_grant`, `SAFE_GRANT` audit
  events); `plant_cap_check` inside SAFE consults the SAFE grants, not
  the global list. Syscall filtering blocks `execve`/`fork`/`ptrace`
  (`SYSCALL_BLOCK` events, `plant_syscall_check`).
- **Hash-chained audit logging** — every ring event carries an FNV-1a
  chain link (seq + kind + msg chained from the previous hash);
  `plant_audit_chain_verify` returns `OK` or `TAMPERED <idx>` and
  survives ring eviction (evicted chain captured); `plant_audit_chain_head`
  exposes the current head hash. `plant_audit_tamper` injects a
  deterministic fault for the integrity tests.
- **`MISSION CONFIG` keys for SAFE** — `SAFE_POOL_CAPACITY`,
  `SAFE_POOL_EXPAND`, `SAFE_HEARTBEAT_MS`, `SAFE_HEARTBEAT_RESPONSE_MS`,
  `SAFE_STARVATION_MS`, `SAFE_CHANNEL_THRESHOLD` applied via
  `plant_async_config` before `main` runs.

### Regression Tests
- `safe_isolation` — BALANCED main calls a SAFE worker twice: pool
  telemetry proves worker binding (`workers=4 spawns=4 served=2`) and
  both runs log `MODE_ENTER SAFE`.
- `safe_heartbeat` — a stalled worker (fault injection backdating its
  heartbeat) is restarted by the pool tick (`tick=1 restarts=1`) and
  the respawned worker serves the next SAFE call.
- `safe_starvation` — 1-worker pool expandable to 2: a 100ms queue wait
  grows the pool (`workers=2`); at the expand cap the second starve
  falls back to BALANCED inline (`fallback=1`) with SAFE work intact.
- `safe_security` — SAFE main calling FAST/SMART/PERSISTENT is blocked
  at each callee (`BOUNDARY SAFE->X blocked` events; bodies never run);
  SAFE→SAFE passes.
- `safe_audit` — zero-perm capability checks (`read0=0`), MissionContext
  grants (`FILE_READ`/`NET_CONNECT` only; `SHUTDOWN_ANY` denied), syscall
  filter (`execve`/`fork`/`ptrace` blocked, `read` allowed), and the
  hash chain: `chain=OK` → tamper → `chain2=TAMPERED 8`.
- `safe_channel` — small payload deep-copied (`recv` round-trips),
  1.2MB payload transferred zero-copy; `copies=1 transfers=1`.

### Perf
- `safe_pool_bench` — 200k SAFE worker calls with pool acquire, channel
  bind and release per entry (~0.5µs/call, ~1.8MB RSS), compared against
  the BALANCED `numeric_bench` baseline in `perf_results.md`.

## v0.48.15 — 2026 (Mission Mode FAST)

### New Features
- **`WITH MISSION FAST` binds an action to the bump heap** — the codegen
  emits `plant_fast_enter("name")` at the action entry, the runtime reset
  the bump cursor per call scope, and `plant_fast_alloc` serves
  alignment-rounded (`FAST_ALIGNMENT`, default 8) allocations from an
  8MB heap that doubles via realloc up to a 64MB cap
  (`FAST_HEAP_CAPACITY`/`FAST_HEAP_LIMIT` `MISSION CONFIG` keys).
- **BALANCED escalation** — when the fast heap is exhausted the runtime
  falls back to malloc once and logs `FAST_ESCALATE "WARN: Fast heap
  capacity exceeded"`; `plant_fast_escalated/used/peak/status` expose the
  heap state to PlantLang code.
- **`WITH MISSION SAFE` and the Boundary Handshake** — SAFE actions carry
  a `plant_boundary_block` guard at entry; a FAST caller reaching a SAFE
  callee is blocked (returns immediately) and the audit ring logs a
  `BOUNDARY` event. FAST also skips redundant context-magic validation
  (zero-trust: fewer checks, bounded side effects).
- **NonBlockingAuditLogger** — a lock-free single-producer audit ring
  (256 × 96B, volatile head) recording `MODE_ENTER`, `ZT_GRANT`,
  `CAP_CHECK`, `FAST_ESCALATE` and `BOUNDARY` events;
  `plant_audit_dump` returns them as `seq,kind,msg` lines with an
  `OVERFLOW n dropped` trailer.
- **Zero-trust capability defaults** — `plant_cap_check` grants only
  `FILE_READ`, `FILE_WRITE` and `NET_CONNECT` by default and logs every
  check; anything else is denied.
- **`MISSION CONFIG` keys for the fast heap** — `FAST_HEAP_CAPACITY` and
  `FAST_HEAP_LIMIT` (≥64 bytes) and `FAST_ALIGNMENT` (≥1, else 8) are
  applied via `plant_async_config` before `main` runs; when config
  statements are present a plain `ACTION main()` is emitted as
  `plant_main()` and invoked by the wrapper entry so the keys apply
  before any FAST heap use (fixes the int/tx_t main clash in generated C).
- **Fixed parser modifier bug** — `PRIORITY`, `WITH MISSION` and the
  body-separator parse only ran when an `->` return type was present
  (the whole block was nested inside the arrow branch); the block is now
  top-level in `parse_action_decl`. `PRIORITY HIGH/NORMAL/LOW` and
  `WITH MISSION FAST/SAFE` now work on arrow-less declarations too.
- **Fixed action_decl map arity** — the parser's action_decl GIVEs
  declared `plant_list_make(14, …)` while carrying 16 elements; the
  trailing `mission_mode` pair was silently dropped. All four sites now
  declare 16.

### Regression Tests
- `fast_escalation` — tiny `FAST_HEAP_CAPACITY 256` heap: 40 × 64B
  allocations overflow the capacity, grow to the 512B limit and escalate
  once to malloc; `plant_fast_escalated` reports "1" and the audit dump
  contains the `FAST_ESCALATE` warning.
- `fast_security` — FAST main calls a FAST worker (runs) and a SAFE
  worker (blocked at entry, "SAFE RAN" never prints); the audit dump
  carries the `BOUNDARY FAST->SAFE blocked safe_worker` event.
- `fast_audit` — zero-trust checks: `FILE_READ`/`NET_CONNECT` grant,
  `SHUTDOWN_ANY` deny, with `ZT_GRANT`/`CAP_CHECK` audit telemetry and
  the `MODE_ENTER` event for the FAST main.

### Perf
- `fast_loop` — 50M-iteration numeric loop in FAST mode with periodic
  bump allocations (~80ms, ~1.4MB RSS), benchmarked against the
  BALANCED `numeric_bench` baseline in `perf_results.md`.

## v0.48.14 — 2026 (Async IN Context)

### New Features
- **`START ... IN ctx` spawns a task into a named execution context** — the
  codegen routes the start through the new `plant_async_start_in`/`plant_async_in`
  wrappers (macros in `plant_compat.h` that forward to the entry's
  `(parent, ctx, …)` prefix), and `ASYNC ACTION ... IN ctx` uses the same
  mechanism. Contexts are created by the runtime (`plant_async_ctx_create`
  now takes a name) or via the FFI (`ffi_ctx_make`).
- **`AWAIT ... IN ctx` suspends into the named context** — the child is
  spawned with `parent = st, ctx = awctx` through `plant_async_await_in`, so
  both the boss and the awaited task belong to the context, and the boss
  resumes when the child completes.
- **`CANCEL ... IN ctx` cancels a handle only if it lives in the context** —
  `plant_async_cancel_in` checks the task's context membership (task magic +
  `t->ctx` match) and no-ops on mismatches, falling through to the plain
  cancel otherwise.
- **`TRACE ... IN ctx` records with the context name as the scope** —
  `plant_async_trace_in` validates the context (falling back to the default
  context) and emits `T,<ms>,<ctx-name>,<level>,<msg>` into the trace file;
  plain `TRACE` keeps an empty scope.
- **`REAP ... IN ctx` and context-aware REAP of async actions** — `REAP h
  FROM sleeper, "x" IN c.` now passes the context into the entry
  (`sleeper(0, c, "x")`), fixing a pre-existing ABI bug where reaping an
  async action emitted a plain call and lost the `__parent`/`__ctx` prefix.
- **Execution-context runtime support** — `plant_actx` carries a name;
  `plant_async_ctx_tasks` counts live tasks in a context; the parser stops
  argument collection at a top-level `IN` and parses `IN ctx` on
  `start`/`await`/`cancel`/`trace`/`reap` statements.
- **`MISSION CONFIG` accepts quoted values** — `TRACE_FILE = "/tmp/x.log"`
  parses correctly (unquoted paths broke on the `/` lexer token).

### Regression Tests
- `async_in_start` — START IN ctx + plain START: ctx task count tracks only
  ctx members (1 vs default), both workers drain.
- `async_in_await` — boss AWAITs IN ctx: count includes the suspended boss
  (2), boss resumes on completion.
- `async_in_cancel` — REAP ... IN ctx then CANCEL ... IN ctx: count drops
  after cancel, other ctx tasks unaffected.
- `async_in_trace` — TRACE IN ctx + plain TRACE read back via `ffi_read_trace`:
  `wctx,0,hello ctx` / `,0,hello plain`.

### Verification
- Self-hosted chain converged (247245 bytes); 67/67 tests pass (native 19,
  generics 7, closures 6, regression 35); compile benchmark on the
  self-hosted compiler within budget (no regression on non-ctx async paths —
  their emitted C is byte-identical).

## v0.48.13 — 2026 (Generics Numeric Returns)

### New Features
- **Generic actions can now return numeric values as text** — an action with
  an explicit `-> T` return whose instantiation substitutes `T` to `NUM` (or
  `FACT`) wraps its numeric `GIVE`s in `_from_long` at the return site, so
  reap targets display and concatenate directly without a manual `_from_long`
  at the caller. Previously the instantiated C emitted bare `long` bits into
  the `tx_t` return slot, producing garbage text for `SHOW`/`+` on reaps of
  generic actions such as `max2[NUM]`.
- **Declared return type threaded through code generation** — `generate_body`
  and `generate_node` carry the enclosing action's substituted return type
  (`rty`), and `GIVE` sites gate the numeric wrap on it: only explicit
  numeric returns get `_from_long`. Implicit-return actions (`ACTION add(a(NUM),
  b(NUM)), GIVE a+b.`) keep the raw-bit convention, so existing callers
  (`numeric_return`, `ffi.plant`, `struct_num`, `tests/generics/multi.plant`)
  are untouched.
- **Parser records plain return types** — `ACTION max2[T](a(T), b(T)) -> T.`
  now stores `"ret"` on the action_decl node (only `Result`/`STRUCT`/`ENUM`/
  `void*` returns were captured before; `-> T` and `-> NUM` were dropped),
  and template instantiations substitute it (`T`→`NUM`) via the existing
  `subst_type` machinery to drive the wrap.
- **Enum returns and numeric returns coexist** — an enum-typed generic return
  (`-> T` with `T = Color`) still uses `_from_enum`, while the `_from_long`
  wrap fires only for `NUM`/`FACT` instantiations, verified by FFI/`_to_enum`
  probes alongside the numeric path.

### Regression Tests
- `generic_num_return` — explicit `-> T` actions instantiated with `NUM`/
  `FACT`; reap targets SHOW and concatenate directly.
- `generic_num_concat` — generic NUM results inside `_cat` chains.
- `generic_num_struct` — NUM fields in `Box[NUM]` round-trip through FFI.
- `generic_num_ffi` — enum-typed generic return + ENUM FFI marshalling in
  one program, zero conflict with the numeric wrap.

## v0.48.12 — 2026 (ENUM FFI)

### New Features
- **ENUM across FFI boundaries** — external functions can now take and return
  enum values. `ACTION ffi_x() -> ENUM Color.` parses as an `external_decl`
  (new parser rule mirroring the `-> STRUCT X.` convention), so the FFI
  extension machinery, signature table, and reap handling see the real
  enum-typed signature.
- **`_to_enum` param marshalling** — every call to an external whose formal
  parameter is `ENUM Color` wraps the argument with
  `_to_enum(arg, "RED,GREEN,BLUE")`: raw member ints pass through unchanged,
  member-name strings (from enum-typed action boundaries or reap targets)
  map back to their index, unknown strings pass through untouched.
- **`_from_enum` return marshalling** — inline calls to enum-returning
  externals (`SHOW ffi_color()`, `"got=" + ffi_color()`) and their reap
  targets (`REAP c FROM ffi_color.`) are wrapped with `_from_enum`, so the
  raw member int the C side returns becomes the member-name string.
  `collect_enums` registers FFI call names (matched by the call prefix in
  `enum_expr_of`) and the walk registers reap targets against `external_decl`
  signatures (now distinguishable in `sigs` via a `type` key).
- **`PLANT_ENUM_<Name>` guard macro** — the generated types block emits
  `#define PLANT_ENUM_Color 1` next to the `typedef enum`, letting mock FFI
  libraries guard their enum functions the same way struct mocks use
  `PLANT_STRUCT_plant_X`.

### Tests
- `enum_ffi` — enum literals, enum-typed variables, enum-returning action
  results, and raw ints as FFI arguments.
- `enum_ffi_show` — inline FFI enum returns through `SHOW` and reap targets.
- `enum_ffi_concat` — FFI enum returns inside string concatenation.

## v0.48.11 — 2026 (ENUM Returns & Perf)

### New Features
- **ENUM GIVE wrapping** — `GIVE` statements now wrap enum values with
  `_from_enum` at the return site, mirroring the `SHOW` path: bare member
  constants, enum-typed variables, and struct field reads
  (`plant_map_get ( v , "field" )`) returned from actions become the member-
  name string. Callers can `SHOW`/concatenate the result directly, and
  values crossing multiple enum-typed function boundaries stay printable —
  `_from_enum` in the runtime is now idempotent (a value that is already a
  member-name string is returned unchanged; small raw ints stay on the
  index path, guarded by a plausible-pointer check).

### Performance
- **`collect_enums` fast-skip** — new `reg_has_enum` pre-check: modules
  without ENUM declarations return an empty enum table immediately instead
  of walking every action body (and per-statement `subst_type` /
  `find_ret` lookups). Modules with only STRUCT entries skip too.
- **`subst_type` fast-path** — an empty substitution map returns the type
  string unchanged without token scanning.
- **`_cl_scopes` type cache + primitive filtering** — create/let type
  resolution (`plant_ctype` + primitivity) is memoized per scope in a
  small flat cache, and the `#t` twin keys are skipped for primitive types
  (`NUM`/`TX`/`LIST`/…); closure params get the same filtering, so only
  enum/struct/generic types allocate the twin keys.

## v0.48.10 — 2026 (ENUM Structs)

### New Features
- **ENUM in STRUCT fields** — enum field values inside STRUCT instances now
  display correctly through `SHOW` and string concatenation. `build_enum_registry`
  emits `STRUCT.<Name>` entries (raw `name:type` field CSV) for every struct
  declaration, and a new `add_struct_enum_keys` resolves the struct template
  (with generic arguments via `build_subst`/`parse_type_args`) and registers
  each enum-typed field read (`plant_map_get ( var , "field" )`) in the per-action
  enum table. `collect_enums`/`collect_enums_walk` also resolve REAP targets
  against action signatures (`find_ret`), covering struct-typed parameters,
  reap targets, and generic struct instances (e.g. `Box[Color]`).
- **ENUM structs in CLOSUREs** — `_cl_scopes` now threads action signatures
  (`sigs`) so a REAP target captured by a closure records its PlantLang type
  (`#t` twin key, resolved via `find_ret`), letting `_cl_stamp_cnode` set the
  shadow `ptype` and `_cl_emit_fn` build the closure's enum table from struct
  field reads inside the closure body.
- **ENUM typedefs in types block** — enum typedefs are now emitted inside the
  `/*__PLANT_TYPES_BEGIN__*/` types block (before `PLANT_STRUCT_` defines),
  so FFI mock code and struct marshallers can reference enum members; the
  dedicated `enum_code` hoisting block was removed.

## v0.48.9 — 2026 (ENUM Closures)

### New Features
- **ENUM in CLOSUREs** — enum values inside closure bodies now display
  correctly through `SHOW` and string concatenation. `_cl_scopes` and
  `collect_closures` carry PlantLang types (`#t` twin keys) through the
  closure scope stack, `_cl_stamp_cnode` resolves each MOVE/REF capture's
  PlantLang type (`ptype`), and `_cl_emit_fn` builds a per-closure enum
  table from closure params, captured shadows, and enum CREATE/LET targets
  so `generate_body` / `_handle_cat` wrap enum refs with `_from_enum` —
  covering expr-body closures, block-form bodies, and closures nested in
  other closures or async step bodies.
- **ENUM typedef hoisting** — `generate_c` now emits enum typedefs into a
  dedicated `enum_code` block placed before the closure definitions, so
  enum member constants used inside closure bodies resolve in C.

## v0.48.8 — 2026 (ENUM Async)

### New Features
- **ENUM in ASYNC ACTIONs** — enum parameters and enum locals inside
  asynchronous actions now display correctly through `SHOW` and string
  concatenation. The async branch of `generate_node` builds the per-action
  enum table (`collect_enums`) from the module registry, and
  `async_emit_step` / `async_argstr` thread it through the phase-body
  generation (`generate_body`), the AWAIT/START argument serializer, and the
  state-machine step function. Enum values cross WAIT phase boundaries via
  the async state struct (stored as `tx_t`) and are wrapped with
  `_from_enum` on restore/display, including AWAIT/START argument
  pass-through to child async actions.

## v0.48.7 — 2026 (ENUM Generics)

### New Features
- **ENUM as generic type argument** — an ENUM can now be passed as a generic
  type parameter (`ACTION foo[T](v(T))` with `foo[Color]`). The enum table
  (`collect_enums`) now substitutes generic type parameters via
  `subst_type` before resolving member lists, and the monomorphization
  emitter (`emit_inst`) builds the per-instantiation enum table from the
  module enum registry and threads it through `generate_body`, so `SHOW` and
  string-concatenation of generic ENUM params (including `CREATE x(T)`
  locals, multi-param `foo[T, U]`, and nested generic call chains) display
  member names through `_from_enum`. Non-enum instantiations
  (`ped[TX]/[NUM]/[FACT]`) are unaffected.

### Bug Fixes
- `collect_enums` ignored generic substitution: params typed `T` were looked
  up raw in the registry, so generic ENUM display silently fell back to the
  broken raw-int path.

## v0.48.6 — 2026 (ENUM Display Repair)

### Bug Fixes
- **ENUM member display** — `SHOW` and string-concatenation of an ENUM-typed
  variable now print the member name (`GREEN`, `"suit=SPADES"`) instead of the
  raw enum-int stored in the `tx_t` slot (which printed garbage / crashed on
  the `plant_print` pointer path). The codegen threads a per-action enum table
  (`collect_enums`) built from the module enum registry
  (`build_enum_registry`) and wraps enum refs at SHOW/concat sites with the
  runtime helper `_from_enum(var, "RED,GREEN,BLUE")`. Bare enum member
  constants, enum-typed params, CREATE/LET targets and refs inside
  IF/SEASON/CYCLE/MATCH bodies are covered; async/closure/generic bodies are
  unchanged (deferred).

## v0.48.4 — 2026 (FFI Extensions — Full Signature Space)

### New Features
- **FFI-extension codegen** — `-> external` declarations now emit correct,
  compilable C for every signature shape: struct-by-value parameters
  (`STRUCT X`) convert call-site maps via `plant_map_to_X`; `REF STRUCT`
  params pass `plant_map_to_ref_X` (heap copy); struct-valued returns come
  back as maps (`plant_plant_X_to_map(...)`); `void*` params/returns pass
  through untouched; varargs externals (`..., ...`) forward `tx_t` args with
  string detection; `CALLBACK` params bind PlantLang actions through
  `plant_cb_ensure(tag, plant_cbw_<name>)` with auto-generated adapters
  (`plant_cbw_<name>`) that handle 2-param, 1-param and 0-param action
  arities and NUM/FACT `(intptr_t)` casts
- **Struct conversion helpers** — `plant_map_to_X` / `plant_map_to_ref_X` /
  `plant_struct_alloc_copy_X` / `plant_X_to_map` are emitted for every
  reachable struct (including generic instantiations) in topological
  dependency order (`ffi_topo_order` / `ffi_topo_emit_helpers`), so nested
  struct fields reference each other's helpers; a depth guard
  (`depth > 3`) protects against cyclic struct conversions
- **Types header block** — generated C carries a
  `/*__PLANT_TYPES_BEGIN__*/ … /*__PLANT_TYPES_END__*/` block (guarded by
  `PLANT_TYPES_INCLUDED`) containing struct typedefs plus FFI-extension
  prototypes; the native harness extracts it with `sed '1d;$d'` and
  force-includes it next to the mock, so plain externals keep the
  hand-written `mock_ffi.h` prototypes (mock-specific C types like
  `long` for `Result<NUM,TX>` stay authoritative)
- **Bare call statements** — `plant_map_set(p, "x", 3).` as a statement
  (previously unrecognized and silently dropped) now parses via
  `parse_call_stmt` and routes through the generic `reap_stmt` codegen
  (`target="_"`), including generic `[T]` calls, closure args and string
  escaping

### Verification
- Native suite extended with `tests/native/ffi_ext.plant` covering
  struct-by-value/ref params, struct returns, `void*`, varargs, CALLBACK
  adapters (`plant_cb_ensure` + `plant_cbw_`), nested struct maps and bare
  `call_stmt` — suite green **19/19** with both `build/plantc_v2` and the
  final self-hosted `bin/Chloroplast`
- Full self-hosting chain converged (`make self`: v3 = v4 = v5,
  217217 bytes); ASAN-clean runs of the extension scenarios (only benign
  runtime arena leaks)

## v0.48.3a — 2026 (Post-Async Polish — Auto Concat & Async Drain)

### New Features
- **Automatic string + number concat (v0.48.3a)** — `"x=" + i` where `i` is a
  NUM/FACT variable now compiles to a string concatenation
  (`_cat("x=", _from_long(i))`) instead of breaking `+` arithmetic. The codegen
  tracks every numeric variable in scope (params, creates/lets, async state
  fields, closure captures) via `collect_nums`/`nums_from_avars`/
  `collect_nums_cb` and classifies each `+` expression: pure-numeric
  expressions (e.g. `sum + i`, `7 + 2`) stay plain C arithmetic, while
  string mixes (e.g. `"v: " + i + "!"`, `"len " + LEN(msg)`) wrap numeric
  segments with `_from_long` per segment — including numeric calls
  (`LEN`→`strlen`, `COUNT`→`plant_array_length`, `_to_long`) and parenthesized
  sub-expressions `(x + 1)`
- **Async drain for top-level `main`** — when `ACTION main` spawns async work
  (`START`, `AWAIT`, `ASYNC IN`) anywhere in its call graph, the generated
  `main` ends with `plant_async_drain();` so all workers run to completion
  before the program exits. The compiler computes `async_reachable(ast)` —
  a BFS over `main`'s callees (through `REAP`/`START`/`AWAIT` action names
  and `START`-values inside set/create/let/give/show/put/cancel/trace/config
  statements, recursing into IF/SEASON/CYCLE/MATCH bodies). Actions that
  finish with an explicit `GIVE` skip the drain (unreachable by design)
- **Perf suite** — `make perf` compiles and runs `tests/perf/` benchmarks
  (`perf_concat` string building, `perf_async` 20 cooperative workers with
  chained `AWAIT`, `perf_mixed` concat-in-async) and auto-writes
  `perf_results.md` with real time, peak RSS and CPU ticks

### Verification
- Native suite extended: `tests/native/concat.plant` now covers bare-numeric
  concat, multi-segment mixes, numeric sub-expressions, `LEN` results and
  numeric-only arithmetic guards — **18/18**
- Full suite green: native **18/18**, generics **7/7**, closures **6/6**
- Drain verified for: direct `START`, transitive (helper action calling
  `START`), `START`-in-expression, `ASYNC IN`, and negative (no-async)
  cases; `perf_async` trace (`PLANT_TRACE=1`) shows all 20 workers spawn,
  suspend on `AWAIT phase2`, resume and complete during `plant_async_drain()`
- ASAN-clean crash-fixing cycle: `collect_nums_cb` expr-body overflow and
  the `strlen(`-prefix off-by-one in `seg_is_numeric` both fixed under ASAN

## v0.48.3 — 2026 (Async Engine — Cooperative Tasks & AWAIT)

### New Features
- **`ASYNC ACTION`** — cooperative concurrency: every async action lowers to a
  C state machine (state struct, `plant_async_register` entry wrapper, step
  function with a `switch (s->__pc)`/label resume protocol) with **zero**
  thread usage — single-threaded cooperative scheduling in the native runtime
- **`AWAIT child, args.`** — suspends the current task and resumes it when
  the awaited task finishes (`plant_async_suspend` /
  `plant_async_await_result`); results transfer via the task's `res` slot
- **`START worker, args.`** — spawns a task without waiting (fire-and-forget)
- **`ASYNC IN` context** — declare and start async tasks from a block form
- **Async engine runtime** (`plant_runtime.c`, +~760 lines) — task registry
  with ids, ready queue, priorities/deadlines/timeouts with inheritance,
  active contexts (`plant_actx`/`g_dctx`), timers, `plant_ms()`/`plant_msleep`,
  metrics sampling, and a `PLANT_TRACE=1` spawn/completion trace for
  verification and debugging
- **Async vars** — local state is hoisted into the state struct (`async_var_add`
  /`async_walk_decl`), including variables captured across `AWAIT` points

### Verification
- Self-hosting chain still converges byte-identically with the async engine
  active (v3 ≡ v4 ≡ v5)
- Full test suite green with the async engine shipped (native/generics/
  closures suites unchanged and passing)
- Trace-verified scenarios: 20-worker fan-out with chained `AWAIT`,
  suspension/resume across phases, task completion accounting


### New Features
- **Closures** — `CREATE f TO [MOVE x, REF y](v(NUM)) -> v + x + y.` declares
  an anonymous function with an explicit capture list; `MOVE` copies the
  variable's value into the closure's private environment, `REF` borrows a
  pointer to the outer variable
- **Env structs** — every closure lowers to a concrete C struct
  (`typedef struct { … } plant_Env_N;`), heap-allocated at the create site
  (`plant_env_alloc`), plus a plain native function
  `tx_t plant_Closure_N_fn(tx_t env, …)` — zero runtime dispatch, no
  closures-in-tables, no allocation in the hot path
- **Invocation** — `REAP r FROM f, args.` resolves closure variables defined
  in the same ACTION scope and emits `plant_Closure_N_fn((tx_t)f, …)`;
  invocable from nested `IF`/`SEASON`/`CYCLE` bodies
- **Block-form bodies** — `-> ( stmts )` runs full statements (CREATE/SHOW/
  SET/REAP/…), including closures nested inside closures
- **MOVE semantics** — the outer variable is cleared (`= 0`) at create time
  (consumed), while the closure keeps its own private, mutable copy across
  invocations; `REF` semantics — mutations to the outer variable after
  creation are visible inside the closure (pointer tracking)
- **Mixed/string captures** — NUM/FACT/LIST/TX captures all supported;
  string closures `-> "s:" + s + ":" + …` work out of the box
- **Closure test suite** — `tests/closures/` (6 cases): MOVE value capture +
  state persistence + outer-var clearing, REF pointer tracking, mixed
  multi-capture closures, nested closures, block-form bodies, invocation
  from SEASON/IF bodies — plus `.grep` structural checks on the emitted C
  (env typedefs, `plant_env_alloc`, capture assignment, `&y` borrows)

### Verification
- Self-hosting convergence from a clean tree:
  `make clean && make all && make self` → **SELF-HOSTING CONVERGED** —
  v3 ≡ v4 ≡ v5 byte-identical with the closures engine active
- Full test suite: native **18/18**, generics **7/7**, closures **6/6**
- Generated-C inspection confirms: env typedefs and closure fn forwards are
  emitted before the ACTION definitions, capture assignments (`__env_N->x`),
  MOVE clears (`x = 0;`), REF borrows (`&y`), and shadow reads
  (`((plant_Env_N*)env)->x`)

### Documentation
- `TECHNICAL.md` §32 — Closures Engine architecture (parser rules, capture
  resolution, pass-0d discovery/stamping, env struct + fn emission,
  MOVE/REF semantics)
- `README.md`, `Language Tour.md` — v0.48.2 metadata and closures examples

### Notes
- Version bumped to v0.48.2 (CLI, package.json, Makefile tarball)
- Closure variables are `tx_t` env pointers and are only invocable in the
  ACTION body (and nested block bodies) where they were defined; passing a
  closure into another ACTION is possible but it cannot be invoked there
- `collect_value` now stops at a `)` at depth 0 (enclosing closure block
  terminator); `_handle_cat`'s numeric-literal detection was fixed
  (`find_any` instead of pointer-range compares) — both fix latent bugs
  exposed by closure block bodies

## v0.48.2 — 2026 (Closures Engine — Env Structs & Native Functions)

### New Features
- **Closures** — `CREATE f TO [MOVE x, REF y](v(NUM)) -> v + x + y.` declares
  an anonymous function with an explicit capture list; `MOVE` copies the
  variable's value into the closure's private environment, `REF` borrows a
  pointer to the outer variable
- **Env structs** — every closure lowers to a concrete C struct
  (`typedef struct { … } plant_Env_N;`), heap-allocated at the create site
  (`plant_env_alloc`), plus a plain native function
  `tx_t plant_Closure_N_fn(tx_t env, …)` — zero runtime dispatch, no
  closures-in-tables, no allocation in the hot path
- **Invocation** — `REAP r FROM f, args.` resolves closure variables defined
  in the same ACTION scope and emits `plant_Closure_N_fn((tx_t)f, …)`;
  invocable from nested `IF`/`SEASON`/`CYCLE` bodies
- **Block-form bodies** — `-> ( stmts )` runs full statements (CREATE/SHOW/
  SET/REAP/…), including closures nested inside closures
- **MOVE semantics** — the outer variable is cleared (`= 0`) at create time
  (consumed), while the closure keeps its own private, mutable copy across
  invocations; `REF` semantics — mutations to the outer variable after
  creation are visible inside the closure (pointer tracking)
- **Mixed/string captures** — NUM/FACT/LIST/TX captures all supported;
  string closures `-> "s:" + s + ":" + …` work out of the box
- **Closure test suite** — `tests/closures/` (6 cases): MOVE value capture +
  state persistence + outer-var clearing, REF pointer tracking, mixed
  multi-capture closures, nested closures, block-form bodies, invocation
  from SEASON/IF bodies — plus `.grep` structural checks on the emitted C
  (env typedefs, `plant_env_alloc`, capture assignment, `&y` borrows)

## v0.48.1 — 2026 (Generics Engine — Monomorphization & Name Mangling)

### New Features
- **Generic actions** — `ACTION name[T, U](...)` declares type-parameter
  lists in square brackets right after the action name; parameters may use
  `T`, `LIST[T]`, `REF T`, `REF LIST[T]` etc. as types
- **Generic invocation** — call sites pass concrete type arguments:
  `REAP r FROM process_list[NUM], item.`; multi-type args `[T, U]` supported
- **Zero-cost monomorphization** — every concrete instantiation is cloned at
  codegen time into plain C; no runtime dispatch, no type tables, no overhead
- **Name mangling** — instantiations get unique C identifiers
  `plant_<name>_<T1>_<T2>` (`ACTION compute[T]` → `plant_compute_NUM`,
  `plant_compute_TX`), collision-free against runtime and user symbols
- **Instantiation cache** — the compiler maintains a per-build cache of
  emitted instantiations (keyed by base name + substituted type args), so
  repeated calls and nested generic-in-generic calls emit each instance
  exactly once; the cache is rebuilt from scratch on every compile run
- **Nested generics** — generic actions may call other generic actions with
  their own type parameters; substitution propagates
  (`outer[TX]` → `inner[T]` → `inner[TX]`)
- **Generic test suite** — `tests/generics/` (5 cases): basic single-param
  generics with multiple instantiations, multi-type params + `LIST[U]`
  container params, nested instantiations + cache reuse, `REF T`
  pass-by-reference params; wired into `make test`
- **Generic structs** — `STRUCT Name[T, U] { field: T, ... }` declares data
  templates with type-parameter lists; the monomorphization engine emits
  concrete C typedefs (`plant_Box_NUM`, `plant_Pair_NUM_TX`) for every
  instantiation reachable from action parameters, `CREATE`/`LET` variable
  types, top-level statements and other structs' fields. Nested generic
  structs (`Wrap[T] { box: Box[T], tag: TX }`) and multi-type params are
  supported; uninstantiated templates emit nothing (no phantom typedefs)

### Verification
- `make test` green — native suite **18/18** + generics suite **7/7**
  (incl. `structs` — non-generic STRUCT + FFI round-trip, and `gstruct` —
  generic `Box[T]`, multi-type `Pair[T, U]`, nested `Wrap[T]` with
  generated-C structural checks via `.grep` files)
- Self-hosting convergence from a clean tree:
  `make clean && make all && make self` → **SELF-HOSTING CONVERGED** —
  v3 ≡ v4 ≡ v5 byte-identical with the generics engine active
- Generated-C inspection confirms cache deduplication: repeated and nested
  instantiations emit exactly one forward declaration and one definition

### Documentation
- `TECHNICAL.md` §31 — Generics Engine architecture (parser rules,
  monomorphization pipeline, mangling scheme, instantiation cache,
  generic-struct discovery and typedef emission)
- `Language Tour.md`, `README.md`, `docs/BUILD.md` — v0.48.1 metadata and
  generics examples

### Notes
- Version bumped to v0.48.1 (CLI, package.json, Makefile tarball)
- `STRUCT` supersedes the old `SHAPE` doc sketch in `tokens.plant`; struct
  values are opaque `tx_t` at runtime — the emitted C typedefs
  (`plant_Point`, `plant_Box_NUM`, …) exist for the C FFI side to cast and
  read the concrete layout
- `T` in `CREATE x(T)` inside a generic body resolves to the instantiated C
  type; type safety is enforced by monomorphization (C compile errors surface
  for incompatible instantiations)


## v0.47.4 — 2026 (Integration, Convergence & Release)

### Verification
- Full native integration suite (`make test`) green at **18/18** — CLI checks,
  compile+run+diff cases, standard libraries (`json`, `strings2`, `fs`, `math`,
  `time`), native data structures (`std_set`, `std_queue`, `std_stack` with
  2000/3000-op stress workloads) and the advanced FFI suite (`ffi`)
- Self-hosting convergence re-verified from a clean tree:
  `make clean && make all && make self` → **SELF-HOSTING CONVERGED
  (72 756 bytes)** — v3 ≡ v4 ≡ v5 byte-identical, with the REF / Result /
  diagnostics FFI features active in every generation
- FFI memory-lifecycle validation under AddressSanitizer + LeakSanitizer:
  `ffi_make_buf`'s allocation is released by `ffi_free` (no leak recorded);
  the only residual allocations are the runtime's design-convention string
  heap (`_cat`/`_from_long`/`ffi_last_error_msg` strdup results — caller-owned,
  freed at process exit like all generated programs)

### Documentation
- `Language Tour.md` — practical examples for std/json, std/string, std/fs,
  std/math, std/time, Set/Queue/Stack, and the advanced FFI workflow
  (REF swap, `-> Result<T, E>` error paths, `ffi_free` lifecycle)
- `TECHNICAL.md` §30 — FFI architecture (signature pre-pass, REF rewriting,
  Result<T,E> parsing, diagnostics) + memory management notes
- `README.md`, `docs/BUILD.md` — v0.47.4 metadata and FFI/test-suite entries

### Notes
- Version bumped to v0.47.4 (CLI, package.json, Makefile tarball)
- Known scope limit: string-heap allocations are freed at process exit by
  design (script semantics); explicit lifecycle (`ffi_free`) covers
  FFI-returned heap pointers

## v0.47.3 — 2026 (Advanced FFI: REF, Result<T,E>, diagnostics)

### New: Advanced Native C FFI (compiler + runtime + tests)
- **`REF var` pass-by-reference** — `ACTION ffi_swap(a(REF NUM), b(REF NUM)) ->
  external.` declares pointer parameters; the self-hosted compiler records a
  signature table for every `external_decl`/`action_decl` and emits `&var` at
  call sites automatically (`REAP _ FROM ffi_swap, a, b.` → `ffi_swap(&a, &b)`)
  - Works for NUM/FACT/LIST/TX (`long*`/`int*`/`PlantArray**`/`tx_t*`); REF
    params on regular `ACTION`s compile to the same pointer ABI
- **`-> Result<T, E>` returns** — `ACTION ffi_open(path(TX)) -> Result<NUM, TX>.`
  is parsed (generics `<T, E>` consumed) and treated as an external declaration;
  the C ABI (value on success, error sentinel + `errno` on failure) lives in
  `plant_compat.h` / the linked C library
- **Diagnostics** — `ffi_last_error()` (errno from the last FFI call) and
  `ffi_last_error_msg()` (`dlerror()` first, then `strerror(errno)`); the
  runtime links `-ldl` for loader errors
- **Memory lifecycle** — `ffi_free(ptr)` frees FFI-returned pointers and rejects
  NULL with `errno=EINVAL` so error-checking code stays uniform
- Compiler changes: `parse_action_decl` accepts `(REF TYPE)` params and
  `-> Result<T, E>`; `generate_c` builds a signature pre-pass threaded through
  `generate_node`/`generate_body`/`reap_stmt`; `external_decl` nodes no longer
  pollute the generated `main`; new `plant_ctype`/`find_params`/`is_ref_at`
  helpers

### Tests
- Mock C library (`tests/native/mock_ffi.c` + `mock_ffi.h`, force-included by
  the runner via `-include`) exporting: `ffi_add` (plain TX), `ffi_swap_ref`
  (REF swap), `ffi_make_buf` (malloc'd buffer), `ffi_open_mock` /
  `ffi_parse_cfg` (`Result<NUM,TX>`-style success/failure via errno)
- New `ffi` suite: swap-through-pointers, errno/errmsg on ENOENT + EINVAL,
  errno cleared on success, buffer length, `ffi_free` ok + NULL rejection
- Suite at **18/18 passing**; self-hosting fixed point 72 756 bytes

### Notes
- The C ABI signatures for external functions live in `plant_compat.h`
  (toolchain FFI manifest); REF/Result *declarations* in `.plant` source drive
  the compiler's call-site rewriting
- Version bumped to v0.47.3 (CLI, package.json, Makefile tarball)

## v0.47.2 — 2026 (Native Data Structures & Stress Tests)

### New: Native Data Structures (pure native C in `plant_runtime.c` + `plant_compat.h`)
- **Set** (unique unordered collection — open-addressing hash table with
  splitmix hashing, in-place growth, tombstones):
  `set_create`, `set_add` ("1" added / "0" already present), `set_has`,
  `set_remove`, `set_size`, `set_to_list` (→ native LIST for iteration/export)
  - Uniqueness is by raw value identity (NUM bits / pointer), so NUM, TX,
    MAP and LIST values are all supported; value 0/NULL (nil) is reserved
- **Queue** (FIFO ring buffer with amortized-O(1) push/pop):
  `queue_create`, `queue_push`, `queue_pop` (front value, safe on empty),
  `queue_peek`, `queue_size`
- **Stack** (LIFO dynamic array):
  `stack_create`, `stack_push`, `stack_pop` (top value, safe on empty),
  `stack_peek`, `stack_size`
- Manual malloc/free memory management in pure C: internal buffers grow via
  `realloc`/`calloc` and never leak per-operation; all handles remain valid
  across growth (in-place rehash fixes a stale-handle crash found by tests)

### Tests
- New native suites: `std_set`, `std_queue`, `std_stack`
- Stress scenarios: 2000 set insertions + 2000 duplicate insertions + 1000
  deletions (size/has checks before/after), 3000 queue pushes → 3000 pops with
  FIFO-integrity verification, 3000 stack pushes → 3000 pops with LIFO
  integrity — plus empty-pop/peek safety checks (no crashes)
- Suite at **17/17 passing**; self-hosting fixed point re-verified after the
  v0.47.2 version bump

### Notes
- The self-hosted compiler subset uses `SEASON` (while) for iteration — the
  `CYCLE` statement is tokenized and code-generated for but not yet parsed;
  stress loops are written accordingly
- Version bumped to v0.47.2 (CLI, package.json, Makefile tarball)

## v0.47.1 — 2026 (Core Standard Library — Phase 1)

### New: std/* Modules (pure native C in `plant_runtime.c` + `plant_compat.h`)
- **`std/json`** — `json_parse(str)`: JSON → native MAP (pair-list `PlantArray`)
  / LIST / scalar TX structures; **invalid JSON never crashes** — returns a
  safe `NULL` nil for programmatic error handling. `json_stringify(val)`:
  MAP/LIST/scalars → standard JSON text (handles `\uXXXX` escapes, surrogate
  pairs, nested objects/arrays; raw pair-list MAPs also accepted)
- **`std/json` accessors** — `json_get(obj, key)`, `json_at(arr, idx)`,
  `json_len(val)`, `json_kind(val)` (0=null 1=bool 2=num 3=str 4=arr 5=obj),
  `json_val(val)`
- **`std/string`** — `string_repeat(str, count)`, `string_reverse(str)`,
  `string_pad(str, length, pad_char)`
- **`std/fs`** — `file_copy(src, dest)` / `file_move(src, dest)` (rename with
  copy+unlink fallback), `file_stat(path)` → MAP with `size`, `mtime`, `mode`
- **`std/math`** — direct C `math.h` wrappers returning decimal TX:
  `math_sin`, `math_cos`, `math_sqrt` (negative → "0"), `math_pow`,
  `math_floor`, `math_ceil`, `math_round`, `math_min`, `math_max`,
  `math_random` (uniform [0,1))
- **`std/time`** — `time_now()` (epoch seconds), `time_format(t, format)` /
  `time_parse(str, format)` (strftime/strptime), `time_sleep(seconds)`
  (fractional seconds via `nanosleep`)
- All modules are pure native C: signatures in `plant_compat.h`,
  implementations in `plant_runtime.c`, linked with `-lm`

### Fixed (self-hosting compiler)
- `parse_reap_stmt`: string arguments were quoted without re-escaping
  (`"\"" + lx + "\""` without `escape_string`) — REAP args containing `"` or
  `\` produced invalid C. Now escaped like `collect_value`/`collect_until`.

### Tests
- New native suites: `json` (valid + invalid/nil + unicode + stringify
  round-trip), `strings2`, `fs` (copy/move/stat/error paths), `math`, `time`
- Test runner now links with `-lm`; suite at **14/14 passing**
- Self-hosting fixed point: 69 710 bytes (`v3 == v4 == v5`)
- `make dist` DISTCHECK OK → `release/plantlang-0.47.1.tar.gz`

## v0.46.4 — 2026 (Pure Native & Chloroplast Transition)

### Breaking: Pure Native Architecture (total JS purge)
- **`core/interpreter.js` deleted** — the legacy JavaScript interpreter is gone;
  PlantLang is now 100% pure native: source → C → native executable
- **Legacy JS test suites removed** — all `tests/*.test.js`, `tests/test_*.js`,
  `tests/llvm/`, `tests/parity/` deleted after review (every one exercised the
  purged JS engine); `make test-js` target removed
- **Legacy JS CLI/drivers removed** — `src/cli/plantc.js`, root `chloroplast.js`,
  `trace_parse.js` deleted
- `package.json` reduced to pure metadata (`name: chloroplast`, `version: 0.46.4`,
  MIT) — no scripts, no devDependencies, no build hooks

### Changed: Rebranding plantc → Chloroplast
- Compiler binary renamed to **Chloroplast**: `bin/Chloroplast`,
  `dist/Chloroplast` (v1 bootstrap), `make install` → `$PREFIX/bin/Chloroplast`
- CLI identity: `usage: Chloroplast <source.plant> [out.c]`,
  `--version` → `Chloroplast 0.46.4 (pure native)`
- Compiler sources (`src/plantc/*.plant`) re-bootstrapped; new fixed point
  `v3 == v4 == v5` at 69 668 bytes of generated C
- Makefile: `VERSION ?= 0.46.4`; dist tarball `release/plantlang-0.46.4.tar.gz`
- Docs: README rewritten for programmers from other languages ("Pure Native 🚀"
  badge), ROADMAP marks v0.46.4 completed + details future milestones
  (v0.47.0 → v1.0.0), TECHNICAL.md / Language Tour.md re-aligned,
  `PHASE4_COMPLETED.md` added

### Verified
- `make all` / `make self` / `make test` (9/9) / `make dist` (DISTCHECK OK) /
  `make install` — all green with the rebranded binary

## v0.45.1 — 2026

### New: Native Self-Hosting Toolchain (Makefile)
- **Makefile rewrite** — native `make` toolchain with `.DEFAULT_GOAL`:
  `all`, `self`, `test`, `test-js`, `fmt`, `lint`, `dist`, `install`, `help`, `clean`
- **`make all`** — full native build from a clean tree: `dist/plantc` (v1 bootstrap) →
  `build/plantc_v2` → `build/plantc_v3` → `bin/plantc`
- **`make self`** — multi-generation self-hosting with a byte-convergence check
  (`plantc_v3.c == plantc_v4.c == plantc_v5.c`); fixed point currently 69 659 bytes
- **`make test`** — native integration suite (`tests/native/`): CLI checks plus
  compile + gcc + run + output-diff cases (hello, join + `strings:LENGTH`,
  number concatenation, string escapes, list ops, module calls) — 9/9 passing
- **`make fmt` / `make lint`** — clang-format / cppcheck on generated C, skip
  gracefully when the tools are missing
- **`make dist`** — versioned tarball `release/plantlang-<VERSION>.tar.gz` with
  `DISTCHECK` validation (unpack → `make all` → `make test` inside the tarball)
- **`make install`** — installs `bin/plantc` + C runtime headers to `$PREFIX`
  (default `~/.local`) and verifies `plantc --version`
- **CLI** — `plantc --help` / `--version` in `src/plantc/main.plant`; missing
  input file now exits nonzero (driver compares `fs:EXISTS` result with
  `ISNT "1"` — `NOT` on a string flag is invalid under C semantics)
- **Docs** — `PHASE3_COMPLETED.md`, `docs/BUILD.md`, README quick-start section

### Fixed (self-hosting compiler)
- `_handle_cat`: quote-toggle bug on `"\"" + x + "\""` patterns (escape-skip fix)
- `_handle_cat`: pure-digit concat parts wrapped in `_from_long(...)`
- `_handle_func_paren`: two-pass split for spaced `kw (` form (`LEN ( x )`)
- `put_stmt` → `plant_list_push(target, item)`; `create_stmt` LIST → `PlantArray*`
- `translate_expr`: ` : ` module separators → `_` (`strings:LENGTH(r)` in expressions)
- Action trailing-return parity; `(COUNT x)` parenthesization fix

## v0.45.0 — 2026

### New: Self-Hosting Compiler Pipeline (Stage 0)
- **`src/plantc/main.plant`** — Stage 0 compiler driver written in PlantLang:
  - Reads source file → tokenizes via `scan_tokens` → parses via `parse_program` → generates C via `generate_c` → writes output
  - File I/O via `PLANT fs` (`fs:EXISTS`, `fs:READ`, `fs:WRITE`) and `PLANT strings` (`strings:REPLACE`, `strings:LENGTH`)
  - Runs as: `node chloroplast.js src/plantc/main.plant myapp.plant`
- **`get_cli_arg` FFI** (`core/interpreter.js`):
  - Registered in `_registerStdStubs` as `get_cli_arg` returning Nth CLI argument
  - Linked to `process.argv` via `cliArgs` parameter from `chloroplast.js`
- **Pipeline Integration**:
  - `chloroplast.js` passes `cliArgs` array to interpreter constructor
  - All 34/34 npm tests pass with zero regressions

## v0.44.0 — 2026

### New: Algebraic Safety Types, Exhaustive MATCH, String Interpolation, Ranges, Slicing & Destructuring
- **Algebraic Safety Types** (`core/ast.js`, `core/interpreter.js`, `runtime/c/plant_runtime.{c,h}`):
  - Built-in `Option<T>` type with `Option.Some(value)` and `Option.None` constructors
  - Built-in `Result<T, E>` type with `Result.Ok(value)` and `Result.Err(error)` constructors
  - `PlantTagged` C struct with tag/kind/payload fields for dual-engine parity
  - C helpers: `plant_option_some/none`, `plant_result_ok/err`, `plant_is_some/none/ok/err`, `plant_unwrap`, `plant_unwrap_err`
- **Exhaustiveness Checker** (`src/compiler/exhaustiveness_checker.js`):
  - Static AST pass that verifies all `MatchStatement`/`MatchExpr` patterns cover every CHOICE variant
  - Checks Option (Some + None), Result (Ok + Err), and any user-defined CHOICE
  - Accepts wildcard `_` as catch-all
  - Emits `CompileError: Non-exhaustive MATCH statement. Missing case: <VariantName>` on incomplete coverage
- **String Interpolation** (`core/tokenizer.js`, `core/parser.js`, `core/interpreter.js`):
  - Tokenizer recognizes `{expr}` inside double-quoted strings with `\{`/`\}` escape support
  - `InterpolatedStringNode` with text/expression segment array
  - Expression segments are recursively parsed and evaluated
  - Lowered at interpret time into concatenated values
- **Range Operator `a..b`** (`core/parser.js`, `runtime/c/plant_runtime.{c,h}`):
  - `RangeExpressionNode` in AST with start/end sub-expressions
  - Evaluates to `[start, start+1, ..., end-1]` integer array
  - C runtime: `plant_range(start, end)` with `plant_array_create`/`plant_array_set`
- **Slicing Syntax `expr[start:end]`** (`core/parser.js`, `runtime/c/plant_runtime.{c,h}`):
  - `SliceExpressionNode` in AST with target, optional start, optional end
  - `arr[1:4]` — explicit bounds; `arr[:3]` — 0:3; `arr[2:]` — 2:len
  - Works on both TX strings and NUM arrays
  - C runtime: `plant_array_slice(arr, start, end)`, `plant_string_slice(str, start, end)`
- **Destructuring Assignment** (`core/parser.js`, `core/interpreter.js`):
  - `LET { x, y } = point.` — object/struct destructuring, lowers to per-field variable declarations
  - `LET [ head, tail ] = list.` — array/list destructuring, lowers to per-index variable declarations
  - `LET x = expr.` — simple `LET` alias for `CREATE`
  - `DestructDeclarationNode` with pattern type, pattern identifiers, and source expression
- **Structured Expression Parsing** (`core/parser.js`):
  - `_parseExpression()` / `_parsePrimary()` / `_parseBinaryExpression()` methods
  - `BinaryOpNode` and `UnaryOpNode` in AS`T with full evaluation in interpreter
  - `MatchExprNode` — MATCH used as expression yielding a value
- **New AST Node Classes** (`core/ast.js`): `BinaryOpNode`, `UnaryOpNode`, `InterpolatedStringNode`, `RangeExpressionNode`, `SliceExpressionNode`, `DestructDeclarationNode`, `OptionConstructNode`, `ResultConstructNode`, `MatchExprNode`
- **CodeWords Security Integration**: All new node types registered in `NETWORK_NODES` set
- **Tokenizer Keywords**: `LET`, `OPTION`, `RESULT`, `SOME`, `NONE`, `OK`, `ERR`, `IS_SOME`, `IS_NONE`, `IS_OK`, `IS_ERR`, `UNWRAP`, `UNWRAP_ERR`; `..` compound range operator token
- **Test Suite**: `tests/v0.44.0_pattern_matching_sugar.test.js` — 75 tests covering:
  - Option/Result instantiation (5 tests)
  - ExhaustivenessChecker (5 tests: complete, incomplete, wildcard for Option/Result)
  - String interpolation (3 tests: text-only, variable, multiple)
  - Range expressions (3 tests: 0..5, 3..3, 1..1)
  - Slicing (4 tests: string slice, array slice, start omitted, end omitted)
  - Destructuring (2 tests: object, array)
  - BinaryOp/UnaryOp evaluation (4 tests: add, equality, NOT, string concat)
  - Tokenizer keywords (11 keyword registrations)
  - C runtime declarations (15 header + 7 source checks)
  - CodeWords Governance (7 NETWORK_NODES registrations)
  - ENUM (Choice) exhaustiveness (2 tests)
- **Zero Regressions** — all testable suites at 100% pass rate

---

## v0.43.0 — 2026

### New: Native File I/O, Compile-Time Constant Folding & Type Infrastructure
- **Native File I/O** (`runtime/c/plant_runtime.{c,h}`):
  - `plant_file_read(filepath)` — reads entire file into heap-allocated string; returns NULL on failure
  - `plant_file_write(filepath, content)` — creates or overwrites file; returns bool success
  - `plant_file_exists(filepath)` — POSIX `stat()` existence check
  - `plant_file_delete(filepath)` — POSIX `remove()` deletion
  - `PlantArray` typedef for dynamic string array results (used by split)
- **String Manipulation Primitives** (`runtime/c/plant_runtime.{c,h}`):
  - `plant_string_split(str, delimiter)` — splits string by delimiter into PlantArray
  - `plant_string_trim(str)` — strips leading/trailing whitespace, returns new allocation
  - `plant_string_index_of(str, substr)` — 0-based index or -1 if not found
- **AST Constant Folder** (`src/compiler/ast_constant_folder.js`):
  - `ASTConstantFolder` pass that runs before IR emission
  - Folds static binary arithmetic (`10+5`→`15`, `3*4`→`12`, `2**10`→`1024`)
  - Folds static string concatenation (`"A"+"B"`→`"AB"`)
  - Folds logical/comparison operations (`10>5`→`true`, `true AND false`→`false`)
  - Unary NOT folding (`NOT true`→`false`)
  - Nested expression folding (`(2*3)+4`→`10`)
  - CONST identifiers folded to their literal values in downstream expressions
- **ENUM Declaration** (`core/ast.js`, `core/parser.js`, `core/interpreter.js`):
  - Syntax: `ENUM Name { MEMBER1, MEMBER2, ... }`
  - Auto-incrementing integer values starting from 0
  - Scoped member access via `EnumName.Member`
  - `EnumDeclarationNode` AST node with `{ name, members }` structure
- **TYPE Alias Declaration** (`core/ast.js`, `core/parser.js`, `core/interpreter.js`):
  - Syntax: `TYPE AliasName = BaseType.`
  - `TypeAliasDeclarationNode` with alias resolution at registration time
- **CONST Declaration** (`core/ast.js`, `core/parser.js`, `core/interpreter.js`):
  - Syntax: `CONST name(TYPE) TO value.`
  - Locked (immutable) soil entries, collectible by ASTConstantFolder
  - `ConstDeclarationNode` with optional type annotation
- **CodeWords Governance** integration:
  - New directives: `#ALLOW_FILE_READ`, `#ALLOW_FILE_WRITE`, `#ALLOW_FILE_DELETE`
  - File I/O node types registered in NETWORK_NODES set
  - `_requiredDirective` maps FileRead/FileWrite/FileDelete to their permissions
- **Test Suite**: `tests/v0.43.0_file_io_types_const.test.js` — 81 tests covering:
  - Arithmetic constant folding (8 scenarios)
  - String concatenation folding
  - ENUM declaration, members, auto-increment values
  - TYPE alias node structure
  - CONST declaration and identifier folding
  - C runtime header/source declarations (7 file I/O + 3 string + PlantArray)
  - File I/O parity via JS fs (read, write, exists, delete, nested dirs)
  - String manipulation (split, trim, index_of, empty segments, missing delim)
  - CodeWords security (5 scenarios: rejection, acceptance, missing directives)
  - Comparison constant folding (GT, LT, NOT)
  - ENUM auto-increment (7-member weekday enum)
  - Tokenizer keyword recognition (CONST, ENUM, TYPE)
  - Nested expression folding (`(2*3)+4`)
- **Zero Regressions** — all 30+ test suites at 100% pass rate

### Documentation
- CHANGELOG.md — new v0.43.0 entry
- ROADMAP.md — v0.43.0 objectives documented with completed table row and footer
- README.md — v0.43.0 completed section with File I/O, CONST/ENUM/TYPE, constant folding

---

## v0.42.0 — 2026

### New: C Backend Parity & Legacy Realignment
- **PlantMap Data Structure** (`runtime/c/plant_runtime.h`, `plant_runtime.c`):
  - `PlantMap` typedef with open-addressing, djb2 hashing, 75% load factor, automatic 2x growth
  - `plant_map_create(size_t)`, `plant_map_set(map, key, value)`, `plant_map_get(map, key)`, `plant_map_keys(map, &count)`, `plant_map_free(map)`
- **PlantIterator Protocol** (`runtime/c/plant_runtime.h`, `plant_runtime.c`):
  - `PlantIterator` struct supporting MAP (kind=0) and ARRAY (kind=1) traversal
  - `plant_iterator_init`, `plant_iterator_has_next`, `plant_iterator_next`, `plant_iterator_free`
- **Domain Primitives** (`runtime/c/plant_runtime.c`):
  - `plant_sys_action(name, payload)` — action dispatch logging to stdout
  - `plant_env_set_weather(type)` / `plant_env_get_weather()` — thread-local weather state for WEATHER block simulation
  - `plant_entity_set_species(entity, name)` — species assignment logging
- **LLVM Codegen** (`src/codegen/llvm/llvm_emitter.js`):
  - `MapLiteral` → `@plant_map_create` + inline `@plant_map_set` for each entry
  - `LinkStatement` → `@plant_map_set(map_ident, key, value)`
  - `ForInStatement` → indexed loop with `@plant_array_get` (array) or `@plant_map_get` (map)
  - `WeatherStatement` → `@plant_env_set_weather` with shelter/calm clause IR labels
  - `SpeciesDeclaration` → `@plant_entity_set_species`
- **Type Mapper** (`src/codegen/llvm/llvm_type_mapper.js`):
  - `MAP` and `DICT` types registered as `i8*`
- **CodeWords Governance** — verified no false-positive violations for MAP, FOR...IN, WEATHER, SPECIES, LINK nodes

### Test Suite
- New `tests/v0.42.0_c_backend_parity.test.js` — 31 tests covering:
  - PlantMap: create/set/get via IR emission, LINK statement translation, count verification
  - FOR...IN: array and map iteration IR labels
  - WEATHER: body/shelter/calm label emission, `@plant_env_set_weather` condition path
  - SPECIES: `@plant_entity_set_species` call with species name constant
  - CodeWords: zero false-positive violations for all new node types
  - Pipeline: end-to-end MAP + LINK + FOR...IN + WEATHER + SPECIES IR generation
  - IR Declarations: all 7 forward declarations present
- All 30+ test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.42.0
- Language Tour.md — header → v0.42.0, new architecture entries for security/codewords_governance.js and testing/test_runner.js
- TECHNICAL.md — new Section 25 for C Backend Parity & Legacy Realignment
- ROADMAP.md — v0.42.0 C Backend Parity objectives documented, table row and footer added
- CHANGELOG.md — new v0.42.0 entry

### Source Layout
- Enhanced `runtime/c/plant_runtime.h` — PlantMap, PlantIterator, domain primitives declarations
- Enhanced `runtime/c/plant_runtime.c` — PlantMap hash map, PlantIterator, domain primitives implementations
- New `tests/v0.42.0_c_backend_parity.test.js`
- New types: `MAP`, `DICT` in TYPE_MAP

---

## v0.41.0 — 2026

### New: Integrated Testing, Native Networking & CodeWords Governance
- **CodeWordsGovernance** (`src/security/codewords_governance.js`):
  - Directive parser: `#ALLOW_NETWORK`, `#ALLOW_HARVEST`, `#ALLOW_LISTEN`
  - Capability inheritance: `#ALLOW_NETWORK` implies both `#ALLOW_HARVEST` and `#ALLOW_LISTEN`
  - Static AST security pass — `checkNode()` / `checkAST()` reject `HarvestStatement` / `ListenBranchStatement` without corresponding directive
  - `SecurityViolationError` with source location, node type, and required directive
- **TestRunner** (`src/testing/test_runner.js`):
  - `SUITE` / `VERIFY` block discovery and execution
  - Truthy/falsy assertion evaluation (boolean, string, number, context lookup)
  - Nested SUITE support with aggregated pass/fail counts
  - `runAll(suites)`, `getSummary()`, `printSummary()`, `getExitCode()`
- **plantc test** subcommand (`src/cli/plantc.js`):
  - `plantc test <file.plant>` — parses, runs CodeWords check, executes all SUITE blocks
  - `--code-words-enforce` / `--skip-code-words` flags
  - Pass/fail summary on stderr, exit code 1 on any failure
- **POSIX Socket Runtime** (`runtime/c/plant_runtime.c`, `plant_runtime.h`):
  - `plant_net_harvest(url)` — HTTP GET via POSIX sockets
  - `plant_net_listen_open(port)` — TCP listener
  - `plant_net_accept(fd)`, `plant_net_read(fd)`, `plant_net_write(fd, data)`, `plant_net_close(fd)`
- **LLVM Codegen** (`src/codegen/llvm/llvm_emitter.js`):
  - AST visitors for `SuiteStatement` (no-op), `VerifyStatement` (no-op), `HarvestStatement` (`@plant_net_harvest`), `ListenBranchStatement` (listen/accept/read/write loop IR)
  - Forward declarations for all 6 socket helpers

### Test Suite
- New `tests/v0.41.0_native_net_governance.test.js` — 69 tests covering:
  - CodeWords: directive parsing, permission checks (direct + implied via #ALLOW_NETWORK), AST violations, SecurityViolationError construction
  - TestRunner: suite name, pass/fail counts, truthy/falsy assertions (boolean, string, number), nested SUITE aggregation, runAll summary, exit code
  - CodeWords + TestRunner integration within plantc test pipeline
- All 30+ test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.41.0
- Language Tour.md — header → v0.41.0, new "Integrated Testing & Native Networking (v0.41.0)" section with CodeWords, SUITE/VERIFY, HARVEST, LISTEN BRANCH documentation
- TECHNICAL.md — new Section 24 for v0.41.0 architecture
- ROADMAP.md — v0.41.0 objectives documented
- CHANGELOG.md — new v0.41.0 entry

### Source Layout
- New `src/security/codewords_governance.js`
- New `src/testing/test_runner.js`
- New `tests/v0.41.0_native_net_governance.test.js`

---

## v0.40.0 — 2026

### New: Geo-Aware Cycles, Dynamic Replica Rebalancing & Stream Compaction
- **GeoTopologyManager** (`src/cluster/topology/geo_topology.js`):
  - Dynamic RTT latency matrix with continuous probing between cluster nodes
  - Simulated RTT based on datacenter/zone/region topology (same-datacenter < 5ms, cross-region > 10ms)
  - `getOptimalNodes(dataLocalityKey, count)` selects lowest-latency nodes with locality affinity scoring and weight-based adjustment
  - Configurable `GEO_PROBE_INTERVAL` (1000-60000ms) and `GEO_PROBE_TIMEOUT` (100-10000ms) via MISSION CONFIG
  - EventEmitter for probe lifecycle events (`node:probed`, `node:registered`, `node:unregistered`)
- **StreamCompactor** (`src/cluster/reap/stream_compactor.js`):
  - Binary format with magic bytes (`PLRS`), version header, 48-bit timestamp, and original size metadata
  - zlib deflateRaw payload compression with configurable level (1-9, default 6)
  - Typed header encoding (string, integer, float, boolean, array) for structured REAP metadata
  - Achieves 60-85% reduction vs JSON on test payloads
  - Transparent `compressReapStream(headers, payload)` / `decompressReapStream(buffer)` round-trip
  - Error handling for invalid magic bytes, truncated buffers, decompression failures, version mismatch
- **DistributedCycleEngine** geo-aware execution:
  - `setGeoTopologyManager(geoTopology)` — wire up geo topology for node placement
  - `setReplicaManager(replicaManager)` — wire up replica manager for connection tracking
  - `executeCycleBlock(blockData, localityKey)` — dispatches blocks to optimal geo-affine nodes via `GeoTopologyManager.getOptimalNodes()`
  - Falls back to `NodeRegistry.getAliveNodes()` when no geo topology or locality key is provided
  - Emits `cycle:block_executed` with `geoAffinity` metadata
- **ReplicaManager** dynamic rebalancing:
  - `handleNodeJoin(nodeId)` — triggers partition rebalancing and replica healing on new node
  - `handleNodeLeave(nodeId)` — triggers primary failover, backup cleanup, and rebalancing on node departure
  - `_rebalancePartitions(newNodeId)` — migrates excess primaries from overloaded nodes to the new node, maintaining balanced actor counts
  - `_healReplicas(newNodeId)` — assigns backup replicas to the new node from under-replicated actors
  - Events: `node:join`, `node:leave`, `rebalance:complete`, `partition:moved`, `replica:healed`, `rebalance:partitions`, `rebalance:healed`

### Test Suite
- New `tests/v0.40.0_distributed.test.js` — 34 tests covering:
  - GeoTopologyManager: creation, latency matrix (same-datacenter < 5ms, cross-region > 10ms), `getOptimalNodes()` locality affinity, empty topology edge case
  - StreamCompactor: default compression level, buffer output, ≥60% reduction (measured 85%), full round-trip header+payload fidelity, error handling (non-Buffer, short buffer, bad magic)
  - DistributedCycleEngine: geo-aware `executeCycleBlock()` with locality key, no-workers fallback, `geoAffinity` metadata
  - ReplicaManager: `handleNodeJoin()` rebalanced=true/healed=true, primary count preservation, replica healing after join, `handleNodeLeave()` affectedActors tracking
- All 30 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.40.0
- Language Tour.md — header → v0.40.0, new "Geo-Aware Cycles & Dynamic Replica Rebalancing (v0.40.0)" section
- TECHNICAL.md — new Section 23 for Geo-Aware Cycles, Dynamic Replica Rebalancing & Stream Compaction design
- ROADMAP.md — v0.40.0 objectives documented
- CHANGELOG.md — new v0.40.0 entry

### Source Layout
- New directories under `src/cluster/`:
  - `src/cluster/topology/geo_topology.js`
- New file under `src/cluster/reap/`:
  - `src/cluster/reap/stream_compactor.js`

---

## v0.39.5 — 2026

### New: Phase 1 LLVM IR Compiler — Primitives & Early SHOW
- **New directories**: `runtime/c/`, `src/codegen/llvm/`, `tests/llvm/`
- **C Runtime Library** (`runtime/c/plant_runtime.{h,c}`):
  - `plnt_print_int(i64)` — prints signed 64-bit integers
  - `plnt_print_decimal(double)` — prints double-precision floats
  - `plnt_print_bool(i1)` — prints `true`/`false`
  - `plnt_print_text(i8*)` — prints null-terminated strings
  - `plnt_pow_i64(i64, i64)` — integer power helper
- **LLVM Codegen Infrastructure** (`src/codegen/llvm/`):
  - `llvm_context.js` — register counter (`%1`-based), string constant pool with dedup, `declare` header accumulator, x86-64 target triple/datalayout
  - `llvm_type_mapper.js` — PlantLang→LLVM type mapping (NUM→i64, SCL→double, FACT→i1, TX→i8*), print-function registry, mixed-type promotion helpers
  - `llvm_symbol_table.js` — variable declaration tracker, `alloca` emission at function entry
  - `llvm_emitter.js` — AST visitor + recursive-descent expression parser:
    - Handles `ProgramNode`, `LiteralNode` (NUMBER/STRING/FACT/RAW_EXPR), `IdentifierNode`, `CreateStatementNode`, `SetStatementNode`, `ShowStatementNode`
    - Full precedence expression parser for RAW_EXPR: arithmetic (`+ - * / % **`), comparison (IS, IS NOT, GT, LT, GTE, LTE), logical (AND, OR, NOT), parentheses, mixed-type promotion (i64↔double)
- **Differential Test Harness** (`tests/llvm/01_primitives.test.js`):
  - 39 tests: parses PlantLang source → generates `.ll` → `llc -O2` → links `plant_runtime.o` → runs binary → captures stdout → compares against AST interpreter output
  - Covers: integer/decimal/boolean/string literals, CREATE+SHOW, SET, arithmetic precedence, comparisons, logical operators, mixed-type expressions, multi-SHOW sequences
  - Generated IR validated by `llvm-as` and `llc -O2`
- Total test count grows from ~1212+ → **~1251+** across **29 test suites**
- All 39 new tests + all 75 existing tests at 100% pass rate

## v0.38.0 — 2026

### New Features
- **AST Zero-Fallback** — removed `RawStatementNode` from the parser; replaced with `EndBlockNode`, `BranchElseNode`, `BlockDelimiterNode` as typed structural markers. Truly unrecognized constructs are skipped gracefully (no fallback wrapping), maintaining the invariant that every node in any parsed AST is a typed, named class:
  - `core/parser.js`: RawStatementNode class removed, `_rawFallback` references eliminated, `assertNoRawStatements()` invariant helper called after every `parse()`.
  - `core/codegen.js`, `core/llvm_codegen.js`: EndBlock, BranchElse, BlockDelimiter no-op cases added.
  - All 28 test suites and all legacy example/regression files parse without producing a single RawStatement.
- **CYCLE...IN Loop with Index Variable** — `CYCLE item [, idx] IN list, body 1\.` syntax with per-iteration scope isolation:
  - `parseCycleStatement` rewritten: lookahead-based `,` disambiguation (index-var comma vs body-delimiter comma); proper `this.match` vs `this.peek` usage fixing advance bugs.
  - `src/interpreter/cycle_evaluator.js` — `evaluateCycleInStatement` with scope isolation, BREAK/CONTINUE signal catching, empty/null/undefined safety.
  - Index variable auto-binding in scope at depth 0 (`NUM`), reset per iteration.
- **BREAK / CONTINUE Statements** — `BREAK.` and `CONTINUE.` with `BreakSignalException`/`ContinueSignalException` for structured loop control:
  - `BreakStatementNode`, `ContinueStatementNode` in AST.
  - Interceptor wraps `_evalBody` with try/catch for signal propagation.
- **Multi-field SORT** — `SORT list BY field1 ASC, field2 DESC, ...` with chained comparator:
  - `SortStatementV2Node` in AST; `parseSortStatement` rewritten to support `BY` syntax.
  - `src/interpreter/sort_evaluator.js` — `_makeChainedComparator` for multi-field ASC/DESC, null-to-end semantics, `localeCompare` for strings.
  - Simple `SORT list.` and `SORT list ASC|DESC.` still work identically (empty `fields` array falls through to `_makeSimpleComparator`).
- **BLOOM AS Visual Governance** — `BLOOM data_expr AS TABLE|GRAPH|CHART.` with target-specific rendering:
  - `BloomAsStatementNode` in AST; `parseBloomAsStatement` method.
  - `src/interpreter/bloom_evaluator.js` — TABLE/GRAPH/CHART renderers with `isRestrictedEnvironment` detection (`CODEPLANT_RESTRICTED` env var, non-TTY).
- **Nested Struct Formatting** — `SHOW` on nested struct instances renders an indented, JSON-like tree view:
  - `src/interpreter/show_formatter.js` — `formatShowValue` with recursive struct descent, type-prefixed keys, indentation.
- **Memory Allocators (FAST / PERSISTENT Patterns)** — two allocator implementations for the local runtime layer:
  - `src/memory/allocator.js` — `ArenaAllocator` (FAST bump allocator with child arena cascading reset) and `ARCHeap` (PERSISTENT cascading reference counting with parent-child retention chains).

### Parser Fixes
- `this.match()` does not advance — added explicit `this.advance()` after `IN` detection in non-index CYCLE path (was collecting `"IN items"` instead of `"items"`).
- FROM/TO path: restored `this.advance()` after `FROM` check (was producing `fromExpr: "FROM 1"` instead of `"1"`); restored comma-stopping inline loop for `toExpr` (was including trailing comma); restored comma/period body delimiter consumption.
- INFUSE, ABSORB, SEAL dispatch removed — these not-yet-migrated features now skip gracefully via the fallthrough handler rather than throwing.
- HARVEST block form (comma-delimited body with HEADERS/TIMEOUT/AS clauses) parses successfully.
- `SHOW_VERIFY_SUMMARY` built-in directive parsed as `ShowVerifySummaryNode` (was being skipped due to tokenization as a single `IDENT`).

### Test Suite
- New `tests/v0.38.0_ergonomics.test.js` — 54 tests covering:
  - AST Zero-Fallback: EndBlock, BranchElse, BlockDelimiter nodes, assertNoRawStatements invariant
  - CYCLE...IN: empty/null/undefined lists, index variable, element iteration
  - BREAK/CONTINUE: signal exceptions, AST node types
  - Multi-field SORT: chained comparator, ASC/DESC, null-to-end, locale sort
  - Nested struct formatting: formatShowValue for all primitive types, struct nesting, deep field access
  - BLOOM AS: AST node type, target type detection, restricted environment detection
  - Memory allocators: ArenaAllocator alloc/reset/child cascading, ARCHeap retain/release/cascading parent-child
  - Integration: end-to-end CYCLE with index, full program execution, zero RawStatement guarantee
- Total test count grows from ~1158+ → **~1212+** across **28 test suites**
- All 28 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.38.0
- Language Tour.md — header → v0.38.0, new CYCLE...IN (with index variable), BREAK/CONTINUE, multi-field SORT, BLOOM AS, nested struct formatting sections
- TECHNICAL.md — new Section 21 for Language Ergonomics & AST Zero-Fallback design
- ROADMAP.md — v0.37.0 objectives marked complete, v0.38.0 Language Ergonomics documented
- CHANGELOG.md — new v0.38.0 entry

### Source Layout
- New directories under `src/`:
  - `src/interpreter/cycle_evaluator.js`
  - `src/interpreter/sort_evaluator.js`
  - `src/interpreter/bloom_evaluator.js`
  - `src/interpreter/show_formatter.js`
  - `src/memory/allocator.js`

---

## v0.37.0 — 2026

### New Features
- **Distributed Cycles & Replica Governance Engine** — three cluster modules implementing partitioned loop distribution, stateless/stateful replication, and dual-mode reap aggregation:
  - `ReplicaManager` — stateless routing: LEAST_CONNECTIONS (default) and ROUND_ROBIN strategies; stateful Primary-Backup: assignment, delta replication log, ACK modes (ONE/QUORUM default/ALL); NodeRegistry-integrated failover with automatic highest-priority backup promotion; MISSION CONFIG for `REPLICA_STRATEGY` and `PRIMARY_BACKUP_ACK`
  - `DistributedCycleEngine` — adaptive chunk size computation via `max(minChunkSize, ceil(N / (activeWorkers × CYCLE_CORE_FACTOR)))`; `scatter()` to split iterations into chunks and assign to workers; `completeChunk()` with automatic work-stealing (`_trySteal()`) from idle workers; `checkTimeouts()` for straggler detection and re-queue with `WORKER_TIMEOUT_MS` (default 5000ms, range 1000–60000); `isComplete()` for cycle completion detection; MISSION CONFIG for `CYCLE_CORE_FACTOR`, `CYCLE_MIN_CHUNK_SIZE`, `WORKER_TIMEOUT_MS`
  - `ReapAggregator` — LOCAL_REAP: in-memory deterministic reduce/merge/flush with keyed deduplication; REMOTE_REAP: stream to MEMORY_BUFFER or URI-based target (`s3://`, `stream://`, etc.) via registered handlers; MISSION CONFIG for `REMOTE_REAP_TARGET`

### Test Suite
- New `tests/v0.37.0_distributed_cycles.test.js` — 89 tests covering ReplicaManager stateless routing (round-robin, least-connections, 100-call distribution), stateful Primary-Backup (assignment, replication log, ONE/QUORUM/ALL ACK modes), primary failover with backup promotion, DistributedCycleEngine chunking (formula, min chunk, scatter count), work-stealing, timeout recovery, ReapAggregator LOCAL_REAP (collect/reduce/merge/flush), REMOTE_REAP (memory buffer, URI targets), MISSION CONFIG validation (all 7 directives with range enforcement), and integration scenarios
- Total test count grows from ~1069+ → **~1158+** across **27 test suites**
- All 27 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.37.0
- Language Tour.md — header → v0.37.0, new "Distributed Cycles & Replica Governance (v0.37.0)" section with ReplicaManager, DistributedCycleEngine, ReapAggregator syntax and examples; Architecture diagram updated with all 3 new modules
- TECHNICAL.md — new Section 20 for Distributed Cycles & Replica Governance design, chunking formula, work-stealing protocol, ACK modes, LOCAL_REAP/REMOTE_REAP pipeline, and code snippets
- CHANGELOG.md — new v0.37.0 entry describing distributed cycles and test suite

### Source Layout
- New directories under `src/cluster/`:
  - `src/cluster/replica/replica_manager.js`
  - `src/cluster/cycles/distributed_cycle_engine.js`
  - `src/cluster/reap/reap_aggregator.js`

---

## v0.36.0 — 2026

### New Features
- **Geographic Routing & State Governance Engine** — three geo-routing modules implementing shared state governance with dual-path consensus, bounded static call-graph affinity analysis, and adaptive SMART execution routing:
  - `ShareGovernance` — SHARED_READ: O(1) versioned snapshot reads with zero lock contention; TCP Gossip invalidation propagation across peer nodes within `GOSSIP_PROPAGATION_MS`; SHARED_WRITE RAFT: single-leader linearizable consensus with log replication and majority commit; SHARED_WRITE CRDT: LWW register with lamport clock and nodeId tiebreak for conflict-free convergence; `parseDirective()` for `SHARE CONFIG <KEY> READ_ONLY|MUTABLE [CONSENSUS=RAFT|CRDT]` syntax; MISSION CONFIG for `GOSSIP_PROPAGATION_MS` and `CONSENSUS_ENGINE`
  - `CallGraphAnalyzer` — adjacency matrix built from function invocation frequencies; bounded depth traversal at `CALL_GRAPH_MAX_DEPTH` (default 3, range 1–10) guaranteeing O(V·E₍bₒᵤₙdₑd₎) pass times; Louvain-inspired community detection forming Affinity Groups; `computePlacement()` for static affinity group → node assignment; `buildFromAST()` factory for compiler integration; MISSION CONFIG for `CALL_GRAPH_MAX_DEPTH`
  - `SmartExecutionRouter` — adaptive triage router: LOCAL_CPU (default, normal workloads), REMOTE_NODE (local CPU > 70% + remote latency < `SMART_ROUTE_MAX_LATENCY_MS`), GPU_ACCELERATED (vector/matrix ops + payload ≥ `SMART_ROUTE_GPU_MIN_BYTES` + registered GPU pipeline); payload size estimation; matrix/vector operation keyword detection; latency caching with 5s TTL; routing overhead < 0.05ms per invocation; MISSION CONFIG for `SMART_ROUTE_GPU_MIN_BYTES` and `SMART_ROUTE_MAX_LATENCY_MS`

### Test Suite
- New `tests/v0.36.0_geo_routing.test.js` — 125 tests covering ShareGovernance SHARED_READ (declare/read/100K-read benchmark/directive parsing), Gossip invalidation propagation (peer send + receive within latency window), SHARED_WRITE RAFT (write/commit/replication convergence), SHARED_WRITE CRDT (LWW write/bidirectional convergence/sequential convergence), CallGraphAnalyzer (edge weights/bounded depth/affinity groups/placement/stats), SmartExecutionRouter (GPU registration/payload estimation/vector detection/triage transitions/1000-call benchmark), MISSION CONFIG (all 5 directives with range enforcement), integration scenarios
- Total test count grows from ~944+ → **~1069+** across **26 test suites**
- All 26 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.36.0
- Language Tour.md — header → v0.36.0, new "Geographic Routing & State Governance (v0.36.0)" section with SHARE CONFIG syntax, SHARED_READ/SHARED_WRITE semantics, affinity analysis, and SMART routing; Architecture diagram updated with all 3 new modules
- TECHNICAL.md — new Section 19 for Geographic Routing & State Governance Engine design, TCP Gossip invalidation, bounded call-graph partitioning, SMART routing triage matrix, and code snippets
- CHANGELOG.md — new v0.36.0 entry describing geo-routing modules and test suite

### Source Layout
- New directories under `src/cluster/`:
  - `src/cluster/config/share_governance.js`
  - `src/cluster/affinity/call_graph_analyzer.js`
  - `src/cluster/router/smart_execution_router.js`

---

## v0.35.0 — 2026

### New Features
- **Cluster Architecture & Distributed Memory** — three cluster modules implementing decentralized node discovery, weighted-least-connections routing with circuit-breaker failover, and consistent-hash-ring-based distributed storage:
  - `NodeRegistry` — heartbeat-based node lifecycle with three health states (HEALTHY/DEGRADED/OFFLINE); per-node telemetry (CPU, heap, active workers); background timer at `HEARTBEAT_INTERVAL` increments `missedBeats`; node marked OFFLINE at `HEARTBEAT_THRESHOLD`; EventEmitter for topology events (node:registered/healthy/degraded/offline); MISSION CONFIG overrides for interval and threshold
  - `ClusterRouter` & `CircuitBreaker` — weighted least-connections node selection (lowest active count, CPU tiebreak); per-node circuit breaker with 3 states (CLOSED, OPEN, HALF-OPEN); sliding-window error rate (≥10 total, errorRate ≥ threshold); cooldown-based HALF-OPEN probe with success→CLOSED / failure→OPEN transitions; transparent backup failover on primary failure; aggregated error on dual failure; mTLSJwtGuard integration for dispatch authentication; MISSION CONFIG for threshold and cooldown
  - `DistributedHeap` & `ConsistentHashRing` — SHA-256 → BigInt hash space; configurable virtual nodes (default 128 per physical node); balanced key distribution (≥0.98 ratio for 2 nodes); PERSISTENT key/value store with lease-based expiry and background GC; stateful actor ownership with proxy detection for non-owner writes; `computeDataKeyMigration()` and `computeMigrationStats()` for zero-downtime rebalancing; `removeNode()` re-owns entries via ring lookup; MISSION CONFIG for virtual node count

### Test Suite
- New `tests/v0.35.0_cluster.test.js` — 88 tests covering NodeRegistry (register/unregister/heartbeat/telemetry/DEGRADED/OFFLINE/MISSION CONFIG), CircuitBreaker (CLOSED→OPEN→HALF-OPEN transitions/cooldown/reset/MISSION CONFIG), ClusterRouter (least-connections/CPU tiebreak/dispatch/failover/aggregated errors/benchmark), ConsistentHashRing (add/remove/distribution ratio/migration/vnode config), DistributedHeap (put/get/delete/actors/lease expiry/removeNode re-own/migration stats/benchmarks), integration scenarios
- Total test count grows from ~856+ → **~944+** across **25 test suites**
- All 25 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.35.0
- Language Tour.md — new "Cluster Architecture & Distributed Memory (v0.35.0)" section with all 3 cluster modules; Architecture diagram updated
- TECHNICAL.md — new Section 18 for Cluster Architecture design, MISSION CONFIG integration, ring implementation details
- CHANGELOG.md — new v0.35.0 entry describing cluster modules and test suite

### Source Layout
- New `src/cluster/` directory tree:
  - `src/cluster/discovery/node_registry.js`
  - `src/cluster/router/cluster_router.js`
  - `src/cluster/memory/distributed_heap.js`

---

## v0.34.0 — 2026

### New Features
- **Zero-Trust Security & Audit Architecture** — three security modules implementing non-blocking audit logging, mutual TLS with JWT authentication, and capability-based sandboxing:
  - `NonBlockingAuditLogger` — SharedArrayBuffer ring buffer (default 10K entries, configurable via `AUDIT_RING_SIZE`); O(1) lock-free atomic writes; SHA256 tamper-evident hash chain with `verifyIntegrity()` chain validation; async Worker Thread background flush; synchronous emergency flush on overflow
  - `mTLSJwtGuard` — TLS 1.3 certificate loading from `MTLS_CERT`/`MTLS_KEY`/`MTLS_CA` env vars or .pem file paths; RS256 and Ed25519 JWT signature verification with localized key caching; anti-replay protection via jti tracking table; certificate expiry auto-detection with renewal hooks; explicit error differentiation (EXPIRED, FORGERY, REPLAY, MTLS_FAILURE)
  - `CapabilityGuard` — zero-trust default: SAFE mode starts with zero permissions; granular capability matrix per mission mode (FILE_READ, FILE_WRITE, NET_CONNECT, NET_LISTEN, PROCESS_SPAWN, etc.); syscall filtering blocking execve/ptrace/fork/clone/kill in SAFE mode; violation enforcement with SIGSYS termination and CRITICAL audit log; `onViolation()` hook for custom alerting

### Test Suite
- New `tests/v0.34.0_security.test.js` — 91 tests covering audit logger integrity (hash chain, overflow, fast path overhead < 100µs), hash chain verification (prev hash chaining, zero genesis), mTLS & JWT verification (RS256 valid/expired/forged/replay/Ed25519), capability sandboxing (SAFE zero-default/grant/revoke/syscall blocking/integration hooks), benchmark suite (1000 record throughput, snapshot cycles, verifyIntegrity performance, 10K bulk write)
- Total test count grows from ~765+ → **~856+** across **26 test suites** (24 existing + runtime + parallel + security)
- All 26 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.34.0
- Language Tour.md — header → v0.34.0, new "Zero-Trust Security & Audit Architecture (v0.34.0)" section with all 3 modules, Architecture diagram updated with security modules
- CHANGELOG.md — new v0.34.0 entry describing all security modules

### Source Layout
- New `src/security/` directory tree:
  - `src/security/audit/audit_logger.js` + `src/security/audit/audit_worker.js`
  - `src/security/network/mtls_jwt_guard.js`
  - `src/security/sandbox/capability_guard.js`

---

## v0.33.0 — 2026

### New Features
- **Parallel Compilation & Telemetry** — four modules enabling multi-threaded codegen, distributed failover, and lock-free metrics:
  - `ParallelCodegenEngine` — DAG dependence graph builder with Tarjan cycle detection; weighted load balancer distributing actions by (1 + nested call count); worker_threads pool for parallel bitcode assembly with lock-free merge
  - `RemoteCompilerNode` — zlib (deflate) compression achieving ≥60% payload reduction on serialized AST; TCP transport via net.Socket; 100ms connect timeout triggers transparent failover to local engine; caller receives result regardless of remote availability
  - `NonBlockingTelemetry` — SharedArrayBuffer ring buffer: 128 entries × 64 bytes each; O(1) lock-free atomic writes via Atomics.add/store; zero-allocation snapshot() returning structured metrics copy; automatic read-ptr advance on overflow; background exporter for external sinks
  - `RuntimeDispatcher` — enableParallelCodegen()/disableParallelCodegen() toggle; auto-detects single-core CPUs and disables parallel mode at creation; hooks into NonBlockingTelemetry for compilation metrics

### Test Suite
- New `tests/v0.33.0_parallel.test.js` — 60 tests covering DAG building/cycle detection, weighted load balancing, network compression ratio (≥60%), 100ms timeout fallback, telemetry ring buffer/snapshot, dispatcher auto-disable, and a 20-node speedup benchmark suite (2/4/8 worker balance ratios)
- Total test count grows from ~705+ → **~765+** across **25 test suites**
- All 25 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.33.0
- ROADMAP.md — v0.32.0 objectives marked complete, v0.33.0 Parallel Compilation & Telemetry documented
- README.md — v0.33.0 completed section with parallel/distributed/telemetry modules; v0.34.0 planned section drafted
- CHANGELOG.md — new v0.33.0 entry describing all parallel/telemetry modules
- Language Tour.md — new "Parallel Compilation & Telemetry (v0.33.0)" section; Architecture diagram updated with all 4 new modules

### Source Layout
- New `src/compiler/parallel/` and `src/compiler/distributed/` directories:
  - `src/compiler/parallel/parallel_codegen.js`
  - `src/compiler/distributed/remote_compiler.js`
  - `src/telemetry/metrics_collector.js`
  - `src/runtime/dispatcher.js`

---

## v0.32.0 — 2026

### New Features
- **Local Runtime & Isolation Layer** — five runtime modules implementing mission-specific memory management, process isolation, adaptive IPC, and telemetry:
  - `BumpAllocator` (FAST mission) — O(1) linear bump allocator with strict 8-byte alignment, default 8MB capacity (hard cap 64MB), configurable via `MISSION CONFIG FAST_HEAP_SIZE = N`. O(1) reset at scope exit. Automatic BALANCED escalation with diagnostic on overflow.
  - `GlobalARCHeap` (PERSISTENT mission) — atomic reference counting with O(1) retain/release. Automatic cycle detection via weak reference registry every 1000 allocations (~0.1ms overhead). `GC.cycle()` interface for manual triggering during idle frames. `onFinalize` callbacks invoked when refcount reaches 0.
  - `WarmProcessPool` (SAFE mission) — 4 pre-warmed idle worker processes (configurable, ceiling `min(OS.cpus() × 2, 16)`). Ping/Pong heartbeat every 5000ms with 10ms timeout → zombie kill + respawn. Queue starvation protection: 50ms timeout → pool expansion or BALANCED fallback.
  - `SafeChannel` — adaptive IPC pipeline: Structured Clone for ≤1MB, Transferable ArrayBuffer zero-copy for >1MB, SharedArrayBuffer for read-only state, ReadableStream/WritableStream for continuous streaming. Emits [TRACE] logs identifying active mechanism.
  - `MissionContext` — wraps active allocator, IPC channel, and pool into unified telemetry. `diagnostic(msg)` for escalation/warnings (always on). `trace(msg)` for verbose logging (only with `--debug`). `getMetrics()` returns JSON with memory, fragmentation, pool status, and GC cycle counts.
- **Escalation & Safety Matrix** — 5 automatic rules: FAST OOM → BALANCED fallback, pool starvation → BALANCED fallback, heartbeat timeout → kill PID + respawn, 1000 allocations → cycle detection, large payload → transferable mode.

### Test Suite
- New `tests/runtime.test.js` — 70 tests covering BumpAllocator alignment/overflow/escalation, ARCHeap retain/release/cycle detection/GC.cycle(), SafeChannel all 4 transfer mechanisms, MissionContext diagnostics/metrics/tracing, ProcessPool creation/heartbeat simulation
- Total test count grows from ~635+ → **~705+** across **24 test suites**
- All 24 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.32.0
- ROADMAP.md — v0.31.0 objectives marked complete, v0.32.0 Local Runtime & Isolation Layer documented
- README.md — Local Runtime & Isolation Layer section; v0.33.0 planned section drafted
- CHANGELOG.md — new v0.32.0 entry describing all runtime modules
- Language Tour.md — new "Local Runtime & Isolation Layer (v0.32.0)" section with idiomatic code examples

### Source Layout
- New `src/runtime/` directory tree:
  - `src/runtime/allocators/bump_allocator.js`
  - `src/runtime/allocators/arc_heap.js`
  - `src/runtime/isolation/process_pool.js`
  - `src/runtime/isolation/worker_bootstrap.js`
  - `src/runtime/isolation/safe_channel.js`
  - `src/runtime/context/mission_context.js`

---

## v0.31.0 — 2026

### New Features
- **Five-Mission Execution Architecture** — five mission modes with distinct memory policies, optimization paths, and cross-mode call permissions:
  - `MISSION: BALANCED/FAST/SAFE/SMART/PERSISTENT.` — parser recognizes `MISSION <MODE>.` via `MissionStatementNode` AST
  - `MissionBlockNode` wraps all top-level statements under a mission declaration
  - `MissionStack` — runtime context tracking via `push(mode)`/`pop()`
  - `ScopedArena` — depth-level memory slabs with per-mission overflow policies (`expand`, `snapshot`, `reset`)
  - `MissionDispatcher` — routes AST nodes to mission-specific evaluators with SMART mission-aware call routing
  - `BoundaryHandshakeMatrix` — 5×5 permission table (`BOUNDARY_MATRIX`) specifying which source modes may call ACTIONs in which target modes
  - `BoundaryViolationError` — structured error with `fromMode`, `toMode`, `reason` fields
  - LLVM codegen: `genMissionStatement` emits `@_mission_mode` global; `genReapStatement` emits mode-check guard IR
  - Typechecker: `_checkMissionStatement` validates mode string and permission matrix entries
  - 75 new tests across `tests/matrix.test.js` (28) and `tests/dispatcher.test.js` (47) — all green

### Test Suite
- 3 new test files: `tests/matrix.test.js` (28 tests), `tests/dispatcher.test.js` (47 tests)
- Total test count grows from ~560+ → **~635+** across **23 test suites**
- All pre-existing failures fixed — every test suite at 100% pass rate:
  - `test_diagnostics.js`: column assertion corrected (4→9) to match actual error column
  - `test_parser_migration.js`: RESPONSE body execution via `_verifyDryRun`; errVar expectation updated to `"division by zero"`
  - `test_llvm_codegen.js`: ACTION/REAP rejection test replaced with positive compilation test

### Documentation
- All `.md` files bumped to v0.31.0
- ROADMAP.md — v0.30.0 objectives marked complete, v0.31.0 Five-Mission Architecture documented with detailed sub-goals
- README.md — Five-Mission Architecture section under v0.31.0 completed; v0.32.0 planned section drafted
- CHANGELOG.md — new v0.31.0 entry describing all Five-Mission features

---

## v0.30.0 — 2026

### New Features
- **Block-Depth Contract Law Enforcement** — semantic depth validation ensuring well-structured code scope:
  - Parser: `enforceDepthContract(nodeType, minDepth, maxDepth, token)` method with `this.currentDepth` tracking
  - Scope entry/exit: `currentDepth` increments on ACTION body and CYCLE block entry, decrements on exit (via `try/finally`)
  - Typechecker: `validateDepthInvariants(ast)` second-pass AST walker verifying depth invariants
  - Enforces: ACTION/SPECIES restricted to Depth 0; REAP/GIVE/CYCLE restricted to Depth ≥ 1
  - DepthContractError: clear `SYNTAX_STORM` with `[DepthContractError]` prefix, expected range, and caret location
  - 13 new tests covering valid/invalid depth scenarios
- **Universal REAP Expression Support** — `REAP x FROM SPLIT(str, delim)`, `REAP x FROM JOIN(arr, delim)`, `REAP x FROM parts[0]` now work natively in both interpreter and LLVM backend:
  - Parser: `parseReapStatement` detects expression sources (`(` or `[` after FROM) and delegates to `parseExpressionSpan` for full AST construction
  - Typechecker: `_checkReap` handles `EXPR` source kind by calling `_inferExprNode` to derive return type
  - Interpreter: `evaluateReapStatement` handles `EXPR` source kind via `evaluateExpressionNode`
  - LLVM Codegen: `genReapStatement` accepts `EXPR` source kind, evaluates via `compileAstExpr`, and auto-creates target variable with inferred type
  - `SHOW` on `[TX]` arrays prints element count
- **C Runtime Library (`libplantlang.so`)** — native C implementations of performance-critical operations callable via LLVM FFI:
  - Math functions: `sqrt`, `sin`, `cos`, `tan`, `floor`, `ceil`, `abs` — thin wrappers around libm
  - Array sort: `plnt_sort_i64`, `plnt_sort_double` — quicksort via `qsort`
  - String operations: `plnt_string_concat` (heap-allocated concat), `plnt_string_len` (length query)
  - `Makefile` with `runtime`, `exec`, `test`, `clean` targets
  - `chloroplast.js` compile pipeline automatically links `-Lruntime -lplantlang`
  - LLVM test harness (`test_llvm_codegen.js`) also links against runtime lib
- **NATIVE Keyword** — parser recognizes `NATIVE ACTION name(params) -> external.` syntax; sets `isExternal = true`
- **RUNTIME_FFI Signature Map** — `core/llvm_codegen.js` maps 12 PlantLang FFI action names to their correct LLVM signatures (`double`, `void`, `%fat_ptr`, `i64` return types)
- **FFI Return Type Handling** — `genReapStatement` now handles `double`, `void`, `i64`, and `%fat_ptr` return types from external C functions:
  - `double` returns: stored directly or bitcast to `i64` for NUM targets
  - `void` returns: call emitted with no result binding
  - `i64` returns: legacy return-register convention preserved
- **Declare Cleanup** — parameter names stripped from `declare` lines for cleaner LLVM IR output

### Test Suite
- New `tests/test_phase21_runtime.js` — 20 tests: IR smoke tests, math FFI, SORT, SPLIT/JOIN (FFI), native SPLIT/JOIN via REAP (expression sources), 70KB large-string stress test
- New `tests/test_depth_contract.js` — 13 tests covering valid (ACTION at depth 0, REAP/CYCLE/GIVE inside ACTION) and invalid (REAP/CYCLE/GIVE at depth 0, nested ACTION, top-level REAP with depth syntax) depth scenarios
- All **20 test suites, ~560+ tests** — all green

### Documentation
- ROADMAP.md bumped to v0.30.0 — completed items marked, remaining objectives listed
- TECHNICAL.md — new section: Block-Depth Contract Law Enforcement (scope tracking, enforceDepthContract, validateDepthInvariants)
- README.md — Quick Reference updated with depth contract notes; Architecture diagram updated

---

## v0.29.0 — 2026

### New Features
- **SPECIES Vtable Dispatch (Phase 19)** — virtual method dispatch via polymorphic vtables in LLVM codegen:
  - Vtable pointer (i8*) added as field 0 of every species LLVM struct
  - Method slots computed across the full parent chain (parent methods prefix, child overrides reuse slots)
  - Per-species `@species.Name.vtable = constant [N x i8*]` globals emitted with function pointers
  - `genCreateSpecies` and `genCreateBloomed` store vtable pointer after zeroing fields
  - `genMethodCallStatement` dispatches through vtable: load → GEP → load function pointer → bitcast → call
  - Uniform `i8*` receiver convention for all species methods (bitcast to concrete type inside function body)
  - Dynamic dispatch also works in expression-level `MethodCall` nodes in `compileAstExpr`
- **CHOICE / MATCH LLVM Codegen (Phase 20)** — tagged unions and pattern matching compiled to native LLVM IR:
  - CHOICE values stored as `{ i64 tag, i64 payload }` struct (`insertvalue` / `extractvalue`)
  - Variant construction: `Option.None` and `Option.Some(10)` emit struct construction inline
  - MATCH statement: extract tag, icmp chain against variant indices, branch to clause body
  - Payload binding: extractvalue payload, store in arena, register in clause scope
  - MAP `get()` returns `Option<V>` (probes map, branches on found/not found, wraps result)
- **SPECIES LLVM Bug Fixes**:
  - Typechecker stores `speciesName` in variable info for BLOOM instances, fixing method resolution
  - LLVM `genFnDef` registers both `self` and `__self` in scope for SET field access
  - `SelfExpression` and `BloomExpression` nodes handled in `compileAstExpr`
  - `BloomStatement` emits clear error message (old-style `BLOOM x AS y.` not supported in compiled mode)

### Full Test Suite
- All **17 test files, ~724 total assertions** — all green
- No regressions from any Phase 17–20 changes

### Documentation
- All `.md` files bumped to v0.29.0
- ROADMAP.md — v0.29.0 objectives marked complete, v0.30.0 roadmap drafted
- TECHNICAL.md — new sections: Species Vtable Dispatch (4), CHOICE/MATCH Codegen (5), MAP get() Option

---

## v0.28.0 — 2026

### New Features
- **Native LIST Operations (Phase 18)** — built-in aggregate operations on dynamic arrays compiled to native LLVM IR:
  - `COUNT(xs)` — O(1) length extraction via `extractvalue` from `%fat_ptr` struct
  - `FIRST(xs)` / `LAST(xs)` — O(1) element access via GEP with element-size offset
  - `SUM(xs)` — O(n) inline accumulation loop emitted as LLVM IR (no external C call)
  - Type checker validates: COUNT/FIRST/LAST require `[T]` array; SUM requires `[NUM]`
  - Interpreter: JS-native implementations via `Array.length`, `Array.reduce`, index access
  - 15 new tests covering empty/populated arrays, single elements, type checking, FOR...IN parity

### LLVM Codegen
- `ListOp` dispatch in `compileAstExpr` — COUNT/FIRST/LAST emit GEP/load; SUM emits phi-based accumulation loop
- All expression contexts updated: SHOW, CREATE RHS, struct field defaults, SET RHS

### Test Suite
- Added **Phase 18 — Native LIST Ops** (`tests/test_phase18_lists.js`) — 15 tests
- Total test suites expanded to **17 files, ~724 total assertions** — all green

### Documentation
- All `.md` files bumped to v0.28.0

---

## v0.27.0 — 2026

### New Features
- **SPECIES / BLOOM — Object-Oriented Foundations (Phase 17)** — class-based OOP with `{ }` body syntax:
  - Syntax: `SPECIES Name { field: TYPE, ACTION method(params) { body } }` — `{ }` body blocks (new) alongside legacy `,`/`/SPECIES.` syntax
  - `FROM` / `PARENT` inheritance — parent fields prefixed in LLVM struct layout, methods accessible via bitcast
  - `BLOOM SpeciesName` expression usable in `CREATE x TO BLOOM SpeciesName.`
  - Colon-dispatch method calls: `REAP result FROM obj:method.`
  - `SELF:field` access in species action bodies (read, SET, INCREASE/DECREASE)
  - LLVM codegen: species registered as LLVM struct types; method calls emit static dispatch with name-mangled function names and bitcast for inherited methods
  - Full interpreter pipeline: `_evalMethodCallStatement`, `BloomExpression`, `SelfExpression` evaluation
  - 10 new tests covering `{ }` body parsing, BLOOM instantiation, method dispatch, inheritance, type checking, SELF mutation
- **FOR...IN Loop (Phase 15)** — iterate over list/array/map values:
  - Syntax: `FOR name IN expr, ... /FOR.` with optional depth prefix and `.` terminator
  - Iterates over LIST values, MAP keys, or TX character spans
  - Supports nested loops with `DEPTH`-aware parsing
  - `STOP_STORM` from `STOP IF` propagates correctly through `FOR` bodies
  - 19 new tests covering empty arrays, typed arrays, TX strings, MAPs, nested IF
- **STRUCT Type (Phase 16)** — alternative struct declaration syntax with `field: TYPE`:
  - Syntax: `STRUCT Person { name: TX, age: NUM }.` (alongside existing `SHAPE … field(TYPE)`)
  - Anonymous struct literals: `CREATE p(Person) TO { name: "Alice", age: 30 }.` in `CREATE` context
  - INCREASE/DECREASE support on struct fields: `INCREASE p.age BY 1.`
  - Full type-checker validation of struct fields and literal arity
  - LLVM codegen: struct literals emit GEP-based field stores
  - 16 new tests covering declaration, nested structs, field access/mutation, type checking, LLVM codegen
- **English-Language Cleanup** — all Arabic-language strings in engine `.js` files translated to English:
  - `core/interpreter.js` — ~58 storm messages and CLI strings converted
  - `core/llvm_codegen.js` — Unicode-escaped Arabic error message replaced
  - `core/runtime.js`, `core/evaluator.js` — Arabic error text translated
  - `chloroplast.js` — CLI banner, error messages, REPL prompts converted
  - `webrepl/examples-data.js` — all example data strings translated
  - `ar-IQ` locale → `en-US` in all date/time formatting calls
- **MAP Type (Hash Table) — Full LLVM Codegen** — open-addressing HashMap with linear probing:
  - Syntax: `CREATE m(MAP[NUM,TX]).` with typed key/value parameters
  - Map literals: `CREATE m(MAP[NUM,TX]) TO { 1: "a", 2: "b" }.`
  - `LINK key WITH value IN map.` — key-value insertion (also `link`/`LINK` in regex pipeline)
  - `m.put(key, value)` — runtime method for insertion
  - `m.has(key)` — returns FACT (true/false) — **compiled natively** via linear probing
  - `m.get(key)` — returns `Option<V>` — interpreter-only (needs MATCH codegen)
  - **Native bucket layout**: `{ i1 is_occupied, key_type, value_type }` with arena-allocated bucket arrays
  - **djb2 hash** for TX keys (emitted as inline LLVM IR loop), identity hash for NUM keys
  - **Automatic growth**: load factor > 0.75 triggers 2× capacity doubling with full rehash
  - **Rooted Depth integration**: maps stored as `%fat_ptr` struct `{ i8* buckets, i64 len, i64 cap }` within arena slabs
  - **Backward compatibility**: legacy untyped `MAP` (plain object) still works in interpreter
  - Full type-checker validation: key/value arity, type matching, MAP[K,V] vs MAP inference

### LLVM Codegen Fixes
- `llvmType()` — added MAP type support (`isMapTypeStr`) so arena allocation returns a valid `%fat_ptr*`
- `@llvm.memset.p0i8.i64` declaration — conditional declare for bucket array zeroing
- Grow loop branch fix — rehash loop was a no-op (both branch targets pointed to exit); now properly iterates old buckets
- `storeL`/`skipL` label separation — fixed unterminated basic block in grow rehash by adding explicit `br` terminator
- `genMapHas` return type changed from `NUM` to `FACT` — SHOW now prints `true`/`false` matching the interpreter
- `mapBucketSize` padding fix — bucket stride now computes correct padded struct size (40 bytes for `{i1,i64,%fat_ptr}` instead of 33), fixing data corruption on multi-element maps

### SHOW Expression Support
- `genShow` now handles `MethodCall` expressions (enables `SHOW m.has(1).` in compiled mode)
- LinkStatement added to `genStatement` switch — compiled via `ExprCompiler.compileExpr` + `genMapPut`

### Test Suite
- Added **Phase 14 — MAP Types** (`tests/test_phase14_maps.js`) — 17 tests covering empty map create, map literals, LINK/put semantics, has/get, overwrite, growth (10 entries), SHOW display, type-checker validation
- Added **Phase 15 — FOR...IN** (`tests/test_phase15_for_in.js`) — 19 tests covering empty arrays, typed arrays, TX strings, MAPs, nested IF, STOP IF propagation
- Added **Phase 16 — STRUCT** (`tests/test_phase16_structs.js`) — 16 tests covering declaration, nested structs, field access/mutation, type checking, LLVM codegen
- Added **Phase 17 — SPECIES/BLOOM** (`tests/test_phase17_species.js`) — 10 tests covering `{ }` body syntax, BLOOM instantiation, method dispatch, inheritance, SELF mutation, type checking
- LLVM backend expanded from 46 → **50 smoke tests** (4 new MAP tests: create+has, all-keys, growth, overwrite)
- Total test suites expanded to **16 files, ~709 total assertions** — all green

### Documentation
- All `.md` files bumped to v0.27.0
- ROADMAP.md — FOR...IN, STRUCT, and SPECIES/BLOOM objectives marked complete
- Language Tour.md — supported subset table now shows FOR...IN and SPECIES/BLOOM as ✅
- TECHNICAL.md — updated test counts from ~634 → ~709, 13 → 16 test files

---

## v0.25.0 — 2026

### New Features
- **SHAPE (Struct Types)** — user-defined aggregate types:
  - Syntax: `SHAPE Point { x(NUM), y(NUM) }.` with named, typed fields
  - Instantiation: `CREATE p(Point) TO Point{ 10, 20 }.`
  - Field access: `p.x`, `p.y`
  - Field mutation: `SET p.x TO 99.`
  - Support for zero-field structs (`SHAPE Unit { }.`)
  - Struct-of-struct composition and deep field access
- **Methods on Structs** — ACTIONs that receive a typed `SELF` parameter:
  - Syntax: `ACTION (self(Point)) move(x(NUM), y(NUM)), SET self.x TO x. SET self.y TO y. /ACTION.`
  - Method call: `REAP _ FROM p:move, 5, 10.` via colon dispatch
  - `SelfReferenceNode` in AST for `SELF` keyword
  - Type checker validates receiver type on method calls
- **Dynamic Arrays (push/pop)** — runtime array growth:
  - `PUT val INTO xs.` — amortized-O(1) appends with capacity doubling
  - `TAKE val FROM xs.` — pop from end with shrink
  - `CREATE xs(LIST) TO.` — empty list declaration
  - Type checker validates list operations
- **Tagged Unions via CHOICE** — type-safe sum types:
  - Syntax: `CHOICE Option { Some(NUM), None }.`
  - Variant construction: `Option.Some(10)`, `Option.None`
  - Variant names may collide with keywords (`Num`, `Empty`), handled by parser
  - Static type checker validates variant existence, payload types, arity
- **Pattern Matching via MATCH** — exhaustive case analysis on CHOICE values:
  - Syntax: `MATCH opt { Some(v) -> { SHOW v } None -> { SHOW 0 } }.`
  - Payload binding: `Some(v)` binds the inner value to `v` in the clause body
  - Payload-free clause: `None -> { SHOW "none" }`
  - Non-exhaustive match detection (missing variant = compile-time error)
  - `MatchStatementNode` in AST with `{variantName, binding, bodyStatements}` clause array
- **Member Access / Method Call Parsing** — full expression-level member access:
  - `Option.Some(10)` parsed as `MethodCallNode` with target `Option`, method `Some`
  - `Option.None` parsed as `MemberAccessNode` with object `Option`, member `None`
  - KEYWORD field names (e.g. `Num`, `Empty`) accepted after `.` in member-access position, with same-line + statement-avoidance heuristics to prevent false matches
- **Inferred CREATE types** — `(TYPE)` annotation optional when the value expression type can be inferred from a CHOICE variant construction or array literal

### Test Suite
- Added **Phase 9 — Structs** (`tests/test_phase9_structs.js`) — 70 tests covering SHAPE declaration, instantiation, field access/mutation, struct-of-struct, type checking, LLVM codegen parity
- Added **Phase 10 — Dynamic Arrays** (`tests/test_phase10_arrays.js`) — 58 tests covering push/pop, capacity growth, type checking, LLVM codegen, interpreter parity
- Added **Phase 11 — Methods** (`tests/test_phase11_methods.js`) — 47 tests covering method declaration, call dispatch, SELF receiver, type checking, LLVM codegen
- Added **Phase 12 — Array Growth** (`tests/test_phase12_arrays_growth.js`) — 64 tests covering amortized capacity doubling, shrink-on-pop, edge cases
- Added **Phase 13 — CHOICE & Pattern Matching** (`tests/test_phase13_choices_matching.js`) — 64 tests covering variant declaration, construction, member access, MATCH exhaustiveness, payload binding, type checking, interpreter execution
- Total test suites expanded to **12 files, ~613 total assertions** — all green

### Documentation
- `README.md` updated to v0.25.0 with SHAPE, Methods, Dynamic Arrays, CHOICE/MATCH sections
- `ROADMAP.md` — v0.25.0 objectives marked complete, v0.26.0 roadmap drafted
- `Language Tour.md` — added SHAPE, CHOICE/MATCH, method call syntax
- `TECHNICAL.md` — updated test counts and architecture diagrams

---

## v0.24.0 — 2026

### New Features
- **IMPORT Statement & Module System** — multi-file PlantLang programs:
  - `IMPORT "path".` syntax for loading external `.plnt` files
  - Relative, absolute, and `std/`-prefixed path resolution
  - Cycle detection with clear error messages (`IMPORT cycle detected`)
  - AST merging — imported statements are spliced into the importing program's AST at parse time
  - Type checker resolves and validates all imports before semantic analysis
  - Interpreter resolves `IMPORT` at runtime with file-system lookup
  - `ImportStatementNode` in AST with `path`, `statements`, `resolvedAbsPath` fields
- **FFI (Foreign Function Interface)** — call native C functions from PlantLang:
  - Syntax: `ACTION name(params) -> external.` — marks an ACTION as an external C function
  - `isExternal` property on `ActionDeclaration` nodes
  - LLVM backend emits `declare` IR for FFI functions with proper type signatures
  - FFI stubs pre-registered in the interpreter for all `runtime_bridge` functions
  - Type checker validates FFI signatures against known bridge functions
- **Standard Library Foundation** (`std/`):
  - `std/io.plnt` — `print`, `println`, `plant_printf`, `plant_puts` (FFI-bridged I/O)
  - `std/string.plnt` — `len`, `upper`, `lower`, `trim`, `contains`, `split`, `replace`, `concat`
  - `std/prelude.plnt` — auto-injected core definitions (TRUE, FALSE, _BOOT)
  - Auto-prelude injection — every program implicitly imports `std/prelude.plnt`
  - `IMPORT "std/io"` and `IMPORT "std/string"` — resolve via `std/` path prefix
- **Core Runtime Bridge** (`core/runtime_bridge.c`):
  - C bridge implementing FFI targets: `plant_printf`, `plant_puts`, `plant_len`, `plant_upper`, `plant_lower`, `plant_trim`, `plant_contains`, `plant_split`, `plant_replace`, `plant_concat`
  - Linked into compiled binaries via the LLVM backend's `declare` + extern resolution

### Test Suite
- Added **Phase 7 — Module System & FFI** (30 test groups)
- Added **Phase 8 — Standard Library** (8 integration test groups)
- LLVM backend expanded from 37 → **46 smoke tests**
- All test suites green — 7 test files, ~300+ total assertions

---

## v0.23.0 — 2026

### New Features
- **ACTION/REAP/GIVE** in the LLVM backend (`core/llvm_codegen.js`) — full function support with:
  - Multiple typed parameters (NUM, SCL, TX, FACT)
  - Recursive calls (factorial, Fibonacci — verified via llc + gcc)
  - IF/ELSE bodies with multiple GIVE statements (terminator-aware branching)
  - SCL (double) params via bitcast through i64 return register
  - TX (string) returns via ptrtoint/inttoptr
  - Void actions (no GIVE) default to `ret i64 0`
- **Rooted Depth System** — deterministic arena-based memory management:
  - 64 depth levels, each backed by a 64KB arena slab (`@arena_offsets[64 x i64]`, `@arena_memory[64 x [65536 x i8]]`)
  - Bump allocation (`arenaAlloc`/`arenaAllocTyped`) replaces all `alloca` across all variable creation sites (CREATE, REAP auto-create, CYCLE iter, function params)
  - Depth tracking (`trackDepth`) injects automatic `arenaResetDepth` on scope exit
  - Arena globals emitted lazily only when `m.usesArena` is set
- **Contract Law Validation** (Article III) — compile-time depth enforcement:
  - CREATE destination must be ≤ current depth, with educational error messages citing the violated rule and suggesting fixes
  - `checkDepthAccess()` helper infrastructure for future cross-depth read/write enforcement
  - Scope entries track `{ptr, plType, depth}` for all variables
- **Forced Exit Arena Cleanup** (Article IX) — `genGiveStatement` emits the Unwinding Chain before every `ret`, resetting all arenas from `currentDepth` down to depth 1 (depth 0 preserved for caller's function params — critical for recursive correctness)
- **Loop Iteration Reset** (Article VII) — `genCycle` and `genSeason` save the arena offset at the loop's depth before the body and restore it after each tick, preventing temporary-variable accumulation across iterations. Deeper arenas entered during the body are also reset.
- **Error Unwinding Protocol** (WEATHER/SHELTER/CALM) — deterministic exception handling in LLVM:
  - `genWeatherStatement` generates the try/catch block structure with body, typed SHELTER handlers, and CALM continuation
  - Division-by-zero detection (`emitZeroCheck`) emits a `fcmp oeq` + conditional branch that sets `@_weather_msg`/`@_weather_type`/`@_weather_flag` globals and transfers control to the matching ZERO_STORM (or ANY_STORM) shelter handler
  - Unwind chain in each handler resets arenas from error depth to shelter depth
  - `errVar` binding loads the error message from `@_weather_msg` into a TX-typed arena slot
  - `shelterStack` enables proper nesting — inner errors are caught by inner handlers; handler body errors propagate outward
- **`core/Module`** gains `ensureWeatherGlobals()` and `weatherGlobalsEmitted` flag for lazy global emission

### Test Suite
- LLVM backend tests expanded from 34 → **37 smoke tests** (3 new: WEATHER body without error, ZERO_STORM caught via division by zero, post-error scope integrity)
- All four test suites green: LLVM backend 37/37, C codegen 9/9, parser migration 108/109 (1 pre-existing RESPONSE resolution discrepancy), diagnostics 44/45 (1 pre-existing)

### Documentation
- `README.md` updated to v0.23.0 with new sections on the Rooted Depth System, complete Quick Reference (ACTION/REAP/GIVE, WEATHER/SHELTER/CALM, depth syntax), updated test counts, and reorganized Roadmap

---

## v0.22.0 — 2026

### New Features
- **LLVM IR Backend** (`core/llvm_codegen.js`) — a real compiler backend emitting LLVM IR text, using the same pipeline architecture as **Rust, Swift, Julia, and Zig**:
  - Proper SSA-form code generation (`alloca`/`load`/`store` per variable, exactly matching clang's own `-O0` output shape, which LLVM's `mem2reg` pass then promotes to true SSA registers)
  - Full expression compiler with correct operator precedence (`OR` → `AND` → `NOT` → comparison → additive → multiplicative → unary → atom), matching the interpreter's evaluation semantics exactly
  - NUM/SCL type promotion, string concatenation via runtime `malloc`/`strcpy`/`strcat`, `BETWEEN` conditions, boolean short-circuit-free `AND`/`OR`/`NOT`
  - Same supported/unsupported subset as the C backend (CREATE/SET/INCREASE/DECREASE/SHOW/IF/ORIF/ELSE/CYCLE/SEASON) — unsupported constructs (LIST, MAP, ACTION, SPECIES, ...) fail with a clear compile-time error, never silently miscompiled
- **`chloroplast compile --backend llvm|c`** — auto-detects an installed LLVM toolchain (`llc`/`llc-14` through `llc-18`) and prefers it by default; falls back to the C backend automatically if no LLVM install is found
- **`opt -O2` integration** — the generated IR is run through LLVM's real optimizer (mem2reg, GVN, loop optimizations, inlining) before `llc` lowers it to object code. Without this step, hand-emitted alloca-heavy IR only gets `llc`'s backend-level optimizations and misses most of what makes LLVM fast — with it, compiled PlantLang matches gcc's own optimizer on equivalent code (verified: a 50-million-iteration accumulation loop drops from 88.7s interpreted to 6ms compiled — a ~14,700× speedup)
- **`tests/test_llvm_codegen.js`** — 27 parity tests (interpreted vs LLVM-compiled output must match exactly) covering arithmetic, SCL/NUM promotion, string concatenation, all comparison operators, BETWEEN, boolean logic, IF/ORIF/ELSE, CYCLE (with and without STEP), SEASON, nested loops, and unsupported-construct error handling
- **`examples/10_performance.plnt`** — a runnable interpreter-vs-compiled benchmark demonstrating the real-world speedup

### Fixes (found by building and testing the LLVM backend against the interpreter)
- **`core/evaluator.js`**: string literals containing text that happens to match a variable name (e.g. `"pi=" + pi` where `pi` is also a variable) were being corrupted — the identifier-substitution regex used plain word-boundaries (`\b...\b`) with no awareness of quote context, so it matched and replaced the `pi` substring *inside* the string literal `"pi="` itself, producing `"3.14=3.14"` instead of `"pi=3.14"`. Fixed by having the substitution regex skip over quoted string-literal spans entirely. This is a real interpreter bug, not just an LLVM-codegen quirk — it affected `chloroplast run` too, and was only caught because the LLVM backend's independent implementation disagreed with the interpreter's on the exact same test case.
- **`core/llvm_codegen.js`**: `CREATE x(NUM) TO someOtherVariable.` was compiling to a hardcoded `0` instead of loading the referenced variable — the parser produces a plain `Identifier` AST node (not a `RAW_EXPR` `Literal`) for single bare-identifier value expressions, and `genCreate()`'s type dispatch had no case for it, silently falling through to a zero default. Caught by a nested-loop test (`SEASON` inside `CYCLE`, where the loop-local variable was initialized from the outer loop's counter) producing empty output instead of a countdown.

---

## v0.21.0 — 2025 (current)

### New Features
- **Web REPL UI** (`webrepl/`) — a single-page, no-build-step browser interface for writing and running PlantLang:
  - Four modes matching the CodeWords Compiler Service's endpoints: **Run it**, **Check it**, **Verify it**, **Compile it**
  - Live connection indicator polling `/health`
  - Eight curated example programs loadable from a dropdown (generated from real `examples/*.plnt` files)
  - Collapsible generated-C-source view for Compile mode
  - `Cmd/Ctrl+Enter` keyboard shortcut, persistent editor content and server URL via `localStorage`
  - Visual design built around PlantLang's "reads like prose" identity — Fraunces serif for the mode tabs/wordmark, output lines settle in individually rather than appearing all at once
- **`webrepl/test_webrepl.js`** — 13 integration tests using jsdom to load the *real* HTML/JS files and drive actual clicks/keyboard events/fetch calls against a *live* CodeWords Compiler Service (not mocked)

### Milestone
This completes the full compiler pipeline originally planned:
```
Tokenizer → Parser/AST → Type Checker → Standard Library
    → C Generator → GCC Binary → Compiler Service → Web REPL UI
```
Every stage is implemented, tested, and documented.

---

## v0.20.0 — 2025 (current)

### New Features
- **CodeWords Compiler Service** (`service/codewords-server.js`) — standalone HTTP API exposing the interpreter, typechecker, and C code generator to external clients:
  - `GET /health` — service status
  - `POST /run` — execute a program, return captured output
  - `POST /check` — static type-check, return diagnostics
  - `POST /verify` — run VERIFY/SUITE tests, return pass/fail counts
  - `POST /compile` — generate C, compile with gcc, run the binary, return output
- **`service/sandbox-runner.js`** — per-request forked child process worker. Untrusted PlantLang code never runs in the main server process, so:
  - Infinite loops are killed cleanly after a 5s timeout without affecting other in-flight requests
  - `LISTEN BRANCH` and `HARVEST` are rejected before execution (no port-binding or outbound network access from submitted code)
  - Output is capped at 64KB; request bodies at 128KB
- **`service/test_service.js`** — 15 automated tests covering every endpoint, error handling, timeout enforcement, network blocking, and true concurrent-request isolation
- **`service/README.md`** — full API reference and safety model documentation

---

## v0.19.0 — 2025 (current)

### New Features
- **C Code Generator** (`core/codegen.js`) — translates a supported subset of PlantLang AST into standalone, compilable C99 source:
  - `CREATE` / `SET` / `INCREASE` / `DECREASE` for `NUM`, `SCL`, `TX`, `FACT`
  - `SHOW` — string literals, identifiers, and `+` concatenation (auto-selects `printf` format per type)
  - `IF` / `ORIF` / `ELSE`
  - `CYCLE var FROM lo TO hi [STEP k]` (numeric ranges)
  - `SEASON` (while loop)
  - Arithmetic (`+ - * / % **`) and comparisons (`IS`, `GREATER THAN`, `BETWEEN`, etc.) translated to native C operators
  - Unsupported constructs (LIST, MAP, ACTION/REAP, SPECIES, WEATHER, HARVEST, LISTEN BRANCH, ...) are reported as clear compile-time errors naming the exact construct and line — never silently miscompiled
- **`chloroplast compile`** — new CLI command:
  ```bash
  chloroplast compile file.plnt --run        # compile + execute immediately
  chloroplast compile file.plnt -o mybinary  # custom output path
  chloroplast compile file.plnt --keep-c     # keep generated .c file for inspection
  ```
  Pipes generated C through `gcc -O2 -lm` to produce a native binary.
- **`tests/test_codegen.js`** — parity smoke tests: runs each fixture both interpreted and compiled, asserts identical stdout (9/9 passing)
- **`examples/09_compile.plnt`** — FizzBuzz-style demo covering CYCLE, IF/ORIF/ELSE, SEASON, SCL comparisons

### Fixes
- `CYCLE var FROM lo TO hi STEP k` — `STEP` is tokenized as `IDENT` (not `KEYWORD`), so the parser's generic `_collectUntilKeyword()` never stopped at it. `toExpr` was swallowing `"20 STEP 5"` whole and `stepExpr` was always null. Fixed with an explicit IDENT-aware scan in `parseCycleStatement`. This was a **pre-existing interpreter bug**, not just a codegen limitation — `CYCLE ... STEP` was silently broken in `chloroplast run` too.

---

## v0.18.0 — 2025 (current)

### New Features
- **Static Type Checker** (`core/typechecker.js`) — full AST-based static analysis:
  - `TYPE_MISMATCH` — wrong type passed to action parameter or arithmetic on non-NUM
  - `ARITY_MISMATCH` — wrong number of arguments to an action
  - `UNDEFINED_ACTION` — calling an action that was never defined
  - `UNDEFINED_VAR` — using a variable that was never declared (warning)
  - `UNDEFINED_SPECIES` — BLOOM of an undefined SPECIES (warning)
  - `LOCK_VIOLATION` — attempt to SET a ROOT or locked variable
  - Errors inside WEATHER body are demoted to info (they're intentionally risky)
  - Two-pass architecture: declaration hoisting then full type checking
  - Library return types pre-declared (`math:SQRT → SCL`, `strings:UPPER → TX`, etc.)
  - Polymorphic action detection (all-TX params treated as ANY)
- **`chloroplast check`** now runs the static type checker with visual caret output
  (previously just counted statements)

### Fixes
- `chloroplast check` shows file/line/column with source preview and error arrow

---

## v0.7 — 2025 (current)

### New Features
- **LISTEN BRANCH** — real Node.js HTTP server with routing (`IF/ORIF/ELSE` on `req:"path"`)
- **HARVEST** — synchronous-style HTTP/HTTPS client (Worker + SharedArrayBuffer)
- **BRAID** — zip two lists into pairs or a MAP
- **VERIFY / SUITE** — built-in testing framework written in PlantLang itself
- **ANALYZE** — type-aware data inspection (sum/avg/min/max for NUM lists, key listing for MAPs)
- **NOW** — current date/time (`FORMAT:DATE`, `FORMAT:TIME`, `FORMAT:STAMP`, `FORMAT:YEAR`)
- **WAIT** — synchronous sleep (capped at 10s)
- **ROOT_SCOPE** — locked config MAP built at program start
- **TYPEOF** — `REAP t FROM TYPEOF x` returns type as TX

### Language Fixes
- `GREATER THAN OR EQUAL` / `LESS THAN OR EQUAL` now work correctly (were parsed as `GREATER THAN OR` + `EQUAL`)
- `IF/ORIF/ELSE` chain now runs only the first matching branch (was running all branches)
- `obj:"key"` quoted MAP access works inside string concatenation
- Chained access `obj:"k1":"k2":"k3"` works in expressions and compound strings
- `**` (power) tokenized as single token — `2 ** 8` now evaluates correctly
- `SET obj:"key" TO val` added with LOCK_STORM guard
- `LOCK_STORM` on `SET` to any locked MAP or ROOT variable

### Architecture
- Full compiler frontend: Tokenizer → Parser → AST (40+ typed node classes) → evaluateNode
- 96% AST coverage — all statements except empty block closers parsed to typed nodes
- CLI (`chloroplast run/verify`) now uses AST pipeline (`runSource`) exclusively
- symbolPass handles typed PlantStatement/RootStatement/RootScopeStatement/MissionStatement
- VERIFY dry-run mode for LISTEN BRANCH inside SUITE blocks

---

## v0.6 — 2025

### New Features
- **VERIFY / SUITE** initial implementation
- **BRAID** initial implementation
- **HARVEST** initial implementation (HTTP via Worker + SAB)
- **ANALYZE / TYPEOF / NOW / WAIT / ROOT_SCOPE** implemented
- **PULSE / WHENEVER** confirmed working with MATCH bodies
- Precision line/column diagnostics with visual caret pointer
- `SHOW_VERIFY_SUMMARY` CLI command, `chloroplast verify` subcommand

### Architecture
- Compiler frontend migration started (Tokenizer, Parser, AST as additive layer)
- SHOW / CREATE / LISTEN BRANCH / RESPONSE migrated to AST
- WEATHER / SHELTER / CALM migrated to AST

---

## v0.5 — 2025

### New Features
- **SPECIES / BLOOM / PARENT** — class-based OOP with inheritance
- **SELF** — instance self-reference in all contexts (read/write/INCREASE/PUT/REAP)
- **FLOW** — action pipeline chaining
- **MATCH / YIELD** — pattern matching with BETWEEN, range, ELSE
- **SEASON** — while-style loop

---

## v0.4 — 2025

### New Features
- **WEATHER / SHELTER / CALM** — try/catch/finally error handling
- **TAP / ABSORB / INFUSE / SEAL** — file I/O (VeinFS virtual filesystem)
- **PULSE / WHENEVER / CHANGES** — reactive variable observation
- **ROOT / LOCK** — immutable constants
- Multiple SHELTER clauses per WEATHER block
- `ANY_STORM` catch-all

---

## v0.3 — 2025

### New Features
- **ACTION / GIVE / REAP** — user-defined functions with return values
- **Recursion** — verified with Fibonacci and factorial
- **REAP _ FROM** — discard return value
- **PLANT** — load built-in libraries (math, strings, lists)
- **math** library: SQRT, ABS, ROUND, FLOOR, CEIL, POW, LOG, SIN, COS
- **strings** library: TRIM, UPPER, LOWER, LENGTH, SPLIT, REPLACE, INCLUDES
- **lists** library: AVERAGE, MEDIAN, UNIQUE, FLATTEN, CHUNK, RANGE

---

## v0.2 — 2025

### New Features
- **CYCLE** — list iteration and numeric range loops (`FROM n TO m STEP k`)
- **SORT / SHAKE** — list sorting and shuffling
- **PUT / TAKE** — list mutation
- **LINK** — MAP key assignment
- **MAP** type with `obj:prop` and `obj:"key"` access
- **STOP IF** — conditional abort with optional message
- Comma continuation for multi-line statements

---

## v0.1 — 2025 (initial)

- Core interpreter in Node.js (Chloroplast)
- Depth prefix `N\` syntax
- Types: NUM, SCL, TX, FACT, LIST
- CREATE / SET / INCREASE / DECREASE / SHOW / LOCK / EVAPORATE
- IF / ORIF / ELSE (single-line and block forms)
- MISSION: SAFE / FAST / SMART / BALANCED
- Arabic-readable design with English-only keywords
