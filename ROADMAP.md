# 🗺️ Pantlang Development Roadmap & Status Report (v0.10.0-Ecosystem Phase)

This document outlines the architectural milestones, verified features, active technical updates, and upcoming design targets for the Pantlang programming language interpreter (**Chloroplast Core v0.10.0**).

---

## 🟩 1. Features Fully Implemented & Verified (100% Stable)

The following components have been fully integrated into the Node.js production runtime environment and validated through native automated regressions:

### 🧱 First-Class Native Testing Framework (New in v0.9.0)
* **`VERIFY` Semantic Engine:** Native test automation grammar executing 5 assertion behaviors directly inside the source engine:
  1. **Expression Validation (`expr IS expected`):** Evaluates relational operations and boundary loops.
  2. **Pipeline Harvesting (`FROM action, args GIVES expected`):** Captures functional output transformations.
  3. **Storm Interception (`STORMS STORM_TYPE FROM expr`):** Confirms absolute crash containment.
  4. **Dynamic Type Assertion (`TYPE var IS TYPE_NAME`):** Verifies dynamic model instances.
  5. **Capacity Bounds (`COUNT cluster IS n`):** Tracks allocation sizing for `LIST` and `MAP`.
* **`SUITE` Layout Isolation:** Contextual grouping structures (`SUITE` / `SUITE/.`) to cleanly wrap, process, and isolate multi-tiered test executions.
* **100% Native Migration Baseline:** Fully deprecated legacy JS testing setups; all **56 core test specs** are migrated directly into `tests/suite.plnt` using pure Pantlang logic.

### ⚙️ Core Runtime & Type Alignments
* **Advanced Lexer & Parser:** Full depth-based block architecture handling multi-line strings (`\n`), statement sealing (`.`), sequence continuation (`,`), and indentation block bounds (`\n.`).
* **The Soil Engine:** Lexical environment managing cross-context variable resolution chains.
* **The Plant Typing Lattice:** Strict data enforcement for `NUM`, `SCL`, `TX`, `FACT`, `LIST`, `MAP`, `INSTANCE`, and `VEIN`.
* **Advanced Reducer & Clock Systems:** Functional `ANALYZE` summaries across compound variables, explicit `TYPEOF` reflection, synchronized clock loops (`NOW`), and safe interlocking delays (`WAIT`) using native `Atomics.wait`.
* **`ROOT_SCOPE` Global Guard:** Protected configuration matrix accessed via `CONFIG:"key"` strings, raising immediate `LOCK_STORM` anomalies upon unauthorized write attempts.

### 🧬 Objects, Functional Ingestion & Pipelines
* **The Species Paradigm (`SPECIES` / `BLOOM` / `PARENT`):** Full object-oriented hierarchy, member inheritance, method resolution order (MRO), and dynamic mutation updates using `SELF`.
* **Recursive Functional Pipelines:** Advanced routing maps handling parameters through `ACTION`/`GIVE`/`REAP` call stacks seamlessly, alongside integrated list operators (`SORT`, `REVERSE`, `UNIQUE`, `FLATTEN`) within **`FLOW`** tracks.

### 🧪 Current Verification Metrics
* **Native Test Automation (`tests/suite.plnt`):** `56 / 56 Passed` (100% Success Rate) ✅
* **Integration Benchmarks:** All `5 / 5 Production Architecture Examples` executed without regressions. ✅

---

## 🟨 2. Technical Debt & Known Gaps (Ecosystem Bridge Tasks)

These remaining gaps focus strictly on finalizing legacy specs and optimizing internal visual tracking logs:

| Category | Component / Feature Gaps | Current Status & Target Scope |
| :--- | :--- | :--- |
| **Specification Gaps** | `BRAID` Concurrency Combinator | Needs parsing engine adjustments to mesh asynchronous execution branches together. |
| **Error Handling** | Visual Error Pointers (`^`) | Swapping raw stack traces for row/column line offset trace diagnostics. |
| **Deep Edge Cases** | Multi-nested `WEATHER` Recovery | Needs extensive automated test variations to verify multi-tier catch behaviors. |

---

## 🟥 3. Complete Future Feature Roadmap (The v0.10.0 Horizon)

Strategic milestones planned to transition Pantlang from a localized tool into a network-aware, secure runtime environment:

### Phase 1: Web & Networking Protocol Core (Priority 1)
* **`HARVEST` Client Architecture:** Integrating native asynchronous HTTP/HTTPS client structures to safely fetch data feeds and interface with web endpoints.
* **`LISTEN BRANCH` Web Server Engine:** A lightweight, high-performance concurrency block to spawn native HTTP servers directly from within a script file.
* **`STREAM` Connectivity:** WebSockets architecture layer managing ongoing duplex channels for live systems messaging.

### Phase 2: Native Compilers & Transpilation Layers (Priority 2)
* **The Mission Transpiler Target Hub:** Building out the compilation blueprint pipelines to generate standalone files from `.plnt` sources:
  * `MISSION:FAST` $\rightarrow$ Go source modules.
  * `MISSION:SAFE` $\rightarrow$ Rust codebases.
  * `MISSION:SMART` $\rightarrow$ Scripted Python structures.
* **Terminal UI Enhancements:** Upgrading the standard REPL shell loop with interactive syntax highlighting, context autocompletion, and visual trace views.

### Phase 3: Package Distribution & Native Arabic Naming
* **`SOW` & `FIND_IN` Storage Adapters:** Abstract persistence wrappers serving as generic query interfaces for database infrastructures.
* **`GREENHOUSE` Dependency Hub:** Centralized system package manager supervising package publishing and semantic imports.
* **`SHIELD` Capabilities Security:** Secure compile-time resource access matrices regulating filesystem, networking, or host process permissions.
* **Arabic Native Localization:** Adjusting the internal identifier regular expressions within the Lexer to accept full Arabic alphabet strings for variables, actions, and method naming frames natively.

---

## 🎯 4. Immediate Strategic Priority Matrix (Sprints for v0.10.0)

Our next upcoming engineering iterations will focus strictly on the following tactical completions:

1. **The `BRAID` Combinator Specification:** Implementing the final missing language manual keyword into `core/interpreter.js` to enable advanced multi-stream code weaving.
2. **The `HARVEST` Asynchronous Client Engine:** Designing the network fetch mechanics, ensuring response streams map smoothly back into native `MAP` or `TX` elements inside the current Soil.
3. **Advanced Visual Errors:** Building context-rich, pointer-based terminal logs (`^`) indicating precise row and column positioning whenever a compilation or runtime `Storm` panics.
