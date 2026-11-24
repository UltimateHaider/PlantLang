/* tests/native/alloc_track.c — v0.63.4 TD-047 exact live-block tracker.
 *
 * Linked only by tests/native/leak_gate.sh. Prints one summary line on
 * stderr at process exit:
 *   TRACK live_blocks=<n> live_bytes=<n>
 * The gate compares a test's live bytes with a baseline program built the
 * same way, so runtime caches that outlive main() cancel out.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "alloc_track.h"

#define TBL_BITS 18
#define TBL_SIZE (1u << TBL_BITS)
#define TBL_MASK (TBL_SIZE - 1)
#define TOMBSTONE ((void*)(uintptr_t)1)

typedef struct { void* p; size_t n; } Slot;

static Slot   tbl[TBL_SIZE];
static size_t live_blocks = 0;
static size_t live_bytes  = 0;

static size_t hash_ptr(void* p) {
    uintptr_t x = (uintptr_t)p >> 4;
    x ^= x >> 17;
    x *= (uintptr_t)0x9E3779B97F4A7C15ULL;
    x ^= x >> 29;
    return (size_t)x & TBL_MASK;
}

static Slot* find_slot(void* p) {
    size_t i = hash_ptr(p);
    size_t probes = 0;
    while (probes++ < TBL_SIZE) {
        Slot* s = &tbl[i];
        if (s->p == NULL) return s;
        if (s->p == p) return s;
        i = (i + 1) & TBL_MASK;
    }
    return NULL;
}

static void track_add(void* p, size_t n) {
    Slot* s;
    if (!p) return;
    s = find_slot(p);
    if (!s || (s->p != NULL && s->p != TOMBSTONE)) return;
    s->p = p;
    s->n = n;
    live_blocks++;
    live_bytes += n;
}

static int track_del(void* p) {
    Slot* s;
    if (!p) return 0;
    s = find_slot(p);
    if (!s || s->p != p) return 0;   /* unknown / already freed */
    live_bytes -= s->n;
    live_blocks--;
    s->p = TOMBSTONE;
    s->n = 0;
    return 1;
}

void* track_malloc(size_t n) {
    void* p = malloc(n);
    track_add(p, n);
    return p;
}

void track_free(void* p) {
    track_del(p);
    free(p);
}

void* track_realloc(void* p, size_t n) {
    void* q;
    track_del(p);
    q = realloc(p, n);
    track_add(q, n);
    return q;
}

void* track_calloc(size_t a, size_t b) {
    void* p = calloc(a, b);
    track_add(p, a * b);
    return p;
}

__attribute__((destructor(101)))
static void track_report(void) {
    fprintf(stderr, "TRACK live_blocks=%zu live_bytes=%zu\n",
            live_blocks, live_bytes);
}
