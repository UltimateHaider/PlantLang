/*
 * plant_codegen.c — v0.49.59c: Abstract Code Generation Interface (ICodegen)
 *
 * Wraps the PlantLang-generated codegen functions (generate_c,
 * generate_node, generate_body, env_make, env_set, env_get) in the
 * ICodegen vtable so callers can interact through the abstract
 * interface without coupling to a particular backend.
 *
 * The internal context carries the current environment pointer,
 * enabling set_env/get_env for execution configuration without
 * requiring the caller to thread env through every call.
 */

#include "plant_codegen.h"
#include "plant_compat.h"
#include <stdlib.h>
#include <string.h>

/* ── PlantLang-generated codegen functions (defined in the
   compiler's generated C code, linked at build time). ── */
extern tx_t generate_c(PlantArray* ast);
extern tx_t generate_node(tx_t node, PlantArray* env);
extern tx_t generate_body(PlantArray* bd, PlantArray* env);
extern tx_t env_make(long indent_num, PlantArray* sigs, PlantArray* subst,
                     PlantArray* clmap, tx_t actx, PlantArray* nums,
                     PlantArray* stvars, PlantArray* evars, tx_t rty,
                     tx_t mexit, tx_t wexit);
extern tx_t env_set(PlantArray* env, long idx, tx_t value);
extern tx_t env_get(PlantArray* env, long idx);

/* ── Internal codegen context ── */
typedef struct {
    PlantArray* current_env;   /* current execution environment */
} _CodegenContext;

/* ── ICodegen vtable implementations ── */

static char* _icodegen_generate(void* ctx, void* ast) {
    (void)ctx;
    return (char*)generate_c((PlantArray*)ast);
}

static char* _icodegen_emit(void* ctx, void* node) {
    _CodegenContext* cc = (_CodegenContext*)ctx;
    if (!cc) return NULL;
    return (char*)generate_node((tx_t)node, cc->current_env);
}

static char* _icodegen_generate_node(void* ctx, void* node, void* env) {
    (void)ctx;
    return (char*)generate_node((tx_t)node, (PlantArray*)env);
}

static void* _icodegen_env_new(void* ctx) {
    (void)ctx;
    /* Create a default env with zeroed fields — mirrors env_make
       with all-zero parameters for callers that need a blank slate. */
    return env_make(0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

static void* _icodegen_env_set(void* ctx, void* env, int idx, void* value) {
    (void)ctx;
    return env_set((PlantArray*)env, (long)idx, (tx_t)value);
}

static void* _icodegen_env_get(void* ctx, void* env, int idx) {
    (void)ctx;
    return env_get((PlantArray*)env, (long)idx);
}

static void _icodegen_env_free(void* ctx, void* env) {
    (void)ctx;
    /* env is a PlantArray allocated by the PlantLang runtime —
       plant_free handles it. Don't free here to avoid double-free
       when the caller also holds a reference. */
    (void)env;
}

static void _icodegen_set_env(void* ctx, void* env) {
    _CodegenContext* cc = (_CodegenContext*)ctx;
    if (cc) cc->current_env = (PlantArray*)env;
}

static void* _icodegen_get_env(void* ctx) {
    _CodegenContext* cc = (_CodegenContext*)ctx;
    if (cc) return cc->current_env;
    return NULL;
}

/* ── Constructor / Destructor ── */

ICodegen* PlantCodegen_create(void* context) {
    ICodegen* cg = (ICodegen*)malloc(sizeof(ICodegen));
    if (!cg) return NULL;

    _CodegenContext* cc = (_CodegenContext*)malloc(sizeof(_CodegenContext));
    if (!cc) { free(cg); return NULL; }

    cc->current_env = NULL;

    cg->context       = context ? context : cc;
    cg->generate      = _icodegen_generate;
    cg->emit          = _icodegen_emit;
    cg->generate_node = _icodegen_generate_node;
    cg->env_new       = _icodegen_env_new;
    cg->env_set       = _icodegen_env_set;
    cg->env_get       = _icodegen_env_get;
    cg->env_free      = _icodegen_env_free;
    cg->set_env       = _icodegen_set_env;
    cg->get_env       = _icodegen_get_env;

    if (!context) cg->context = cc;

    return cg;
}

void PlantCodegen_destroy(ICodegen* cg) {
    if (!cg) return;
    if (cg->context) {
        _CodegenContext* cc = (_CodegenContext*)cg->context;
        /* Don't free cc->current_env — owned by PlantLang runtime */
        free(cc);
    }
    free(cg);
}

/* ── Convenience helpers ── */

char* plant_iCodegen_generate(ICodegen* cg, void* ast) {
    if (cg && cg->generate) return cg->generate(cg->context, ast);
    return NULL;
}

char* plant_iCodegen_emit(ICodegen* cg, void* node) {
    if (cg && cg->emit) return cg->emit(cg->context, node);
    return NULL;
}

char* plant_iCodegen_generate_node(ICodegen* cg, void* node, void* env) {
    if (cg && cg->generate_node) return cg->generate_node(cg->context, node, env);
    return NULL;
}

void* plant_iCodegen_env_new(ICodegen* cg) {
    if (cg && cg->env_new) return cg->env_new(cg->context);
    return NULL;
}

void* plant_iCodegen_env_set(ICodegen* cg, void* env, int idx, void* value) {
    if (cg && cg->env_set) return cg->env_set(cg->context, env, idx, value);
    return NULL;
}

void* plant_iCodegen_env_get(ICodegen* cg, void* env, int idx) {
    if (cg && cg->env_get) return cg->env_get(cg->context, env, idx);
    return NULL;
}

void plant_iCodegen_env_free(ICodegen* cg, void* env) {
    if (cg && cg->env_free) cg->env_free(cg->context, env);
}

void plant_iCodegen_set_env(ICodegen* cg, void* env) {
    if (cg && cg->set_env) cg->set_env(cg->context, env);
}

void* plant_iCodegen_get_env(ICodegen* cg) {
    if (cg && cg->get_env) return cg->get_env(cg->context);
    return NULL;
}
