#ifndef PLANT_COMPLEX_H
#define PLANT_COMPLEX_H

/* v0.51.11 — PlantLang CMP support.
 *
 * The complex value type is PlantComplex (defined in plant_math.h, the
 * v0.50.2 Complex Number Subsystem). This header adds only the accessors
 * and transcendental helpers that PlantComplex does not already provide,
 * under non-colliding names. The arithmetic core (add/sub/mul/div/conj/
 * abs/make) is reused from plant_math.c — no duplication.
 *
 * Accessors that produce a real value return tx_t (string), matching the
 * language runtime convention (cf. plant_abs). Complex-valued helpers
 * return PlantComplex. */

#include "plant_math.h"
#include "plant_types.h"

/* Accessors (real-valued → tx_t) */
tx_t plant_cmp_real(PlantComplex z);
tx_t plant_cmp_imag(PlantComplex z);
tx_t plant_cmp_arg(PlantComplex z);
tx_t plant_cmp_cabs(PlantComplex z);

/* Complex-valued helpers */
PlantComplex plant_cmp_exp(PlantComplex z);
PlantComplex plant_cmp_log(PlantComplex z);
PlantComplex plant_cmp_sqrt(PlantComplex z);
PlantComplex plant_cmp_sin(PlantComplex z);
PlantComplex plant_cmp_cos(PlantComplex z);

#endif
