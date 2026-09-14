/*
 * plant_math.h — v0.50.3: Symbolic Math + Advanced CAS + Complex Numbers + Limits
 *
 * MathNode AST structures and API for the PlantLang symbolic algebra subsystem.
 * Supports numbers, symbols, constants, binary/unary ops, function calls,
 * operator precedence, right-associative exponentiation, evaluation to double,
 * automatic simplification, like terms collection, distribution,
 * descending term ordering, symbolic differentiation, integration
 * (power rule, trig, exp, log, by-parts, by-substitution),
 * polynomial factoring, GCD extraction, quadratic equation solving
 * with complex number support, complete rational fraction unification,
 * full complex number arithmetic (PlantComplex), and limit evaluation
 * with L'Hôpital's rule for indeterminate forms.
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

/* Symbolic differentiation: d/dx(node). Caller frees result. */
MathNode* plant_math_derivative(const MathNode* node, const char* var);

/* Convenience: differentiate an expression string. Caller frees result. */
char*     plant_math_derivative_str(const char* expr, const char* var);

/* Symbolic integration: ∫ node dx. Returns NULL if unsupported. Caller frees result. */
MathNode* plant_math_integral(const MathNode* node, const char* var);

/* Convenience: integrate an expression string. Caller frees result. */
char*     plant_math_integral_str(const char* expr, const char* var);

/* Polynomial factoring. Caller frees result. */
MathNode* plant_math_factor(const MathNode* node);

/* Convenience: factor an expression string. Caller frees result. */
char*     plant_math_factor_str(const char* expr);

/* Compute GCD of two numbers (Euclidean algorithm). */
long      plant_math_gcd(long a, long b);

/* Quadratic equation solver: ax^2 + bx + c = 0.
 * Returns a string with roots (real or complex). Caller frees result. */
char*     plant_math_quadratic(double a, double b, double c);

/* Convenience: solve quadratic from string coefficients. Caller frees result. */
char*     plant_math_quadratic_str(const char* a, const char* b, const char* c);

/* Convenience: free a PlantMath*. */
void      plant_math_free(void* math_ptr);

/* ====================================================================
 *  v0.50.4 — Series Expansion Subsystem
 * ==================================================================== */

/* Compute Maclaurin series (center=0) of node, up to max_order terms.
 * Returns a polynomial MathNode (sum of terms). Caller must free. */
MathNode* plant_math_series(const MathNode* node, const char* var, int max_order);

/* Convenience: compute Maclaurin series from strings. Caller frees result. */
char*     plant_math_series_str(const char* expr, const char* var, int max_order);

/* Compute Taylor series about center, up to max_order terms.
 * Returns a polynomial MathNode. Caller must free. */
MathNode* plant_math_taylor(const MathNode* node, const char* var,
                             double center, int max_order);

/* Convenience: compute Taylor series from strings. Caller frees result. */
char*     plant_math_taylor_str(const char* expr, const char* var,
                                 const char* center, int max_order);

/* ====================================================================
 *  v0.50.4 — Partial Fraction Decomposition
 * ==================================================================== */

/* Decompose a rational expression into partial fractions.
 * Input must be a division node (num/den).
 * Returns a string representation. Caller frees result. */
char*     plant_math_partial_fractions_str(const char* expr, const char* var);

/* ====================================================================
 *  v0.50.3 — Limit Evaluation Subsystem
 * ==================================================================== */

/* Evaluate lim_{var→point} of node. Returns NAN if limit doesn't exist.
 * Use point = MATH_INF for limits at infinity. */
double    plant_math_limit_val(const MathNode* node, const char* var, double point);

/* Convenience: evaluate limit from strings. Returns result as string. Caller frees. */
char*     plant_math_limit_str(const char* expr, const char* var, const char* point);

/* ====================================================================
 *  v0.50.3 — Advanced Integration Techniques
 * ==================================================================== */

/* Integration by parts: ∫ u dv = uv - ∫ v du.
 * Automatically selects u via LIATE heuristic. Returns NULL if unsupported. */
MathNode* plant_math_integral_parts(const MathNode* node, const char* var);
char*     plant_math_integral_parts_str(const char* expr, const char* var);

/* Integration by substitution: ∫ f(g(x))·g'(x) dx = ∫ f(u) du.
 * Pattern-matches known composite forms. Returns NULL if unsupported. */
MathNode* plant_math_integral_subst(const MathNode* node, const char* var);
char*     plant_math_integral_subst_str(const char* expr, const char* var);

/* ====================================================================
 *  v0.50.2 — Complex Number Subsystem (PlantComplex)
 * ==================================================================== */

typedef struct {
    double real;
    double imag;
} PlantComplex;

/* Complex arithmetic */
PlantComplex plant_complex_add(PlantComplex a, PlantComplex b);
PlantComplex plant_complex_sub(PlantComplex a, PlantComplex b);
PlantComplex plant_complex_mul(PlantComplex a, PlantComplex b);
PlantComplex plant_complex_div(PlantComplex a, PlantComplex b);
PlantComplex plant_complex_conj(PlantComplex z);
double       plant_complex_abs(PlantComplex z);

/* Convenience: create complex from doubles */
PlantComplex plant_complex_make(double real, double imag);

/* Convenience: format complex as string "a+bi". Caller frees result. */
char*        plant_complex_to_str(PlantComplex z);

/* Convenience: complex built-in wrappers (string args → complex ops) */
char*  plant_complex_add_str(const char* a, const char* b);
char*  plant_complex_sub_str(const char* a, const char* b);
char*  plant_complex_mul_str(const char* a, const char* b);
char*  plant_complex_div_str(const char* a, const char* b);
char*  plant_complex_conj_str(const char* a);
char*  plant_complex_abs_str(const char* a);

#endif /* PLANT_MATH_H */
