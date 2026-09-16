/*
 * plant_malloc.h — v0.51.1: Shared malloc wrapper with failure simulation.
 *
 * Provides plant_malloc(size) which either calls malloc() directly
 * (production) or routes through plant_malloc_sim() for test injection.
 * Gated by PLANT_MALLOC_SIMULATE. C89-compatible.
 */

#ifndef PLANT_MALLOC_H
#define PLANT_MALLOC_H

#include <stdlib.h>

#ifdef PLANT_MALLOC_SIMULATE

int   plant_malloc_sim_get_count(void);
void  plant_malloc_sim_reset(void);
void* plant_malloc_sim(size_t size);

#define plant_malloc(size) plant_malloc_sim(size)

#else

#define plant_malloc(size) malloc(size)

#endif /* PLANT_MALLOC_SIMULATE */

#endif /* PLANT_MALLOC_H */
