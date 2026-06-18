# 🌱 Pantlang (Chloroplast Complete Engine — v0.9.0)

Pantlang is an innovative, high-level, structural programming language designed around human-like readability and structural simplicity. By utilizing an ecosystem-inspired vocabulary (such as Soil, Storms, Sprouting, and Reaping), Pantlang bridges the gap between natural language logic and robust computing architecture.

The project features a highly capable, interactive **Web Simulator & Core AST Interpreter** that fully demonstrates the language's syntax parsing, runtime memory analysis, concurrent behaviors, strict scoping, and reactive state management.

---

## ✨ The Core Philosophy

Pantlang is built upon three foundational design pillars:

* **Code Depth (No Braces):** Every block of code begins with an explicit depth number (e.g., `1.` for primary execution, `2.` for sub-operations). This completely eliminates nested brackets `{}` or strict tab-indentations.
* **Natural Punctuation:** Statements resemble human literature. An operation is fully sealed using a period `.`, while continuous structural statements can be extended onto subsequent lines using a comma `,`.
* **Pure Human Readability:** Syntactic commands read naturally as standard English instructions (e.g., `CREATE score(NUM) TO 100.`).

---

## 🚀 Key Features Implemented (Simulator & Interpreter Core)

The active **Chloroplast v0.9.0** engine manages complex execution scopes including:

* **Native Test Automation (`SUITE` / `VERIFY`):** Built-in first-class assertion frame supporting isolated expression checks, action harvesting, and native exception tracking.
* **Object-Oriented Ecosystem (`SPECIES` / `PARENT` / `BLOOM`):** Supports true class structures, method overriding, absolute dynamic state isolation, and inheritance lineage validation via `IS_A`.
* **Advanced Functional Scopes (`ACTION` / `GIVE` / `REAP`):** Explicit typing parameter passing, deep functional recursion capabilities, and strict result harvesting.
* **Simulated Concurrency (`SPROUT` / `SYNC` / `RACE`):** Native multi-threaded block queuing where you can spawn async tasks, synchronize multiple workers, or capture the fastest resolving thread.
* **Reactive Data Binding (`PULSE` / `WHENEVER`):** Observable variables that trigger deeply nested responsive handlers automatically upon every mutation (Fully verified alongside nested `MATCH` blocks).
* **Virtual I/O Streaming (`TAP` / `ABSORB` / `INFUSE` / `SEAL`):** A mock file-system data layout to open paths, stream lines, inject content bytes, and safely close buffers.
* **The 12-Storm Exception Matrix:** 12 strongly-typed systemic exceptions caught natively via localized `WEATHER` / `SHELTER` / `CALM` safety shelters.
* **The PLANT Standard Library:** Immediate structural extensions: `PLANT math` (11 functions), `PLANT strings` (10 functions), and `PLANT lists` (9 algorithms).

---

## 🛠️ Quick Syntax Reference

| Command Pattern | Operation Type | Meaning / Purpose |
| :--- | :--- | :--- |
| `CREATE x(TYPE) TO val` | Memory | Instantiate a strongly-typed variable in the current Soil. |
| `ROOT X TO val` | Security | Instantiate an immutable, protected constant layer. |
| `SHOW x` | Console | Render variable evaluation or cluster structures natively. |
| `SUITE "Title" / SUITE/.` | Testing | Structural encapsulation block for test isolation groupings. |
| `VERIFY "desc", condition.`| Testing | First-class native assertion statement for validation logic. |
| `FLOW expression` | Pipeline | Chain the output of an expression into sequential filters. |
| `MATCH x / IS / YIELD` | Branching | Non-fallthrough pattern matching with `IS BETWEEN` constraints. |
| `ANALYZE list` | Inspection | Extract deep statistical reports and multi-type metrics. |
| `EVAPORATE x` | Lifecycle | Instantly delete a variable pointer and free memory soil. |

---

## 📦 Recent Architectural Alignments & Enhancements (v0.9.0 Changelog)

The current core engine line deprecates pure internal runtime runners and introduces native verification syntaxes alongside the stable v0.8 ecosystem additions:

### 🧪 First-Class Test Engine (New in v0.9.0)
* **`VERIFY` Grammar Core:** Added native keywords to process 5 structural execution modes without external packages:
  1. **Expression Matching (`expr IS expected`):** Standard and compound boundaries evaluations.
  2. **Pipeline Harvesting (`FROM action, args GIVES expected`):** Direct regression assertion for explicit action block executions.
  3. **Storm Interception (`STORMS STORM_TYPE FROM expr`):** Validates precise exception bubbling across runtime panics.
  4. **Dynamic Type Assertion (`TYPE var IS TYPE_NAME`):** Strict verification of active runtime instances.
  5. **Capacity Bounds (`COUNT cluster IS n`):** Structural tracker checking `LIST` and `MAP` volumetric profiles.
* **`SUITE` Context Isolation:** Added block clustering to separate test reports cleanly.
* **Automated Regression Migration:** Transferred all historical core tests into `tests/suite.plnt` written entirely in pure Pantlang.

### 🚀 Core Baseline Improvements (v0.8.0 Legacy Consolidation)
* **`ANALYZE` Reducer Engine:** Multi-type statistical analysis evaluating `LIST` models (sum, avg, min, max, median), `TX` sizes, and `MAP` dynamic footprints.
* **`TYPEOF` Operator:** Real-time type model extraction via `REAP t FROM TYPEOF x.` expressions.
* **`NOW` Clock & `WAIT` Interlocking:** Fixed timing block collisions to revive `NOW` timestamps, and integrated true synchronous thread blocking using underlying `Atomics.wait` structures (bounded safely at 10 seconds).
* **`ROOT_SCOPE` Security Guard:** Protected global environment map initializing inside `_firstPass`. Governs global configs via `CONFIG:"key"` strings, throwing a `LOCK_STORM` on unauthorized mutations.
* **Quoted Key Evaluations (`obj:"key"`):** Extended expression evaluators (`evalExpr`), `SHOW`, and `SET` syntax rules to seamlessly support quoted string literals as identifiers inside `MAP` collections.

---

## 🧪 Verification & Testing Status

* **Native Test Automation (`tests/suite.plnt`):** `56 / 56 Assertions Passed` (100% Success Rate) ✅
* **Integration Benchmarks:** All `5 / 5 Production Examples` executed flawlessly without regressions or execution leaks. ✅

---

## 📦 Installation & Execution

You can clone and run the standalone **Chloroplast CLI Engine** natively using Node.js:

```bash
# Clone the official repository using GitHub CLI
gh repo clone UltimateHaider/PlantLang
cd PlantLang

# Link the binary globally to use the 'chloroplast' command directly
npm link
