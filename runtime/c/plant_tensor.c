/*
 * plant_tensor.c — v0.51.1: PlantTensor implementation.
 *
 * N-dimensional tensor type with contiguous row-major double* storage.
 * All allocations use plant_malloc() for test-injectable failure simulation.
 * C89-compatible.
 */

#include "plant_tensor.h"
#include "plant_malloc.h"
#include "plant_runtime.h"
#include "plant_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>   /* v0.55.2 — activations (tanh, exp) */
#include <stdarg.h> /* v0.61.2 — TD-067 variadic reshape */

/* ── Internal: free fields without freeing the struct ───────── */
static void _tensor_free_fields(PlantTensor* t) {
    if (!t) return;
    if (t->shape)    free(t->shape);
    if (t->strides)  free(t->strides);
    if (t->data)     free(t->data);
    if (t->error_msg) free(t->error_msg);
    t->shape = NULL;
    t->strides = NULL;
    t->data = NULL;
    t->error_msg = NULL;
}

/* ── Internal: compute product of shape[0..ndim-1] ──────────── */
static int64_t _tensor_compute_size(int64_t ndim, const int64_t* shape) {
    int64_t s = 1;
    int64_t i;
    for (i = 0; i < ndim; i++) {
        s *= shape[i];
    }
    return s;
}

/* ── Internal: compute row-major strides ────────────────────── */
static void _tensor_compute_strides(int64_t ndim, const int64_t* shape,
                                    int64_t* strides) {
    int64_t i;
    if (ndim <= 0) return;
    strides[ndim - 1] = 1;
    for (i = ndim - 2; i >= 0; i--) {
        strides[i] = strides[i + 1] * shape[i + 1];
    }
}

/* ── plant_tensor_create_error ──────────────────────────────── */
PlantTensor* plant_tensor_create_error(const char* msg) {
    PlantTensor* t = (PlantTensor*)plant_malloc(sizeof(PlantTensor));
    if (!t) return NULL;
    t->magic = PLANT_TENSOR_MAGIC;
    t->ndim = 0;
    t->shape = NULL;
    t->strides = NULL;
    t->data = NULL;
    t->size = 0;
    t->ref_count = 1;
    t->error_msg = NULL;
    t->requires_grad = 0;
    if (msg) {
        size_t len = strlen(msg);
        t->error_msg = (char*)plant_malloc(len + 1);
        if (t->error_msg) {
            memcpy(t->error_msg, msg, len + 1);
        }
    }
    return t;
}

/* ── plant_tensor_create ────────────────────────────────────── */
PlantTensor* plant_tensor_create(int64_t ndim, const int64_t* shape) {
    PlantTensor* t;
    int64_t i;
    int64_t sz;

    if (ndim < 1) {
        return plant_tensor_create_error("TENSOR requires ndim >= 1");
    }
    for (i = 0; i < ndim; i++) {
        if (shape[i] < 1) {
            return plant_tensor_create_error("TENSOR shape requires all dimensions > 0");
        }
    }

    sz = _tensor_compute_size(ndim, shape);

    t = (PlantTensor*)plant_malloc(sizeof(PlantTensor));
    if (!t) return plant_tensor_create_error("TENSOR allocation failed (struct)");

    t->shape = (int64_t*)plant_malloc((size_t)ndim * sizeof(int64_t));
    if (!t->shape) {
        _tensor_free_fields(t);
        return plant_tensor_create_error("TENSOR allocation failed (shape)");
    }

    t->strides = (int64_t*)plant_malloc((size_t)ndim * sizeof(int64_t));
    if (!t->strides) {
        _tensor_free_fields(t);
        return plant_tensor_create_error("TENSOR allocation failed (strides)");
    }

    t->data = (double*)plant_malloc((size_t)sz * sizeof(double));
    if (!t->data) {
        _tensor_free_fields(t);
        return plant_tensor_create_error("TENSOR allocation failed (data)");
    }

    t->ndim = ndim;
    t->size = sz;
    t->ref_count = 1;
    t->magic = PLANT_TENSOR_MAGIC;
    t->error_msg = NULL;
    t->requires_grad = 0;

    for (i = 0; i < ndim; i++) t->shape[i] = shape[i];
    _tensor_compute_strides(ndim, shape, t->strides);
    for (i = 0; i < sz; i++) t->data[i] = 0.0;

    return t;
}

/* ── plant_tensor_free ──────────────────────────────────────── */
void plant_tensor_free(PlantTensor* t) {
    if (!t) return;
    t->ref_count--;
    if (t->ref_count > 0) return;
    _tensor_free_fields(t);
    free(t);
}

/* v0.56.0a — Autograd primitives. */
void plant_tensor_retain(PlantTensor* t) {
    if (t) t->ref_count++;
}
int plant_tensor_requires_grad(PlantTensor* t) {
    return t ? (int)t->requires_grad : 0;
}
void plant_tensor_set_requires_grad(PlantTensor* t, int v) {
    if (t) t->requires_grad = (v != 0);
}
PlantTensor* plant_tensor_track(PlantTensor* t) {
    if (t) t->requires_grad = 1;
    return t;
}

/* v0.56.0a — tape hook (assigned by plant_autograd.c). */
void (*plant_tape_record_hook)(int op, PlantTensor* a, PlantTensor* b,
                               double s, PlantTensor* out) = 0;

/* v0.61.2 — CONV2D tape hook (assigned by plant_autograd.c / plant_ad2.c). */
void (*plant_tape_record_conv_hook)(PlantTensor* x, PlantTensor* k,
                                    PlantTensor* b, int64_t stride,
                                    int64_t pad, PlantTensor* out) = 0;

/* v0.61.3 — norm tape hooks. */
void (*plant_tape_record_batchnorm_hook)(PlantTensor* x, PlantTensor* g,
                                         PlantTensor* b, double eps,
                                         PlantTensor* out) = 0;
void (*plant_tape_record_layernorm_hook)(PlantTensor* x, PlantTensor* g,
                                         PlantTensor* b, double eps,
                                         PlantTensor* out) = 0;
void (*plant_tape_record_dropout_hook)(PlantTensor* x, double p, int64_t seed,
                                       PlantTensor* out) = 0;

