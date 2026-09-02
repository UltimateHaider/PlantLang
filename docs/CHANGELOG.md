## v0.49.56 - 2026 (Layered Architecture Refinement & Static Analysis Integration)

### Architecture
- **Error management unification**: New `plant_error.c` module providing
  `plant_log(level, format, ...)`, `plant_error`, `plant_warning`, `plant_info`,
  `plant_debug`, and `plant_set_log_level` with the `PlantLogLevel` severity enum.
  Existing error paths in `plant_runtime.c` are preserved unchanged; the new
  interface is opt-in for modular code.
- **Reporting subsystem extraction**: `plant_report.c` (core orchestrator),
  `plant_report_json.c` (JSON exporter), `plant_report_xml.c` (JUnit XML
  exporter), `plant_report_html.c` (interactive HTML generator) — each in a
  dedicated compilation unit under `runtime/c/`.
- **COLOR macros centralized**: ANSI color macros moved from `plant_runtime.c`
  into `plant_compat.h` so error, report, and runtime modules share the same
  color definitions.
- **plant_compat.h reorganized**: Added clearly delineated section headers
  (Execution Lifecycle & Assertions, Unified Error Management, Reporting
  Subsystem, Networking Boundaries) for strict boundary definition.
- **Codegen boundary**: `generate_node()` confirmed as the sole
  parser-to-codegen interface; no direct internal state coupling remains.

### Tooling
- **Clang-Tidy integration** (`make tidy`): Runs clang-tidy on all runtime C
  files (skips gracefully if clang-tidy is not installed).
- **Cppcheck integration** (`make cppcheck`): Comprehensive static analysis
  with all checks enabled (skips gracefully if cppcheck is missing).
- **Valgrind integration** (`make valgrind`): Memory leak and invalid-access
  audit of runtime execution paths (skips gracefully if valgrind is absent).

### Tests
- `tests/regression/compatibility/test_error_compat.plant`: Verifies unified
  error logging interface produces expected severity-tagged output.
- `tests/regression/compatibility/test_report_json_compat.plant`: Validates
  JSON report export format.
- `tests/regression/compatibility/test_report_html_compat.plant`: Validates
  HTML report export format.
- `tests/regression/run_regression_tests.sh`: Updated to recurse into
  `compatibility/` subdirectory for compatibility test fixtures.

### Internal
- Version markers moved to v0.49.56 (Makefile, src/plantc/main.plant banner,
  tests/native/run_native_tests.sh).
- New C files: `plant_error.c`, `plant_report.c`, `plant_report_json.c`,
  `plant_report_xml.c`, `plant_report_html.c` added to Makefile `RUNTIME_C`
  compilation path and release/install targets.
- Verification: regression 187→190 passing, native 20/20, `make self`
  converged.

## v0.49.55 - 2026 (Layered Architecture Foundation: Lexer/Parser/Runtime Separation)

### Architecture
- **Lexer interface extraction**: Token-stream utilities (`tok_lex`, `tok_type`,
  `tok_line_leading`, `peek`, `consume`, `is_eof`, `_first`, `_second`, `_third`)
  moved from `parser.plant` into `lexer.plant` as the formal public lexer interface.
- **`tokenize(source)` wrapper**: New public entry point in `lexer.plant` that
  delegates to `scan_tokens`; downstream consumers call `tokenize()` rather than
  reaching into internal scanning state.
- **Parser decoupling**: `parser.plant` now consumes tokens exclusively through
  the formal lexer interface (`tokenize`, `peek`, `consume`, `tok_lex`, `tok_type`,
  `is_eof`) rather than sharing internal state definitions.
- **Runtime boundaries**: `plant_compat.h` reorganized into clearly delineated
  sections: execution lifecycle, assertion/verify, suite lifecycle hooks,
  import/fs/storm, networking, async, memory, and math/strings helpers.

### Compatibility Testing
- `tests/regression/compatibility/test_lexer_compat.plant`: Validates identical
  token output between the legacy inline lexer and the extracted modular lexer.
- `tests/regression/compatibility/test_parser_compat.plant`: Validates identical
  AST structure between the legacy parser and the refactored parser.
- `tests/regression/compatibility/test_runtime_compat.plant`: Validates identical
  execution output between baseline runtime calls and the reorganized compat headers.

### Internal
- Version markers moved to v0.49.55 (Makefile, src/plantc/main.plant banner,
  tests/native/run_native_tests.sh).
- Verification: regression 187→188 passing, native 20/20, generics 7/7,
  closures 6/6, `make self` converged.

## v0.49.54 - 2026 (SUITE Lifecycle Hooks: SETUP/TEARDOWN/SUITE)

### Language
- **SUITE lifecycle hooks**: `SUITE "name", SETUP expr. ... TEARDOWN expr. ... /SUITE .`
  introduces a named test-suite block with explicit setup and teardown phases.
  - `SETUP "label"` emits `plant_suite_setup_hook("label")` — runs once at suite entry.
  - `TEARDOWN "label"` emits `plant_suite_teardown_hook("label")` — runs once at suite exit.
  - `VERIFY` assertions inside a SUITE are automatically wrapped with
    `plant_verify_begin()`/`plant_verify_end()`.
  - Suites without SETUP emit no setup call; suites without TEARDOWN emit no teardown call.
  - Suites with no VERIFY assertions emit neither begin nor end markers.

### Runtime
- `plant_suite_setup()`: Blue banner "Initializing test suite."
- `plant_suite_teardown()`: Blue banner "Cleaning up test suite."
- `plant_suite_setup_hook(tx_t)`: Blue banner with hook expression
- `plant_suite_teardown_hook(tx_t)`: Blue banner with hook expression
- All four declared in `plant_compat.h`

### Parser
- SUITE keyword added to lexer (`is_keyword`/`keyword_to_type`)
- `parse_suite_stmt`: Parses optional suite name, body loop with SETUP/TEARDOWN/statement
  handling, `/SUITE .` terminator; returns `[suite_node, end_pos]` pair
- `parse_setup_stmt` and `parse_teardown_stmt`: Parse expression value, return node pairs

### Codegen
- `suite_stmt` codegen: Scans body for setup_stmt (sets `has_st`) and verify_stmt
  (sets `has_verify`); conditionally wraps body with setup/teardown/verify_begin/end calls
- `setup_stmt` codegen: `plant_suite_setup_hook(expr)`
- `teardown_stmt` codegen: `plant_suite_teardown_hook(expr)`
- `action_decl`: Conditional `plant_verify_begin/end` wrapping based on `has_verify`
  scan of body nodes (not always-on)

### Tests
- `test_suite_lifecycle.plant` + `.expected`: SUITE with SETUP/TEARDOWN/VERIFY producing
  blue banners and green assertion summary

### Internal
- Version markers moved to v0.49.54 (Makefile, src/plantc/main.plant banner,
  tests/native/run_native_tests.sh).
- Verification: regression 186→187 passing, native 20/20, generics 7/7,
  closures 6/6, `make self` converged (494199 bytes).

## v0.49.53 - 2026 (ANSI Color-Coded Output Subsystem)

### Runtime
- ANSI color macros: COLOR_RESET, COLOR_GREEN, COLOR_RED, COLOR_YELLOW, COLOR_BLUE, COLOR_CYAN, COLOR_BOLD
- plant_colorize(text, color): Safe text colorizer utility using static buffer
- plant_verify: Green/red colored VERIFY FAILED messages
- plant_verify_end: Green "All N assertions passed." / Red failure summary
- plant_compat.h: Exposed plant_colorize prototype

### Codegen
- verify_stmt: VERIFY "label" condition. → plant_verify("label", cond);
- Automatic plant_verify_begin()/plant_verify_end() wrapping in action bodies
- _from_long() wrapping for numeric boolean conditions

### Parser
- VERIFY keyword routing in parse_statement
- parse_verify_stmt: Consumes string literal label + expression condition
- "verify" keyword added to lexer is_keyword check

### Tests
- test_colors.plant: Color-coded VERIFY output regression test

### Internal
- Version markers moved to v0.49.53.

## v0.49.51 - 2026 (OOM Resolution & Memory Safeguards)

### Runtime
- g_mallocs and g_frees: global allocation counters for memory auditing
- PLANT_MAX_STRING_LEN (4096): strict string length limit enforced
- Leak audit warnings: differential checks for malloc/free mismatches
- Defensive NULL checks: validated malloc results across core functions

### Codegen
- _swap_super_prefix deduplication: tracked substituted values to prevent redundant allocations
- Length pre-check: skip string substitution for strings exceeding 4096 chars
- Loop efficiency: optimized traversal for large method lists (50+ methods)

### Stress Tests
- stress_deep_inheritance.plant: 10-level inheritance with 50 methods per level
- stress_large_methods.plant: single species with 100+ methods
- stress_from_implements.plant: combined FROM + IMPLEMENTS under heavy load
- stress_memory_pressure.plant: rapid creation/destruction cycles to detect leaks

### Documentation
- Memory safeguards and bounds documented in GAP_ANALYSIS.md
- Memory management patterns documented in Language Tour.md

### Internal
- Version markers moved to v0.49.51.

## v0.49.50 - 2026 (IMPLEMENTS Clause Refinement & Enhanced Dedup)

### Runtime
- plant_impl_iface(species_name, interface_name): Binds interface to species compliance registry
- plant_is_a(species, interface): Verifies species-interface conformance check
- Enhanced dedup mechanism: _map_replace_enhanced and _swap_super_prefix for signature-aware name conflict resolution

### Codegen
- _map_replace_enhanced: Map replacement with signature verification during deduplication
- _swap_super_prefix: Safe management of complex naming collisions in multi-interface composition

### Tests
- test_implements_basic.plant: Basic IMPLEMENTS clause syntax
- test_implements_multi.plant: Multiple interface compliance (IMPLEMENTS A, B, C)
- test_implements_from.plant: Combined FROM inheritance + IMPLEMENTS clauses
- test_dedup_conflict.plant: Method name collision resolution in inheritance
- test_signature_conflict.plant: Signature mismatch detection and handling

### Documentation
- IMPLEMENTS status transitioned from M (Medium) to S (Supported/Stable) in GAP_ANALYSIS.md

### Internal
- Version markers moved to v0.49.50.

## v0.49.20 — 2026 (Expressive features — single-quoted strings, a..b ranges, Option/Result constructors)

### Language
- **Single-quoted strings**: `'str'` literals now tokenize via the
  shared scanner — the opening quote selects the delimiter (`"` or
  `'`), escapes work in both styles (`'it\'s'` → `it's`; `\n \t \r`
  shared), and single quotes emit the same STRING token (no
  `${...}` interpolation). Previously `'` fell through as an unknown
  character and compiled into C multi-char literals (segfault class).
- **Infix range shorthand**: `a..b` lowers to
  `plant_range_list(a, b)` — depth-aware, string-literal-aware splice
  of the first range per pass, repeated until none remain; works at
  any bracket depth (`JOIN(1..4, ",")`), with numeric literal bounds
  coerced via `_wrap_math_args`. Empty/half-open edges match the
  RANGE built-in (`5..5` → `[]`).
- **Option/Result monadic constructors**: `Option_Some(x)`,
  `Option_None()`, `Result_Ok(x)`, `Result_Err(x)` lower to the
  v0.44.0 runtime tagged unions (`plant_option_some`, …).
  Underscore compounds sidestep dot-notation field-access ambiguity.
  REAP ingestion supported. The four predicates
  (`plant_is_some/none/ok/err`) are whitelisted as known numeric
  calls, so `CREATE n(NUM) TO plant_is_none(t).` and numeric SHOW
  contexts wrap correctly.

### Compiler fixes surfaced by this release
- **Variable-vs-variable `IS` on strings lowers to pointer `==`**
  (only literal comparisons compile to `strcmp`). Both new scanners
  initially hit this: delimiter matching now goes through `str_eq`.
  Documented here because any future dynamic-delimiter code must do
  the same.
- `_handle_range` first version misdetected `".."` *inside* string
  literals (self-host crash); final version tracks quote state with
  a pre-character `wasin` flag so an opening quote cannot close
  itself in the same iteration.

### Tests
- New `tests/regression/expr_features.plant` (+ `.expected`): 20
  assertions — quote styles + escapes + INCLUDES on both, range
  values/negatives/empty edges/RANGE-equivalence, monad round-trips
  (Some/Ok/Err + unwraps) and all four predicates.
- Regression suite: 162 → **163** passing files.

### Internal
- Lexer: `match_string` parameterized over the opening quote;
  `'` dispatch branch added (no interpolation path).
- Codegen: `_trim_sp`, `_range_once`, `_handle_range` passes hooked
  after the name-rewrite rows (ordering matters: COUNT-style rewrites
  must land before splicing); four monad rows; predicate whitelist
  entries in `seg_is_numeric`.
- Parser: monad names added to `is_reap_builtin`.
- Known pre-existing quirk (out of scope, documented): paren-form
  `COUNT(expr)` emits `plant_array_length()(...)` (double parens) —
  only the space form is valid today.
- Version markers moved to v0.49.20 (`Makefile`,
  `src/plantc/main.plant` banner, `tests/native/run_native_tests.sh`).
- Verification: regression 163/163, native 20/20, generics 7/7,
  closures 6/6, `make self` converged (439507 bytes).

## v0.49.19 — 2026 (Math gap closure — uniform argument handling, LOG built-in, full module namespace)

### Language
- **Legacy eight upgraded**: `ABS ROUND POW CEIL FLOOR SIN COS SQRT`
  now route through `_math_func_paren` like every other math built-in,
  so decimal literal arguments compile and keep full precision
  (`ROUND(3.7)` → 4, `POW(2.5, 2)` → 6.25, `SIN(0.5)` → 0.4794255386,
  `SQRT(2.25)` → 1.5). Previously these emitted raw C doubles against
  tx_t parameters — a hard compile error for any decimal argument.
- **`LOG` is a bare built-in**: natural logarithm as a reserved
  keyword in every call form — expression (`SHOW LOG(10)`), REAP
  ingestion (`REAP r FROM LOG(8)`), and module (`math:LOG`). x <= 0
  returns the deterministic diagnostic string from `math_log`
  ("ERR: math_log(x): x must be > 0 …"), now pinned by test.
  `math_log` itself was hardened to decode tagged small ints safely
  via `_plant_math_num` (legacy path preserved for other payloads).

### Runtime / FFI
- **Complete `math:` module namespace**: 37 new static wrappers in
  `plant_compat.h` (after the five v0.48.27 ones) — every math-family
  endpoint is now reachable as `REAP … FROM math:FUNC`: legacy eight +
  RANDOM route to their tagged-safe `plant_*` helpers; the v0.49.17 /
  v0.49.18 tiers route to their `math_*` helpers. 42 module endpoints
  in total (incl. SIGN/CLAMP/PI/E).

### Tests
- New `tests/regression/math_fix.plant` (+ `.expected`): 31
  assertions — decimals on the legacy eight (incl. negatives:
  `ABS(-2.5)`, `ROUND(-3.2)`), bare LOG incl. domain error and
  `EXP(LOG(10))` round-trip, and 15 module-form probes across both
  math tiers plus PI.
- Regression suite: 161 → **162** passing files.

### Internal
- Codegen: 8 rows migrated `_handle_func_paren` → `_math_func_paren`;
  new LOG row placed after the shadow-order guard block. Parser:
  `is_reap_builtin` gains a LOG entry. Lexer: LOG keyword registered
  ahead of LOG10/LOG2/LOG1P.
- Version markers moved to v0.49.19 (`Makefile`,
  `src/plantc/main.plant` banner, `tests/native/run_native_tests.sh`).
- Verification: regression 162/162, native 20/20, generics 7/7,
  closures 6/6, `make self` converged (433695 bytes).

## v0.49.18 — 2026 (Advanced math library — 11 built-ins; math subsystem mature)

