#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * PlantLang Runtime Library (libplantlang.so)
 *
 * Calling convention:
 *   NUM  → int64_t
 *   SCL  → double
 *   TX   → plant_tx (fat_ptr struct: { i8* ptr, i64 len, i64 cap })
 *   FACT → int64_t (0 or 1)
 */

typedef struct {
    char*  ptr;
    int64_t len;
    int64_t cap;
} plant_tx;

/* ─────────────────────────────────────────────
 * Chained Arena Slab Allocator
 * ─────────────────────────────────────────────
 *
 * ArenaSlab: 64KB linked-list node with bump-pointer offset.
 * Arena: head/current pointers + slab_size.
 *
 * arena_alloc: fast bump allocation within current slab;
 *   auto-chains a new 64KB slab when capacity is exhausted.
 *   Alloc'd memory persists for the arena lifetime — no free needed.
 */

#define ARENA_DEFAULT_SLAB_SIZE (65536)

typedef struct ArenaSlab {
    char               data[ARENA_DEFAULT_SLAB_SIZE];
    size_t             offset;
    struct ArenaSlab*  next;
} ArenaSlab;

typedef struct {
    ArenaSlab*  head;
    ArenaSlab*  current;
    size_t      slab_size;
} Arena;

/* Initialise a stack-allocated Arena with one empty slab. */
static void arena_init(Arena* arena) {
    ArenaSlab* slab = (ArenaSlab*)malloc(sizeof(ArenaSlab));
    slab->offset = 0;
    slab->next = NULL;
    arena->head = slab;
    arena->current = slab;
    arena->slab_size = ARENA_DEFAULT_SLAB_SIZE;
}

/* Free all slabs in the arena chain. */
static void arena_destroy(Arena* arena) {
    ArenaSlab* s = arena->head;
    while (s) {
        ArenaSlab* next = s->next;
        free(s);
        s = next;
    }
    arena->head = NULL;
    arena->current = NULL;
}

/* Bump-allocate `size` bytes from the arena (8-byte aligned).
 * Chains a new slab if the current one is full. */
static void* arena_alloc(Arena* arena, size_t size) {
    size = (size + 7) & ~7; /* align to 8 */
    if (arena->current->offset + size > arena->slab_size) {
        ArenaSlab* slab = (ArenaSlab*)malloc(sizeof(ArenaSlab));
        slab->offset = 0;
        slab->next = NULL;
        arena->current->next = slab;
        arena->current = slab;
    }
    void* ptr = arena->current->data + arena->current->offset;
    arena->current->offset += size;
    return ptr;
}

/* ─────────────────────────────────────────────
 * String Operations
 * ───────────────────────────────────────────── */

plant_tx plnt_string_concat(plant_tx a, plant_tx b) {
    int64_t totalLen = a.len + b.len;
    char* buf = (char*)malloc((size_t)totalLen + 1);
    if (!buf) return (plant_tx){ NULL, 0, 0 };
    if (a.ptr && a.len > 0) memcpy(buf, a.ptr, (size_t)a.len);
    if (b.ptr && b.len > 0) memcpy(buf + a.len, b.ptr, (size_t)b.len);
    buf[totalLen] = '\0';
    return (plant_tx){ buf, totalLen, totalLen + 1 };
}

int64_t plnt_string_len(plant_tx s) {
    return s.len;
}

/* ─────────────────────────────────────────────
 * String SPLIT
 *
 * Splits `src` on each occurrence of `delim`.
 * Returns a plant_tx whose ptr field points to
 * an arena-allocated array of plant_tx (one per
 * substring).  len/cap hold the element count.
 *
 * Two-pass:
 *   Pass 1 — count parts.
 *   Pass 2 — allocate part array + each substring.
 * ───────────────────────────────────────────── */

