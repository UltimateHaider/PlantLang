# 🌿 PlantLang — Chloroplast v0.17

A programming language designed to be read like natural prose.

## Quick Start

```bash
node chloroplast.js run    examples/01_basics.plnt
node chloroplast.js verify tests/suite.plnt
node chloroplast.js repl
```

## Core Keywords (Complete)

| Category | Keywords |
|---|---|
| Variables | CREATE SET INCREASE DECREASE SHOW LOCK ROOT EVAPORATE |
| Logic | IF ORIF ELSE STOP IF PICK |
| Matching | MATCH YIELD |
| Loops | CYCLE SEASON |
| Lists | PUT TAKE SORT SHAKE EMPTY BRAID |
| Maps | LINK |
| Actions | ACTION GIVE REAP FLOW |
| Objects | SPECIES BLOOM PARENT SELF VAR |
| Storms | WEATHER SHELTER CALM |
| Files | TAP ABSORB INFUSE SEAL |
| Reactive | PULSE WHENEVER CHANGES |
| Time | NOW WAIT |
| Introspection | ANALYZE TYPEOF |
| Config | ROOT_SCOPE MISSION PLANT |
| Testing | VERIFY SUITE SHOW_VERIFY_SUMMARY STORMS GIVES |
| Network | HARVEST |

## HARVEST — HTTP/HTTPS Client

```
HARVEST "https://api.example.com/data" AS result.
HARVEST url METHOD:POST BODY:payload AS result.
HARVEST url HEADERS:hdrMap AS result.
HARVEST url METHOD:POST BODY:payload HEADERS:hdrMap TIMEOUT:5 AS result.
```

- JSON responses are mapped to native `MAP`/`LIST` structures recursively.
- Non-JSON responses are returned as `TX`.
- Network failures throw `NETWORK_STORM` (catchable via `WEATHER`/`SHELTER` or `VERIFY STORMS`).
- HTTP error status codes (4xx/5xx) do **not** throw — they're normal data (`result:"status"`, `result.ok`).
- Execution is synchronous from the language's perspective: a worker thread performs
  the real `fetch()` and writes the result into a `SharedArrayBuffer`; the main thread
  blocks via `Atomics.wait` directly on that buffer (not on a worker `message` event),
  which is what makes true synchronous-style blocking safe without deadlocking.

## Test Suite

```bash
# Legacy suite (56 tests, Arabic-output)
node chloroplast.js run tests/all.plnt

# VERIFY suite (70 tests, written in PlantLang itself)
node chloroplast.js verify tests/suite.plnt
```

## Files

- `core/lexer.js`          — tokenizer
- `core/runtime.js`        — Soil (scope chain), Storms, types
- `core/evaluator.js`      — expression / condition evaluation
- `core/innate.js`         — built-in libraries (math/strings/lists/fs/io)
- `core/interpreter.js`    — execution engine
- `core/harvest.js`        — synchronous HTTP/HTTPS bridge
- `core/harvest_worker.js` — worker thread performing the actual fetch
- `chloroplast.js`         — CLI (run / verify / repl / check)
- `examples/`              — 5 full example programs
- `tests/all.plnt`         — 56-test legacy suite
- `tests/suite.plnt`       — 70-test VERIFY suite (includes HARVEST tests)

## Diagnostics — Visual Error Pointers (^)

PlantLang tracks precise `line` and `column` for every statement and threads
this location through the Storm system, even for storms thrown deep inside
`runtime.js` (e.g. `LOCK_STORM` from `Soil.update()`) or `evaluator.js`
(e.g. `ZERO_STORM` from division). When an uncaught Storm reaches the CLI,
it's rendered as a clean diagnostic panel instead of a raw JS stack trace:

```
⛈️  Atmospheric Storm Panic: MISSING_STORM
  --> examples/06_diagnostics.plnt:21:4

  21 \ 1\ SHOW subtotl.
        ^

Error: "subtotl" Not exist
```

- `core/diagnostics.js` — `formatStormDiagnostic(err, filePath, sourceText)`
- Run `node tests/test_diagnostics.js` to verify the system end-to-end
  (20 checks: line/column accuracy, panel structure, caret alignment,
  clean process exit, no leaked stack traces).
- `examples/06_diagnostics.plnt` — a runnable demo with an intentional typo.

## LISTEN BRANCH — Web Server Grammar (Phase 1: Lexer/Parser/Diagnostics)

```
LISTEN BRANCH ON [portExpr] WITH [configExpr] AS [requestIdent] MAP,
  ...handler body, may use GIVE [expr] AS RESPONSE...
LISTEN/.
```

This phase implements the **grammar layer**: tokenizer registration, strict
multi-stage validation (`ON` → `WITH` → `AS` → `MAP`), structured AST node
shapes (`core/ast.js`), and coordinate-aware diagnostics. Each connective
keyword is validated independently; a missing or misspelled `ON`/`WITH`/
`AS`/`MAP` raises `SYNTAX_STORM` with the caret aimed at the exact offending
token via `core/lexer.js`'s `subTokenColumn()` helper.

The handler body executes synchronously once (request bound to an empty MAP)
so logic and `GIVE ... AS RESPONSE` extraction can be authored and tested
ahead of the real listener runtime (true socket binding is a later phase).

- `core/ast.js` — `ListenBranchStatementNode`, `ResponseStatementNode`
- `core/interpreter.js` — `parseListenBranch()`, `parseResponseStatement()`
- `examples/07_server_syntax_error.plnt` — intentional "missing ON" demo
- `node tests/test_diagnostics.js` covers all 4 grammar breakpoints

