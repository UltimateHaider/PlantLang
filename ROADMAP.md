# 🗺️ PlantLang Development Roadmap & Status Report (v0.18.0 Milestone)

This document outlines the architectural evolution of PlantLang, tracking the transition from the Chloroplast v0.17.0 Interpreter Core into the high-performance Native Compiler infrastructure.

---

## 🟢 1. Milestones Achieved (v0.17.0 Baseline)
The core infrastructure has reached production-grade stability:
* **Pure AST Pipeline:** Tokenizer ➔ Parser ➔ AST ➔ SymbolPass ➔ evaluateNode.
* **Live Network:** `LISTEN BRANCH` native HTTP engine with isolated Soil sandboxing.
* **Developer Experience:** VSCode Extension (Syntax highlighting, Theme, 30+ Snippets).
* **Validation:** 313/313 tests passed (100% Zero-Regression).

---

## 🔵 2. The Native Compiler Evolution (v0.18.0 Targets)
We are moving beyond the interpreter floor to enable bare-metal performance through a multi-pass compilation pipeline.

### Phase 1: Structural Static Analysis (The Validator)
* **Static Type Checker:** Implementing a type-inference pass over the AST.
    * Enforce strict typing for `NUM`, `TX`, `MAP`.
    * Pre-execution validation of variable declarations and scope integrity.
* **Symbol Table Finalization:** Mapping all identifiers to strict memory locations before binary generation.

### Phase 2: Transpilation (The Generator)
* **AST-to-C Transpiler:** * Migrating AST nodes to C-compliant syntax trees.
    * Handling memory allocation and garbage collection logic for PlantLang objects.
* **Native Binary Integration:** * Invoking GCC/Clang via CLI.
    * Generating standalone executables (`.bin` / `.exe`) from `.plnt` source code.

### Phase 3: Compiler Service & Web UI
* **CodeWords Compiler Service:** A backend-as-a-service (BaaS) for remote compilation of PlantLang scripts.
* **Web REPL UI:** A browser-based IDE offering real-time visualization of the AST and transpilation results.

---

## ⚠️ 3. Known Constraints & Technical Debt
* **Remaining Legacy Fallback:** While ~96% of the pipeline is native AST, ~4% of edge-case statements still utilize the `RawStatementNode` fallback.
* **Boolean Evaluation:** Refinement required for `FACT` constants to ensure strict boolean parity with system-level evaluations.
* **Library Extensibility:** Current `innate` library needs modular migration for the upcoming `GREENHOUSE` package manager.

---

## 🎯 4. Strategic Priority Matrix (v0.18.0 Sprints)
1. **Static Type Checker:** Establish the validation layer (Priority: Extreme).
2. **C Code Generator:** Establish the structural mapping between AST and C (Priority: High).
3. **GREENHOUSE Integration:** Formalizing the import/package system (Priority: Medium).

---

## 🧪 Verification Matrix
| Component | Status | Target |
| :--- | :--- | :--- |
| AST Interpreter | Stable ✅ | v0.17.0 |
| Static Type Checker | In-Progress 🛠️ | v0.18.0 |
| C Transpiler | Research 🔍 | v0.18.0 |
| Native Binary | Roadmap 🔜 | v0.19.0 |

*Developed by Haider Mohammed Al-Khuzai — Nasiriyah, Iraq (2026).*

---

## 🟢 1. Features Fully Implemented & Verified (100% Stable v0.15.0-core)

The following components have been fully integrated into the production runtime environment and validated through a consolidated suite of automated regressions, marking the transition into a true two-pass compiler frontend pipeline.

