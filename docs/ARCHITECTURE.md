# PlantLang Chloroplast — Clean Architecture

> v0.49.61 — Finalized architectural specification.

## Overview

The Chloroplast compiler follows Robert C. Martin's **Clean Architecture** model,
with dependencies pointing inward toward the domain core. All cross-layer communication
goes through abstract interface contracts, enforced via Dependency Inversion (DIP).

## Tier Structure

```
┌─────────────────────────────────────────────────────────┐
│                    ENTITIES                             │
│   AST, Token, Node — core syntax and token data         │
│   PlantArray, PlantMap — foundational data structures    │
├─────────────────────────────────────────────────────────┤
│                   USE CASES                             │
│   SUITE, VERIFY, SETUP, TEARDOWN — test lifecycle       │
│   tokenize → parse → generate — compilation pipeline    │
├─────────────────────────────────────────────────────────┤
│                 INTERFACES (DIP Boundary)                │
│   ILexer    │ IParser    │ ICodegen                     │
│   IRuntime  │ IReport                                  │
├─────────────────────────────────────────────────────────┤
│                INFRASTRUCTURE                           │
│   lexer.plant, parser.plant, codegen_c.plant            │
│   plant_runtime.c, plant_report.c, plant_lexer.c        │
│   plant_parser.c, plant_codegen.c                       │
└─────────────────────────────────────────────────────────┘
```

## Compiler Layers

```
Source (.plant)
      │
      ▼
  ┌────────┐     ┌────────┐     ┌──────────┐
  │ Lexer  │────▶│ Parser │────▶│ Codegen  │
  │        │     │        │     │          │
  │ tokens │     │  AST   │     │ C source │
  └────────┘     └────────┘     └──────────┘
      │              │               │
      └──────────────┴───────────────┘
         All through ILexer/IParser/ICodegen
              via DI accessors
```

### 1. Lexer (lexer.plant → plant_lexer.c)
- **Input:** Raw source text
- **Output:** Token array (`PlantArray`)
- **Interface:** `ILexer` — tokenize, peek, consume, tok_type, tok_lex, is_eof
- **Random-access:** peek_at, consume_at, is_eof_at (explicit tokens+pos)
- **Concrete:** `PlantLexer_create()` wraps PlantLang-generated lexer functions

### 2. Parser (parser.plant → plant_parser.c)
- **Input:** Token array
- **Output:** AST (map-based node tree)
- **Interface:** `IParser` — parse, parse_statement, parse_expression, parse_block
- **Concrete:** `PlantParser_create()` wraps PlantLang-generated parser functions
- **Lexer access:** All parser functions obtain lexer via `get_lexer()` as first statement

### 3. Codegen (codegen_c.plant → plant_codegen.c)
- **Input:** AST
- **Output:** C source code string
- **Interface:** `ICodegen` — generate, emit, generate_node + env lifecycle
- **Concrete:** `PlantCodegen_create()` wraps PlantLang-generated codegen functions
- **All cross-layer calls:** Dispatched through DI accessors and vtable pointers

## Dependency Inversion Pattern

Every cross-layer call follows this pattern:

```c
// In generated C code:
plant_iReport_print(get_report(), value);      // SHOW statement
plant_iRuntime_verify(get_runtime(), lbl, c);  // VERIFY statement
plant_iLexer_peek_at(get_lexer(), tokens, pos); // Parser token peek
```

### DI Accessors (plant_runtime.c)

```c
ILexer*   get_lexer(void);    // Lazily creates PlantLexer
IParser*  get_parser(void);   // Lazily creates PlantParser
ICodegen* get_codegen(void);  // Lazily creates PlantCodegen
IRuntime* get_runtime(void);  // Lazily creates PlantRuntime
IReport*  get_report(void);   // Lazily creates PlantReport
```

All factories use `__attribute__((weak))` for graceful degradation in test
programs that don't link the full compiler runtime.

