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
