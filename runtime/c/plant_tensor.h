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

#endif /* PLANT_TENSOR_H */