/* ── plant_tensor_deep_copy ─────────────────────────────────── */
PlantTensor* plant_tensor_deep_copy(const PlantTensor* src) {
    PlantTensor* t;
    int64_t i;
    if (!src) return NULL;
    if (src->error_msg) return plant_tensor_create_error(src->error_msg);

    t = (PlantTensor*)plant_malloc(sizeof(PlantTensor));
    if (!t) return plant_tensor_create_error("TENSOR deep_copy allocation failed (struct)");

    t->ndim = src->ndim;
    t->size = src->size;
    t->ref_count = 1;
    t->magic = PLANT_TENSOR_MAGIC;
    t->error_msg = NULL;
    t->requires_grad = src->requires_grad;

    t->shape = (int64_t*)plant_malloc((size_t)src->ndim * sizeof(int64_t));
    if (!t->shape) {
        _tensor_free_fields(t);
        return plant_tensor_create_error("TENSOR deep_copy allocation failed (shape)");
    }

    t->strides = (int64_t*)plant_malloc((size_t)src->ndim * sizeof(int64_t));
    if (!t->strides) {
        _tensor_free_fields(t);
        return plant_tensor_create_error("TENSOR deep_copy allocation failed (strides)");
    }

    t->data = (double*)plant_malloc((size_t)src->size * sizeof(double));
    if (!t->data) {
        _tensor_free_fields(t);
        return plant_tensor_create_error("TENSOR deep_copy allocation failed (data)");
    }

    for (i = 0; i < src->ndim; i++) {
        t->shape[i] = src->shape[i];
        t->strides[i] = src->strides[i];
    }
    for (i = 0; i < src->size; i++) {
        t->data[i] = src->data[i];
    }

    return t;
}

/* ── Error helpers ──────────────────────────────────────────── */
int plant_tensor_is_error(const PlantTensor* t) {
    if (!t) return 1;
    return t->error_msg != NULL;
}

const char* plant_tensor_error_msg(const PlantTensor* t) {
    if (!t) return "NULL tensor";
    return t->error_msg ? t->error_msg : "(no error)";
}

/* ── Type detection ─────────────────────────────────────────── */
int plant_tensor_is_tensor(const void* ptr) {
    const PlantTensor* t;
    if (!ptr) return 0;
    t = (const PlantTensor*)ptr;
    return t->magic == PLANT_TENSOR_MAGIC;
}

/* ── Internal: PlantArray magic check ───────────────────────── */
#define PLANT_ARRAY_MAGIC_ID 0x504C4152

static int _is_list(const void* ptr) {
    if (!ptr) return 0;
    return *((const uint32_t*)ptr) == PLANT_ARRAY_MAGIC_ID;
}

/* ── Internal: compute ndim of nested list ──────────────────── */
static int64_t _list_ndim(const void* list) {
    int64_t ndim = 1;
    const void* first;
    if (!_is_list(list)) return 0;
    first = ((PlantArray*)list)->items[0];
    if (_is_list(first)) {
        ndim = 1 + _list_ndim(first);
    }
    return ndim;
}

/* ── Internal: fill shape array from nested list ────────────── */
static void _list_shape(const void* list, int64_t depth, int64_t* shape) {
    PlantArray* a = (PlantArray*)list;
    int64_t i;
    if (!_is_list(list)) return;
    shape[depth] = a->count;
    if (a->count > 0 && _is_list(a->items[0])) {
        _list_shape(a->items[0], depth + 1, shape);
    }
}

/* ── Internal: flatten nested list into double array ─────────── */
static int64_t _list_flatten(const void* list, double* out) {
    PlantArray* a = (PlantArray*)list;
    int64_t i;
    int64_t idx = 0;
    if (!_is_list(list)) {
        const char* s = (const char*)list;
        out[0] = s ? strtod(s, NULL) : 0.0;
        return 1;
    }
    for (i = 0; i < a->count; i++) {
        if (_is_list(a->items[i])) {
            idx += _list_flatten(a->items[i], out + idx);
        } else {
            const char* s = (const char*)a->items[i];
            out[idx] = s ? strtod(s, NULL) : 0.0;
            idx++;
        }
    }
    return idx;
}

/* ── Internal: validate raggedness ──────────────────────────── */
static int _list_check_ragged(const void* list) {
    PlantArray* a = (PlantArray*)list;
    int64_t i;
    int64_t sub_len = -1;
    if (!_is_list(list)) return 0;
    for (i = 0; i < a->count; i++) {
        if (_is_list(a->items[i])) {
            PlantArray* sub = (PlantArray*)a->items[i];
            if (sub_len == -1) {
                sub_len = sub->count;
            } else if (sub->count != sub_len) {
                return 1;
            }
            if (_list_check_ragged(a->items[i])) return 1;
        }
    }
    return 0;
}

/* ── plant_tensor_from_list ─────────────────────────────────── */
void* plant_tensor_from_list(void* list) {
    PlantTensor* t;
    int64_t ndim;
    int64_t* shape;
    int64_t sz;
    int64_t i;

    if (!list) return plant_tensor_create_error("TENSOR received NULL list");
    if (!_is_list(list)) {
        return plant_tensor_create_error("TENSOR expects a list argument");
    }

    ndim = _list_ndim(list);
    if (ndim < 1) {
        return plant_tensor_create_error("TENSOR cannot determine dimensions from empty list");
    }

    shape = (int64_t*)plant_malloc((size_t)ndim * sizeof(int64_t));
    if (!shape) return plant_tensor_create_error("TENSOR allocation failed (shape from list)");
    for (i = 0; i < ndim; i++) shape[i] = 0;
    _list_shape(list, 0, shape);

    /* Validate all dimensions > 0 */
    for (i = 0; i < ndim; i++) {
        if (shape[i] < 1) {
            free(shape);
            return plant_tensor_create_error("TENSOR does not accept empty nested lists");
        }
    }

    /* Check raggedness at each level */
    {
        PlantArray* a = (PlantArray*)list;
        for (i = 0; i < a->count; i++) {
            if (_is_list(a->items[i])) {
                if (_list_check_ragged(a->items[i])) {
                    free(shape);
                    return plant_tensor_create_error("TENSOR nested list is ragged");
                }
            }
        }
    }

    t = plant_tensor_create(ndim, shape);
    free(shape);
    if (plant_tensor_is_error(t)) return t;

    /* Fill data from flattened list */
    _list_flatten(list, t->data);

    return t;
}

