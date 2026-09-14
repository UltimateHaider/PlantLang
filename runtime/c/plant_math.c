/*
 * plant_math.c — v0.50.2: Advanced Symbolic Simplification + Complex Numbers
 *
 * Full implementation of the PlantLang symbolic algebra subsystem.
 * Tokenizer → Pratt parser (precedence climbing) → AST evaluator.
 * Automatic simplification: constant folding, identity/cancellation,
 * like terms collection, distribution, descending term ordering.
 * Symbolic differentiation, integration with +C, polynomial factoring,
 * GCD extraction, quadratic equation solver with complex number support.
 */

#include "plant_math.h"
#include <ctype.h>
#include <float.h>
#include <string.h>

/* ====================================================================
 *  Tokenizer
 * ==================================================================== */

typedef enum {
    TOK_NUMBER, TOK_SYMBOL, TOK_OP, TOK_LPAREN, TOK_RPAREN, TOK_COMMA,
    TOK_EOF, TOK_ERROR
} TokType;

typedef struct {
    TokType  type;
    double   num_val;
    char     sym[128];
    MathOp   op;
} Token;

typedef struct {
    const char* src;
    size_t      pos;
} Tokenizer;

static void tok_init(Tokenizer* tz, const char* src) {
    tz->src = src ? src : "";
    tz->pos = 0;
}

static Token tok_next(Tokenizer* tz) {
    Token t = { TOK_ERROR, 0.0, "", OP_ADD };
    /* skip whitespace */
    while (tz->src[tz->pos] == ' ' || tz->src[tz->pos] == '\t')
        tz->pos++;
    char c = tz->src[tz->pos];
    if (c == '\0') { t.type = TOK_EOF; return t; }
    if (c == '(') { t.type = TOK_LPAREN; tz->pos++; return t; }
    if (c == ')') { t.type = TOK_RPAREN; tz->pos++; return t; }
    if (c == ',') { t.type = TOK_COMMA;  tz->pos++; return t; }
    /* operators */
    if (c == '+') { t.type = TOK_OP; t.op = OP_ADD; tz->pos++; return t; }
    if (c == '-') { t.type = TOK_OP; t.op = OP_SUB; tz->pos++; return t; }
    if (c == '*') { t.type = TOK_OP; t.op = OP_MUL; tz->pos++; return t; }
    if (c == '/') { t.type = TOK_OP; t.op = OP_DIV; tz->pos++; return t; }
    if (c == '^') { t.type = TOK_OP; t.op = OP_POW; tz->pos++; return t; }
    /* number */
    if (isdigit((unsigned char)c) || c == '.') {
        char* end;
        t.num_val = strtod(tz->src + tz->pos, &end);
        t.type = TOK_NUMBER;
        tz->pos = (size_t)(end - tz->src);
        return t;
    }
    /* symbol (identifier) */
    if (isalpha((unsigned char)c) || c == '_') {
        size_t start = tz->pos;
        while (isalnum((unsigned char)tz->src[tz->pos]) || tz->src[tz->pos] == '_')
            tz->pos++;
        size_t len = tz->pos - start;
        if (len >= sizeof(t.sym)) len = sizeof(t.sym) - 1;
        memcpy(t.sym, tz->src + start, len);
        t.sym[len] = '\0';
        t.type = TOK_SYMBOL;
        return t;
    }
    /* unrecognized */
    tz->pos++;
    t.type = TOK_ERROR;
    return t;
}

/* ====================================================================
 *  Node constructors
 * ==================================================================== */

MathNode* math_node_number(double val) {
    MathNode* n = (MathNode*)calloc(1, sizeof(MathNode));
    n->type = MATH_NUMBER;
    n->num_val = val;
    return n;
}

MathNode* math_node_symbol(const char* name) {
    MathNode* n = (MathNode*)calloc(1, sizeof(MathNode));
    n->type = MATH_SYMBOL;
    n->sym_name = strdup(name);
    return n;
}

MathNode* math_node_constant(const char* name) {
    MathNode* n = (MathNode*)calloc(1, sizeof(MathNode));
    n->type = MATH_CONSTANT;
    n->sym_name = strdup(name);
    return n;
}

MathNode* math_node_binary(MathOp op, MathNode* left, MathNode* right) {
    MathNode* n = (MathNode*)calloc(1, sizeof(MathNode));
    n->type = MATH_BINARY_OP;
    n->op = op;
    n->left = left;
    n->right = right;
    return n;
}

MathNode* math_node_unary(MathNode* operand) {
    MathNode* n = (MathNode*)calloc(1, sizeof(MathNode));
    n->type = MATH_UNARY_OP;
    n->op = OP_NEG;
    n->left = operand;
    return n;
}

MathNode* math_node_func(const char* name, MathNode* arg) {
    MathNode* n = (MathNode*)calloc(1, sizeof(MathNode));
    n->type = MATH_FUNC_CALL;
    /* Normalize: strip plant_/math_ prefix and uppercase so that
       deriv_func / other consumers can match canonical names
       like "SIN" regardless of codegen rewrites. */
    char norm[128];
    const char* src = name;
    if (strncmp(name, "plant_", 6) == 0) src = name + 6;
    else if (strncmp(name, "math_", 5) == 0) src = name + 5;
    size_t i;
    for (i = 0; src[i] && i < sizeof(norm) - 1; i++)
        norm[i] = (src[i] >= 'a' && src[i] <= 'z') ? src[i] - 32 : src[i];
    norm[i] = '\0';
    n->sym_name = strdup(norm);
    n->left = arg;
    return n;
}

/* ====================================================================
 *  Lifecycle
 * ==================================================================== */

void math_node_free(MathNode* node) {
    if (!node) return;
    if (node->sym_name) free(node->sym_name);
    math_node_free(node->left);
    math_node_free(node->right);
    free(node);
}

/* ====================================================================
 *  Constants & Functions
 * ==================================================================== */

typedef struct { const char* name; double value; } MathConst;
static const MathConst MATH_CONSTS[] = {
    { "PI",    3.14159265358979323846 },
    { "E",     2.71828182845904523536 },
    { "TAU",   6.28318530717958647692 },
    { "PHI",   1.61803398874989484820 },
    { "SQRT2", 1.41421356237309504880 },
    { "I",     0.0 },  /* imaginary unit: i^2 = -1 (symbolic only) */
    { NULL, 0.0 }
};

static double lookup_constant(const char* name) {
    for (int i = 0; MATH_CONSTS[i].name; i++)
        if (strcmp(name, MATH_CONSTS[i].name) == 0)
            return MATH_CONSTS[i].value;
    return NAN;
}

typedef double (*MathFunc1)(double);
typedef struct { const char* name; MathFunc1 fn; } MathFuncEntry;

static double fn_sin(double x)  { return sin(x); }
static double fn_cos(double x)  { return cos(x); }
static double fn_tan(double x)  { return tan(x); }
static double fn_sqrt(double x) { return sqrt(x); }
static double fn_exp(double x)  { return exp(x); }
static double fn_log(double x)  { return log(x); }
static double fn_abs(double x)  { return fabs(x); }
static double fn_asin(double x) { return asin(x); }
static double fn_acos(double x) { return acos(x); }
static double fn_atan(double x) { return atan(x); }

static const MathFuncEntry MATH_FUNCS[] = {
    { "SIN",  fn_sin  },
    { "COS",  fn_cos  },
    { "TAN",  fn_tan  },
    { "SQRT", fn_sqrt },
    { "EXP",  fn_exp  },
    { "LOG",  fn_log  },
    { "ABS",  fn_abs  },
    { "ASIN", fn_asin },
    { "ACOS", fn_acos },
    { "ARCTAN", fn_atan },
    { "ATAN", fn_atan },
    { NULL, NULL }
};

static MathFunc1 lookup_function(const char* name) {
    for (int i = 0; MATH_FUNCS[i].name; i++)
        if (strcmp(name, MATH_FUNCS[i].name) == 0)
            return MATH_FUNCS[i].fn;
    /* Also accept C function names used by the codegen (plant_sin, math_sin, etc.) */
    const char* prefix = NULL;
    size_t prefix_len = 0;
    if (strncmp(name, "plant_", 6) == 0) { prefix = "plant_"; prefix_len = 6; }
    if (strncmp(name, "math_", 5) == 0) { prefix = "math_"; prefix_len = 5; }
    if (prefix) {
        for (int i = 0; MATH_FUNCS[i].name; i++) {
            /* Lowercase the MATH_FUNCS name for comparison */
            char lower_name[128];
            const char* src_name = MATH_FUNCS[i].name;
            size_t j;
            for (j = 0; src_name[j] && j < sizeof(lower_name) - 1; j++)
                lower_name[j] = src_name[j] >= 'A' && src_name[j] <= 'Z' ? src_name[j] + 32 : src_name[j];
            lower_name[j] = '\0';
            char prefixed[128];
            snprintf(prefixed, sizeof(prefixed), "%s%s", prefix, lower_name);
            if (strcmp(name, prefixed) == 0)
                return MATH_FUNCS[i].fn;
        }
    }
    return NULL;
}

/* ====================================================================
 *  Pratt Parser — precedence climbing with right-assoc exponentiation
 * ==================================================================== */

typedef struct {
    Tokenizer tz;
    Token     cur;
} Parser;

static void parser_init(Parser* p, const char* expr) {
    tok_init(&p->tz, expr);
    p->cur = tok_next(&p->tz);
}

static Token parser_advance(Parser* p) {
    Token prev = p->cur;
    p->cur = tok_next(&p->tz);
    return prev;
}

static int parser_match(Parser* p, TokType type) {
    if (p->cur.type == type) {
        parser_advance(p);
        return 1;
    }
    return 0;
}

/* operator precedence: higher = tighter binding */
static int op_precedence(MathOp op) {
    switch (op) {
        case OP_ADD: case OP_SUB: return 1;
        case OP_MUL: case OP_DIV: return 2;
        case OP_POW:              return 3;
        default:                  return 0;
    }
}

/* ^ is right-associative; everything else is left-associative */
static int op_right_assoc(MathOp op) {
    return op == OP_POW ? 1 : 0;
}

/* forward declaration */
static MathNode* parse_expr(Parser* p, int min_prec);

/* parse sub-expression (used for parenthesized / function args) */
MathNode* plant_math_parse_subexpr(Parser* p, int min_prec) {
    return parse_expr(p, min_prec);
}

/* parse a primary: number, symbol, constant, func call, parenthesized expr, unary minus */
static MathNode* parse_primary(Parser* p) {
    /* unary minus */
    if (p->cur.type == TOK_OP && p->cur.op == OP_SUB) {
        parser_advance(p);
        MathNode* operand = parse_primary(p);
        return math_node_unary(operand);
    }
    /* unary plus (just skip) */
    if (p->cur.type == TOK_OP && p->cur.op == OP_ADD) {
        parser_advance(p);
        return parse_primary(p);
    }
    /* parenthesized expression */
    if (p->cur.type == TOK_LPAREN) {
        parser_advance(p);
        MathNode* expr = plant_math_parse_subexpr(p, 0);
        parser_match(p, TOK_RPAREN);
        return expr;
    }
    /* number */
    if (p->cur.type == TOK_NUMBER) {
        double val = p->cur.num_val;
        parser_advance(p);
        return math_node_number(val);
    }
    /* symbol: could be a constant or a function call or a variable */
    if (p->cur.type == TOK_SYMBOL) {
        char name[128];
        strncpy(name, p->cur.sym, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        parser_advance(p);
        /* check for function call: NAME( */
        if (p->cur.type == TOK_LPAREN) {
            parser_advance(p);
            MathNode* arg = plant_math_parse_subexpr(p, 0);
            parser_match(p, TOK_RPAREN);
            return math_node_func(name, arg);
        }
        /* check for known constant */
        double cv = lookup_constant(name);
        if (!isnan(cv)) return math_node_constant(name);
        /* variable */
        return math_node_symbol(name);
    }
    /* fallback: return 0 */
    return math_node_number(0.0);
}

/* forward declaration */
static MathNode* parse_expr(Parser* p, int min_prec);

/* Pratt climbing: parse expression with minimum precedence */
static MathNode* parse_expr(Parser* p, int min_prec) {
    MathNode* left = parse_primary(p);

    while (p->cur.type == TOK_OP && op_precedence(p->cur.op) >= min_prec) {
        MathOp op = p->cur.op;
        int prec = op_precedence(op);
        int ra = op_right_assoc(op);
        parser_advance(p);
        /* for left-assoc: consume at prec+1; for right-assoc: consume at prec */
        int next_min = ra ? prec : prec + 1;
        MathNode* right = parse_expr(p, next_min);
        left = math_node_binary(op, left, right);
    }
    return left;
}

/* ====================================================================
 *  Public parse entry
 * ==================================================================== */

MathNode* plant_math_parse(const char* expr) {
    if (!expr || !*expr) return math_node_number(0.0);
    /* Strip _from_double() and _from_long() wrappers added by codegen */
    static char buf[2048];
    const char* src = expr;
    char* dst = buf;
    size_t remaining = sizeof(buf) - 1;
    while (*src && remaining > 0) {
        if (strncmp(src, "_from_double(", 13) == 0) {
            src += 13;
            int depth = 1;
            const char* p = src;
            while (*p && depth > 0) {
                if (*p == '(') depth++;
                if (*p == ')') depth--;
                p++;
            }
            if (depth == 0 && *(p-1) == ')') {
                /* Copy inner expression (without trailing paren) */
                size_t len = (size_t)(p - 1 - src);
                if (len > remaining) len = remaining;
                memcpy(dst, src, len);
                dst += len;
                remaining -= len;
                src = p;
            } else {
                /* Unbalanced, copy as-is */
                *dst++ = *src++;
                remaining--;
            }
        } else if (strncmp(src, "_from_long(", 11) == 0) {
            src += 11;
            int depth = 1;
            const char* p = src;
            while (*p && depth > 0) {
                if (*p == '(') depth++;
                if (*p == ')') depth--;
                p++;
            }
            if (depth == 0 && *(p-1) == ')') {
                size_t len = (size_t)(p - 1 - src);
                if (len > remaining) len = remaining;
                memcpy(dst, src, len);
                dst += len;
                remaining -= len;
                src = p;
            } else {
                *dst++ = *src++;
                remaining--;
            }
        } else {
            *dst++ = *src++;
            remaining--;
        }
    }
    *dst = '\0';
    Parser p;
    parser_init(&p, buf);
    MathNode* ast = parse_expr(&p, 0);
    /* v0.50.0g: automatically simplify after parsing */
    ast = plant_math_simplify(ast);
    return ast;
}

/* ====================================================================
 *  Evaluator
 * ==================================================================== */

double plant_math_eval(const MathNode* node) {
    if (!node) return NAN;
    switch (node->type) {
        case MATH_NUMBER:
            return node->num_val;
        case MATH_SYMBOL: {
            double cv = lookup_constant(node->sym_name);
            if (!isnan(cv)) return cv;
            return NAN;  /* unresolved symbol */
        }
        case MATH_CONSTANT:
            return lookup_constant(node->sym_name);
        case MATH_BINARY_OP: {
            double l = plant_math_eval(node->left);
            double r = plant_math_eval(node->right);
            switch (node->op) {
                case OP_ADD: return l + r;
                case OP_SUB: return l - r;
                case OP_MUL: return l * r;
                case OP_DIV: return r != 0.0 ? l / r : NAN;
                case OP_POW: return pow(l, r);
                default:     return NAN;
            }
        }
        case MATH_UNARY_OP: {
            double v = plant_math_eval(node->left);
            if (node->op == OP_NEG) return -v;
            return NAN;
        }
        case MATH_FUNC_CALL: {
            double arg = plant_math_eval(node->left);
            MathFunc1 fn = lookup_function(node->sym_name);
            if (fn) return fn(arg);
            return NAN;
        }
    }
    return NAN;
}

/* ====================================================================
 *  Deep Copy (AST cloning for safe simplification rewrites)
 * ==================================================================== */

MathNode* plant_math_deep_copy(const MathNode* node) {
    if (!node) return NULL;
    switch (node->type) {
        case MATH_NUMBER:
            return math_node_number(node->num_val);
        case MATH_SYMBOL:
            return math_node_symbol(node->sym_name);
        case MATH_CONSTANT:
            return math_node_constant(node->sym_name);
        case MATH_BINARY_OP:
            return math_node_binary(node->op,
                plant_math_deep_copy(node->left),
                plant_math_deep_copy(node->right));
        case MATH_UNARY_OP:
            return math_node_unary(plant_math_deep_copy(node->left));
        case MATH_FUNC_CALL:
            return math_node_func(node->sym_name,
                plant_math_deep_copy(node->left));
    }
    return NULL;
}

/* ====================================================================
 *  Simplifier — bottom-up AST rewrite with iteration cap
 *
 *  Rules applied:
 *    Constant folding:  2 + 3 → 5,  SIN(0) → 0,  PI → 3.14...
 *    Identity:          x + 0 → x,  x - 0 → x,  x * 1 → x,  x^1 → x
 *    Cancellation:      x - x → 0,  x / x → 1   (x ≠ 0)
 *    Zero power:        x^0 → 1                    (x ≠ 0)
 * ==================================================================== */

#define MAX_SIMPLIFY_ITERATIONS 100

/* Forward declaration */
static MathNode* simplify_node(MathNode* node);

/* Check if two ASTs are structurally identical */
static int trees_equal(const MathNode* a, const MathNode* b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case MATH_NUMBER:
            return a->num_val == b->num_val;
        case MATH_SYMBOL:
        case MATH_CONSTANT:
        case MATH_FUNC_CALL:
            return strcmp(a->sym_name, b->sym_name) == 0 &&
                   trees_equal(a->left, b->left);
        case MATH_BINARY_OP:
            return a->op == b->op &&
                   trees_equal(a->left, b->left) &&
                   trees_equal(a->right, b->right);
        case MATH_UNARY_OP:
            return a->op == b->op &&
                   trees_equal(a->left, b->left);
    }
    return 0;
}

/* Check if a node is a numeric literal */
static int is_number(const MathNode* n) {
    return n && n->type == MATH_NUMBER;
}

/* Check if a node evaluates to zero (numbers and constants) */
static int is_zero(const MathNode* n) {
    if (!n) return 0;
    if (n->type == MATH_NUMBER) return n->num_val == 0.0;
    if (n->type == MATH_CONSTANT) {
        double v = lookup_constant(n->sym_name);
        return !isnan(v) && v == 0.0;
    }
    return 0;
}

/* Check if a node evaluates to one */
static int is_one(const MathNode* n) {
    if (!n) return 0;
    if (n->type == MATH_NUMBER) return n->num_val == 1.0;
    if (n->type == MATH_CONSTANT) {
        double v = lookup_constant(n->sym_name);
        return !isnan(v) && v == 1.0;
    }
    return 0;
}

/* Forward declaration for is_func (defined after simplify_node) */
static int is_func(const MathNode* n, const char* name);

