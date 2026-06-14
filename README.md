# 🌱 Pantlang (Chloroplast Complete Engine — v0.7.0)

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

The active **Chloroplast v0.6.0** engine manages complex execution scopes including:

* **Object-Oriented Ecosystem (`SPECIES` / `PARENT` / `BLOOM`):** Supports true class structures, method overriding, absolute dynamic state isolation, and inheritance lineage validation via `IS_A`.
* **Advanced Functional Scopes (`ACTION` / `GIVE` / `REAP`):** Explicit typing parameter passing, deep functional recursion capabilities, and strict result harvesting.
* **Simulated Concurrency (`SPROUT` / `SYNC` / `RACE`):** Native multi-threaded block queuing where you can spawn async tasks, synchronize multiple workers, or capture the fastest resolving thread.
* **Reactive Data Binding (`PULSE` / `WHENEVER`):** Observable variables that trigger deeply nested responsive handlers automatically upon every mutation.
* **Virtual I/O Streaming (`TAP` / `ABSORB` / `INFUSE` / `SEAL`):** A mock file-system data layout to open paths, stream lines, inject content bytes, and safely close buffers.
* **The 12-Storm Exception Matrix:** 12 strongly-typed systemic exceptions (like `ZERO_STORM`, `LOCK_STORM`, `MISSING_STORM`) caught natively via localized `WEATHER` / `SHELTER` / `CALM` safety shelters.
* **The PLANT Standard Library:** Immediate structural extensions: `PLANT math` (11 functions), `PLANT strings` (10 functions), and `PLANT lists` (9 algorithms).

---

## 🛠️ Quick Syntax Reference

| Command Pattern | Operation Type | Meaning / Purpose |
| :--- | :--- | :--- |
| `CREATE x(TYPE) TO val` | Memory | Instantiate a strongly-typed variable in the current Soil. |
| `ROOT X TO val` | Security | Instantiate an immutable, protected constant layer. |
| `SHOW x` | Console | Render variable evaluation or cluster structures natively. |
| `INCREASE x BY n` | Arithmetic | Increment numerical boundaries safely. |
| `PUT val INTO list` | Collection | Inject an element into the end of a cluster. |
| `LINK key WITH val IN SELF:map` | Collection | Associate and bind key-value pairs directly inside a native MAP. |
| `FLOW expression` | Pipeline | Chain the output of an expression into sequential filters (supports multi-line parsing). |
| `MATCH x / IS / YIELDBranching` | Branching | Non-fallthrough pattern matching with `IS BETWEEN` constraints. |
| `ANALYZE list` | Inspection | Extract deep statistics (sum, avg, median, min, max). |
| `EVAPORATE x` | Lifecycle | Instantly delete a variable pointer and free memory soil. |

---

## 📦 Recent Architectural Fixes & Enhancements (v0.6.0 Changelog)

The latest stable release closes critical grammar, scoping, and data serialization gaps:

* **Escaped Delimiters in `CYCLE`:** Fixed a lexer bug where the closing double-backslash (`\\`) in `CYCLE FROM/TO` blocks inside `ACTION` contexts failed to tokenize correctly using strict regex evaluation (`/^\\+$/`).
* **Multi-line `FLOW` Sanitization:** Resolved parsing failures caused by trailing commas or redundant tokens in multi-line `FLOW` steps by introducing a robust line-cleansing pipeline (`clean,` normalization).
* **Quoted Sources in `FLOW`:** Upgraded the `FLOW` source extractor regex to fully support quoted string literals containing Arabic punctuation, spaces, and special characters without breaking the pipeline.
* **`COUNT SELF` within `SPECIES`:** Fixed an environment scoping bug where `COUNT SELF:prop` expressions evaluated to zero or threw errors when nested inside member functions of a `SPECIES` block.
* **`GIVE` Return Mechanism:** Fixed a crucial runtime bug where the `GIVE` control-flow command failed to bubble up and return values to the caller expression when invoked within `WEATHER` or `SHELTER` functional contexts.
* **Native Map Initialization:** Corrected the default value initialization for fields of type `MAP` inside `BLOOM` definitions from an empty string literal (`''`) to a proper native empty object structure (`{}`).
* **String Representation in `SHOW`:** Fixed an output formatting bug where executing `SHOW obj:prop` on compound types (Objects and Arrays) lazily printed `[object Object]`. The engine now correctly serializes and pretty-prints complex structures.

---

## 🧪 Verification & Testing Status

* **Core Unit Tests:** `50 / 50 Passed` (100% Success Rate) ✅
* **Integration Benchmarks:** All `5 / 5 Production Examples` executed without architectural or syntax regressions. ✅

---

## 🚀 How to Run the Simulator

Since the full core interpreter is built inside a highly responsive sandbox web environment:

1. Clone this repository or download the latest `plantlang_complete_v6.html` file.
2. Open the file directly by double-clicking it in any modern web browser (Chrome, Edge, Firefox, Safari).
3. Choose any of the ready-made built-in benchmark scripts from the dropdown:
   * **Comprehensive Ecosystem Test:** (Inheritance, Constants, Time Calculations).
   * **Advanced Math & Operations:** (PEMDAS compound calculation validation).
   * **ACTION / GIVE:** (Deep function recursion checks).
4. Click ▶ **GROW** to see your code compile, execute, and display real-time memory mutations in the Current Soil inspector pane.

---

## 🗺️ Project Roadmap (Future Goals)

To evolve Pantlang from a sandboxed web engine into a globally deployable production environment, the development lifecycle maps out the following milestones:

### 1. Standalone Core
* **Native Interpreter:** Moving from browser JavaScript into a fast standalone CLI engine binary (`Chloroplast Native`).
* **Transpiler Targets:** Compiling `.plnt` scripts into highly optimized Go, Rust, or Python source projects.
* **Interactive REPL:** A command-line console to test statements and track variables live.
* **Advanced Error Diagnostics:** Context-aware visual stack traces (`^`) indicating precise row/column offsets during compilation or runtime storms.

### 2. Network & Storage Layer
* **HARVEST API:** Integrated HTTP client blocks to process external network streams.
* **SOW / FIND_IN Drivers:** Abstract integration wrappers for persistence structures and database drivers.
* **LISTEN BRANCH Web Core:** Native backend router blocks to spin up localized micro-service web servers.

### 3. Developer Tooling
* **GREENHOUSE Packages:** A dedicated module compiler registry and dependency management suite.
* **SHIELD Security:** Capability-oriented resource bounds governing safe OS system operations.
* **IDE Ecosystem / LSP:** Full VSCode Extension supporting Language Server Protocol for Syntax Highlighting and Auto-complete.
* **Visual Logic & Localization Integration:** Native RTL parsing support allowing variable names and script code strings to accept Arabic input parameters natively, maintaining the visual integrity of both scripts.