/* ── Internal: recursive display helper ─────────────────────── */
static char* _tensor_to_string_recursive(const PlantTensor* t, int64_t dim, int64_t* coords) {
    char* buf;
    int64_t i;
    int64_t pos = 0;
    int64_t buf_size = 1024;

    if (!t || t->error_msg) return strdup("(error)");

    buf = (char*)plant_malloc((size_t)buf_size);
    if (!buf) return strdup("(alloc error)");
    buf[0] = '\0';

    if (dim == t->ndim) {
        int64_t flat_idx = 0;
        for (i = 0; i < t->ndim; i++) {
            flat_idx += coords[i] * t->strides[i];
        }
        pos += snprintf(buf + pos, (size_t)(buf_size - pos), "%g", t->data[flat_idx]);
        return buf;
    }

    buf[pos++] = '[';
    for (i = 0; i < t->shape[dim]; i++) {
        char* sub;
        coords[dim] = i;
        sub = _tensor_to_string_recursive(t, dim + 1, coords);
        if (sub) {
            size_t slen = strlen(sub);
            if (pos + slen + 2 >= (size_t)buf_size) {
                char* new_buf = (char*)plant_malloc((size_t)(buf_size * 2));
                if (new_buf) {
                    memcpy(new_buf, buf, (size_t)pos);
                    free(buf);
                    buf = new_buf;
                    buf_size *= 2;
                }
            }
            memcpy(buf + pos, sub, slen);
            pos += slen;
            free(sub);
        }
        if (i < t->shape[dim] - 1) {
            buf[pos++] = ',';
            buf[pos++] = ' ';
        }
    }
    buf[pos++] = ']';
    buf[pos] = '\0';
    return buf;
}

/* ── plant_tensor_to_string ─────────────────────────────────── */
char* plant_tensor_to_string(const PlantTensor* t) {
    int64_t* coords;
    char* result;
    int64_t i;

    if (!t) return strdup("(null tensor)");
    if (t->error_msg) {
        size_t len = strlen(t->error_msg);
        char* buf = (char*)plant_malloc(len + 1);
        if (buf) memcpy(buf, t->error_msg, len + 1);
        return buf;
    }

    coords = (int64_t*)plant_malloc((size_t)t->ndim * sizeof(int64_t));
    if (!coords) return strdup("(alloc error)");
    for (i = 0; i < t->ndim; i++) coords[i] = 0;

    result = _tensor_to_string_recursive(t, 0, coords);
    free(coords);
    return result;
}

/* ── plant_tensor_to_string_static ──────────────────────────── */
/* Writes tensor representation into caller-provided buffer.
   Returns number of chars written (excluding NUL), or -1 on error.
   v0.51.2a — static variant to avoid heap allocation (TD-003). */
int64_t plant_tensor_to_string_static(const PlantTensor* t,
                                      char* buf, int64_t bufsize) {
    char* tmp;
    int64_t len;
    if (!buf || bufsize < 1) return -1;
    if (!t) { buf[0] = '\0'; return 0; }
    tmp = plant_tensor_to_string(t);
    if (!tmp) { buf[0] = '\0'; return 0; }
    len = (int64_t)strlen(tmp);
    if (len >= bufsize) len = bufsize - 1;
    memcpy(buf, tmp, (size_t)len);
    buf[len] = '\0';
    free(tmp);
    return len;
}

/* ── TENSOR_SHAPE — v0.51.2b introspection ─────────────────── */
/* Returns a 1-D PlantArray containing the shape as integer values
   (via _from_long, consistent with all PlantLang integers).
   Caller owns the returned list. Returns NULL if t is NULL. */
PlantArray* plant_tensor_shape(PlantTensor* t) {
    int64_t ndim, i;
    PlantArray* result;
    if (!t) return NULL;
    ndim = t->ndim;
    result = plant_list_create(ndim);
    if (!result) return NULL;
    for (i = 0; i < ndim; i++) {
        plant_list_push(result, _from_long((long)t->shape[i]));
    }
    return result;
}

/* ── TENSOR_NDIM — v0.51.2b introspection ──────────────────── */
/* Returns the number of dimensions. 0 if t is NULL. */
int64_t plant_tensor_ndim(PlantTensor* t) {
    if (!t) return 0;
    return t->ndim;
}

/* ── TENSOR_SIZE — v0.51.2b introspection ──────────────────── */
/* Returns total number of elements (product of shape). 0 if t is NULL. */
int64_t plant_tensor_size(PlantTensor* t) {
    if (!t) return 0;
    return t->size;
}

/* ══════════════════════════════════════════════════════════════
   v0.55.0 — Tensor Operations (core)
   Each op returns a NEW ref-counted PlantTensor (ref_count = 1).
   Errors return an error tensor (magic intact, error_msg set).
   ══════════════════════════════════════════════════════════════ */

/* Elementwise helper: op 0=add, 1=sub, 2=mul, 3=div. */
/* v0.55.1 — NumPy-like broadcast shape. Returns 1 on success. */
static int _pt_bcast_shape(PlantTensor* a, PlantTensor* b,
                           int64_t* out_nd, int64_t* out_s) {
    int64_t na = a->ndim, nb = b->ndim;
    int64_t n = na > nb ? na : nb;
    int64_t i;
    for (i = 0; i < n; i++) {
        int64_t da = (i < n - na) ? 1 : a->shape[i - (n - na)];
        int64_t db = (i < n - nb) ? 1 : b->shape[i - (n - nb)];
        if (da != db && da != 1 && db != 1) return 0;
        out_s[i] = da > db ? da : db;
    }
    *out_nd = n;
    return 1;
}

/* v0.55.1 — row-major index of t at output coordinate oidx, with
   broadcasting (a size-1 dim contributes index 0).
   v0.56.0b-2 — exported for autograd broadcast reduction. */
int64_t plant_tensor_bc_index(PlantTensor* t, const int64_t* oidx, int64_t out_nd) {
    int64_t off = 0;
    int64_t shift = out_nd - t->ndim;
    int64_t i;
    for (i = 0; i < t->ndim; i++) {
        int64_t idx = oidx[i + shift];
        if (t->shape[i] == 1) idx = 0;
        off += idx * t->strides[i];
    }
    return off;
}

