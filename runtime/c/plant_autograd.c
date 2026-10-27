/*
 * plant_autograd.c — v0.56.0a: reverse-mode Autograd (tape core).
 *
 * Records differentiable ops on a linked-list tape (reverse traversal
 * via `prev`) and runs a backward pass from the loss root.
 *
 * Scope: add/sub/mul/div/scale/sum/mean, exact shape only.
 * Broadcast-aware reduction, matmul and activations → v0.56.0b/c.
 */

#include "plant_autograd.h"
#include "plant_malloc.h"
#include "plant_memory.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* v0.61.1 — TD-066: relative-tolerance loss comparison (was bit-exact). */
/* _from_double uses %g (6 significant digits), so a DBL loss round-trips
   through a string and never matches the tape scalar bit-exactly.  1e-9
   relative still catches the silent-wrong case, which differs by a whole
   value (the loss is built from a previous gradient). */
static int _loss_matches(double a, double b) {
    double diff = fabs(a - b);
    double scale = fmax(1.0, fmax(fabs(a), fabs(b)));
    return diff <= 1e-9 * scale;
}

/* ── Tape ───────────────────────────────────────────────────── */
typedef struct TapeNode {
    int op;
    PlantTensor* in0;
    PlantTensor* in1;
    double       scalar;
    PlantTensor* out;
    struct TapeNode* prev;
} TapeNode;

static TapeNode* plant_tape_head = 0;

/* ── Pseudo-tensors (scalar-root reductions) ─────────────────── */
typedef struct PseudoEntry {
    PlantTensor* t;
    struct PseudoEntry* next;
} PseudoEntry;

static PseudoEntry* plant_pseudo_list = 0;

static void plant_pseudo_add(PlantTensor* t) {
    PseudoEntry* e = (PseudoEntry*)plant_malloc(sizeof(PseudoEntry));
    e->t = t;
    e->next = plant_pseudo_list;
    plant_pseudo_list = e;
}

static void plant_pseudo_free_all(void) {
    PseudoEntry* e = plant_pseudo_list;
    while (e) {
        PseudoEntry* n = e->next;
        plant_tensor_free(e->t);
        free(e);
        e = n;
    }
    plant_pseudo_list = 0;
}

/* ── Grad side-table (w->grad deferred to v0.56.0b) ──────────── */
typedef struct GradEntry {
    PlantTensor* t;
    PlantTensor* g;
    struct GradEntry* next;
} GradEntry;

static GradEntry* plant_grad_map = 0;

static PlantTensor* grad_get(PlantTensor* t) {
    GradEntry* e = plant_grad_map;
    while (e) { if (e->t == t) return e->g; e = e->next; }
    return 0;
}

static void grad_set(PlantTensor* t, PlantTensor* g) {
    GradEntry* e = (GradEntry*)plant_malloc(sizeof(GradEntry));
    e->t = t;
    e->g = g;
    e->next = plant_grad_map;
    plant_grad_map = e;
}

static PlantTensor* grad_or_zero(PlantTensor* t) {
    PlantTensor* g = grad_get(t);
    if (g) return g;
    g = plant_tensor_create(t->ndim, t->shape);
    /* plant_tensor_create zero-fills data. */
    grad_set(t, g);
    return g;
}

static void grad_free_all(void) {
    GradEntry* e = plant_grad_map;
    while (e) {
        GradEntry* n = e->next;
        plant_tensor_free(e->g);
        free(e);
        e = n;
    }
    plant_grad_map = 0;
}

/* ── v0.56.0b-2 — Broadcast-aware accumulation helpers ──────── */

/* target += sign * reduce(d, target-shape). */
static void ag_acc_reduced(PlantTensor* target, const PlantTensor* d,
                           double sign) {
    PlantTensor* r;
    int64_t i;
    if (!target) return;
    r = plant_tensor_bcast_reduce(d, target);
    if (plant_tensor_is_error(r)) { plant_tensor_free(r); return; }
    for (i = 0; i < r->size; i++) target->data[i] += sign * r->data[i];
    plant_tensor_free(r);
}

