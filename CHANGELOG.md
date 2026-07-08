# Changelog — PlantLang / Chloroplast

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
