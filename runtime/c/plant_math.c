/*
 * plant_math.c — v0.50.0f: Symbolic Math Core
 *
 * Full implementation of the PlantLang symbolic algebra subsystem.
 * Tokenizer → Pratt parser (precedence climbing) → AST evaluator.
 */

#include "plant_math.h"
#include <ctype.h>
#include <float.h>

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
    n->sym_name = strdup(name);
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

static const MathFuncEntry MATH_FUNCS[] = {
    { "SIN",  fn_sin  },
    { "COS",  fn_cos  },
    { "TAN",  fn_tan  },
    { "SQRT", fn_sqrt },
    { "EXP",  fn_exp  },
    { "LOG",  fn_log  },
    { "ABS",  fn_abs  },
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

/* Public simplification entry point — iterates until fixed point or cap */
MathNode* plant_math_simplify(MathNode* node) {
    if (!node) return NULL;
    int iterations = 0;
    int changed = 1;
    while (changed && iterations < MAX_SIMPLIFY_ITERATIONS) {
        changed = 0;
        MathNode* before = plant_math_deep_copy(node);
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
            switch (node->op) {
                case OP_ADD: op_str = "+"; break;
                case OP_SUB: op_str = "-"; break;
                case OP_MUL: op_str = "*"; break;
                case OP_DIV: op_str = "/"; break;
                case OP_POW: op_str = "^"; break;
                default: break;
            }
            append_str(buf, len, cap, "(");
            node_to_string(node->left, buf, len, cap);
            append_str(buf, len, cap, op_str);
            node_to_string(node->right, buf, len, cap);
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