## Compiler Frontend Migration (Phase 1: incremental, in progress)

PlantLang's engine is migrating from a regex-based flat-statement loop
to a formal compiler frontend: **character-level tokenizer → token
stream → recursive-descent parser → typed AST → node-routed evaluator**.
This is being done incrementally, verified at every milestone, with the
original 176-test regression matrix kept fully intact throughout (it
continues to run unmodified against the proven legacy engine).

**New modules (additive — the legacy engine is untouched):**

- `core/tokenizer.js` — pure character-by-character state machine.
  Produces `Token {type, value, line, column, depth}`. Depth is
  established by a leading `N\` marker and inherited by every token
  on that logical line; columns are tracked with absolute precision
  (verified against hand-counted source in `tests/test_tokenizer.js`).
- `core/ast.js` — formal node hierarchy (`AstNode` base class with
  `{line, column, depth}` on every node): `ProgramNode`,
  `CreateStatementNode`, `ShowStatementNode`, `IdentifierNode`,
  `LiteralNode`, plus the prior phase's `ListenBranchStatementNode`
  and `ResponseStatementNode` (now upgraded with `depth`).
- `core/parser.js` — recursive-descent `Parser` with `peek()`/
  `consume()`/`match()`. Fully migrated statement kinds: `SHOW`,
  `CREATE` (including the `PULSE` modifier and the bare `TO.`
  empty-list idiom), `LISTEN BRANCH`/`GIVE...AS RESPONSE` (with full
  block-aware nested-body collection, generalizing the ACTION/SPECIES
  depth-closer pattern to the token stream). Every other statement
  kind is safely captured as a `RawStatementNode` so a complete
  real-world `.plnt` file can be parsed end-to-end without crashing
  on constructs not yet migrated. Grammar violations raise
  `SYNTAX_STORM` with the exact offending token's `{line, column}`,
  rendered by the unmodified `core/diagnostics.js` caret system.
- `core/interpreter.js` additions — `runSource()`/`runProgram()`
  entry points (alongside, not replacing, `run()`), `evaluateNode()`
  central router, and typed evaluators (`evaluateCreateStatement`,
  `evaluateShowStatement`, `evaluateListenBranch`,
  `evaluateResponseStatement`) operating on the same `Soil`
  scope-chain as the legacy pipeline.

**Verification:**
```bash
node tests/test_tokenizer.js          # 24 checks — character scanning, depth, columns
node tests/test_parser_migration.js   # 47 checks — AST shape, SYNTAX_STORM accuracy,
                                       #             whole-corpus parse coverage,
                                       #             explicit guard that the legacy
                                       #             176-test matrix is still green
```

**Not yet migrated** (still routed through `RawStatementNode` →
legacy `_execOne` fallback inside `evaluateNode()`): IF/MATCH/CYCLE/
SEASON, ACTION/SPECIES/BLOOM, HARVEST, VERIFY/SUITE, TAP/ABSORB/
INFUSE/SEAL, and all remaining statement kinds. These migrate in
subsequent milestones following the same verify-before-proceeding
discipline.

### WEATHER / SHELTER / CALM (migrated this milestone)

```
WEATHER,
  ...protected body, may raise a Storm...
SHELTER STORM_TYPE [AS errVar],
  ...recovery body...
[SHELTER ANOTHER_TYPE, ...]*
CALM.
```

Fully migrated to typed AST nodes (`WeatherStatementNode`,
`ShelterStatementNode`, `CalmStatementNode` in `core/ast.js`) with
dedicated recursive-descent parsing (`parseWeatherStatement`,
`parseShelterClause`, `parseCalmClause` in `core/parser.js`) and
typed evaluators (`evaluateWeatherStatement`, `evaluateShelterStatement`,
`evaluateCalmStatement` in `core/interpreter.js`). Each connective
keyword is validated independently; a missing `CALM` or malformed
`SHELTER` clause raises `SYNTAX_STORM` with the caret aimed at the
exact offending token. The protected body and each `SHELTER` clause's
recovery body run in their own sandboxed child `Soil` scope — locals
created inside never leak to the enclosing scope (verified explicitly:
a `CREATE` inside `WEATHER`'s body, and a `SHELTER`'s bound `errVar`,
are both confirmed invisible outside the block).

**Two real bugs found and fixed while verifying this migration against
the existing `examples/03_storms.plnt` corpus file** (not synthetic
test cases — actual pre-existing real-world usage):
1. `evaluateCreateStatement` didn't replicate the legacy engine's
   special-case `CREATE x(LIST) TO a, b, c.` comma-split parsing,
   silently producing a joined string instead of a real array.
2. `runSource()`/`runProgram()` never ran `_firstPass()` (which
   pre-registers `ROOT`/`ROOT_SCOPE`/`ACTION`/`SPECIES` definitions),
   so a `ROOT` constant referenced from inside an AST-routed `WEATHER`
   block was invisible — fixed by running `_firstPass()` against the
   legacy lexer's flat statement list before AST execution, mirroring
   exactly what `run()` already does for the legacy pipeline.

Coordinate-pure diagnostics were verified end-to-end: a `ZERO_STORM`
raised by a bare division *inside* a `WEATHER` body (with no matching
`SHELTER` clause, so it propagates uncaught) correctly carries the
**inner statement's** line/column — not the `WEATHER` header's —
because `evaluateNode()`'s location-backfill wraps every node
individually, so the innermost frame's coordinates win.