### Language
- **Advanced math built-ins (v0.49.18)**: 11 functions shipped as bare
  expression built-ins with FFI extern declarations — completing the
  math subsystem:
  - **Reciprocal trig**: `SEC(x)` = 1/cos(x), `CSC(x)` = 1/sin(x)
    (zero divisors report `-nan`).
  - **Inverse hyperbolics**: `ASINH`, `ACOSH` (domain x >= 1),
    `ATANH` (domain |x| < 1) — violations report `-nan`.
  - **Special functions**: `ERF`, `ERFC` (error functions),
    `GAMMA` (tgamma), `LGAMMA` (log-gamma; domain x > 0).
  - **Utilities**: `EXP2(x)` = 2^x, `LOG_BASE(x, b)` =
    log(x)/log(b) with domain checks x > 0, b > 0, b != 1
    (violations report `-nan`; no silent ln fallback).

### Compiler fixes surfaced by this release (all verified by the new suites)
- **Decimal-literal crash fix (`_wrap_math_args`)**: expression-path
  numeric literal args of the math built-ins now wrap in
  `_from_double(...)` via a depth-aware arg scanner
  (`_math_func_paren`). Previously `MIN(2.5, 1.5)`/`TAN(0.5)` emitted
  raw C doubles against tx_t parameters — a hard compile error
  (integers only worked through the tagged-int accident; ints beyond
  ±4096 were also unsafe).
- **Tagged-long index corruption fix**: `_wrap_math_args` consumes the
  REAP'd result of `_find_substr`, whose GIVE lowers to a raw tagged
  long; arithmetic on it must go through the tagged-safe converter
  `plant_rw_arg_long` (now whitelisted as a known numeric call in
  `seg_is_numeric`). The previous `_to_long` attempt dereferenced
  small pointer values (`atol(*(char*)1)`) and segfaulted the
  compiler whenever a match offset was nonzero.
- **Keyword-shadow order guard**: ASINH/ACOSH/ATANH/LGAMMA and
  ASIN/ACOS/ATAN rows moved ahead of SINH/COSH/TANH/GAMMA/SIN/COS/TAN
  in `translate_expr` — `_handle_func_paren` splits on plain
  substrings, so `"SINH ("` carved `"ASINH ("` into
  `"A"+"math_sinh(...)"` (`Amath_sinh` link errors). Rewritten
  lowercase C can never rematch an uppercase needle.
- **`LOG_BASE` domain tightened**: removed the silent natural-log
  fallback for invalid bases — all domain violations report `-nan`.

### Tests
- New `tests/regression/math_final.plant` (+ `.expected`): 33
  assertions — sec/csc values, inverse-hyperbolic domains
  (`ACOSH(0.5)`, `ATANH(1)` → `-nan`), erf/erfc/gamma/lgamma,
  exp2, log_base domain matrix (x=0, b=1, b=0 → `-nan`), and nested
  compositions (`EXP2(LOG_BASE(8, 2))` → 8,
  `MIN(MAX(GAMMA(5), 1), 30)` → 24).
- `tests/regression/math_extra.plant` (v0.49.17) finally wired in:
  `.expected` generated and the suite passes — it had never been
  linked before (missing fixture + decimal/shadow compile failures).
- Regression suite: 159 → **161** passing files.

### Internal
- Runtime helpers in `runtime/c/plant_runtime.c` (after `math_hypot`);
  extern declarations in `runtime/c/plant_compat.h`
  ("v0.49.18 Advanced math library" block). Codegen: `_is_num_literal`,
  `_wrap_math_args`, `_math_func_paren` helpers + shadow-order guard
  rows in `src/plantc/codegen_c.plant`; lexer keywords in
  `src/plantc/lexer.plant` (`keyword_to_type`); REAP ingestion chains
  in `is_reap_builtin` (`src/plantc/parser.plant`) covering both the
  v0.49.17 names (previously missing) and the 11 new ones.
- Version markers moved to v0.49.18 (`Makefile`,
  `src/plantc/main.plant` banner, `tests/native/run_native_tests.sh`).
- Verification: regression 161/161, native 20/20, generics 7/7,
  closures 6/6, `make self` converged (433540 bytes).

## v0.49.16 — 2026 (List Built-ins, Batch 2 — Lists Complete)

### Language
- **List built-ins (batch 2)**: `FLATTEN`, `CHUNK`, `ZIP`,
  `FILTER_GT`, `FILTER_LT` are bare expression built-ins — the legacy
  `lists` (14) surface is now **100% expression built-ins**:
  - `FLATTEN(list)` → `plant_list_flatten` — single-level unnest:
    direct sub-lists are spliced in; deeper nesting and maps pass
    through (`FLATTEN([[1, [2]]])` → `[1, [2]]`).
  - `CHUNK(list, size)` → `plant_list_chunk` — sub-lists of at most
    `size` elements (`size < 1` or empty input → `[]`).
  - `ZIP(a, b)` → `plant_list_zip` — element-wise `[a_i, b_i]` pair
    lists, truncated to the shorter input; a non-list argument → `[]`.
  - `FILTER_GT(list, t)` / `FILTER_LT(list, t)` →
    `plant_list_filter_gt`/`plant_list_filter_lt` — keep elements
    strictly greater / strictly less than the numeric threshold
    (tagged-int/strtod parsing; non-numeric elements dropped; the
    literal `0` threshold is supported).
- All five are lexer keywords (`is_keyword`/`keyword_to_type` in
  `src/plantc/lexer.plant`), recognized by the REAP general-expression
  classification (`is_reap_builtin` in `src/plantc/parser.plant`), and
  translated in `_handle_func_paren` (`src/plantc/codegen_c.plant`) to
  new runtime helpers in `runtime/c/plant_runtime.c` (declared in
  `plant_runtime.h` and as `extern` in `plant_compat.h`). Nested calls
  work (`SORT(FILTER_GT([5, 1, 4, 2], 2))` → `[4, 5]`).
- Fix included: numeric-argument parsing in the new filters treats the
  literal `0` (which arrives as a NULL tx_t slot) as the value `0`,
  matching the `_slice_arg` convention; NULL *elements* are still
  skipped.

### Tests
- New `tests/regression/list_builtins2.plant` (+ `.expected`): all five
  built-ins — edge cases (empty lists, `size < 1` chunks, oversized
  single-chunk, truncated zip, strict boundaries, non-numeric elements,
  `0` threshold), chunk counts via `COUNT var` (the space form), and
  nested calls.

### Internal
- Version marker moved to v0.49.16 (`Makefile`,
  `src/plantc/main.plant` version banner,
  `tests/native/run_native_tests.sh` check).
- Verification: regression 159/159, native 20/20, generics 7/7,
  closures 6/6, `make self` converged. CI-equivalent commands
  (`make all && make self && make test`) pass on the v0.49.16 HEAD.

## v0.49.17 — 2026 (Extended math library — 17 built-ins complete)

### Language
- **Math built-ins (v0.49.17)**: 17 functions shipped as bare expression built-ins and FFI module bindings — completing feature parity for the math module:
  - **Bounds / combinators**: `MIN`, `MAX` — 2-argument comparison returning the original text of the smaller/larger argument (via `_plant_math_num` coercion). Edge cases: empty args → `"0"`; numeric literals safely decoded (previously `MIN(3,5)` with int literals segfaulted; now coerces via `_plant_math_num`; string args still work directly).
  - **Trigonometric**: `TAN` / `COT` — standard C library functions; `TAN` dispatches on type (string → `REVERSE`-style behavior preserved; numeric → `math_tan`).
  - **Inverse trigonometric**: `ASIN`, `ACOS`, `ATAN`, `ATAN2` — with domain validation; `ASIN`/`ACOS` return `-nan` for out-of-range inputs; `ATAN2` supports 2-argument form.
  - **Hyperbolic**: `SINH`, `COSH`, `TANH` — standard `sinh/cosh/tanh`.
  - **Exponential / logarithmic**: `EXP` / `EXPM1` (expm1 for numerical stability near 0) / `LOG10`, `LOG2`, `LOG1P` (log1p for stability near 0).
  - **Pythagorean**: `HYPOT` — hypot(x,y) with safe argument coercion.

- **Expression built-ins**: 17 functions all recognized as keywords (`is_keyword` in `lexer.plant`) and dispatched through `_handle_func_paren` to their C math runtime equivalents (`math_tan`, `math_atan`, etc.). All also support REAP ingestion (`REAP r FROM TAN(3).`, `REAP r FROM MIN(1,2).`).

- **FFI module**: `math:` module now provides all 17 functions via `REAP r FROM math:TAN(3).`, `REAP r FROM math:MIN(1,2).` etc.; all declared as `extern` wrappers in `plant_compat.h`.

### Tests
- New `tests/regression/math_extra.plant` (+ `.expected`): 32 assertions covering
  bounds (MIN/MAX edge cases), trig (TAN, COT, ASIN, ACOS, ATAN, ATAN2), 
  hyperbolics (SINH, COSH, TANH), logs (EXP, EXPM1, LOG10, LOG2, LOG1P), 
  and HYPOT. Includes invalid-input tests (domain errors, -nan returns). 
  Tests FFI module calls (`math:LOG(100)`) and nested expressions 
  (MIN(MAX(...), ...)).

### Internal
- 17 new math helpers implemented in `runtime/c/plant_runtime.c` using
  `_plant_math_num` → `_plant_math_result` pattern for safe numeric coercion;
  math_min/math_max patched to handle tagged integers (previously crashed
  on numeric literal args).
- 15 new extern declarations appended to `runtime/c/plant_compat.h` (after
  legacy v0.48.27 bindings: LOG PI E SIGN CLAMP → now 19 total).
- 17 new keyword registrations in `src/plantc/lexer.plant` (`is_keyword` /
  `keyword_to_type`) — MIN MAX TAN ATAN COT ASIN ACOS ATAN2 SINH COSH TANH
  EXP EXPM1 LOG10 LOG2 LOG1P HYPOT, all mapped to token type `"T"`.
- `_handle_func_paren` in `src/plantc/codegen_c.plant` updated with 17
  new translation rows mapping parenthesized expressions to math C helpers.
- `is_reap_builtin` in `src/plantc/parser.plant` extended for 17 math functions,
  enabling `REAP` ingestion form.
- Version marker moved to v0.49.17 (`Makefile`,
  `src/plantc/main.plant` version banner,
  `tests/native/run_native_tests.sh` check).
- Verification: regression 160/160 (new math_extra tests pass),
  native 20/20, generics 7/7, closures 6/6, `make self` converged.
  CI-equivalent commands (`make all && make self && make test`) pass on the
  v0.49.17 HEAD.

## v0.49.15 — 2026 (List Built-ins, Batch 1)

### Language
- **List built-ins (batch 1)**: `REVERSE`, `RANGE`, `SORT`, `INCLUDES`,
  `INDEX_OF`, `UNIQUE`, `AVERAGE`, `MEDIAN` are now bare expression
  built-ins — no `strings:`/`lists:` module prefix required:
  - `REVERSE(list)` → `plant_list_reverse` — element-wise reversed
    copy; string arguments still reverse characters (dispatch keeps
    the v0.48.38e string behavior).
  - `RANGE(start, end)` → `plant_range_list` — half-open `[start, end)`
    integer list (`end <= start` → `[]`, negative bounds supported).
  - `SORT(list)` → `plant_list_sort` — ascending sort
    (`plant_sort(list, "")`; the v0.48.29 `SORT lst [ASC|DESC]`
    statement is unchanged).
  - `INCLUDES(list, item)` → `plant_list_includes` — element
    membership `"1"`/`"0"`; string arguments keep substring semantics.
  - `INDEX_OF(list, item)` → `plant_list_index_of` — first matching
    index or `"-1"`.
  - `UNIQUE(list)` → `plant_list_unique` — first-occurrence dedupe
    copy, order preserved.
  - `AVERAGE(list)` → `plant_list_average` — mean of numeric elements
    (fractional results like `2.5`; empty/unnumeric → `"0"`).
  - `MEDIAN(list)` → `plant_list_median` — median of numeric elements
    (even count → mean of the middle pair).
- All eight are lexer keywords (`is_keyword`/`keyword_to_type` in
  `src/plantc/lexer.plant`), recognized by the REAP general-expression
  classification (`is_reap_builtin` in `src/plantc/parser.plant`), and
  translated in `_handle_func_paren` (`src/plantc/codegen_c.plant`) to
  list-aware runtime helpers (new section in
  `runtime/c/plant_runtime.c`, declared in `plant_runtime.h` and as
  `extern` in `plant_compat.h`). Nested calls work
  (`AVERAGE(UNIQUE([1, 1, 2, 3]))` → `2`).
- Backward compatible: string `REVERSE`/`INCLUDES` behavior is
  preserved via runtime dispatch on the first argument's type.

### Tests
- New `tests/regression/list_builtins.plant` (+ `.expected`): all eight
  built-ins — edge cases (empty lists, `end <= start` ranges, negative
  bounds, absent items, single-element median, all-unnumeric lists),
  string-dispatch regression (`REVERSE("abc")`, `INCLUDES("hello",
  "lo")`), and nested calls.

### Internal
- Version marker moved to v0.49.15 (`Makefile`,
  `src/plantc/main.plant` version banner,
  `tests/native/run_native_tests.sh` check).
- Verification: regression 158/158, native 20/20, generics 7/7,
  closures 6/6, `make self` converged. CI-equivalent commands
  (`make all && make self && make test`) pass on the v0.49.15 HEAD.

## v0.49.14 — 2026 (Community Profile & CI Pause)

### Community
- **`CODE_OF_CONDUCT.md`**: Contributor Covenant v2.1 with the standard
  pledge, standards, enforcement responsibilities/scope, and the four-step
  enforcement ladder. The enforcement contact is a placeholder to be filled
  in by the maintainer (`[INSERT CONTACT METHOD — e.g. maintainer email or
  GitHub username]`).
- **`SECURITY.md`**: vulnerability reporting guidance — private channels
  only (GitHub Security tab / security advisory, or email placeholder),
  never a public issue; acknowledgement within 48h and status updates
  every 7 days; supported-versions table (0.49.x supported, <= 0.48
  end-of-life); coordinated disclosure policy (fix-then-disclose, reporter
  credited on request, early disclosure only for in-the-wild exploitation).
- **Issue forms** (`.github/ISSUE_TEMPLATE/`): `bug_report.yml` (GitHub
  Issue Forms — version, description, reproduction steps, actual vs
  expected output, environment) and `feature_request.yml` (problem,
  proposed solution, alternatives considered, example usage), both with
  required-field validation and appropriate labels.
- **Pull request template** (`.github/PULL_REQUEST_TEMPLATE.md`):
  description, type-of-change checkboxes (bugfix/feature/compiler/runtime/
  docs/infrastructure/breaking), testing checklist (`make all`, `make self`,
  `make test` with current suite counts), and a contributor checklist
  (project conventions, CONTRIBUTING.md read, CHANGELOG updated, no
  secrets).

### Infrastructure
- **GitHub Actions removed**: `.github/workflows/test.yml` deleted — CI is
  temporarily disabled due to a billing issue on the account. All local
  verification commands are unaffected: `make all && make self && make test`
  remain the canonical check (regression 157/157, native 20/20, generics
  7/7, closures 6/6 on the v0.49.14 HEAD).

### Internal
- Version marker moved to v0.49.14 (`Makefile`,
  `src/plantc/main.plant` version banner,
  `tests/native/run_native_tests.sh` check).
- No compiler, runtime, or test-suite source changes in this release.

## v0.49.13 — 2026 (Bare String Built-ins)

