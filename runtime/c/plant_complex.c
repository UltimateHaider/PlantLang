#include "plant_complex.h"
#include "plant_compat.h"
#include <math.h>

/* v0.51.11 — CMP accessors + transcendental helpers.
 * Reuses PlantComplex arithmetic from plant_math.c. */

tx_t plant_cmp_real(PlantComplex z) { return _from_double(z.real); }
tx_t plant_cmp_imag(PlantComplex z) { return _from_double(z.imag); }
tx_t plant_cmp_arg(PlantComplex z) { return _from_double(atan2(z.imag, z.real)); }
tx_t plant_cmp_cabs(PlantComplex z) { return _from_double(plant_complex_abs(z)); }

PlantComplex plant_cmp_exp(PlantComplex z) {
    double e = exp(z.real);
    return plant_complex_make(e * cos(z.imag), e * sin(z.imag));
}
PlantComplex plant_cmp_log(PlantComplex z) {
    return plant_complex_make(log(plant_complex_abs(z)), atan2(z.imag, z.real));
}
PlantComplex plant_cmp_sqrt(PlantComplex z) {
    double r = plant_complex_abs(z);
    double re = sqrt((r + z.real) / 2.0);
    double im = (z.imag >= 0 ? 1.0 : -1.0) * sqrt((r - z.real) / 2.0);
    return plant_complex_make(re, im);
}
PlantComplex plant_cmp_sin(PlantComplex z) {
    return plant_complex_make(sin(z.real) * cosh(z.imag),
                              cos(z.real) * sinh(z.imag));
}
PlantComplex plant_cmp_cos(PlantComplex z) {
    return plant_complex_make(cos(z.real) * cosh(z.imag),
                              -sin(z.real) * sinh(z.imag));
}
