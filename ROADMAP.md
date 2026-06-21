# 🗺️ PlantLang Development Roadmap & Status Report (v0.14.0 Compiler Frontend Core)

This document outlines the architectural milestones, verified features, active technical updates, and upcoming design targets for the PlantLang programming language interpreter and compiler infrastructure (**Chloroplast Core v0.13.0**).

---

## 🟢 1. Features Fully Implemented & Verified (100% Stable v0.13.0 Core)

The following components have been fully integrated into the production runtime environment and validated through native automated regressions within the dual-pipeline Chloroplast engine baseline:

### 🛡️ Formal Compiler Frontend Pipeline (Phase 1 Migration)
* **Pure State-Machine Tokenizer (`core/lexer.js`):** Scans source code character-by-character to generate a safe Token Stream. It embeds strict location metadata `{ type, value, line, column, depth }` and implements smart lookahead mechanics.
* **Recursive-Descent Predictive Parser (`core/parser.js`):** Built-in formal predictive parser utilizing `peek()`, `consume()`, and `match()` design patterns. It constructs a strongly-typed Abstract Syntax Tree (AST) while utilizing a `RawStatementNode` zero-leak fallback for unmigrated structures.
* **Coordinate-Pure AST Schema (`core/ast.js`):** Formalized syntax tree node hierarchy inheriting geographic source coordinates. Successfully migrated statements include: `SHOW`, `CREATE`, `LISTEN BRANCH`, and `RESPONSE`.
* **Additive Interpreter Bridge (`core/interpreter.js`):** Seamless dynamic routing via `evaluateProgram(ast, soil)` and `runSource()` evaluating node execution against the existing active Memory Soil architecture without breaking backward compatibility.