### Language
- **Bare string built-ins**: `INCLUDES`, `STARTS_WITH`, `ENDS_WITH`,
  `REPEAT`, `PAD`, and `PAD_LEFT` are now first-class expression
  built-ins — callable without the `strings:` module prefix:
  - `INCLUDES(text, sub)` → `string_includes` — `"1"` when `text`
    contains `sub` (empty `sub` always matches), else `"0"`.
  - `STARTS_WITH(text, pre)` → `string_starts_with` — `"1"` when `text`
    starts with `pre`, else `"0"`.
  - `ENDS_WITH(text, suf)` → `string_ends_with` — `"1"` when `text`
    ends with `suf`, else `"0"`.
  - `REPEAT(text, count)` → `string_repeat` — `text` repeated `count`
    times (`count <= 0` yields `""`).
  - `PAD(text, length, pad_char)` → `string_pad` — right-pad `text` to
    `length` with `pad_char` (passthrough when already at/over length).
  - `PAD_LEFT(text, length, pad_char)` → `string_pad_left` — left-pad
    `text` to `length` with `pad_char`.
- The six names are registered as lexer keywords
  (`is_keyword`/`keyword_to_type` in `src/plantc/lexer.plant`), recognized
  by the REAP general-expression classification (`is_reap_builtin` in
  `src/plantc/parser.plant`), and translated in `_handle_func_paren`
  (`src/plantc/codegen_c.plant`). They work in REAP, SHOW, and SET value
  positions, including nested calls (`PAD_LEFT(REPEAT("y", 2), 4, "_")`).
- Backward compatible: the module-qualified forms (`strings:REPEAT`, ...)
  remain fully supported and unchanged.

### Tests
- New `tests/regression/string_builtins.plant` (+ `.expected`): bare-form
  coverage of all six built-ins with edge cases (empty strings, empty
  substring/suffix/prefix, zero-iteration repetition, target length below
  input length) plus expression-position and nested-call checks.

### Internal
- Version marker moved to v0.49.13 (`Makefile`,
  `src/plantc/main.plant` version banner,
  `tests/native/run_native_tests.sh` check).
- Verification: regression 157/157, native 20/20, generics 7/7, closures
  6/6, `make self` converged. CI-equivalent commands
  (`make all && make self && make test`) pass on the v0.49.13 HEAD.

## v0.49.12 — 2026 (Infrastructure & Developer Experience)

### Infrastructure
- **CI workflow** (`.github/workflows/test.yml`): runs on push and pull
  request against `main` and `Safarna`. Steps: checkout, install `gcc` +
  `make`, `make all`, `make self` (self-hosting convergence check), and
  `make test` (native + generics + closures + regression suites). The
  workflow fails if any step fails, and always reports the pipeline result.
- **`CONTRIBUTING.md`**: new-contributor guide — repository layout, build
  commands (`make all`, `make self`, `make test`), how the test suites work
  (`.plant` + `.expected` diffs), code style conventions (two-space indent,
  digit-free concatenated variable names, stepwise two-operand `SET`
  concats — the stale v1 bootstrap constraints), and issue/PR submission
  guidance against `main` / `Safarna`.
- **Syntax highlighting** (`syntaxes/plantlang.tmLanguage.json`): TextMate
  grammar (`source.plantlang`) covering the full keyword set from
  `src/plantc/lexer.plant` plus expression built-ins (PICK, FIND, COUNT_OF,
  SLICE, JOIN, FIRST, LAST, SUM, UPPER, LOWER, TRIM, REVERSE, ABS, ROUND,
  POW, CEIL, FLOOR, RANDOM, SIN, COS, SQRT, HAS, ANY, ALL), type
  annotations (NUM, SCL, TX, FACT, LIST, MAP, STRUCT, ENUM, VEIN), strings
  with `${...}` interpolation, `#` comments, numbers, and operators.
- **`docs/BUILD.md`**: new "Binary Artifacts in Git" section documenting why
  `bin/Chloroplast`, `dist/Chloroplast`, and `build/plantc_v*` are tracked —
  they are bootstrap seeds for the self-hosting chain; `dist/Chloroplast` is
  the immutable v1 seed (never rebuilt, kept by `make clean`) and the
  artifacts are required for `make self` convergence.

### Internal
- Version marker moved to v0.49.12 (`Makefile`,
  `src/plantc/main.plant` version banner,
  `tests/native/run_native_tests.sh` check).
- Verification: regression 156/156, native 20/20, generics 7/7, closures
  6/6, `make self` converged. CI-equivalent commands
  (`make all && make self && make test`) pass on the v0.49.12 HEAD.

## v0.49.11 — 2026 (Native Method Calls `a.method(args)`)

### Language
- **`obj.method(args)` lowers directly to the runtime API.** `IDENT .`
  followed by a method name and `(` is recognized at parse time
  (`src/plantc/parser.plant` `parse_method_call`, reached from the
  `parse_field_access` LPAREN guard) and emits the call inline —
  no intermediate AST. Method map:
  `push(x)` → `plant_list_push(obj, x)`,
  `pop()` → `plant_list_pop(obj)`,
  `get(k)` → `plant_map_get(obj, k)`,
  `put(k, v)` → `plant_map_set(obj, k, v)`,
  `has(k)` → `plant_map_has(obj, k)`.
  - the receiver selection mirrors field access: a trailing bare
    `IDENT .` binds to that IDENT only (`"x=" + m.push(v)` emits
    `_cat("x=", plant_list_push(m, v))`); any other tail (previous
    emission, indexed expression) takes the whole collected text.
  - arguments are the collected value up to the closing `)` at depth 0
    (commas, nesting, field access and nested method calls inside
    arguments all work); empty argument lists emit no comma
    (`plant_list_pop(l)`).
  - `has` is parser-wrapped in `_from_long(...)` so `SHOW m.has(k)`
    prints text; in arithmetic the numeric wraps in codegen treat
    `_from_long(` as a lookup prefix (`_to_long(_from_long(...))`).
  - chained forms compose through the existing `collect_value` loop:
    `l.push("x").pop()` → `plant_list_pop(plant_list_push(l, "x"))`,
    `nested.get("pt").get("y")` →
    `plant_map_get(plant_map_get(nested, "pt"), "y")`,
    `m.get("x").name` → `_map_get(plant_map_get(m, "x"), "name")`.
  - unknown method names are rejected at parse time (falls back to the
    pre-v0.49.11 behavior).
- **Bare method statements.** `m.put("k", "v").` / `l.push("x").` as a
  statement is recognized by the statement dispatcher
  (`IDENT . IDENT (`), collected whole (chains included) and emitted
  raw via the new `emit_stmt` node (`plant_map_set(m, "k", "v");`).
- **Line-aware statement termination.** The lexer marks the first token
  of each line (3rd element of the token pair) and `parse_field_access`
  refuses to chain onto a line-leading IDENT. A chain never opens a
  new line, so `SHOW m.name.` followed by `m.put(...)` on the next
  line stays two statements instead of merging into one expression.
  (Same-line bare statements after an expression remain ambiguous and
  chain, as before.)
- **`plant_map_get`/`plant_map_has` dispatch on representation.**
  runtime/c/plant_runtime.c: array magic (`PLANT_ARRAY_MAGIC`) → linear
  pair-list scan returning `""` on miss; PlantMap hash maps → probe
  (NULL on miss, preserving the FFI-struct marshalling contract).
  `plant_list_pop` pops the last element (`""` when empty).
- **Regression:** `tests/regression/method_call.plant` covers
  get/has/put on map-backed lists, arithmetic (`m.get("count") + 1`),
  string concat (`"x=" + m.get("name")`), bare mutation statements,
  `push`/`pop` on lists, chained method-method, nested
  method-method, and method results feeding index/field positions.
  156/156 regression tests pass.

### Internal
- `is_lookup_prefix` (codegen_c.plant) unifies the lookup-prefix test
  (`_map_get(`, `plant_map_get(`, `plant_list_pop(`, `_from_long(`) —
  replaces the hardcoded `_map_get(` checks in the has_str exclusions,
  the raw-`+` `_to_long` wraps and the CREATE/GIVE numeric wraps.
- Stale-v1 bootstrap constraint: all new emission strings are built in
  stepwise two-operand concats (no `+` rewrites inside call parens);
  token flags are passed as `_from_long(ls0)` (plant_list_make varargs
  read pointers, so a raw NUM would crash the generated C).
- Version marker moved to v0.49.11 (`Makefile`,
  `src/plantc/main.plant` version banner,
  `tests/native/run_native_tests.sh` check).

## v0.49.10 — 2026 (Native Field Access `a.b.c`)

### Language
- **`a.b.c` field access lowers to `_map_get` lookups.** `IDENT . IDENT`
  sequences are recognized at parse time (`src/plantc/parser.plant`
  `parse_field_access`) and emit
  `_map_get(target, "field")` (runtime `_map_get`, plant_compat.h:
  421). Chained access nests: `a.b.c` →
  `_map_get(_map_get(a, "b"), "c")`. Field values are looked up in
  map-backed lists (`plant_list_make` key/value pairs).
  - target binding: a trailing bare `IDENT .` binds to that IDENT only
    (so `"x=" + m.name` emits `_cat("x=", _map_get(m, "name"))`); any
    other tail (chained access, indexed expression like `x[0].name`,
    string concat) uses the whole expression as the target.
  - the token after `.` must be a non-keyword IDENT (keywords such as
    `ORIF` excluded) and must not be followed by `(` or `:`, so
    `SHOW "big". ORIF ...` stays a plain statement.
  - indexing after a field (`m.list[0]`) and further chaining both
    work: `m.list[0]` → `plant_list_get(_map_get(m, "list"), 0)`.
- **Numeric coercion for field reads.** In arithmetic, a top-level
  `_map_get(...)` segment next to a numeric literal is wrapped in
  `_to_long(...)`: `m.count + 1` → `_from_long(_to_long(_map_get(m,
  "count"))+1)` (same for `CREATE x(NUM) TO m.f.` and `GIVE`/`SET`
  numeric targets). Field + field or field + string has no numeric
  literal, so it concatenates (`_cat`) like any other text value.
- **String concat classification fixed.** `has_str` in `_handle_cat`
  is now computed over the expression parts (including the final tail
  segment — previously the tail after the last ` + ` was missed, so
  async codegen emitted raw `code + "..."` C concatenation); a segment
  is a string when it starts with `"` or contains quotes inside a
  call (e.g. `_cat3("", s, "")`), except `_map_get(`-prefixed
  segments (kept arithmetic).
- **Field access does not misfire inside action calls.** A field
  pattern where the preceding token is not a bare IDENT (e.g.
  `f(a.b)` — preceded by `(`) is left untouched, and keyword tokens
  are excluded from field position.
- **Regression:** `tests/regression/field_access.plant` covers single
  field, chained (2- and 3-level) access, field + literal arithmetic,
  field + string concat, `SET`/`CREATE NUM` from a field, index into
  a field list, and statement-terminator safety. 155/155 regression
  tests pass.

### Internal
- Stale-v1 bootstrap constraint: the new code paths keep all
  concatenated variable names digit-free (`has_str`/`has_lit` raw
  `+` path in v1) and avoid `+` rewrites inside call parens.
- Version marker moved to v0.49.10 (`Makefile`, `src/plantc/main.plant`
  version banner, `tests/native/run_native_tests.sh` check).

## v0.49.9 — 2026 (Expression Support in REAP Statements)

### Language
- **`REAP` accepts general expressions.** The value after `FROM` is
  classified at parse time: `IDENT ,` action calls, `IDENT :` module
  calls, `IDENT [types]` generic calls, and `IDENT (...)` calls to
  non-builtin actions keep the legacy forms; everything else is a
  general expression translated exactly like a `SET`/`SHOW` value
  (`src/plantc/parser.plant` `parse_reap_stmt`):
  - translate-time builtins now work in REAP position — previously
    `REAP f FROM FIND(t, s).` emitted a raw `FIND(...)` C call and
    failed at link time; now it emits `plant_find(...)`. Covered:
    `FIND JOIN SLICE UPPER LOWER ABS ROUND LEN FIRST LAST SUM TRIM
    REVERSE POW CEIL FLOOR RANDOM SIN COS SQRT HAS ANY ALL PICK
    COUNT_OF TAP INFUSE ABSORB SEAL TEST COUNT NOW ANALYZE TYPEOF`.
  - arithmetic (`REAP n FROM 2 + 3.`), indexing (`REAP x FROM a[0].`,
    `REAP p FROM lst[1] + "!".`), literals, and bare variables.
  - nested combinations, e.g. `REAP b FROM UPPER(JOIN(lst, ":")) + "!".`
    — embedded action calls are registered for prototype emission via
    the `callees_of` expression scan.
- **Numeric results are stored as text.** `REAP n FROM 2 + 3.` emits
  `_from_long(2 + 3)` so the target holds `"5"` — concatenation and
  `SHOW` never dereference raw integer bits (matches the existing
  numeric-action REAP convention).
- **Fixed `REAP x FROM a[0].` undeclared-target bug** (v0.49.6): the
  expression-REAP target is now registered by `collect_used_walk` and
  declared `tx_t ""` like any other REAP target.

### Tests
- New `tests/regression/reap_expr.plant/.expected`: 20 cases covering
  FIND/JOIN/SLICE/UPPER/LOWER/ABS/ROUND, COUNT, arithmetic, indexing,
  index+concat, nested builtins with embedded action calls, and the
  preserved legacy forms (zero-arg bare call, parens call, comma call,
  module call). Full regression: 154/154; native: 20/20.

### Internal
- New parser helper `is_reap_builtin` (name → "1"/"0") mirroring the
  `_handle_func_paren`/`_handle_func`/`_ni_replace` list in
  `translate_expr`; classification guards keep legacy action forms
  (bare `IDENT .` zero-arg calls, `(` calls, generic `[types]`).
- Version bump 0.49.8 → 0.49.9 (Makefile, compiler banner, native test
  runner version check).

## v0.49.8 — 2026 (STEP Spacing Refinement & CYCLE Documentation)

### Language
- **Robust `STEP` parsing in numeric `CYCLE` loops.** `STEP` is now a
  registered lexer keyword, and the to-expression is collected
  token-wise with `collect_until_keyword` (stopping at `STEP` or the
  header comma) instead of the old `strings:SPLIT` on `" STEP "`. All
  spacing and attachment variants parse identically:
  - `CYCLE i FROM 1 TO 5 STEP 2` (standard)
  - `CYCLE i FROM 1 TO 5 STEP  2` (multiple spaces)
  - `CYCLE i FROM 1 TO 5 STEP2` (attached value — recovered from the
    single NAME token via a guarded prefix split: word-boundary
    `STEP` followed by a digit; a variable like `MYSTEP2` or a bare
    trailing `STEP` never matches)
  - `CYCLE i FROM 5 TO 1 STEP -2` and `CYCLE i FROM 5 TO 1 STEP-2`
    (negative increments — the `-` is its own MINUS token, so both
    lex identically)
- **Validation guardrails unchanged and re-verified:** a statically
  zero `STEP` (`STEP 0`, `STEP 2 - 2`) is a compile-time error
  (`#error STEP cannot be 0`) at both the parser and codegen layers;
  runtime steps keep the `!= 0` guarded bound so a zero-valued
  runtime step iterates zero times instead of spinning.

### Tests
- Added `tests/regression/cycle_step_spacing.plant` (+ `.expected`):
  standard, multi-space, attached, spaced/attached negative, spaced
  minus (`STEP - 3`), and runtime-variable steps.
- Existing `cycle_*` suite re-verified (153 regression tests pass).

### Documentation
- `Language Tour.md`: new **CYCLE** section after SEASON — collection
  iteration (`CYCLE item IN list`), indexed collection iteration
  (`CYCLE item, idx IN list`), range iteration (`CYCLE i FROM lo TO
  hi`), stepped ranges (`… STEP k`), step sign/bound semantics,
  zero-step guard, and BREAK/CONTINUE integration.

### Housekeeping
- Version bump 0.49.7 → 0.49.8 (Makefile, compiler banner, native test
  suite version check).

## v0.49.5 — 2026 (Map Literals `{k: v}`)