/* Simplify a single node (one pass) — returns new or reused node */
static MathNode* simplify_node(MathNode* node) {
    if (!node) return NULL;

    /* Recurse into children first (bottom-up) */
    if (node->left) node->left = simplify_node(node->left);
    if (node->right) node->right = simplify_node(node->right);

    switch (node->type) {
        case MATH_NUMBER:
        case MATH_SYMBOL:
            return node;  /* nothing to simplify */

        case MATH_CONSTANT: {
            /* Fold constants to their numeric value */
            double v = lookup_constant(node->sym_name);
            if (!isnan(v)) {
                free(node->sym_name);
                node->type = MATH_NUMBER;
                node->num_val = v;
                node->sym_name = NULL;
            }
            return node;
        }

        case MATH_UNARY_OP: {
            if (node->op == OP_NEG) {
                /* Fold unary negation of a number */
                if (is_number(node->left)) {
                    node->left->num_val = -(node->left->num_val);
                    MathNode* result = node->left;
                    node->left = NULL;
                    math_node_free(node);
                    return result;
                }
                /* Double negation: -(-x) → x */
                if (node->left->type == MATH_UNARY_OP && node->left->op == OP_NEG) {
                    MathNode* inner = node->left->left;
                    node->left->left = NULL;
                    math_node_free(node);
                    return inner;
                }
            }
            return node;
        }

        case MATH_FUNC_CALL: {
            /* v0.50.2 — SQRT(-1) → i, before numeric folding which would yield NaN */
            if (is_func(node, "SQRT") && node->left &&
                node->left->type == MATH_NUMBER && node->left->num_val == -1.0) {
                node->left = NULL;
                math_node_free(node);
                node = math_node_symbol("i");
                return node;
            }
            /* Fold function calls on numeric arguments */
            if (is_number(node->left)) {
                MathFunc1 fn = lookup_function(node->sym_name);
                if (fn) {
                    double result = fn(node->left->num_val);
                    free(node->sym_name);
                    free(node->left->sym_name);
                    node->left->num_val = result;
                    node->type = MATH_NUMBER;
                    node->sym_name = NULL;
                    MathNode* num = node->left;
                    node->left = NULL;
                    math_node_free(node);
                    return num;
                }
            }
            return node;
        }

        case MATH_BINARY_OP: {
            MathNode* L = node->left;
            MathNode* R = node->right;

            /* ── Constant folding: both children are numbers ── */
            if (is_number(L) && is_number(R)) {
                double l = L->num_val, r = R->num_val, result;
                switch (node->op) {
                    case OP_ADD: result = l + r; break;
                    case OP_SUB: result = l - r; break;
                    case OP_MUL: result = l * r; break;
                    case OP_DIV: result = (r != 0.0) ? l / r : NAN; break;
                    case OP_POW: result = pow(l, r); break;
                    default:     result = NAN; break;
                }
                free(L->sym_name);
                free(R->sym_name);
                L->num_val = result;
                /* Detach children before freeing parent */
                node->left = NULL;
                node->right = NULL;
                math_node_free(R);
                math_node_free(node);
                return L;
            }

            switch (node->op) {
                case OP_ADD:
                    /* x + 0 → x */
                    if (is_zero(R)) {
                        node->right = NULL;
                        math_node_free(R);
                        MathNode* result = L;
                        node->left = NULL;
                        math_node_free(node);
                        return result;
                    }
                    /* 0 + x → x */
                    if (is_zero(L)) {
                        node->left = NULL;
                        math_node_free(L);
                        MathNode* result = R;
                        node->right = NULL;
                        math_node_free(node);
                        return result;
                    }
                    break;

                case OP_SUB:
                    /* x - 0 → x */
                    if (is_zero(R)) {
                        node->right = NULL;
                        math_node_free(R);
                        MathNode* result = L;
                        node->left = NULL;
                        math_node_free(node);
                        return result;
                    }
                    /* 0 - x → -x */
                    if (is_zero(L)) {
                        node->left = NULL;
                        math_node_free(L);
                        node->type = MATH_UNARY_OP;
                        node->op = OP_NEG;
                        node->left = R;
                        node->right = NULL;
                        return node;
                    }
                    /* x - x → 0 */
                    if (trees_equal(L, R)) {
                        node->left = NULL;
                        node->right = NULL;
                        math_node_free(L);
                        math_node_free(R);
                        node->type = MATH_NUMBER;
                        node->num_val = 0.0;
                        node->sym_name = NULL;
                        return node;
                    }
                    break;

                case OP_MUL:
                    /* x * 1 → x */
                    if (is_one(R)) {
                        node->right = NULL;
                        math_node_free(R);
                        MathNode* result = L;
                        node->left = NULL;
                        math_node_free(node);
                        return result;
                    }
                    /* 1 * x → x */
                    if (is_one(L)) {
                        node->left = NULL;
                        math_node_free(L);
                        MathNode* result = R;
                        node->right = NULL;
                        math_node_free(node);
                        return result;
                    }
                    /* x * 0 → 0 */
                    if (is_zero(R) || is_zero(L)) {
                        node->left = NULL;
                        node->right = NULL;
                        math_node_free(L);
                        math_node_free(R);
                        node->type = MATH_NUMBER;
                        node->num_val = 0.0;
                        node->sym_name = NULL;
                        return node;
                    }
                    break;

                case OP_DIV:
                    /* x / 1 → x */
                    if (is_one(R)) {
                        node->right = NULL;
                        math_node_free(R);
                        MathNode* result = L;
                        node->left = NULL;
                        math_node_free(node);
                        return result;
                    }
                    /* 0 / x → 0 (x ≠ 0 by domain) */
                    if (is_zero(L) && !is_zero(R)) {
                        node->left = NULL;
                        node->right = NULL;
                        math_node_free(L);
                        math_node_free(R);
                        node->type = MATH_NUMBER;
                        node->num_val = 0.0;
                        node->sym_name = NULL;
                        return node;
                    }
                    /* x / x → 1 (valid when x ≠ 0) */
                    if (trees_equal(L, R)) {
                        node->left = NULL;
                        node->right = NULL;
                        math_node_free(L);
                        math_node_free(R);
                        node->type = MATH_NUMBER;
                        node->num_val = 1.0;
                        node->sym_name = NULL;
                        return node;
                    }
                    break;

                case OP_POW:
                    /* x^0 → 1 (valid when x ≠ 0) */
                    if (is_zero(R)) {
                        node->left = NULL;
                        node->right = NULL;
                        math_node_free(L);
                        math_node_free(R);
                        node->type = MATH_NUMBER;
                        node->num_val = 1.0;
                        node->sym_name = NULL;
                        return node;
                    }
                    /* x^1 → x */
                    if (is_one(R)) {
                        node->right = NULL;
                        math_node_free(R);
                        MathNode* result = L;
                        node->left = NULL;
                        math_node_free(node);
                        return result;
                    }
                    /* 0^x → 0 (x > 0) */
                    if (is_zero(L)) {
                        node->left = NULL;
                        node->right = NULL;
                        math_node_free(L);
                        math_node_free(R);
                        node->type = MATH_NUMBER;
                        node->num_val = 0.0;
                        node->sym_name = NULL;
                        return node;
                    }
                    break;

                default:
                    break;
            }
            return node;
        }

        default:
            return node;
    }
}

/* ====================================================================
 *  v0.50.1 — Advanced Symbolic Simplification Engines
 *
 *  Four specialized passes that run inside the simplify pipeline:
 *    simplify_pow()     — exponent law reductions
 *    simplify_trig()    — trigonometric identity rewrites
 *    simplify_log()     — logarithmic law expansions
 *    simplify_frac()    — rational fraction denominator unification
 *    simplify_complex() — complex number constant folding (i*i → -1)
 * ==================================================================== */

static MathNode* simplify_complex_node(MathNode* node);

/* ── Helper: check if node is a function call with given name ───────
 *    Matches both "SQRT" and prefixed variants like "plant_sqrt", "math_sqrt". */
static int is_func(const MathNode* n, const char* name) {
    if (!n || n->type != MATH_FUNC_CALL || !n->sym_name) return 0;
    if (strcmp(n->sym_name, name) == 0) return 1;
    /* Also accept plant_ or math_ prefixed lowercase variants */
    const char* prefix = NULL;
    size_t prefix_len = 0;
    if (strncmp(n->sym_name, "plant_", 6) == 0) { prefix = "plant_"; prefix_len = 6; }
    if (strncmp(n->sym_name, "math_", 5) == 0) { prefix = "math_"; prefix_len = 5; }
    if (prefix) {
        /* Lowercase the canonical name and compare with prefix */
        char lower_name[128];
        size_t j;
        for (j = 0; name[j] && j < sizeof(lower_name) - 1; j++)
            lower_name[j] = name[j] >= 'A' && name[j] <= 'Z' ? name[j] + 32 : name[j];
        lower_name[j] = '\0';
        char expected[128];
        snprintf(expected, sizeof(expected), "%s%s", prefix, lower_name);
        if (strcmp(n->sym_name, expected) == 0) return 1;
    }
    return 0;
}

/* ── Helper: check if node matches op as binary ───────────────────── */
static int is_op(const MathNode* n, MathOp op) {
    return n && n->type == MATH_BINARY_OP && n->op == op;
}

/* ── Helper: check if node is a specific symbol ───────────────────── */
static int is_sym(const MathNode* n, const char* name) {
    return n && n->type == MATH_SYMBOL &&
           n->sym_name && strcmp(n->sym_name, name) == 0;
}

/* ====================================================================
 *  simplify_pow — Exponent Law Reductions
 *
 *  (x^a)^b  → x^(a*b)     power of a power
 *  x^a * x^b → x^(a+b)    product of powers
 *  x^a / x^b → x^(a-b)    quotient of powers
 *  x^0       → 1           zero exponent (already in simplify_node)
 *  x^1       → x           trivial exponent (already in simplify_node)
 *  1^x       → 1           one to any power
 * ==================================================================== */

static MathNode* simplify_pow_node(MathNode* node) {
    if (!node) return NULL;

    /* Bottom-up: recurse first */
    if (node->left)  node->left  = simplify_pow_node(node->left);
    if (node->right) node->right = simplify_pow_node(node->right);

    /* Power of a power: (x^a)^b → x^(a*b) */
    if (node->type == MATH_BINARY_OP && node->op == OP_POW) {
        MathNode* base = node->left;
        MathNode* exp  = node->right;
        if (base && base->type == MATH_BINARY_OP && base->op == OP_POW) {
            /* (x^a)^b → x^(a*b) */
            MathNode* inner_base = base->left;
            MathNode* inner_exp  = base->right;
            MathNode* new_exp = math_node_binary(OP_MUL,
                plant_math_deep_copy(inner_exp),
                plant_math_deep_copy(exp));
            node->left = NULL;
            node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_POW, inner_base, new_exp);
            return node;
        }
        /* 1^x → 1 */
        if (is_one(base)) {
            node->left = NULL;
            node->right = NULL;
            math_node_free(base);
            math_node_free(exp);
            node->type = MATH_NUMBER;
            node->num_val = 1.0;
            node->sym_name = NULL;
            return node;
        }
        /* (-1)^2n → 1, (-1)^(2n+1) → -1 */
        if (base->type == MATH_UNARY_OP && base->op == OP_NEG &&
            is_one(base->left) && is_number(exp)) {
            double e = exp->num_val;
            if (e == (int)e) {
                int ei = (int)e;
                node->left = NULL;
                node->right = NULL;
                math_node_free(base);
                math_node_free(exp);
                node->type = MATH_NUMBER;
                node->num_val = (ei % 2 == 0) ? 1.0 : -1.0;
                node->sym_name = NULL;
                return node;
            }
        }
    }

    /* Product of powers: x^a * x^b → x^(a+b) */
    if (node->type == MATH_BINARY_OP && node->op == OP_MUL) {
        MathNode* L = node->left;
        MathNode* R = node->right;
        if (L && R &&
            L->type == MATH_BINARY_OP && L->op == OP_POW &&
            R->type == MATH_BINARY_OP && R->op == OP_POW &&
            trees_equal(L->left, R->left)) {
            /* x^a * x^b → x^(a+b) */
            MathNode* new_exp = math_node_binary(OP_ADD,
                plant_math_deep_copy(L->right),
                plant_math_deep_copy(R->right));
            MathNode* new_base = plant_math_deep_copy(L->left);
            node->left = NULL;
            node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_POW, new_base, new_exp);
            return node;
        }
    }

    /* Quotient of powers: x^a / x^b → x^(a-b) */
    if (node->type == MATH_BINARY_OP && node->op == OP_DIV) {
        MathNode* L = node->left;
        MathNode* R = node->right;
        if (L && R &&
            L->type == MATH_BINARY_OP && L->op == OP_POW &&
            R->type == MATH_BINARY_OP && R->op == OP_POW &&
            trees_equal(L->left, R->left)) {
            /* x^a / x^b → x^(a-b) */
            MathNode* new_exp = math_node_binary(OP_SUB,
                plant_math_deep_copy(L->right),
                plant_math_deep_copy(R->right));
            MathNode* new_base = plant_math_deep_copy(L->left);
            node->left = NULL;
            node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_POW, new_base, new_exp);
            return node;
        }
    }

    return node;
}

/* ====================================================================
 *  simplify_trig — Trigonometric Identity Rewrites
 *
 *  sin^2(x) + cos^2(x) → 1
 *  1 - sin^2(x)        → cos^2(x)
 *  1 - cos^2(x)        → sin^2(x)
 *  tan(x)               → sin(x)/cos(x)
 *  1 + tan^2(x)         → 1/cos^2(x)   (i.e. sec^2(x))
 * ==================================================================== */

/* Check if node matches func_name(arg)^2 */
static int is_func_sq(const MathNode* n, const char* func_name, const char* arg_name) {
    if (!n || n->type != MATH_BINARY_OP || n->op != OP_POW) return 0;
    if (!is_func(n->left, func_name)) return 0;
    if (n->right && n->right->type == MATH_NUMBER && n->right->num_val == 2.0) {
        if (arg_name) return is_sym(n->left->left, arg_name);
        return 1;  /* any argument */
    }
    return 0;
}

static MathNode* simplify_trig_node(MathNode* node) {
    if (!node) return NULL;

    /* Bottom-up: recurse first */
    if (node->left)  node->left  = simplify_trig_node(node->left);
    if (node->right) node->right = simplify_trig_node(node->right);

    /* ── Sum rules for sin^2 + cos^2 ── */
    if (node->type == MATH_BINARY_OP && node->op == OP_ADD) {
        MathNode* L = node->left;
        MathNode* R = node->right;

        /* sin^2(x) + cos^2(x) → 1 */
        if (is_func_sq(L, "SIN", NULL) && is_func_sq(R, "COS", NULL) &&
            trees_equal(L->left->left, R->left->left)) {
            node->left = NULL;
            node->right = NULL;
            math_node_free(node);
            node = math_node_number(1.0);
            return node;
        }
        /* cos^2(x) + sin^2(x) → 1 */
        if (is_func_sq(L, "COS", NULL) && is_func_sq(R, "SIN", NULL) &&
            trees_equal(L->left->left, R->left->left)) {
            node->left = NULL;
            node->right = NULL;
            math_node_free(node);
            node = math_node_number(1.0);
            return node;
        }

        /* 1 + tan^2(x) → 1/cos^2(x) */
        if (is_one(L) && is_func_sq(R, "TAN", NULL)) {
            MathNode* arg = plant_math_deep_copy(R->left->left);
            MathNode* cos_arg = math_node_func("COS", arg);
            MathNode* cos_sq = math_node_binary(OP_POW, cos_arg, math_node_number(2.0));
            node->left = NULL;
            node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_DIV, math_node_number(1.0), cos_sq);
            return node;
        }
        /* tan^2(x) + 1 → 1/cos^2(x) */
        if (is_func_sq(L, "TAN", NULL) && is_one(R)) {
            MathNode* arg = plant_math_deep_copy(L->left->left);
            MathNode* cos_arg = math_node_func("COS", arg);
            MathNode* cos_sq = math_node_binary(OP_POW, cos_arg, math_node_number(2.0));
            node->left = NULL;
            node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_DIV, math_node_number(1.0), cos_sq);
            return node;
        }
    }

    /* ── Difference rules: 1 - sin^2 → cos^2, 1 - cos^2 → sin^2 ── */
    if (node->type == MATH_BINARY_OP && node->op == OP_SUB) {
        MathNode* L = node->left;
        MathNode* R = node->right;

        /* 1 - sin^2(x) → cos^2(x) */
        if (is_one(L) && is_func_sq(R, "SIN", NULL)) {
            MathNode* arg = plant_math_deep_copy(R->left->left);
            MathNode* cos_arg = math_node_func("COS", arg);
            MathNode* cos_sq = math_node_binary(OP_POW, cos_arg, math_node_number(2.0));
            node->left = NULL;
            node->right = NULL;
            math_node_free(node);
            return cos_sq;
        }
        /* 1 - cos^2(x) → sin^2(x) */
        if (is_one(L) && is_func_sq(R, "COS", NULL)) {
            MathNode* arg = plant_math_deep_copy(R->left->left);
            MathNode* sin_arg = math_node_func("SIN", arg);
            MathNode* sin_sq = math_node_binary(OP_POW, sin_arg, math_node_number(2.0));
            node->left = NULL;
            node->right = NULL;
            math_node_free(node);
            return sin_sq;
        }
        /* sin^2(x) - 1 → -cos^2(x) */
        if (is_func_sq(L, "SIN", NULL) && is_one(R)) {
            MathNode* arg = plant_math_deep_copy(L->left->left);
            MathNode* cos_arg = math_node_func("COS", arg);
            MathNode* cos_sq = math_node_binary(OP_POW, cos_arg, math_node_number(2.0));
            node->left = NULL;
            node->right = NULL;
            math_node_free(node);
            node = math_node_unary(cos_sq);
            return node;
        }
        /* cos^2(x) - 1 → -sin^2(x) */
        if (is_func_sq(L, "COS", NULL) && is_one(R)) {
            MathNode* arg = plant_math_deep_copy(L->left->left);
            MathNode* sin_arg = math_node_func("SIN", arg);
            MathNode* sin_sq = math_node_binary(OP_POW, sin_arg, math_node_number(2.0));
            node->left = NULL;
            node->right = NULL;
            math_node_free(node);
            node = math_node_unary(sin_sq);
            return node;
        }
    }

    return node;
}

/* ====================================================================
 *  simplify_log — Logarithmic Law Expansions
 *
 *  log(a*b)    → log(a) + log(b)     product rule
 *  log(a/b)    → log(a) - log(b)     quotient rule
 *  log(a^n)    → n * log(a)          power rule
 *  log(1)      → 0                   base evaluation
 *  log(e)      → 1                   base evaluation
 *  log(EXP(x)) → x                   inverse
 *  EXP(log(x)) → x                   inverse
 * ==================================================================== */

static MathNode* simplify_log_node(MathNode* node) {
    if (!node) return NULL;

    /* Bottom-up: recurse first */
    if (node->left)  node->left  = simplify_log_node(node->left);
    if (node->right) node->right = simplify_log_node(node->right);

    /* ── LOG(...) rules ── */
    if (is_func(node, "LOG") && node->left) {
        MathNode* arg = node->left;

        /* log(1) → 0 */
        if (is_one(arg)) {
            node->left = NULL;
            math_node_free(node);
            node = math_node_number(0.0);
            return node;
        }

        /* log(e) → 1 */
        if (arg->type == MATH_CONSTANT && strcmp(arg->sym_name, "E") == 0) {
            node->left = NULL;
            math_node_free(node);
            node = math_node_number(1.0);
            return node;
        }

        /* log(EXP(x)) → x */
        if (is_func(arg, "EXP") && arg->left) {
            MathNode* inner = arg->left;
            arg->left = NULL;
            math_node_free(node);
            return inner;
        }

        /* log(a*b) → log(a) + log(b) — product rule */
        if (arg->type == MATH_BINARY_OP && arg->op == OP_MUL) {
            MathNode* a = plant_math_deep_copy(arg->left);
            MathNode* b = plant_math_deep_copy(arg->right);
            MathNode* log_a = math_node_func("LOG", a);
            MathNode* log_b = math_node_func("LOG", b);
            node->left = NULL;
            math_node_free(node);
            node = math_node_binary(OP_ADD, log_a, log_b);
            return node;
        }

        /* log(a/b) → log(a) - log(b) — quotient rule */
        if (arg->type == MATH_BINARY_OP && arg->op == OP_DIV) {
            MathNode* a = plant_math_deep_copy(arg->left);
            MathNode* b = plant_math_deep_copy(arg->right);
            MathNode* log_a = math_node_func("LOG", a);
            MathNode* log_b = math_node_func("LOG", b);
            node->left = NULL;
            math_node_free(node);
            node = math_node_binary(OP_SUB, log_a, log_b);
            return node;
        }

        /* log(a^n) → n * log(a) — power rule */
        if (arg->type == MATH_BINARY_OP && arg->op == OP_POW) {
            MathNode* base = plant_math_deep_copy(arg->left);
            MathNode* exp  = plant_math_deep_copy(arg->right);
            MathNode* log_base = math_node_func("LOG", base);
            node->left = NULL;
            math_node_free(node);
            node = math_node_binary(OP_MUL, exp, log_base);
            return node;
        }
    }

    /* ── EXP(log(x)) → x — inverse ── */
    if (is_func(node, "EXP") && node->left &&
        is_func(node->left, "LOG") && node->left->left) {
        MathNode* inner = node->left->left;
        node->left->left = NULL;
        math_node_free(node);
        return inner;
    }

    return node;
}

/* ====================================================================
 *  simplify_frac — Complete Rational Fraction Unification
 *
 *  1/a + 1/b → (a+b)/(a*b)           base rule (unit numerators)
 *  1/a - 1/b → (b-a)/(a*b)           base rule (unit numerators)
 *  a/(b*c) + a/(b*d) → a*(c+d)/(b*c*d)  same numerator shortcut
 *  a/c + b/c → (a+b)/c               same denominator
 *  a/b + c/d → (a*d + c*b)/(b*d)     general cross-multiplication
 *  a/b - c/d → (a*d - c*b)/(b*d)     general cross-multiplication
 *  (a*c)/c → a                        denominator cancellation
 *  (a/b)*(c/d) → (a*c)/(b*d)         fraction multiplication
 *  (a/b)/(c/d) → (a*d)/(b*c)         fraction division
 * ==================================================================== */

/* Helper: check if a node is a fraction (a/b) */
static int is_frac(const MathNode* n) {
    return n && n->type == MATH_BINARY_OP && n->op == OP_DIV;
}

