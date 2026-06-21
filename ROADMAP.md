# 🗺️ PlantLang Development Roadmap & Status Report (v0.13.0 Planned Phase)

This document outlines the architectural milestones, verified features, active technical updates, and upcoming design targets for the PlantLang programming language interpreter (**Chloroplast Core v0.13.0-Planned**).

---

## 🟩 1. Features Fully Implemented & Verified (100% Stable v0.12.0 Core)

The following components have been fully integrated into the Node.js production runtime environment and validated through native automated regressions within the Chloroplast v0.12.0 engine baseline:

### ⛈️ Precision Visual Diagnostics & Caret Locator Engine (`^`)
* **Coordinate-Aware Lexer:** Upgraded `core/lexer.js` to accurately track `{depth, text, line, column}` parameters for every statement. The column calculation natively handles dynamic whitespace expansions, tab structures, and variable-length depth matrix prefixes (`N\`).
* **Multiline Anchor Preservation:** Maintained structural location mapping across multi-line sequence continuation scopes linked via commas (`,`).
* **Deep-Layer Storm Interception:** Refactored over 47 standalone internal `storm()` panic execution nodes. Implemented dual centralized interceptors wrapping the expression evaluator `E()` inside `_exec` and adding an automatic fallback catch matrix inside `_execOne`. Any position-less deep exceptions (like mathematical `ZERO_STORM` or transactional runtime `LOCK_STORM`) are automatically stamped with the current statement's row/column bounds before rendering.
* **Pure Clean Diagnostics Output:** Suppresses messy, raw engine stack traces, rendering a beautiful, custom terminal error alignment panel instead:
  ```text
  ⛈️  Atmospheric Storm Panic: MISSING_STORM  --> examples/06_diagnostics.plnt:21:4
  21 \ 1\ SHOW subtotl.
              ^
  Error: "subtotl" is not defined in the current dynamic Memory Soil.
  ```

### 🌾 Synchronous Remote Ingestion (`HARVEST` Engine)
* **Deadlock-Free Concurrency:** Utilizes isolated Node.js background Worker Threads linked with atomic OS-level signals (`SharedArrayBuffer` & `Atomics.wait`) to ingestion-stream remote data payloads without stalling the active runtime loop.
* **Automatic Format Inference:** Unpacks external JSON objects and arrays into strongly-typed native `MAP` structures and sequence `LIST` configurations recursively.

### 🧪 First-Class Native Testing Framework (`SUITE` & `VERIFY`)
* **VERIFY Operational Core:** Integrated native keywords supporting 5 core assertion profiles (relational matches, reduction transformations, storm containments, dynamic instance types, and structural bound counts).
* **Fully Native Suite Infrastructure:** 100% native execution mapping. Legacy test frames are completely deprecated; all core assertions run inside the language itself.

### 🔄 Data Weaving & Object Environments (`BRAID` & `SPECIES`)
* **The BRAID Matrix Engine:** Merges discrete structural collections into balanced positional key-value layouts, safely short-circuiting arrays down to match the shortest collection boundary size.
* **The Species Blueprint:** Prototype-free object-oriented hierarchy supporting strict functional parameters, context protection layers via `SELF`, inheritance lines, and instance validation.

### 📊 Current Verification Metrics
* **Total Automated Regressions:** **146 / 146 Assertions Passed (100% Success Rate)** ✅
  * *Visual Diagnostic Panel & Arrow Alignment Sprints:* 20 / 20 Passed
  * *Native Core Testing Matrix (`VERIFY`):* 70 / 70 Passed
  * *Legacy Core Evaluation Tests (Arabic Base Compatibility):* 56 / 56 Passed
* **Integration Benchmarks:** 6 / 6 Production Architecture Examples executed flawlessly with absolute clean zero-stack leak parameters. ✅

---

## 🟨 2. Technical Debt & Known Gaps (v0.13.0 Optimization Sprints)

These remaining maintenance items focus strictly on hardening memory isolation parameters and preparing the environment for concurrent server routing frameworks:

* **Category: State Isolation**
  * *Component / Feature Gaps:* Multi-threaded Dynamic Soil Sandboxing
  * *Current Status & Target Scope:* Restructuring deep `Soil.clone()` behaviors to ensure incoming web connection requests spawn safely inside non-leaking concurrent thread execution scopes.
* **Category: Edge Case Verification**
  * *Component / Feature Gaps:* Multi-nested `WEATHER` Bounds
  * *Current Status & Target Scope:* Expanding automated suite scenarios to track multiple stacked fallback recoveries inside concurrent processing pipelines.

---

## 🟦 3. Complete Future Feature Roadmap (The v0.13.0 Horizon)

Strategic milestones planned to transition PlantLang from a standalone desktop interpreter into a highly secure, high-concurrency web-server ecosystem:

### Phase 1: Web Server Protocol Engine (Priority 1)
* **`LISTEN BRANCH` Web Server Core:** Introducing lightweight, high-performance runtime blocks to instantiate concurrent native HTTP servers and map routing branches cleanly directly from inside a script file.
* **`STREAM` Connectivity:** Permanent full-duplex communication pipeline matrices implementing native WebSocket architectures for live interactive messaging layers.

### Phase 2: Persistence, Dependency Hub & Security (Priority 2)
* **`SOW` & `FIND_IN` Storage Adapters:** Abstract data mapper contracts and persistence modules serving as query interfaces for major database infrastructures.
* **`GREENHOUSE` Package Hub:** Unified package management system supervising atomic publishing workflows, semantic version tracking, and explicit structural imports.
* **`SHIELD` Resource Control:** Access control security vectors enforcing compile-time permission validation for host filesystems, networks, or process actions.

### Phase 3: Compilers & Transpilation Target Hub (Priority 3)
* **The Mission Transpiler:** Blueprint targets optimized to convert `.plnt` files into high-performance source formats matching specialized operational priorities:
  * `MISSION:FAST` → Compiled Go structures.
  * `MISSION:SAFE` → Memory-safe Rust applications.
  * `MISSION:SMART` → Lean Python execution workflows.

### Phase 4: Native Arabic Naming & Localization (Priority 4)
* **Arabic Native Localization:** Modifying identifier regular expressions inside tokenizing matrices to natively parse Arabic alphabet characters for dynamic variable tags, functional behaviors, and structural scopes.

---

## 🎯 4. Immediate Strategic Priority Matrix (Sprints for v0.13.0)

Our next upcoming engineering iterations will focus strictly on the following tactical milestones:

1. **The Sprouting Server Blueprint (`LISTEN BRANCH`):** Designing syntax rules, core routing maps, and basic request interceptors to spawn a stable local listening port.
2. **Concurrent Request Lifecycle Tracking:** Ensuring incoming HTTP payloads are automatically ingested as native `MAP` payloads and handled cleanly without memory race conditions.
3. **Diagnostic Extended Printing:** Integrating the new `core/diagnostics.js` engine into web-layer exceptions to preserve pinpoint error lines during live server crashes.

---
*Developed by Haider Mohammed Abd Alwahid — Nasiriyah, Iraq (2026).*
```