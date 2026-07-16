# PlantLang Roadmap: Version v0.23.0 (The Complex Structures Release)

Following the successful achievement of Native Compilation (LLVM Backend) in v0.22.0, the focus for **v0.23.0** shifts from core primitives to complex data structures and object-oriented paradigms. The goal is to move PlantLang from a "system-level primitive language" to a "data-rich application language."

---

## 🚀 Key Objectives for v0.23.0

### 1. Advanced Data Structures (LIST & MAP)
* **LIST Implementation:** Introduce dynamic array support for homogeneous and heterogeneous data collections.
* **MAP Implementation:** Introduce key-value pair structures with hashing support.
* **LLVM Integration:** Define `struct` layouts in LLVM IR to handle dynamic pointers, including memory allocation logic (`malloc/free` wrappers in `runtime.c`).

### 2. Paradigm Shift (SPECIES & ACTION)
* **SPECIES:** Implement object-like templates to group data and methods. This will provide the architectural backbone for complex software designs.
* **ACTION:** Implement first-class function/method definitions, allowing reusable logic blocks that can be passed as arguments or stored in structures.

### 3. Memory & Runtime Enhancements
* **Memory Management:** Introduce basic reference counting or an ownership-based approach to safely handle the lifecycle of `LIST` and `SPECIES` instances.
* **Standard Library Expansion:** Develop `libplantlang.so` (or static equivalent) containing pre-compiled routines for sorting, searching, and IO manipulation.

### 4. Compiler Optimization
* **Loop Unrolling:** Enable LLVM’s advanced loop optimizations for `CYCLE` and `SEASON` constructs.
* **Dead Code Elimination:** Improve the AST-to-IR generation phase to prune unreachable branches before LLVM optimization.

---

## 🛠️ Engineering Milestones

| Milestone | Task | Priority |
| :--- | :--- | :--- |
| **M1** | AST Extensions for LIST/MAP/SPECIES | High |
| **M2** | LLVM IR Struct Generation & Type Lowering | High |
| **M3** | Runtime Memory Allocation/Deallocation Logic | Critical |
| **M4** | Integration Testing for Complex Structures | High |
| **M5** | Benchmarking (v0.22 vs v0.23) | Medium |

---

## 🎯 Success Criteria
* **Full Parity:** Every complex structure must behave identically in the Interpreter and the LLVM Native Binary.
* **No Memory Leaks:** Validation of the new memory management system using Valgrind/LLVM Sanitizers during integration tests.
* **AI-Generator Support:** Updating the language style guide to include examples of `SPECIES` and `ACTION` generation for LLMs.

---
*PlantLang v0.23.0: Evolving from Primitives to Paradigm. Building the future of Prose Programming.*