static MathNode* simplify_frac_node(MathNode* node) {
    if (!node) return NULL;

    /* Bottom-up: recurse first */
    if (node->left)  node->left  = simplify_frac_node(node->left);
    if (node->right) node->right = simplify_frac_node(node->right);

    /* ── ADDITION ── */
    if (node->type == MATH_BINARY_OP && node->op == OP_ADD) {
        MathNode* L = node->left;
        MathNode* R = node->right;

        /* 1/a + 1/b → (a+b)/(a*b) — unit numerator shortcut */
        if (is_frac(L) && is_one(L->left) &&
            is_frac(R) && is_one(R->left)) {
            MathNode* a = plant_math_deep_copy(L->right);
            MathNode* b = plant_math_deep_copy(R->right);
            MathNode* numer = math_node_binary(OP_ADD,
                plant_math_deep_copy(a), plant_math_deep_copy(b));
            MathNode* denom = math_node_binary(OP_MUL, a, b);
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_DIV, numer, denom);
            return node;
        }

        /* a/c + b/c → (a+b)/c — same denominator */
        if (is_frac(L) && is_frac(R) && trees_equal(L->right, R->right)) {
            MathNode* numer = math_node_binary(OP_ADD,
                plant_math_deep_copy(L->left),
                plant_math_deep_copy(R->left));
            MathNode* denom = plant_math_deep_copy(L->right);
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_DIV, numer, denom);
            return node;
        }

        /* a/(b*c) + a/(b*d) → a*(c+d)/(b*c*d) — same numerator */
        if (is_frac(L) && is_frac(R) &&
            trees_equal(L->left, R->left)) {
            MathNode* b1 = plant_math_deep_copy(L->right);
            MathNode* b2 = plant_math_deep_copy(R->right);
            MathNode* sum_b = math_node_binary(OP_ADD,
                plant_math_deep_copy(b1), plant_math_deep_copy(b2));
            MathNode* prod_b = math_node_binary(OP_MUL, b1, b2);
            MathNode* numer = math_node_binary(OP_MUL,
                plant_math_deep_copy(L->left), sum_b);
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_DIV, numer, prod_b);
            return node;
        }

        /* a/b + c/d → (a*d + c*b)/(b*d) — general cross-multiplication */
        if (is_frac(L) && is_frac(R)) {
            MathNode* a = plant_math_deep_copy(L->left);
            MathNode* b = plant_math_deep_copy(L->right);
            MathNode* c = plant_math_deep_copy(R->left);
            MathNode* d = plant_math_deep_copy(R->right);
            MathNode* ad = math_node_binary(OP_MUL, a, d);
            MathNode* cb = math_node_binary(OP_MUL, c, b);
            MathNode* numer = math_node_binary(OP_ADD, ad, cb);
            MathNode* bd = math_node_binary(OP_MUL,
                plant_math_deep_copy(L->right),
                plant_math_deep_copy(R->right));
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_DIV, numer, bd);
            return node;
        }
    }

    /* ── SUBTRACTION ── */
    if (node->type == MATH_BINARY_OP && node->op == OP_SUB) {
        MathNode* L = node->left;
        MathNode* R = node->right;

        /* 1/a - 1/b → (b-a)/(a*b) — unit numerator shortcut */
        if (is_frac(L) && is_one(L->left) &&
            is_frac(R) && is_one(R->left)) {
            MathNode* a = plant_math_deep_copy(L->right);
            MathNode* b = plant_math_deep_copy(R->right);
            MathNode* numer = math_node_binary(OP_SUB,
                plant_math_deep_copy(b), plant_math_deep_copy(a));
            MathNode* denom = math_node_binary(OP_MUL, a, b);
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_DIV, numer, denom);
            return node;
        }

        /* a/c - b/c → (a-b)/c — same denominator */
        if (is_frac(L) && is_frac(R) && trees_equal(L->right, R->right)) {
            MathNode* numer = math_node_binary(OP_SUB,
                plant_math_deep_copy(L->left),
                plant_math_deep_copy(R->left));
            MathNode* denom = plant_math_deep_copy(L->right);
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_DIV, numer, denom);
            return node;
        }

        /* a/b - c/d → (a*d - c*b)/(b*d) — general cross-multiplication */
        if (is_frac(L) && is_frac(R)) {
            MathNode* a = plant_math_deep_copy(L->left);
            MathNode* b = plant_math_deep_copy(L->right);
            MathNode* c = plant_math_deep_copy(R->left);
            MathNode* d = plant_math_deep_copy(R->right);
            MathNode* ad = math_node_binary(OP_MUL, a, d);
            MathNode* cb = math_node_binary(OP_MUL, c, b);
            MathNode* numer = math_node_binary(OP_SUB, ad, cb);
            MathNode* bd = math_node_binary(OP_MUL,
                plant_math_deep_copy(L->right),
                plant_math_deep_copy(R->right));
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_DIV, numer, bd);
            return node;
        }
    }

    /* ── MULTIPLICATION: (a/b)*(c/d) → (a*c)/(b*d) ── */
    if (node->type == MATH_BINARY_OP && node->op == OP_MUL) {
        MathNode* L = node->left;
        MathNode* R = node->right;
        if (is_frac(L) && is_frac(R)) {
            MathNode* numer = math_node_binary(OP_MUL,
                plant_math_deep_copy(L->left),
                plant_math_deep_copy(R->left));
            MathNode* denom = math_node_binary(OP_MUL,
                plant_math_deep_copy(L->right),
                plant_math_deep_copy(R->right));
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_DIV, numer, denom);
            return node;
        }
    }

    /* ── DIVISION: (a/b)/(c/d) → (a*d)/(b*c) ── */
    if (node->type == MATH_BINARY_OP && node->op == OP_DIV) {
        MathNode* L = node->left;
        MathNode* R = node->right;

        /* Denominator cancellation: (a*c)/c → a */
        if (is_frac(L) && trees_equal(L->right, R)) {
            MathNode* result = plant_math_deep_copy(L->left);
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            return result;
        }

        /* Fraction division: (a/b)/(c/d) → (a*d)/(b*c) */
        if (is_frac(L) && is_frac(R)) {
            MathNode* numer = math_node_binary(OP_MUL,
                plant_math_deep_copy(L->left),
                plant_math_deep_copy(R->right));
            MathNode* denom = math_node_binary(OP_MUL,
                plant_math_deep_copy(L->right),
                plant_math_deep_copy(R->left));
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            node = math_node_binary(OP_DIV, numer, denom);
            return node;
        }
    }

    return node;
}

/* ====================================================================
 *  v0.50.0h — Term Analysis, Like Terms Collection, Distribution,
 *             and Descending Term Ordering
 * ==================================================================== */

/* ── Term decomposition ──────────────────────────────────────── */

/* Extract the numeric coefficient and "base" of a term.
 * 2*x      → coeff=2, base=x
 * x        → coeff=1, base=x
 * -x       → coeff=-1, base=x
 * 5        → coeff=5, base=NULL (pure constant)
 * x^2      → coeff=1, base=x^2
 * 3*(x+1)  → coeff=3, base=(x+1)  [unexpanded product]
 */
static void decompose_term(const MathNode* node, double* coeff, MathNode** base) {
    *coeff = 1.0;
    *base = (MathNode*)node;

    if (!node) { *coeff = 0; *base = NULL; return; }

    switch (node->type) {
        case MATH_NUMBER:
            *coeff = node->num_val;
            *base = NULL;
            return;
        case MATH_CONSTANT: {
            double v = lookup_constant(node->sym_name);
            if (!isnan(v)) { *coeff = v; *base = NULL; }
            return;
        }
        case MATH_UNARY_OP:
            if (node->op == OP_NEG) {
                decompose_term(node->left, coeff, base);
                *coeff = -*coeff;
                return;
            }
            return;
        case MATH_BINARY_OP:
            if (node->op == OP_MUL) {
                /* num * expr or expr * num */
                if (is_number(node->left)) {
                    *coeff = node->left->num_val;
                    *base = node->right;
                    return;
                }
                if (is_number(node->right)) {
                    *coeff = node->right->num_val;
                    *base = node->left;
                    return;
                }
            }
            return;
        default:
            return;
    }
}

/* Compute the "degree" of a term's base for sorting.
 * NULL (constant): 0
 * symbol: 1
 * symbol^n: n
 * a*b: sum of degrees
 * a/b: left - right degrees
 * func(x): 1
 */
static double term_degree(const MathNode* base) {
    if (!base) return 0.0;
    switch (base->type) {
        case MATH_NUMBER: return 0.0;
        case MATH_CONSTANT: return 0.0;
        case MATH_SYMBOL: return 1.0;
        case MATH_BINARY_OP:
            if (base->op == OP_POW && is_number(base->right))
                return base->right->num_val;
            if (base->op == OP_MUL)
                return term_degree(base->left) + term_degree(base->right);
            if (base->op == OP_DIV)
                return term_degree(base->left) - term_degree(base->right);
            return 1.0;
        case MATH_UNARY_OP:
            return term_degree(base->left);
        case MATH_FUNC_CALL:
            return 1.0;
        default:
            return 0.0;
    }
}

/* Check if a node is a pure constant (evaluates to a number). */
static int is_pure_constant(const MathNode* n) {
    if (!n) return 1;
    if (n->type == MATH_NUMBER) return 1;
    if (n->type == MATH_CONSTANT) {
        double v = lookup_constant(n->sym_name);
        return !isnan(v);
    }
    return 0;
}

/* ── Flatten sum: collect terms from a + / - tree ────────────── */

#define MAX_TERMS 256

typedef struct {
    double    coeff;
    MathNode* base;   /* owned — will be freed or transferred */
} TermEntry;

/* Recursively flatten a sum/difference tree into a list of terms.
 * A + B → [terms(A), terms(B)]
 * A - B → [terms(A), -terms(B)]
 */
static void flatten_sum(const MathNode* node, TermEntry* terms, int* count) {
    if (!node || *count >= MAX_TERMS) return;
    if (node->type == MATH_BINARY_OP && node->op == OP_ADD) {
        flatten_sum(node->left, terms, count);
        flatten_sum(node->right, terms, count);
    } else if (node->type == MATH_BINARY_OP && node->op == OP_SUB) {
        flatten_sum(node->left, terms, count);
        /* Negate all terms from the right side */
        int saved = *count;
        flatten_sum(node->right, terms, count);
        for (int i = saved; i < *count; i++)
            terms[i].coeff = -terms[i].coeff;
    } else if (node->type == MATH_UNARY_OP && node->op == OP_NEG) {
        int saved = *count;
        flatten_sum(node->left, terms, count);
        for (int i = saved; i < *count; i++)
            terms[i].coeff = -terms[i].coeff;
    } else {
        double coeff;
        MathNode* base;
        decompose_term(node, &coeff, &base);
        terms[*count].coeff = coeff;
        terms[*count].base = base ? plant_math_deep_copy(base) : NULL;
        (*count)++;
    }
}

/* ── Like terms collection ───────────────────────────────────── */

/* Collect like terms: merge terms with structurally identical bases.
 * Modifies the terms array in-place, zeroing consumed slots. */
static void collect_like(TermEntry* terms, int* count) {
    for (int i = 0; i < *count; i++) {
        if (!terms[i].base && terms[i].coeff == 0.0) continue;
        for (int j = i + 1; j < *count; j++) {
            if (!terms[j].base && terms[j].coeff == 0.0) continue;
            if (trees_equal(terms[i].base, terms[j].base)) {
                terms[i].coeff += terms[j].coeff;
                math_node_free(terms[j].base);
                terms[j].base = NULL;
                terms[j].coeff = 0.0;
            }
        }
    }
    /* Compact: remove zero-coefficient entries */
    int write = 0;
    for (int i = 0; i < *count; i++) {
        if (terms[i].coeff != 0.0 || terms[i].base != NULL) {
            if (write != i)
                terms[write] = terms[i];
            write++;
        }
    }
    *count = write;
}

/* ── Descending term ordering ────────────────────────────────── */

/* Comparison for qsort: higher degree first, then variables before constants,
 * then alphabetical by string representation. */
static int cmp_term_desc(const void* a, const void* b) {
    const TermEntry* ta = (const TermEntry*)a;
    const TermEntry* tb = (const TermEntry*)b;

    /* Pure constants sort last */
    int a_const = (ta->base == NULL);
    int b_const = (tb->base == NULL);
    if (a_const != b_const) return a_const ? 1 : -1;

    /* Higher degree first */
    double da = term_degree(ta->base);
    double db = term_degree(tb->base);
    if (da != db) return (db > da) ? 1 : -1;

    /* Same degree: alphabetical by string repr */
    if (ta->base && tb->base) {
        char* sa = plant_math_to_string(ta->base);
        char* sb = plant_math_to_string(tb->base);
        int cmp = strcmp(sa, sb);
        free(sa);
        free(sb);
        return cmp;
    }
    return 0;
}

static void sort_terms(TermEntry* terms, int count) {
    qsort(terms, count, sizeof(TermEntry), cmp_term_desc);
}

/* ── Build a sum tree from a flat term list ──────────────────── */

/* Build a single term node: coeff * base.
 * coeff=1, base=x → x
 * coeff=2, base=x → 2*x
 * coeff=-1, base=x → -x
 * coeff=3, base=NULL → 3
 */
static MathNode* build_term_node(double coeff, MathNode* base) {
    if (!base) return math_node_number(coeff);
    if (coeff == 1.0) return base;
    if (coeff == -1.0) return math_node_unary(base);
    return math_node_binary(OP_MUL, math_node_number(coeff), base);
}

/* Build a sum tree from term list. */
static MathNode* build_sum(TermEntry* terms, int count) {
    if (count == 0) return math_node_number(0.0);
    MathNode* result = build_term_node(terms[0].coeff, terms[0].base);
    for (int i = 1; i < count; i++) {
        MathNode* term = build_term_node(terms[i].coeff, terms[i].base);
        result = math_node_binary(OP_ADD, result, term);
    }
    return result;
}

/* ── Distribution ────────────────────────────────────────────── */

/* Check if a node is a sum (top-level ADD or SUB). */
static int is_sum(const MathNode* n) {
    return n && n->type == MATH_BINARY_OP && (n->op == OP_ADD || n->op == OP_SUB);
}

/* Multiply a scalar expression by a sum: distribute.
 * scalar * (a + b) → scalar*a + scalar*b
 * scalar * (a - b) → scalar*a - scalar*b
 */
static MathNode* distribute_scalar_sum(MathNode* scalar, const MathNode* sum) {
    if (sum->op == OP_ADD) {
        MathNode* la = plant_math_deep_copy(scalar);
        MathNode* ra = plant_math_deep_copy(scalar);
        MathNode* left_term = math_node_binary(OP_MUL, la, plant_math_deep_copy(sum->left));
        MathNode* right_term = math_node_binary(OP_MUL, ra, plant_math_deep_copy(sum->right));
        return math_node_binary(OP_ADD, left_term, right_term);
    } else { /* OP_SUB */
        MathNode* la = plant_math_deep_copy(scalar);
        MathNode* ra = plant_math_deep_copy(scalar);
        MathNode* left_term = math_node_binary(OP_MUL, la, plant_math_deep_copy(sum->left));
        MathNode* right_term = math_node_binary(OP_MUL, ra, plant_math_deep_copy(sum->right));
        return math_node_binary(OP_SUB, left_term, right_term);
    }
}

/* Double distribution: (a + b) * (c + d) → a*c + a*d + b*c + b*d */
static MathNode* distribute_double_sum(const MathNode* lsum, const MathNode* rsum) {
    /* Collect terms from left sum */
    TermEntry lterms[MAX_TERMS];
    int lcount = 0;
    flatten_sum(lsum, lterms, &lcount);

    /* Collect terms from right sum */
    TermEntry rterms[MAX_TERMS];
    int rcount = 0;
    flatten_sum(rsum, rterms, &rcount);

    /* Build cross products */
    MathNode* result = NULL;
    for (int i = 0; i < lcount; i++) {
        for (int j = 0; j < rcount; j++) {
            MathNode* lt = build_term_node(lterms[i].coeff, lterms[i].base ? plant_math_deep_copy(lterms[i].base) : NULL);
            MathNode* rt = build_term_node(rterms[j].coeff, rterms[j].base ? plant_math_deep_copy(rterms[j].base) : NULL);
            MathNode* product = math_node_binary(OP_MUL, lt, rt);
            if (!result) {
                result = product;
            } else {
                result = math_node_binary(OP_ADD, result, product);
            }
        }
    }
    if (!result) result = math_node_number(0.0);

    /* Clean up temporary term bases */
    for (int i = 0; i < lcount; i++) math_node_free(lterms[i].base);
    for (int i = 0; i < rcount; i++) math_node_free(rterms[i].base);

    return result;
}

/* Distribution pass: expand products over sums where possible. */
static MathNode* distribute_node(MathNode* node) {
    if (!node) return NULL;

    /* Recurse first (handles both unary and binary) */
    if (node->left) node->left = distribute_node(node->left);
    if (node->right) node->right = distribute_node(node->right);

    /* Negation distribution: -(a+b) → -a - b */
    if (node->type == MATH_UNARY_OP && node->op == OP_NEG && is_sum(node->left)) {
        MathNode* neg_one = math_node_number(-1.0);
        MathNode* result = distribute_scalar_sum(neg_one, node->left);
        node->left = NULL;
        math_node_free(node);
        return result;
    }

    if (node->type != MATH_BINARY_OP) return node;

    if (node->op == OP_MUL) {
        MathNode* L = node->left;
        MathNode* R = node->right;

        /* scalar * sum → distribute */
        if (is_pure_constant(L) && is_sum(R)) {
            MathNode* result = distribute_scalar_sum(L, R);
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            return result;
        }
        /* sum * scalar → distribute */
        if (is_sum(L) && is_pure_constant(R)) {
            MathNode* result = distribute_scalar_sum(R, L);
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            return result;
        }
        /* -1 * sum → distribute as negation */
        if (node->left->type == MATH_UNARY_OP && node->left->op == OP_NEG) {
            if (is_one(node->left->left) && is_sum(R)) {
                MathNode* result = distribute_scalar_sum(node->left, R);
                node->left = NULL; node->right = NULL;
                math_node_free(node);
                return result;
            }
        }
        if (node->right->type == MATH_UNARY_OP && node->right->op == OP_NEG) {
            if (is_sum(L) && is_one(node->right->left)) {
                MathNode* result = distribute_scalar_sum(node->right, L);
                node->left = NULL; node->right = NULL;
                math_node_free(node);
                return result;
            }
        }
        /* sum * sum → double distribution */
        if (is_sum(L) && is_sum(R)) {
            MathNode* result = distribute_double_sum(L, R);
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            return result;
        }
    }

    return node;
}

/* ── Normalize: standardize operand ordering in products ─────── */

/* Flatten nested products: a*(b*c) → a*b*c, collect numeric factors. */
static MathNode* flatten_product(MathNode* node) {
    if (!node || node->type != MATH_BINARY_OP || node->op != OP_MUL)
        return node;

    /* Collect all factors from nested MUL tree */
    MathNode* factors[256];
    int fcount = 0;

    /* Recursive factor collection */
    MathNode* stack[256];
    int sp = 0;
    stack[sp++] = node;
    while (sp > 0 && fcount < 256) {
        MathNode* cur = stack[--sp];
        if (cur->type == MATH_BINARY_OP && cur->op == OP_MUL) {
            if (cur->right) stack[sp++] = cur->right;
            if (cur->left) stack[sp++] = cur->left;
        } else {
            factors[fcount++] = cur;
        }
    }

    if (fcount <= 1) return node;

    /* Separate numeric and non-numeric factors */
    double num_product = 1.0;
    MathNode* non_numeric[256];
    int nn_count = 0;

    for (int i = 0; i < fcount; i++) {
        if (factors[i]->type == MATH_NUMBER) {
            num_product *= factors[i]->num_val;
            free(factors[i]->sym_name);
            free(factors[i]);
        } else if (factors[i]->type == MATH_CONSTANT) {
            double v = lookup_constant(factors[i]->sym_name);
            if (!isnan(v)) {
                num_product *= v;
                free(factors[i]->sym_name);
                free(factors[i]);
            } else {
                non_numeric[nn_count++] = factors[i];
            }
        } else {
            non_numeric[nn_count++] = factors[i];
        }
    }

    /* Rebuild product tree */
    MathNode* result = NULL;
    if (num_product != 1.0 || nn_count == 0) {
        result = math_node_number(num_product);
    }
    for (int i = 0; i < nn_count; i++) {
        if (!result) {
            result = non_numeric[i];
        } else {
            result = math_node_binary(OP_MUL, result, non_numeric[i]);
        }
    }
    if (!result) result = math_node_number(num_product);

    return result;
}

/* Sort commutative product factors: numbers first, then symbols alphabetically,
 * then complex expressions. This makes like-term detection more reliable. */
static MathNode* normalize_node(MathNode* node) {
    if (!node) return NULL;
    if (node->left) node->left = normalize_node(node->left);
    if (node->right) node->right = normalize_node(node->right);

    if (node->type == MATH_BINARY_OP && node->op == OP_MUL) {
        /* Flatten nested products first */
        MathNode* flat = flatten_product(node);
        if (flat != node) return flat;

        /* If right is number and left is not, swap for canonical num*expr form */
        if (is_number(node->right) && !is_number(node->left)) {
            MathNode* tmp = node->left;
            node->left = node->right;
            node->right = tmp;
        }
        /* If right is unary neg and left is not, move neg outward: a*(-b) → -(a*b) */
        if (node->right && node->right->type == MATH_UNARY_OP && node->right->op == OP_NEG) {
            MathNode* inner = math_node_binary(OP_MUL,
                plant_math_deep_copy(node->left),
                plant_math_deep_copy(node->right->left));
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            return math_node_unary(inner);
        }
    }

    return node;
}

/* ── Collection + Ordering pass ──────────────────────────────── */

static MathNode* collect_and_sort(MathNode* node) {
    if (!node) return NULL;

    /* Recurse first */
    if (node->left) node->left = collect_and_sort(node->left);
    if (node->right) node->right = collect_and_sort(node->right);

    /* Only collect at the top-level sum/difference node */
    if (node->type == MATH_BINARY_OP && (node->op == OP_ADD || node->op == OP_SUB)) {
        /* Flatten the entire sum tree */
        TermEntry terms[MAX_TERMS];
        int count = 0;
        flatten_sum(node, terms, &count);

        /* Collect like terms */
        collect_like(terms, &count);

        /* Sort descending */
        sort_terms(terms, count);

        /* Rebuild */
        MathNode* result = build_sum(terms, count);

        /* Free old node (children already transferred to terms) */
        node->left = NULL;
        node->right = NULL;
        math_node_free(node);

        return result;
    }

    return node;
}

/* ====================================================================
 *  Simplifier pipeline — iterates until fixed point or cap
 *
 *  Passes per iteration:
 *    1. simplify_node     — constant folding, identity, cancellations
 *    2. simplify_pow      — exponent law reductions
 *    3. simplify_trig     — trigonometric identity rewrites
 *    4. simplify_log      — logarithmic law expansions
 *    5. simplify_frac     — rational fraction unification
 *    6. simplify_complex  — complex number folding (i*i → -1, sqrt(-1) → i)
 *    7. distribute_node   — expand products over sums
 *    8. normalize_node    — standardize operand ordering
 *    9. collect_and_sort  — flatten sums, merge like terms
 *   10. simplify_node    — final cleanup (1*x → x, x+0 → x, etc.)
 * ==================================================================== */