/* Elementwise helper (op: 0=add, 1=sub, 2=mul, 3=div) with broadcasting. */
static PlantTensor* _pt_ew(PlantTensor* a, PlantTensor* b, int op) {
    int64_t out_nd = 0;
    int64_t out_s[16];
    int64_t oidx[16];
    int64_t i, d;
    PlantTensor* o;
    if (!a || !b) return plant_tensor_create_error("tensor op: null operand");
    if (plant_tensor_is_error(a)) return a;
    if (plant_tensor_is_error(b)) return b;
    if (!_pt_bcast_shape(a, b, &out_nd, out_s))
        return plant_tensor_create_error("tensor op: shape mismatch");
    o = plant_tensor_create(out_nd, out_s);
    if (plant_tensor_is_error(o)) return o;
    for (d = 0; d < out_nd; d++) oidx[d] = 0;
    for (i = 0; i < o->size; i++) {
        int64_t ia = plant_tensor_bc_index(a, oidx, out_nd);
        int64_t ib = plant_tensor_bc_index(b, oidx, out_nd);
        double x = a->data[ia], y = b->data[ib];
        switch (op) {
            case 0: o->data[i] = x + y; break;
            case 1: o->data[i] = x - y; break;
            case 2: o->data[i] = x * y; break;
            case 3: o->data[i] = (y != 0.0) ? (x / y) : 0.0; break;
            default: o->data[i] = x; break;
        }
        for (d = out_nd - 1; d >= 0; d--) {
            oidx[d]++;
            if (oidx[d] < out_s[d]) break;
            oidx[d] = 0;
        }
    }
    /* v0.56.0a — autograd hook.
       v0.56.0b-2 — record broadcast ops too (backward reduces the grad). */
    if (plant_tape_record_hook &&
        (plant_tensor_requires_grad(a) || plant_tensor_requires_grad(b))) {
        /* Internal _pt_ew codes are 0..3; tape codes OP_ADD..OP_DIV are
           1..4, so shift by one (v0.56.0a). */
        plant_tape_record_hook(op + 1, a, b, 0.0, o);
        o->requires_grad = 1;
    }
    return o;
}

PlantTensor* plant_tensor_add(PlantTensor* a, PlantTensor* b) { return _pt_ew(a, b, 0); }
PlantTensor* plant_tensor_sub(PlantTensor* a, PlantTensor* b) { return _pt_ew(a, b, 1); }
PlantTensor* plant_tensor_mul(PlantTensor* a, PlantTensor* b) { return _pt_ew(a, b, 2); }
PlantTensor* plant_tensor_div(PlantTensor* a, PlantTensor* b) { return _pt_ew(a, b, 3); }

PlantTensor* plant_tensor_scale(PlantTensor* a, double s) {
    int64_t i;
    PlantTensor* o;
    if (!a) return plant_tensor_create_error("scale: null tensor");
    if (plant_tensor_is_error(a)) return a;
    o = plant_tensor_create(a->ndim, a->shape);
    if (plant_tensor_is_error(o)) return o;
    for (i = 0; i < a->size; i++) o->data[i] = a->data[i] * s;
    if (plant_tape_record_hook && plant_tensor_requires_grad(a)) {
        plant_tape_record_hook(5 /* OP_SCALE */, a, 0, s, o);
        o->requires_grad = 1;
    }
    return o;
}

PlantTensor* plant_tensor_matmul(PlantTensor* a, PlantTensor* b) {
    int64_t i, j, k, m, n, p;
    int64_t oshape[2];
    PlantTensor* o;
    if (!a || !b) return plant_tensor_create_error("matmul: null operand");
    if (plant_tensor_is_error(a)) return a;
    if (plant_tensor_is_error(b)) return b;
    if (a->ndim != 2 || b->ndim != 2)
        return plant_tensor_create_error("matmul: 2d tensors required");
    if (a->shape[1] != b->shape[0])
        return plant_tensor_create_error("matmul: inner dimension mismatch");
    m = a->shape[0];
    n = a->shape[1];
    p = b->shape[1];
    oshape[0] = m;
    oshape[1] = p;
    o = plant_tensor_create(2, oshape);
    if (plant_tensor_is_error(o)) return o;
    for (i = 0; i < m; i++) {
        for (j = 0; j < p; j++) {
            double sum = 0.0;
            for (k = 0; k < n; k++) {
                sum += a->data[i * n + k] * b->data[k * p + j];
            }
            o->data[i * p + j] = sum;
        }
    }
    /* v0.56.0b-1 — autograd hook (matmul). */
    if (plant_tape_record_hook &&
        (plant_tensor_requires_grad(a) || plant_tensor_requires_grad(b))) {
        plant_tape_record_hook(8 /* OP_MATMUL */, a, b, 0.0, o);
        o->requires_grad = 1;
    }
    return o;
}

PlantTensor* plant_tensor_transpose(PlantTensor* a) {
    int64_t i, j, m, n;
    int64_t oshape[2];
    PlantTensor* o;
    if (!a) return plant_tensor_create_error("transpose: null tensor");
    if (plant_tensor_is_error(a)) return a;
    if (a->ndim != 2)
        return plant_tensor_create_error("transpose: 2d tensor required");
    m = a->shape[0];
    n = a->shape[1];
    oshape[0] = n;
    oshape[1] = m;
    o = plant_tensor_create(2, oshape);
    if (plant_tensor_is_error(o)) return o;
    for (i = 0; i < m; i++) {
        for (j = 0; j < n; j++) {
            o->data[j * m + i] = a->data[i * n + j];
        }
    }
    if (plant_tape_record_hook && plant_tensor_requires_grad(a)) {
        plant_tape_record_hook(12 /* OP_TRANSPOSE */, a, 0, 0.0, o);
        o->requires_grad = 1;
    }
    return o;
}

PlantTensor* plant_tensor_reshape(PlantTensor* a, PlantArray* shape) {
    int64_t nd, i, tot;
    int64_t* ns;
    PlantTensor* o;
    if (!a || !shape) return plant_tensor_create_error("reshape: null argument");
    if (plant_tensor_is_error(a)) return a;
    nd = shape->count;
    if (nd < 1) return plant_tensor_create_error("reshape: empty shape");
    ns = (int64_t*)plant_malloc(sizeof(int64_t) * (size_t)nd);
    if (!ns) return plant_tensor_create_error("reshape: allocation failed");
    tot = 1;
    for (i = 0; i < nd; i++) {
        const char* sv = _S(plant_list_get(shape, i));
        ns[i] = (int64_t)strtoll(sv ? sv : "0", NULL, 10);
        tot *= ns[i];
    }
    if (tot != a->size) {
        free(ns);
        return plant_tensor_create_error("reshape: size mismatch");
    }
    o = plant_tensor_create(nd, ns);
    free(ns);
    if (plant_tensor_is_error(o)) return o;
    memcpy(o->data, a->data, sizeof(double) * (size_t)a->size);
    if (plant_tape_record_hook && plant_tensor_requires_grad(a)) {
        plant_tape_record_hook(13 /* OP_RESHAPE */, a, 0, 0.0, o);
        o->requires_grad = 1;
    }
    return o;
}

