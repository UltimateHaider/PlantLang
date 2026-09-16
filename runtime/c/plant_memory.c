/* runtime/c/plant_memory.c — Unified memory management (v0.51.2a) */
#include "plant_memory.h"
#include <stdint.h>
#include <stdlib.h>

/* Minimum valid heap pointer value.  The first page (0x0000..0x0FFF) is
   always unmapped on modern systems, so any real pointer is >= 0x1000. */
#define _PLANT_PTR_MIN ((uintptr_t)0x1000)

/* Compile-time verification: magic MUST be first field */
ASSERT_MAGIC_FIRST(PlantArray);
ASSERT_MAGIC_FIRST(PlantTensor);

/* === Basic array free (no recursion) === */
void plant_array_free(PlantArray* list) {
    if (!list) return;
    if (list->items) {
        free(list->items);
        list->items = NULL;
    }
    free(list);
}

/* === Recursive list free === */
void plant_list_free(PlantArray* list) {
    int64_t i;
    if (!list) return;
    if (list->items) {
        for (i = 0; i < list->count; i++) {
            void* item = (void*)list->items[i];
            uint64_t magic;
            if (!item) continue;
            if ((uintptr_t)item < _PLANT_PTR_MIN) continue;  /* small int — not a heap obj */
            magic = *(uint64_t*)item;
            if (magic == PLANT_ARRAY_MAGIC) {
                plant_list_free((PlantArray*)item);
            } else if (magic == PLANT_TENSOR_MAGIC) {
                plant_tensor_free((PlantTensor*)item);
            }
            /* else: primitive — not freed here */
        }
        free(list->items);
        list->items = NULL;
    }
    free(list);
}

/* === Generic dispatcher (stub in v0.51.2a) === */
void plant_free(void* obj) {
    uint64_t magic;
    if (!obj) return;
    if ((uintptr_t)obj < _PLANT_PTR_MIN) return;  /* small int — nothing to free */
    magic = *(uint64_t*)obj;
    if (magic == PLANT_ARRAY_MAGIC) {
        plant_list_free((PlantArray*)obj);
    } else if (magic == PLANT_TENSOR_MAGIC) {
        plant_tensor_free((PlantTensor*)obj);
    }
    /* Unknown magic: silently ignore (defensive) */
}

int plant_is_list(const void* obj) {
    if (!obj) return 0;
    if ((uintptr_t)obj < _PLANT_PTR_MIN) return 0;
    return *(const uint64_t*)obj == PLANT_ARRAY_MAGIC;
}

int plant_is_tensor(const void* obj) {
    if (!obj) return 0;
    if ((uintptr_t)obj < _PLANT_PTR_MIN) return 0;
    return *(const uint64_t*)obj == PLANT_TENSOR_MAGIC;
}