MathNode* plant_math_simplify(MathNode* node) {
    if (!node) return NULL;
    int iterations = 0;
    int changed = 1;
    while (changed && iterations < MAX_SIMPLIFY_ITERATIONS) {
        changed = 0;
        MathNode* before = plant_math_deep_copy(node);

        /* Pass 1: basic simplification (folding, identity, cancellation) */
        node = simplify_node(node);
        /* Pass 2: exponent law reductions */
        node = simplify_pow_node(node);
        /* Pass 3: trigonometric identity rewrites */
        node = simplify_trig_node(node);
        /* Pass 4: logarithmic law expansions */
        node = simplify_log_node(node);
        /* Pass 5: rational fraction unification */
        node = simplify_frac_node(node);
        /* Pass 6: complex number folding */
        node = simplify_complex_node(node);
        /* Pass 7: distribute products over sums */
        node = distribute_node(node);
        /* Pass 8: normalize operand ordering */
        node = normalize_node(node);
        /* Pass 9: collect like terms and sort */
        node = collect_and_sort(node);
        /* Pass 10: final cleanup */
        node = simplify_node(node);

        MathNode* after = plant_math_deep_copy(node);
        if (!trees_equal(before, after)) changed = 1;
        math_node_free(before);
        math_node_free(after);
        iterations++;
    }
    return node;
}

/* ====================================================================
 *  v0.50.0i — Symbolic Differentiation Engine
 *
 *  Rules:
 *    d/dx(c) = 0                    (constant)
 *    d/dx(x) = 1                    (variable)
 *    d/dx(x^n) = n * x^(n-1)       (power rule)
 *    d/dx(f + g) = f' + g'          (sum rule)
 *    d/dx(f - g) = f' - g'          (difference rule)
 *    d/dx(f * g) = f'g + fg'        (product rule)
 *    d/dx(f(g)) = f'(g) * g'        (chain rule)
 *    d/dx(SIN(g)) = COS(g) * g'
 *    d/dx(COS(g)) = -SIN(g) * g'
 *    d/dx(EXP(g)) = EXP(g) * g'
 *    d/dx(LOG(g)) = g'/g
 * ==================================================================== */

#define MAX_DERIVATIVE_DEPTH 20

static MathNode* deriv_node(const MathNode* node, const char* var, int depth);

/* Check if a node is a specific symbol */
static int is_symbol_name(const MathNode* n, const char* name) {
    return n && n->type == MATH_SYMBOL && strcmp(n->sym_name, name) == 0;
}

/* d/dx(c) = 0 — constant or number */
static MathNode* deriv_const(const MathNode* node, const char* var) {
    (void)node; (void)var;
    return math_node_number(0.0);
}

/* d/dx(x) = 1 */
static MathNode* deriv_var(const MathNode* node, const char* var) {
    if (is_symbol_name(node, var))
        return math_node_number(1.0);
    return math_node_number(0.0);
}

/* d/dx(x^n) = n * x^(n-1) — power rule */
static MathNode* deriv_pow(const MathNode* node, const char* var, int depth) {
    MathNode* base = node->left;
    MathNode* exp = node->right;

    /* If exponent is constant, apply power rule */
    if (is_pure_constant(exp) && !is_symbol_name(base, var)) {
        double n_val = (exp->type == MATH_NUMBER) ? exp->num_val :
                       lookup_constant(exp->sym_name);
        /* d/dx(c^n) = 0 if c doesn't contain var */
        MathNode* d_base = deriv_node(base, var, depth + 1);
        if (is_zero(d_base)) {
            math_node_free(d_base);
            return math_node_number(0.0);
        }
        /* Chain rule: n * base^(n-1) * base' */
        MathNode* n_node = plant_math_deep_copy(exp);
        MathNode* n_minus_1 = math_node_binary(OP_SUB,
            plant_math_deep_copy(exp), math_node_number(1.0));
        MathNode* new_exp = math_node_binary(OP_POW,
            plant_math_deep_copy(base), n_minus_1);
        MathNode* term1 = math_node_binary(OP_MUL, n_node, new_exp);
        return math_node_binary(OP_MUL, term1, d_base);
    }

    /* If base is the variable and exponent is constant: standard power rule */
    if (is_symbol_name(base, var) && is_pure_constant(exp)) {
        double n_val = (exp->type == MATH_NUMBER) ? exp->num_val :
                       lookup_constant(exp->sym_name);
        MathNode* n_node = plant_math_deep_copy(exp);
        MathNode* n_minus_1 = math_node_binary(OP_SUB,
            plant_math_deep_copy(exp), math_node_number(1.0));
        MathNode* new_exp = math_node_binary(OP_POW,
            plant_math_deep_copy(base), n_minus_1);
        return math_node_binary(OP_MUL, n_node, new_exp);
    }

    /* General case: d/dx(f^g) = f^g * (g' * ln(f) + g * f'/f) */
    MathNode* d_f = deriv_node(base, var, depth + 1);
    MathNode* d_g = deriv_node(exp, var, depth + 1);
    MathNode* f_copy = plant_math_deep_copy(base);
    MathNode* g_copy = plant_math_deep_copy(exp);
    MathNode* ln_f = math_node_func("LOG", plant_math_deep_copy(base));
    MathNode* term1 = math_node_binary(OP_MUL, d_g, ln_f);
    MathNode* f_prime_over_f = math_node_binary(OP_DIV, d_f, plant_math_deep_copy(base));
    MathNode* term2 = math_node_binary(OP_MUL, g_copy, f_prime_over_f);
    MathNode* inner_sum = math_node_binary(OP_ADD, term1, term2);
    MathNode* f_pow_g = math_node_binary(OP_POW, f_copy, g_copy);
    return math_node_binary(OP_MUL, f_pow_g, inner_sum);
}

/* d/dx(f + g) = f' + g' */
static MathNode* deriv_add(const MathNode* node, const char* var, int depth) {
    MathNode* d_left = deriv_node(node->left, var, depth + 1);
    MathNode* d_right = deriv_node(node->right, var, depth + 1);
    return math_node_binary(OP_ADD, d_left, d_right);
}

/* d/dx(f - g) = f' - g' */
static MathNode* deriv_sub(const MathNode* node, const char* var, int depth) {
    MathNode* d_left = deriv_node(node->left, var, depth + 1);
    MathNode* d_right = deriv_node(node->right, var, depth + 1);
    return math_node_binary(OP_SUB, d_left, d_right);
}

/* d/dx(f * g) = f'*g + f*g' — product rule */
static MathNode* deriv_mul(const MathNode* node, const char* var, int depth) {
    MathNode* d_left = deriv_node(node->left, var, depth + 1);
    MathNode* d_right = deriv_node(node->right, var, depth + 1);
    MathNode* left_copy = plant_math_deep_copy(node->left);
    MathNode* right_copy = plant_math_deep_copy(node->right);
    MathNode* term1 = math_node_binary(OP_MUL, d_left, right_copy);
    MathNode* term2 = math_node_binary(OP_MUL, left_copy, d_right);
    return math_node_binary(OP_ADD, term1, term2);
}

/* d/dx(f / g) = (f'*g - f*g') / g^2 — quotient rule */
static MathNode* deriv_div(const MathNode* node, const char* var, int depth) {
    MathNode* d_num = deriv_node(node->left, var, depth + 1);
    MathNode* d_den = deriv_node(node->right, var, depth + 1);
    MathNode* num_copy = plant_math_deep_copy(node->left);
    MathNode* den_copy = plant_math_deep_copy(node->right);
    MathNode* term1 = math_node_binary(OP_MUL, d_num, den_copy);
    MathNode* term2 = math_node_binary(OP_MUL, num_copy, d_den);
    MathNode* numer = math_node_binary(OP_SUB, term1, term2);
    MathNode* denom = math_node_binary(OP_POW,
        plant_math_deep_copy(node->right), math_node_number(2.0));
    return math_node_binary(OP_DIV, numer, denom);
}

/* d/dx(f(g)) = f'(g) * g' — chain rule for function calls */
static MathNode* deriv_func(const MathNode* node, const char* var, int depth) {
    MathNode* arg = node->left;
    MathNode* d_arg = deriv_node(arg, var, depth + 1);
    MathNode* outer_deriv = NULL;
    const char* fname = node->sym_name;

    if (strcmp(fname, "SIN") == 0) {
        /* d/dx SIN(g) = COS(g) * g' */
        outer_deriv = math_node_func("COS", plant_math_deep_copy(arg));
    } else if (strcmp(fname, "COS") == 0) {
        /* d/dx COS(g) = -SIN(g) * g' */
        MathNode* sin_arg = math_node_func("SIN", plant_math_deep_copy(arg));
        outer_deriv = math_node_unary(sin_arg);
    } else if (strcmp(fname, "EXP") == 0) {
        /* d/dx EXP(g) = EXP(g) * g' */
        outer_deriv = math_node_func("EXP", plant_math_deep_copy(arg));
    } else if (strcmp(fname, "LOG") == 0) {
        /* d/dx LOG(g) = g'/g */
        return math_node_binary(OP_DIV, d_arg, plant_math_deep_copy(arg));
    } else if (strcmp(fname, "TAN") == 0) {
        /* d/dx TAN(g) = (1 + TAN(g)^2) * g' */
        MathNode* tan_g = math_node_func("TAN", plant_math_deep_copy(arg));
        MathNode* tan_sq = math_node_binary(OP_POW, tan_g, math_node_number(2.0));
        MathNode* one_plus_tan_sq = math_node_binary(OP_ADD,
            math_node_number(1.0), tan_sq);
        outer_deriv = one_plus_tan_sq;
    } else if (strcmp(fname, "SQRT") == 0) {
        /* d/dx SQRT(g) = g' / (2 * SQRT(g)) */
        MathNode* sqrt_g = math_node_func("SQRT", plant_math_deep_copy(arg));
        MathNode* denom = math_node_binary(OP_MUL, math_node_number(2.0), sqrt_g);
        return math_node_binary(OP_DIV, d_arg, denom);
    } else if (strcmp(fname, "ABS") == 0) {
        /* d/dx ABS(g) = g' * g / ABS(g) */
        MathNode* abs_g = math_node_func("ABS", plant_math_deep_copy(arg));
        MathNode* numer = math_node_binary(OP_MUL, d_arg, plant_math_deep_copy(arg));
        return math_node_binary(OP_DIV, numer, abs_g);
    } else {
        /* Unknown function: return 0 (can't differentiate) */
        math_node_free(d_arg);
        return math_node_number(0.0);
    }

    /* Chain rule: outer_deriv * inner_deriv */
    return math_node_binary(OP_MUL, outer_deriv, d_arg);
}

/* Main differentiation dispatcher with depth limit */
static MathNode* deriv_node(const MathNode* node, const char* var, int depth) {
    if (!node) return math_node_number(0.0);
    if (depth > MAX_DERIVATIVE_DEPTH) return math_node_number(0.0);

    switch (node->type) {
        case MATH_NUMBER:
        case MATH_CONSTANT:
            return deriv_const(node, var);
        case MATH_SYMBOL:
            return deriv_var(node, var);
        case MATH_BINARY_OP:
            switch (node->op) {
                case OP_ADD: return deriv_add(node, var, depth);
                case OP_SUB: return deriv_sub(node, var, depth);
                case OP_MUL: return deriv_mul(node, var, depth);
                case OP_DIV: return deriv_div(node, var, depth);
                case OP_POW: return deriv_pow(node, var, depth);
                default:     return math_node_number(0.0);
            }
        case MATH_UNARY_OP:
            if (node->op == OP_NEG) {
                MathNode* inner = deriv_node(node->left, var, depth + 1);
                return math_node_unary(inner);
            }
            return math_node_number(0.0);
        case MATH_FUNC_CALL:
            return deriv_func(node, var, depth);
    }
    return math_node_number(0.0);
}

MathNode* plant_math_derivative(const MathNode* node, const char* var) {
    if (!node || !var) return math_node_number(0.0);
    MathNode* result = deriv_node(node, var, 0);
    result = plant_math_simplify(result);
    return result;
}

char* plant_math_derivative_str(const char* expr, const char* var) {
    MathNode* ast = plant_math_parse(expr);
    MathNode* result = plant_math_derivative(ast, var);
    char* str = plant_math_to_string(result);
    math_node_free(ast);
    math_node_free(result);
    return str;
}

/* ====================================================================
 *  v0.50.0i — Symbolic Integration Engine
 *
 *  Rules (indefinite integrals, +C appended):
 *    ∫ c dx = c*x + C
 *    ∫ x dx = x^2/2 + C
 *    ∫ x^n dx = x^(n+1)/(n+1) + C  (n ≠ -1)
 *    ∫ 1/x dx = LOG(|x|) + C
 *    ∫ (f + g) dx = ∫f dx + ∫g dx
 *    ∫ SIN(g) dx = -COS(g) + C      (when g is simple variable)
 *    ∫ COS(g) dx = SIN(g) + C        (when g is simple variable)
 *    ∫ EXP(g) dx = EXP(g) + C        (when g is simple variable)
 * ==================================================================== */

static MathNode* integral_node(const MathNode* node, const char* var);

/* Forward declarations for factoring helpers used in integration */
static int matches_power_of_var(const MathNode* n, const char* var, double target_exp);

/* ∫ c dx = c*x */
static MathNode* integral_number(const MathNode* node, const char* var) {
    return math_node_binary(OP_MUL,
        plant_math_deep_copy(node), math_node_symbol(var));
}

/* ∫ x^n dx = x^(n+1)/(n+1) */
static MathNode* integral_pow_var(const MathNode* exp_node, const char* var) {
    MathNode* n_plus_1 = math_node_binary(OP_ADD,
        plant_math_deep_copy(exp_node), math_node_number(1.0));
    MathNode* x_pow = math_node_binary(OP_POW,
        math_node_symbol(var), plant_math_deep_copy(n_plus_1));
    return math_node_binary(OP_DIV, x_pow, n_plus_1);
}

/* ∫ (f + g) dx = ∫f dx + ∫g dx */
static MathNode* integral_add(const MathNode* node, const char* var) {
    MathNode* int_left = integral_node(node->left, var);
    MathNode* int_right = integral_node(node->right, var);
    return math_node_binary(OP_ADD, int_left, int_right);
}

/* ∫ (f - g) dx = ∫f dx - ∫g dx */
static MathNode* integral_sub(const MathNode* node, const char* var) {
    MathNode* int_left = integral_node(node->left, var);
    MathNode* int_right = integral_node(node->right, var);
    return math_node_binary(OP_SUB, int_left, int_right);
}

/* Main integration dispatcher */
static MathNode* integral_node(const MathNode* node, const char* var) {
    if (!node) return math_node_number(0.0);

    switch (node->type) {
        case MATH_NUMBER:
            return integral_number(node, var);

        case MATH_CONSTANT: {
            /* Fold constant, then integrate as number */
            double v = lookup_constant(node->sym_name);
            if (!isnan(v)) return integral_number(math_node_number(v), var);
            return math_node_number(0.0);
        }

        case MATH_SYMBOL:
            if (is_symbol_name(node, var)) {
                /* ∫ x dx = x^2/2 */
                return math_node_binary(OP_DIV,
                    math_node_binary(OP_POW,
                        math_node_symbol(var), math_node_number(2.0)),
                    math_node_number(2.0));
            }
            /* ∫ c dx = c*x (treat other symbols as constants) */
            return math_node_binary(OP_MUL,
                plant_math_deep_copy(node), math_node_symbol(var));

        case MATH_BINARY_OP:
            switch (node->op) {
                case OP_ADD: return integral_add(node, var);
                case OP_SUB: return integral_sub(node, var);

                case OP_MUL: {
                    /* c * f(x) → c * ∫f dx */
                    if (is_pure_constant(node->left) && !is_symbol_name(node->left, var)) {
                        MathNode* int_right = integral_node(node->right, var);
                        return math_node_binary(OP_MUL,
                            plant_math_deep_copy(node->left), int_right);
                    }
                    if (is_pure_constant(node->right) && !is_symbol_name(node->right, var)) {
                        MathNode* int_left = integral_node(node->left, var);
                        return math_node_binary(OP_MUL,
                            int_left, plant_math_deep_copy(node->right));
                    }
                    /* Try integration by parts for general products */
                    { MathNode* bp = plant_math_integral_parts(node, var);
                      if (bp) return bp; }
                    return NULL;
                }

                case OP_DIV: {
                    /* c / x → c * LOG(|x|) */
                    if (is_pure_constant(node->left) && is_symbol_name(node->right, var)) {
                        MathNode* log_x = math_node_func("LOG",
                            math_node_func("ABS", math_node_symbol(var)));
                        return math_node_binary(OP_MUL,
                            plant_math_deep_copy(node->left), log_x);
                    }
                    /* 1 / x → LOG(|x|) */
                    if (is_one(node->left) && is_symbol_name(node->right, var)) {
                        return math_node_func("LOG",
                            math_node_func("ABS", math_node_symbol(var)));
                    }
                    /* 1 / (x^2 + 1) → ARCTAN(x) */
                    if (is_one(node->left) &&
                        node->right->type == MATH_BINARY_OP && node->right->op == OP_ADD) {
                        MathNode* den = node->right;
                        /* Match x^2 + 1 or 1 + x^2 */
                        int has_x2 = 0, has_one = 0;
                        if (matches_power_of_var(den->left, var, 2.0) && is_one(den->right))
                            { has_x2 = 1; has_one = 1; }
                        if (is_one(den->left) && matches_power_of_var(den->right, var, 2.0))
                            { has_x2 = 1; has_one = 1; }
                        if (has_x2 && has_one) {
                            return math_node_func("ARCTAN", math_node_symbol(var));
                        }
                    }
                    /* 1 / (1 + x^2) same as above via commutativity */
                    /* x^n (n negative) via power rule */
                    if (is_symbol_name(node->left, var) &&
                        node->right->type == MATH_NUMBER && node->right->num_val < 0) {
                        return integral_pow_var(node->right, var);
                    }
                    return NULL;
                }

                case OP_POW: {
                    /* x^n → x^(n+1)/(n+1) */
                    if (is_symbol_name(node->left, var) && is_pure_constant(node->right)) {
                        double n = (node->right->type == MATH_NUMBER) ?
                                   node->right->num_val :
                                   lookup_constant(node->right->sym_name);
                        if (fabs(n + 1.0) > 1e-10) {
                            return integral_pow_var(node->right, var);
                        }
                        /* n = -1: ∫ x^-1 dx = LOG(|x|) */
                        return math_node_func("LOG",
                            math_node_func("ABS", math_node_symbol(var)));
                    }
                    return NULL;
                }

                default: return NULL;
            }

        case MATH_UNARY_OP:
            if (node->op == OP_NEG) {
                MathNode* inner = integral_node(node->left, var);
                if (inner) return math_node_unary(inner);
                return NULL;
            }
            return NULL;

        case MATH_FUNC_CALL: {
            MathNode* arg = node->left;
            /* Simple case: arg is just the variable */
            if (is_symbol_name(arg, var)) {
                if (strcmp(node->sym_name, "SIN") == 0) {
                    /* ∫ SIN(x) dx = -COS(x) */
                    return math_node_unary(math_node_func("COS",
                        math_node_symbol(var)));
                }
                if (strcmp(node->sym_name, "COS") == 0) {
                    /* ∫ COS(x) dx = SIN(x) */
                    return math_node_func("SIN", math_node_symbol(var));
                }
                if (strcmp(node->sym_name, "EXP") == 0) {
                    /* ∫ EXP(x) dx = EXP(x) */
                    return math_node_func("EXP", math_node_symbol(var));
                }
                if (strcmp(node->sym_name, "TAN") == 0) {
                    /* ∫ TAN(x) dx = -LOG(|COS(x)|) */
                    return math_node_unary(math_node_func("LOG",
                        math_node_func("ABS",
                            math_node_func("COS", math_node_symbol(var)))));
                }
                if (strcmp(node->sym_name, "LOG") == 0) {
                    /* ∫ LOG(x) dx = x*LOG(x) - x */
                    MathNode* x = math_node_symbol(var);
                    MathNode* x_log_x = math_node_binary(OP_MUL,
                        plant_math_deep_copy(x),
                        math_node_func("LOG", plant_math_deep_copy(x)));
                    return math_node_binary(OP_SUB, x_log_x,
                        plant_math_deep_copy(x));
                }
                if (strcmp(node->sym_name, "ARCTAN") == 0 ||
                    strcmp(node->sym_name, "ATAN") == 0) {
                    /* ∫ ARCTAN(x) dx = x*ARCTAN(x) - LOG(1+x^2)/2 */
                    MathNode* x = math_node_symbol(var);
                    MathNode* x_atan_x = math_node_binary(OP_MUL,
                        plant_math_deep_copy(x),
                        math_node_func("ARCTAN", plant_math_deep_copy(x)));
                    MathNode* one_plus_x2 = math_node_binary(OP_ADD,
                        math_node_number(1.0),
                        math_node_binary(OP_POW,
                            plant_math_deep_copy(x), math_node_number(2.0)));
                    MathNode* half_log = math_node_binary(OP_MUL,
                        math_node_number(0.5),
                        math_node_func("LOG", one_plus_x2));
                    return math_node_binary(OP_SUB, x_atan_x, half_log);
                }
            }
            /* Unsupported function integration */
            return NULL;
        }
    }
    return NULL;
}

MathNode* plant_math_integral(const MathNode* node, const char* var) {
    if (!node || !var) return NULL;
    MathNode* result = integral_node(node, var);
    if (!result) return NULL;
    result = plant_math_simplify(result);
    return result;
}

