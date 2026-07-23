#include "plant_runtime.h"
#include <stdio.h>

void plnt_print_int(int64_t val) {
    printf("%lld\n", (long long)val);
}

void plnt_print_decimal(double val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.10g", val);
    printf("%s\n", buf);
}

void plnt_print_bool(int8_t val) {
    printf("%s\n", val ? "true" : "false");
}

void plnt_print_text(const char *val) {
    printf("%s\n", val);
}

/* Integer power: a^b for non-negative b */
int64_t plnt_pow_i64(int64_t a, int64_t b) {
    if (b < 0) return 0;
    int64_t r = 1;
    while (b--) r *= a;
    return r;
}
