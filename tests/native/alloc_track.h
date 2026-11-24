/* tests/native/alloc_track.h — v0.63.4 TD-047 leak-gate allocator.
 *
 * Test-only. The leak gate recompiles the runtime with
 *   -Dmalloc=track_malloc -Dfree=track_free
 *   -Drealloc=track_realloc -Dcalloc=track_calloc
 * so every heap block the runtime takes or returns is accounted exactly.
 * Never linked into the shipped compiler.
 */
#ifndef PLANT_TEST_ALLOC_TRACK_H
#define PLANT_TEST_ALLOC_TRACK_H

#include <stddef.h>

void* track_malloc(size_t n);
void  track_free(void* p);
void* track_realloc(void* p, size_t n);
void* track_calloc(size_t a, size_t b);

#endif /* PLANT_TEST_ALLOC_TRACK_H */