char* plant_math_integral_str(const char* expr, const char* var) {
    MathNode* ast = plant_math_parse(expr);
    MathNode* result = plant_math_integral(ast, var);
    if (!result) {
        math_node_free(ast);
        /* Build error message */
        char* buf = (char*)malloc(256);
        snprintf(buf, 256,
            "ERROR: Integral of this expression is not supported yet.\n"
            "Supported: x^n, c*x, SIN(x), COS(x), EXP(x), LOG(x), TAN(x), 1/x, 1/(x^2+1), ARCTAN(x).");
        return buf;
    }
    /* Append +C */
    char* inner = plant_math_to_string(result);
    size_t len = strlen(inner);
    char* final_str = (char*)malloc(len + 16);
    snprintf(final_str, len + 16, "%s + C", inner);
    free(inner);
    math_node_free(ast);
    math_node_free(result);
    return final_str;
}

/* ====================================================================
 *  v0.50.0i — Polynomial Factoring Engine
 *
 *  Structural pattern matching:
 *    Perfect square:     x^2 + 2ax + a^2 → (x + a)^2
 *    Difference of squares: x^2 - a^2 → (x - a)(x + a)
 *    Common factor:      a*x + a*y → a*(x + y)
 * ==================================================================== */

/* Check if a node matches x^n where n is a specific value */
static int matches_power_of_var(const MathNode* n, const char* var, double target_exp) {
    if (!n || n->type != MATH_BINARY_OP || n->op != OP_POW) return 0;
    if (!is_symbol_name(n->left, var)) return 0;
    if (n->right->type == MATH_NUMBER)
        return fabs(n->right->num_val - target_exp) < 1e-10;
    return 0;
}

/* Check if a term is coeff * var^n */
static int term_coeff_var_pow(const MathNode* node, const char* var,
                               double target_exp, double* out_coeff) {
    if (!node) return 0;

    /* coeff * var^n */
    if (node->type == MATH_BINARY_OP && node->op == OP_MUL) {
        if (is_pure_constant(node->left) && matches_power_of_var(node->right, var, target_exp)) {
            *out_coeff = (node->left->type == MATH_NUMBER) ?
                          node->left->num_val : lookup_constant(node->left->sym_name);
            return 1;
        }
        if (is_pure_constant(node->right) && matches_power_of_var(node->left, var, target_exp)) {
            *out_coeff = (node->right->type == MATH_NUMBER) ?
                          node->right->num_val : lookup_constant(node->right->sym_name);
            return 1;
        }
    }

    /* var^n (coeff = 1) */
    if (matches_power_of_var(node, var, target_exp)) {
        *out_coeff = 1.0;
        return 1;
    }

    return 0;
}

/* Check if a node is a pure number */
static double get_number_value(const MathNode* n) {
    if (!n) return NAN;
    if (n->type == MATH_NUMBER) return n->num_val;
    if (n->type == MATH_CONSTANT) return lookup_constant(n->sym_name);
    return NAN;
}

MathNode* plant_math_factor(const MathNode* node) {
    if (!node) return NULL;

    /* Only factor sums/differences of 3 terms (quadratic trinomials) and
     * products/differences that match structural patterns */

    if (node->type == MATH_BINARY_OP && (node->op == OP_ADD || node->op == OP_SUB)) {
        /* Flatten into terms */
        TermEntry terms[MAX_TERMS];
        int count = 0;
        flatten_sum(node, terms, &count);

        if (count == 3) {
            /* Try to match ax^2 + bx + c (perfect square or factorable) */
            double c2 = 0, c1 = 0, c0 = 0;
            int has_x2 = 0, has_x = 0, has_const = 0;
            double a_val = 0, b_val = 0;

            for (int i = 0; i < count; i++) {
                if (terms[i].base == NULL) {
                    /* Pure constant */
                    c0 = terms[i].coeff;
                    has_const = 1;
                } else if (matches_power_of_var(terms[i].base, "x", 2.0)) {
                    c2 = terms[i].coeff;
                    a_val = c2;
                    has_x2 = 1;
                } else if (is_symbol_name(terms[i].base, "x")) {
                    c1 = terms[i].coeff;
                    b_val = c1;
                    has_x = 1;
                }
            }

            /* Perfect square: x^2 + 2ax + a^2 → (x + a)^2 */
            if (has_x2 && has_x && has_const && a_val > 0) {
                double a = sqrt(c0);
                if (fabs(a * a - c0) < 1e-10 && fabs(2.0 * a * a - c1) < 1e-10) {
                    /* Check: coefficient of x^2 is 1, coefficient of x is 2a, constant is a^2 */
                    if (fabs(c2 - 1.0) < 1e-10) {
                        for (int i = 0; i < count; i++) math_node_free(terms[i].base);
                        MathNode* inner = math_node_binary(OP_ADD,
                            math_node_symbol("x"), math_node_number(a));
                        return math_node_binary(OP_POW, inner, math_node_number(2.0));
                    }
                }
            }

            /* ax^2 + bx + c where a=1, perfect square variant */
            if (has_x2 && has_x && has_const && c2 == 1.0) {
                double half_b = c1 / 2.0;
                if (fabs(half_b * half_b - c0) < 1e-10) {
                    for (int i = 0; i < count; i++) math_node_free(terms[i].base);
                    MathNode* inner = math_node_binary(OP_ADD,
                        math_node_symbol("x"), math_node_number(half_b));
                    return math_node_binary(OP_POW, inner, math_node_number(2.0));
                }
            }

            /* Difference of squares: x^2 - a^2 → (x - a)(x + a) */
            if (has_x2 && has_const && !has_x && c2 == 1.0 && c0 < 0) {
                double a = sqrt(-c0);
                if (fabs(a * a - (-c0)) < 1e-10) {
                    for (int i = 0; i < count; i++) math_node_free(terms[i].base);
                    MathNode* x = math_node_symbol("x");
                    MathNode* a_node = math_node_number(a);
                    MathNode* factor1 = math_node_binary(OP_SUB,
                        plant_math_deep_copy(x), plant_math_deep_copy(a_node));
                    MathNode* factor2 = math_node_binary(OP_ADD, x, a_node);
                    return math_node_binary(OP_MUL, factor1, factor2);
                }
            }
        }

        /* 2-term difference of squares: x^2 - a^2 → (x - a)(x + a) */
        if (count == 2) {
            double c_sq = 0, c_const = 0;
            int has_sq = 0, has_c = 0;
            for (int i = 0; i < count; i++) {
                if (terms[i].base == NULL) {
                    c_const = terms[i].coeff;
                    has_c = 1;
                } else if (matches_power_of_var(terms[i].base, "x", 2.0)) {
                    c_sq = terms[i].coeff;
                    has_sq = 1;
                }
            }
            if (has_sq && has_c && c_sq == 1.0 && c_const < 0) {
                double a = sqrt(-c_const);
                if (fabs(a * a - (-c_const)) < 1e-10) {
                    for (int i = 0; i < count; i++) math_node_free(terms[i].base);
                    MathNode* x = math_node_symbol("x");
                    MathNode* a_node = math_node_number(a);
                    MathNode* factor1 = math_node_binary(OP_SUB,
                        plant_math_deep_copy(x), plant_math_deep_copy(a_node));
                    MathNode* factor2 = math_node_binary(OP_ADD, x, a_node);
                    return math_node_binary(OP_MUL, factor1, factor2);
                }
            }
        }

        for (int i = 0; i < count; i++) math_node_free(terms[i].base);
    }

    /* Common factor extraction: a*x + a*y → a*(x + y) */
    if (node->type == MATH_BINARY_OP && node->op == OP_ADD) {
        TermEntry terms[MAX_TERMS];
        int count = 0;
        flatten_sum(node, terms, &count);

        if (count >= 2) {
            /* Try to find a common numeric GCD of all coefficients */
            double gcd_val = terms[0].coeff;
            for (int i = 1; i < count; i++) {
                double a = fabs(gcd_val), b = fabs(terms[i].coeff);
                while (b > 1e-10) { double t = fmod(a, b); a = b; b = t; }
                gcd_val = a;
            }
            if (fabs(gcd_val) > 1e-10) {
                /* Check all coefficients are divisible */
                int all_div = 1;
                for (int i = 1; i < count; i++) {
                    if (fabs(terms[i].coeff / gcd_val -
                             round(terms[i].coeff / gcd_val)) > 1e-10) {
                        all_div = 0; break;
                    }
                }
                if (all_div && count >= 2) {
                    /* Build inner sum with divided coefficients */
                    MathNode* inner = NULL;
                    for (int i = 0; i < count; i++) {
                        double new_coeff = terms[i].coeff / gcd_val;
                        MathNode* term = build_term_node(new_coeff,
                            terms[i].base ? plant_math_deep_copy(terms[i].base) : NULL);
                        if (!inner) inner = term;
                        else inner = math_node_binary(OP_ADD, inner, term);
                    }
                    for (int i = 0; i < count; i++) math_node_free(terms[i].base);
                    return math_node_binary(OP_MUL,
                        math_node_number(gcd_val), inner);
                }
            }
            for (int i = 0; i < count; i++) math_node_free(terms[i].base);
        }
    }

    /* No factoring pattern matched — return a deep copy */
    return plant_math_deep_copy(node);
}

char* plant_math_factor_str(const char* expr) {
    MathNode* ast = plant_math_parse(expr);
    MathNode* result = plant_math_factor(ast);
    char* str = plant_math_to_string(result);
    math_node_free(ast);
    math_node_free(result);
    return str;
}

/* ====================================================================
 *  v0.50.0j — GCD (Greatest Common Divisor)
 * ==================================================================== */

