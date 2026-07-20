#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * PlantLang Runtime Library (libplantlang.so)
 *
 * Provides C implementations of performance-critical operations
 * that are called via FFI from the LLVM backend.
 *
 * Calling convention:
 *   NUM  → int64_t
 *   SCL  → double
 *   TX   → plant_tx (fat_ptr struct: { i8* ptr, i64 len, i64 cap })
 *   FACT → int64_t (0 or 1)
 *
 * All functions use standard cdecl calling convention for
 * compatibility with LLVM's declare/call mechanism.
 */

typedef struct {
    char*  ptr;
    int64_t len;
    int64_t cap;
} plant_tx;

/* ─────────────────────────────────────────────
 * String Operations
 * ───────────────────────────────────────────── */

/* Concatenate two TX strings into a new heap-allocated TX.
 * Caller is responsible for freeing the returned buffer.
 * Returns: plant_tx with malloc'd ptr (caller must free). */
plant_tx plnt_string_concat(plant_tx a, plant_tx b) {
    int64_t totalLen = a.len + b.len;
    char* buf = (char*)malloc((size_t)totalLen + 1);
    if (!buf) return (plant_tx){ NULL, 0, 0 };
    if (a.ptr && a.len > 0) memcpy(buf, a.ptr, (size_t)a.len);
    if (b.ptr && b.len > 0) memcpy(buf + a.len, b.ptr, (size_t)b.len);
    buf[totalLen] = '\0';
    return (plant_tx){ buf, totalLen, totalLen + 1 };
}

/* Return the length of a TX string. */
int64_t plnt_string_len(plant_tx s) {
    return s.len;
}

/* ─────────────────────────────────────────────
 * List / Array Operations
 * ───────────────────────────────────────────── */

/* Compare function for int64_t qsort (ascending). */
static int cmp_i64_asc(const void* a, const void* b) {
    int64_t x = *(const int64_t*)a;
    int64_t y = *(const int64_t*)b;
    if (x < y) return -1;
    if (x > y) return  1;
    return 0;
}

/* Compare function for double qsort (ascending). */
static int cmp_dbl_asc(const void* a, const void* b) {
    double x = *(const double*)a;
    double y = *(const double*)b;
    if (x < y) return -1;
    if (x > y) return  1;
    return 0;
}

/* Sort a NUM (int64_t) array in-place.
 *   buf  — pointer to the array data (i8* from fat_ptr)
 *   len  — number of elements */
void plnt_sort_i64(void* buf, int64_t len) {
    if (!buf || len <= 1) return;
    qsort(buf, (size_t)len, sizeof(int64_t), cmp_i64_asc);
}

/* Sort a SCL (double) array in-place. */
void plnt_sort_double(void* buf, int64_t len) {
    if (!buf || len <= 1) return;
    qsort(buf, (size_t)len, sizeof(double), cmp_dbl_asc);
}

/* ─────────────────────────────────────────────
 * Math Functions (thin wrappers for LLVM FFI)
 * ───────────────────────────────────────────── */

double plnt_sqrt(double x)  { return sqrt(x); }
double plnt_sin(double x)   { return sin(x);  }
double plnt_cos(double x)   { return cos(x);  }
double plnt_tan(double x)   { return tan(x);  }
double plnt_floor(double x) { return floor(x); }
double plnt_ceil(double x)  { return ceil(x);  }
double plnt_abs(double x)   { return fabs(x); }
