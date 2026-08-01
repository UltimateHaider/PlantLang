# PlantLang — Chloroplast Build Guide (Pure Native Self-Hosting Toolchain)

This document covers building, testing, packaging, and installing the **pure
native self-hosted PlantLang compiler — Chloroplast** (`src/plantc/*.plant` → C).

## Requirements

- `gcc` (or `CC` override) and GNU `make`
- `clang-format`, `cppcheck` — optional (used by `make fmt` / `make lint`, skipped when missing)

No Node.js, no JavaScript, no other dependencies.

## Quick Reference

```sh
make help        # list all targets with descriptions
make             # = make all
make all         # full native build → bin/Chloroplast
make self        # multi-generation self-hosting + convergence check
make test        # native integration tests (bin/Chloroplast + gcc + run + diff)
make fmt         # clang-format generated C (skips if missing)
make lint        # cppcheck generated C (skips if missing)
make dist        # versioned tarball + unpack/build/test validation
make install     # install to $PREFIX/bin (default ~/.local)
make clean       # remove build artifacts (keeps dist/Chloroplast bootstrap)
```

Variables: `VERSION` (default `0.47.1`), `PREFIX` (default `~/.local`),
`CC` (default `gcc`).

## How the Bootstrap Works

`dist/Chloroplast` is the pre-built **bootstrap compiler**. The chain is:

```
dist/Chloroplast (v1) ──► build/plantc_v2.c ──► build/plantc_v2 (gcc)
                    ──► build/plantc_v3.c ──► build/plantc_v3 (gcc)   ← bin/Chloroplast
                    ──► build/plantc_v4.c ──► build/plantc_v4
                    ──► build/plantc_v5.c
```

`make self` additionally checks that `plantc_v3.c`, `plantc_v4.c`, and
`plantc_v5.c` are byte-identical (fixed-point convergence), proving the
compiler is self-hosting and stable.

The compiler sources are concatenated in dependency order
(`lexer.plant parser.plant codegen_c.plant main.plant`), with `IMPORT`/`PLANT`
directives stripped:

```sh
cat src/plantc/{lexer,parser,codegen_c,main}.plant | \
  grep -v '^IMPORT\|^PLANT ' > build/plantc_all.plant
```

## Compiling a PlantLang Program

```sh
./bin/Chloroplast app.plant app.c            # generate C
gcc -w -O0 -I runtime/c app.c runtime/c/plant_runtime.c -lm -o app
./app
```

The generated C includes `<plant_compat.h>` (runtime helpers) and links against
`runtime/c/plant_runtime.c`. Link with `-lm` when using `std/math`
(`math_sin`, `math_sqrt`, …) or any other module — the runtime always
references libm functions.

## Standard Library (std/*)

Since v0.47.1 the core standard library ships in the native runtime
(signatures in `plant_compat.h`, implementations in `plant_runtime.c`):

- `std/json` — `json_parse` (→ native MAP/LIST/scalars; safe nil `NULL` on
  invalid JSON, never crashes), `json_stringify`, `json_get`, `json_at`,
  `json_len`, `json_kind`, `json_val`
- `std/string` — `string_repeat`, `string_reverse`, `string_pad`
- `std/fs` — `file_copy`, `file_move`, `file_stat` (MAP: size/mtime/mode)
- `std/math` — `math_sin`, `math_cos`, `math_sqrt`, `math_pow`,
  `math_floor`, `math_ceil`, `math_round`, `math_min`, `math_max`,
  `math_random`
- `std/time` — `time_now`, `time_format`, `time_parse`, `time_sleep`

## Tests

`make test` — native integration suite in `tests/native/`:

- CLI checks (`--help`, `--version` → `Chloroplast 0.47.1 (pure native)`,
  missing-file exit code)
- compile + gcc + run + output-diff cases (hello, join/`strings:LENGTH`,
  number concatenation, string escapes, list ops, `strings:` module calls)
- standard library cases: `json` (valid + invalid/nil + unicode + stringify
  round-trip + raw MAP), `strings2` (repeat/reverse/pad), `fs`
  (copy/move/stat + error paths), `math`, `time`

The legacy JavaScript test suites were removed in v0.46.4 (Pure Native purge).

## Packaging (make dist)

`make dist` produces `release/plantlang-<VERSION>.tar.gz` containing the
sources, runtime, Makefile, bootstrap compiler (`dist/Chloroplast`), tests, and
docs. It then extracts the tarball into `build/distcheck/` and re-runs
`make all` and `make test` inside it — the tarball only ships if the unpacked
copy builds and passes all tests (`DISTCHECK OK`).

## Installing

```sh
make install                      # → ~/.local/bin/Chloroplast
make install PREFIX=/usr/local    # system-wide
```

Installs the compiler binary and the C runtime headers
(`plant_compat.h`, `plant_runtime.h`, `plant_runtime.c`) into
`$PREFIX/bin` and `$PREFIX/include/plantlang/`, then verifies
`Chloroplast --version`.

## Known Limitations

- `SET` requires a prior `CREATE` declaration (no auto-declaration).
- Concatenating a numeric **result** (e.g. `"n=" + plant_array_length(x)`)
  is not yet stringified; numeric **literals** are wrapped in `_from_long()`
  automatically.
- Generated code is not yet clang-formatted; only the grammar subset used by
  the compiler itself is covered by the integration suite.
