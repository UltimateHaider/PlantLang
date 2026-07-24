#ifndef PLANT_RUNTIME_H
#define PLANT_RUNTIME_H

#include <stdint.h>
#include <stdlib.h>

void plnt_print_int(int64_t val);
void plnt_print_decimal(double val);
void plnt_print_bool(int8_t val);
void plnt_print_text(const char *val);
int64_t plnt_pow_i64(int64_t a, int64_t b);

void* plant_alloc(size_t size);
void plant_free(void* ptr);
char* plant_str_concat(const char* a, const char* b);
int64_t* plant_array_create(int64_t capacity);
int64_t plant_array_get(int64_t* arr, int64_t index);
void plant_array_set(int64_t* arr, int64_t index, int64_t value);

#endif
