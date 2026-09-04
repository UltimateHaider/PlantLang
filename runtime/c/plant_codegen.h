#ifndef PLANT_CODEGEN_H
#define PLANT_CODEGEN_H

#include <plant_types.h>

/* ═══════════════════════════════════════════════════════════════
   v0.49.59c — Abstract Code Generation Interface (ICodegen)
   Comprehensive translation and environment management contract.
   Encapsulates the core code generation pipeline (AST → C source),
   environment container lifecycle, and execution configuration.

   Concrete codegen implementations (PlantLang's generate_c,
   future language backends) implement these pointers; callers
   interact through the abstract interface only.

   The context pointer carries codegen-specific state (indentation
   level, signature tables, substitution maps) so callers never
   see the internals of any particular backend.
   ═══════════════════════════════════════════════════════════════ */

typedef struct ICodegen ICodegen;
struct ICodegen {
    void* context;

    /* ── Core Code Generation ── */
    char* (*generate)(void* ctx, void* ast);
    char* (*emit)(void* ctx, void* node);
    char* (*generate_node)(void* ctx, void* node, void* env);

    /* ── Environment Subsystem ── */
    void* (*env_new)(void* ctx);
    void* (*env_set)(void* ctx, void* env, int idx, void* value);
    void* (*env_get)(void* ctx, void* env, int idx);
    void  (*env_free)(void* ctx, void* env);

    /* ── Configuration State ── */
    void  (*set_env)(void* ctx, void* env);
    void* (*get_env)(void* ctx);
};

/* Default codegen: wraps PlantLang's generate_c(), generate_node(),
   generate_body(), env_make(), env_set(), env_get() generated C
   functions. */
ICodegen* PlantCodegen_create(void* context);
void       PlantCodegen_destroy(ICodegen* cg);

/* Convenience helpers with null-safety */
char* plant_iCodegen_generate(ICodegen* cg, void* ast);
char* plant_iCodegen_emit(ICodegen* cg, void* node);
char* plant_iCodegen_generate_node(ICodegen* cg, void* node, void* env);
void* plant_iCodegen_env_new(ICodegen* cg);
void* plant_iCodegen_env_set(ICodegen* cg, void* env, int idx, void* value);
void* plant_iCodegen_env_get(ICodegen* cg, void* env, int idx);
void  plant_iCodegen_env_free(ICodegen* cg, void* env);
void  plant_iCodegen_set_env(ICodegen* cg, void* env);
void* plant_iCodegen_get_env(ICodegen* cg);

#endif