/* v0.61.2 — TD-067: variadic reshape (no list literal → no leak). */
PlantTensor* plant_tensor_reshape_n(PlantTensor* a, int nd, ...) {
    int64_t shape[16];
    int64_t i, tot;
    va_list ap;
    PlantTensor* o;
    if (!a) return plant_tensor_create_error("reshape: null tensor");
    if (plant_tensor_is_error(a)) return a;
    if (nd < 1 || nd > 16)
        return plant_tensor_create_error("reshape: nd out of range");
    va_start(ap, nd);
    tot = 1;
    for (i = 0; i < nd; i++) {
        int d = va_arg(ap, int);
        if (d < 1) {
            va_end(ap);
            return plant_tensor_create_error(
                "reshape: shape requires all dimensions > 0");
        }
        shape[i] = (int64_t)d;
        tot *= shape[i];
    }
    va_end(ap);
    if (tot != a->size) return plant_tensor_create_error("reshape: size mismatch");
    o = plant_tensor_create(nd, shape);
    if (plant_tensor_is_error(o)) return o;
    memcpy(o->data, a->data, sizeof(double) * (size_t)a->size);
    if (plant_tape_record_hook && plant_tensor_requires_grad(a)) {
        plant_tape_record_hook(13 /* OP_RESHAPE */, a, 0, 0.0, o);
        o->requires_grad = 1;
    }
    return o;
}

/* v0.61.3 — BatchNorm forward (direct loops, biased variance).
   4-D [N,C,H,W] → per-channel over (N,H,W); 2-D [N,F] → per-feature.
   Statistics are recomputed identically by the backward rule. */
PlantTensor* plant_batchnorm_forward(PlantTensor* x, PlantTensor* gamma,
                                     PlantTensor* beta, double eps) {
    int64_t n, c, h, w, i;
    int64_t N, C, M, H, W;
    double sum, sq, mean, var, inv, g, b;
    PlantTensor* y;
    if (!x || !gamma || !beta)
        return plant_tensor_create_error("batchnorm: null argument");
    if (plant_tensor_is_error(x)) return x;
    if (plant_tensor_is_error(gamma)) return gamma;
    if (plant_tensor_is_error(beta)) return beta;
    if (x->ndim != 2 && x->ndim != 4)
        return plant_tensor_create_error("batchnorm: ndim must be 2 or 4");
    N = x->shape[0];
    C = x->shape[1];
    H = (x->ndim == 4) ? x->shape[2] : 1;
    W = (x->ndim == 4) ? x->shape[3] : 1;
    M = N * H * W;
    if (gamma->size != C || beta->size != C)
        return plant_tensor_create_error("batchnorm: gamma/beta must be [C]");
    y = plant_tensor_create(x->ndim, x->shape);
    if (plant_tensor_is_error(y)) return y;
    for (c = 0; c < C; c++) {
        sum = 0.0; sq = 0.0;
        for (n = 0; n < N; n++)
          for (h = 0; h < H; h++)
            for (w = 0; w < W; w++) {
                double v = x->data[((n * C + c) * H + h) * W + w];
                sum += v; sq += v * v;
            }
        mean = sum / (double)M;
        var = sq / (double)M - mean * mean;
        inv = 1.0 / sqrt(var + eps);
        g = gamma->data[c];
        b = beta->data[c];
        for (n = 0; n < N; n++)
          for (h = 0; h < H; h++)
            for (w = 0; w < W; w++) {
                i = ((n * C + c) * H + h) * W + w;
                y->data[i] = g * ((x->data[i] - mean) * inv) + b;
            }
    }
    if (plant_tape_record_batchnorm_hook &&
        (plant_tensor_requires_grad(x) || plant_tensor_requires_grad(gamma) ||
         plant_tensor_requires_grad(beta))) {
        plant_tape_record_batchnorm_hook(x, gamma, beta, eps, y);
        y->requires_grad = 1;
    }
    return y;
}

/* v0.61.3 — LayerNorm forward (last axis, any ndim). */
PlantTensor* plant_layernorm_forward(PlantTensor* x, PlantTensor* gamma,
                                     PlantTensor* beta, double eps) {
    int64_t F, f, r, rows;
    double sum, sq, mean, var, inv;
    PlantTensor* y;
    if (!x || !gamma || !beta)
        return plant_tensor_create_error("layernorm: null argument");
    if (plant_tensor_is_error(x)) return x;
    if (plant_tensor_is_error(gamma)) return gamma;
    if (plant_tensor_is_error(beta)) return beta;
    if (x->ndim < 1)
        return plant_tensor_create_error("layernorm: ndim < 1");
    F = x->shape[x->ndim - 1];
    if (gamma->size != F || beta->size != F)
        return plant_tensor_create_error("layernorm: gamma/beta must be [F]");
    rows = x->size / F;
    y = plant_tensor_create(x->ndim, x->shape);
    if (plant_tensor_is_error(y)) return y;
    for (r = 0; r < rows; r++) {
        sum = 0.0; sq = 0.0;
        for (f = 0; f < F; f++) {
            double v = x->data[r * F + f];
            sum += v; sq += v * v;
        }
        mean = sum / (double)F;
        var = sq / (double)F - mean * mean;
        inv = 1.0 / sqrt(var + eps);
        for (f = 0; f < F; f++)
            y->data[r * F + f] = gamma->data[f] *
                                 ((x->data[r * F + f] - mean) * inv) + beta->data[f];
    }
    if (plant_tape_record_layernorm_hook &&
        (plant_tensor_requires_grad(x) || plant_tensor_requires_grad(gamma) ||
         plant_tensor_requires_grad(beta))) {
        plant_tape_record_layernorm_hook(x, gamma, beta, eps, y);
        y->requires_grad = 1;
    }
    return y;
}

/* v0.61.4 — Dropout PRNG: local xorshift64* (no global state, fully
   reproducible from the seed). The backward rule replays the same stream,
   so the mask never has to be stored. */
uint64_t plant_dropout_state(int64_t seed) {
    uint64_t s = (uint64_t)seed * 0x9E3779B97F4A7C15ULL;
    return s ? s : 0x9E3779B97F4A7C15ULL;
}

double plant_dropout_uniform(uint64_t* st) {
    uint64_t x = *st;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *st = x;
    return (double)((x * 0x2545F4914F6CDD1DULL) >> 11) * (1.0 / 9007199254740992.0);
}

/* v0.61.4 — Dropout forward. p in [0,1); p == 0 is eval (identity copy). */
PlantTensor* plant_dropout_forward(PlantTensor* x, double p, int64_t seed) {
    PlantTensor* y;
    int64_t i;
    uint64_t st;
    double scale;
    if (!x) return plant_tensor_create_error("dropout: null tensor");
    if (plant_tensor_is_error(x)) return x;
    if (p < 0.0 || p >= 1.0)
        return plant_tensor_create_error("dropout: p must be in [0,1)");
    y = plant_tensor_create(x->ndim, x->shape);
    if (plant_tensor_is_error(y)) return y;
    if (p <= 0.0) {
        memcpy(y->data, x->data, sizeof(double) * (size_t)x->size);
    } else {
        st = plant_dropout_state(seed);
        scale = 1.0 / (1.0 - p);
        for (i = 0; i < x->size; i++) {
            double u = plant_dropout_uniform(&st);
            y->data[i] = (u >= p) ? x->data[i] * scale : 0.0;
        }
    }
    if (plant_tape_record_dropout_hook && plant_tensor_requires_grad(x)) {
        plant_tape_record_dropout_hook(x, p, seed, y);
        y->requires_grad = 1;
    }
    return y;
}