/* target += reduce(d ⊙ broadcast(u), target-shape). */
static void ag_acc_reduced_mul(PlantTensor* target, const PlantTensor* d,
                               const PlantTensor* u) {
    PlantTensor* full;
    int64_t oidx[16], i, dd;
    if (!target) return;
    full = plant_tensor_create(d->ndim, d->shape);
    if (plant_tensor_is_error(full)) { plant_tensor_free(full); return; }
    for (dd = 0; dd < d->ndim; dd++) oidx[dd] = 0;
    for (i = 0; i < d->size; i++) {
        int64_t ui = plant_tensor_bc_index((PlantTensor*)u, oidx, d->ndim);
        full->data[i] = d->data[i] * u->data[ui];
        for (dd = d->ndim - 1; dd >= 0; dd--) {
            oidx[dd]++;
            if (oidx[dd] < d->shape[dd]) break;
            oidx[dd] = 0;
        }
    }
    ag_acc_reduced(target, full, 1.0);
    plant_tensor_free(full);
}

/* target += reduce(d / broadcast(u), target-shape)   (for da). */
static void ag_acc_reduced_div_a(PlantTensor* target, const PlantTensor* d,
                                 const PlantTensor* u) {
    PlantTensor* full;
    int64_t oidx[16], i, dd;
    if (!target) return;
    full = plant_tensor_create(d->ndim, d->shape);
    if (plant_tensor_is_error(full)) { plant_tensor_free(full); return; }
    for (dd = 0; dd < d->ndim; dd++) oidx[dd] = 0;
    for (i = 0; i < d->size; i++) {
        int64_t ui = plant_tensor_bc_index((PlantTensor*)u, oidx, d->ndim);
        double bb = u->data[ui];
        full->data[i] = (bb != 0.0) ? (d->data[i] / bb) : 0.0;
        for (dd = d->ndim - 1; dd >= 0; dd--) {
            oidx[dd]++;
            if (oidx[dd] < d->shape[dd]) break;
            oidx[dd] = 0;
        }
    }
    ag_acc_reduced(target, full, 1.0);
    plant_tensor_free(full);
}

/* target += reduce(-d*a/b², target-shape)   (for db). */
static void ag_acc_reduced_div_b(PlantTensor* target, const PlantTensor* d,
                                 const PlantTensor* a, const PlantTensor* b) {
    PlantTensor* full;
    int64_t oidx[16], i, dd;
    if (!target) return;
    full = plant_tensor_create(d->ndim, d->shape);
    if (plant_tensor_is_error(full)) { plant_tensor_free(full); return; }
    for (dd = 0; dd < d->ndim; dd++) oidx[dd] = 0;
    for (i = 0; i < d->size; i++) {
        int64_t ai = plant_tensor_bc_index((PlantTensor*)a, oidx, d->ndim);
        int64_t bi = plant_tensor_bc_index((PlantTensor*)b, oidx, d->ndim);
        double bb = b->data[bi];
        full->data[i] = (bb != 0.0)
            ? (-d->data[i] * a->data[ai] / (bb * bb)) : 0.0;
        for (dd = d->ndim - 1; dd >= 0; dd--) {
            oidx[dd]++;
            if (oidx[dd] < d->shape[dd]) break;
            oidx[dd] = 0;
        }
    }
    ag_acc_reduced(target, full, 1.0);
    plant_tensor_free(full);
}

/* ── Init ───────────────────────────────────────────────────── */
void plant_autograd_init(void) {
    plant_tape_record_hook = plant_tape_record;
}