### 🚨 Precision Visual Diagnostics & Caret Locator Engine (`^`)
* **Coordinate-Aware Processing:** Tracks precise sub-token coordinates. The column calculator accounts for leading whitespaces, variable depth matrix prefixes (`N\`), and preserves ancestry across comma-linked multi-line boundaries.
* **Deep-Layer Storm Interception:** Centralized exception handlers wrap expression evaluation `E()` inside `_exec` and provide fallback intercepts inside `_execOne`. Deep, un-tracked runtime anomalies (like mathematical `ZERO_STORM` or state `LOCK_STORM`) are automatically stamped with the exact statement row/column metrics before rendering.
* **Pure Clean Diagnostics Output:** Suppresses raw engine stack traces, rendering a custom terminal error alignment panel instead:
  ```text
  🚨 Atmospheric Storm Panic: MISSING_STORM  --> examples/06_diagnostics.plnt:21:4
  21 \ 1\ SHOW subtotl.
              ^
  Error: "subtotl" is not defined in the current dynamic Memory Soil.
  ```

### 🌐 Web Server Block-Construct Baseline
* **AST-Integrated Web Server Core:** Natively parsed `LISTEN BRANCH` blocks managing concurrent local web server port bindings, isolated incoming request payloads, and sequential evaluation routing layers.

### 🌾 Synchronous Remote Ingestion (`HARVEST` Engine)
* **Deadlock-Free Concurrency:** Utilizes isolated Node.js background Worker Threads linked with atomic OS-level signals (`SharedArrayBuffer` & `Atomics.wait`) to ingestion-stream remote data payloads without stalling the host event loop.
* **Automatic Format Inference:** Unpacks external JSON objects and arrays into strongly-typed native `MAP` structures and sequence `LIST` configurations recursively.

### 🧪 First-Class Native Testing Framework (`SUITE` & `VERIFY`)
* **VERIFY Operational Core:** Integrated native keywords supporting core assertion profiles (relational matches, reduction transformations, storm containments, dynamic instance types, and structural bound counts).
* **Fully Native Suite Infrastructure:** 100% native execution mapping running isolated expressions directly inside PlantLang itself.

### 🔄 Data Weaving & Object Environments (`BRAID` & `SPECIES`)
* **The BRAID Matrix Engine:** Merges discrete structural collections into balanced positional key-value layouts, safely short-circuiting arrays down to match the shortest collection boundary size.
* **The Species Blueprint:** Prototype-free object-oriented hierarchy supporting strict functional parameters, context protection layers via `SELF`, inheritance lines, and instance validation.

### 📊 Current Verification Metrics
* **Total Automated Regressions:** **247 / 247 Assertions Passed (100% Success Rate)** 🟢
  * *New Tokenizer Scanner Sprints:* 24 / 24 Passed
  * *New Parser & AST Statement Migration:* 47 / 47 Passed
  * *Visual Diagnostic Panel & Arrow Alignment Sprints:* 45 / 45 Passed
  * *Legacy Core Regression Suite (`all.plnt`):* 56 / 56 Passed
  * *Legacy VERIFY Architecture Matrix:* 75 / 75 Passed
* **Integration Benchmarks:** All production architecture example files executed flawlessly with clean zero-stack leak parameters across both engines. 🤝

---

## 🟡 2. Technical Debt Squashed & Known Gaps (Active Optimizations)

### ⚡ Technical Debt Resolved in v0.13.0 Sprints
* **The Trailing Period Collision:** Resolved a critical lexer bug where statement-terminating periods were being swallowed by numerical constants. The lookahead now correctly isolates float decimals (e.g., `3.14`) while preserving statement seals.
* **Nested Depth Scope Ingestion:** Fixed block-parsing leaks inside server scopes where deep-nested statements were collapsing into flat blocks. Line-level `DEPTH` markers are now strictly consumed before inner evaluation delegation, eliminating string prefix contamination (e.g., `"2 GIVE..."`).
* **Server Scope Race Conditions:** Enforced strict sequential AST evaluation order within the interpreter routing bridge, guaranteeing that local block-level variables are fully instantiated before terminal `RESPONSE` expressions read them.

### ⚠️ Remaining Gaps for Next Sprints
* **Category: State Isolation**
  * *Component / Feature Gaps:* Multi-threaded Dynamic Soil Sandboxing.
  * *Current Status & Target Scope:* Restructuring deep `Soil.clone()` behaviors to ensure incoming HTTP thread pools maintain completely isolated, non-leaking concurrent memory allocation lifetimes.
* **Category: Syntax Coverage**
  * *Component / Feature Gaps:* Unmigrated Statement Blocks.
  * *Current Status & Target Scope:* Gradually refactoring complex remaining structures (`WEATHER`, `SHELTER`, `BRAID`) out of the legacy flat regex matching engine and converting them into formal AST nodes.

---

## 🔵 3. Complete Future Feature Roadmap (The Next Horizons)

Strategic milestones planned to transition PlantLang into a fully compiler-driven, secure, high-concurrency software ecosystem:

### Phase 1: Complete AST Node Adoption & Static Analysis (Priority 1)
* **Full Syntax Tree Migration:** Systematically migrate all remaining structural keywords out of the legacy regex fallback loop, achieving a 100% AST-driven compiler frontend.
* **Compile-Time Static Linter:** Introduce a static analysis subsystem using the token stream to detect unreferenced variables, structural depth violations, and type mismatches before spinning up the execution floor.
* **`STREAM` Connectivity:** Permanent full-duplex communication pipelines implementing native WebSocket architectures for live real-time interaction layers.

### Phase 2: Persistence, Dependency Hub & Security (Priority 2)
* **`SOW` & `FIND_IN` Storage Adapters:** Abstract data mapper contracts and persistence modules serving as query interfaces for major database infrastructures.
* **`GREENHOUSE` Package Hub:** Unified package management system supervising atomic publishing workflows, semantic version tracking, and explicit structural imports.
* **`SHIELD` Resource Control:** Access control security vectors enforcing compile-time permission validation for host filesystems, networks, or process actions.

### Phase 3: Compilers & Transpilation Target Hub (Priority 3)
* **The Mission Transpiler:** Blueprint targets optimized to convert `.plnt` AST structures into high-performance native source formats matching specialized operational priorities:
  * `MISSION:FAST` → Compiled Go structures.
  * `MISSION:SAFE` → Memory-safe Rust applications.
  * `MISSION:SMART` → Lean Python execution workflows.

### Phase 4: Native Arabic Naming & Localization (Priority 4)
* **Arabic Native Localization:** Modifying identifier regular expressions within tokenizing matrices to natively parse Arabic alphabet characters for dynamic variable tags, functional behaviors, and structural scopes.

---

## 🎯 4. Immediate Strategic Priority Matrix (Sprints for v0.14.0)

Our next upcoming engineering iterations will focus strictly on the following tactical milestones:

1. **AST Structural Migration (Error Safety Focus):** Migrating `WEATHER`, `SHELTER`, and `CALM` blocks into the formal parser to achieve deep-layer native exception handling within the syntax tree.
2. **Concurrent Soil Duplication Verification:** Stress-testing concurrent HTTP requests against cloned Soil states to uncover and patch memory leaks under high artificial loads.
3. **Linter Proof-of-Concept:** Creating a lightweight CLI check subcommand (`chloroplast check`) powered exclusively by the new `core/parser.js` to flag dangling syntax warnings.

---
*Developed by Haider Mohammed Abd Alwahid — Nasiriyah, Iraq (2026).*