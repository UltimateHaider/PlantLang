/*
 * plant_tensor.h — v0.51.1: PlantTensor type definition.
 *
 * A NEW native type for N-dimensional numerical tensors.
 * NOT an extension of PlantArray — contiguous double* data storage,
 * row-major layout, reference-counted lifecycle.
 */

#ifndef PLANT_TENSOR_H
#define PLANT_TENSOR_H

#include <stdint.h>

#define PLANT_TENSOR_MAGIC 0x5453454EULL  /* "TSEN" */

typedef struct PlantTensor {
    uint64_t magic;      /* PLANT_TENSOR_MAGIC */
    int64_t  ndim;       /* Number of dimensions (1..N) */
    int64_t* shape;      /* shape[0..ndim-1] */
    int64_t* strides;    /* Row-major strides, in elements */
    double*  data;       /* Contiguous row-major storage */
    int64_t  size;       /* Total elements (product of shape) */
    int64_t  ref_count;  /* Reference count (starts at 1) */
    char*    error_msg;  /* NULL if no error */
    int64_t  requires_grad;  /* v0.56.0a — autograd */
} PlantTensor;

/* Lifecycle */
PlantTensor* plant_tensor_create(int64_t ndim, const int64_t* shape);
PlantTensor* plant_tensor_create_error(const char* msg);
void         plant_tensor_free(PlantTensor* t);
PlantTensor* plant_tensor_deep_copy(const PlantTensor* t);

/* Error helpers */
int          plant_tensor_is_error(const PlantTensor* t);
const char*  plant_tensor_error_msg(const PlantTensor* t);

/* Type detection */
int          plant_tensor_is_tensor(const void* ptr);

/* Display */
char*        plant_tensor_to_string(const PlantTensor* t);
int64_t      plant_tensor_to_string_static(const PlantTensor* t,
                                           char* buf, int64_t bufsize);

/* Construction from PlantArray (nested list) */
void* plant_tensor_from_list(void* list);

/* Introspection — v0.51.2b */
struct PlantArray;  /* forward declaration */
struct PlantArray*  plant_tensor_shape(PlantTensor* t);
int64_t             plant_tensor_ndim(PlantTensor* t);
int64_t             plant_tensor_size(PlantTensor* t);

/* v0.55.0 — Tensor Operations (core). */

/* Elementwise (same-shape). Error tensor on mismatch. */
PlantTensor* plant_tensor_add(PlantTensor* a, PlantTensor* b);
PlantTensor* plant_tensor_sub(PlantTensor* a, PlantTensor* b);
PlantTensor* plant_tensor_mul(PlantTensor* a, PlantTensor* b);
PlantTensor* plant_tensor_div(PlantTensor* a, PlantTensor* b);

/* Scalar multiplication. */
PlantTensor* plant_tensor_scale(PlantTensor* a, double s);

/* Matrix multiplication [m,n] x [n,p] -> [m,p]. */
PlantTensor* plant_tensor_matmul(PlantTensor* a, PlantTensor* b);

/* Shape ops. */
PlantTensor* plant_tensor_transpose(PlantTensor* a);
PlantTensor* plant_tensor_reshape(PlantTensor* a, struct PlantArray* shape);
/* v0.61.2 — TD-067: variadic reshape (no list literal → no leak). */
PlantTensor* plant_tensor_reshape_n(PlantTensor* a, int nd, ...);

/* v0.61.2 — TD-067: legacy TENSOR_RESHAPE(a, [..]) target. Codegen always
   passes a fresh list literal, so this owns and frees it. Never pass a
   shared list variable to TENSOR_RESHAPE. */
PlantTensor* plant_tensor_reshape_and_free(PlantTensor* a, struct PlantArray* shape);

/* v0.61.2 — CONV2D (NCHW, stride/pad single value). b may be NULL. */
PlantTensor* plant_conv2d_forward(PlantTensor* x, PlantTensor* k,
                                  PlantTensor* b, int64_t stride, int64_t pad);

/* v0.61.3 — BatchNorm: per-channel (4-D NCHW) or per-feature (2-D). */
PlantTensor* plant_batchnorm_forward(PlantTensor* x, PlantTensor* gamma,
                                     PlantTensor* beta, double eps);
