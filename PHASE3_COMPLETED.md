# Phase 3 Completion — Native Self-Hosting Toolchain

Status: **COMPLETE** — 2026-07-31

## Summary

Phase 3 delivers a fully self-hosted PlantLang compiler with a `make`-driven
native toolchain. The compiler in `src/plantc/*.plant` (lexer, parser, C
codegen, driver) is compiled to C, bootstrapped through five generations, and
verified to reach a byte-stable fixed point.

## Deliverables

### 1. Native Makefile toolchain (`Makefile`)

| Target    | Purpose                                                        |
|-----------|----------------------------------------------------------------|
| `all`     | Full native build → `bin/plantc` (v1→v2→v3 chain)              |
| `self`    | Multi-generation self-hosting + byte-convergence check         |
| `test`    | Native integration suite (compile + gcc + run + diff)          |
| `test-js` | Legacy JS suites (requires node)                               |
| `fmt`     | clang-format generated C (skips when missing)                  |
| `lint`    | cppcheck generated C (skips when missing)                      |
| `dist`    | Versioned tarball + unpack/build/test validation (`DISTCHECK`) |
| `install` | Install to `$PREFIX` (default `~/.local`) + version check      |
| `clean`   | Remove build artifacts (keeps `dist/plantc` bootstrap)         |
| `help`    | Target list with descriptions                                  |

`.DEFAULT_GOAL := all`.

### 2. Self-Hosting Pipeline

```
dist/plantc (v1 bootstrap) → build/plantc_v2 → build/plantc_v3 (bin/plantc)
                           → build/plantc_v4 → build/plantc_v5
```

`make self` verifies `plantc_v3.c == plantc_v4.c == plantc_v5.c` — the
compiler reproduces itself byte-for-byte. Current fixed point:
**69 659 bytes** of generated C (`v3 == v4 == v5`).

### 3. CLI

- `plantc --help` / `-h` — usage + options, exit 0
- `plantc --version` / `-v` — `plantc 1.0.0-phase5 (self-hosted)`, exit 0
- `plantc missing.plant` — error + exit 1 (fixed `fs:EXISTS` check; `NOT`
  on a string flag is not valid C semantics, so the driver now compares
  `exists ISNT "1"`)

### 4. Integration Test Suite (`tests/native/`)

9 checks, all passing with the native binary:

- CLI: `--help`, `--version`, missing-file exit code
- `hello` — basic SHOW
- `stress` — LIST + join action + `strings:LENGTH` in an expression
- `concat` — number literals in string concatenation (`_cat("n", _from_long(5))`)
- `escapes` — `\"`, `\\`, `\t` string escaping
- `lists` — `plant_list_push`/`plant_list_get`/`_at`
- `modules` — `strings:LENGTH`, `strings:REPLACE` module calls

### 5. Packaging & Installation

- `make dist` → `release/plantlang-1.0.0-phase5.tar.gz` (sources, runtime,
  bootstrap, Makefile, tests, docs); extracts and re-runs `make all` +
  `make test` inside the tarball (`DISTCHECK OK`)
- `make install PREFIX=~/.local` → `~/.local/bin/plantc` +
  `~/.local/include/plantlang/` + `plantc --version` verification

## Fixes Landed During This Phase

- `_handle_cat`: skip the character after `\` inside strings (quote-toggle bug
  on `"\"" + x + "\""` patterns)
- `_handle_cat`: wrap pure-digit concat parts in `_from_long(...)` (numbers no
  longer crash `_cat` at runtime)
- `_handle_func_paren`: two-pass split for `kw (` / `kw(` (spaced `LEN ( x )`)
- `put_stmt`: emit `plant_list_push(target, item)`; `create_stmt` LIST type
  emits `PlantArray*`
- `translate_expr`: ` : ` module separators → `_` (`strings:LENGTH(r)` in
  expressions)
- Action trailing-return parity; `(COUNT x)` paren fix (avoid
  `plant_array_length(x>0)` pointer comparisons)
- Driver: `--help`/`--version` handling; nonzero exit for missing input

## Known Gaps

- `SET` on an undeclared variable does not auto-declare (declarations are
  emitted only by `CREATE` / parameters)
- Concatenating a numeric *result* (e.g. `"n=" + plant_array_length(x)`) is
  not yet stringified; numeric *literals* are wrapped in `_from_long()`
- `CALM` is a no-op in the C codegen (top-level `GIVE` is used for early exit)
- clang-format/cppcheck not installed in the dev environment — `fmt`/`lint`
  skip gracefully
- CI/CD: not yet wired to the new `make` targets (bootstrap binary
  `dist/plantc` must be tracked first)
