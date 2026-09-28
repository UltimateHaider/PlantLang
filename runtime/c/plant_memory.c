/* runtime/c/plant_memory.c — Unified memory management (v0.51.2a) */
#include "plant_memory.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Minimum valid heap pointer value.  The first page (0x0000..0x0FFF) is
   always unmapped on modern systems, so any real pointer is >= 0x1000. */
#define _PLANT_PTR_MIN ((uintptr_t)0x1000)

/* Compile-time verification: magic MUST be first field */
ASSERT_MAGIC_FIRST(PlantArray);
ASSERT_MAGIC_FIRST(PlantTensor);

/* v0.51.2d.1 — TD-010: header-tagged heap strings for numeric values.
   _from_digit (multi-digit) allocates through plant_heapstr_alloc_copy;
   plant_list_free releases only blocks carrying PLANT_STR_MAGIC, so
   static string literals are never freed. */
#define PLANT_STR_MAGIC ((uint64_t)0x504C4E5453545231ULL)  /* "PLNTSTR1" */

typedef struct { uint64_t magic; } PlantStrHdr;

char* plant_heapstr_alloc_copy(const char* s) {
    size_t n;
    PlantStrHdr* h;
    if (!s) return NULL;
    n = strlen(s);
    /* v0.51.2d.1 — TD-010: use a non-simulated allocator for numeric
     * strings (realloc(NULL, n)), matching the original strdup
     * semantics so MAT_MALLOC_FAIL_AT injection points are preserved. */
    h = (PlantStrHdr*)realloc(NULL, sizeof(PlantStrHdr) + n + 1);
    if (!h) return NULL;
    h->magic = PLANT_STR_MAGIC;
    memcpy((char*)h + sizeof(PlantStrHdr), s, n + 1);
    return (char*)h + sizeof(PlantStrHdr);
}

int plant_heapstr_release(void* p) {
    PlantStrHdr* h;
    if (!p) return 0;
    if ((uintptr_t)p < _PLANT_PTR_MIN) return 0;
    h = (PlantStrHdr*)((char*)p - sizeof(PlantStrHdr));
    if (h->magic != PLANT_STR_MAGIC) return 0;
    h->magic = 0;
    free(h);
    return 1;
}

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
            } else {
                /* v0.51.2d.1 — TD-010: free header-tagged numeric strings. */
                plant_heapstr_release(item);
            }
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
