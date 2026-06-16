# 🌱 Pantlang (Chloroplast Complete Engine — v0.8.0)

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

The active **Chloroplast v0.8.0** engine manages complex execution scopes including:

* **Object-Oriented Ecosystem (`SPECIES` / `PARENT` / `BLOOM`):** Supports true class structures, method overriding, absolute dynamic state isolation, and inheritance lineage validation via `IS_A`.
* **Advanced Functional Scopes (`ACTION` / `GIVE` / `REAP`):** Explicit typing parameter passing, deep functional recursion capabilities, and strict result harvesting.
* **Simulated Concurrency (`SPROUT` / `SYNC` / `RACE`):** Native multi-threaded block queuing where you can spawn async tasks, synchronize multiple workers, or capture the fastest resolving thread.
* **Reactive Data Binding (`PULSE` / `WHENEVER`):** Observable variables that trigger deeply nested responsive handlers automatically upon every mutation (Fully verified alongside nested `MATCH` blocks).
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
| `MATCH x / IS / YIELD` | Branching | Non-fallthrough pattern matching with `IS BETWEEN` constraints. |
| `ANALYZE list` | Inspection | Extract deep statistical reports and multi-type metrics. |
| `EVAPORATE x` | Lifecycle | Instantly delete a variable pointer and free memory soil. |

---

## 📦 Recent Architectural Alignments & Enhancements (v0.8.0 Changelog)

The latest release achieves complete production alignment with the language specifications, resolving historical technical debts and adding missing core functionalities:

### 🚀 Feature Enhancements
* **`ANALYZE` Engine Core:** Fully implemented custom multi-type statistical reporting:
  * **`LIST`:** Extracts deep metrics (`sum`, `avg`, `min`, `max`, and `median`).
  * **`TX` (Strings):** Computes total length and string boundaries metrics.
  * **`MAP`:** Inspects active keys and dynamic structural types.
* **`TYPEOF` Operator:** Implemented type extraction evaluation via `REAP t FROM TYPEOF x.` returning string representations of data models.
* **`NOW` Real-time Clock:** Successfully debugged and revived. Resolved an execution conflict with the global `REAP` pipeline and fixed scope collisions between `SHOW NOW` and custom local variables named `now`.
* **`WAIT` Synchronous Interlocking:** Implemented true synchronous blocking using `Atomics.wait` backend execution loops, securely bounded by a maximum of 10 seconds for runtime safety.
* **`ROOT_SCOPE` Execution Guard:** Built as a protected native `MAP` inside `_firstPass`. Supports dynamic global reads and configuration adjustments via `CONFIG:"key"` blocks while strictly raising a `LOCK_STORM` upon unauthorized mutations.
* **Quoted Map Key Evaluation (`obj:"key"`):** Extended expression evaluators (`evalExpr`), `SHOW`, and `SET` syntax rules to seamlessly support quoted string literals as identifiers inside `MAP` collections.
* **Deep Immutability Enforcement:** Added missing strict immutability checks for both `SET obj:prop` and `SET obj:"key"`, ensuring mutated locked maps gracefully throw `LOCK_STORM` exceptions.

### 🛠️ Stable Bug Fixes (Consolidated)
* **Escaped Delimiters in `CYCLE`:** Fixed a lexer bug where the closing double-backslash (`\\`) in `CYCLE FROM/TO` blocks inside `ACTION` contexts failed to tokenize correctly using strict regex evaluation (`/^\\+$/`).
* **Multi-line `FLOW` Sanitization:** Resolved parsing failures caused by trailing commas or redundant tokens in multi-line `FLOW` steps by introducing a robust line-cleansing pipeline (`clean,` normalization).
* **Quoted Sources in `FLOW`:** Upgraded the `FLOW` source extractor regex to fully support quoted string literals containing Arabic punctuation, spaces, and special characters without breaking the pipeline.
* **`COUNT SELF` within `SPECIES`:** Fixed an environment scoping bug where `COUNT SELF:prop` expressions evaluated to zero or threw errors when nested inside member functions of a `SPECIES` block.
* **`GIVE` Return Mechanism:** Fixed a crucial runtime bug where the `GIVE` control-flow command failed to bubble up and return values to the caller expression when invoked within `WEATHER` or `SHELTER` functional contexts.
* **Native Map Initialization:** Corrected the default value initialization for fields of type `MAP` inside `BLOOM` definitions from an empty string literal (`''`) to a proper native empty object structure (`{}`).
* **String Representation in `SHOW`:** Fixed an output formatting bug where executing `SHOW obj:prop` on compound types (Objects and Arrays) lazily printed `[object Object]`. The engine now correctly serializes and pretty-prints complex structures.

---

## 🧪 Verification & Testing Status

* **Core Unit Tests:** `56 / 56 Passed` (100% Success Rate — 6 new tests added for v0.8.0 core targets) ✅
* **Integration Benchmarks:** All `5 / 5 Production Examples` executed flawlessly without regressions. ✅

---

## 📦 Installation & Execution (Command Line Core)

You can clone and run the standalone **Chloroplast CLI Engine** natively using Node.js:

```bash
# Clone the official repository using GitHub CLI
gh repo clone UltimateHaider/PlantLang
cd PlantLang

# Link the binary globally to use the 'chloroplast' command directly
npm link

# Alternatively, run the core interpreter file directly using Node.js
node chloroplast.js --help