void plnt_str_split(plant_tx* result, char* src_ptr, int64_t src_len, int64_t src_cap, char* delim_ptr, int64_t delim_len, int64_t delim_cap) {
    plant_tx src  = { src_ptr,  src_len,  src_cap  };
    plant_tx delim = { delim_ptr, delim_len, delim_cap };
    if (!src.ptr || src.len == 0) {
        /* Empty source → one empty part */
        plant_tx* arr = (plant_tx*)malloc(sizeof(plant_tx));
        arr[0] = (plant_tx){ NULL, 0, 0 };
        *result = (plant_tx){ (char*)arr, 1, 1 };
        return;
    }
    if (!delim.ptr || delim.len == 0) {
        /* No delimiter → return source as single element */
        plant_tx* arr = (plant_tx*)malloc(sizeof(plant_tx));
        char* copy = (char*)malloc((size_t)src.len + 1);
        memcpy(copy, src.ptr, (size_t)src.len);
        copy[src.len] = '\0';
        arr[0] = (plant_tx){ copy, src.len, src.len + 1 };
        *result = (plant_tx){ (char*)arr, 1, 1 };
        return;
    }

    /* Pass 1: count parts */
    int64_t count = 1;
    const char* p = src.ptr;
    const char* end = src.ptr + src.len;
    while (p + delim.len <= end) {
        if (memcmp(p, delim.ptr, (size_t)delim.len) == 0) {
            count++;
            p += delim.len;
        } else {
            p++;
        }
    }

    /* Pass 2: allocate part array + substring copies */
    plant_tx* parts = (plant_tx*)malloc((size_t)count * sizeof(plant_tx));
    int64_t idx = 0;
    const char* start = src.ptr;
    p = src.ptr;
    while (p + delim.len <= end) {
        if (memcmp(p, delim.ptr, (size_t)delim.len) == 0) {
            int64_t partLen = p - start;
            char* copy = (char*)malloc((size_t)partLen + 1);
            if (partLen > 0) memcpy(copy, start, (size_t)partLen);
            copy[partLen] = '\0';
            parts[idx++] = (plant_tx){ copy, partLen, partLen + 1 };
            start = p + delim.len;
            p += delim.len;
        } else {
            p++;
        }
    }
    /* Last part */
    int64_t partLen = end - start;
    char* copy = (char*)malloc((size_t)partLen + 1);
    if (partLen > 0) memcpy(copy, start, (size_t)partLen);
    copy[partLen] = '\0';
    parts[idx++] = (plant_tx){ copy, partLen, partLen + 1 };

    *result = (plant_tx){ (char*)parts, count, count };
}

/* ─────────────────────────────────────────────
 * String JOIN
 *
 * Concatenates all strings in `parts` (a plant_tx
 * whose ptr is an array of plant_tx) separated by
 * `delim`.  Returns a single plant_tx with the
 * combined string (arena-allocated).
 *
 * Two-pass:
 *   Pass 1 — compute total byte length.
 *   Pass 2 — memcpy each substring + delimiter.
 * ───────────────────────────────────────────── */

void plnt_str_join(plant_tx* result, char* parts_ptr, int64_t parts_len, int64_t parts_cap, char* delim_ptr, int64_t delim_len, int64_t delim_cap) {
    plant_tx parts = { parts_ptr, parts_len, parts_cap };
    plant_tx delim = { delim_ptr, delim_len, delim_cap };
    if (!parts.ptr || parts.len == 0) {
        *result = (plant_tx){ NULL, 0, 0 };
        return;
    }

    int64_t count = parts.len;
    plant_tx* arr = (plant_tx*)parts.ptr;

    /* Pass 1: compute total length */
    int64_t totalLen = 0;
    for (int64_t i = 0; i < count; i++) {
        totalLen += arr[i].len;
    }
    if (count > 1) {
        totalLen += delim.len * (count - 1);
    }

    /* Pass 2: assemble */
    char* buf = (char*)malloc((size_t)totalLen + 1);
    if (!buf) { *result = (plant_tx){ NULL, 0, 0 }; return; }
    char* out = buf;
    for (int64_t i = 0; i < count; i++) {
        if (i > 0 && delim.len > 0) {
            memcpy(out, delim.ptr, (size_t)delim.len);
            out += delim.len;
        }
        if (arr[i].len > 0 && arr[i].ptr) {
            memcpy(out, arr[i].ptr, (size_t)arr[i].len);
            out += arr[i].len;
        }
    }
    *out = '\0';
    *result = (plant_tx){ buf, totalLen, totalLen + 1 };
}