### Language
- **Native map literals:** `{k: v}` — braces tokenize as LBRACE/
  RBRACE (lexer), statement collectors depth-count braces (so a
  terminator inside `{ ... }` never fires), and `translate_expr`'s
  `_map_literal` scanner (quote-/escape-aware, brace-depth aware,
  identifier-guarded so legacy `Obj{...}` struct-literal syntax
  passes through) rewrites `{ ... }` spans into a chain of pair-list
  MAP setters:
  - `{ "name": "plant", "year": 2026 }` →
    `plant_map_set(plant_map_set(plant_map_create(), "name",
    "plant"), "year", _from_long(2026))`
  - nested `{ ... }` and `[ ... ]` values recurse (`_seg_map` →
    `_push_pair` → `_find_colon` for the key/value split, values via
    the shared `_enc_el` coder — same `_from_long` / TRUE-FALSE /
    operator wrapping as list elements)
  - `{}` → `plant_map_create()`; `{k: v}` at expression start or
    after `(`/`,`/space opens a span; `)`/`]`/identifier before `{`
    blocks (struct-literal syntax)
  - `_map_literal` runs before `_list_literal` so `[ {a: 1} ]` and
    `{ a: [1, 2] }` both convert; the emitted chains contain no
    brackets or bare colons for later passes.
- **Runtime — pair-list map API:** `tx_t plant_map_create(void)` and
  `tx_t plant_map_set(tx_t map, tx_t key, tx_t value)` (plant_runtime.c)
  build the language's pair-list MAP representation (PlantArray,
  kind = 1) — the same form `LINK`, `_map_get`, `plant_map_to_string`,
  `json_stringify` and the LISTEN/HARVEST request maps consume.
  `set` upserts (existing key replaced, like `plant_link`) and returns
  the map so calls chain. Exposed via plant_runtime.h + plant_compat.h.
- **Hash-table API renamed:** the C-level `plant_map_create(size_t)` /
  `plant_map_set(PlantMap*, const char*, void*)` (struct/FFI
  marshalling, mock profiles) became `plant_map_hash_create` /
  `plant_map_hash_set`; `plant_map_get` / `plant_map_free` unchanged.
  Emitters updated: struct `plant_<T>_to_map` codegen, `ffi_ext.plant`.
- **Serializer:** empty pair-list MAPs now render `{}` instead of `[]`
  (kind-aware `_plant_ser`).

### Tests
- Added `tests/regression/map_literal.plant` (+ `.expected`): simple
  `{ "name": "plant", "kind": "tree", "year": 2026 }`, nested
  `{ "a": [1, 2], "b": { "x": n + 1 } }`, arithmetic value
  `"lit": 1 + 1`, empty `{}`, reads via `_map_get`, serialization via
  `plant_map_to_string` and `json_stringify`.
- STRUCT/native suites re-verified against the renamed hash API.

### Documentation
- `Language Tour.md`: Maps section documents the literal syntax,
  nesting, incremental `plant_map_create()`/`plant_map_set()`
  building, serialization, and the hash-table rename.

### Housekeeping
- Version bump 0.49.4 → 0.49.5 (Makefile, compiler banner, native test
  suite version check).

## v0.49.4 — 2026 (List Literals Syntax)

### Language
- **Native list literals:** `[e1, e2, ...]` — brackets tokenize as
  LBRACKET/RBRACKET and pass through the statement collectors, and
  `translate_expr`'s `_list_literal` scanner (quote/escape-aware,
  nesting-depth aware, identifier-guarded so `name[expr]` indexing is
  never mistaken for a literal) rewrites them into
  `plant_list_make(count, e1, ...)`:
  - integer literals wrap in `_from_long` (`[1, 2, 3]` →
    `plant_list_make(3, _from_long(1), _from_long(2), _from_long(3))`)
  - nested lists recurse through `_seg_list` (`["a", ["b", "c"], "w"]`
    → `plant_list_make(3, "a", plant_list_make(2, "b", "c"), "w")`)
  - `[]` → `plant_list_make(0)`
  - bare TRUE/FALSE materialize as the quoted strings (SUM skips them)
  - strings, string variables, and list variables pass through as tx_t
- **Fix (v0.49.4):** NUM-typed expressions inside a literal — `[x + 1,
  y]` — previously emitted the raw C `long` (pointer-truncated into
  the varargs list, segfaulting the serializer). `_push_el` now wraps
  unquoted elements containing an arithmetic operator in
  `_from_long(...)` → `plant_list_make(2, _from_long(x + 1), y)`.
- **Element access:** `name[expr]` → `plant_list_get(name, expr)`
  (C-side `handle_brackets`, pre-existing; single-level only — chained
  indexing like `b[1][0]` remains unsupported).
- Existing syntax (v0.48.38c-era): literals in expression/call
  positions (`JOIN([m1, [2, 3, 4]], ",")`) already worked and stay
  covered by `join.plant`.

### Tests
- Added `tests/regression/list_literal.plant` (+ `.expected`): simple
  `[1, 2, 3]`, nested `["x", ["y", "z"], "w"]`, empty `[]`, mixed
  `[n + 1, y]` (NUM expression + TX variable), arithmetic `[1 + 1, 5]`,
  literal in SHOW position, and `name[expr]` element access (single +
  nested-list via `JOIN(b[1], "-")`).

### Documentation
- `Language Tour.md`: Lists section documents the literal syntax,
  element access, nesting, and the unsupported cases (chained
  indexing, string concatenation inside a literal).

### Housekeeping
- Version bump 0.49.3 → 0.49.4 (Makefile, compiler banner, native test
  suite version check).

## v0.49.3 — 2026 (JSON Bodies in HTTP Subsystem)

### Network
- **HARVEST JSON client parsing:** `HARVEST url AS resp JSON.` routes to
  the new `plant_net_harvest_json` runtime function — the request runs
  as `plant_net_harvest`, then the raw response body is passed through
  `json_parse` and the structured `PlantJson` replaces the string in
  the response MAP's `body` key (nested access via `json_get` /
  `json_at`, scalars via `json_val`; a body that does not parse as
  JSON becomes the empty string, `ok` stays `TRUE`). The plain form is
  unchanged (raw text body).
- **GIVE ... AS RESPONSE JSON:** `GIVE body AS RESPONSE JSON.` routes to
  `plant_net_respond_json`, which serializes the body with
  `json_stringify` and replies with `Content-Type: application/json`
  (plain `GIVE ... AS RESPONSE` stays `text/plain`). `json_stringify`
  gains a v0.49.3 array path: a plain LIST (odd element count — not a
  pair-list MAP) now serializes as a JSON array `["a","b","c"]` instead
  of being misread as object key/value pairs; scalar quoting logic was
  extracted into a shared `_json_scalar_string` helper (null/true/
  false/numbers raw, everything else quoted).
- Parser: the HARVEST option scan accepts a trailing `JSON` flag (any
  position among the other modifiers); `GIVE ... AS RESPONSE [JSON].`
  sets the flag on the respond node. Codegen: `json` wins over `MAP`
  for harvest routing.
- Header sync: `plant_runtime.h` exposes `plant_net_harvest_json` and
  `plant_net_respond_json`; linkage to generated C flows through
  `plant_compat.h`'s `#include <plant_runtime.h>` — no duplicate
  externs required.

### Tests
- `tests/regression/json_http.plant` (+ `.expected`): HARVEST JSON with
  nested key access (`body["nested"]["x"]`), array element access and
  `json_stringify` round-trips (`{"key":"value","nested":{"x":1},
  "list":[1,2,3]}` / `[1,2,3]`), plus the plain-form fallback (raw
  `hello mock:none` string). Mock server gained a `/json` endpoint
  (Content-Type: application/json).
- `tests/regression/listen_json.plant` (+ `.expected`): `GIVE
  MAP AS RESPONSE JSON` — client sees `{"name":"chloroplast","ok":true}`.
- `tests/regression/listen_json_list.plant` (+ `.expected`): `GIVE
  LIST AS RESPONSE JSON` — client sees `["a","b","c"]` (array path).
- Wire format verified manually: the response carries
  `Content-Type: application/json`; server-side tests are
  `listen_`-prefixed so the harness drives them with `listen_client.py`
  (a plain `json_http.plant` cannot host a LISTEN — the suite would
  block on accept with no client).

### Documentation
- `Language Tour.md`: Networking section updated with the JSON forms
  (`HARVEST ... AS resp JSON.`, `GIVE ... AS RESPONSE JSON.`) and the
  `PlantJson` access pattern (`json_get` / `json_at` / `json_val`).

### Housekeeping
- Version bump 0.49.2 → 0.49.3 (Makefile, compiler banner, native test
  suite version check).

## v0.49.2 — 2026 (LISTEN HTTP Server Subsystem)

### Network
- Formalized the LISTEN HTTP server subsystem: `LISTEN ON port AS req`
  was confirmed fully wired end-to-end (parser `parse_listen_stmt`,
  codegen → `plant_net_listen`, runtime request MAP
  `ok`/`method`/`path`/`headers`/`body`/`sock`, `GIVE body AS RESPONSE`
  → `plant_net_respond`) — shipped v0.48.33, no changes needed there.
- **New `TIMEOUT` option (v0.49.2):** `LISTEN ON port AS req TIMEOUT t.`
  — the parser now accepts an optional `TIMEOUT` modifier after the
  request binding, and codegen routes to the new
  `plant_net_listen_timeout(port, t)` runtime function. The runtime
  sets `SO_RCVTIMEO` on the listening socket, so a client-less
  `accept()` fails with `EAGAIN` after `t` seconds and the request MAP
  comes back `ok = "FALSE"` (verified: 1 s timeout expires in ~1.06 s).
  `plant_net_listen` is now a thin wrapper over the shared
  `_plant_net_listen_ex(port, timeout)`.
- Header sync: `plant_runtime.h` exposes `plant_net_listen`,
  `plant_net_listen_timeout`, and `plant_net_respond`. Linkage to
  generated C flows through `plant_compat.h`'s
  `#include <plant_runtime.h>` — no duplicate externs required.

### Tests
- Added `tests/regression/listen_http.plant` (+ `.expected`): a
  TIMEOUT-configured LISTEN driven by `listen_client.py` on port 41235,
  covering request property inspection (`method`/`path`/`body`/
  `headers["X-Probe"]`) and a `GIVE "Hello from Chloroplast"
  AS RESPONSE.` round-trip (client sees `HTTP/1.1 200 OK`).
  Named `listen_http` (not `listen`) so the harness's `listen_*`
  client-driving path applies — a bare `listen.plant` would block on
  accept with no client and hang the suite.

### Documentation
- `Language Tour.md`: new **Networking (v0.48.32+)** section covering
  the HARVEST client syntax (options, MAP mode) and the LISTEN server
  (request MAP properties, `TIMEOUT`, `GIVE … AS RESPONSE`, limits).

### Housekeeping
- Version bump 0.49.1 → 0.49.2 (Makefile, compiler banner, native test
  suite version check).

## v0.49.1 — 2026 (Regression Runner Stabilization)

### Tests
- `run_regression_tests.sh`: diff comparisons now normalize the
  `.expected` fixture (`awk 1` into a build-dir copy) so a missing
  trailing newline can no longer fail an otherwise identical output
  (the v0.49.0 `harvest_http` failure mode). The harness stays
  POSIX-sh portable (no process substitution; `/bin/sh` is dash).
- `run_regression_tests.sh`: the mock HTTP server is now reaped on
  every exit path — the background PID is captured at launch and a
  `trap` on `EXIT`, `INT`, and `TERM` kills and `wait`s the process,
  eliminating the leaked-server port contention on 41234 that
  accumulated across repeated suite runs.

### Housekeeping
- Version bump 0.49.0 → 0.49.1 (Makefile, compiler banner, native
  test suite version check).

## v0.49.0 — 2026 (HARVEST HTTP Client Subsystem)

### Network
- Formalized the HARVEST HTTP client subsystem as the v0.49.0
  headline: the native runtime, self-hosted parser, and code
  generator now present one synchronized surface:
  `HARVEST url AS resp [METHOD m] [BODY b] [HEADERS h] [TIMEOUT t] [MAP].`
  - Options are scanned in any order after the mandatory `AS resp`
    binding (comma separators allowed); `MAP` is a flag, not a value.
  - `resp` is a fresh response MAP with `ok`/`status`/`body`/`headers`
    keys: `ok` is `TRUE`/`FALSE`, `status` is the numeric code (0 on
    failure), `body` the response payload, `headers` a MAP of response
    headers.
  - Defaults: method `GET`, empty body, no extra headers, timeout 5s
    (`TIMEOUT 0` also means 5s). `HEADERS` accepts a MAP built with
    `LINK "Name" WITH "value" IN h`.
  - `MAP` mode (keeps the connection alive) adds a `sock` key holding
    the descriptor as a decimal string, for use with
    `plant_net_read` / `plant_net_write` / `plant_net_close`.
- Verified and locked down the runtime API in `plant_runtime.c`
  (v0.48.32/34 era) — no runtime changes were required; the subsystem
  was already complete. Linkage to generated C flows through
  `plant_compat.h`'s `#include <plant_runtime.h>` (declarations at
  `plant_runtime.h:23,30-33`), so no duplicate externs are needed.
- Confirmed the parser (`parse_harvest_stmt`) and codegen
  (`harvest_stmt` → `plant_net_harvest` / `plant_net_harvest_map`)
  match the grammar exactly; added the consolidated regression suite.

### Tests
- Added `tests/regression/harvest_http.plant` (+ `.expected`) covering
  the three canonical scenarios against the local mock server
  (127.0.0.1:41234, started by the regression runner when
  `harvest_*.plant`/`listen_*.plant` exist):
  - Basic GET with response-MAP extraction
  - Custom method + body + headers + timeout in non-canonical option
    order (`TIMEOUT 5 METHOD POST BODY "hello=world" HEADERS h`)
  - MAP-mode streaming lifecycle: `sock` extraction, buffered
    `plant_net_read`, send-all `plant_net_write`, idempotent
    `plant_net_close`

### Housekeeping
- Version bump 0.48.38m → 0.49.0 (Makefile, compiler banner,
  native test suite version check).

## v0.48.38m — 2026 (Legacy File Removal)

### Housekeeping
- Removed all legacy, unused, and obsolete files from the repository so
  the tree contains only the active self-hosted pipeline:
  - `core/` (17 files) — pre-self-host JavaScript engine (≤ v0.45.x)
  - `src/**/*.js` (40 files) — JS/LLVM compiler tree that preceded
    self-hosting (compiler, interpreter, cluster, security, telemetry,
    driver, memory, runtime, testing, codegen subdirectories)
  - `service/` — legacy Node.js compile/serve services
    (`sandbox-runner.js` required the removed `core/interpreter.js`)
  - `webrepl/` — legacy web REPL for the JS engine
  - `std/` — legacy standard library for the JS/LLVM engine (the
    native compiler uses built-in FFI instead)
  - `benchmarks/` — v0.47.4-era benchmark suite (`make perf` uses
    `tests/perf/`), including a committed Python bytecode cache
  - `syntaxes/`, `themes/`, `snippets/`, `language-configuration.json`
    — orphaned VS Code extension assets (no manifest)
  - `tmp/`, `hello`, `missing.c`, `--version`, `--version.c`,
    `test.plant`, `test_simple.plant`, `test_parser3.plant`,
    `_test_pipeline.plant`, `run_phase1.sh`, `build.log`, `phase2.log`
    — root stray/scratch files
  - `runtime/runtime.c`, `runtime/runtime.o`, `runtime/runtime_bridge.o`,
    `runtime/libplantlang.so` — older runtime version and build leftovers
  - `tests/suite.plnt` — legacy monolithic suite (unused by any runner)
  - `examples/log.txt`, `examples/results.txt`, `examples/hello.c` —
    generated/scratch outputs
  - `src/plantc/` scratch files — `lexer.c`, `main.c`, `ast.plant`,
    `tokens.plant`, and 16 `test_*.plant` files (not part of the
    active 4-file pipeline)

