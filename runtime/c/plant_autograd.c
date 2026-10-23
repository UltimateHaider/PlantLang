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
                                 PlantTensor* w, int use_tensor) {
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
        (void)loss_d;
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
                PlantTensor* da = grad_or_zero(n->in0);
                PlantTensor* db = n->in1 ? grad_or_zero(n->in1) : 0;
                int64_t i;
                for (i = 0; i < d->size; i++) {
                    da->data[i] += d->data[i];
                    if (db) db->data[i] += d->data[i];
                }
            } else if (n->op == OP_SUB) {
                PlantTensor* da = grad_or_zero(n->in0);
                PlantTensor* db = n->in1 ? grad_or_zero(n->in1) : 0;
                int64_t i;
                for (i = 0; i < d->size; i++) {
                    da->data[i] += d->data[i];
                    if (db) db->data[i] -= d->data[i];
                }
            } else if (n->op == OP_MUL) {
                PlantTensor* da = grad_or_zero(n->in0);
                PlantTensor* db = grad_or_zero(n->in1);
                int64_t i;
                for (i = 0; i < d->size; i++) {
                    da->data[i] += d->data[i] * n->in1->data[i];
                    db->data[i] += d->data[i] * n->in0->data[i];
                }
            } else if (n->op == OP_DIV) {
                PlantTensor* da = grad_or_zero(n->in0);
                PlantTensor* db = grad_or_zero(n->in1);
                int64_t i;
                for (i = 0; i < d->size; i++) {
                    double b = n->in1->data[i];
                    da->data[i] += d->data[i] / b;
                    db->data[i] -= d->data[i] * n->in0->data[i] / (b * b);
                }
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

    plant_tape_clear();
    grad_free_all();
    return result;
}

/* ── Convenience GRAD (2-arg; mapper target) ────────────────── */
PlantTensor* plant_grad(double loss_d, PlantTensor* w) {
    return plant_autograd_grad(0, loss_d, w, 0);
}
