/*
 * plant_ad2.c — v0.60.0a: isolated reverse-mode ADv2 (higher-order AD).
 *
 * A SECOND, independent tape engine behind GRAD_HO(...). It chains the
 * single forward hook in plant_tensor.c, records into its own tape
 * (frames), and runs a TAPED backward so the gradients are themselves
 * recorded and therefore differentiable.
 *
 * The original first-order engine (plant_autograd.c / GRAD) is untouched:
 * plant_ad2_record forwards every event to the previously installed hook
 * BEFORE recording its own copy.
 *
 * Supported in v0.60.0a (EASY, linear + elementwise):
 *   OP_ADD OP_SUB OP_MUL(same-shape) OP_SCALE OP_SUM OP_MEAN
 *   OP_TRANSPOSE OP_RESHAPE  (+ local A2_BCAST / A2_AXIS_SUM)
 * Everything else returns an explicit error tensor (never silent-wrong).
 */

#include "plant_ad2.h"
#include "plant_autograd.h"
#include "plant_malloc.h"
#include "plant_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define A2_MAXDIM 16

/* ── Engine state ───────────────────────────────────────────── */
static void (*saved_hook)(int, PlantTensor*, PlantTensor*, double, PlantTensor*) = 0;
static A2Frame* frame_top = 0;

/* Per-pass gradient map (accumulator per tensor). Map owns its ref. */
typedef struct A2Grad {
    PlantTensor* t;
    PlantTensor* g;
    struct A2Grad* next;
} A2Grad;
static A2Grad* grad_map = 0;

static PlantTensor* a2_zeros_like(PlantTensor* t) {
    PlantTensor* o;
    if (!t) return 0;
    o = plant_tensor_create(t->ndim, t->shape);
    return o;
}

static void a2_emit(int op, PlantTensor* a, PlantTensor* b, double s,
                    PlantTensor* out, int pseudo) {
    A2Node* n;
    if (!frame_top || frame_top->frozen) return;
    n = (A2Node*)plant_malloc(sizeof(A2Node));
    n->op = op;
    n->in0 = a;
    n->in1 = b;
    n->scalar = s;
    n->out = out;
    n->pseudo = pseudo;
    n->prev = frame_top->head;
    frame_top->head = n;
    if (a) plant_tensor_retain(a);
    if (b) plant_tensor_retain(b);
    if (out && !pseudo) plant_tensor_retain(out);
}

static void plant_ad2_record(int op, PlantTensor* a, PlantTensor* b,
                             double s, PlantTensor* out) {
    /* 1) Forward to the previously installed hook (first-order GRAD). */
    if (saved_hook) saved_hook(op, a, b, s, out);
    /* 2) Record our own copy while a frame is recording. */
    if (!frame_top || frame_top->frozen) return;
    if (!out && (op == OP_SUM || op == OP_MEAN)) {
        /* v0.56.0a pattern: scalar roots get a 1-element pseudo tensor. */
        int64_t one[1];
        one[0] = 1;
        out = plant_tensor_create(1, one);
        if (!out || plant_tensor_is_error(out)) return;
        out->data[0] = 0.0;
        a2_emit(op, a, b, s, out, 1);
        return;
    }
    a2_emit(op, a, b, s, out, 0);
}

/* ── Frames ─────────────────────────────────────────────────── */
static void a2_frame_free(A2Frame* f) {
    A2Node* n = f->head;
    while (n) {
        A2Node* p = n->prev;
        if (n->in0) plant_tensor_free(n->in0);
        if (n->in1) plant_tensor_free(n->in1);
        if (n->out) plant_tensor_free(n->out);
        free(n);
        n = p;
    }
    free(f);
}

static void a2_push_frame(int level, int frozen) {
    A2Frame* f = (A2Frame*)plant_malloc(sizeof(A2Frame));
    f->head = 0;
    f->frozen = frozen;
    f->level = level;
    f->prev = frame_top;
    frame_top = f;
}

void plant_ad2_init(void) {
    if (saved_hook) return;               /* idempotent */
    saved_hook = plant_tape_record_hook;
    plant_tape_record_hook = plant_ad2_record;
    if (!frame_top) a2_push_frame(0, 0);  /* forward frame */
}

