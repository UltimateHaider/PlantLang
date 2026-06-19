# 🌱 PlantLang (Chloroplast Complete Engine — v1.0.0-Core)

PlantLang is an innovative, high-level, structural programming language designed around human-like readability and structural simplicity. By utilizing an ecosystem-inspired vocabulary (such as Soil, Storms, Sprouting, and Reaping), PlantLang bridges the gap between natural language logic and robust computing architecture.

The project features a highly capable, interactive Web Simulator & Core AST Interpreter that fully demonstrates the language's syntax parsing, runtime memory analysis, concurrent behaviors, strict scoping, and reactive state management.

---

## ✨ The Core Philosophy

PlantLang is built upon three foundational design pillars:

* Code Depth (No Braces): Every block of code begins with an explicit depth number (e.g., 1. for primary execution, 2. for sub-operations). This completely eliminates nested brackets {} or strict tab-indentations.
* Natural Punctuation: Statements resemble human literature. An operation is fully sealed using a period ., while continuous structural statements can be extended onto subsequent lines using a comma ,.
* Pure Human Readability: Syntactic commands read naturally as standard English instructions (e.g., CREATE score(NUM) TO 100.).

---

## 🚀 Key Features Implemented (Simulator & Interpreter Core)

The foundational language core is now officially 100% Complete. The active Chloroplast v1.0.0 engine manages complex execution scopes including:

* Native Test Automation (SUITE / VERIFY): Built-in first-class assertion frame supporting isolated expression checks, action harvesting, and native exception tracking written entirely inside PlantLang itself.
* Data Intertwining (BRAID): Native capability to align multiple parallel collections into paired blocks or zip key/value streams directly into dynamic MAP layouts.
* Object-Oriented Ecosystem (SPECIES / PARENT / BLOOM): Supports true class structures, method overriding, absolute dynamic state isolation, and inheritance lineage validation via IS_A.
* Advanced Functional Scopes (ACTION / GIVE / TRACE): Explicit typing parameter passing, deep functional recursion capabilities, and compound inline reduction harvesting (FIRST, LAST, COUNT, SUM, MAX, MIN).
* Simulated Concurrency (SPROUT / SYNC / RACE): Native multi-threaded block queuing where you can spawn async tasks, synchronize multiple workers, or capture the fastest resolving thread.
* Reactive Data Binding (PULSE / WHENEVER): Observable variables that trigger deeply nested responsive handlers automatically upon every mutation (Fully verified alongside nested MATCH blocks).
* Virtual I/O Streaming (TAP / ABSORB / INFUSE / SEAL): A mock file-system data layout to open paths, stream lines, inject content bytes, and safely close buffers.
* The 12-Storm Exception Matrix: 12 strongly-typed systemic exceptions caught natively via localized WEATHER / SHELTER / CALM safety shelters.
* The PLANT Standard Library: Immediate structural extensions: PLANT math (11 functions), PLANT strings (10 functions), and PLANT lists (9 algorithms).

---

## 🛠️ Quick Syntax Reference

| Command Pattern | Operation Type | Meaning / Purpose |
| :--- | :--- | :--- |
| CREATE x(TYPE) TO val | Memory | Instantiate a strongly-typed variable in the current Soil. |
| ROOT X TO val | Security | Instantiate an immutable, protected constant layer. |
| SHOW x | Console | Render variable evaluation or cluster structures natively. |
| BRAID list1 WITH list2 AS x | Collection | Weave parallel arrays into localized pairing lists or strict objects. |
| SUITE "Title" / SUITE/. | Testing | Structural encapsulation block for test isolation groupings. |
| VERIFY "desc", condition. | Testing | First-class native assertion statement for validation logic. |
| FLOW expression | Pipeline | Chain the output of an expression into sequential filters. |
| MATCH x / IS / YIELD | Branching | Non-fallthrough pattern matching with IS BETWEEN constraints. |
| ANALYZE list | Inspection | Extract deep statistical reports and multi-type metrics. |
| EVAPORATE x | Lifecycle | Instantly delete a variable pointer and free memory soil. |

---

## 📦 Recent Architectural Alignments & Enhancements (v1.0.0 Changelog)

The core engine line deprecates pure internal runtime runners and locks down the finalized structural vocabulary blueprint:

### 🔀 The BRAID Integration (The Final Keyword)
* Structural Array Weaving: Merges separate runtime data clusters into clean positional element pairings natively.
* Dynamic Mapping Compression: Zips matching keys and properties into a strongly-typed native MAP via BRAID keys WITH vals AS config MAP. blocks.
* Bounds Safety Trimming: Short-circuits matching loop sets to gracefully clip allocations tightly down to the shortest target collection size.