### 🛡️ Formal Compiler Frontend & Core Block Migration (Phase 1 Completed)
* **Pure State-Machine Tokenizer (`core/lexer.js`):** Scans source code character-by-character to generate a safe Token Stream. It embeds strict location metadata `{ type, value, line, column, depth }` and implements smart lookahead mechanics.
* **Recursive-Descent Predictive Parser (`core/parser.js`):** Fully migrated core high-level constructs out of the legacy text loop into formal typed AST nodes, including: `SHOW`, `CREATE`, `LISTEN BRANCH / RESPONSE`, `WEATHER / SHELTER / CALM`, `ACTION` (Declarations), `SPECIES` (Classes), `BLOOM` (Instantiation), `TAP` (I/O), and `WHENEVER` (Reactive Data Watchers).
* **Backward-Compatible Conditional `WEATHER`:** Refactored error-handling block grammar to support both traditional unconditional `,WEATHER` blocks and the newly introduced conditional `WEATHER IF [condition]` block structure without breaking existing test scripts.
* **Dynamic Symbol Table Pass (`symbolPass`):** Fully eliminated the legacy flat text pre-scanning hack (`_firstPass`). The engine now implements a compiler-grade two-pass design: `symbolPass(programNode)` recursively traverses the AST to pre-register top-level declarations (Constants, Actions, Species) before sequential execution begins, natively unlocking forward-referencing (hoisting).
* **Polymorphic Action Execution (`_callAction`):** Upgraded the functional execution bridge to be completely polymorphic—it seamlessly detects and routes both legacy string records (`.text`) and typed AST node bodies (`.type`) without altering downstream execution sites.