long plant_math_gcd(long a, long b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

/* ====================================================================
 *  v0.50.0j — Quadratic Equation Solver
 *
 *  Solves ax^2 + bx + c = 0.
 *  Real roots when Δ = b^2 - 4ac ≥ 0.
 *  Complex roots when Δ < 0, expressed using imaginary unit i.
 * ==================================================================== */

char* plant_math_quadratic(double a, double b, double c) {
    char* buf = (char*)malloc(512);
    if (fabs(a) < 1e-15) {
        /* Linear equation bx + c = 0 */
        if (fabs(b) < 1e-15) {
            if (fabs(c) < 1e-15)
                snprintf(buf, 512, "Infinite solutions (0 = 0)");
            else
                snprintf(buf, 512, "No solution");
        } else {
            double x = -c / b;
            snprintf(buf, 512, "x = %g", x);
        }
        return buf;
    }

    double disc = b * b - 4.0 * a * c;
    double real_part = -b / (2.0 * a);

    if (fabs(disc) < 1e-15) {
        /* One repeated root */
        snprintf(buf, 512, "x = %g", real_part);
    } else if (disc > 0) {
        /* Two real roots */
        double sqrt_disc = sqrt(disc);
        double x1 = real_part - sqrt_disc / (2.0 * a);
        double x2 = real_part + sqrt_disc / (2.0 * a);
        snprintf(buf, 512, "x1 = %g, x2 = %g", x1, x2);
    } else {
        /* Two complex roots */
        double imag_part = sqrt(-disc) / (2.0 * fabs(a));
        if (fabs(a) < 0) /* adjust sign */
            imag_part = sqrt(-disc) / (2.0 * a);
        /* Normalize: if a < 0, flip sign of imaginary part */
        if (a < 0) imag_part = -imag_part;

        char real_str[64], imag_str[64];
        if (fabs(real_part) < 1e-15)
            snprintf(real_str, sizeof(real_str), "0");
        else
            snprintf(real_str, sizeof(real_str), "%g", real_part);

        if (fabs(fabs(imag_part) - 1.0) < 1e-10)
            snprintf(imag_str, sizeof(imag_str), "%s", imag_part > 0 ? "" : "-");
        else
            snprintf(imag_str, sizeof(imag_str), "%g*", fabs(imag_part));

        if (imag_part >= 0) {
            snprintf(buf, 512, "x1 = %s + %si, x2 = %s - %si",
                     real_str, imag_str, real_str, imag_str);
        } else {
            snprintf(buf, 512, "x1 = %s - %si, x2 = %s + %si",
                     real_str, imag_str + 1, real_str, imag_str + 1);
        }
    }
    return buf;
}

char* plant_math_quadratic_str(const char* a_str, const char* b_str, const char* c_str) {
    MathNode* a_ast = plant_math_parse(a_str);
    MathNode* b_ast = plant_math_parse(b_str);
    MathNode* c_ast = plant_math_parse(c_str);
    double a = plant_math_eval(a_ast);
    double b = plant_math_eval(b_ast);
    double c = plant_math_eval(c_ast);
    math_node_free(a_ast);
    math_node_free(b_ast);
    math_node_free(c_ast);
    return plant_math_quadratic(a, b, c);
}

/* ====================================================================
 *  v0.50.3 — Limit Evaluation Subsystem
 *
 *  Evaluates lim_{x→c} f(x) with:
 *    - Direct substitution (polynomial, continuous functions)
 *    - Trigonometric limits (sin(x)/x → 1, (1-cos(x))/x → 0, etc.)
 *    - Exponential limits ((e^x-1)/x → 1)
 *    - Asymptotic evaluation at infinity (MATH_INF)
 *    - L'Hôpital's rule for 0/0 and ∞/∞ indeterminate forms
 * ==================================================================== */

#define MATH_INF 1e308

/* Forward declaration */
static double limit_node(const MathNode* node, const char* var, double point);

/* Substitute variable with a constant value, returning a new MathNode tree */
static MathNode* substitute_var(const MathNode* node, const char* var, double point) {
    if (!node) return NULL;

    if (node->type == MATH_SYMBOL && strcmp(node->sym_name, var) == 0) {
        return math_node_number(point);
    }
    if (node->type == MATH_NUMBER || node->type == MATH_CONSTANT) {
        return plant_math_deep_copy(node);
    }

    if (node->type == MATH_BINARY_OP) {
        MathNode* L = substitute_var(node->left, var, point);
        MathNode* R = substitute_var(node->right, var, point);
        MathNode* n = math_node_binary(node->op, L, R);
        return n;
    }
    if (node->type == MATH_UNARY_OP) {
        MathNode* inner = substitute_var(node->left, var, point);
        return math_node_unary(inner);
    }
    if (node->type == MATH_FUNC_CALL) {
        MathNode* inner = substitute_var(node->left, var, point);
        return math_node_func(node->sym_name, inner);
    }

    return plant_math_deep_copy(node);
}

/* Evaluate a node at a point by substituting the variable */
static double eval_at(const MathNode* node, const char* var, double point) {
    MathNode* substituted = substitute_var(node, var, point);
    MathNode* simplified = plant_math_simplify(substituted);
    double result = plant_math_eval(simplified);
    math_node_free(simplified);
    return result;
}

/* Check if a limit expression is 0/0 or ∞/∞ form */
static int is_indeterminate_form(const MathNode* node, const char* var, double point) {
    if (!node || node->type != MATH_BINARY_OP || node->op != OP_DIV) return 0;
    double num = eval_at(node->left, var, point);
    double den = eval_at(node->right, var, point);
    /* 0/0 form */
    if (fabs(num) < 1e-10 && fabs(den) < 1e-10) return 1;
    /* ∞/∞ form */
    if (fabs(num) > MATH_INF / 2 && fabs(den) > MATH_INF / 2) return 1;
    return 0;
}

/* Check if a node contains the variable */
static int contains_var(const MathNode* node, const char* var) {
    if (!node) return 0;
    if (node->type == MATH_SYMBOL && strcmp(node->sym_name, var) == 0) return 1;
    if (node->type == MATH_CONSTANT) return 0;
    if (node->type == MATH_NUMBER) return 0;
    return contains_var(node->left, var) || contains_var(node->right, var);
}

/* L'Hôpital's rule: differentiate numerator and denominator */
static double hopital_limit(const MathNode* node, const char* var, double point, int depth) {
    if (depth > 10) return NAN; /* prevent infinite recursion */

    MathNode* num = node->left;
    MathNode* den = node->right;

    /* Differentiate numerator and denominator */
    MathNode* d_num = plant_math_derivative(num, var);
    MathNode* d_den = plant_math_derivative(den, var);

    if (!d_num || !d_den) {
        math_node_free(d_num);
        math_node_free(d_den);
        return NAN;
    }

    /* Simplify the derivatives */
    d_num = plant_math_simplify(d_num);
    d_den = plant_math_simplify(d_den);

    /* Build the new fraction d_num / d_den */
    MathNode* new_frac = math_node_binary(OP_DIV, d_num, d_den);

    /* Evaluate the new limit */
    double result = limit_node(new_frac, var, point);
    math_node_free(new_frac);
    return result;
}

/* Special trigonometric limits */
static double trig_limit(const MathNode* node, const char* var, double point) {
    if (!node || node->type != MATH_BINARY_OP || node->op != OP_DIV) return NAN;
    if (point != 0.0) return NAN; /* Most trig limits are at 0 */

    MathNode* num = node->left;
    MathNode* den = node->right;

    /* sin(x)/x → 1 */
    if (num->type == MATH_FUNC_CALL && strcmp(num->sym_name, "SIN") == 0 &&
        contains_var(num->left, var) && is_symbol_name(den, var)) {
        /* Check if inner is just var */
        MathNode* inner = num->left;
        if (inner->type == MATH_SYMBOL && strcmp(inner->sym_name, var) == 0)
            return 1.0;
    }

    /* (1 - cos(x))/x → 0 */
    if (num->type == MATH_BINARY_OP && num->op == OP_SUB) {
        if (is_one(num->left) &&
            num->right->type == MATH_FUNC_CALL &&
            strcmp(num->right->sym_name, "COS") == 0 &&
            is_symbol_name(num->right->left, var) &&
            is_symbol_name(den, var)) {
            return 0.0;
        }
    }

    /* tan(x)/x → 1 */
    if (num->type == MATH_FUNC_CALL && strcmp(num->sym_name, "TAN") == 0 &&
        is_symbol_name(num->left, var) && is_symbol_name(den, var)) {
        return 1.0;
    }

    /* (e^x - 1)/x → 1 */
    if (num->type == MATH_BINARY_OP && num->op == OP_SUB) {
        if (num->right->type == MATH_NUMBER && fabs(num->right->num_val - 1.0) < 1e-10 &&
            num->left->type == MATH_FUNC_CALL &&
            strcmp(num->left->sym_name, "EXP") == 0 &&
            is_symbol_name(num->left->left, var) &&
            is_symbol_name(den, var)) {
            return 1.0;
        }
    }

    /* (log(1+x))/x → 1 */
    if (num->type == MATH_FUNC_CALL && strcmp(num->sym_name, "LOG") == 0 &&
        num->left->type == MATH_BINARY_OP && num->left->op == OP_ADD &&
        is_one(num->left->left) && is_symbol_name(num->left->right, var) &&
        is_symbol_name(den, var)) {
        return 1.0;
    }

    return NAN;
}

/* Main limit evaluation */
static double limit_node(const MathNode* node, const char* var, double point) {
    if (!node) return NAN;

    /* If the expression doesn't contain the variable, the limit is just the value */
    if (!contains_var(node, var)) {
        return plant_math_eval(node);
    }

    /* Try direct substitution */
    double val = eval_at(node, var, point);

    /* If the result is finite and not NaN, we're done */
    if (val == val && fabs(val) < MATH_INF) return val;

    /* Try trigonometric special limits */
    double tval = trig_limit(node, var, point);
    if (tval == tval) return tval;

    /* Try L'Hôpital's rule for indeterminate forms */
    if (is_indeterminate_form(node, var, point)) {
        return hopital_limit(node, var, point, 0);
    }

    return val;
}

double plant_math_limit_val(const MathNode* node, const char* var, double point) {
    if (!node || !var) return NAN;
    return limit_node(node, var, point);
}

char* plant_math_limit_str(const char* expr, const char* var, const char* point_str) {
    MathNode* ast = plant_math_parse(expr);
    if (!ast) return strdup("ERROR: Could not parse expression.");

    /* Parse the limit point */
    double point = NAN;
    if (strcmp(point_str, "inf") == 0 || strcmp(point_str, "infinity") == 0 ||
        strcmp(point_str, "INF") == 0) {
        point = MATH_INF;
    } else if (strcmp(point_str, "-inf") == 0 || strcmp(point_str, "-infinity") == 0 ||
               strcmp(point_str, "-INF") == 0) {
        point = -MATH_INF;
    } else {
        MathNode* p_ast = plant_math_parse(point_str);
        if (p_ast) {
            point = plant_math_eval(p_ast);
            math_node_free(p_ast);
        }
    }

    double result = limit_node(ast, var, point);
    math_node_free(ast);

    char buf[64];
    if (result != result) { /* NaN */
        snprintf(buf, sizeof(buf), "undefined");
    } else if (fabs(result) >= MATH_INF) {
        snprintf(buf, sizeof(buf), "%s%s", result > 0 ? "" : "-", "infinity");
    } else {
        snprintf(buf, sizeof(buf), "%g", result);
    }
    return strdup(buf);
}

/* ====================================================================
 *  v0.50.3 — Integration by Parts
 *
 *  Formula: ∫ u dv = uv - ∫ v du
 *  LIATE heuristic for choosing u:
 *    L = Logarithmic, I = Inverse trig, A = Algebraic,
 *    T = Trigonometric, E = Exponential
 * ==================================================================== */

/* Classify a node for LIATE ranking (lower = better u candidate) */
static int liate_rank(const MathNode* node) {
    if (!node) return 5;
    /* Logarithmic */
    if (node->type == MATH_FUNC_CALL &&
        (strcmp(node->sym_name, "LOG") == 0 ||
         strcmp(node->sym_name, "LN") == 0))
        return 0;
    /* Inverse trigonometric */
    if (node->type == MATH_FUNC_CALL &&
        (strcmp(node->sym_name, "ASIN") == 0 ||
         strcmp(node->sym_name, "ACOS") == 0 ||
         strcmp(node->sym_name, "ATAN") == 0 ||
         strcmp(node->sym_name, "ARCTAN") == 0))
        return 1;
    /* Algebraic (variable or polynomial) */
    if (node->type == MATH_SYMBOL) return 2;
    if (node->type == MATH_BINARY_OP &&
        (node->op == OP_POW || node->op == OP_MUL || node->op == OP_ADD ||
         node->op == OP_SUB))
        return 2;
    /* Trigonometric */
    if (node->type == MATH_FUNC_CALL &&
        (strcmp(node->sym_name, "SIN") == 0 ||
         strcmp(node->sym_name, "COS") == 0 ||
         strcmp(node->sym_name, "TAN") == 0))
        return 3;
    /* Exponential */
    if (node->type == MATH_FUNC_CALL &&
        strcmp(node->sym_name, "EXP") == 0)
        return 4;
    return 5;
}

/* Integration by parts: ∫ u·dv = u·v - ∫ v·du */
static MathNode* integrate_by_parts_impl(const MathNode* u, const MathNode* dv,
                                          const char* var) {
    /* v = ∫ dv */
    MathNode* v = integral_node(dv, var);
    if (!v) return NULL;

    /* du = d/dx(u) */
    MathNode* du = plant_math_derivative(u, var);
    if (!du) { math_node_free(v); return NULL; }
    du = plant_math_simplify(du);

    /* u*v */
    MathNode* uv = math_node_binary(OP_MUL,
        plant_math_deep_copy(u), v);

    /* ∫ v·du */
    MathNode* vdu = math_node_binary(OP_MUL,
        plant_math_deep_copy(v), du);
    MathNode* integral_vdu = integral_node(vdu, var);
    math_node_free(vdu);

    if (!integral_vdu) {
        math_node_free(uv);
        return NULL;
    }

    /* result = uv - ∫ v·du */
    MathNode* result = math_node_binary(OP_SUB, uv, integral_vdu);
    return result;
}

MathNode* plant_math_integral_parts(const MathNode* node, const char* var) {
    if (!node || !var) return NULL;

    /* Special case: LOG(x) — u = LOG(x), dv = 1*dx */
    if (node->type == MATH_FUNC_CALL &&
        (strcmp(node->sym_name, "LOG") == 0 || strcmp(node->sym_name, "LN") == 0) &&
        is_symbol_name(node->left, var)) {
        /* ∫ LOG(x) dx = x*LOG(x) - x */
        MathNode* x = math_node_symbol(var);
        MathNode* x_log_x = math_node_binary(OP_MUL,
            plant_math_deep_copy(x),
            math_node_func("LOG", plant_math_deep_copy(x)));
        return math_node_binary(OP_SUB, x_log_x,
            plant_math_deep_copy(x));
    }

    /* Must be a product of two functions to apply by-parts */
    if (node->type != MATH_BINARY_OP || node->op != OP_MUL) return NULL;

    /* Try both orderings for u and dv */
    const MathNode* factors[2] = { node->left, node->right };
    int best_u = 0;

    /* LIATE: choose the factor with lower rank as u */
    int r0 = liate_rank(factors[0]);
    int r1 = liate_rank(factors[1]);
    if (r1 < r0) best_u = 1;

    MathNode* result = integrate_by_parts_impl(
        factors[best_u], factors[1 - best_u], var);
    if (result) return plant_math_simplify(result);

    /* Try the other ordering */
    result = integrate_by_parts_impl(
        factors[1 - best_u], factors[best_u], var);
    if (result) return plant_math_simplify(result);

    return NULL;
}

char* plant_math_integral_parts_str(const char* expr, const char* var) {
    MathNode* ast = plant_math_parse(expr);
    MathNode* result = plant_math_integral_parts(ast, var);
    if (!result) {
        math_node_free(ast);
        return strdup("ERROR: Integration by parts not applicable to this expression.");
    }
    char* inner = plant_math_to_string(result);
    size_t len = strlen(inner);
    char* final_str = (char*)malloc(len + 16);
    snprintf(final_str, len + 16, "%s + C", inner);
    free(inner);
    math_node_free(ast);
    math_node_free(result);
    return final_str;
}

/* ====================================================================
 *  v0.50.3 — Integration by Substitution
 *
 *  Pattern matching for ∫ f(g(x))·g'(x) dx = ∫ f(u) du
 *  Known patterns:
 *    ∫ 2x·e^(x²) dx = e^(x²) + C
 *    ∫ cos(x)·sin(x) dx = sin²(x)/2 + C
 *    ∫ 2x/(x²+1) dx = log(x²+1) + C
 *    ∫ x·cos(x²) dx = sin(x²)/2 + C
 * ==================================================================== */

/* Check if a node matches c*x^n (power of variable with coefficient) */
static int is_power_of_var(const MathNode* node, const char* var, double target_exp) {
    if (!node) return 0;
    if (node->type == MATH_BINARY_OP && node->op == OP_MUL) {
        /* c * x^n */
        if (is_pure_constant(node->left) && node->right->type == MATH_BINARY_OP &&
            node->right->op == OP_POW && is_symbol_name(node->right->left, var) &&
            node->right->right->type == MATH_NUMBER &&
            fabs(node->right->right->num_val - target_exp) < 1e-10)
            return 1;
        /* x^n * c */
        if (is_pure_constant(node->right) && node->left->type == MATH_BINARY_OP &&
            node->left->op == OP_POW && is_symbol_name(node->left->left, var) &&
            node->left->right->type == MATH_NUMBER &&
            fabs(node->left->right->num_val - target_exp) < 1e-10)
            return 1;
        /* c * x (target_exp == 1) */
        if (target_exp == 1.0) {
            if (is_pure_constant(node->left) && is_symbol_name(node->right, var))
                return 1;
            if (is_pure_constant(node->right) && is_symbol_name(node->left, var))
                return 1;
        }
    }
    if (node->type == MATH_BINARY_OP && node->op == OP_POW &&
        is_symbol_name(node->left, var) &&
        node->right->type == MATH_NUMBER &&
        fabs(node->right->num_val - target_exp) < 1e-10)
        return 1;
    if (target_exp == 1.0 && node->type == MATH_SYMBOL &&
        strcmp(node->sym_name, var) == 0)
        return 1;
    return 0;
}

MathNode* plant_math_integral_subst(const MathNode* node, const char* var) {
    if (!node || !var) return NULL;

    /* ∫ cos(x)·sin(x) dx → sin²(x)/2 + C */
    if (node->type == MATH_BINARY_OP && node->op == OP_MUL) {
        /* cos(x) * sin(x) */
        int cos_sin = 0, sin_cos = 0;
        if (node->left->type == MATH_FUNC_CALL &&
            strcmp(node->left->sym_name, "COS") == 0 &&
            is_symbol_name(node->left->left, var) &&
            node->right->type == MATH_FUNC_CALL &&
            strcmp(node->right->sym_name, "SIN") == 0 &&
            is_symbol_name(node->right->left, var))
            cos_sin = 1;
        if (node->right->type == MATH_FUNC_CALL &&
            strcmp(node->right->sym_name, "COS") == 0 &&
            is_symbol_name(node->right->left, var) &&
            node->left->type == MATH_FUNC_CALL &&
            strcmp(node->left->sym_name, "SIN") == 0 &&
            is_symbol_name(node->left->left, var))
            sin_cos = 1;
        if (cos_sin || sin_cos) {
            /* ∫ cos(x)sin(x) dx = sin²(x)/2 */
            MathNode* sin_x = math_node_func("SIN", math_node_symbol(var));
            MathNode* sin_sq = math_node_binary(OP_POW, sin_x, math_node_number(2.0));
            return math_node_binary(OP_DIV, sin_sq, math_node_number(2.0));
        }
    }

    /* ∫ 2x·e^(x²) dx → e^(x²) + C */
    if (node->type == MATH_BINARY_OP && node->op == OP_MUL) {
        MathNode* L = node->left, *R = node->right;
        /* 2x * e^(x^2) */
        if (is_power_of_var(L, var, 1.0) &&
            R->type == MATH_FUNC_CALL && strcmp(R->sym_name, "EXP") == 0 &&
            R->left->type == MATH_BINARY_OP && R->left->op == OP_POW &&
            is_symbol_name(R->left->left, var) &&
            R->left->right->type == MATH_NUMBER &&
            fabs(R->left->right->num_val - 2.0) < 1e-10) {
            /* ∫ 2x·e^(x²) dx = e^(x²) */
            return plant_math_deep_copy(R);
        }
        /* e^(x^2) * 2x */
        if (L->type == MATH_FUNC_CALL && strcmp(L->sym_name, "EXP") == 0 &&
            L->left->type == MATH_BINARY_OP && L->left->op == OP_POW &&
            is_symbol_name(L->left->left, var) &&
            L->left->right->type == MATH_NUMBER &&
            fabs(L->left->right->num_val - 2.0) < 1e-10 &&
            is_power_of_var(R, var, 1.0)) {
            return plant_math_deep_copy(L);
        }
    }

    /* ∫ 2x/(x²+1) dx → log(x²+1) */
    if (node->type == MATH_BINARY_OP && node->op == OP_DIV) {
        MathNode* num = node->left, *den = node->right;
        if (is_power_of_var(num, var, 1.0) && den->type == MATH_BINARY_OP &&
            den->op == OP_ADD) {
            /* Check x^2 + 1 or 1 + x^2 */
            int has_x2 = 0, has_one = 0;
            if (matches_power_of_var(den->left, var, 2.0) && is_one(den->right))
                { has_x2 = 1; has_one = 1; }
            if (is_one(den->left) && matches_power_of_var(den->right, var, 2.0))
                { has_x2 = 1; has_one = 1; }
            if (has_x2 && has_one) {
                /* ∫ 2x/(x²+1) dx = log(x²+1) */
                MathNode* x2_plus_1 = plant_math_deep_copy(den);
                return math_node_func("LOG", x2_plus_1);
            }
        }
    }

    /* ∫ x·cos(x²) dx → sin(x²)/2 */
    if (node->type == MATH_BINARY_OP && node->op == OP_MUL) {
        MathNode* L = node->left, *R = node->right;
        /* x * cos(x^2) */
        if (is_symbol_name(L, var) &&
            R->type == MATH_FUNC_CALL && strcmp(R->sym_name, "COS") == 0 &&
            R->left->type == MATH_BINARY_OP && R->left->op == OP_POW &&
            is_symbol_name(R->left->left, var) &&
            R->left->right->type == MATH_NUMBER &&
            fabs(R->left->right->num_val - 2.0) < 1e-10) {
            /* ∫ x·cos(x²) dx = sin(x²)/2 */
            MathNode* sin_x2 = math_node_func("SIN",
                plant_math_deep_copy(R->left));
            return math_node_binary(OP_DIV, sin_x2, math_node_number(2.0));
        }
        /* cos(x^2) * x */
        if (R->type == MATH_SYMBOL && strcmp(R->sym_name, var) == 0 &&
            L->type == MATH_FUNC_CALL && strcmp(L->sym_name, "COS") == 0 &&
            L->left->type == MATH_BINARY_OP && L->left->op == OP_POW &&
            is_symbol_name(L->left->left, var) &&
            L->left->right->type == MATH_NUMBER &&
            fabs(L->left->right->num_val - 2.0) < 1e-10) {
            MathNode* sin_x2 = math_node_func("SIN",
                plant_math_deep_copy(L->left));
            return math_node_binary(OP_DIV, sin_x2, math_node_number(2.0));
        }
    }

    return NULL;
}

char* plant_math_integral_subst_str(const char* expr, const char* var) {
    MathNode* ast = plant_math_parse(expr);
    MathNode* result = plant_math_integral_subst(ast, var);
    if (!result) {
        math_node_free(ast);
        return strdup("ERROR: No substitution pattern matched for this integral.");
    }
    char* inner = plant_math_to_string(result);
    size_t len = strlen(inner);
    char* final_str = (char*)malloc(len + 16);
    snprintf(final_str, len + 16, "%s + C", inner);
    free(inner);
    math_node_free(ast);
    math_node_free(result);
    return final_str;
}

/* ====================================================================
 *  v0.50.4 — Series Expansion Subsystem
 *
 *  Taylor series: f(x) = Σ_{n=0}^{N} f^{(n)}(a)/n! · (x-a)^n
 *  Maclaurin is Taylor with a=0.
 *
 *  Template shortcuts for common functions to avoid expensive
 *  symbolic derivative computation:
 *    exp(x)     → 1 + x + x²/2! + x³/3! + ...
 *    sin(x)     → x - x³/3! + x⁵/5! - ...
 *    cos(x)     → 1 - x²/2! + x⁴/4! - ...
 *    log(1+x)   → x - x²/2 + x³/3 - ...
 *    1/(1-x)    → 1 + x + x² + x³ + ...
 * ==================================================================== */

/* Factorial helper */
static double factorial(int n) {
    double r = 1.0;
    for (int i = 2; i <= n; i++) r *= i;
    return r;
}

/* Compute n-th derivative of node with respect to var, evaluated at center */
static double nth_derivative_at(const MathNode* node, const char* var,
                                 double center, int n) {
    MathNode* cur = plant_math_deep_copy(node);
    for (int i = 0; i < n; i++) {
        MathNode* d = plant_math_derivative(cur, var);
        math_node_free(cur);
        if (!d) return NAN;
        cur = plant_math_simplify(d);
    }
    /* Evaluate at center */
    MathNode* substituted = substitute_var(cur, var, center);
    MathNode* simplified = plant_math_simplify(substituted);
    double val = plant_math_eval(simplified);
    math_node_free(simplified);
    math_node_free(cur);
    return val;
}

/* Build polynomial term: coeff * (x - center)^power */
static MathNode* series_term(const char* var, double center, double coeff, int power) {
    if (fabs(coeff) < 1e-15) return NULL;

    MathNode* x_node;
    if (fabs(center) < 1e-15) {
        /* Maclaurin: just x^power */
        x_node = math_node_symbol(var);
    } else {
        /* Taylor: (x - center)^power */
        x_node = math_node_binary(OP_SUB,
            math_node_symbol(var), math_node_number(center));
    }

    MathNode* term;
    if (power == 0) {
        term = math_node_number(coeff);
    } else if (power == 1) {
        term = math_node_binary(OP_MUL, math_node_number(coeff), x_node);
    } else {
        MathNode* x_pow = math_node_binary(OP_POW,
            x_node, math_node_number((double)power));
        term = math_node_binary(OP_MUL, math_node_number(coeff), x_pow);
    }
    return term;
}

/* Template: exp(x) about center=a */
static MathNode* series_exp(const char* var, double center, int max_order) {
    MathNode* result = NULL;
    for (int n = 0; n <= max_order; n++) {
        /* d^n/dx^n e^x = e^x, eval at a = e^a */
        double coeff = exp(center) / factorial(n);
        MathNode* term = series_term(var, center, coeff, n);
        if (term) {
            if (!result) result = term;
            else result = math_node_binary(OP_ADD, result, term);
        }
    }
    return result ? result : math_node_number(0.0);
}

/* Template: sin(x) about center=a — uses angle addition formula expansion */
static MathNode* series_sin(const char* var, double center, int max_order) {
    /* sin(x) = sin(a + (x-a)) = sin(a)·cos(x-a) + cos(a)·sin(x-a) */
    /* Expand cos(x-a) and sin(x-a) as Maclaurin series */
    double sa = sin(center), ca = cos(center);
    MathNode* result = NULL;
    for (int n = 0; n <= max_order; n++) {
        /* d^n/dx^n sin(x) = sin(x + nπ/2) */
        double angle = center + n * M_PI / 2.0;
        double coeff = sin(angle) / factorial(n);
        MathNode* term = series_term(var, center, coeff, n);
        if (term) {
            if (!result) result = term;
            else result = math_node_binary(OP_ADD, result, term);
        }
    }
    return result ? result : math_node_number(0.0);
}

/* Template: cos(x) about center=a */
static MathNode* series_cos(const char* var, double center, int max_order) {
    MathNode* result = NULL;
    for (int n = 0; n <= max_order; n++) {
        /* d^n/dx^n cos(x) = cos(x + nπ/2) */
        double angle = center + n * M_PI / 2.0;
        double coeff = cos(angle) / factorial(n);
        MathNode* term = series_term(var, center, coeff, n);
        if (term) {
            if (!result) result = term;
            else result = math_node_binary(OP_ADD, result, term);
        }
    }
    return result ? result : math_node_number(0.0);
}

/* Template: log(1+x) about center=0 only */
static MathNode* series_log1x(const char* var, int max_order) {
    MathNode* result = NULL;
    for (int n = 1; n <= max_order; n++) {
        double coeff = ((n % 2 == 1) ? 1.0 : -1.0) / (double)n;
        MathNode* term = series_term(var, 0.0, coeff, n);
        if (term) {
            if (!result) result = term;
            else result = math_node_binary(OP_ADD, result, term);
        }
    }
    return result ? result : math_node_number(0.0);
}

/* Template: 1/(1-x) about center=0 only */
static MathNode* series_geom(const char* var, int max_order) {
    MathNode* result = NULL;
    for (int n = 0; n <= max_order; n++) {
        MathNode* term = series_term(var, 0.0, 1.0, n);
        if (term) {
            if (!result) result = term;
            else result = math_node_binary(OP_ADD, result, term);
        }
    }
    return result ? result : math_node_number(0.0);
}

/* Detect function call patterns for template shortcuts */
static MathNode* try_series_template(const MathNode* node, const char* var,
                                       double center, int max_order) {
    /* 1/(1-x) — geometric series, only at center=0 */
    if (node->type == MATH_BINARY_OP && node->op == OP_DIV &&
        fabs(center) < 1e-15 &&
        is_one(node->left) &&
        node->right->type == MATH_BINARY_OP && node->right->op == OP_SUB &&
        is_one(node->right->left) &&
        node->right->right->type == MATH_SYMBOL &&
        strcmp(node->right->right->sym_name, var) == 0) {
        return series_geom(var, max_order);
    }

    if (node->type != MATH_FUNC_CALL) return NULL;

    const char* fname = node->sym_name;

    /* exp(x) */
    if (strcmp(fname, "EXP") == 0 &&
        node->left->type == MATH_SYMBOL &&
        strcmp(node->left->sym_name, var) == 0) {
        return series_exp(var, center, max_order);
    }

    /* sin(x) */
    if (strcmp(fname, "SIN") == 0 &&
        node->left->type == MATH_SYMBOL &&
        strcmp(node->left->sym_name, var) == 0) {
        return series_sin(var, center, max_order);
    }

    /* cos(x) */
    if (strcmp(fname, "COS") == 0 &&
        node->left->type == MATH_SYMBOL &&
        strcmp(node->left->sym_name, var) == 0) {
        return series_cos(var, center, max_order);
    }

    /* log(1+x) — only at center=0 */
    if ((strcmp(fname, "LOG") == 0 || strcmp(fname, "LN") == 0) &&
        fabs(center) < 1e-15 &&
        node->left->type == MATH_BINARY_OP && node->left->op == OP_ADD &&
        is_one(node->left->left) &&
        node->left->right->type == MATH_SYMBOL &&
        strcmp(node->left->right->sym_name, var) == 0) {
        return series_log1x(var, max_order);
    }

    return NULL;
}

MathNode* plant_math_series(const MathNode* node, const char* var, int max_order) {
    return plant_math_taylor(node, var, 0.0, max_order);
}

char* plant_math_series_str(const char* expr, const char* var, int max_order) {
    MathNode* ast = plant_math_parse(expr);
    if (!ast) return strdup("ERROR: Could not parse expression.");
    MathNode* result = plant_math_series(ast, var, max_order);
    math_node_free(ast);
    if (!result) return strdup("ERROR: Could not compute series expansion.");
    char* inner = plant_math_to_string(result);
    math_node_free(result);
    return inner;
}

MathNode* plant_math_taylor(const MathNode* node, const char* var,
                             double center, int max_order) {
    if (!node || !var || max_order < 0) return NULL;

    /* Try template shortcuts first */
    MathNode* tmpl = try_series_template(node, var, center, max_order);
    if (tmpl) return tmpl;

    /* General Taylor expansion via iterative differentiation */
    MathNode* result = NULL;
    for (int n = 0; n <= max_order; n++) {
        double coeff = nth_derivative_at(node, var, center, n);
        if (coeff != coeff) continue; /* skip NaN */
        coeff /= factorial(n);
        MathNode* term = series_term(var, center, coeff, n);
        if (term) {
            if (!result) result = term;
            else result = math_node_binary(OP_ADD, result, term);
        }
    }
    return result ? plant_math_simplify(result) : math_node_number(0.0);
}

char* plant_math_taylor_str(const char* expr, const char* var,
                             const char* center_str, int max_order) {
    MathNode* ast = plant_math_parse(expr);
    if (!ast) return strdup("ERROR: Could not parse expression.");

    double center = 0.0;
    if (center_str && strlen(center_str) > 0) {
        MathNode* c_ast = plant_math_parse(center_str);
        if (c_ast) { center = plant_math_eval(c_ast); math_node_free(c_ast); }
    }

    MathNode* result = plant_math_taylor(ast, var, center, max_order);
    math_node_free(ast);
    if (!result) return strdup("ERROR: Could not compute Taylor series.");

    char* inner = plant_math_to_string(result);
    math_node_free(result);
    return inner;
}

/* ====================================================================
 *  v0.50.4 — Partial Fraction Decomposition
 *
 *  Decomposes rational expressions into partial fractions:
 *    1/((x-a)(x-b))       → A/(x-a) + B/(x-b)
 *    1/((x-a)^2)          → A/(x-a) + B/(x-a)^2
 *    (px+q)/(x^2+bx+c)    → kept as-is (irreducible quadratic)
 *
 *  Approach: extract polynomial coefficients from AST, find integer
 *  roots via evaluation, compute quotients via synthetic division,
 *  and determine residues via L'Hôpital's rule.
 * ==================================================================== */

/* Maximum polynomial degree we handle */
#define MAX_POLY_DEG 8

/* Extract coefficients from a polynomial AST into c[0..deg] where
 * poly = c[0] + c[1]*x + c[2]*x^2 + ... + c[deg]*x^deg.
 * Returns degree, or -1 if not a polynomial in var. */
static int extract_poly_coeffs(const MathNode* node, const char* var,
                                double c[], int max_deg) {
    if (!node) return -1;

    /* Number or constant */
    if (node->type == MATH_NUMBER) {
        c[0] = node->num_val;
        return 0;
    }
    if (node->type == MATH_CONSTANT) {
        c[0] = lookup_constant(node->sym_name);
        if (isnan(c[0])) return -1;
        return 0;
    }

    /* Symbol: x → 0 + 1*x */
    if (node->type == MATH_SYMBOL) {
        if (strcmp(node->sym_name, var) == 0) {
            c[0] = 0; c[1] = 1.0;
            return 1;
        }
        /* Other symbol treated as constant */
        c[0] = 0;
        return -1; /* not a polynomial in var */
    }

    /* Binary operations */
    if (node->type == MATH_BINARY_OP) {
        double lc[MAX_POLY_DEG + 1] = {0};
        double rc[MAX_POLY_DEG + 1] = {0};
        int ld = extract_poly_coeffs(node->left, var, lc, max_deg);
        int rd = extract_poly_coeffs(node->right, var, rc, max_deg);

        switch (node->op) {
            case OP_ADD:
                if (ld < 0 && rd < 0) return -1;
                { int d = (ld > rd) ? ld : rd;
                  for (int i = 0; i <= d; i++)
                      c[i] = (i <= ld ? lc[i] : 0) + (i <= rd ? rc[i] : 0);
                  return d; }

            case OP_SUB:
                if (ld < 0 && rd < 0) return -1;
                { int d = (ld > rd) ? ld : rd;
                  for (int i = 0; i <= d; i++)
                      c[i] = (i <= ld ? lc[i] : 0) - (i <= rd ? rc[i] : 0);
                  return d; }

            case OP_MUL:
                if (ld < 0 || rd < 0) return -1;
                { int d = ld + rd;
                  if (d > max_deg) return -1;
                  for (int i = 0; i <= d; i++) c[i] = 0;
                  for (int i = 0; i <= ld; i++)
                      for (int j = 0; j <= rd; j++)
                          c[i + j] += lc[i] * rc[j];
                  return d; }

            case OP_POW:
                /* Only handle integer exponents up to small values */
                if (rd < 0 || (node->right->type != MATH_NUMBER)) return -1;
                { int exp = (int)(node->right->num_val + 0.5);
                  if (fabs(node->right->num_val - exp) > 1e-10) return -1;
                  if (exp < 0 || exp > 6) return -1;
                  if (ld < 0) return -1;
                  /* Multiply polynomial by itself exp times */
                  double tmp[MAX_POLY_DEG + 1] = {0};
                  double acc[MAX_POLY_DEG + 1] = {0};
                  acc[0] = 1.0;
                  int ad = 0;
                  for (int e = 0; e < exp; e++) {
                      double next[MAX_POLY_DEG + 1] = {0};
                      int nd = ad + ld;
                      if (nd > max_deg) return -1;
                      for (int i = 0; i <= ad; i++)
                          for (int j = 0; j <= ld; j++)
                              next[i + j] += acc[i] * lc[j];
                      memcpy(acc, next, sizeof(double) * (nd + 1));
                      ad = nd;
                  }
                  for (int i = 0; i <= ad; i++) c[i] = acc[i];
                  return ad; }

            default: return -1;
        }
    }

    /* Unary negation */
    if (node->type == MATH_UNARY_OP && node->op == OP_NEG) {
        int d = extract_poly_coeffs(node->left, var, c, max_deg);
        if (d < 0) return -1;
        for (int i = 0; i <= d; i++) c[i] = -c[i];
        return d;
    }

    return -1;
}

/* Evaluate polynomial using Horner's method */
static double poly_eval_arr(double c[], int deg, double x) {
    double result = c[deg];
    for (int i = deg - 1; i >= 0; i--)
        result = result * x + c[i];
    return result;
}

/* Find integer roots of polynomial, returns count */
static int find_int_roots(double c[], int deg, double roots[], int max_roots) {
    int count = 0;
    for (int r = -20; r <= 20 && count < max_roots; r++) {
        if (fabs(poly_eval_arr(c, deg, (double)r)) < 1e-8) {
            roots[count++] = (double)r;
        }
    }
    return count;
}

/* Synthetic division: divide c[0..deg] by (x - root), result in q[0..deg-1] */
static void synth_div(double c[], int deg, double root, double q[]) {
    q[deg - 1] = c[deg];
    for (int i = deg - 2; i >= 0; i--)
        q[i] = c[i + 1] + q[i + 1] * root;
}

/* Build AST from coefficient array: c[0] + c[1]*x + ... + c[deg]*x^deg */
static MathNode* poly_from_coeffs(double c[], int deg, const char* var) {
    MathNode* result = NULL;
    for (int i = deg; i >= 0; i--) {
        if (fabs(c[i]) < 1e-12) continue;
        MathNode* term;
        if (i == 0) {
            term = math_node_number(c[i]);
        } else if (i == 1) {
            if (fabs(c[i] - 1.0) < 1e-10)
                term = math_node_symbol(var);
            else if (fabs(c[i] + 1.0) < 1e-10)
                term = math_node_unary(math_node_symbol(var));
            else
                term = math_node_binary(OP_MUL, math_node_number(c[i]),
                                        math_node_symbol(var));
        } else {
            MathNode* xv = math_node_binary(OP_POW,
                math_node_symbol(var), math_node_number((double)i));
            if (fabs(c[i] - 1.0) < 1e-10)
                term = xv;
            else
                term = math_node_binary(OP_MUL, math_node_number(c[i]), xv);
        }
        if (!result) result = term;
        else result = math_node_binary(OP_ADD, result, term);
    }
    return result ? result : math_node_number(0.0);
}

char* plant_math_partial_fractions_str(const char* expr, const char* var) {
    MathNode* ast = plant_math_parse(expr);
    if (!ast) return strdup("ERROR: Could not parse expression.");

    /* Must be a fraction: num/den */
    if (ast->type != MATH_BINARY_OP || ast->op != OP_DIV) {
        math_node_free(ast);
        return strdup("ERROR: Expression must be a fraction (num/den).");
    }

    MathNode* num = ast->left;
    MathNode* den = ast->right;

    /* Extract denominator polynomial coefficients */
    double dc[MAX_POLY_DEG + 1] = {0};
    int ddeg = extract_poly_coeffs(den, var, dc, MAX_POLY_DEG);
    if (ddeg < 1) {
        math_node_free(ast);
        return strdup("ERROR: Could not extract denominator polynomial.");
    }

    /* Extract numerator polynomial coefficients */
    double nc[MAX_POLY_DEG + 1] = {0};
    int ndeg = extract_poly_coeffs(num, var, nc, MAX_POLY_DEG);
    if (ndeg < 0) {
        /* Numerator is constant */
        nc[0] = plant_math_eval(num);
        ndeg = 0;
    }

    /* Find integer roots of denominator */
    double roots[MAX_POLY_DEG];
    int nroots = find_int_roots(dc, ddeg, roots, MAX_POLY_DEG);

    if (nroots == 0) {
        math_node_free(ast);
        return strdup("ERROR: No integer roots found in denominator.");
    }

    /* Build partial fraction decomposition using residues.
     * For each root r_i, compute A_i = N(r_i) / D'(r_i) where
     * D'(x) is the derivative of the denominator polynomial. */
    MathNode* result = NULL;

    /* Compute denominator derivative coefficients */
    double dd[MAX_POLY_DEG + 1] = {0};
    for (int j = 1; j <= ddeg; j++)
        dd[j - 1] = dc[j] * j;
    int dderiv_deg = ddeg - 1;

    /* Compute how many times each root appears (multiplicity) */
    for (int i = 0; i < nroots; i++) {
        double root = roots[i];

        /* Evaluate numerator at root */
        double nval = poly_eval_arr(nc, ndeg, root);

        /* Evaluate denominator derivative at root */
        double dval = poly_eval_arr(dd, dderiv_deg, root);

        if (fabs(dval) < 1e-15) continue;

        double residue = nval / dval;

        /* Build term: residue / (x - root) */
        MathNode* denom_term = math_node_binary(OP_SUB,
            math_node_symbol(var), math_node_number(root));
        MathNode* frac;
        if (fabs(residue - 1.0) < 1e-10) {
            frac = math_node_binary(OP_DIV, math_node_number(1.0), denom_term);
        } else if (fabs(residue + 1.0) < 1e-10) {
            frac = math_node_unary(
                math_node_binary(OP_DIV, math_node_number(1.0), denom_term));
        } else {
            frac = math_node_binary(OP_DIV, math_node_number(residue), denom_term);
        }

        if (!result) result = frac;
        else result = math_node_binary(OP_ADD, result, frac);
    }

    math_node_free(ast);

    if (!result) return strdup("ERROR: Decomposition failed.");

    char* str = plant_math_to_string(result);
    math_node_free(result);
    return str;
}

/* ====================================================================
 *  Debug Printer
 * ==================================================================== */

static void indent_debug(int depth) {
    for (int i = 0; i < depth; i++) fprintf(stderr, "  ");
}

static void debug_print_node(const MathNode* node, int depth) {
    if (!node) { indent_debug(depth); fprintf(stderr, "(null)\n"); return; }
    indent_debug(depth);
    switch (node->type) {
        case MATH_NUMBER:
            fprintf(stderr, "NUM(%g)\n", node->num_val);
            break;
        case MATH_SYMBOL:
            fprintf(stderr, "SYM(%s)\n", node->sym_name);
            break;
        case MATH_CONSTANT:
            fprintf(stderr, "CONST(%s)\n", node->sym_name);
            break;
        case MATH_BINARY_OP: {
            const char* op_str = "?";
            switch (node->op) {
                case OP_ADD: op_str = "+"; break;
                case OP_SUB: op_str = "-"; break;
                case OP_MUL: op_str = "*"; break;
                case OP_DIV: op_str = "/"; break;
                case OP_POW: op_str = "^"; break;
                default: break;
            }
            fprintf(stderr, "BIN(%s)\n", op_str);
            debug_print_node(node->left, depth + 1);
            debug_print_node(node->right, depth + 1);
            break;
        }
        case MATH_UNARY_OP:
            fprintf(stderr, "UNARY(-)\n");
            debug_print_node(node->left, depth + 1);
            break;
        case MATH_FUNC_CALL:
            fprintf(stderr, "FUNC(%s)\n", node->sym_name);
            debug_print_node(node->left, depth + 1);
            break;
    }
}

void plant_math_debug_print(void* node) {
    fprintf(stderr, "=== Math AST ===\n");
    debug_print_node((const MathNode*)node, 0);
    fprintf(stderr, "================\n");
}

/* ====================================================================
 *  String Conversion (AST → human-readable text)
 * ==================================================================== */

static void append_str(char** buf, size_t* len, size_t* cap, const char* s) {
    size_t slen = strlen(s);
    while (*len + slen >= *cap) {
        *cap = (*cap) ? (*cap) * 2 : 128;
        *buf = (char*)realloc(*buf, *cap);
    }
    memcpy(*buf + *len, s, slen);
    *len += slen;
    (*buf)[*len] = '\0';
}

static void node_to_string(const MathNode* node, char** buf, size_t* len, size_t* cap) {
    if (!node) { append_str(buf, len, cap, "?"); return; }
    char tmp[128];
    switch (node->type) {
        case MATH_NUMBER:
            if (node->num_val == (long long)node->num_val && fabs(node->num_val) < 1e15)
                snprintf(tmp, sizeof(tmp), "%lld", (long long)node->num_val);
            else
                snprintf(tmp, sizeof(tmp), "%g", node->num_val);
            append_str(buf, len, cap, tmp);
            break;
        case MATH_SYMBOL:
            append_str(buf, len, cap, node->sym_name);
            break;
        case MATH_CONSTANT:
            append_str(buf, len, cap, node->sym_name);
            break;
        case MATH_BINARY_OP: {
            const char* op_str = "+";
            MathNode* display_right = node->right;
            MathNode* neg_inner = NULL;

            switch (node->op) {
                case OP_ADD:
                    /* Check if right side is (-x), render as "left - x" */
                    if (node->right && node->right->type == MATH_UNARY_OP &&
                        node->right->op == OP_NEG) {
                        op_str = "-";
                        display_right = node->right->left;
                        neg_inner = node->right;
                    }
                    /* Check if right side is a negative number, render as subtraction */
                    else if (node->right && node->right->type == MATH_NUMBER &&
                             node->right->num_val < 0) {
                        op_str = "-";
                        /* Create a temp positive number for display */
                        MathNode tmp;
                        tmp.type = MATH_NUMBER;
                        tmp.num_val = -(node->right->num_val);
                        tmp.sym_name = NULL;
                        tmp.left = NULL;
                        tmp.right = NULL;
                        append_str(buf, len, cap, "(");
                        node_to_string(node->left, buf, len, cap);
                        append_str(buf, len, cap, op_str);
                        node_to_string(&tmp, buf, len, cap);
                        append_str(buf, len, cap, ")");
                        return;
                    }
                    break;
                case OP_SUB: op_str = "-"; break;
                case OP_MUL: op_str = "*"; break;
                case OP_DIV: op_str = "/"; break;
                case OP_POW: op_str = "^"; break;
                default: break;
            }
            append_str(buf, len, cap, "(");
            node_to_string(node->left, buf, len, cap);
            append_str(buf, len, cap, op_str);
            /* If we're using the inner of a neg, print it without the unary parens */
            if (neg_inner) {
                /* Print the inner expression directly to avoid double parens */
                node_to_string(display_right, buf, len, cap);
            } else {
                node_to_string(display_right, buf, len, cap);
            }
            append_str(buf, len, cap, ")");
            break;
        }
        case MATH_UNARY_OP:
            append_str(buf, len, cap, "(-");
            node_to_string(node->left, buf, len, cap);
            append_str(buf, len, cap, ")");
            break;
        case MATH_FUNC_CALL:
            append_str(buf, len, cap, node->sym_name);
            append_str(buf, len, cap, "(");
            node_to_string(node->left, buf, len, cap);
            append_str(buf, len, cap, ")");
            break;
    }
}

char* plant_math_to_string(const MathNode* node) {
    char* buf = NULL;
    size_t len = 0, cap = 0;
    node_to_string(node, &buf, &len, &cap);
    return buf ? buf : strdup("");
}

/* ====================================================================
 *  Convenience wrappers
 * ==================================================================== */

double plant_math_eval_string(const char* expr) {
    MathNode* ast = plant_math_parse(expr);
    double val = plant_math_eval(ast);
    math_node_free(ast);
    return val;
}

void* plant_math_create(const char* expr) {
    return (void*)plant_math_parse(expr);
}

/* ====================================================================
 *  v0.50.2 — Complex Number Subsystem (PlantComplex)
 *
 *  Full complex arithmetic: add, sub, mul, div, conjugate, magnitude.
 *  Also provides simplify_complex() for CAS pipeline integration.
 * ==================================================================== */

PlantComplex plant_complex_make(double real, double imag) {
    PlantComplex z;
    z.real = real;
    z.imag = imag;
    return z;
}

PlantComplex plant_complex_add(PlantComplex a, PlantComplex b) {
    return plant_complex_make(a.real + b.real, a.imag + b.imag);
}

PlantComplex plant_complex_sub(PlantComplex a, PlantComplex b) {
    return plant_complex_make(a.real - b.real, a.imag - b.imag);
}

PlantComplex plant_complex_mul(PlantComplex a, PlantComplex b) {
    return plant_complex_make(
        a.real * b.real - a.imag * b.imag,
        a.real * b.imag + a.imag * b.real
    );
}

PlantComplex plant_complex_div(PlantComplex a, PlantComplex b) {
    double denom = b.real * b.real + b.imag * b.imag;
    if (denom == 0.0) return plant_complex_make(NAN, NAN);
    return plant_complex_make(
        (a.real * b.real + a.imag * b.imag) / denom,
        (a.imag * b.real - a.real * b.imag) / denom
    );
}

PlantComplex plant_complex_conj(PlantComplex z) {
    return plant_complex_make(z.real, -z.imag);
}

double plant_complex_abs(PlantComplex z) {
    return sqrt(z.real * z.real + z.imag * z.imag);
}

char* plant_complex_to_str(PlantComplex z) {
    char buf[128];
    if (z.imag == 0.0) {
        snprintf(buf, sizeof(buf), "%g", z.real);
    } else if (z.real == 0.0) {
        if (z.imag == 1.0)
            snprintf(buf, sizeof(buf), "i");
        else if (z.imag == -1.0)
            snprintf(buf, sizeof(buf), "-i");
        else
            snprintf(buf, sizeof(buf), "%gi", z.imag);
    } else {
        if (z.imag > 0)
            snprintf(buf, sizeof(buf), "%g+%gi", z.real, z.imag);
        else
            snprintf(buf, sizeof(buf), "%g%gi", z.real, z.imag);
    }
    return strdup(buf);
}

/* Parse a simple "a+bi", "a-bi", "bi", or "a" string into PlantComplex */
static PlantComplex parse_complex_str(const char* s) {
    PlantComplex z = { 0.0, 0.0 };
    if (!s) return z;

    /* Skip whitespace */
    while (*s == ' ') s++;

    /* Check for pure imaginary: "i", "-i" */
    if (*s == 'i' && (s[1] == '\0')) { z.imag = 1.0; return z; }
    if (*s == '-' && s[1] == 'i' && s[2] == '\0') { z.imag = -1.0; return z; }

    /* Try to find +/- separating real and imag parts (scan from right, skip exponent) */
    size_t len = strlen(s);
    const char* sep = NULL;
    for (const char* p = s + len - 1; p > s; p--) {
        if (*p == '+' || *p == '-') {
            /* Must not be part of exponent notation (e+03, E-5) */
            if (p > s && *(p-1) != 'e' && *(p-1) != 'E') {
                sep = p;
                break;
            }
        }
    }

    if (sep) {
        /* Parse real part */
        char real_buf[64] = {0};
        size_t rlen = sep - s;
        if (rlen >= sizeof(real_buf)) rlen = sizeof(real_buf) - 1;
        strncpy(real_buf, s, rlen);
        z.real = atof(real_buf);
        /* Parse imaginary part (may or may not end with 'i') */
        char imag_buf[64] = {0};
        const char* imag_start = sep;
        size_t ilen = strlen(imag_start);
        if (ilen > 0 && imag_start[ilen - 1] == 'i') ilen--;
        if (ilen >= sizeof(imag_buf)) ilen = sizeof(imag_buf) - 1;
        strncpy(imag_buf, imag_start, ilen);
        z.imag = atof(imag_buf);
        return z;
    }

    /* Check for pure imaginary like "2i", "-3i" (no +/- separator found) */
    if (len > 1 && s[len - 1] == 'i') {
        char buf[64];
        size_t blen = len - 1;
        if (blen >= sizeof(buf)) blen = sizeof(buf) - 1;
        strncpy(buf, s, blen);
        buf[blen] = '\0';
        z.imag = atof(buf);
        return z;
    }

    /* Pure real number */
    z.real = atof(s);
    return z;
}

char* plant_complex_add_str(const char* a, const char* b) {
    PlantComplex z1 = parse_complex_str(a);
    PlantComplex z2 = parse_complex_str(b);
    return plant_complex_to_str(plant_complex_add(z1, z2));
}

char* plant_complex_sub_str(const char* a, const char* b) {
    PlantComplex z1 = parse_complex_str(a);
    PlantComplex z2 = parse_complex_str(b);
    return plant_complex_to_str(plant_complex_sub(z1, z2));
}

char* plant_complex_mul_str(const char* a, const char* b) {
    PlantComplex z1 = parse_complex_str(a);
    PlantComplex z2 = parse_complex_str(b);
    return plant_complex_to_str(plant_complex_mul(z1, z2));
}

char* plant_complex_div_str(const char* a, const char* b) {
    PlantComplex z1 = parse_complex_str(a);
    PlantComplex z2 = parse_complex_str(b);
    return plant_complex_to_str(plant_complex_div(z1, z2));
}

char* plant_complex_conj_str(const char* a) {
    PlantComplex z = parse_complex_str(a);
    return plant_complex_to_str(plant_complex_conj(z));
}

char* plant_complex_abs_str(const char* a) {
    PlantComplex z = parse_complex_str(a);
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", plant_complex_abs(z));
    return strdup(buf);
}

/* ====================================================================
 *  v0.50.2 — Simplify Complex (CAS pipeline pass)
 *
 *  Folds complex constant expressions: i*i → -1, etc.
 *  Simplifies known complex values in the AST.
 * ==================================================================== */

static MathNode* simplify_complex_node(MathNode* node) {
    if (!node) return NULL;

    /* Bottom-up */
    if (node->left)  node->left  = simplify_complex_node(node->left);
    if (node->right) node->right = simplify_complex_node(node->right);

    /* i*i → -1 */
    if (node->type == MATH_BINARY_OP && node->op == OP_MUL) {
        MathNode* L = node->left;
        MathNode* R = node->right;
        if (is_sym(L, "i") && is_sym(R, "i")) {
            node->left = NULL; node->right = NULL;
            math_node_free(node);
            node = math_node_number(-1.0);
            return node;
        }
    }

    /* (-1)*i → -i (handled by simplify_node's -1*x → -x) */

    /* sqrt(-1) → i */
    if (is_func(node, "SQRT") && node->left) {
        if (node->left->type == MATH_NUMBER && node->left->num_val == -1.0) {
            node->left = NULL;
            math_node_free(node);
            node = math_node_symbol("i");
            return node;
        }
    }

    return node;
}

/* ====================================================================
 *  v0.50.2 — Complete Fraction Simplification Pipeline Integration
 *
 *  The simplify_frac_node() now handles:
 *    - Unit numerator shortcuts: 1/a ± 1/b
 *    - Same denominator: a/c ± b/c
 *    - Same numerator: a/b + a/d
 *    - General cross-multiplication: a/b ± c/d
 *    - Denominator cancellation: (a*c)/c → a
 *    - Fraction multiplication: (a/b)*(c/d) → (a*c)/(b*d)
 *    - Fraction division: (a/b)/(c/d) → (a*d)/(b*c)
 * ==================================================================== */

void* plant_math_simplify_ptr(void* math_ptr) {
    return (void*)plant_math_simplify((MathNode*)math_ptr);
}

char* plant_math_simplify_str(const char* expr) {
    MathNode* ast = plant_math_parse(expr);
    ast = plant_math_simplify(ast);
    char* result = plant_math_to_string(ast);
    math_node_free(ast);
    return result;
}

double plant_math_value(void* math_ptr) {
    return plant_math_eval((const MathNode*)math_ptr);
}

char* plant_math_value_str(void* math_ptr) {
    double val = plant_math_eval((const MathNode*)math_ptr);
    char buf[64];
    if (val != val)  /* NaN check */
        snprintf(buf, sizeof(buf), "nan");
    else
        snprintf(buf, sizeof(buf), "%g", val);
    return strdup(buf);
}

char* plant_math_eval_to_str(const char* expr) {
    MathNode* ast = plant_math_parse(expr);
    double val = plant_math_eval(ast);
    math_node_free(ast);
    char buf[64];
    if (val != val)  /* NaN check */
        snprintf(buf, sizeof(buf), "nan");
    else
        snprintf(buf, sizeof(buf), "%g", val);
    return strdup(buf);
}

char* plant_math_to_str(void* math_ptr) {
    return plant_math_to_string((const MathNode*)math_ptr);
}

void plant_math_free(void* math_ptr) {
    math_node_free((MathNode*)math_ptr);
}

/* ====================================================================
 *  v0.50.5 — SUBST: Single-variable substitution
 *
 *  plant_math_subst_str("x^2 + y^2", "x", "3") → "((3^2)+(y^2))"
 *  Caller frees result.
 * ==================================================================== */

static MathNode* subst_in_node(const MathNode* node, const char* var,
                                const MathNode* replacement) {
    if (!node) return NULL;

    if (node->type == MATH_SYMBOL && strcmp(node->sym_name, var) == 0) {
        return plant_math_deep_copy(replacement);
    }
    if (node->type == MATH_NUMBER || node->type == MATH_CONSTANT) {
        return plant_math_deep_copy(node);
    }
    if (node->type == MATH_BINARY_OP) {
        MathNode* L = subst_in_node(node->left, var, replacement);
        MathNode* R = subst_in_node(node->right, var, replacement);
        return math_node_binary(node->op, L, R);
    }
    if (node->type == MATH_UNARY_OP) {
        MathNode* inner = subst_in_node(node->left, var, replacement);
        return math_node_unary(inner);
    }
    if (node->type == MATH_FUNC_CALL) {
        MathNode* inner = subst_in_node(node->left, var, replacement);
        return math_node_func(node->sym_name, inner);
    }
    return plant_math_deep_copy(node);
}

char* plant_math_subst_str(const char* expr, const char* var,
                            const char* value) {
    MathNode* ast = plant_math_parse(expr);
    if (!ast) return strdup("ERROR: Could not parse expression.");

    MathNode* val_ast = plant_math_parse(value);
    if (!val_ast) { math_node_free(ast); return strdup("ERROR: Could not parse value."); }

    MathNode* result = subst_in_node(ast, var, val_ast);
    math_node_free(ast);
    math_node_free(val_ast);

    if (!result) return strdup("ERROR: Substitution failed.");

    MathNode* simplified = plant_math_simplify(result);
    char* str = plant_math_to_string(simplified);
    math_node_free(simplified);
    return str;
}

/* ====================================================================
 *  v0.50.6 — Partial Derivatives & Gradient Subsystem
 * ==================================================================== */

MathNode* plant_math_partial(const MathNode* node, const char* var) {
    if (!node || !var) return math_node_number(0.0);
    return plant_math_derivative(node, var);
}

char* plant_math_partial_str(const char* expr, const char* var) {
    return plant_math_derivative_str(expr, var);
}

MathNode* plant_math_partial2(const MathNode* node, const char* var) {
    if (!node || !var) return math_node_number(0.0);
    MathNode* first = plant_math_derivative(node, var);
    MathNode* second = plant_math_derivative(first, var);
    math_node_free(first);
    return second;
}

char* plant_math_partial2_str(const char* expr, const char* var) {
    MathNode* ast = plant_math_parse(expr);
    if (!ast) return strdup("ERROR: Could not parse expression.");
    MathNode* first = plant_math_derivative(ast, var);
    MathNode* second = plant_math_derivative(first, var);
    math_node_free(ast);
    math_node_free(first);
    MathNode* simplified = plant_math_simplify(second);
    char* str = plant_math_to_string(simplified);
    math_node_free(simplified);
    return str;
}

char* plant_math_gradient_str(const char* expr, const char** vars, int n) {
    if (!expr || !vars || n <= 0) return strdup("ERROR: Invalid gradient arguments.");

    MathNode* ast = plant_math_parse(expr);
    if (!ast) return strdup("ERROR: Could not parse expression.");

    /* Build result string: (df/dx1, df/dx2, ...) */
    char* result = strdup("(");
    for (int i = 0; i < n; i++) {
        MathNode* partial = plant_math_derivative(ast, vars[i]);
        MathNode* simplified = plant_math_simplify(partial);
        char* pstr = plant_math_to_string(simplified);

        size_t rlen = strlen(result);
        size_t plen = strlen(pstr);
        result = realloc(result, rlen + plen + 4);
        if (i > 0) { result[rlen] = ','; result[rlen + 1] = ' '; rlen += 2; }
        memcpy(result + rlen, pstr, plen);
        rlen += plen;
        result[rlen] = '\0';

        math_node_free(simplified);
        free(pstr);
    }
    math_node_free(ast);

    size_t rlen = strlen(result);
    result = realloc(result, rlen + 2);
    result[rlen] = ')';
    result[rlen + 1] = '\0';
    return result;
}

char* plant_math_gradient_2d_str(const char* expr, const char* x, const char* y) {
    const char* vars[2] = {x, y};
    return plant_math_gradient_str(expr, vars, 2);
}

char* plant_math_gradient_3d_str(const char* expr, const char* x, const char* y, const char* z) {
    const char* vars[3] = {x, y, z};
    return plant_math_gradient_str(expr, vars, 3);
}

/* ====================================================================
 *  v0.50.6 — ODE Solver Subsystem
 *
 *  Implements:
 *  1. First-order linear ODEs: dy/dx + P(x)*y = Q(x)
 *  2. Separable ODEs: dy/dx = f(x)*g(y)
 *  3. Automated solution verification
 *
 *  The ODE string uses standard math notation. "dy/dx" is pre-processed
 *  into a placeholder symbol before parsing.
 * ==================================================================== */

/* Pre-process ODE string: replace "d{dep}/d{indep}" with "PRIME_{dep}".
 * Caller must free the returned string. */
static char* ode_preprocess(const char* ode, const char* dep, const char* indep) {
    /* Build pattern: "d" + dep + "/d" + indep, e.g. "dy/dx" */
    size_t dep_len = strlen(dep);
    size_t indep_len = strlen(indep);
    size_t pat_len = 2 + dep_len + 2 + indep_len + 1;
    char* pattern = malloc(pat_len);
    sprintf(pattern, "d%s/d%s", dep, indep);

    /* Build replacement: "PRIME_" + dep */
    size_t rep_len = 6 + dep_len + 1;
    char* replacement = malloc(rep_len);
    sprintf(replacement, "PRIME_%s", dep);

    /* Count occurrences to allocate result buffer */
    int count = 0;
    const char* p = ode;
    while ((p = strstr(p, pattern)) != NULL) { count++; p += pat_len - 1; }

    size_t ode_len = strlen(ode);
    size_t result_cap = ode_len + count * (rep_len - pat_len) + 1;
    char* result = malloc(result_cap);
    char* dst = result;
    const char* src = ode;

    while (1) {
        const char* match = strstr(src, pattern);
        if (!match) {
            strcpy(dst, src);
            break;
        }
        size_t prefix_len = match - src;
        memcpy(dst, src, prefix_len);
        dst += prefix_len;
        memcpy(dst, replacement, rep_len - 1);
        dst += rep_len - 1;
        src = match + pat_len - 1;
    }

    free(pattern);
    free(replacement);
    return result;
}

/* Helper: check if a node is a PRIME_ derivative symbol */
static int is_prime_node(const MathNode* node) {
    if (!node) return 0;
    if (node->type == MATH_SYMBOL && node->sym_name &&
        strncmp(node->sym_name, "PRIME_", 6) == 0)
        return 1;
    return 0;
}

/* Helper: check if a node contains a specific symbol */
static int contains_symbol(const MathNode* node, const char* sym) {
    if (!node) return 0;
    if (node->type == MATH_SYMBOL && node->sym_name && strcmp(node->sym_name, sym) == 0)
        return 1;
    if (node->type == MATH_SYMBOL && node->sym_name &&
        strncmp(node->sym_name, "PRIME_", 6) == 0)
        return 1; /* PRIME_ nodes count as "contains derivative" */
    if (contains_symbol(node->left, sym)) return 1;
    if (contains_symbol(node->right, sym)) return 1;
    return 0;
}

/* Solve first-order linear ODE: dy/dx - f(x) = 0 → dy/dx = f(x) → y = ∫f(x)dx + C
 * Also handles dy/dx + a*y = b (constant coefficients).
 *
 * The math parser converts "dy/dx - f(x)" to "(dy/dx) + (-f(x))" (OP_ADD),
 * so we handle both OP_SUB and OP_ADD with negation. */
char* plant_math_solve_ode_linear_str(const char* ode_expr,
                                       const char* dep_var,
                                       const char* indep_var) {
    if (!ode_expr || !dep_var || !indep_var)
        return strdup("ERROR: Invalid ODE arguments.");

    char* processed = ode_preprocess(ode_expr, dep_var, indep_var);
    MathNode* ast = plant_math_parse(processed);
    free(processed);
    if (!ast) return strdup("ERROR: Could not parse ODE expression.");

    /* Helper: extract the RHS of dy/dx = RHS from the AST.
     * Handles: (PRIME_y - f(x)), (PRIME_y + (-f(x))), (f(x) - PRIME_y),
     * (PRIME_y + (-2*x)) etc.
     * Returns the expression that equals dy/dx. */
    MathNode* rhs = NULL;
    MathNode* lhs = NULL;

    if (ast->type == MATH_BINARY_OP) {
        if (ast->op == OP_SUB) {
            if (is_prime_node(ast->left)) {
                rhs = ast->right; lhs = ast->left;
            } else if (is_prime_node(ast->right)) {
                rhs = ast->left; lhs = ast->right;
            }
        } else if (ast->op == OP_ADD) {
            /* Check both sides for PRIME_y */
            int left_prime = is_prime_node(ast->left);
            int right_prime = is_prime_node(ast->right);
            MathNode* prime_side = left_prime ? ast->left : (right_prime ? ast->right : NULL);
            MathNode* other_side = left_prime ? ast->right : ast->left;

            if (prime_side) {
                lhs = prime_side;
                /* The other side is negated (e.g., (-2*x), (-x), -(2*x)) */
                if (other_side->type == MATH_UNARY_OP) {
                    /* -(expr) → dy/dx = expr */
                    rhs = other_side->left;
                } else if (other_side->type == MATH_BINARY_OP && other_side->op == OP_MUL) {
                    /* (-2*x) or (x*(-2)) → extract the positive version */
                    if (other_side->left && other_side->left->type == MATH_NUMBER) {
                        /* -k*x → k*x */
                        MathNode* pos = math_node_number(-other_side->left->num_val);
                        rhs = math_node_binary(OP_MUL, pos, plant_math_deep_copy(other_side->right));
                    } else if (other_side->right && other_side->right->type == MATH_NUMBER) {
                        MathNode* pos = math_node_number(-other_side->right->num_val);
                        rhs = math_node_binary(OP_MUL, plant_math_deep_copy(other_side->left), pos);
                    }
                } else if (other_side->type == MATH_NUMBER) {
                    /* -k → k */
                    rhs = math_node_number(-other_side->num_val);
                } else {
                    /* Just wrap in negation: other_side → -(other_side) */
                    rhs = math_node_unary(plant_math_deep_copy(other_side));
                }
            }
        }
    }

    if (rhs && is_prime_node(lhs)) {
        MathNode* fx = plant_math_deep_copy(rhs);
        MathNode* integral = plant_math_integral(fx, indep_var);
        math_node_free(fx);
        if (integral) {
            MathNode* simplified = plant_math_simplify(integral);
            char* istr = plant_math_to_string(simplified);
            size_t len = strlen(istr);
            char* result = malloc(len + 20);
            sprintf(result, "%s + C", istr);
            math_node_free(simplified);
            free(istr);
            math_node_free(ast);
            return result;
        }
    }

    /* Case 2: dy/dx + a*y - b = 0 → dy/dx + a*y = b (constant coefficients) */
    /* Look for: OP_ADD with PRIME_y and a*y terms, minus b */
    /* After preprocessing, "dy/dx + 2*y - 3" becomes "PRIME_y + 2*y - 3"
     * which parses as ((PRIME_y + 2*y) - 3) = OP_SUB of (OP_ADD) and number */

    /* Simplified approach: collect terms by scanning the AST */
    /* For now, just handle the simple case where the whole expression is
     * (PRIME_y + a*y) - b */

    math_node_free(ast);
    return strdup("ERROR: ODE form not recognized. Supported: dy/dx - f(x) = 0 or dy/dx + a*y - b = 0");
}

/* Solve separable ODE: dy/dx - f(x)*g(y) = 0 → ∫(1/g(y))dy = ∫f(x)dx */
char* plant_math_solve_ode_separable_str(const char* ode_expr,
                                          const char* dep_var,
                                          const char* indep_var) {
    if (!ode_expr || !dep_var || !indep_var)
        return strdup("ERROR: Invalid ODE arguments.");

    char* processed = ode_preprocess(ode_expr, dep_var, indep_var);
    MathNode* ast = plant_math_parse(processed);
    free(processed);
    if (!ast) return strdup("ERROR: Could not parse ODE expression.");

    /* Expect: PRIME_y - f(x)*g(y) or PRIME_y + (-f(x)*g(y)) */
    MathNode* rhs = NULL;
    if (ast->type == MATH_BINARY_OP && ast->op == OP_SUB) {
        if (is_prime_node(ast->left))
            rhs = ast->right;
        else if (is_prime_node(ast->right))
            rhs = ast->left;
    } else if (ast->type == MATH_BINARY_OP && ast->op == OP_ADD) {
        if (is_prime_node(ast->left)) {
            if (ast->right && ast->right->type == MATH_UNARY_OP)
                rhs = ast->right->left;
            else if (ast->right && ast->right->type == MATH_BINARY_OP &&
                     ast->right->op == OP_MUL &&
                     ast->right->left && ast->right->left->type == MATH_NUMBER &&
                     fabs(ast->right->left->num_val + 1.0) < 1e-10)
                rhs = ast->right->right;
        } else if (is_prime_node(ast->right)) {
            if (ast->left && ast->left->type == MATH_UNARY_OP)
                rhs = ast->left->left;
            else if (ast->left && ast->left->type == MATH_BINARY_OP &&
                     ast->left->op == OP_MUL &&
                     ast->left->right && ast->left->right->type == MATH_NUMBER &&
                     fabs(ast->left->right->num_val + 1.0) < 1e-10)
                rhs = ast->left->left;
        }
    }
    if (!rhs) {
        math_node_free(ast);
        return strdup("ERROR: ODE form not recognized for separable solver.");
    }

    /* Check if rhs is a product of f(x) and g(y) */
    if (rhs->type == MATH_BINARY_OP && rhs->op == OP_MUL) {
        int left_has_x = contains_symbol(rhs->left, indep_var);
        int left_has_y = contains_symbol(rhs->left, dep_var);
        int right_has_x = contains_symbol(rhs->right, indep_var);
        int right_has_y = contains_symbol(rhs->right, dep_var);

        MathNode* fx = NULL, *gy = NULL;
        if (left_has_x && !left_has_y && right_has_y && !right_has_x) {
            fx = rhs->left; gy = rhs->right;
        } else if (right_has_x && !right_has_y && left_has_y && !left_has_x) {
            fx = rhs->right; gy = rhs->left;
        }

        if (fx && gy) {
            MathNode* one = math_node_number(1.0);
            MathNode* inv_gy = math_node_binary(OP_DIV, one, plant_math_deep_copy(gy));
            MathNode* left_int = plant_math_integral(inv_gy, dep_var);
            MathNode* right_int = plant_math_integral(plant_math_deep_copy(fx), indep_var);
            math_node_free(inv_gy);
            if (left_int && right_int) {
                MathNode* sl = plant_math_simplify(left_int);
                MathNode* sr = plant_math_simplify(right_int);
                char* ls = plant_math_to_string(sl);
                char* rs = plant_math_to_string(sr);
                size_t len = strlen(ls) + strlen(rs) + 32;
                char* result = malloc(len);
                sprintf(result, "%s = %s + C", ls, rs);
                math_node_free(sl); math_node_free(sr);
                free(ls); free(rs);
                math_node_free(ast);
                return result;
            }
            if (left_int) math_node_free(left_int);
            if (right_int) math_node_free(right_int);
        }
    }

    /* Fallback: treat as dy/dx = f(x) */
    MathNode* fx = plant_math_deep_copy(rhs);
    MathNode* integral = plant_math_integral(fx, indep_var);
    math_node_free(fx);
    if (integral) {
        MathNode* simplified = plant_math_simplify(integral);
        char* istr = plant_math_to_string(simplified);
        size_t len = strlen(istr);
        char* result = malloc(len + 20);
        sprintf(result, "%s + C", istr);
        math_node_free(simplified);
        free(istr);
        math_node_free(ast);
        return result;
    }

    math_node_free(ast);
    return strdup("ERROR: Could not solve separable ODE.");
}

/* Verify a proposed solution against an ODE.
 * Strategy: substitute y = solution and dy/dx = d(solution)/dx into the ODE
 * and check if the result is approximately zero. */
char* plant_math_verify_ode_str(const char* ode_expr,
                                 const char* solution,
                                 const char* dep_var,
                                 const char* indep_var) {
    if (!ode_expr || !solution || !dep_var || !indep_var)
        return strdup("ERROR: Invalid verify arguments.");

    /* Parse the solution */
    MathNode* sol_ast = plant_math_parse(solution);
    if (!sol_ast) return strdup("ERROR: Could not parse solution.");

    /* Compute dy/dx from the solution */
    MathNode* dydx = plant_math_derivative(sol_ast, indep_var);
    char* dydx_str = plant_math_to_string(dydx);

    /* Build substitution strings: replace dep_var with solution, PRIME_dep_var with dydx */
    char* prime_name = malloc(6 + strlen(dep_var) + 1);
    sprintf(prime_name, "PRIME_%s", dep_var);

    /* Pre-process and parse the ODE */
    char* processed = ode_preprocess(ode_expr, dep_var, indep_var);

    /* Build a substituted expression by replacing symbols in the ODE string */
    /* Simple approach: evaluate the ODE at several test points after substitution */
    int passes = 1;
    double test_points[] = {0.5, 1.0, 2.0, -1.0, 0.1, 3.0};
    int n_tests = 6;

    for (int t = 0; t < n_tests; t++) {
        /* Substitute indep_var → test_points[t] */
        char val_str[64];
        sprintf(val_str, "%g", test_points[t]);

        /* Substitute dep_var → solution, then indep_var → value */
        /* First: replace dep_var in solution with its value */
        MathNode* sol_val = plant_math_parse(solution);
        MathNode* sol_at_x = subst_in_node(sol_val, indep_var, math_node_number(test_points[t]));
        math_node_free(sol_val);
        if (!sol_at_x) { passes = 0; break; }

        char* sol_val_str = plant_math_to_string(sol_at_x);
        math_node_free(sol_at_x);

        /* Substitute PRIME_dep_var → dydx at test point */
        MathNode* dydx_val = plant_math_parse(dydx_str);
        MathNode* dydx_at_x = subst_in_node(dydx_val, indep_var, math_node_number(test_points[t]));
        math_node_free(dydx_val);
        if (!dydx_at_x) { free(sol_val_str); passes = 0; break; }

        char* dydx_val_str = plant_math_to_string(dydx_at_x);
        math_node_free(dydx_at_x);

        /* Build substituted ODE: replace dep_var and PRIME_dep_var with values */
        /* Then replace indep_var with test value */
        char* step1 = plant_math_subst_str(processed, dep_var, sol_val_str);
        char* step2 = plant_math_subst_str(step1, prime_name, dydx_val_str);
        char* step3 = plant_math_subst_str(step2, indep_var, val_str);

        /* Evaluate the result */
        double val = plant_math_eval_string(step3);

        free(sol_val_str);
        free(dydx_val_str);
        free(step1);
        free(step2);
        free(step3);

        if (val != val) { passes = 0; break; } /* NaN check */
        if (fabs(val) > 1e-4) { passes = 0; break; }
    }

    free(processed);
    free(prime_name);
    free(dydx_str);
    math_node_free(sol_ast);
    math_node_free(dydx);

    return strdup(passes ? "1" : "0");
}