### Verified
- `make clean && make all` succeeds; `make self` converges (378128 B)
- Native 20/0, generics 7/0, closures 6/0, regression 144/0
- `bin/Chloroplast --version` reports 0.48.38m

## v0.48.38l — 2026 (Legacy Depth Marker Removal)

### Breaking Change
- **`N\` depth markers removed.** The legacy `N\` depth-prefix system
  is permanently removed from the compiler. The lexer no longer
  produces `DEPTH` tokens (`src/plantc/lexer.plant`), and the parser
  no longer strips them (`src/plantc/parser.plant`). A prefix such as
  `1\` now lexes as a NUMBER token plus an unhandled backslash ERROR
  token; the parser's generic unrecognized-token skip discards them
  one at a time, so the marker no longer carries any meaning.

### Test Suite
- `tests/suite.plnt` — all 46 legacy `N\` prefix markers stripped;
  statements now use standard undecorated syntax.

### Notes
- The `\` escape semantics inside string literals (`\n`, `\t`, `\r`,
  `\"`, `\\`) and interpolation (`\${`) are unaffected.
- `codegen_c.plant`'s FFI serialization depth guard (`depth > 3`) is
  unrelated to the removed markers and remains.

## v0.48.38k — 2026 (VEIN Resource & File Management)

### New Features
- **`TAP(path, mode)`** (plant_runtime.c, codegen_c.plant): opens a
  file or resource with the standard modes `"r"` (read), `"w"`
  (write/truncate), and `"a"` (append). The `FILE*` is encapsulated
  in a heap block tagged with a magic (`VEIN_MAGIC`) so the other
  operations can validate the handle before touching it. Returns
  `NULL` (falsy) when the path or mode is invalid/empty or the open
  fails.
- **`ABSORB(vein)`** (plant_runtime.c, codegen_c.plant): reads the
  entire stream into a freshly allocated `tx_t` (size via
  `fseek`/`ftell`, exact read length honored). Invalid handles yield
  `""`.
- **`INFUSE(vein, data)`** (plant_runtime.c, codegen_c.plant):
  writes or appends `data` into the open vein, returning `"1"` on a
  complete write and `"0"` on any failure.
- **`SEAL(vein)`** (plant_runtime.c, codegen_c.plant): closes the
  stream (`fclose`), zeroes the magic tag, frees the handle block,
  and returns `"1"`/`"0"`.
- **Mappings** (codegen_c.plant): `TAP`/`INFUSE` bind through
  `_handle_func_paren` (dual-argument); `ABSORB`/`SEAL` bind through
  both `_handle_func_paren` (paren form, used by the tests) and
  `_handle_func` (space form per the directive).

### Changes
- `plant_runtime.h` declares all four functions; `plant_compat.h`
  mirrors them for the FFI surface.
- New regression suite `tests/regression/vein.plant` covering the
  full write → seal → read → seal cycle plus append mode; the
  scratch file lives in `/tmp` (the `fs_append` convention) so the
  suite never writes into the repository.

## v0.48.38i — 2026 (Universal Sequence Slicing: SLICE)

### New Features
- **`SLICE(data, start, end)`** (plant_runtime.c, codegen_c.plant):
  one built-in slices both strings (`TX`) and lists (`LIST`),
  dispatching at runtime on the value's magic tag. Slices are
  half-open `[start, end)`. `translate_expr` maps the triple-argument
  form through `_handle_func_paren` to `plant_slice`.
- **Index resolution** (plant_runtime.c): arguments accept raw
  small-integer literals (the literal `0` is indistinguishable from
  the NULL sentinel and counts as a real index, so the small-int
  check precedes everything else) or numeric strings. "Not given"
  arguments (NULL / empty / unparseable) default to `0` for `start`
  and the sequence length for `end`. Per the specification, `-1` is
  the **bound-expansion marker**: `start = -1` defaults to the
  beginning and `end = -1` extends the slice to the end of the
  sequence; other negative indices resolve relative to the length
  (`length + index`). Bounds clamp to `[0, length]` and
  `end < start` yields an empty result.
- **List results** (plant_runtime.c, codegen_c.plant): sliced lists
  are fresh `PlantArray`s whose elements are canonicalized to text
  (raw small integers become decimal strings), so results print as
  `[1, 2, 3]`. `SHOW` of a `SLICE(...)` list result wraps the call
  in `plant_map_to_string` (the serializer) — `_cat`/`_S` cannot
  stringify an array.

### Changes
- `plant_runtime.h` declares `plant_slice`; `plant_compat.h` mirrors
  it for the FFI surface.
- New regression suite `tests/regression/slice.plant` covering
  positive and negative bounds on strings and lists, bound
  expansion (`end = -1`), clamping, and empty ranges. Note: the
  runtime follows the specification's written index rules; three
  illustrative examples in the specification table are internally
  contradictory (e.g. `(0, -1)` → `"hello worl"` requires
  `end = -1` → `length - 1`, while `(-5, -1)` → `"world"` requires
  `end = -1` → `length`) and resolve per the prose: `end = -1`
  extends to the end (`SLICE("hello world", 0, -1)` → `"hello
  world"`, `SLICE([1, 2, 3, 4, 5], 0, -1)` → `[1, 2, 3, 4, 5]`).

## v0.48.38j — 2026 (String Analysis Built-ins: FIND, COUNT_OF)

### New Features
- **`FIND(text, sub)`** (plant_runtime.c, codegen_c.plant): returns
  the 0-based index of the first occurrence of `sub` inside `text`
  as text via `_from_long`. Empty/`NULL` `sub` yields `"0"` (an
  empty needle matches at the start), empty/`NULL` `text` or a
  missing substring yields `"-1"`.
- **`COUNT_OF(text, sub)`** (plant_runtime.c, codegen_c.plant):
  counts the total number of **non-overlapping** occurrences of
  `sub` in `text` using `strstr` with `pos + strlen(sub)` pointer
  advancement (`COUNT_OF("aaaa", "aa")` → `2`). Yields `"0"` when
  either argument is empty/`NULL`.
- Both map through `_handle_func_paren` in `translate_expr`
  (`FIND` → `plant_find`, `COUNT_OF` → `plant_count_of`), matching
  the established dual-argument built-in pattern.

### Changes
- `plant_runtime.h` declares both functions; `plant_compat.h`
  mirrors them for the FFI surface.
- New regression suite `tests/regression/string_find.plant`
  covering first-occurrence lookup (`FIND("hello world", "world")`
  → `"6"`), misses, empty-sub/text edge cases, repetition counting
  (`"hello hello"` → 2, `"abcabcabc"` → 3), and the non-overlap
  guarantee (`"aaaa"` / `"aa"` → 2).

## v0.48.38h — 2026 (Ternary Built-in: PICK)

### New Features
- **`PICK(cond, true_val, false_val)`** (plant_runtime.c,
  codegen_c.plant): a concise ternary — returns `true_val` when the
  condition is truthy and `false_val` otherwise. `translate_expr`
  maps the triple-argument form through `_handle_func_paren` to
  `plant_pick`, so conditions are evaluated lazily at the call site
  in C and only the chosen value is materialized.
- **Truthiness rules** (plant_runtime.c): a condition is truthy when
  it is a nonzero raw small-integer literal (negative integers
  included; the small-int check precedes the NULL guard because the
  literal `0` is indistinguishable from the NULL sentinel) or a
  non-empty string other than `"0"`, `"false"`, `"FALSE"`. Bare
  `TRUE` / `FALSE` conditions become `1` / `0` via
  `plant_sanitize_bools` before reaching the runtime.
- **Result canonicalization** (plant_runtime.c): returned values
  render as text like list elements — raw small integers become
  decimal via `_from_long` (`PICK(TRUE, 1, 0)` → `"1"`), strings
  pass through.

### Changes
- `plant_runtime.h` declares `plant_pick`; `plant_compat.h` mirrors
  it for the FFI surface.
- New regression suite `tests/regression/pick.plant` covering
  integer literals (`PICK(1, ...)` → yes, `PICK(0, ...)` → no,
  `PICK(-1, ...)` → yes), bare TRUE/FALSE, comparisons
  (`PICK(20 >= 18, ...)` / `PICK(10 >= 18, ...)`), and text
  conditions (`""`, `"TRUE"`, `"0"`, arbitrary non-empty strings).

## v0.48.38g — 2026 (Conditional List Built-ins: HAS, ANY, ALL)

### New Features
- **`HAS(list, value)`** (plant_runtime.c, codegen_c.plant): reports
  `"1"` when `value` is present in `list`. Both sides canonicalize to
  text first — raw small integers convert via `_from_long` (so
  `HAS([1, 2, 3], 2)` matches the `"2"` element) and anything else
  stringifies — then compares with `strcmp`. Empty lists yield `"0"`.
- **`ANY(list, cond)` / `ALL(list, cond)`** (plant_runtime.c,
  codegen_c.plant): evaluate a runtime condition string
  (`"> 2"`, `"<= 0.5"`) against each numeric element of the list.
  The condition supports `> < >= <= == != =`; the word-form
  comparison (`ANY(l, IS 2)`) works too because `translate_expr`'s
  ` IS ` → ` == ` replacement runs before the quote step. Elements
  that do not coerce to a number fail the predicate. `ANY` yields
  `"0"` for empty lists; `ALL` is vacuously `"1"` for empty lists.
- **Condition quoting** (codegen_c.plant): `ANY`/`ALL` arguments
  would be invalid C as bare text (`plant_any(l, > 2)`), so a new
  `_quote_cond_arg` action scans backward from the closing paren for
  the depth-0 comma separating the list argument from the condition
  and wraps the remainder in a string literal (already-quoted
  conditions pass through). A new `_find_substr` action performs the
  substring search — `find_any` only matches a single character of
  its delimiter set and is unsuitable for multi-character needles.

### Changes
- `plant_runtime.h` declares `plant_has`/`plant_any`/`plant_all`;
  `plant_compat.h` mirrors them for the FFI surface.
- New regression suite `tests/regression/list_cond.plant` covering
  membership (present/absent/empty/string elements) and conditional
  predicates (`>`, `==`, `>=`, `<=`; empty-list behavior).

## v0.48.38f — 2026 (Math Built-ins)

### New Features
- **`ABS` / `ROUND` / `POW` / `CEIL` / `FLOOR` / `RANDOM` / `SIN` /
  `COS` / `SQRT`** (plant_runtime.c, codegen_c.plant): a native math
  library replacing external FFI calls. `translate_expr` maps the
  single-argument forms, the dual-argument `POW(x, y)`, and the
  parameterless `RANDOM()` through `_handle_func_paren` to
  `plant_abs`/`plant_round`/`plant_pow`/`plant_ceil`/`plant_floor`/
  `plant_random`/`plant_sin`/`plant_cos`/`plant_sqrt`.
- **tx_t → double coercion** (plant_runtime.c): operands convert
  dynamically — raw small integers of either sign (integer literals
  arrive unwrapped at the call site, so `plant_abs(-5)` and
  `plant_pow(0, 0)` stay exact; the small-int check precedes the NULL
  guard because 0 doubles as the NULL sentinel) and numeric strings
  (`"2"`, `"3.7"` — the dialect has no decimal literals, so fractional
  scenarios pass strings) via a full-consumption `strtod` scan.
  Unparseable inputs coerce to NaN.
- **Edge-case safety**: `SQRT(-1)` → `"nan"` (explicit domain check);
  `POW(0, 0)` → `"1"` (standard C `pow`); `RANDOM()` produces a
  pseudo-random value in `[0.0, 1.0)` (`rand()` scaled by
  `RAND_MAX + 1` so the upper bound stays exclusive).
- **Result formatting**: integral results render as long integers,
  fractional results with `"%.10g"`; `ROUND` follows C `round`
  semantics (half away from zero — `ROUND("-2.5")` → `-3`).

### Changes
- `plant_runtime.h` declares all nine functions; `plant_compat.h`
  mirrors the externs for the FFI surface.
- Version markers (Makefile, main.plant, run_native_tests.sh) bumped
  to 0.48.38f.

### Tests
- `tests/regression/math_ops.plant` (regression suite): `ABS(-5)` → `5`;
  `ROUND("3.7")` → `4`; `POW(2, 3)` → `8`; `CEIL("3.2")` → `4`;
  `FLOOR("3.9")` → `3`; `SIN(0)` → `0`; `COS(0)` → `1`;
  `SQRT(16)` → `4`; `SQRT(-1)` → `nan`; `POW(0, 0)` → `1`;
  `RANDOM()` asserted in-code to lie in `[0, 1)`; string operands
  (`ABS("-7")`, `ROUND("-2.5")` → `-3`), `SQRT(9)` → `3`. 139/0
  regression, 20/0 native, self-hosting converged (376802 B).

## v0.48.38e — 2026 (String Case Operations: UPPER, LOWER)

### New Features
- **`UPPER(text)` / `LOWER(text)` case conversion** (plant_runtime.c,
  codegen_c.plant): convert every character of the input string to
  its uppercase / lowercase form. Each character is cast to
  `unsigned char` before `toupper`/`tolower`, keeping high-bit bytes
  well-defined on platforms with signed `char`. NULL and empty inputs
  return `""`. The compiler maps the calls through
  `_handle_func_paren` (the same single-argument mechanism as
  `JOIN`/`FIRST`) to `plant_upper(text)` / `plant_lower(text)`.
- **`TRIM(text)` / `REVERSE(text)` string utilities** (plant_runtime.c,
  codegen_c.plant): `TRIM` strips `' '`, `'\t'`, `'\n'`, `'\r'` from
  both boundaries (all-whitespace, empty and NULL inputs yield `""`);
  `REVERSE` writes the characters into a fresh buffer in reverse
  index order (`"hello"` → `"olleh"`). Both allocate through the
  ARC/arena framework (`plant_alloc`) and are mapped in
  `translate_expr` via `_handle_func_paren` to `plant_trim(text)` /
  `plant_reverse(text)`.

### Changes
- `plant_runtime.h` declares `plant_upper`, `plant_lower`,
  `plant_trim`, `plant_reverse`; `plant_compat.h` mirrors the externs
  for the FFI surface.
- Version markers (Makefile, main.plant, run_native_tests.sh) bumped
  to 0.48.38e.

### Tests
- `tests/regression/string_ops.plant` (regression suite):
  `UPPER("hello")` → `HELLO`; `LOWER("HELLO")` → `hello`;
  `UPPER("Hello World")` → `HELLO WORLD`;
  `LOWER("Hello World")` → `hello world`; mixed-case strings with
  digits and punctuation; `UPPER("")` / `LOWER("")` → `""`;
  `UPPER(NULL)` / `LOWER(NULL)` → `""`;
  `TRIM(" hello ")` → `hello`; `TRIM("\thello\n")` → `hello`;
  `TRIM("   all whitespace ")` → `all whitespace`;
  `TRIM("")` / `TRIM(NULL)` → `""`;
  `REVERSE("hello")` → `olleh`; `REVERSE("world")` → `dlrow`;
  `REVERSE("racecar")` → `racecar`; `REVERSE("")` / `REVERSE(NULL)`
  → `""`. 138/0 regression, 20/0 native, self-hosting converged
  (376325 B).

## v0.48.38d — 2026 (List Operations: FIRST, LAST, SUM)

### New Features
- **`FIRST(list)` / `LAST(list)` boundary extraction** (plant_runtime.c,
  codegen_c.plant): return the initial / final element of a list as
  tx_t text. Empty lists — and NULL / non-array arguments — return
  `""`. The compiler maps the calls through `_handle_func_paren` (the
  same single-argument mechanism as `JOIN`/`LEN`) to
  `plant_first(list)` / `plant_last(list)`.
