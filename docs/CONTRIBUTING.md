# Contributing to PlantLang / Chloroplast

Thanks for your interest in PlantLang. This project is the **pure native
self-hosted PlantLang compiler** — `src/plantc/*.plant` written in PlantLang
itself, compiling to C, with no Node.js or JavaScript in the toolchain.

## Repository layout

- `src/plantc/` — the compiler, written in PlantLang
  (`lexer.plant`, `parser.plant`, `codegen_c.plant`, `main.plant`)
- `runtime/c/` — the C runtime (`plant_runtime.c`, `plant_runtime.h`,
  `plant_compat.h`)
- `tests/regression/` — `.plant` + `.expected` pairs diffed by the test harness
- `tests/native/` — end-to-end suite (compile + gcc + run + diff)
- `docs/` — architecture notes, gap analysis, build guide
- `bin/Chloroplast`, `dist/Chloroplast`, `build/plantc_v*` — bootstrap seeds
  for the self-hosting chain (see `docs/BUILD.md`)

## Building

You need `gcc` and GNU `make`. Nothing else.

```sh
make all    # build bin/Chloroplast via the v1→v2→v3 bootstrap chain
make self   # full self-hosting chain + fixed-point convergence check
make test   # regression + native + generics + closures integration suites
```

`make all` is required before `make test` — the test suites compile and run
programs with the freshly built `bin/Chloroplast`.

## Running the tests

```sh
make test
```

Runs `tests/regression/run_regression_tests.sh` and
`tests/native/run_native_tests.sh`. Expected results are the `*.expected`
files in `tests/regression/`. A test fails when the compiled program's output
does not match its `.expected` file. The suite must be green before a PR is
merged — **regression 156/156, native 20/20** at v0.49.12.

## Code style

Follow the conventions already in the tree — the compiler sources are
self-hosted and must keep compiling through the stale `dist/Chloroplast` v1
seed:

- Indent with two spaces; keep lines short.
- Use the existing keyword vocabulary (`ACTION`, `CREATE`, `SET`, `SHOW`,
  `REAP`, `GIVE`, `IF`, `SEASON`, `PUT`, …) — the language is defined by
  `src/plantc/lexer.plant`.
- Keep concatenated variable names **digit-free** (`m`, `fin`, `mres`) — the
  stale v1 bootstrap only handles simple concat pairs.
- Build multi-segment strings with **stepwise two-operand `SET` concats**,
  never multi-`+` chains, and avoid `+` rewrites inside call parens.
- Match the header comment blocks (`# ── name ──`) used across the `.plant`
  files.
- No trailing whitespace.

## Submitting issues

Use the GitHub issue tracker. Include:

- The exact command you ran
- The `.plant` source and any compiler output
- `bin/Chloroplast --version` (e.g. `Chloroplast 0.49.12 (pure native)`)
- Whether `make self` converges on your machine

## Submitting pull requests

1. Work on a topic branch (default development branch is `Safarna`).
2. Make sure `make all`, `make self`, and `make test` all pass locally.
3. Add a regression fixture (`tests/regression/<feature>.plant` +
   `.expected`) for any behavior change.
4. Update the docs (`CHANGELOG.md`, `docs/GAP_ANALYSIS.md`, and the Language
   Tour) to match your change.
5. Open the PR against `main` or `Safarna`. CI is temporarily disabled
   (see the v0.49.14 changelog) — the local pipeline is the gate: `make all`,
   `make self`, and `make test` must all pass, and the verification results
   should be mentioned in the PR description.

## Version bumps

Each release bumps `VERSION` in the `Makefile`, the banner in
`src/plantc/main.plant`, and the version check in
`tests/native/run_native_tests.sh`, then tags the release commit `v0.49.x`.