### 🚨 Precision Visual Diagnostics & Caret Locator Engine (`^`)
* **Coordinate-Aware Processing:** Tracks precise sub-token coordinates. The column calculator accounts for leading whitespaces, variable depth matrix prefixes (`N\`), and preserves ancestry across comma-linked multi-line boundaries.
* **Uncaught Storm Coordinate Backfills:** Centralized exception handlers wrap deep, un-tracked runtime anomalies (like mathematical `ZERO_STORM` or state `LOCK_STORM`) inside AST-routed blocks, automatically backfilling row/column metrics to ensure visual carets (`^`) point directly to the exact faulting line.

### 🌐 Legacy Core Floor (100% Stable Fallback)
* All high-level features remain fully functional via the robust regex-statement matching engine fallback (`RawStatementNode` -> `_execOne`), including synchronous remote ingestion (`HARVEST`), object-oriented lineage validation (`IS_A`), data weaving (`BRAID`), and file persistence (`ABSORB / INFUSE / SEAL`).

### 📊 Current Verification Metrics
* **Total Automated Regressions:** **313 / 313 Assertions Passed (100% Success Rate)** 🟢
  * *New Tokenizer Scanner Sprints:* 33 / 33 Passed
  * *New Parser & AST Core Block Migration:* 104 / 104 Passed
  * *Visual Diagnostic Panel & Arrow Alignment Sprints:* 45 / 45 Passed
  * *Legacy Core Regression Suite (`all.plnt`):* 56 / 56 Passed
  * *Legacy VERIFY Architecture Matrix:* 75 / 75 Passed
* **Status:** Zero-regression dual-pipeline verified end-to-end from a clean environment ZIP extraction.

---

## 🟡 2. Technical Debt Squashed & Known Gaps (Active Optimizations)

### ⚡ Technical Debt Resolved in recent Sprints
* **The `REAP` Argument Spacing Collision:** Fixed a token-reconstruction bug where the fallback statement-joining mechanism forced whitespace around list commas (`add , 3 , 4`), corrupting legacy regex matchers. Replaced raw joins with a contextual `joinTokens()` utility.
* **Keyword Identifier Overlaps:** Resolved a parser panic inside class declarations where standard field identifiers like `count`, `name`, or `step` collided with the lexer's strict reserved `KEYWORDS` matrix. The field-name parser now accepts both `IDENT` and `KEYWORD` tokens.
* **The Tokenized Colon Spacing Bug:** Fixed an evaluation failure in object method calls where the join utility injected a trailing space after object-property boundaries (`c1: tick`). Enforced tight left-and-right constraints on colons (`c1:tick`).
* **Double Declaration Suppression:** Introduced a `_symbolPassDone` suppression system to prevent duplicate functional registrations and noisy outputs during the secondary execution pass.

### ⚠️ Active Bugs & Structural Gaps (Current Sprints)

#### 1. Known Runtime Bugs (Observed)
* **`FACT` Logic Break:** `CREATE x(FACT) TO FACT:TRUE.` returns `null` instead of a boolean representation because `FACT:TRUE` fails to hit the correct lexer handler.
* **`lists:UNIQUE` Multi-Arg Crash:** The library function fails when supplied with multiple arguments, as it strictly expects a single unified `LIST` type.
* **`strings:INCLUDES` Type Mismatch:** Returns a native boolean, whereas the internal `VERIFY` framework expects a literal `"true"` string representation.
* **`CYCLE x IN lst` Boundary Failures:** Iteration loops fail to resolve correctly over specific numeric literal range syntax patterns.

#### 2. AST Conversion Coverage Gap
While the core pipeline is fully established, **~75% of actual runtime statements still fall back to `RawStatementNode` -> `_execOne`**. Only ~25% of absolute statement variants are natively processed as typed AST nodes. 

---

## 🔵 3. Complete Future Feature Roadmap (The Next Horizons)

Strategic milestones planned to transition PlantLang into a fully compiler-driven, secure, high-concurrency software ecosystem:

### Phase 1: Complete AST Statement Adoption & Static Analysis (Priority 1)
* **High-Priority AST Migrations (The Remaining 75% Raw Traffic):**
  * **`REAP` Statements:** The absolute highest priority item (94 occurrences in the test corpus). Migrating this will shift the pipeline majority to native AST.
  * **Core Inline Operations:** `GIVE` (plain return statements), `SET`, `INCREASE`, `DECREASE`.
  * **Control Flow Subsystems:** `IF / ORIF / ELSE`, `MATCH / YIELD`, `CYCLE`, `SEASON`.
  * **Collection Mutators:** `LINK`, `PUT`, `TAKE`.
* **Medium-Priority AST Migrations:**
  * Innate functional library invocations (`REAP x FROM library:FUNCTION`).
  * Core commands (`ANALYZE`, `WAIT`, `SHOW_VERIFY_SUMMARY`).
  * Multi-line constructs (`ROOT_SCOPE` blocks, file I/O `ABSORB / INFUSE / SEAL`).
* **Static Type Checker & Linter:** Introduce a compile-time validation floor to intercept type mismatches, unreferenced tokens, and scope violations before launching the execution interpreter.

### Phase 2: Persistence, Dependency Hub & Security (Priority 2)
* **`SOW` & `FIND_IN` Storage Adapters:** Abstract data mapper contracts and persistence modules serving as query interfaces for major database infrastructures.
* **`GREENHOUSE` Package Hub & Real File Imports:** Unified package management system supervising atomic publishing workflows and supporting explicit file inclusions via an actual `IMPORT` keyword (replacing purely local `PLANT` definitions).
* **`SHIELD` Resource Control:** Access control security vectors enforcing compile-time permission validation for host filesystems, networks, or process actions.

### Phase 3: Native Arabic Naming & Localization (Priority 3)
* **Arabic Native Localization:** Modifying identifier regular expressions within tokenizing matrices to natively parse Arabic alphabet characters for dynamic variable tags, functional behaviors, and structural scopes—enabling local engineering practices.

### Phase 4: Compilers & Transpilation Target Hub (Priority 4)
* **The Mission Transpiler:** Blueprint targets optimized to convert `.plnt` AST structures into high-performance native source formats matching specialized operational priorities:
  * `MISSION:FAST` → Compiled Go structures.
  * `MISSION:SAFE` → Memory-safe Rust applications.
  * `MISSION:SMART` → Lean Python execution workflows.

---

## 🎯 4. Immediate Strategic Priority Matrix (Sprints for v0.16.0-core)

Our upcoming engineering iterations for **v0.16.0-core** will focus strictly on the following tactical milestones:

1. **The REAP Core Migration:** Build a formal typed `ReapStatementNode` and accompanying parser method to clean up 94 legacy call sites, unlocking the majority AST pipeline milestone.
2. **Squash the `FACT` Boolean Bug:** Refactor the lexer matrix to correctly ingest and evaluate boolean constants without defaulting to `null`.
3. **Migrate Basic Assignments:** Port `SET`, `INCREASE`, and `DECREASE` out of the text-fallback pool into formal AST node execution paths.

---
*Developed by Haider Mohammed Abd Alwahid — Nasiriyah, Iraq (2026).*

```