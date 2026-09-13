/*
 * plant_math.h — v0.50.0h: Symbolic Math + Like Terms + Distribution
 *
 * MathNode AST structures and API for the PlantLang symbolic algebra subsystem.
 * Supports numbers, symbols, constants, binary/unary ops, function calls,
 * operator precedence, right-associative exponentiation, evaluation to double,
 * automatic symbolic simplification, like terms collection, distribution,
 * and descending term ordering.
 */

#ifndef PLANT_MATH_H
#define PLANT_MATH_H

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ── Node types ─────────────────────────────────────────────── */

typedef enum {
    MATH_NUMBER,       /* numeric literal */
    MATH_SYMBOL,       /* variable / identifier */
    MATH_CONSTANT,     /* PI, E, TAU, PHI, SQRT2 */
    MATH_BINARY_OP,    /* +, -, *, /, ^ */
    MATH_UNARY_OP,     /* unary - */
    MATH_FUNC_CALL,    /* SIN(x), SQRT(x), etc. */
} MathNodeType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW,
    OP_NEG,  /* unary negation */
} MathOp;

/* ── MathNode ───────────────────────────────────────────────── */

typedef struct MathNode {
    MathNodeType type;
    MathOp       op;           /* for BINARY_OP / UNARY_OP */
    double       num_val;      /* for NUMBER */
    char*        sym_name;     /* for SYMBOL, CONSTANT, FUNC_CALL */
    struct MathNode* left;     /* binary: left operand; unary: operand; func: arg */
    struct MathNode* right;    /* binary: right operand only */
} MathNode;

/* ── Node constructors ──────────────────────────────────────── */

MathNode* math_node_number(double val);
MathNode* math_node_symbol(const char* name);
MathNode* math_node_constant(const char* name);
MathNode* math_node_binary(MathOp op, MathNode* left, MathNode* right);
MathNode* math_node_unary(MathNode* operand);
MathNode* math_node_func(const char* name, MathNode* arg);

/* ── Lifecycle ──────────────────────────────────────────────── */

void math_node_free(MathNode* node);

/* ── Deep Copy ──────────────────────────────────────────────── */

MathNode* plant_math_deep_copy(const MathNode* node);

/* ── Core API ───────────────────────────────────────────────── */

/* Parse a math expression string into an AST. Returns NULL on error. */
MathNode* plant_math_parse(const char* expr);

/* Evaluate an AST to a double value. Returns NaN on error. */
double    plant_math_eval(const MathNode* node);

/* Serialize an AST back to human-readable string. Caller frees result. */
char*     plant_math_to_string(const MathNode* node);

/* Simplify an AST: constant folding, identity reductions, cancellations.
 * Iterates until fixed point or MAX_SIMPLIFY_ITERATIONS (100). */
MathNode* plant_math_simplify(MathNode* node);

/* Debug: print AST hierarchy to stderr. */
void      plant_math_debug_print(void* node);

/* Convenience: parse + eval in one call. */
double    plant_math_eval_string(const char* expr);

/* Convenience: create a PlantMath* from an expression string. */
void*     plant_math_create(const char* expr);

/* Convenience: evaluate a PlantMath* to double. */
double    plant_math_value(void* math_ptr);

/* Convenience: evaluate a PlantMath* to string. Caller frees. */
char*     plant_math_value_str(void* math_ptr);

/* Convenience: convert PlantMath* back to string. Caller frees. */
char*     plant_math_to_str(void* math_ptr);

/* Convenience: parse + eval in one call, return result as string. Caller frees. */
char*     plant_math_eval_to_str(const char* expr);

/* Convenience: simplify a PlantMath* in-place. Returns the simplified AST. */
void*     plant_math_simplify_ptr(void* math_ptr);

/* Convenience: simplify an expression string. Caller frees result. */
char*     plant_math_simplify_str(const char* expr);

/* Convenience: free a PlantMath*. */
void      plant_math_free(void* math_ptr);

#endif /* PLANT_MATH_H */