/* v0.61.2 — TD-067: legacy list-form reshape; frees the literal list. */
PlantTensor* plant_tensor_reshape_and_free(PlantTensor* a, PlantArray* shape) {
    PlantTensor* o = plant_tensor_reshape(a, shape);
    if (shape) plant_list_free(shape);
    return o;
}

/* v0.61.2 — CONV2D forward (NCHW, single stride/pad, optional bias).
   Direct loops (no im2col temp); recorded as one tape node for the
   dX/dK/db backward. */
PlantTensor* plant_conv2d_forward(PlantTensor* x, PlantTensor* k,
                                  PlantTensor* b, int64_t stride, int64_t pad) {
    int64_t n, c, h, w, co, kh, kw, hout, wout;
    int64_t i, j, oh, ow, ci, ki, kj;
    int64_t oshape[4];
    PlantTensor* o;
    if (!x || !k) return plant_tensor_create_error("conv2d: null operand");
    if (plant_tensor_is_error(x)) return x;
    if (plant_tensor_is_error(k)) return k;
    if (b && plant_tensor_is_error(b)) return b;
    if (x->ndim != 4 || k->ndim != 4)
        return plant_tensor_create_error("conv2d: 4d tensors required");
    if (stride < 1 || pad < 0)
        return plant_tensor_create_error("conv2d: invalid stride/pad");
    n = x->shape[0]; c = x->shape[1]; h = x->shape[2]; w = x->shape[3];
    co = k->shape[0]; kh = k->shape[2]; kw = k->shape[3];
    if (k->shape[1] != c)
        return plant_tensor_create_error("conv2d: channel mismatch");
    if (b && (b->ndim != 1 || b->shape[0] != co))
        return plant_tensor_create_error("conv2d: bias must be [c_out]");
    hout = (h + 2 * pad - kh) / stride + 1;
    wout = (w + 2 * pad - kw) / stride + 1;
    if (hout < 1 || wout < 1)
        return plant_tensor_create_error("conv2d: kernel larger than input");
    oshape[0] = n; oshape[1] = co; oshape[2] = hout; oshape[3] = wout;
    o = plant_tensor_create(4, oshape);
    if (plant_tensor_is_error(o)) return o;
    for (i = 0; i < n; i++) {
        for (j = 0; j < co; j++) {
            for (oh = 0; oh < hout; oh++) {
                for (ow = 0; ow < wout; ow++) {
                    double acc = b ? b->data[j] : 0.0;
                    for (ci = 0; ci < c; ci++) {
                        for (ki = 0; ki < kh; ki++) {
                            int64_t ih = oh * stride - pad + ki;
                            if (ih < 0 || ih >= h) continue;
                            for (kj = 0; kj < kw; kj++) {
                                int64_t iw = ow * stride - pad + kj;
                                if (iw < 0 || iw >= w) continue;
                                acc += x->data[((i * c + ci) * h + ih) * w + iw]
                                     * k->data[((j * c + ci) * kh + ki) * kw + kj];
                            }
                        }
                    }
                    o->data[((i * co + j) * hout + oh) * wout + ow] = acc;
                }
            }
        }
    }
    if (plant_tape_record_conv_hook &&
        (plant_tensor_requires_grad(x) || plant_tensor_requires_grad(k) ||
         (b && plant_tensor_requires_grad(b)))) {
        plant_tape_record_conv_hook(x, k, b, stride, pad, o);
        o->requires_grad = 1;
    }
    return o;
}

double plant_tensor_sum(PlantTensor* a) {
    int64_t i;
    double s = 0.0;
    if (!a || plant_tensor_is_error(a)) return 0.0;
    for (i = 0; i < a->size; i++) s += a->data[i];
    if (plant_tape_record_hook && plant_tensor_requires_grad(a))
        plant_tape_record_hook(6 /* OP_SUM */, a, 0, s, 0);
    return s;
}

double plant_tensor_mean(PlantTensor* a) {
    int64_t i;
    double s = 0.0;
    if (!a || plant_tensor_is_error(a) || a->size == 0) return 0.0;
    for (i = 0; i < a->size; i++) s += a->data[i];
    if (plant_tape_record_hook && plant_tensor_requires_grad(a))
        plant_tape_record_hook(7 /* OP_MEAN */, a, 0, s / (double)a->size, 0);
    return s / (double)a->size;
}

double plant_tensor_max(PlantTensor* a) {
    int64_t i;
    double m;
    if (!a || plant_tensor_is_error(a) || a->size == 0) return 0.0;
    m = a->data[0];
    for (i = 1; i < a->size; i++) if (a->data[i] > m) m = a->data[i];
    return m;
}

double plant_tensor_min(PlantTensor* a) {
    int64_t i;
    double m;
    if (!a || plant_tensor_is_error(a) || a->size == 0) return 0.0;
    m = a->data[0];
    for (i = 1; i < a->size; i++) if (a->data[i] < m) m = a->data[i];
    return m;
}

