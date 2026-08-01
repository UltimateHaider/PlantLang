# PlantLang — Chloroplast

**Pure Native 🚀** · Self-Hosting Compiler & Build System · v0.47.2

**Chloroplast** is the official PlantLang compiler: a 100% pure native, self-hosted
toolchain that compiles PlantLang source to C and links it against a lightweight C
runtime. No Node.js, no JavaScript, no interpreter — the compiler compiles itself
(`dist/Chloroplast` → v1 → v2 → v3 …) and converges to a byte-identical fixed point.

---

## Why PlantLang?

You've written code in C, Python, JavaScript, Rust, Go — and you're tired of
picking between readable and fast. PlantLang refuses the tradeoff:

- **Prose-based syntax.** Code reads like English sentences: `CREATE x(NUM) TO 42.`
  `SHOW "hello".` `IF x IS > 10, ... /IF.` Less bracket noise, fewer symbols,
  easier review — for humans *and* for AI assistants.
- **Native performance.** Every program compiles to C and then to a native
  executable. No virtual machine, no garbage collector pauses.
- **A compiler you can trust.** Chloroplast is self-hosted: the compiler is
  written in the language it compiles, and `make self` proves convergence across
  generations byte-for-byte.
- **Deterministic memory.** The Rooted Depth System allocates from per-scope
  arena slabs — freed automatically when a scope exits, with zero GC.

---

## Standout Features

- **`ACTION` functions** with `REAP` params and `GIVE` returns (recursion included)
- **`IF / ORIF / ELSE`**, **`CYCLE`** (numeric loops), **`SEASON`** (condition loops),
  **`FOR ... IN`** (iteration over lists/maps/strings)
- **`WEATHER / SHELTER / CALM`** — deterministic exception handling with typed matching
- **`MATCH`** — exhaustive pattern matching on tagged unions (`CHOICE`)
- **`SHAPE` / `STRUCT` / `SPECIES`** — structs and OO-style classes with inheritance
- **`LIST` / `MAP`** — dynamic arrays and typed hash tables
- **`IMPORT`** — multi-file module system with cycle detection
- **FFI** — declare `-> external` C functions for direct native interop
- **`PLANT std`** — built-in runtime services (filesystem, strings, lists, math)
- **Standard library (v0.47.2)** — `std/json` (parse/stringify with safe nil on
  invalid JSON), `std/string` (repeat/reverse/pad), `std/fs` (copy/move/stat),
  `std/math` (sin/cos/sqrt/pow/floor/ceil/round/min/max/random),
  `std/time` (now/format/parse/sleep) — pure native C
- **Self-hosting** — the entire compiler pipeline (lexer, parser, C codegen,
  CLI) is written in PlantLang under `src/plantc/`

---

## Quick Start

Requires `gcc` and `make`. No other dependencies.

```sh
make all        # full native build → bin/Chloroplast (v1→v2→v3 chain)
make self       # multi-generation self-hosting + byte-convergence check
make test       # native integration suite (compile + run + compare)
make install    # install to ~/.local (PREFIX=/path/to/prefix to override)
```

Write `hello.plant`:

```plantlang
CREATE msg(TX) TO "hello, world".
SHOW msg.
```

Compile and run with Chloroplast:

```sh
./bin/Chloroplast hello.plant out.c
gcc -w -O0 -I runtime/c out.c runtime/c/plant_runtime.c -lm -o hello
./hello
```

CLI:

```sh
./bin/Chloroplast --help        # usage + options
./bin/Chloroplast --version     # Chloroplast 0.47.2 (pure native)
```

For the full build system reference (targets, packaging, install), see
[docs/BUILD.md](docs/BUILD.md).

---

## A Taste of the Language

```plantlang
# Functions
ACTION greet(name(TX)) -> TX,
  GIVE "hello, " + name.
/ACTION.

# Conditionals & loops
CYCLE i FROM 1 TO 5,
  IF i IS > 3,
    SHOW "big: " + i.
  ORIF i IS 2,
    SHOW "two".
  ELSE,
    SHOW "small: " + i.
  /IF.
/CYCLE.

# Lists
CREATE xs(LIST) TO 1, 2, 3.
CREATE n(NUM) TO PLANT list:LENGTH xs.

# Errors
WEATHER,
  CREATE q(NUM) TO 10 / 0.
SHELTER,
  SHOW "caught division by zero".
/WEATHER.
```

> **Note:** this is a flavor sample. The complete ground-up tutorial is in
> [Language Tour.md](Language%20Tour.md).

---

## Language Core at a Glance

| Concept | PlantLang | Familiar equivalent |
|---|---|---|
| Integer | `NUM` | `int` (64-bit) |
| Float | `SCL` | `double` |
| String | `TX` | `string` |
| Boolean | `FACT` | `bool` |
| Function | `ACTION name(args) -> RET` | `fn` / `def` |
| Struct | `SHAPE Point { x(NUM), y(NUM) }.` | `struct` |
| Class | `SPECIES Greeter { ... }` | `class` |
| Tagged union | `CHOICE Option { Some(NUM), None }.` | `enum` + payload |
| Array | `LIST` / `MAP` | `Vec` / `HashMap` |
| Exceptions | `WEATHER / SHELTER / CALM` | `try / catch` |

Memory: deterministic arena slabs per scope depth (Rooted Depth System) —
no GC, no manual free.

---

## Where to Go Next

| Document | What you'll find |
|---|---|
| [Language Tour.md](Language%20Tour.md) | Complete ground-up syntax & mechanics guide |
| [TECHNICAL.md](TECHNICAL.md) | Deep technical details: architecture, codegen, memory model, systems |
| [ROADMAP.md](ROADMAP.md) | Completed milestones + future plans (v0.47.0 → v1.0.0) |
| [docs/BUILD.md](docs/BUILD.md) | Build system reference: targets, packaging, install |
| [CHANGELOG.md](CHANGELOG.md) | Full version history through v0.47.2 |
| [PHASE4_COMPLETED.md](PHASE4_COMPLETED.md) | Pure Native transition record (plantc → Chloroplast) |

---

## Contributing / Source Layout

```
dist/Chloroplast      pre-built v1 bootstrap compiler (never rebuilt)
src/plantc/           compiler sources written in PlantLang
  lexer.plant         tokenizer
  parser.plant        parser + AST
  codegen_c.plant     C code generator
  main.plant          CLI driver (--help / --version / compile)
runtime/c/            native runtime (plant_runtime.c/.h, plant_compat.h)
tests/native/         integration test suite
```

Chloroplast is self-hosted and MIT licensed.
