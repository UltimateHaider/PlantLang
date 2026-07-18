#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

/*
 * Runtime bridge between PlantLang TX (fat pointer) and C I/O functions.
 *
 * Matches the LLVM %fat_ptr = type { i8*, i64, i64 } struct.
 * On x86-64 System V ABI, the 24-byte struct is passed via memory
 * (caller allocates temp copy, passes pointer).  LLVM IR's struct-by-value
 * and C's struct-by-value generate identical ABI, ensuring compatibility.
 */

typedef struct {
    char*  ptr;
    int64_t len;
    int64_t cap;
} __attribute__((packed)) plant_tx;

/* Print a TX to stdout (no newline). Returns bytes written. */
int64_t plant_printf(plant_tx s) {
    (void)s.cap;
    return (int64_t)fwrite(s.ptr, 1, (size_t)s.len, stdout);
}

/* Print a TX to stdout with trailing newline. Returns bytes written (incl newline). */
int64_t plant_puts(plant_tx s) {
    (void)s.cap;
    int64_t n = (int64_t)fwrite(s.ptr, 1, (size_t)s.len, stdout);
    fputc('\n', stdout);
    return n + 1;
}

/* Flush stdout. Returns 0. */
int64_t plant_flush(void) {
    fflush(stdout);
    return 0;
}