/* v0.55.1 — SLICE: ranges = [[s0,e0],[s1,e1],...] (half-open per dim). */
PlantTensor* plant_tensor_slice(PlantTensor* a, PlantArray* ranges) {
    int64_t nd, i, d;
    int64_t* s0;
    int64_t* e0;
    int64_t* oshape;
    int64_t idx[16];
    PlantTensor* o;
    if (!a || !ranges) return plant_tensor_create_error("slice: null argument");
    if (plant_tensor_is_error(a)) return a;
    nd = a->ndim;
    if (ranges->count != nd)
        return plant_tensor_create_error("slice: ndim mismatch");
    s0 = (int64_t*)plant_malloc(sizeof(int64_t) * (size_t)nd);
    e0 = (int64_t*)plant_malloc(sizeof(int64_t) * (size_t)nd);
    oshape = (int64_t*)plant_malloc(sizeof(int64_t) * (size_t)nd);
    if (!s0 || !e0 || !oshape) {
        if (s0) free(s0);
        if (e0) free(e0);
        if (oshape) free(oshape);
        return plant_tensor_create_error("slice: allocation failed");
    }
    for (i = 0; i < nd; i++) {
        PlantArray* r = (PlantArray*)plant_list_get(ranges, i);
        const char* sv;
        const char* ev;
        if (!r || r->magic != PLANT_ARRAY_MAGIC || r->count < 2) {
            free(s0); free(e0); free(oshape);
            return plant_tensor_create_error("slice: bad range entry");
        }
        sv = _S(plant_list_get(r, 0));
        ev = _S(plant_list_get(r, 1));
        s0[i] = (int64_t)strtoll(sv ? sv : "0", NULL, 10);
        e0[i] = (int64_t)strtoll(ev ? ev : "0", NULL, 10);
        if (s0[i] < 0 || e0[i] > a->shape[i] || s0[i] >= e0[i]) {
            free(s0); free(e0); free(oshape);
            return plant_tensor_create_error("slice: range invalid");
        }
        oshape[i] = e0[i] - s0[i];
    }
    o = plant_tensor_create(nd, oshape);
    free(oshape);
    if (plant_tensor_is_error(o)) { free(s0); free(e0); return o; }
    for (d = 0; d < nd; d++) idx[d] = 0;
    for (i = 0; i < o->size; i++) {
        int64_t src = 0, dst = 0;
        for (d = 0; d < nd; d++) src += (s0[d] + idx[d]) * a->strides[d];
        for (d = 0; d < nd; d++) dst += idx[d] * o->strides[d];
        o->data[dst] = a->data[src];
        for (d = nd - 1; d >= 0; d--) {
            idx[d]++;
            if (idx[d] < o->shape[d]) break;
            idx[d] = 0;
        }
    }
    free(s0); free(e0);
    return o;
}

/* v0.55.1 — TENSOR_AT(a, [i0, i1, ...]) → double (element access). */
double plant_tensor_at(PlantTensor* a, PlantArray* idx) {
    int64_t off = 0;
    int64_t i;
    if (!a || plant_tensor_is_error(a)) return 0.0;
    if (!idx || idx->count != a->ndim) return 0.0;
    for (i = 0; i < a->ndim; i++) {
        const char* sv = _S(plant_list_get(idx, i));
        int64_t k = (int64_t)strtoll(sv ? sv : "0", NULL, 10);
        if (k < 0 || k >= a->shape[i]) return 0.0;
        off += k * a->strides[i];
    }
    return a->data[off];
}

/* v0.56.0b-2 — Sum d (output shape) down to target's broadcast shape. */
PlantTensor* plant_tensor_bcast_reduce(const PlantTensor* d,
                                       const PlantTensor* t) {
    int64_t oidx[16], i, dd;
    PlantTensor* r;
    if (!d || !t) return plant_tensor_create_error("bcast_reduce: null");
    r = plant_tensor_create(t->ndim, t->shape);
    if (plant_tensor_is_error(r)) return r;
    for (dd = 0; dd < d->ndim; dd++) oidx[dd] = 0;
    for (i = 0; i < d->size; i++) {
        int64_t ti = plant_tensor_bc_index((PlantTensor*)t, oidx, d->ndim);
        r->data[ti] += d->data[i];
        for (dd = d->ndim - 1; dd >= 0; dd--) {
            oidx[dd]++;
            if (oidx[dd] < d->shape[dd]) break;
            oidx[dd] = 0;
        }
    }
    return r;
}

/* v0.58.0 — Magic-checked helpers (no type inference; safe on
   non-tensor pointers and small-int-as-pointer values). */
void plant_tensor_retain_if_tensor(void* p) {
    uintptr_t u = (uintptr_t)p;
    if (u < 4096) return;
    if (u > (uintptr_t)0x00007FFFFFFFFFFFULL) return;  /* -1 / non-canonical */
    if (*(uint64_t*)p != PLANT_TENSOR_MAGIC) return;
    plant_tensor_retain((PlantTensor*)p);
}

void plant_tensor_free_if_tensor(void* p) {
    uintptr_t u = (uintptr_t)p;
    if (u < 4096) return;
    if (u > (uintptr_t)0x00007FFFFFFFFFFFULL) return;  /* -1 / non-canonical */
    if (*(uint64_t*)p != PLANT_TENSOR_MAGIC) return;
    plant_tensor_free((PlantTensor*)p);
}

/* v0.55.2 — Activations. */

PlantTensor* plant_tensor_relu(PlantTensor* a) {
    if (!a || plant_tensor_is_error(a)) return plant_tensor_create_error("relu: invalid");
    PlantTensor* o = plant_tensor_create(a->ndim, a->shape);
    if (plant_tensor_is_error(o)) return o;
    for (int64_t i = 0; i < a->size; i++) {
        double x = a->data[i];
        o->data[i] = (x > 0.0) ? x : 0.0;
    }
    if (plant_tape_record_hook && plant_tensor_requires_grad(a)) {
        plant_tape_record_hook(9 /* OP_RELU */, a, 0, 0.0, o);
        o->requires_grad = 1;
    }
    return o;
}

PlantTensor* plant_tensor_sigmoid(PlantTensor* a) {
    if (!a || plant_tensor_is_error(a)) return plant_tensor_create_error("sigmoid: invalid");
    PlantTensor* o = plant_tensor_create(a->ndim, a->shape);
    if (plant_tensor_is_error(o)) return o;
    for (int64_t i = 0; i < a->size; i++) {
        o->data[i] = 1.0 / (1.0 + exp(-a->data[i]));
    }
    if (plant_tape_record_hook && plant_tensor_requires_grad(a)) {
        plant_tape_record_hook(10 /* OP_SIGMOID */, a, 0, 0.0, o);
        o->requires_grad = 1;
    }
    return o;
}

PlantTensor* plant_tensor_tanh(PlantTensor* a) {
    if (!a || plant_tensor_is_error(a)) return plant_tensor_create_error("tanh: invalid");
    PlantTensor* o = plant_tensor_create(a->ndim, a->shape);
    if (plant_tensor_is_error(o)) return o;
    for (int64_t i = 0; i < a->size; i++) {
        o->data[i] = tanh(a->data[i]);
    }
    if (plant_tape_record_hook && plant_tensor_requires_grad(a)) {
        plant_tape_record_hook(11 /* OP_TANH */, a, 0, 0.0, o);
        o->requires_grad = 1;
    }
    return o;
}