### 🧪 First-Class Test Engine (VERIFY Architecture)
* VERIFY Grammar Core: Added native keywords to process 5 structural validation configurations (Expression verification, Pipeline harvesting, Storm interception, Type assertions, and Collection bounds tracking).
* Automated CI/CD Ready CLI: Integrated chloroplast verify pipeline streaming clear terminal logs and returning explicit system exit codes (0 on success, 1 on regression failures).
* 100% Native Migration Baseline: Successfully migrated all 63 core unit tests into tests/suite.plnt written entirely in pure PlantLang.

### 🚀 Core Baseline Improvements
* Inline Evaluation Reducers: Refactored array analysis structures (FIRST, LAST, COUNT, SUM, MAX, MIN) to compile dynamically inside inline calculations and string combinations.
  
  1\ SHOW FIRST pair + " scored " + LAST pair.
  1\ SHOW "Total Points: " + SUM scores.

* ANALYZE Reducer Engine: Multi-type statistical analysis evaluating LIST models (sum, avg, min, max, median), TX sizes, and MAP dynamic footprints.
* ROOT_SCOPE Security Guard: Protected global environment map initializing inside _firstPass. Governs global configs via CONFIG:"key" strings, throwing a LOCK_STORM on unauthorized mutations.
* Quoted Key Evaluations (obj:"key"): Extended expression evaluators (evalExpr), SHOW, and SET syntax rules to seamlessly support quoted string literals as identifiers inside MAP collections.

---

## 🧪 Verification & Testing Status

* Native Test Automation (tests/suite.plnt): 63 / 63 Assertions Passed (100% Success Rate) ✅
* Legacy Evaluation Tests: 56 / 56 Assertions Passed (Arabic Compatibility Baseline) ✅
* Integration Benchmarks: All 5 / 5 Production Examples executed flawlessly without regressions or execution leaks. ✅

---

## 📦 Installation & Execution

You can clone and run the standalone Chloroplast CLI Engine natively using Node.js:

  gh repo clone UltimateHaider/PlantLang
  cd PlantLang
  npm link

---

## ⌨️ CLI Command Reference

Once linked or running locally, calling the environment signature reveals the built-in subcommands:

  chloroplast --help

### Expected Output Structure:
  🌿 PlantLang — Chloroplast v1.0.0
  Usage:
    chloroplast run   <file.plnt> [--mission FAST|SAFE|SMART] [--verbose]
    chloroplast verify <file.plnt>
    chloroplast repl  [--mission FAST|SAFE|SMART]
    chloroplast check <file.plnt>

### Command Flags Breakdown:
* verify <file>: Executes an automated test configuration profile natively, streaming green/red tracking blocks and returning an explicit system Exit Code (0 on complete success, 1 on hit regression).
* run <file>: Executes a standard .plnt file inside production boundaries without active testing instrumentation.
* repl: Launches the interactive command-line sandbox loop.
* check <file>: Validates structural script format correctness against the Lexer rules without executing variables evaluation.

---

## 🚀 How to Run the Web Simulator

If you prefer using the sandboxed interactive visual interface:

1. Open plantlang_complete_v8.html (or latest build) directly inside any modern web browser (Chrome, Edge, Firefox, Safari).
2. Choose any built-in script example from the benchmark dropdown menu.
3. Click ▶ GROW to watch your source code compile into target AST structures and manipulate active memory soil live.

---

## 🗺️ Project Roadmap (The Ecosystem Phase)

To evolve PlantLang from a locked standalone core engine into a globally deployable web stack, the next structural development cycle targets the following milestones:

### 1. Web, Networking & Persistency Modules
* TRACE Client API: Integrated asynchronous HTTP/HTTPS client blocks to process external network data streams directly into the local Soil.
* LISTEN BRANCH Server Engine: Concurrent routing layers to spin up live web micro-servers directly within scripts.
* STREAM Connectivity: Permanent duplex channel web pipelines to run real-time WebSocket communication blocks.
* SOW & FIND_IN Storage: Abstract data mapper layouts serving as abstract query hooks for database drivers.

### 2. Developer Experience (DX) & Identification Expansion
* Pinpoint Visual Diagnostic Logs: Injecting context-rich pointer indicators (^) reflecting rows and column intervals on terminal runtime exceptions.
* IDE Ecosystem / LSP: Full VSCode Extension supporting Language Server Protocol for Syntax Highlighting and Auto-complete.
* Arabic Native Identification: Modifying internal identifier regular expressions within the Lexer token matrices to accept complete Arabic script characters for variables, methods, and routines natively.
