/* runtime/c/plant_memory.h — Unified memory management (v0.51.2a) */
#ifndef PLANT_MEMORY_H
#define PLANT_MEMORY_H

#include "plant_runtime.h"
#include "plant_tensor.h"
#include <stddef.h>

/* === Type-specific free (v0.51.2a — used by codegen) === */
void plant_array_free(PlantArray* list);     /* basic (no recursion) */
void plant_list_free(PlantArray* list);      /* recursive */

/* plant_tensor_free is declared in plant_tensor.h */

/* === Generic dispatcher ===
 * v0.51.2a: AVAILABLE for manual use, NOT emitted by codegen.
 * v0.51.2b: Will be emitted by codegen. See TECH_DEBT.md (TD-005). */
void plant_free(void* obj);

/* === Type detection helpers === */
int plant_is_list(const void* obj);
int plant_is_tensor(const void* obj);

/* === Compile-time magic layout verification === */
#define ASSERT_MAGIC_FIRST(type) \
    typedef char type##_magic_must_be_first[ \
        (offsetof(type, magic) == 0) ? 1 : -1 \
    ]

#endif /* PLANT_MEMORY_H */