void plant_ad2_clear(void) {
    while (frame_top) {
        A2Frame* p = frame_top->prev;
        a2_frame_free(frame_top);
        frame_top = p;
    }
    /* NOTE: do not re-push a frame here — a TAPE_CLEAR() discards the
       tape by design; the next GRAD_HO reports `empty tape`. */
}

/* ── Gradient map ───────────────────────────────────────────── */
static PlantTensor* gm_get(PlantTensor* t) {
    A2Grad* e = grad_map;
    while (e) { if (e->t == t) return e->g; e = e->next; }
    return 0;
}

/* Takes ownership of `g` (no extra retain). */
static void gm_set(PlantTensor* t, PlantTensor* g) {
    A2Grad* e = grad_map;
    while (e) {
        if (e->t == t) {
            if (e->g) plant_tensor_free(e->g);
            e->g = g;
            return;
        }
        e = e->next;
    }
    e = (A2Grad*)plant_malloc(sizeof(A2Grad));
    e->t = t;
    e->g = g;
    e->next = grad_map;
    grad_map = e;
}

static void gm_free_all(void) {
    A2Grad* e = grad_map;
    while (e) {
        A2Grad* n = e->next;
        if (e->g) plant_tensor_free(e->g);
        free(e);
        e = n;
    }
    grad_map = 0;
}

/* Accumulate `contrib` (ownership transferred) into `target`'s gradient. */
static void a2_acc(PlantTensor* target, PlantTensor* contrib) {
    PlantTensor* old;
    PlantTensor* sum;
    if (!target || !contrib) { if (contrib) plant_tensor_free(contrib); return; }
    if (plant_tensor_is_error(contrib)) { plant_tensor_free(contrib); return; }
    old = gm_get(target);
    if (!old) {
        old = a2_zeros_like(target);
        if (!old || plant_tensor_is_error(old)) { plant_tensor_free(contrib); return; }
        gm_set(target, old);            /* map owns old */
    }
    /* Make sure the accumulation is recorded (grads must be differentiable). */
    plant_tensor_track(old);
    sum = plant_tensor_add(old, contrib);
    plant_tensor_free(contrib);         /* recorded node (if any) retained it */
    if (!sum || plant_tensor_is_error(sum)) { if (sum) plant_tensor_free(sum); return; }
    gm_set(target, sum);                /* frees the previous map ref */
}

/* ── ADv2-local ops: broadcast / axis-sum (adjoints of each other) ── */
static PlantTensor* a2_bcast(PlantTensor* d, PlantTensor* like) {
    PlantTensor* o;
    int64_t oidx[A2_MAXDIM];
    int64_t i, dd;
    if (!d || !like) return plant_tensor_create_error("ad2: bcast null");
    o = plant_tensor_create(like->ndim, like->shape);
    if (!o || plant_tensor_is_error(o)) return o;
    for (dd = 0; dd < like->ndim; dd++) oidx[dd] = 0;
    for (i = 0; i < o->size; i++) {
        int64_t si = plant_tensor_bc_index(d, oidx, like->ndim);
        o->data[i] = d->data[si];
        for (dd = like->ndim - 1; dd >= 0; dd--) {
            oidx[dd]++;
            if (oidx[dd] < like->shape[dd]) break;
            oidx[dd] = 0;
        }
    }
    a2_emit(A2_BCAST, d, 0, 0.0, o, 0);
    return o;
}

static PlantTensor* a2_axis_sum(PlantTensor* d, PlantTensor* like) {
    PlantTensor* o;
    int64_t oidx[A2_MAXDIM];
    int64_t i, dd;
    if (!d || !like) return plant_tensor_create_error("ad2: axis_sum null");
    o = plant_tensor_create(like->ndim, like->shape);
    if (!o || plant_tensor_is_error(o)) return o;
    for (i = 0; i < o->size; i++) o->data[i] = 0.0;
    for (dd = 0; dd < d->ndim; dd++) oidx[dd] = 0;
    for (i = 0; i < d->size; i++) {
        int64_t oi = plant_tensor_bc_index(o, oidx, d->ndim);
        o->data[oi] += d->data[i];
        for (dd = d->ndim - 1; dd >= 0; dd--) {
            oidx[dd]++;
            if (oidx[dd] < d->shape[dd]) break;
            oidx[dd] = 0;
        }
    }
    a2_emit(A2_AXIS_SUM, d, 0, 0.0, o, 0);
    return o;
}

