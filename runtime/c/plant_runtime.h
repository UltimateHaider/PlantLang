#ifndef PLANT_RUNTIME_H
#define PLANT_RUNTIME_H

#include <plant_types.h>
#include <stdint.h>
#include <stdlib.h>
#include <setjmp.h>

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
tx_t        plant_net_harvest(tx_t url, tx_t method, tx_t body, tx_t headers, int64_t timeout); /* response MAP: ok/status/body/headers */
int64_t plant_net_listen_open(int64_t port);
int64_t plant_net_accept(int64_t fd);

/* ── v0.48.34: HTTP Server + Socket Utilities ── */
tx_t        plant_net_listen(int64_t port);                 /* request MAP: ok/method/path/headers/body/sock */
tx_t        plant_net_respond(tx_t req, tx_t body);         /* sends HTTP/1.1 200 OK + Content-Length, closes the connection */
tx_t        plant_net_harvest_map(tx_t url, tx_t method, tx_t body, tx_t headers, int64_t timeout); /* HARVEST ... AS MAP: response MAP with live sock key */
tx_t        plant_net_read(tx_t fd);                        /* buffered read (500ms idle window); sock as decimal string */
tx_t        plant_net_write(tx_t fd, tx_t data);            /* send-all; "TRUE" on success, "FALSE" on failure */
tx_t        plant_net_close(tx_t fd);                       /* idempotent double-close-safe termination; "TRUE" */

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

/* ── v0.48.23: WEATHER/SHELTER/CALM Exception Management ──
   A PlantWeather checkpoint is a setjmp frame pushed on a thread-level
   stack by plant_weather_enter. plant_throw transfers control to the
   innermost frame (raised=1) so the generated shelter dispatch runs;
   unmatched storms are rethrown after the CALM body executes, bubbling
   up frame by frame. The volatile members are written before longjmp
   and read after it, so they stay determined across the transfer. */
typedef struct PlantWeather {
    struct PlantWeather* volatile next;
    jmp_buf buf;
    volatile int raised;
    volatile int handled;
    volatile char* exc_type;
    volatile char* exc_msg;
} PlantWeather;

void        plant_weather_enter(PlantWeather* w);
void        plant_weather_leave(PlantWeather* w);
void        plant_calm(PlantWeather* w);
void        plant_throw(const char* type, const char* msg);
const char* plant_exc_type(void);
const char* plant_exc_msg(void);
int         plant_storm_match(const char* thrown_type, const char* shelter_type);
int         plant_storm_is_known(const char* type);
const char* plant_storm_default_message(const char* type);

/* ── v0.43.0: PlantArray type (dynamic string array for split results) ── */
#define PLANT_ARRAY_MAGIC 0x504C4152 /* "PLAR" */

typedef struct PlantArray {
    uint64_t magic;  /* PLANT_ARRAY_MAGIC for reliable type detection */
    int64_t  count;
    int64_t  capacity;
    char**   items;
} PlantArray;

PlantArray* plant_list_create(int64_t capacity);
void*       plant_list_get(PlantArray* list, int64_t index);
void        plant_list_set(PlantArray* list, int64_t index, void* value);
PlantArray* plant_list_push(PlantArray* list, void* value);
PlantArray* plant_list_make(int64_t count, ...);
tx_t        plant_list_add(tx_t list, tx_t value);    /* NULL-safe: instantiate when list is NULL */
tx_t        plant_list_remove(tx_t list, tx_t value); /* first matching occurrence; NULL/empty no-op */
tx_t        plant_braid(tx_t left, tx_t right);       /* zip into pair list (min length) */
tx_t        plant_braid_map(tx_t left, tx_t right);   /* pair list with unique keys (last wins) */
tx_t        plant_link(tx_t map, tx_t key, tx_t value); /* upsert; NULL map instantiated */
tx_t        plant_sort(tx_t list, tx_t spec);    /* qsort; spec ""|"DESC"|"f:ASC,g:DESC" */
tx_t        plant_shuffle(tx_t list);            /* Fisher-Yates (in place) */

/* ── v0.44.0: Option/Result Tagged Union Helpers ── */
typedef struct PlantTagged {
    int       tag;         /* 0=Some/Ok, 1=None, 2=Err */
    void*     payload;     /* heap-allocated value */
    int       kind;        /* 0=Option, 1=Result */
} PlantTagged;

PlantTagged* plant_option_some(void* value);
PlantTagged* plant_option_none(void);
PlantTagged* plant_result_ok(void* value);
PlantTagged* plant_result_err(void* value);
int          plant_is_some(PlantTagged* t);
int          plant_is_none(PlantTagged* t);
void*        plant_unwrap(PlantTagged* t);
int          plant_is_ok(PlantTagged* t);
int          plant_is_err(PlantTagged* t);
void*        plant_unwrap_err(PlantTagged* t);

/* ── v0.44.0: Array/String Slice Primitives ── */
int64_t*     plant_array_slice(int64_t* arr, int64_t start, int64_t end);
char*        plant_string_slice(const char* str, int64_t start, int64_t end);

/* ── v0.44.0: Range Generation ── */
int64_t*     plant_range(int64_t start, int64_t end);

/* ── v0.43.0: File I/O Primitives ── */
char*       plant_file_read(const char* filepath);
int         plant_file_write(const char* filepath, const char* content);
int         plant_file_exists(const char* filepath);
int         plant_file_delete(const char* filepath);

/* ── v0.43.0: String Manipulation Primitives ── */
PlantArray* plant_string_split(const char* str, const char* delimiter);
char*       plant_string_trim(const char* str);
int64_t     plant_string_index_of(const char* str, const char* substr);

/* ── v0.48.37: Memory Safety Layer (EVAPORATE) ── */
extern long g_bal_bytes;                       /* BALANCED allocation counter */
char*       plant_str_slab_alloc(size_t n);    /* fixed-size (64B) string slab */
tx_t        plant_mem_free(tx_t v);            /* FREE statement: safe dealloc */
tx_t        plant_mem_report(void);            /* MAP: live bytes by allocator owner */
tx_t        plant_mem_scan(void);              /* audit scanner: anomalous patterns */
tx_t        plant_dist_init(tx_t nodes);
tx_t        plant_dist_alloc(tx_t size, tx_t key);
tx_t        plant_dist_node(tx_t obj);
tx_t        plant_dist_release(tx_t obj);
tx_t        plant_dist_status(void);
tx_t        plant_safe_boundary_copy(tx_t chan, tx_t payload); /* copy/transfer enforcement */

#endif
