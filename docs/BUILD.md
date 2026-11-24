# PlantLang — Chloroplast Build Guide (Pure Native Self-Hosting Toolchain)

This document covers building, testing, packaging, and installing the **pure
native self-hosted PlantLang compiler — Chloroplast** (`src/plantc/*.plant` → C).

## Requirements

- `gcc` (or `CC` override) and GNU `make`
- `python3` — optional, used by the regression suite's mock HTTP server
  (network cases are skipped when it is absent)
- `clang-format`, `cppcheck` — optional (used by `make fmt` / `make lint`, skipped when missing)

No Node.js, no JavaScript, no other dependencies.

### Windows (MSYS2)

The runtime and test runners are POSIX-oriented (`fork`, `AF_UNIX`, shell
scripts, `diff`, `cmp`), so on Windows build inside **MSYS2**, not from
PowerShell/CMD directly:

```sh
# In an MSYS2 MINGW64/UCRT64 shell (package names may differ per env):
pacman -S --needed gcc make diffutils python curl

cd /d/PlantLang/PlantLang
make all          # -> bin/Chloroplast.exe
make self         # -> SELF-HOSTING CONVERGED
make test         # -> native + generics + closures + regression suites
```

From PowerShell, `scripts\build-windows.ps1` locates MSYS2 and forwards
targets, e.g. `.\scripts\build-windows.ps1 all self test`
(use `-MsysRoot D:\msys64` if MSYS2 is not installed at `C:\msys64`).

Notes:

- Binaries carry the `.exe` suffix: `dist/Chloroplast.exe`,
  `bin/Chloroplast.exe`, `build/plantc_v*.exe`.
- The resulting executables are **MSYS2-dependent** and need `msys-2.0.dll`
  at runtime; they are not standalone Win32 binaries.
- Test runners normalize CRLF fixtures and honor `CC` / `TEST_CFLAGS`, so
  GCC 14+ (which promotes legacy C conversions to errors) also works.
