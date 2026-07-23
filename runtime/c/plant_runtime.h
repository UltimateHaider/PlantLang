#ifndef PLANT_RUNTIME_H
#define PLANT_RUNTIME_H

#include <stdint.h>

void plnt_print_int(int64_t val);
void plnt_print_decimal(double val);
void plnt_print_bool(int8_t val);
void plnt_print_text(const char *val);
int64_t plnt_pow_i64(int64_t a, int64_t b);

#endif