/* v0.55.2 — SOFTMAX along axis (0=cols, 1=rows). 1D → axis 0. */
PlantTensor* plant_tensor_softmax(PlantTensor* a, int axis) {
    if (!a || plant_tensor_is_error(a)) return plant_tensor_create_error("softmax: invalid");
    if (a->ndim < 1) return plant_tensor_create_error("softmax: ndim<1");
    PlantTensor* o = plant_tensor_create(a->ndim, a->shape);
    if (plant_tensor_is_error(o)) return o;
    if (a->ndim == 1) {
        double mx = a->data[0];
        for (int64_t i = 1; i < a->size; i++) if (a->data[i] > mx) mx = a->data[i];
        double s = 0.0;
        for (int64_t i = 0; i < a->size; i++) { o->data[i] = exp(a->data[i] - mx); s += o->data[i]; }
        for (int64_t i = 0; i < a->size; i++) o->data[i] /= s;
        /* v0.56.0c — autograd hook (softmax 1D; axis in scalar). */
        if (plant_tape_record_hook && plant_tensor_requires_grad(a)) {
            plant_tape_record_hook(14 /* OP_SOFTMAX */, a, 0, (double)axis, o);
            o->requires_grad = 1;
        }
        return o;
    }
    if (a->ndim != 2) return plant_tensor_create_error("softmax: only 1d/2d");
    {
        int64_t m = a->shape[0], n = a->shape[1];
        if (axis == 1) {
            /* rows: each row sums to 1 */
            for (int64_t i = 0; i < m; i++) {
                double mx = a->data[i*n];
                for (int64_t j = 1; j < n; j++) if (a->data[i*n+j] > mx) mx = a->data[i*n+j];
                double s = 0.0;
                for (int64_t j = 0; j < n; j++) { double e = exp(a->data[i*n+j] - mx); o->data[i*n+j] = e; s += e; }
                for (int64_t j = 0; j < n; j++) o->data[i*n+j] /= s;
            }
        } else {
            /* columns: each col sums to 1 */
            for (int64_t j = 0; j < n; j++) {
                double mx = a->data[j];
                for (int64_t i = 1; i < m; i++) if (a->data[i*n+j] > mx) mx = a->data[i*n+j];
                double s = 0.0;
                for (int64_t i = 0; i < m; i++) { double e = exp(a->data[i*n+j] - mx); o->data[i*n+j] = e; s += e; }
                for (int64_t i = 0; i < m; i++) o->data[i*n+j] /= s;
            }
        }
    }
    /* v0.56.0c — autograd hook (softmax 2D; axis in scalar). */
    if (plant_tape_record_hook && plant_tensor_requires_grad(a)) {
        plant_tape_record_hook(14 /* OP_SOFTMAX */, a, 0, (double)axis, o);
        o->requires_grad = 1;
    }
    return o;
}

/* v0.55.3 — Modern activations. */

PlantTensor* plant_tensor_gelu(PlantTensor* a) {
    if (!a || plant_tensor_is_error(a)) return plant_tensor_create_error("gelu: invalid");
    PlantTensor* o = plant_tensor_create(a->ndim, a->shape);
    if (plant_tensor_is_error(o)) return o;
    const double SQRT_2_OVER_PI = 0.7978845608028654;
    const double COEF = 0.044715;
    for (int64_t i = 0; i < a->size; i++) {
        double x = a->data[i];
        double t = tanh(SQRT_2_OVER_PI * (x + COEF * x * x * x));
        o->data[i] = 0.5 * x * (1.0 + t);
    }
    if (plant_tape_record_hook && plant_tensor_requires_grad(a)) {
        plant_tape_record_hook(15 /* OP_GELU */, a, 0, 0.0, o);
        o->requires_grad = 1;
    }
    return o;
}

PlantTensor* plant_tensor_swish(PlantTensor* a) {
    if (!a || plant_tensor_is_error(a)) return plant_tensor_create_error("swish: invalid");
    PlantTensor* o = plant_tensor_create(a->ndim, a->shape);
    if (plant_tensor_is_error(o)) return o;
    for (int64_t i = 0; i < a->size; i++) {
        double x = a->data[i];
        o->data[i] = x / (1.0 + exp(-x));
    }
    if (plant_tape_record_hook && plant_tensor_requires_grad(a)) {
        plant_tape_record_hook(16 /* OP_SWISH */, a, 0, 0.0, o);
        o->requires_grad = 1;
    }
    return o;
}

PlantTensor* plant_tensor_mish(PlantTensor* a) {
    if (!a || plant_tensor_is_error(a)) return plant_tensor_create_error("mish: invalid");
    PlantTensor* o = plant_tensor_create(a->ndim, a->shape);
    if (plant_tensor_is_error(o)) return o;
    for (int64_t i = 0; i < a->size; i++) {
        double x = a->data[i];
        double sp = (x > 0.0 ? x : 0.0) + log1p(exp(-fabs(x)));
        o->data[i] = x * tanh(sp);
    }
    if (plant_tape_record_hook && plant_tensor_requires_grad(a)) {
        plant_tape_record_hook(17 /* OP_MISH */, a, 0, 0.0, o);
        o->requires_grad = 1;
    }
    return o;
}

/* ── v0.61.0a — NN primitives (DENSE) ───────────────────────── */
PlantTensor* plant_tensor_random_uniform(int64_t ndim, const int64_t* shape,
                                         double lo, double hi) {
    PlantTensor* t;
    int64_t i;
    double range;
    if (ndim < 1 || ndim > 16 || !shape)
        return plant_tensor_create_error("rand: bad shape");
    t = plant_tensor_create(ndim, shape);
    if (!t || plant_tensor_is_error(t)) return t;
    range = hi - lo;
    for (i = 0; i < t->size; i++)
        t->data[i] = lo + range * ((double)rand() / ((double)RAND_MAX + 1.0));
    return t;
}

PlantTensor* plant_tensor_rand_uniform(PlantArray* dims, double lo, double hi) {
    int64_t nd, i, shp[16];
    if (!dims) return plant_tensor_create_error("rand: null dims");
    nd = dims->count;
    if (nd < 1 || nd > 16) return plant_tensor_create_error("rand: bad dims");
    for (i = 0; i < nd; i++) {
        const char* sv = _S(plant_list_get(dims, i));
        shp[i] = (int64_t)strtoll(sv ? sv : "0", NULL, 10);
    }
    return plant_tensor_random_uniform(nd, shp, lo, hi);
}

PlantTensor* plant_dense_forward(PlantTensor* x, PlantTensor* w, PlantTensor* b) {
    PlantTensor* xw;
    PlantTensor* y;
    if (!x || !w || !b) return plant_tensor_create_error("dense_forward: null argument");
    xw = plant_tensor_matmul(x, w);
    if (!xw || plant_tensor_is_error(xw)) return xw;
    y = plant_tensor_add(xw, b);
    plant_tensor_free(xw);          /* local intermediate (tape keeps its own ref) */
    return y;
}