- **`SUM(list)` numeric aggregation** (plant_runtime.c): accumulates
  the list's numeric elements into a double. NUM/SCL elements arrive
  pre-converted as tx_t text (`_from_long`/`_from_double` casts happen
  at the call site); numeric strings such as `"2"` are converted with
  a full-consumption `strtod` scan; everything else — non-parsable
  strings (`"a"`), bare booleans, nested MAP/LIST containers, NULLs
  and empty strings — is skipped without interrupting the
  accumulation. An empty or NULL list sums to `"0"`. Integral results
  render as long integers, fractional results with `"%.10g"`.
- **Bare booleans inside list literals** (codegen_c.plant): the
  `_list_literal` rewrite now runs before boolean sanitization, and
  `_push_el` materializes a bare `TRUE`/`FALSE` element as the quoted
  string `"TRUE"`/`"FALSE"` — a non-numeric element for `SUM`
  (`SUM([TRUE, 2])` → `2`), and visible as text through `JOIN`,
  `FIRST` and `LAST`. Quoted `"TRUE"` strings are unaffected, and the
  sanitizer still converts bare booleans to `1`/`0` everywhere else.

### Changes
- `plant_runtime.h` declares `plant_first`, `plant_last`, `plant_sum`;
  `plant_compat.h` mirrors the three externs for the FFI surface.
- Version markers (Makefile, main.plant, run_native_tests.sh) bumped
  to 0.48.38d.

### Tests
- `tests/regression/list_ops.plant` (regression suite):
  `FIRST([1, 2, 3])` → `1`; `LAST([1, 2, 3])` → `3`;
  `SUM([1, 2, 3])` → `6`; `FIRST([])` / `LAST([])` → `""`;
  `SUM([])` → `0`; `SUM([1, "2", 3])` → `6` (parsable string);
  `SUM(["a", "b"])` → `0` (non-parsable skipped);
  `SUM([TRUE, 2])` → `2` (booleans ignored);
  `SUM([1, [2, 3], 4])` → `5` (nested containers skipped);
  `SUM(NULL)` → `0`. 137/0 regression, 20/0 native, self-hosting
  converged (376103 B).

## v0.48.38c — 2026 (JOIN Built-In Function)

### New Features
- **`JOIN(list, delim)` built-in** (plant_runtime.c, codegen_c.plant):
  concatenates a list's elements into one string separated by `delim`,
  mirroring the classic SPLIT/JOIN pair from the legacy dialect. The
  compiler maps the two-argument call `JOIN(l, d)` directly to
  `plant_join(l, d)` through `_handle_func_paren` (the same mechanics
  as `LEN`), so `JOIN` is usable in any expression position.
- **Guards and edge semantics** (plant_runtime.c): an empty list and a
  NULL list both join to `""`; a NULL `delim` is treated as `""`.
- **Element conversion**: tx_t values are strings in the native model —
  the NUM/SCL/FACT casts (the `_from_long`/`_from_double`/TRUE-FALSE
  translation) already happen at the call site, so numeric and boolean
  elements arrive pre-converted and pass through unchanged. Nested
  MAP/LIST elements serialize through `plant_map_to_string`, the
  runtime's object serializer — a standalone `plant_to_string` does not
  exist, so the serializer stands in for the directive's "complex-type
  conversion" path; NULL elements render as `""`.

### Changes
- `plant_runtime.h` and `plant_compat.h` declare `tx_t plant_join(tx_t
  list, tx_t delim);` (prototype + FFI-surface mirror).

### Tests
- `tests/regression/join.plant` (regression suite): multi-element join
  with a spaced delimiter; numeric elements with `-`; empty-list join;
  single-element join; mixed string/numeric/boolean elements with a
  pipe delimiter; nested MAP/LIST elements serialized by the object
  serializer; NULL delimiter; NULL list.

### v0.48.38c hotfix — Literal List Syntax & Boolean Sanitization

- **Literal list syntax `[expr, expr, ...]`** (codegen_c.plant): the
  parser already accepted `[ ... ]` in expression position, but the
  seam between parser and codegen (no expression AST) meant brackets
  passed through to C verbatim. A new `_list_literal` ACTION in
  `translate_expr` rewrites list brackets into
  `plant_list_make(count, elem1, ...)`: integer elements wrap in
  `_from_long`, nested `[ ... ]` recurse through `_seg_list`, strings
  and other elements pass through as tx_t text. An identifier-guard
  keeps `name[expr]` indexing intact (walking back over whitespace —
  tokenized expression text is space-joined — and blocking `)`/`]`),
  and the scanner is quote-aware with backslash-escape handling, so
  bracket characters inside string literals are never treated as list
  boundaries. `[ ]` compiles to `plant_list_make(0)`.
- **Boolean sanitization scoped outside quotes** (plant_compat.h):
  `plant_sanitize_bools` replaces `TRUE`/`FALSE` with `1`/`0` only
  outside double-quoted string literals, with identifier-boundary
  checks (a digit-free `TRUESTATE`-style name stays intact). The
  `TRUE`/`FALSE` literal replacements in `translate_expr` were
  replaced by a single call to it, so `"hello | TRUE"` stays pristine.
- **`PlantArray.kind` tag** (plant_runtime.h, plant_runtime.c): MAPs
  and LISTs are no longer inferred from element-count parity — a
  2-element list used to serialize/typeof as a map. `plant_list_create`
  (and `plant_string_split`) set `kind = 0`; the seven metadata
  constructors (ANALYZE, storm, telemetry: persist/weather/lock/mem
  report/mem scan) mark `kind = 1`. `_plant_ser` and
  `_plant_val_kind` consult the tag, so `JOIN([1, [2, 3], 4], "-")`
  renders nested lists as `[2, 3]` and `TYPEOF` of an even-count list
  reports `list`. (The ANALYZE/typeof regression expectations that
  asserted the old parity heuristic were corrected.)
- **Tests**: `tests/regression/join.plant` converted to native bracket
  syntax — `JOIN(["a", "b"], ",")` → `a,b`; `JOIN([], ",")` → `""`;
  `JOIN([1, [2, 3], 4], "-")` → `1-[2, 3]-4` (documented deviation:
  the object serializer separates items with `", "`); a direct-string
  `"hello | TRUE"` assertion. 136/0 regression, 20/0 native,
  self-hosting converged (375590 B).

## v0.48.38b — 2026 (Location Backfill & SHELTER/AS Metadata Binding)

### New Features
- **Source-context injection** (codegen_c.plant): every `storm(...)`
  factory call now compiles to
  `plant_storm("TYPE", "msg", __FILE__, __LINE__, 0)` — the
  compile-time source path and line of the generated call site are
  packed into the exception object. Column tracking is non-standard,
  so `0` is passed and the field is omitted from the object.
- **Location backfill in the factory** (plant_runtime.c,
  plant_runtime.h, plant_compat.h): `plant_storm` now takes
  `(type, msg, file, line, column)`. `file` (tx_t) and `line` (long)
  are packed conditionally — non-NULL file, positive line/column —
  so objects built from legacy two-argument calls remain byte-
  identical to v0.48.38a output. Empty type still falls back to
  `ANY_STORM`; empty message still falls back to the registry default
  via `plant_storm_default_message` (or `(unclassified storm)` for
  unconventional types).
- **SHELTER/AS metadata binding** (plant_runtime.c): the complete
  exception MAP — `type`, `message`, and the injected `file`/`line`
  fields — flows into the `AS e` variable (the `plant_storm_match`
  dispatch matches on the type string and hands the whole object to
  the handler, exactly as in v0.48.38a), so user code reads the
  metadata with standard lookup syntax: `_map_get(e, "file")`,
  `_map_get(e, "line")`.

### Changes
- `_storm_inject` (codegen_c.plant) replaces the plain `storm(` →
  `plant_storm(` rewrite in `translate_expr`: it finds each `storm(`
  call's matching close paren (paren-depth and string-literal aware),
  appends the metadata just before it, and renames the call. The
  split is guarded so `storm (` inside a longer identifier (e.g.
  `plant_storm(` from prior rewrites) is left untouched — this also
  protects the self-hosted bootstrap chain from rewriting its own
  literals.
- Regression coverage now asserts exact metadata values: the
  generated-C source path and the C line of the `plant_storm` call
  (deterministic per checkout; asserted non-empty for `file` and
  exact for `line`), and column omission.

### Tests
- `tests/regression/storm_factory.plant` (updated): factory objects
  serialize with `file`/`line` present and `column` absent; empty-type
  and empty-message normalization unchanged.
- `tests/regression/storm_shelter.plant` (new): `THROW storm(...).`
  caught in `WEATHER/SHELTER ... AS e` exposes `e["type"]`,
  `e["message"]`, a non-empty `e["file"]`, the exact `e["line"]`, and
  an absent `e["column"]`.

## v0.48.38a — 2026 (storm() Exception Factory)

### New Features
- **`storm("TYPE", "msg")` exception factory** (parser.plant,
  codegen_c.plant, plant_runtime.c): builds a first-class exception
  object — a `{type, message}` MAP registered on the ARC heap — that
  survives setjmp/longjmp unwinding instead of dying with the throwing
  call frame. `THROW storm(...).` raises such an object through the
  innermost WEATHER checkpoint; `SHELTER TYPE AS e` binds the object
  itself, so handlers unpack it via `_map_get(e, "type")` /
  `_map_get(e, "message")` or serialize it with `plant_map_to_string`.
- **Factory-object lifecycle** (plant_runtime.c): `plant_storm` creates
  the object with one reference (its ARC wrapper's payload is the MAP,
  so finalization frees both). `plant_throw_obj` transfers ownership to
  the frame; unmatched storms propagate outward frame-by-frame through
  `plant_calm` (same reference, no retain/release churn); a matching
  SHELTER consumes it — the generated dispatch calls
  `plant_storm_release` after the handler body runs, dropping the count
  to zero for ARC finalization. Automatic ARC GC (v0.48.37) keeps
  in-flight objects safe: refs > 0 keeps them marked.
- **Normalization rules** (plant_runtime.c): empty type falls back to
  `ANY_STORM`; empty message falls back to the registered storm default
  (or `(unclassified storm)` for unconventional types), mirroring
  `plant_throw`'s NULL-message behavior. Classic `THROW type "msg".`
  storms are untouched and bind the message string in `AS e` clauses
  exactly as before.

### Changes
- `PlantWeather` frame (plant_runtime.h) gains a `volatile tx_t
  exc_obj` field alongside the classic `exc_type`/`exc_msg` strings.
- `parse_throw_stmt` (parser.plant) recognizes `THROW storm(...).`:
  the whole call expression is collected as the payload and the
  classic type token is emptied; codegen emits `plant_throw_obj(...)`
  for that form.
- `translate_expr` (codegen_c.plant) rewrites `storm(` calls to
  `plant_storm(`; the bare-statement path (`storm(...).` and
  `CALL storm(...).`) maps the action name `storm` → `plant_storm`.
- Weather dispatch (codegen_c.plant) captures `tx_t __ev =
  plant_exc_val()` beside `__et`/`__em` before popping the frame;
  `AS e` binds `__ev` (object for factory storms, message string for
  classic ones); `plant_storm_release(__ev)` runs after each matched
  handler body. Handlers that exit early (GIVE/BREAK/CONTINUE inside
  a SHELTER) skip the release — the object stays ARC-managed but
  uncollected until process end (known limitation, classic path has
  the same string-lifetime caveat).

### Tests
- `tests/regression/storm_factory.plant` (regression suite): factory
  creation and serialization; registry-default messages; ANY_STORM
  type fallback; `THROW storm(...)` with type/message integrity via
  `_map_get`; nested WEATHER propagation of an unmatched factory storm
  (CUSTOM) into an outer ANY_STORM shelter; rapid successive
  throw/handle cycles inside a loop.

## v0.48.37e — 2026 (WAIT and LOCK Synchronization Primitives)

### New Features
- **`WAIT [n].` execution throttling** (parser.plant, codegen_c.plant,
  plant_runtime.c): a statement-level pause that blocks for `n`
  milliseconds via POSIX `nanosleep` (`plant_msleep`). The duration is
  any numeric expression (literal, `NUM` variable, arithmetic); zero and
  negative durations are invalid timing arguments and return
  immediately, and the bare `WAIT.` (the legacy async phase-boundary
  spelling) is likewise a no-op.
- **`LOCK var.` synchronization** (plant_runtime.c): a centralized Lock
  Table (`PLANT_LOCK_MAX = 64`) registers a locking flag keyed by the
  target variable's *value*. `plant_lock` returns `"1"` on acquisition,
  `"0"` when the key is already locked (the concurrency guard against
  concurrent access or modification), `"ERR:undefined"` for an empty
  variable value (undefined or out-of-scope) and `"ERR:full"` when the
  table is exhausted. `plant_lock_release`, `plant_lock_held` and the
  `plant_lock_status` telemetry `MAP` (`locked_count`) complete the API.
- **`plant_now_ms`** (plant_runtime.c): non-static monotonic
  milliseconds (CLOCK_MONOTONIC) wrapper around `plant_ms`, exposed to
  tests via `ffi_now` for timing verification.

### Changes
- **Lexer**: `WAIT` and `LOCK` added to the keyword tables
  (`is_keyword`, `keyword_to_type`) and the TokenType ENUM.
- **Parser**: new `parse_wait_stmt` / `parse_lock_stmt` actions. `WAIT`
  collects the token text up to the terminating `.` (string operands are
  rejected with a syntax error) and produces `wait_stmt { ms }`; `LOCK`
  requires a plain identifier target (`lock_stmt { var }`) and rejects
  non-identifiers with a syntax error. Both are dispatched in
  `parse_statement` and whitelisted as block-form closure bodies.
- **Codegen**: `wait_stmt` → `plant_msleep(<ms>);` (raw long
  expression); `lock_stmt` → `plant_lock((tx_t)<var>);` with numeric
  targets stringified via `_from_long`.
- **Runtime**: `plant_msleep` now treats `ms <= 0` as a no-op (was a
  1 ms clamp); the Lock Table section (`g_lock_table`) is added before
  the Memory Safety Layer.
- **Tests**: `wait` (timing bounds for `WAIT 120.`, no-op `WAIT 0.`,
  negative and bare forms), `wait_invalid` (`WAIT "abc".`),
  `lock` (double-lock refusal, held probe, release, idle release,
  `LOCK` statement, status telemetry, `ERR:undefined` empty value),
  `lock_invalid` (`LOCK 123.`). FFI wrappers `ffi_now`, `ffi_lock`,
  `ffi_lock_release`, `ffi_lock_held`, `ffi_lock_status` in mock_ffi.

### Notes
- Regression suite 133/0, native suite 20/0, self-hosting converged.

## v0.48.37d — 2026 (Weather Memory Management and Exception Cleanup)

### New Features
- **WEATHER exit-lists** (parser.plant, plant_runtime.c): every `WEATHER`
  AST node now provisions a dedicated local `exit_list`; at runtime each
  `PlantWeather` frame carries a matching exit-list
  (`PLANT_WEATHER_EXIT_MAX = 64`) of registered resource handles.
  `plant_weather_leave` walks the list on every exit path — normal
  completion, handled storms, unmatched propagation and the threaded
  `GIVE`/`BREAK`/`CONTINUE` chains — freeing each handle ARC-aware
  (edges + heap bookkeeping) or via `plant_mem_free`, and draining the
  ARC heap's deferred-deallocation queue, so protected scopes reclaim
  systemically.
- **SHELTER handler cleanup**: the storm-routing engine now brackets
  every handler body with `plant_weather_shelter_enter/leave`, purging
  temporary objects, scratch buffers and ARC links registered while the
  handler ran immediately before the shelter scope exits. The popped
  frame becomes the registration target during dispatch
  (`plant_weather_handling_begin/end`), so handler temporaries are
  tracked without disturbing active-frame accounting.
- **Weather memory telemetry** (`plant_weather_status`): a structured
  `MAP` reporting `active_frames`, `live_objects` (protected
  allocations in weather scopes), `pending_frees` (deferred
  deallocations queued within exit-lists) and `storm_handlers`
  (registered exception handler hooks); exposed to tests via
  `ffi_weather_status`.