/* v0.61.3 — LayerNorm: last axis, any ndim. */
PlantTensor* plant_layernorm_forward(PlantTensor* x, PlantTensor* gamma,
                                     PlantTensor* beta, double eps);

/* v0.61.4 — Dropout (deterministic local PRNG; p = 0 is eval/identity). */
PlantTensor* plant_dropout_forward(PlantTensor* x, double p, int64_t seed);
/* Shared PRNG helpers (forward and backward must stay in lock-step). */
uint64_t plant_dropout_state(int64_t seed);
double   plant_dropout_uniform(uint64_t* st);

/* Reductions (scalar). */
double plant_tensor_sum(PlantTensor* a);
double plant_tensor_mean(PlantTensor* a);
double plant_tensor_max(PlantTensor* a);
double plant_tensor_min(PlantTensor* a);

/* v0.55.1 — tensor completion. */
PlantTensor* plant_tensor_slice(PlantTensor* a, struct PlantArray* ranges);
double       plant_tensor_at(PlantTensor* a, struct PlantArray* idx);

/* v0.55.2 — Activations. */
PlantTensor* plant_tensor_relu(PlantTensor* a);
PlantTensor* plant_tensor_sigmoid(PlantTensor* a);
PlantTensor* plant_tensor_tanh(PlantTensor* a);
PlantTensor* plant_tensor_softmax(PlantTensor* a, int axis);

/* v0.55.3 — Modern activations. */
PlantTensor* plant_tensor_gelu(PlantTensor* a);
PlantTensor* plant_tensor_swish(PlantTensor* a);
PlantTensor* plant_tensor_mish(PlantTensor* a);

/* v0.56.0a — Autograd primitives. */
void         plant_tensor_retain(PlantTensor* t);
int          plant_tensor_requires_grad(PlantTensor* t);
void         plant_tensor_set_requires_grad(PlantTensor* t, int v);
/* v0.61.0a — NN primitives (DENSE). */
PlantTensor* plant_tensor_random_uniform(int64_t ndim, const int64_t* shape,
                                         double lo, double hi);
PlantTensor* plant_tensor_rand_uniform(struct PlantArray* dims, double lo, double hi);
PlantTensor* plant_dense_forward(PlantTensor* x, PlantTensor* w, PlantTensor* b);

PlantTensor* plant_tensor_track(PlantTensor* t);

/* v0.56.0a — tape hook (defined in plant_tensor.c, assigned by
   plant_autograd.c). NULL disables recording. */
extern void (*plant_tape_record_hook)(int op, PlantTensor* a,
                                      PlantTensor* b, double s,
                                      PlantTensor* out);
/* v0.61.2 — CONV2D tape hook (set by the autograd engines). */
extern void (*plant_tape_record_conv_hook)(PlantTensor* x, PlantTensor* k,
                                           PlantTensor* b, int64_t stride,
                                           int64_t pad, PlantTensor* out);

/* v0.61.3 — norm tape hooks (set by the autograd engines). */
extern void (*plant_tape_record_batchnorm_hook)(PlantTensor* x, PlantTensor* g,
                                                PlantTensor* b, double eps,
                                                PlantTensor* out);
extern void (*plant_tape_record_layernorm_hook)(PlantTensor* x, PlantTensor* g,
                                                PlantTensor* b, double eps,
                                                PlantTensor* out);
extern void (*plant_tape_record_dropout_hook)(PlantTensor* x, double p,
                                              int64_t seed, PlantTensor* out);

/* v0.56.0b-2 — Broadcast helpers (exported). */
int64_t      plant_tensor_bc_index(PlantTensor* t, const int64_t* oidx,
                                   int64_t out_nd);
PlantTensor* plant_tensor_bcast_reduce(const PlantTensor* d,
                                       const PlantTensor* t);

/* v0.58.0 — Magic-checked retain/free (ownership epic). */
void plant_tensor_retain_if_tensor(void* p);
void plant_tensor_free_if_tensor(void* p);

#endif /* PLANT_TENSOR_H */
