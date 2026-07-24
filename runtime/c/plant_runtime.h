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

/* ── v0.41.0: Network / Socket Helpers ── */
char* plant_net_harvest(const char* url, const char* method, const char* body, const char* headers, int64_t timeout_sec);
int64_t plant_net_listen_open(int64_t port);
int64_t plant_net_accept(int64_t fd);
char* plant_net_read(int64_t fd);
int64_t plant_net_write(int64_t fd, const char* data);
void plant_net_close(int64_t fd);

#endif