### Changes
- **Parser**: `weather_stmt` AST gains an `exit_list` field binding each
  block to its resource-reclamation structure.
- **Codegen**: `plant_weather_enter(&w, nhandlers)` records the frame's
  SHELTER hook count; `plant_weather_handling_begin/end` bracket the
  shelter dispatch and `plant_weather_shelter_enter/leave` bracket each
  handler body.
- **Runtime**: `plant_weather_register_handle` / `plant_weather_defer_handle`
  register protected handles and queue deferred deallocations within the
  active frame's exit-list.

### Tests
- `weather_memory`: ARC objects registered inside a WEATHER block are
  freed after the terminal `CALM` (weather + persist status both report
  zero live objects).
- `weather_handler_cleanup`: temporaries created inside a variable-free
  `SHELTER` handler are purged on handler exit (`live_objects` returns
  to 0, handler hook count reported).
- `weather_status`: telemetry accuracy across handle deferral (pending
  deallocation queued in the exit-list), an empty `CALM` block, and
  unmatched storm propagation through nested WEATHER frames.

## v0.48.37c — 2026 (True SAFE Worker-Process Isolation)

### New Features
- **SAFE actions now execute in real worker processes** (plant_runtime.c,
  codegen_c.plant): a SAFE call spawns/fork a worker, marshals args and
  results through a typed wire codec (`'N'`/`'I'` raw numerics, `'S'`
  strings, `'A'` arrays, `'F'` = memfd + `SCM_RIGHTS` zero-copy for
  payloads above the 1MB threshold), and hands large payloads back to the
  parent byte-for-byte. `safe_real_*` regression tests prove typed
  arguments, string/list returns, and a 1.5MB payload across a real
  process boundary.

### Changes
- **Wire codec numeric-arg hardening**: SAFE call arguments of numeric
  type (`long`/`int`) are emitted as `_from_long(...)` strings at the
  call site instead of raw C literals. The codec's small-int heuristic
  (`(uintptr_t)v < 4096`) would otherwise misinterpret a raw literal
  >= 4096 as a heap pointer and crash on dereference during encode
  (`plant_runtime.c:5254`). The generated worker adapter parses the
  string back with the new `plant_rw_arg_long()` helper, which accepts
  both raw small ints and wire strings.
- **Parser**: multi-arg `REAP ... FROM` actions consume their
  parenthesized argument lists correctly.
- **Wire decode**: `'S'`/`'A'` payloads are decoded with initialized
  length/count variables.

### Tests
- `safe_real_arg`: SAFE worker receives NUM/FACT/TX args through the
  generated adapter and combines them.
- `safe_real_str`: SAFE worker returns a string verified by length and
  equality in the parent.
- `safe_real_list`: SAFE worker builds a `PlantArray` returned across
  the process boundary and decoded element-by-element.
- `safe_real_big`: SAFE worker returns a 1,572,864-byte payload (above
  the 1MB threshold); the codec hands it over via memfd + `SCM_RIGHTS`
  and the parent recovers every byte.

## v0.48.37b — 2026 (PERSISTENT GC and Lease Enhancements)

### New Features
- **Dynamic GC intervals (`MISSION CONFIG PERSIST_GC_INTERVAL = N`)**:
  automatic ARC cycle detection already read the configured interval
  from the mission config handler; the scheduling path is now covered
  by a targeted test (interval 3 fires exactly 10 cycles across 30
  allocations) and the runtime comment no longer claims a fixed 1000.
- **Proactive lease eviction (`lease_evict()`)** (plant_runtime.c):
  new memory-pressure tracking (`plant_persist_pressure`: FAST bump
  ratio primary, ARC live bytes vs a 64MB soft cap secondary, with
  `MISSION CONFIG PERSIST_PRESSURE = N` forcing a simulated level).
  Under pressure: below 80% leases run to expiry; 80-89% zero-ref
  leased objects that are expired or within the `PERSIST_LEASE_MS`
  margin are queued for reclamation; 90%+ destroys every zero-ref
  leased object BEFORE its lease expires and drains the queue. The
  allocator path triggers the check automatically each allocation.
- **Deferred-free queue** (plant_runtime.c): `g_arc_deferred` holds
  objects scheduled for early eviction; `plant_arc_gc` drains the
  queue as the queued leases expire (objects are marked `deferred` so
  GC and the DistributedHeap evictor skip them until the drain).
- **`plant_persist_status()` now returns a structured MAP** with
  `live_objects` / `gc_runs` / `leased_count` / `pending_frees`
  (`leased_count` counts objects under an active lease, and
  `pending_frees` is the deferred-queue length).

### Changes
- `ffi_persist_status` now returns a MAP; `persistent_cache.plant`
  and `persistent_cycle.plant` render it with `plant_map_to_string`.
- New mock_ffi wrappers: `ffi_lease_evict`, `ffi_persist_pressure`.

### Tests
- `persistent_gc_interval`: customized interval changes the GC
  schedule (10 cycles at interval 3 over 30 allocations).
- `persistent_lease_evict`: simulated critical pressure (95%) evicts
  leased objects before a 60s lease expires.
- `persistent_status`: moderate pressure (85%, 30ms margin) queues
  20ms leases as `pending_frees = 2`, then a manual GC drains them
  after expiry (live 2 → 0, gc_runs 0 → 1).

## v0.48.37a — 2026 (Memory Safety Layer)
 — 2026 (Memory Safety Layer)

### New Features
- **`FREE x.`** (parser.plant + codegen_c.plant + plant_runtime.c): the
  explicit-deallocation statement. `x` is passed to `plant_mem_free`,
  which releases the storage through the correct allocator and writes
  NULL back to the variable, so a double `FREE` is a no-op. The parser
  requires an identifier target (`Error: FREE requires an identifier
  target.`).
- **`ARC LINK parent TO child.` / `ARC UNLINK parent FROM child.`**
  (parser.plant + codegen_c.plant): statement forms of the v0.48.31
  ARC edge API — the parent keeps the child alive (strong edge); UNLINK
  drops it. A released object with zero references finalizes.
- **`FAST RESET.`** (parser.plant + codegen_c.plant): releases the bump
  heap mid-scope. In addition, every `WITH MISSION FAST` action now
  emits `plant_fast_reset()` before `plant_fast_exit()` (RAII), so the
  heap is empty when a FAST callee returns.
- **Fixed-size string slab pool** (plant_runtime.c + plant_compat.h):
  64-byte blocks over a single 1024-block region; the `_cat`/`_cat3`/
  `_cat4` families allocate from the pool when the result fits and fall
  back to `malloc`. `plant_mem_free` range-checks the pool before
  freeing. The pool also grows the BALANCED allocation counter
  (`g_bal_bytes`).
- **`plant_mem_report`** (plant_runtime.c): a MAP of live bytes by
  allocator owner — `arena` / `fast` / `arc` / `balanced` / `slab` —
  backed by new per-allocator byte counters.
- **`plant_mem_scan`** (plant_runtime.c): audit-ring scanner returning
  a MAP with `fast_escalations`, `arc_allocs` / `arc_frees` /
  `arc_live`, `arena_miss_pct`, `slab_blocks` and a `warnings` string
  (`arc_churn`, `slab_exhausted`, `clean`).
- **DistributedHeap** (plant_runtime.c): a consistent-hash ring (FNV-1a
  over 64 virtual points per node) places ARC allocations onto
  per-node segments; `plant_dist_alloc(node, key)` /
  `plant_dist_node` / `plant_dist_release` / `plant_dist_status`, plus
  lease-based per-node eviction when `MISSION CONFIG DIST_NODE_CAP` is
  set. `MISSION CONFIG DIST_NODES` (1-64) rebuilds the ring.
- **SAFE boundary verification** (plant_runtime.c): `plant_safe_boundary_copy`
  checks that payloads at or below the channel threshold crossed as
  copies and flags shared-buffer violations in the audit ring.

### Fixes
- Fixed a free-list construction bug in the slab pool where the first
  block self-looped, so the pool could re-issue the same block after
  1024 allocations (double-allocation corruption; this crashed the
  self-hosting chain on some inputs).
- `plant_dist_status` returned a dangling stack buffer; it now uses a
  static buffer.

### Tests
- New regression tests: `mem_free` (double-free safety), `arc_link`
  (LINK/UNLINK + finalizer counting), `fast_reset` (mid-scope reset),
  `mem_report`, `mem_scan`, `fast_scope` (RAII reset on FAST exit),
  `dist` (ring placement + status) and the negative `free_invalid`.
- New mock_ffi wrappers: `ffi_mem_free`, `ffi_mem_report`,
  `ffi_mem_scan`, `ffi_dist_init`, `ffi_dist_alloc`, `ffi_dist_node`,
  `ffi_dist_release`, `ffi_dist_status`.

### Documentation
- `docs/EVAPORATE.md` documents the memory-safety architecture and the
  verified state of each component against the runtime sources.

## v0.48.36 — 2026 (Now / Analyze / Typeof)
 — 2026 (Now / Analyze / Typeof)

### New Features
- **`NOW FORMAT : NAME.`** (lexer.plant + parser.plant + codegen_c.plant +
  plant_runtime.c): the current date/time as a string. Supported format
  names: `DATE` (`%Y-%m-%d`), `TIME` (`%H:%M:%S`), `YEAR` (`%Y`),
  `STAMP` (Unix epoch). A bare `NOW.` (no FORMAT clause) also yields the
  epoch. Unknown format names print the deterministic
  `bad-format:<NAME>` string instead of failing, and the parser rejects a
  FORMAT clause without `:` or without a name (`Error: NOW FORMAT requires
  ':' before the format name.`).
- **`TYPEOF x.`** (parser.plant + codegen_c.plant + plant_runtime.c):
  prints the runtime type of `x` as one of `int` / `string` / `map` /
  `list` / `closure` / `null`. Values are untagged at runtime, so the
  shared `_plant_val_kind` classifier resolves them structurally: numeric
  text (and C small integers) classify as `int`, `PlantArray` containers
  with the even/odd heuristic as `map`/`list` (closure-shaped arrays and
  registered closure environments as `closure`), and NULL as `null`. A
  bare identifier target that is otherwise undeclared is implicitly
  declared (like REAP targets) and therefore classifies as `null`.
- **`ANALYZE x.`** (parser.plant + codegen_c.plant + plant_runtime.c):
  prints a structural report as a uniform map
  `{type = ..., size = ..., keys = [...]}` — `size` is the element/pair
  count for list/map, the string length for strings/ints, 0 for
  null/opaque closures; `keys` lists the elements (list), the key strings
  (map), or is empty (scalars). Strings render via the new recursive
  `plant_map_to_string`/`_plant_ser` serializer (depth-capped at 8,
  maps as `{k = v, ...}`, lists as `[e1, ...]`, empty containers as `[]`).
- **Expression forms** (codegen_c.plant): `NOW FORMAT : NAME`, `TYPEOF x`
  and `ANALYZE x` are also usable inside expressions (e.g.
  `SHOW "t" + TYPEOF n.`). The codegen's new quote-aware `_ni_replace`
  text transformer rewrites these constructs at the token level in
  `translate_expr`, recursing so nested forms
  (e.g. `ANALYZE NOW FORMAT : YEAR`) compose, and the ANALYZE expression
  form self-renders through `plant_map_to_string`.

### Tests
- `tests/regression/now.plant` — deterministic structural checks of all
  four formats (string lengths via `ANALYZE`), the bad-format fallback,
  bare-NOW typing, and concatenated expression forms.
- `tests/regression/analyze.plant` — list/map/null/int/string reports
  with element and key lists, plus `ANALYZE NOW FORMAT : YEAR` nesting.
- `tests/regression/typeof.plant` — literals, NUM/STRING variables,
  NULL, an implicitly-declared undefined variable, list/map/closure
  values, and expression forms.
- Negative pairs: `now_invalid.invalid` (missing `:`),
  `typeof_invalid.invalid` (missing target).
- Full suite green: native 20, generics 7, closures 6, regression 111.

## v0.48.35 — 2026 (Immutables: CONST / ROOT / ROOT_SCOPE)

### New Features
- **`CONST name TO "value".`** (parser.plant + codegen_c.plant): declares a
  block-scoped immutable constant. Values must be a single quoted string
  literal (rejected otherwise at parse time); the compiler emits
  `static const char *name = "value";` inside the enclosing C block, so a
  CONST declared inside an IF/SEASON/CYCLE/WEATHER branch or closure block
  is scoped to exactly that block. CONSTs shadow outer CONSTs of the same
  name (local-first lookup matching C block scoping), and a use of a CONST
  before its definition within the same block is a compile error (the
  parser scans the block body from its start position).
- **`ROOT name TO "value".`** (parser.plant + codegen_c.plant): declares a
  module-wide global constant. ROOTs are hoisted by a first-pass
  `collect_roots` walk in `generate_c` and emitted once at file scope right
  after the `#include <plant_compat.h>` header, so using a ROOT before its
  textual declaration (even at the top level) is legal. Redeclaring a ROOT
  is rejected at parse time via the module-wide `rtab` table.
- **`ROOT_SCOPE` blocks** (parser.plant): `ROOT_SCOPE … /ROOT_SCOPE.`
  establishes a scope in which every inner `CONST` is automatically
  elevated to a `ROOT` (the `const_stmt` node is flagged and codegen skips
  the block-local emission), making it easy to batch-promote a set of
  shared constants. Nested ROOTs inside the block work as usual.
- **Multi-tier constant tables** (parser.plant): every statement-block
  parser (`parse_program`, `parse_action_decl`, `parse_closure`,
  `parse_if_stmt` with fresh tables per ORIF/ELSE branch, `parse_season_stmt`,
  `parse_cycle_stmt`, `parse_weather_stmt`, `parse_root_scope_stmt`) threads
  a per-block `ctab`, the module `rtab`, and a `bstart` token position used
  for use-before-definition detection. ROOT_SCOPE blocks get their own
  table; `emode` flags elevation.

### Bug Fixes
- **Latent `plant_list_make` count/arg mismatch (compile-stage segfault)**:
  seven AST-node constructions (closure, call_stmt, shake_stmt, if_stmt ×2,
  throw_stmt, stop_if_stmt) passed a count two larger than the actual
  argument list, so `plant_list_make` pulled two garbage values off the
  stack into the node map. `_map_get` walkers (`_cl_walk` calling
  `_map_get(node, "closure")` on every statement) then `strcmp`'d those
  slots — undefined behavior that happened to survive on old stack layouts,
  but deterministically crashed the compiler (NULL strcmp in
  `runtime/c/plant_compat.h:292`) after the new parser signatures shifted
  caller frames. The counts now match the argument lists
  (`stop_if_stmt`/`shake_stmt` 6→4, `throw_stmt` 8→6, `call_stmt` 10→8,
  `closure`/`if_stmt` 12→10), un-breaking `stop_if.plant` and
  `shuffle.plant` compilation.

### Tests
- `tests/regression/const.plant` — local shadowing (inner IF block CONST
  shadows the outer constant; outer visible again after the block),
  SEASON/CYCLE containment.
- `tests/regression/root.plant` — top-level use-before-declaration
  (`SHOW` of a ROOT before its `ROOT` line), `ROOT_SCOPE` elevation with a
  nested ROOT, and an action-local CONST shadowing an elevated constant.
- Negative pairs: `const_redecl.invalid` (same-block redeclaration),
  `const_usebeforedef.invalid` (use before definition),
  `root_redecl.invalid` (duplicate ROOT), `const_nonliteral.invalid`
  (non-string-literal value).
- Full suite green: native 20, generics 7, closures 6, regression 106
  (including the previously-crashing `stop_if`/`shuffle`).



