# <img src="https://img.shields.io/badge/PlantLang-Chloroplast-4CAF50?style=for-the-badge&logo=tree&logoColor=white" alt="PlantLang Chloroplast" width="400"/>

<p align="center">
  <img src="https://img.shields.io/badge/Pure_Native-🚀-success?style=flat-square" alt="Pure Native"/>
  <img src="https://img.shields.io/badge/Self_Hosting-✅-blue?style=flat-square" alt="Self-Hosting"/>
  <img src="https://img.shields.io/badge/Zero_GC-💚-brightgreen?style=flat-square" alt="Zero GC"/>
  <img src="https://img.shields.io/badge/No_VM-⚡-orange?style=flat-square" alt="No VM"/>
  <img src="https://img.shields.io/badge/License-MIT-ff69b4?style=flat-square" alt="License MIT"/>
  <img src="https://img.shields.io/badge/Version-0.49.9-9cf?style=flat-square" alt="Version 0.49.9"/>
</p>

<p align="center">
  <i>Write code that reads like prose. Compile to C. Run at native speed.</i>
  <br>
  <b>No VM · No GC · No Interpreter · Just Pure Native Performance</b>
</p>

---

## 📖 Table of Contents

- [Why PlantLang?](#-why-plantlang)
- [Philosophy](#-philosophy)
- [Standout Features](#-standout-features)
- [Quick Start](#-quick-start)
- [A Taste of the Language](#-a-taste-of-the-language)
- [Who Is PlantLang For?](#-who-is-plantlang-for)
- [Source Layout](#-source-layout)
- [Build Status](#-build-status)
- [Where to Go Next](#-where-to-go-next)
- [License](#-license)

---

## 🌱 Why PlantLang?

You've written code in **C**, **Python**, **JavaScript**, **Rust**, **Go** — and you're tired of picking between **readable** and **fast**. PlantLang refuses the tradeoff.

<table>
<tr>
<td width="33%">

### 📖 **Readable**
Code reads like English sentences. Less bracket noise, fewer symbols, easier review — for humans *and* for AI.

</td>
<td width="33%">

### ⚡ **Fast**
Compiles to C → native executable. No VM, no interpreter, no GC pauses.

</td>
<td width="33%">

### 🔒 **Trustworthy**
Self-hosted compiler proves itself via byte-identical convergence across generations.

</td>
</tr>
</table>

```plantlang
CREATE age(NUM) TO 25.
CREATE name(TX) TO "Alice".
IF age GREATER THAN OR EQUAL 18,
  SHOW name + " is an adult."
.
```

---

## 🎯 Philosophy

| Principle | Description |
|-----------|-------------|
| **Prose over Symbols** | Code should be readable by humans *and* AI. |
| **Performance by Default** | Every program runs at C speed. |
| **Zero Trust Required** | Self-hosting proves the compiler's correctness. |
| **Deterministic Memory** | Arena allocation — no GC, no manual `free()`. |
| **Modern Features** | Generics, closures, async, pattern matching, FFI. |
| **Simplicity** | No package manager, no VM, no interpreter — just `gcc` + `make`. |

---

## 🚀 Standout Features

<table>
<tr>
<td valign="top" width="50%">

### 🧩 **Core Language**
- `ACTION` functions with recursion & generics
- `IF / ORIF / ELSE` conditional chains
- `CYCLE` (numeric) & `SEASON` (condition) loops
- `FOR ... IN` iteration over lists/maps/strings
- `MATCH` exhaustive pattern matching
- `STRUCT` / `SPECIES` — structs & OO classes

### 📦 **Data Structures**
- `LIST` — dynamic arrays
- `MAP` — typed hash tables
- `CHOICE` — tagged unions with payloads

</td>
<td valign="top" width="50%">

### 🔧 **Advanced Features**
- **Generics** (v0.48.1+) — zero-cost monomorphization
- **Closures** (v0.48.2+) — `MOVE`/`REF` captures
- **Async Engine** (v0.48.3+) — cooperative concurrency
- **FFI** — `-> external` C functions
- **`WEATHER / SHELTER / CALM`** — deterministic exceptions
- **Self-Hosting** — compiler written in PlantLang

### 📚 **Standard Library**
- `std/json` — parse/stringify with safe nil
- `std/string` — repeat/reverse/pad
- `std/fs` — copy/move/stat
- `std/math` — sin/cos/sqrt/pow/round
- `std/time` — now/format/parse/sleep

</td>
</tr>
</table>

---

## ⚡ Quick Start

### Prerequisites

```bash
gcc  # Any modern version
make # GNU make
```

### Build

```bash
git clone https://github.com/your/plantlang.git
cd plantlang
make all        # full native build → bin/Chloroplast
make self       # verify self-hosting convergence
make test       # run all test suites
```

### Write Your First Program

```plantlang
ACTION main,
  SHOW "Hello, world!".
  GIVE 0.
/GIVE main.
```

### Compile & Run

```bash
./bin/Chloroplast hello.plant out.c
gcc -w -I runtime/c out.c runtime/c/plant_runtime.c -lm -o hello
./hello
# Hello, world!
```

```bash
./bin/Chloroplast --help     # usage + options
./bin/Chloroplast --version  # Chloroplast 0.49.9 (pure native)
```

---

## 🌿 A Taste of the Language

```plantlang
# Functions with generics
ACTION greet[T](name(T)) -> TX,
  GIVE "Hello, " + name + "!".
/GIVE.

# Pattern matching
MATCH status {
  200 -> SHOW "OK".
  404 -> SHOW "Not found".
  _   -> SHOW "Other: " + status.
}

# Lists and iteration
CREATE nums(LIST) TO [1, 2, 3, 4, 5].
CYCLE n IN nums,
  SHOW n.
/CYCLE.

# Maps
CREATE config TO {"name": "PlantLang", "version": 1}.
SHOW config["name"].
# → PlantLang

# Error handling
WEATHER,
  CREATE result(NUM) TO 10 / 0.
SHELTER ZERO_STORM AS err,
  SHOW "Caught: " + err.
/WEATHER.

# Async
ASYNC ACTION fetch(url(TX)),
  SHOW "Fetching " + url.
  AWAIT sleep, 100.
  GIVE "Done".
/ASYNC.

ACTION main,
  START fetch, "https://api.example.com".
  AWAIT sleep, 50.
/GIVE main.
```

---

## 🎯 Who Is PlantLang For?

| If you... | PlantLang is for you |
|-----------|---------------------|
| 🧠 **Want readable code** | Prose-based syntax reduces cognitive load |
| ⚡ **Need native performance** | Compiles to C → native executable |
| 🔒 **Trust self-hosting** | Compiler proves itself via byte-identical convergence |
| 💚 **Avoid GC** | Deterministic arena-based memory management |
| 🚀 **Want modern features** | Generics, closures, async, pattern matching, FFI |
| 🧘 **Value simplicity** | No package manager, no VM, no interpreter — just `gcc` + `make` |

---

## 📂 Source Layout

```
dist/Chloroplast      pre-built v1 bootstrap compiler (never rebuilt)
src/plantc/           compiler sources written in PlantLang
  lexer.plant         tokenizer
  parser.plant        parser + AST
  codegen_c.plant     C code generator
  main.plant          CLI driver
runtime/c/            native runtime
  plant_runtime.c     core runtime
  plant_runtime.h     runtime declarations
  plant_compat.h      FFI bridge
tests/                test suites
  regression/         `.plant` + `.expected`
  native/             integration tests
  generics/           generic tests
  closures/           closure tests
```

---

## ✅ Build Status

```bash
make all && make self && make test
```

```text
✅ Self-hosting converged (v3 ≡ v4 ≡ v5)
✅ Regression: 154/154
✅ Native: 20/20
✅ Generics: 7/7
✅ Closures: 6/6
✅ Version: 0.49.9
```

---

## 📚 Where to Go Next

| Document | Description |
|----------|-------------|
| **[Language Tour.md](Language%20Tour.md)** | Complete ground-up syntax & mechanics guide |
| **[TECHNICAL.md](TECHNICAL.md)** | Deep technical architecture, codegen, memory model |
| **[CHANGELOG.md](CHANGELOG.md)** | Full version history through v0.49.9 |
| **[ROADMAP.md](ROADMAP.md)** | Completed milestones + future plans |
| **[docs/BUILD.md](docs/BUILD.md)** | Build system reference: targets, packaging, install |

---

## 📄 License

<div align="center">

**MIT** — © 2026 PlantLang Project

[![License: MIT](https://img.shields.io/badge/License-MIT-ff69b4.svg?style=for-the-badge)](https://opensource.org/licenses/MIT)

</div>

---

<p align="center">
  <b>🌱 PlantLang. Readable. Fast. Self-Hosting.</b>
  <br>
  <i>"Code that reads like prose, compiled to C, running at native speed."</i>
</p>

---

### 🏷️ Badges

```markdown
[![Pure Native](https://img.shields.io/badge/Pure_Native-🚀-success?style=flat-square)](https://github.com/your/plantlang)
[![Self-Hosting](https://img.shields.io/badge/Self_Hosting-✅-blue?style=flat-square)](https://github.com/your/plantlang)
[![Zero GC](https://img.shields.io/badge/Zero_GC-💚-brightgreen?style=flat-square)](https://github.com/your/plantlang)
[![No VM](https://img.shields.io/badge/No_VM-⚡-orange?style=flat-square)](https://github.com/your/plantlang)
[![License MIT](https://img.shields.io/badge/License-MIT-ff69b4?style=flat-square)](LICENSE)
[![Version](https://img.shields.io/badge/Version-0.49.9-9cf?style=flat-square)](https://github.com/your/plantlang)
[![Tests](https://img.shields.io/badge/Tests-154/154-4CAF50?style=flat-square)](https://github.com/your/plantlang)
[![Self-Hosted](https://img.shields.io/badge/Self_Hosted-🔄-brightgreen?style=flat-square)](https://github.com/your/plantlang)
```