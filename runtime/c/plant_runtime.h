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

/* ── v0.42.0: Map Data Structure ── */

typedef struct PlantMapEntry {
    char* key;
    void* value;
    int   occupied;  /* 1 = active slot, 0 = empty/tombstone */
} PlantMapEntry;

typedef struct PlantMap {
    PlantMapEntry* entries;
    size_t capacity;
    size_t count;
    size_t threshold;  /* load-factor ceiling */
} PlantMap;

PlantMap* plant_map_create(size_t initial_capacity);
void      plant_map_set(PlantMap* map, const char* key, void* value);
void*     plant_map_get(PlantMap* map, const char* key);
char**    plant_map_keys(PlantMap* map, size_t* out_count);
void      plant_map_free(PlantMap* map);

/* ── v0.42.0: Iterator Protocol ── */

typedef struct PlantIterator {
    void*     container;   /* pointer to PlantMap or int64_t* array */
    int       kind;        /* 0 = MAP, 1 = ARRAY */
    size_t    index;       /* current position */
    size_t    size;        /* total count */
    char**    keys;        /* for MAP: cached key array */
    void**    values;      /* for MAP: cached value array */
    int64_t*  array_data;  /* for ARRAY: pointer to elements (skip header) */
} PlantIterator;

void      plant_iterator_init(PlantIterator* it, void* container, int kind);
int       plant_iterator_has_next(PlantIterator* it);
void*     plant_iterator_next(PlantIterator* it);
void      plant_iterator_free(PlantIterator* it);

/* ── v0.42.0: Domain Primitives ── */
void        plant_sys_action(const char* action_name, void* payload);
void        plant_env_set_weather(const char* weather_type);
const char* plant_env_get_weather(void);
void        plant_entity_set_species(void* entity, const char* species_name);

/* ── v0.43.0: PlantArray type (dynamic string array for split results) ── */
typedef struct PlantArray {
    int64_t  count;
    char**   items;
} PlantArray;

/* ── v0.43.0: File I/O Primitives ── */
char*       plant_file_read(const char* filepath);
int         plant_file_write(const char* filepath, const char* content);
int         plant_file_exists(const char* filepath);
int         plant_file_delete(const char* filepath);

/* ── v0.43.0: String Manipulation Primitives ── */
PlantArray* plant_string_split(const char* str, const char* delimiter);
char*       plant_string_trim(const char* str);
int64_t     plant_string_index_of(const char* str, const char* substr);

#endif