/* ─────────────────────────────────────────────
 * List / Array Operations
 * ───────────────────────────────────────────── */

static int cmp_i64_asc(const void* a, const void* b) {
    int64_t x = *(const int64_t*)a;
    int64_t y = *(const int64_t*)b;
    if (x < y) return -1;
    if (x > y) return  1;
    return 0;
}

static int cmp_dbl_asc(const void* a, const void* b) {
    double x = *(const double*)a;
    double y = *(const double*)b;
    if (x < y) return -1;
    if (x > y) return  1;
    return 0;
}

void plnt_sort_i64(void* buf, int64_t len) {
    if (!buf || len <= 1) return;
    qsort(buf, (size_t)len, sizeof(int64_t), cmp_i64_asc);
}

void plnt_sort_double(void* buf, int64_t len) {
    if (!buf || len <= 1) return;
    qsort(buf, (size_t)len, sizeof(double), cmp_dbl_asc);
}

/* ─────────────────────────────────────────────
 * Math Functions
 * ───────────────────────────────────────────── */

double plnt_sqrt(double x)  { return sqrt(x); }
double plnt_sin(double x)   { return sin(x);  }
double plnt_cos(double x)   { return cos(x);  }
double plnt_tan(double x)   { return tan(x);  }
double plnt_floor(double x) { return floor(x); }
double plnt_ceil(double x)  { return ceil(x);  }
double plnt_abs(double x)   { return fabs(x); }

/* ─────────────────────────────────────────────
 * Stress Test: Large-String Split/Join
 *
 * Creates a ~70KB string, splits by ",", joins
 * back, and verifies roundtrip equality.
 * Prints PASS/FAIL status lines for test harness.
 * ───────────────────────────────────────────── */
void plnt_stress_test_split_join(void) {
    int64_t n = 70000;
    char* big = (char*)malloc((size_t)n + 1);
    if (!big) { printf("FAIL: malloc big\n"); return; }
    /* Fill with pattern "abcdefghij," repeated */
    for (int64_t i = 0; i < n; i++) {
        big[i] = (i % 11 == 10) ? ',' : ('a' + (i % 10));
    }
    big[n] = '\0';

    /* Count delim "," occurrences */
    int64_t count = 1;
    for (int64_t i = 0; i < n; i++) {
        if (big[i] == ',') count++;
    }

    printf("Stress test: input len=%ld, expected parts=%ld\n", n, count);

    /* Split via plnt_str_split (decomposed params + sret) */
    plant_tx split_result;
    plnt_str_split(&split_result, big, n, n + 1, ",", 1, 2);

    plant_tx* parts = (plant_tx*)split_result.ptr;
    int64_t got_count = split_result.len;
    printf("Split count: %ld\n", got_count);

    /* Join parts back via plnt_str_join */
    plant_tx join_result;
    plnt_str_join(&join_result, (char*)parts, got_count, got_count, ",", 1, 2);

    printf("Joined length: %ld\n", join_result.len);

    /* Verify roundtrip */
    if (join_result.len == n && memcmp(big, join_result.ptr, (size_t)n) == 0) {
        printf("PASS: stress test roundtrip OK\n");
    } else {
        printf("FAIL: roundtrip mismatch (got len %ld, expected %ld)\n", join_result.len, n);
    }

    /* Free split allocations (each part string + parts array) */
    for (int64_t i = 0; i < got_count; i++) {
        if (parts[i].ptr) free(parts[i].ptr);
    }
    free(parts);
    /* Free joined result */
    if (join_result.ptr) free(join_result.ptr);
    free(big);
}
