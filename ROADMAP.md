🗺️ PlantLang Development Roadmap & Status Report (v0.11.0 Future Phase)

This document outlines the architectural milestones, verified features, active technical updates, and upcoming design targets for the PlantLang programming language interpreter (Chloroplast Core v0.11.0-Future).

---

🟩 1. Features Fully Implemented & Verified (100% Stable Core)

The following components have been fully integrated into the Node.js production runtime environment and validated through native automated regressions within the Chloroplast v1.0.0 engine baseline:

🧱 First-Class Native Testing Framework (Finalized)
* VERIFY Semantic Engine: Native test automation grammar executing 5 assertion behaviors directly inside the source engine:
  * Expression Verification (expr IS expected): Evaluates relational operations and boundary loops.
  * Pipeline Harvesting (FROM action, args GIVES expected): Captures functional output transformations.
  * Storm Interception (STORMS STORM_TYPE FROM expr): Confirms absolute crash containment.
  * Dynamic Type Assertion (TYPE var IS TYPE_NAME): Verifies dynamic model instances.
  * Capacity Bounds (COUNT cluster IS n): Tracks allocation sizing for LIST and MAP.
* SUITE Layout Isolation: Contextual grouping structures (SUITE / SUITE/.) to cleanly wrap, process, and isolate multi-tiered test executions.
* 100% Native Migration Baseline: Fully deprecated legacy JS testing setups; all 63 core unit tests are migrated directly into tests/suite.plnt using pure PlantLang logic.

🔀 Advanced Data Weaving & Mapping (The BRAID Engine)
* Structural Array Weaving: Merges separate runtime data clusters into clean positional element pairings natively via the BRAID keyword.
* Dynamic Mapping Compression: Zips matching keys and properties into a strongly-typed native MAP via 'BRAID keys WITH vals AS config MAP.' blocks.
* Bounds Safety Trimming: Short-circuits matching loop sets to gracefully clip allocations tightly down to the shortest target collection size.

⚙️ Core Runtime & Type Alignments
* Advanced Lexer & Parser: Full depth-based block architecture handling multi-line strings (\n), statement sealing (.), sequence continuation (,), and indentation block bounds (\n.).
* The Soil Engine: Lexical environment managing cross-context variable resolution chains.
* The Plant Typing Lattice: Strict data enforcement for NUM, SCL, TX, FACT, LIST, MAP, INSTANCE, and VEIN.
* Advanced Reducer & Clock Systems: Functional ANALYZE summaries across compound variables, explicit TYPEOF reflection, synchronized clock loops (NOW), and safe interlocking delays (WAIT) using native Atomics.wait.
* ROOT_SCOPE Global Guard: Protected configuration matrix accessed via CONFIG:"key" strings, raising immediate LOCK_STORM anomalies upon unauthorized write attempts.
* Quoted Key Evaluations (obj:"key"): Extended expression evaluators (evalExpr), SHOW, and SET syntax rules to seamlessly support quoted string literals as identifiers inside MAP collections.

🧬 Objects, Functional Ingestion & Pipelines
* The Species Paradigm (SPECIES / BLOOM / PARENT): Full object-oriented hierarchy, member inheritance, method resolution order (MRO), and dynamic mutation updates using SELF.
* Recursive Functional Pipelines: Advanced routing maps handling parameters through ACTION/GIVE/TRACE call stacks seamlessly, alongside integrated list operators (SORT, REVERSE, UNIQUE, FLATTEN) within FLOW tracks.

🧪 Current Verification Metrics
* Native Test Automation (tests/suite.plnt): 63 / 63 Passed (100% Success Rate) ✅
* Legacy Evaluation Tests: 56 / 56 Passed (Arabic Compatibility Baseline) ✅
* Integration Benchmarks: All 5 / 5 Production Architecture Examples executed flawlessly without regressions or execution leaks. ✅

---

🟨 2. Technical Debt & Known Gaps (v0.11.0 Optimization Sprints)

These remaining items focus strictly on optimizing developer experience and refining deep execution safety:

* Category: Error Handling
  * Component / Feature Gaps: Visual Error Pointers (^)
  * Current Status & Target Scope: Swapping raw stack traces for row/column line offset trace diagnostics to pinpoint compile-time and runtime exceptions.

* Category: Deep Edge Cases
  * Component / Feature Gaps: Multi-nested WEATHER Recovery
  * Current Status & Target Scope: Needs extensive automated test variations to verify multi-tier catch behaviors within the 12-Storm Exception Matrix.

---

🟥 3. Complete Future Feature Roadmap (The v0.11.0 Horizon)

Strategic milestones planned to transition PlantLang from a localized tool into a network-aware, secure runtime ecosystem:

Phase 1: Web & Networking Protocol Core (Priority 1)
* TRACE Client API: Integrating native asynchronous HTTP/HTTPS client structures to safely fetch external network data feeds and interface with web endpoints directly into the local Soil.
* LISTEN BRANCH Web Server Engine: A lightweight, high-performance concurrency block to spawn native HTTP servers and handle routing layers directly from within a script file.
* STREAM Connectivity: Permanent duplex channel web pipelines managing ongoing WebSocket architectures for live systems messaging.

Phase 2: Persistence, Dependency Hub & Security (Priority 2)
* SOW & FIND_IN Storage Adapters: Abstract data mapper layouts and persistence wrappers serving as generic query interfaces for database infrastructures.
* GREENHOUSE Dependency Hub: Centralized system package manager supervising package publishing, semantic versioning, and external file imports.
* SHIELD Capabilities Security: Secure compile-time resource access matrices regulating filesystem, networking, or host process permissions.

Phase 3: Native Compilers & Transpilation Layers (Priority 3)
* The Mission Transpiler Target Hub: Building out compilation blueprint pipelines to generate standalone files from .plnt sources based on specialized runtime flags:
  * MISSION:FAST → High-performance Go source modules.
  * MISSION:SAFE → Memory-safe Rust codebases.
  * MISSION:SMART → Scripted Python structures.
* Terminal UI Enhancements: Upgrading the standard REPL shell loop with interactive syntax highlighting, context autocompletion, and visual execution trace views.

Phase 4: Native Arabic Naming & Localization (Priority 4)
* Arabic Native Localization: Modifying internal identifier regular expressions within the Lexer token matrices to accept complete Arabic alphabet characters for variables, actions, and method naming frames natively.

---

🎯 4. Immediate Strategic Priority Matrix (Sprints for v0.11.0)

Our next upcoming engineering iterations will focus strictly on the following tactical completions:

1. The Asynchronous Network Core: Finalizing the fetch mechanics for the TRACE Client API, ensuring response streams map smoothly back into native MAP or TX elements inside the current Soil.
2. Advanced Visual Errors: Building context-rich, pointer-based terminal logs (^) indicating precise row and column positioning whenever a compilation or runtime Storm panics.
3. Multi-tier Exception Regressions: Expanding the automated suite to thoroughly cover nested exception handling boundaries before moving into network server architectures.