/* ── Record ─────────────────────────────────────────────────── */
void plant_tape_record(int op, PlantTensor* a, PlantTensor* b,
                       double s, PlantTensor* out) {
    TapeNode* n;
    /* Scalar-root: reductions return double; give the tape a
       1-element pseudo-tensor so backward has a root to seed. */
    if (!out && (op == OP_SUM || op == OP_MEAN)) {
        int64_t one[1];
        one[0] = 1;
        out = plant_tensor_create(1, one);
        out->data[0] = 0.0;
        plant_pseudo_add(out);
    }
    n = (TapeNode*)plant_malloc(sizeof(TapeNode));
    n->op = op;
    n->in0 = a;
    n->in1 = b;
    n->scalar = s;
    n->out = out;
    n->prev = plant_tape_head;
    plant_tape_head = n;
    if (a) plant_tensor_retain(a);
    if (b) plant_tensor_retain(b);
    if (out) plant_tensor_retain(out);
}

/* ── Clear ──────────────────────────────────────────────────── */
void plant_tape_clear(void) {
    TapeNode* n = plant_tape_head;
    while (n) {
        TapeNode* prev = n->prev;
        if (n->in0) plant_tensor_free(n->in0);
        if (n->in1) plant_tensor_free(n->in1);
        if (n->out) plant_tensor_free(n->out);
        free(n);
        n = prev;
    }
    plant_tape_head = 0;
    plant_pseudo_free_all();
}