### Null-Safety

All `plant_i*_function()` helpers check for NULL interface pointers before
dereferencing vtable entries, returning safe defaults (NULL, 0, or 1).

## Interface Contracts

### ILexer (plant_lexer.h)
```c
struct ILexer {
    void* context;
    void* (*tokenize)(void* ctx, const char* source);
    void* (*peek)(void* ctx);
    void* (*consume)(void* ctx);
    const char* (*tok_type)(void* ctx, void* token);
    const char* (*tok_lex)(void* ctx, void* token);
    int (*is_eof)(void* ctx);
    void* (*peek_at)(void* ctx, void* tokens, long pos);
    void* (*consume_at)(void* ctx, void* tokens, long pos);
    int   (*is_eof_at)(void* ctx, void* tokens, long pos);
};
```

### IParser (plant_parser.h)
```c
struct IParser {
    void* context;
    void* (*parse)(void* ctx, const char* source);
    void* (*parse_statement)(void* ctx);
    void* (*parse_expression)(void* ctx);
    void* (*parse_block)(void* ctx);
};
```

### ICodegen (plant_codegen.h)
```c
struct ICodegen {
    void* context;
    char* (*generate)(void* ctx, void* ast);
    char* (*emit)(void* ctx, void* node);
    char* (*generate_node)(void* ctx, void* node, void* env);
    void* (*env_new)(void* ctx);
    void* (*env_set)(void* ctx, void* env, int idx, void* value);
    void* (*env_get)(void* ctx, void* env, int idx);
    void  (*env_free)(void* ctx, void* env);
    void  (*set_env)(void* ctx, void* env);
    void* (*get_env)(void* ctx);
};
```

### IRuntime (plant_runtime.h)
```c
struct IRuntime {
    void* context;
    void  (*execute)(void* ctx, const char* source);
    void  (*verify)(void* ctx, const char* label, int condition);
    void  (*verify_begin)(void* ctx);
    void  (*verify_end)(void* ctx);
    void  (*suite_setup)(void* ctx);
    void  (*suite_teardown)(void* ctx);
    void  (*error)(void* ctx, const char* msg);
    void  (*warning)(void* ctx, const char* msg);
    void  (*info)(void* ctx, const char* msg);
    void  (*fatal)(void* ctx, const char* msg);
};
```

### IReport (plant_report.h)
```c
struct IReport {
    void* context;
    void  (*print)(void* ctx, const char* message);
    void  (*summary)(void* ctx, int total, int passed, int failed);
    char* (*to_json)(void* ctx);
    char* (*to_html)(void* ctx);
    char* (*to_xml)(void* ctx);
    void  (*begin)(void* ctx);
    void  (*end)(void* ctx);
    void  (*add_result)(void* ctx, const char* name, int passed, double time);
};
```

## Self-Hosting Chain

```
dist/Chloroplast (v1, bootstrap)
      │ compiles
      ▼
    plantc_v2.c  ──gcc──▶  plantc_v2
                              │ compiles
                              ▼
                          plantc_v3.c  ──gcc──▶  plantc_v3  (= bin/Chloroplast)
                              │ compiles
                              ▼
                          plantc_v4.c  (must == plantc_v3.c for convergence)
```

Convergence at v0.49.61: **464520 bytes** (v3 = v4 = v5).

## Key Invariants

1. **No cross-layer direct calls:** Parser never calls codegen functions directly.
   All dispatch goes through interface vtables.
2. **DI accessors are lazy:** First call to `get_*()` creates a default instance.
3. **Weak symbols for portability:** Test programs that don't link the full runtime
   gracefully get NULL from `get_lexer()`/`get_codegen()`/`get_parser()`.
4. **NULL-safe vtable dispatch:** All `plant_i*_*()` helpers check interface pointer
   before dereferencing.
5. **Convergence:** Self-hosting chain produces identical output from v3 onward.
