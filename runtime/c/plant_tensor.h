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

#endif /* PLANT_TENSOR_H */