/* ── Backward ───────────────────────────────────────────────── */
PlantTensor* plant_autograd_grad(PlantTensor* loss_t, double loss_d,
                                 PlantTensor* w, int use_tensor, int retain) {
    PlantTensor* result;
    PlantTensor* root;
    TapeNode* n;

    if (!w) return plant_tensor_create_error("grad: null w");
    if (!w->requires_grad) {
        plant_tape_clear();
        return plant_tensor_create_error("grad: w not tracked");
    }
    if (!plant_tape_head) return plant_tensor_create_error("grad: empty tape");

    if (use_tensor && loss_t) {
        root = loss_t;
    } else {
        root = plant_tape_head->out;
        if (!root) return plant_tensor_create_error("grad: null root");
        /* v0.57.4 — TD-048 hotfix: detect a second-derivative attempt.
           When retain=1 and the head is a reduction, the supplied loss
           must match head->scalar (same reduction). A mismatch means the
           "loss" is built from a previous gradient (silently-wrong);
           return a clean error.  v0.61.1 (TD-066): compare with a relative
           tolerance because the DBL loss round-trips through %g. */
        if (retain
            && (plant_tape_head->op == OP_SUM || plant_tape_head->op == OP_MEAN)
            && !_loss_matches(loss_d, plant_tape_head->scalar)) {
            plant_tape_clear();
            grad_free_all();
            return plant_tensor_create_error(
                "grad: loss does not match retained tape (second derivatives unsupported)");
        }
    }

    {
        PlantTensor* seed = plant_tensor_create(root->ndim, root->shape);
        int64_t i;
        for (i = 0; i < seed->size; i++) seed->data[i] = 1.0;
        grad_set(root, seed);
    }

    n = plant_tape_head;
    while (n) {
        PlantTensor* d = n->out ? grad_get(n->out) : 0;
        if (d) {
            if (n->op == OP_ADD) {
                /* v0.56.0b-2 — broadcasting-aware reduce. */
                ag_acc_reduced(grad_or_zero(n->in0), d, +1.0);
                if (n->in1) ag_acc_reduced(grad_or_zero(n->in1), d, +1.0);
            } else if (n->op == OP_SUB) {
                ag_acc_reduced(grad_or_zero(n->in0), d, +1.0);
                if (n->in1) ag_acc_reduced(grad_or_zero(n->in1), d, -1.0);
            } else if (n->op == OP_MUL) {
                ag_acc_reduced_mul(grad_or_zero(n->in0), d, n->in1);
                ag_acc_reduced_mul(grad_or_zero(n->in1), d, n->in0);
            } else if (n->op == OP_DIV) {
                ag_acc_reduced_div_a(grad_or_zero(n->in0), d, n->in1);
                ag_acc_reduced_div_b(grad_or_zero(n->in1), d, n->in0, n->in1);
            } else if (n->op == OP_SCALE) {
                PlantTensor* da = grad_or_zero(n->in0);
                int64_t i;
                for (i = 0; i < d->size; i++) da->data[i] += d->data[i] * n->scalar;
            } else if (n->op == OP_SUM) {
                PlantTensor* da = grad_or_zero(n->in0);
                int64_t i;
                for (i = 0; i < da->size; i++) da->data[i] += d->data[0];
            } else if (n->op == OP_MEAN) {
                PlantTensor* da = grad_or_zero(n->in0);
                double inv = 1.0 / (double)da->size;
                int64_t i;
                for (i = 0; i < da->size; i++) da->data[i] += d->data[0] * inv;
            } else if (n->op == OP_MATMUL) {
                /* dA[i][j] = Sum_l d[i][l]*B[j][l];
                   dB[j][l] = Sum_i A[i][j]*d[i][l] */
                int64_t m = n->in0->shape[0];
                int64_t k = n->in0->shape[1];
                int64_t p = n->in1->shape[1];
                PlantTensor* da = plant_tensor_requires_grad(n->in0)
                                  ? grad_or_zero(n->in0) : 0;
                PlantTensor* db = plant_tensor_requires_grad(n->in1)
                                  ? grad_or_zero(n->in1) : 0;
                int64_t i, j, l;
                for (i = 0; i < m; i++) {
                    for (j = 0; j < k; j++) {
                        double sum = 0.0;
                        for (l = 0; l < p; l++)
                            sum += d->data[i * p + l] * n->in1->data[j * p + l];
                        if (da) da->data[i * k + j] += sum;
                    }
                }
                for (j = 0; j < k; j++) {
                    for (l = 0; l < p; l++) {
                        double sum = 0.0;
                        for (i = 0; i < m; i++)
                            sum += n->in0->data[i * k + j] * d->data[i * p + l];
                        if (db) db->data[j * p + l] += sum;
                    }
                }
            } else if (n->op == OP_RELU) {
                PlantTensor* da = grad_or_zero(n->in0);
                int64_t i;
                for (i = 0; i < d->size; i++)
                    da->data[i] += d->data[i] * (n->in0->data[i] > 0.0 ? 1.0 : 0.0);
            } else if (n->op == OP_SIGMOID) {
                PlantTensor* da = grad_or_zero(n->in0);
                int64_t i;
                for (i = 0; i < d->size; i++) {
                    double s = n->out->data[i];
                    da->data[i] += d->data[i] * s * (1.0 - s);
                }
            } else if (n->op == OP_TANH) {
                PlantTensor* da = grad_or_zero(n->in0);
                int64_t i;
                for (i = 0; i < d->size; i++) {
                    double t = n->out->data[i];
                    da->data[i] += d->data[i] * (1.0 - t * t);
                }
            } else if (n->op == OP_TRANSPOSE) {
                /* da[i][j] += d[j][i] */
                PlantTensor* da = grad_or_zero(n->in0);
                int64_t m = n->in0->shape[0];
                int64_t nn = n->in0->shape[1];
                int64_t i, j;
                for (i = 0; i < m; i++)
                    for (j = 0; j < nn; j++)
                        da->data[i * nn + j] += d->data[j * m + i];
            } else if (n->op == OP_RESHAPE) {
                /* v0.56.0b-1 — ACCUMULATE (+=), not memcpy. */
                PlantTensor* da = grad_or_zero(n->in0);
                int64_t i;
                for (i = 0; i < da->size; i++)
                    da->data[i] += d->data[i];
            } else if (n->op == OP_SOFTMAX) {
                /* v0.56.0c — softmax backward (per axis slice).
                   dx_i = out_i * (d_i - Sum_j(out_j * d_j)) */
                PlantTensor* da = grad_or_zero(n->in0);
                int axis = (int)n->scalar;
                if (n->out->ndim == 1) {
                    double sum = 0.0;
                    int64_t i;
                    for (i = 0; i < d->size; i++) sum += n->out->data[i] * d->data[i];
                    for (i = 0; i < d->size; i++)
                        da->data[i] += n->out->data[i] * (d->data[i] - sum);
                } else if (n->out->ndim == 2) {
                    int64_t m = n->out->shape[0], p = n->out->shape[1];
                    if (axis == 1) {
                        int64_t i, j;
                        for (i = 0; i < m; i++) {
                            double sum = 0.0;
                            for (j = 0; j < p; j++)
                                sum += n->out->data[i*p+j] * d->data[i*p+j];
                            for (j = 0; j < p; j++)
                                da->data[i*p+j] += n->out->data[i*p+j] * (d->data[i*p+j] - sum);
                        }
                    } else {
                        int64_t i, j;
                        for (j = 0; j < p; j++) {
                            double sum = 0.0;
                            for (i = 0; i < m; i++)
                                sum += n->out->data[i*p+j] * d->data[i*p+j];
                            for (i = 0; i < m; i++)
                                da->data[i*p+j] += n->out->data[i*p+j] * (d->data[i*p+j] - sum);
                        }
                    }
                }
            } else if (n->op == OP_GELU) {
                /* v0.56.0c — gelu backward (tanh approx). */
                const double K = 0.7978845608028654;
                const double COEF = 0.044715;
                PlantTensor* da = grad_or_zero(n->in0);
                int64_t i;
                for (i = 0; i < d->size; i++) {
                    double x = n->in0->data[i];
                    double u = K * (x + COEF * x * x * x);
                    double t = tanh(u);
                    double du = K * (1.0 + 3.0 * COEF * x * x);
                    da->data[i] += d->data[i] *
                        (0.5 * (1.0 + t) + 0.5 * x * (1.0 - t * t) * du);
                }
            } else if (n->op == OP_SWISH) {
                /* v0.56.0c — swish backward. s = sigmoid(x);
                   dx = d * (s + x*s*(1-s)). */
                PlantTensor* da = grad_or_zero(n->in0);
                int64_t i;
                for (i = 0; i < d->size; i++) {
                    double x = n->in0->data[i];
                    double s = 1.0 / (1.0 + exp(-x));
                    da->data[i] += d->data[i] * (s + x * s * (1.0 - s));
                }
            } else if (n->op == OP_MISH) {
                /* v0.56.0c — mish backward.
                   sp = softplus(x); t = tanh(sp); sp' = sigmoid(x);
                   dx = d * (t + x*(1-t^2)*sp'). */
                PlantTensor* da = grad_or_zero(n->in0);
                int64_t i;
                for (i = 0; i < d->size; i++) {
                    double x = n->in0->data[i];
                    double sp = (x > 0.0 ? x : 0.0) + log1p(exp(-fabs(x)));
                    double t = tanh(sp);
                    double sig = 1.0 / (1.0 + exp(-x));
                    da->data[i] += d->data[i] * (t + x * (1.0 - t * t) * sig);
                }
            }
        }
        n = n->prev;
    }

    {
        PlantTensor* g = grad_get(w);
        if (!g) {
            plant_tape_clear();
            grad_free_all();
            return plant_tensor_create_error("grad: w not in graph");
        }
        result = plant_tensor_create(g->ndim, g->shape);
        memcpy(result->data, g->data, sizeof(double) * (size_t)g->size);
    }

    /* v0.57.3 — retain_graph: keep the tape when retain != 0.
       The grad side-table is always reset so the next GRAD re-seeds. */
    if (!retain) {
        plant_tape_clear();
    }
    grad_free_all();
    return result;
}

/* ── Convenience GRAD (2-arg; mapper target) ────────────────── */
PlantTensor* plant_grad(double loss_d, PlantTensor* w) {
    return plant_autograd_grad(0, loss_d, w, 0, 0);  /* retain = 0 */
}

/* v0.57.3 — 3-arg GRAD with explicit retain_graph flag. */
PlantTensor* plant_grad_retain(double loss_d, PlantTensor* w, int retain) {
    return plant_autograd_grad(0, loss_d, w, 0, retain);
}

/* v0.57.3 — TAPE_CLEAR(): explicit tape cleanup (idempotent). */
void plant_tape_clear_public(void) {
    plant_tape_clear();
}