static PlantTensor* a2_reshape_like(PlantTensor* d, PlantTensor* like) {
    PlantArray* sh;
    PlantTensor* r;
    int64_t k;
    if (!d || !like) return plant_tensor_create_error("ad2: reshape null");
    sh = plant_list_make(0);
    for (k = 0; k < like->ndim; k++)
        sh = plant_list_push(sh, _from_long(like->shape[k]));
    r = plant_tensor_reshape(d, sh);
    plant_list_free(sh);
    return r;
}

/* Untracked constant tensor (for 1-eps expressions / masks). */
static PlantTensor* a2_const_like(PlantTensor* t, double v) {
    PlantTensor* o = plant_tensor_create(t->ndim, t->shape);
    int64_t i;
    if (!o || plant_tensor_is_error(o)) return o;
    for (i = 0; i < o->size; i++) o->data[i] = v;
    return o;
}

/* 1 when any element is exactly zero (division guards). */
static int a2_has_zero(PlantTensor* t) {
    int64_t i;
    if (!t) return 0;
    for (i = 0; i < t->size; i++) if (t->data[i] == 0.0) return 1;
    return 0;
}

/* Scratch list: intermediates created by a MID backward rule. Every
   entry is released at the end of the rule (the tape holds its own
   retain on whatever was recorded). */
static PlantTensor* a2_keep[96];
static int a2_nkeep = 0;

static PlantTensor* a2_k(PlantTensor* t) {
    if (t && a2_nkeep < 96) a2_keep[a2_nkeep++] = t;
    return t;
}

static void a2_release(void) {
    int i;
    for (i = 0; i < a2_nkeep; i++) plant_tensor_free(a2_keep[i]);
    a2_nkeep = 0;
}

/* ── Backward for one frame's nodes (taped) ─────────────────── */
/* Returns NULL on success, an error tensor on failure. The gradient map
   is shared across ALL frames of the current level (v0.60.0a fix: a
   level-k backward must also traverse the older frames, because the
   gradients recorded in frame k-1 depend on FORWARD tensors). */