- Mission Mode `SAFE` falls back to an in-process implementation on
  non-Linux hosts. The public API and telemetry match the Linux worker
  engine, but hard OS isolation (`fork`/`AF_UNIX`/`memfd`/`SCM_RIGHTS`)
  is Linux-only; see [Known Limitations](#known-limitations).

### Smart App Control (Windows 11)

Smart App Control (SAC) blocks unsigned executables created locally, including
the `collect2.exe`/`ld` stage of the GCC toolchain. Typical symptoms:

- `collect2.exe: fatal error: cannot execute 'ld': ...`
- `Access is denied` while linking
- a freshly built `bin/Chloroplast.exe` disappears immediately

Check the current state:

```powershell
Get-MpComputerStatus | Select-Object SmartAppControlState
(Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy').VerifiedAndReputablePolicyState
```

`VerifiedAndReputablePolicyState`: `0` = off, `1` = enforcement, `2` = evaluation.

If SAC is enforcing, disable it via
**Windows Security → App & browser control → Smart App Control → Off**.
This is a one-way switch: re-enabling requires resetting or reinstalling
Windows. As an alternative, build inside WSL2 or a Linux CI runner.

SAC is separate from Microsoft Defender antivirus. Defender exclusions do
**not** bypass SAC. If Defender alone quarantines build outputs, add an
exclusion:

```powershell
Add-MpPreference -ExclusionPath 'D:\PlantLang\PlantLang'   # elevated shell
```

## Quick Reference

```sh
make help        # list all targets with descriptions
make             # = make all
make all         # full native build → bin/Chloroplast[.exe]
make self        # multi-generation self-hosting + convergence check
make test        # native + generics + closures + regression suites
make fmt         # clang-format generated C (skips if missing)
make lint        # cppcheck generated C (skips if missing)
make dist        # versioned tarball + unpack/build/test validation
make install     # install to $PREFIX/bin (default ~/.local)
make clean       # remove build artifacts (keeps the bootstrap seed)
```

Variables: `VERSION` (default `0.63.3`), `PREFIX` (default `~/.local`),
`CC` (default `gcc`/`cc`). On Windows the build appends `.exe` automatically
and uses the checked-in `dist/` bootstrap seeds described below.

## How the Bootstrap Works

On Linux, `dist/Chloroplast` is the pre-built **bootstrap compiler**. On
Windows the committed `dist/Chloroplast.exe` seed is used; if it is missing,
it is compiled from `dist/bootstrap/plantc_bootstrap.c` (portable generated
C). Either way the chain is:

```
bootstrap (v1) ──► build/plantc_v2.c ──► build/plantc_v2 (gcc)
               ──► build/plantc_v3.c ──► build/plantc_v3 (gcc)
               ──► build/plantc_v4.c ──► build/plantc_v4   ← bin/Chloroplast
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

## Binary Artifacts in Git

Only the `dist/` bootstrap seeds are tracked:

- `dist/Chloroplast` — the **Linux v1 seed**. It is a pre-built snapshot of the
  compiler, checked in at the last stable release, and it is **never rebuilt
  automatically** (`make clean` keeps it). It starts the bootstrap chain by
  compiling `src/plantc/*.plant` into `build/plantc_v2.c`.
- `dist/Chloroplast.exe` — the **Windows v1 seed** used by `make` on MSYS2.
- `dist/bootstrap/plantc_bootstrap.c` — portable generated-C snapshot used to
  rebuild `dist/Chloroplast.exe` when the binary seed is missing.

`bin/` and `build/` are **generated locally** and are gitignored:

- `bin/Chloroplast[.exe]` is the v4 compiler produced by `make all`.
- `build/plantc_all.plant` and `build/plantc_v2`…`v5` are intermediate
  generations of the self-hosting chain; `make self` re-derives them and
  confirms a fixed point (`plantc_v3.c == plantc_v4.c == plantc_v5.c`).

A fresh clone must run `make all` before using `./bin/Chloroplast[.exe]`;
`make test` builds it automatically. Do not commit artifacts from `bin/` or
`build/`.

Do not rebuild `dist/Chloroplast` from current sources — the point of the
bootstrap seed is that it is an immutable, known-good starting point. When a
release intentionally refreshes the seed, use `make bootstrap-update` and
commit the result with the sources. The release tarball (`make dist`) ships
`$(BOOTSTRAP)` from `dist/`, not from `bin/`.

On Windows, `dist/Chloroplast` is a Linux ELF and cannot run. `make` uses
`dist/Chloroplast.exe` instead. Refresh the binary seed and the portable C
snapshot together:

```sh
make all && make self
make bootstrap-update
cp build/plantc_v4.c dist/bootstrap/plantc_bootstrap.c
```

## Compiling a PlantLang Program

```sh
./bin/Chloroplast app.plant app.c            # generate C
gcc -w -O0 -std=gnu11 -I runtime/c app.c runtime/c/plant_runtime.c -lm -o app
./app
```

On Windows use `./bin/Chloroplast.exe` and link with `-ldl` as well (MSYS2
provides it); the runtime uses the POSIX loader shim for FFI.

The generated C includes `<plant_compat.h>` (runtime helpers) and links against
`runtime/c/plant_runtime.c`. Link with `-lm` when using `std/math`
(`math_sin`, `math_sqrt`, …) or any other module — the runtime always
references libm functions.

## Standard Library (std/*)

`std/*` refers to runtime module names (`std/json`, `std/string`, …), not a
`std/` directory (the legacy `std/` directory was removed in v0.63.1).
Since v0.47.1 the core standard library ships in the native runtime
(signatures in `plant_compat.h`, implementations in `plant_runtime.c`):

- `std/json` — `json_parse` (→ native MAP/LST/scalars; safe nil `NULL` on
  invalid JSON, never crashes), `json_stringify`, `json_get`, `json_at`,
  `json_len`, `json_kind`, `json_val`
- `std/string` — `string_repeat`, `string_reverse`, `string_pad`
- `std/fs` — `file_copy`, `file_move`, `file_stat` (MAP: size/mtime/mode)
- `std/math` — `math_sin`, `math_cos`, `math_sqrt`, `math_pow`,
  `math_floor`, `math_ceil`, `math_round`, `math_min`, `math_max`,
  `math_random`
- `std/time` — `time_now`, `time_format`, `time_parse`, `time_sleep`
- Native data structures (v0.47.2) — `set_create/add/has/remove/size/to_list`,
  `queue_create/push/pop/peek/size`, `stack_create/push/pop/peek/size`
- Advanced FFI (v0.47.3) — `REF` pointer parameters, `-> Result<T, E>`
  returns, `ffi_free(ptr)`, `ffi_last_error()` (errno), `ffi_last_error_msg()`
  (dlerror → strerror); link with `-ldl` for loader diagnostics

## Tests

`make test` runs four suites:

- `tests/native/` — CLI checks (`--help`, `--version`, missing-file exit
  code) and compile + gcc + run + output-diff cases (language features,
  standard library, advanced FFI with the mock C library)
- `tests/generics/` — generics engine integration (+ optional `.grep`
  structural checks on generated C)
- `tests/closures/` — closures engine integration (+ optional `.grep` checks)
- `tests/regression/` — numeric/FFI/async/SAFE regressions; network cases
  (`harvest_*`, `listen_*`, `json_http`) need `python3` and are skipped
  when it is unavailable

All runners honor `CC` and `TEST_CFLAGS`, normalize CRLF fixtures, and work
from any current directory. The legacy JavaScript test suites were removed in
v0.46.4 (Pure Native purge); the remaining legacy JavaScript source files were
removed in v0.63.1 (repository cleanup).

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
- **Windows SAFE isolation:** Mission Mode `SAFE` keeps its full public API,
  permission checks, audit log, and pool telemetry, but executes adapters
  in-process. The Linux engine's hard isolation (separate worker processes,
  `AF_UNIX SOCK_SEQPACKET`, `memfd`, `SCM_RIGHTS` fd passing) is unavailable
  there, so a crashed adapter can take down the host process. Run untrusted
  SAFE workloads on Linux when isolation is required.
- **Windows Smart App Control:** blocks locally built unsigned binaries and can
  break the GCC link step. See [Smart App Control](#smart-app-control-windows-11).


## Bootstrap Seed

`dist/Chloroplast` is the pre-built compiler used to start the self-hosting
chain on Linux. Build the compiler first, then refresh the seed:

    make all && make self
    make bootstrap-update        # cp bin/Chloroplast dist/Chloroplast

On Windows the binary seed is `dist/Chloroplast.exe`; refresh it and the
portable generated-C snapshot together:

    make all && make self
    make bootstrap-update
    cp build/plantc_v4.c dist/bootstrap/plantc_bootstrap.c

Then verify: `make clean && make all && make self && make test`

`make clean` removes `bin/` and `build/`, so the checked-in `dist/` seeds are
what lets a clean tree rebuild.

### v0.51.4 bootstrap upgrade

`dist/Chloroplast` was upgraded from **v0.50.0h** to **v0.51.3b**. The old
v0.50.0h seed did not support the `GLOBAL` keyword and silently **dropped**
`GLOBAL` statements; putting a `GLOBAL` (the Type Registry) in the compiler
source therefore produced a broken first bootstrap step
(`TYPE_REGISTRY undeclared`). The seed now supports GLOBAL, so the compiler
source may use it. Keep `dist/Chloroplast` in sync with `bin/Chloroplast`
at each release (`make bootstrap-update`).