### New Features
- **`HARVEST url AS resp MAP.`** (parser.plant + codegen_c.plant): the
  optional trailing `MAP` specifier (usable in any option order) sets
  a `map` flag on the `harvest_stmt` AST node; codegen routes the call
  to the new `plant_net_harvest_map(url, method, payload, headers,
  timeout)` runtime function. MAP mode keeps the connection alive
  after the response and adds a `sock` key — the live descriptor as a
  decimal string — to the response MAP alongside the uniform
  `ok`/`status`/`body`/`headers` fields, enabling follow-up
  reads/writes and explicit teardown.
- **`plant_net_harvest_map` runtime** (`plant_runtime.c` +
  `plant_runtime.h`): shared `_plant_net_harvest_ex` core with a
  keep-alive flag. MAP mode sends the request without `Connection:
  close`, reads exactly the `Content-Length` body, and stashes any
  over-read bytes (same TCP segment as the body) in a per-fd pending
  buffer so a later `plant_net_read` still sees the stream in order;
  the body field is truncated to exactly the Content-Length bytes.
  When the response lacks a header block or Content-Length the
  connection drains to EOF/timeout, is closed, and `sock` reports
  `-1`.
- **`plant_net_read(fd)` runtime**: replaces the legacy one-shot recv.
  Accepts a sock reference as a decimal string, drains the pending
  buffer first, then accumulates `recv` data under a 500 ms
  idle/SO_RCVTIMEO window (1 MiB cap) so slow peers cannot hang the
  caller. Closed, negative, or non-numeric descriptors yield the
  empty string safely.
- **`plant_net_write(fd, data)` runtime**: replaces the legacy
  byte-count return. Performs a send-all loop over the full payload
  and returns the boolean `"TRUE"` on successful transmission,
  `"FALSE"` on any send failure or closed/invalid descriptor.
- **`plant_net_close(fd)` runtime**: replaces the legacy void close.
  Releases the descriptor through a closed-fd registry, making
  double-close attempts safe idempotent no-ops (an already-closed fd
  is never re-closed, so a recycled descriptor cannot be corrupted);
  entries are forgotten when the runtime opens a fresh socket with
  the same number. Always reports `"TRUE"`.
- **Unified sock representation**: the LISTEN request MAP's `sock`
  key is now the same decimal-string form as HARVEST MAP mode, and
  `plant_net_respond` resolves it the same way (internal `close` and
  `send` paths use shared `_plant_close_raw`/`_plant_send_all`
  helpers).

### Regression Tests
- `harvest_full`: complete network lifecycle against a new
  `/readback` mock endpoint (200 "hello mock" with keep-alive, a
  pushed "server-push" stream chunk, follow-up payload echo
  "push-ack:<payload>"): MAP-mode HARVEST field extraction
  (ok/status/body/Content-Length), `plant_net_read` on the live sock,
  `plant_net_write` boolean result, `plant_net_read` of the pushed
  stream and the ack, idempotent double `plant_net_close`
  (`TRUE`/`TRUE`), and the defensive edges: read after close (empty),
  write after close (`FALSE`), and read on a bogus descriptor
  (empty).

## v0.48.34 — 2026 (Networking Enhancements: Read/Write/Close + HARVEST MAP)

## v0.48.33 — 2026 (LISTEN / RESPONSE HTTP Server)

### New Features
- **`LISTEN ON port AS req.` statement** (parser.plant +
  codegen_c.plant): `listen_stmt` AST node storing the port
  expression and the request identifier; codegen declares the fresh
  variable and emits `tx_t req = plant_net_listen(port);`. LISTEN
  joins the statement dispatch and the block-body keyword list.
- **`GIVE body AS RESPONSE.` statement**: `respond_stmt` AST node
  (distinct from the `GIVE value.` return statement) storing the
  response expression and the request variable it replies on; codegen
  emits `plant_net_respond(req, body);`. Quoted literals pass
  through, raw expressions go through the normal translation path.
- **Response binding** (`parser.plant`): each statement-list loop
  (program body, action bodies, closure block bodies) tracks the
  most recent `listen_stmt` in the block as the *active request
  context*; the binding threads through IF/SEASON/CYCLE/WEATHER
  bodies via the new `clv` parameter (nested blocks inherit the
  outer binding; a LISTEN inside a nested block binds only its own
  block). A `GIVE ... AS RESPONSE.` with no LISTEN in scope is
  dropped silently at codegen.
- **`plant_net_listen(int64_t port)` runtime** (`plant_runtime.c` +
  `plant_runtime.h`): opens a TCP socket (`SO_REUSEADDR`), binds,
  listens with a backlog, accepts ONE client connection, closes the
  listening socket, and reads the request (5s receive timeout, cap
  of 1 MiB) into a request MAP with keys `ok` (`"TRUE"` once
  accepted, `"FALSE"` when the port is unavailable), `method`,
  `path`, `headers` (nested pair-list MAP), `body` (per
  Content-Length), and an internal `sock` handle consumed by the
  responder. Malformed/empty requests are accepted cleanly with
  empty method/path/body strings.
- **`plant_net_respond(req, body)` runtime**: builds an
  `HTTP/1.1 200 OK` response with `Content-Type: text/plain` and
  `Content-Length`, sends it on the request's socket, closes the
  connection, and frees the boxed handle; NULL/sock-less requests
  are safe no-ops.

### Regression Tests
- `listen_basic`: one-shot server on 41235 driven by the new
  `listen_client.py` (request mode sends a POST with an X-Probe
  header and payload); verifies request MAP parsing (ok/method/path/
  body/header) and that the client receives `HTTP/1.1 200 OK` with
  the `GIVE ... AS RESPONSE.` payload.
- `listen_malformed`: client sends a non-HTTP line; the server stays
  healthy, reports empty method/path, and still responds 200.
- `listen_busy`: LISTEN on 41234 — the port held by the mock HTTP
  server — fails fast with `ok = FALSE`.
- `run_regression_tests.sh` starts the mock server whenever `listen_*`
  tests exist and runs client-driven LISTEN tests (binary in the
  background, client output appended after the server's stdout so one
  `.expected` file covers both sides).

## v0.48.32 — 2026 (HARVEST HTTP Client)

### New Features
- **`HARVEST url AS resp [METHOD m] [BODY b] [HEADERS h] [TIMEOUT t].`
  statement** (parser.plant + codegen_c.plant): `harvest_stmt` AST
  node capturing the URL expression, result identifier, and optional
  method/payload/headers/timeout options in any order (comma-
  separated option lists are accepted). Codegen emits
  `tx_t resp = plant_net_harvest(url, method, payload, headers, timeout);`
  with `"GET"` as the default method and `""`/`0`/`0` for the
  unpopulated options; quoted string options pass through, raw
  expressions are translated through the normal expression path.
- **`collect_until_keyword`** parser helper: like `collect_until` but
  terminates on the first of a keyword LIST at depth 0 (stops
  BEFORE the keyword), with the same string escaping/quoting rules.
- **Runtime rebuild of `plant_net_harvest`** (`plant_runtime.c` +
  `plant_runtime.h`): the legacy v0.41 GET-only client (raw-body
  string result) is replaced by a full tx_t response MAP:
  - uniform 4-pair result with keys `ok` (`"TRUE"` for 2xx,
    `"FALSE"` otherwise), `status` (status code as string), `body`
    (response body text), `headers` (nested pair-list MAP of
    response headers);
  - HTTP/1.1 client with host[:port] parsing, optional custom
    request headers (pair-list MAP), `Content-Length` + payload for
    POST, `Connection: close`;
  - `SO_SNDTIMEO`/`SO_RCVTIMEO` timeouts in seconds (0 → 5s
    default); malformed responses yield `ok=FALSE`, `status=0`;
    empty URL yields a clean `ok=FALSE` without touching the
    network.
- HARVEST joins the statement dispatch and the block-body keyword
  list; the Makefile dist copy now includes `tests/regression/*.py`
  so the mock HTTP server ships with the source distribution.

### Regression Tests
- `harvest_get`: GET baseline (ok/status/body/header-map parsing),
  custom request header transmission (echoed by the mock), empty
  header list, 404 status handling, and empty-URL rejection.
- `harvest_post`: POST payload delivery (echoed), empty payload,
  explicit `TIMEOUT` on a fast request, excessively short timeout
  against a slow endpoint (clean failure, no hang), malformed server
  response (`THIS IS NOT HTTP`), and empty-response parsing — all
  against `tests/regression/mock_http_server.py` (127.0.0.1:41234),
  which `run_regression_tests.sh` starts/stops around the suite.

## v0.48.31 — 2026 (BRAID and LINK Collection Operations)

### New Features
- **`BRAID l1 WITH l2 AS name.` statement** (parser.plant +
  codegen_c.plant): `braid_stmt` AST node storing both source lists
  and the result identifier; codegen declares the fresh variable and
  emits `PlantArray* name = plant_braid(l1, l2);`.
- **`BRAID l1 WITH l2 AS name MAP.` statement**: `braid_map_stmt`
  node; emits `plant_braid_map` (the `MAP` keyword is optional and
  recognized after the result name).
- **`LINK key WITH value IN map.` statement**: `link_stmt` node
  capturing key operand, value operand, and target map reference;
  emits `map = plant_link(map, key, value);` upsert.
- All three keywords join the statement dispatch and the block-body
  keyword list.
- **Runtime** (`plant_runtime.c` + `plant_runtime.h`):
  - `plant_braid` — zips two lists into a fresh pair list
    `[k0, v0, k1, v1, …]`; mismatched lengths pair only the
    `min(countL, countR)` leading elements and safely ignore the
    excess; NULL/invalid inputs yield an empty pair list.
  - `plant_braid_map` — builds a map from the same parallel lists
    (first list = keys, second = values); duplicate keys collapse to
    a single entry with the LAST value, updated in place.
  - `plant_link` — upsert: an existing key's value is replaced in
    place (pair count unchanged), otherwise a new key/value pair is
    appended; a NULL/uninitialized map target is instantiated
    automatically.

### Regression Tests
- `braid`: pair-list generation with `_map_get` lookups, shorter-
  values and shorter-keys mismatches (excess ignored), MAP-form
  braiding, duplicate-key collapse (last value wins), and empty
  inputs.
- `link`: key insertion, in-place value update on a pre-existing key
  (pair count stays constant), duplicate-key collapse, and NULL
  target instantiation — all verified through `_map_get` and element
  counts.

## v0.48.30 — 2026 (TAKE and PUT List Operations)

### New Features
- **`TAKE val FROM list.` statement** (parser.plant + codegen_c.plant):
  new `take_stmt` AST node capturing the operand and target list;
  joins the statement dispatch and the block-body keyword list.
  Codegen emits `list = plant_list_remove(list, val);` (same
  translate path as `PUT`, so literals, variables, and expressions
  work as operands).
- **`PUT val INTO list.` hardening**: the emitter now calls
  `plant_list_add` instead of the raw `plant_list_push`, making the
  insertion NULL-safe.
- **Runtime** (`plant_runtime.c` + `plant_runtime.h`):
  - `plant_list_add` — appends the value at the end of the
    collection; a NULL (or invalid) list reference is instantiated
    on the fly, so PUT works against uninitialized targets.
  - `plant_list_remove` — locates and removes only the FIRST matching
    occurrence: string elements match by `strcmp`, container
    elements (PlantArray magic) by pointer identity. NULL lists,
    empty collections, and absent values are safe no-ops that return
    the list unchanged (defensive checks before any access).

### Regression Tests
- `take`: standard removal with count verification, removal of a
  non-existent value (no-op), TAKE on an empty list and on a NULL
  target, and duplicate handling — only the first matching
  occurrence is removed (`1 2 1 3` → take "1" → `2 1 3`).
- `put_full`: appending into an existing list, instantiating a fresh
  collection from a NULL target (`CREATE x(LIST) TO NULL.` + PUT),
  appending duplicate values, and TAKE/PUT integration where removal
  of a duplicated value leaves the later copy intact.

## v0.48.29 — Type Header Decoupling (Refactor)

### Architecture
- **`plant_types.h` (new).** Single authoritative home for the
  foundational type:
  - `typedef void* tx_t;`
  The opaque pointer type is now isolated from execution logic and
  compatibility wrappers; `plant_runtime.h` (declarations) and
  `plant_compat.h` (FFI bindings) both inherit it via
  `#include <plant_types.h>`, with include guards for idempotency.
- **`plant_compat.h`.** Local `typedef void* tx_t;` removed; the
  header now consumes the type from `plant_types.h` and is scoped to
  FFI bindings and compatibility wrappers only.
- **`plant_runtime.h`.** Includes `plant_types.h` at the top; the
  v0.48.29 `plant_sort` / `plant_shuffle` declarations use `tx_t`
  consistently (reverting the interim `void*` form that existed only
  because the typedef was previously unavailable at that point).
- **`plant_runtime.c`.** Unchanged — `tx_t` arrives through its
  existing `#include <plant_runtime.h>`.
- **Distribution.** `make dist` stages `plant_types.h` alongside the
  other runtime headers so distcheck builds resolve it.

### Verification
- `tx_types.c` native probe (new): includes `plant_runtime.h` then
  `plant_compat.h` directly, and at compile time asserts
  - `sizeof(tx_t) == sizeof(void*)` (opaque-pointer contract);
  - `plant_sort` / `plant_shuffle` addresses assign to `tx_t`
    function-pointer types (signatures use `tx_t`, not raw `void*`);
  - FFI statics (`strings_UPPER`) accept/return `tx_t`.
  Wired into `tests/native/run_native_tests.sh` as the
  `tx_types header decoupling` case (links the real runtime).
- Full suite re-run: all 127 cases green (native 20 + generics 7 +
  closures 6 + regression 94), self-hosting converged, distcheck OK.

## v0.48.29 — 2026 (SORT and SHAKE List Operations)

### New Features
- **`SORT` statement** (parser.plant + codegen_c.plant):
  - `SORT l.` — base form, ascending by default.
  - `SORT l ASC.` / `SORT l DESC.` — explicit directional qualifiers.
  - `SORT l BY field.` — sort pair-list MAP elements by a named
    field, ascending by default.
  - `SORT l BY f1 DESC, f2 ASC.` — multi-field compound ordering with
    per-field directional binding; each field without a qualifier
    inherits the global direction (default ASC). The AST stores the
    target, global direction, and parallel field/direction lists.
  - Codegen emits `l = plant_sort(l, "<spec>")` with the spec encoded
    as `""` (plain ASC), `"DESC"` (plain DESC), or
    `"f1:DESC,f2:ASC"` for field sorts.
- **`SHAKE` statement** — `SHAKE l.` reorders a list in place, emitted
  as `l = plant_shuffle(l)`.
- **Runtime** (`plant_runtime.c` + declarations in `plant_runtime.h`):
  - `plant_sort` — `qsort` over list items with a spec-driven
    comparator. Comparisons are numeric-aware (values that fully parse
    as doubles compare numerically and sort before non-numbers,
    otherwise `strcmp`); multi-field evaluation walks the spec keys in
    order, using the first decisive field. Elements that are not maps
    contribute `""` for any field. NULL/non-list/short lists pass
    through untouched.
  - `plant_shuffle` — Fisher-Yates in place over a `rand()` source
    seeded once from `time ^ pid`, uniform over all `n!`
    permutations; empty and single-element lists pass through.
- Parser integration notes: `SORT`/`SHAKE` join the statement dispatch
  and the block-body keyword list (statements inside `IF`/`SEASON`
  bodies).

### Regression Tests
- `sort`: default-ASC string sort, explicit ASC/DESC, numeric-aware
  sort of numeric strings, duplicate values, single-element and empty
  lists, `BY name` ascending, `BY age DESC`, multi-field
  `BY dept ASC, age DESC` (tie-breaking), and `BY dept, age` with
  default per-field directions. Maps are built with bare
  `plant_list_make` pair-list calls.
- `shuffle`: multiset preservation (element sum before/after), the
  canonical sorted order after shaking (permutation verification),
  and a two-shuffle order-difference check (1/40320 false-positive
  chance at n=8) for randomization; single-element and empty lists
  survive shaking unchanged.

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