static PlantTensor* a2_backprop_nodes(A2Frame* f) {
    A2Node* n;
    for (n = f->head; n; n = n->prev) {
        PlantTensor* d = gm_get(n->out);
        if (!d) continue;
        if (plant_tensor_is_error(d)) continue;
        switch (n->op) {
            case OP_ADD:
                a2_acc(n->in0, plant_tensor_deep_copy(d));
                if (n->in1) a2_acc(n->in1, plant_tensor_deep_copy(d));
                break;
            case OP_SUB:
                a2_acc(n->in0, plant_tensor_deep_copy(d));
                if (n->in1) a2_acc(n->in1, plant_tensor_scale(d, -1.0));
                break;
            case OP_MUL:
                if (!n->in0 || !n->in1 || n->in0->size != n->in1->size) {
                    return plant_tensor_create_error("ad2: broadcast mul unsupported (0c)");
                }
                a2_acc(n->in0, plant_tensor_mul(d, n->in1));
                a2_acc(n->in1, plant_tensor_mul(d, n->in0));
                break;
            case OP_SCALE:
                a2_acc(n->in0, plant_tensor_scale(d, n->scalar));
                break;
            case OP_SUM:
                a2_acc(n->in0, a2_bcast(d, n->in0));
                break;
            case OP_MEAN:
                a2_acc(n->in0, plant_tensor_scale(a2_bcast(d, n->in0),
                                                  1.0 / (double)n->in0->size));
                break;
            case OP_TRANSPOSE:
                a2_acc(n->in0, plant_tensor_transpose(d));
                break;
            case OP_RESHAPE:
                a2_acc(n->in0, a2_reshape_like(d, n->in0));
                break;
            /* ── v0.60.0b — MID ops (taped double-backward) ────── */
            case OP_DIV: {
                PlantTensor* t1;
                PlantTensor* t2;
                PlantTensor* t3;
                if (!n->in0 || !n->in1 || n->in0->size != n->in1->size)
                    return plant_tensor_create_error("ad2: broadcast div unsupported (0c)");
                if (a2_has_zero(n->in1))
                    return plant_tensor_create_error("ad2: division by zero in backward");
                /* da = d / b */
                a2_acc(n->in0, plant_tensor_div(d, n->in1));
                /* db = -(d * a) / (b * b) */
                t1 = a2_k(plant_tensor_mul(d, n->in0));
                t2 = a2_k(plant_tensor_mul(n->in1, n->in1));
                t3 = a2_k(plant_tensor_div(t1, t2));
                a2_acc(n->in1, plant_tensor_scale(t3, -1.0));
                a2_release();
                break;
            }
            case OP_MATMUL: {
                PlantTensor* tb = a2_k(plant_tensor_transpose(n->in1));
                PlantTensor* ta = a2_k(plant_tensor_transpose(n->in0));
                PlantTensor* da_c = plant_tensor_matmul(d, tb);
                PlantTensor* db_c = plant_tensor_matmul(ta, d);
                a2_acc(n->in0, da_c);
                a2_acc(n->in1, db_c);
                a2_release();
                break;
            }
            case OP_RELU: {
                PlantTensor* mask = a2_const_like(n->in0, 0.0);
                int64_t k;
                if (!mask || plant_tensor_is_error(mask)) return mask;
                for (k = 0; k < mask->size; k++)
                    mask->data[k] = (n->in0->data[k] > 0.0) ? 1.0 : 0.0;
                a2_acc(n->in0, plant_tensor_mul(d, mask));
                plant_tensor_free(mask);
                break;
            }
            case OP_SIGMOID: {
                PlantTensor* one = a2_k(a2_const_like(n->out, 1.0));
                PlantTensor* om = a2_k(plant_tensor_sub(one, n->out));
                PlantTensor* prod = a2_k(plant_tensor_mul(n->out, om));
                a2_acc(n->in0, plant_tensor_mul(d, prod));
                a2_release();
                break;
            }
            case OP_TANH: {
                PlantTensor* one = a2_k(a2_const_like(n->out, 1.0));
                PlantTensor* sq = a2_k(plant_tensor_mul(n->out, n->out));
                PlantTensor* om = a2_k(plant_tensor_sub(one, sq));
                a2_acc(n->in0, plant_tensor_mul(d, om));
                a2_release();
                break;
            }
            case OP_SWISH: {
                PlantTensor* sg = a2_k(plant_tensor_sigmoid(n->in0));
                PlantTensor* one = a2_k(a2_const_like(sg, 1.0));
                PlantTensor* om = a2_k(plant_tensor_sub(one, sg));
                PlantTensor* inner = a2_k(plant_tensor_mul(sg, om));
                PlantTensor* term = a2_k(plant_tensor_mul(n->in0, inner));
                PlantTensor* sum = a2_k(plant_tensor_add(sg, term));
                a2_acc(n->in0, plant_tensor_mul(d, sum));
                a2_release();
                break;
            }
            case OP_GELU: {
                const double GK = 0.7978845608028654;
                const double GC = 0.044715;
                PlantTensor* sq = a2_k(plant_tensor_mul(n->in0, n->in0));
                PlantTensor* cu = a2_k(plant_tensor_mul(sq, n->in0));
                PlantTensor* sc = a2_k(plant_tensor_scale(cu, GC));
                PlantTensor* ad = a2_k(plant_tensor_add(n->in0, sc));
                PlantTensor* u = a2_k(plant_tensor_scale(ad, GK));
                PlantTensor* th = a2_k(plant_tensor_tanh(u));
                PlantTensor* one = a2_k(a2_const_like(th, 1.0));
                PlantTensor* s3 = a2_k(plant_tensor_scale(sq, 3.0 * GC));
                PlantTensor* a1 = a2_k(plant_tensor_add(one, s3));
                PlantTensor* du = a2_k(plant_tensor_scale(a1, GK));
                PlantTensor* t1 = a2_k(plant_tensor_add(one, th));
                PlantTensor* h1 = a2_k(plant_tensor_scale(t1, 0.5));
                PlantTensor* th2 = a2_k(plant_tensor_mul(th, th));
                PlantTensor* om = a2_k(plant_tensor_sub(one, th2));
                PlantTensor* od = a2_k(plant_tensor_mul(om, du));
                PlantTensor* ha = a2_k(plant_tensor_scale(n->in0, 0.5));
                PlantTensor* t2 = a2_k(plant_tensor_mul(ha, od));
                PlantTensor* g = a2_k(plant_tensor_add(h1, t2));
                a2_acc(n->in0, plant_tensor_mul(d, g));
                a2_release();
                break;
            }
            case OP_MISH: {
                /* t = tanh(softplus(x)) = out / x  (exact for x != 0) */
                PlantTensor* t = a2_k(plant_tensor_div(n->out, n->in0));
                PlantTensor* sg = a2_k(plant_tensor_sigmoid(n->in0));
                PlantTensor* t2 = a2_k(plant_tensor_mul(t, t));
                PlantTensor* one = a2_k(a2_const_like(t, 1.0));
                PlantTensor* om = a2_k(plant_tensor_sub(one, t2));
                PlantTensor* os = a2_k(plant_tensor_mul(om, sg));
                PlantTensor* term = a2_k(plant_tensor_mul(n->in0, os));
                PlantTensor* sum = a2_k(plant_tensor_add(t, term));
                a2_acc(n->in0, plant_tensor_mul(d, sum));
                a2_release();
                break;
            }
            case A2_BCAST:
                a2_acc(n->in0, a2_axis_sum(d, n->in0));
                break;
            case A2_AXIS_SUM:
                a2_acc(n->in0, a2_bcast(d, n->in0));
                break;
            default:
                return plant_tensor_create_error("ad2: unsupported op for higher-order AD");
        }
    }

    return 0;
}

