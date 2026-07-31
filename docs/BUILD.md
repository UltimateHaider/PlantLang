# PlantLang — Build Guide (Native Self-Hosting Toolchain)

This document covers building, testing, packaging, and installing the **native
self-hosted PlantLang compiler** (`src/plantc/*.plant` → C).

## Requirements

- `gcc` (or `CC` override) and GNU `make`
- `clang-format`, `cppcheck` — optional (used by `make fmt` / `make lint`, skipped when missing)
- `node` — only for the legacy JS test suites (`make test-js`)

## Quick Reference

```sh
make help        # list all targets with descriptions
make             # = make all
make all         # full native build → bin/plantc
make self        # multi-generation self-hosting + convergence check
make test        # native integration tests (bin/plantc + gcc + run + diff)
make fmt         # clang-format generated C (skips if missing)
make lint        # cppcheck generated C (skips if missing)
make dist        # versioned tarball + unpack/build/test validation
make install     # install to $PREFIX/bin (default ~/.local)
make clean       # remove build artifacts (keeps dist/plantc bootstrap)
```

Variables: `VERSION` (default `1.0.0-phase5`), `PREFIX` (default `~/.local`),
`CC` (default `gcc`).

## How the Bootstrap Works

`dist/plantc` is the pre-built **bootstrap compiler**. The chain is:

```
dist/plantc (v1) ──► build/plantc_v2.c ──► build/plantc_v2 (gcc)
                ──► build/plantc_v3.c ──► build/plantc_v3 (gcc)   ← bin/plantc
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
./bin/plantc app.plant app.c            # generate C
gcc -w -O0 -I runtime/c app.c runtime/c/plant_runtime.c -o app
./app
```

The generated C includes `<plant_compat.h>` (runtime helpers) and links against
`runtime/c/plant_runtime.c`.

## Tests

- `make test` — native integration suite in `tests/native/`:
  - CLI checks (`--help`, `--version`, missing-file exit code)
  - compile + gcc + run + output-diff cases (hello, join/`strings:LENGTH`,
    number concatenation, string escapes, list ops, `strings:` module calls)
- `make test-js` — legacy JavaScript suites (`tests/test_*.js`, requires node)

## Packaging (make dist)

`make dist` produces `release/plantlang-<VERSION>.tar.gz` containing the
sources, runtime, Makefile, bootstrap compiler, tests, and docs. It then
extracts the tarball into `build/distcheck/` and re-runs `make all` and
`make test` inside it — the tarball only ships if the unpacked copy builds
and passes all tests (`DISTCHECK OK`).

## Installing

```sh
make install                      # → ~/.local/bin/plantc
make install PREFIX=/usr/local    # system-wide
```

Installs the compiler binary and the C runtime headers
(`plant_compat.h`, `plant_runtime.h`, `plant_runtime.c`) into
`$PREFIX/bin` and `$PREFIX/include/plantlang/`, then verifies
`plantc --version`.

## Known Limitations

- `SET` requires a prior `CREATE` declaration (no auto-declaration).
- Concatenating a numeric **result** (e.g. `"n=" + plant_array_length(x)`)
  is not yet stringified; numeric **literals** are wrapped in `_from_long()`
  automatically.
- Generated code is not yet clang-formatted; only the grammar subset used by
  the compiler itself is covered by the integration suite.