void plant_tape_clear_both(void) {
    plant_tape_clear_public();
    plant_ad2_clear();
}

PlantTensor* plant_ad2_grad(double loss_d, PlantTensor* w) {
    A2Frame* f;
    PlantTensor* out;
    (void)loss_d;                       /* seed is ones (mirrors plant_grad) */

    if (!w) return plant_tensor_create_error("ad2: null w");
    if (plant_tensor_is_error(w)) return w;
    if (!saved_hook) return plant_tensor_create_error("ad2: not initialized");
    if (!w->requires_grad) return plant_tensor_create_error("ad2: w requires_grad=0");
    if (!frame_top) return plant_tensor_create_error("ad2: not initialized");

    /* v0.60.0b — gate level >= 3. The single-DAG refactor that makes the
       third derivative correct is deferred to 0c; failing loudly is much
       better than returning a wrong number (the 0a pass under-counted
       triple-product paths, e.g. x^3 -> 4 instead of 6). */
    if (frame_top->level >= 2) {
        return plant_tensor_create_error(
            "ad2: 3rd+ derivatives unsupported (single-DAG pending 0c)");
    }

    f = frame_top;
    if (!f->head) return plant_tensor_create_error("ad2: empty tape");
    if (!f->head->out) return plant_tensor_create_error("ad2: null root");

    /* The new frame receives every op emitted by this backward pass so the
       gradients are differentiable. All consumed frames stop recording. */
    a2_push_frame(f->level + 1, 0);
    f->frozen = 1;
    {
        PlantTensor* seed = plant_tensor_create(f->head->out->ndim,
                                                f->head->out->shape);
        int64_t i;
        if (!seed || plant_tensor_is_error(seed)) {
            gm_free_all();
            return seed ? seed : plant_tensor_create_error("ad2: seed oom");
        }
        for (i = 0; i < seed->size; i++) seed->data[i] = 1.0;
        gm_set(f->head->out, seed);     /* map owns seed */
    }

    /* IMPORTANT: traverse every older frame too. Forward tensors appear in
       frame 0 only; stopping at the top frame silently drops those paths
       (x^3 gave 4x instead of 6x for the second derivative). */
    {
        A2Frame* fr;
        for (fr = f; fr; fr = fr->prev) {
            PlantTensor* err = a2_backprop_nodes(fr);
            fr->frozen = 1;
            if (err && plant_tensor_is_error(err)) {
                gm_free_all();
                return err;
            }
        }
    }

    out = gm_get(w);
    if (!out) {
        PlantTensor* z = a2_zeros_like(w);
        gm_free_all();
        return z;
    }
    plant_tensor_retain(out);           /* caller's reference */
    gm_free_all();
    return out;
}
