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
tx_t        plant_net_listen_timeout(int64_t port, int64_t timeout); /* LISTEN ... TIMEOUT t: accept() fails ok=FALSE after t seconds */
tx_t        plant_net_respond(tx_t req, tx_t body);         /* sends HTTP/1.1 200 OK + Content-Length, closes the connection */
tx_t        plant_net_harvest_map(tx_t url, tx_t method, tx_t body, tx_t headers, int64_t timeout); /* HARVEST ... AS MAP: response MAP with live sock key */
tx_t        plant_net_harvest_json(tx_t url, tx_t method, tx_t body, tx_t headers, int64_t timeout); /* HARVEST ... AS JSON: parsed PlantJson in body key */
tx_t        plant_net_respond_json(tx_t req, tx_t body);              /* GIVE ... AS RESPONSE JSON: json_stringify + application/json */
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

PlantMap* plant_map_hash_create(size_t initial_capacity);
void      plant_map_hash_set(PlantMap* map, const char* key, void* value);
void*     plant_map_get(PlantMap* map, const char* key);
int       plant_map_has(PlantMap* map, const char* key); /* 1 if present; dispatches on representation */
char**    plant_map_keys(PlantMap* map, size_t* out_count);
void      plant_map_free(PlantMap* map);

/* v0.49.5 — native map literal API: plant_map_create()/plant_map_set()
   build the language's pair-list MAP representation (PlantArray,
   kind = 1) — the form LINK / _map_get / plant_map_to_string /
   json_stringify all consume. set upserts (existing key replaced,
   like plant_link) and returns the map so calls chain. */
tx_t plant_map_create(void);
tx_t plant_map_set(tx_t map, tx_t key, tx_t value);

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
#define PLANT_WEATHER_EXIT_MAX 64

typedef struct PlantWeather {
    struct PlantWeather* volatile next;
    jmp_buf buf;
    volatile int raised;
    volatile int handled;
    volatile char* exc_type;
    volatile char* exc_msg;
    /* v0.48.38a — factory storm object (ARC-managed {type, message}
       MAP from plant_storm) carried by the frame during propagation;
       NULL for classic THROW type/msg storms. Ownership transfers
       frame-to-frame on rethrow and is released by the generated
       shelter dispatch after the handler body runs. */
    volatile tx_t exc_obj;
    /* v0.48.37d — exit-list teardown. Every WEATHER frame provisions a
       dedicated exit-list of protected resource handles. Handles are
       freed by plant_weather_leave on every exit path (ARC-aware, then
       plant_mem_free for plain allocations), deferred deallocations
       queued within the list are drained, and temporary objects
       registered while a SHELTER body runs are purged by
       plant_weather_shelter_leave immediately on handler exit. */
    tx_t  exit_list[PLANT_WEATHER_EXIT_MAX];
    unsigned char exit_deferred[PLANT_WEATHER_EXIT_MAX];
    int   exit_count;       /* registered handles in this frame */
    int   storm_handlers;   /* SHELTER clauses bound to this frame */
    int   shelter_mark;     /* exit_count snapshot at shelter entry; -1 outside */
} PlantWeather;

void        plant_weather_enter(PlantWeather* w, int storm_handlers);
void        plant_weather_leave(PlantWeather* w);
void        plant_calm(PlantWeather* w);
void        plant_throw(const char* type, const char* msg);
const char* plant_exc_type(void);
const char* plant_exc_msg(void);
/* v0.48.38a/b — storm() Exception Factory. plant_storm builds an
   ARC-managed exception object (a {type, message} MAP) that persists
   across setjmp/longjmp unwinding; plant_throw_obj raises such an
   object through the innermost checkpoint; plant_exc_val returns the
   binding value a SHELTER AS-clause receives (the object for factory
   storms, else the message string); plant_storm_release decrements
   the object's reference count once the handler logic has run.
   v0.48.38b — source-context injection: the factory additionally
   accepts file (tx_t), line and column (long) metadata that is
   conditionally packed into the object (file when non-NULL, line and
   column when > 0), so THROW storm(...). objects carry their compile
   site and SHELTER AS bindings expose e["file"] / e["line"] via
   _map_get. */
tx_t        plant_storm(tx_t type, tx_t msg, tx_t file, long line, long column);
void        plant_throw_obj(tx_t obj);
tx_t        plant_exc_val(void);
void        plant_storm_release(tx_t obj);
/* v0.48.38c — JOIN(list, delim) built-in: concatenates the list's
   elements into one string separated by delim ("" for NULL delim;
   "" result for empty/NULL lists; nested MAP/LIST elements serialize
   through the runtime object serializer). */
tx_t        plant_join(tx_t list, tx_t delim);
/* v0.48.38d — FIRST / LAST / SUM list operations: boundary-element
   extraction with "" for empty/NULL lists; SUM aggregates numeric
   elements (NUM/SCL arrive pre-converted as tx_t text), parses
   numeric strings with a full-consumption scan, and skips non-
   numeric elements (non-parsable strings, booleans, MAP/LIST). */
tx_t        plant_first(tx_t list);
tx_t        plant_last(tx_t list);
tx_t        plant_sum(tx_t list);
/* v0.48.38e — UPPER / LOWER string case operations: ASCII-safe
   conversion (characters cast to unsigned char before toupper/
   tolower); NULL and empty inputs yield "". */
tx_t        plant_upper(tx_t text);
tx_t        plant_lower(tx_t text);
/* v0.48.38e (extension) — TRIM / REVERSE string utilities: whitespace
   stripping (' ', '\t', '\n', '\r') from both boundaries, and full
   character reversal; NULL and empty inputs yield "". */
tx_t        plant_trim(tx_t text);
tx_t        plant_reverse(tx_t text);
/* v0.48.38f — math built-ins: tx_t operands coerce to double
   (raw small integers and numeric strings); integral results
   render as long integers, fractional with "%.10g". */
tx_t        plant_abs(tx_t x);
tx_t        plant_round(tx_t x);
tx_t        plant_pow(tx_t x, tx_t y);
tx_t        plant_ceil(tx_t x);
tx_t        plant_floor(tx_t x);
tx_t        plant_random(void);
tx_t        plant_sin(tx_t x);
tx_t        plant_cos(tx_t x);
tx_t        plant_sqrt(tx_t x);
/* v0.48.38g — conditional list built-ins: HAS checks element
   presence (canonicalized text comparison); ANY / ALL evaluate a
   runtime condition string ("<op> <num>") against each element. */
tx_t        plant_has(tx_t list, tx_t value);
tx_t        plant_any(tx_t list, tx_t cond);
tx_t        plant_all(tx_t list, tx_t cond);
/* v0.48.38h — ternary built-in: truthy conditions ("1", "TRUE",
   "true", nonzero raw int literals, any non-empty string except
   "0"/"false"/"FALSE") yield true_val; everything else yields
   false_val. Results canonicalize to text. */
tx_t        plant_pick(tx_t cond, tx_t true_val, tx_t false_val);
/* v0.48.38j — string analysis: FIND returns the 0-based index of
   the first substring occurrence ("0" for an empty sub, "-1" when
   text is empty or absent); COUNT_OF returns the number of
   non-overlapping occurrences ("0" when either argument is empty). */
tx_t        plant_find(tx_t text, tx_t sub);
tx_t        plant_count_of(tx_t text, tx_t sub);
/* v0.48.38i — universal slicing over strings and lists with
   negative-index resolution (length + index), half-open [start,
   end) bounds, clamping, and "not given" defaults (0 / length).
   List results canonicalize elements to text. */
tx_t        plant_slice(tx_t data, tx_t start, tx_t end);

/* ── v0.49.15: List built-ins (batch 1) ── */
tx_t        plant_list_reverse(tx_t data);  /* list → reversed copy; else plant_reverse (string) */
tx_t        plant_range_list(tx_t start, tx_t end); /* [start, end) integer list; e<=s → [] */
tx_t        plant_list_sort(tx_t data);     /* list → plant_sort(data, ""); else passthrough */
tx_t        plant_list_includes(tx_t data, tx_t item); /* list element scan; else string_includes */
tx_t        plant_list_index_of(tx_t data, tx_t item); /* first index or "-1"; non-list → "-1" */
tx_t        plant_list_unique(tx_t data);   /* first-occurrence dedupe copy; non-list passthrough */
tx_t        plant_list_average(tx_t data);  /* mean of numeric elements; empty → "0" */
tx_t        plant_list_median(tx_t data);   /* median of numeric elements; empty → "0" */

/* ── v0.49.16: List built-ins (batch 2) ── */
tx_t        plant_list_flatten(tx_t data);  /* single-level unnest of kind-0 sub-lists; else passthrough */
tx_t        plant_list_chunk(tx_t data, tx_t size);  /* sub-lists of max size; size<1 or empty → [] */
tx_t        plant_list_zip(tx_t left, tx_t right);   /* element-wise pairs, truncated to shorter; non-list → [] */
tx_t        plant_list_filter_gt(tx_t data, tx_t threshold); /* numeric elements strictly > threshold */
tx_t        plant_list_filter_lt(tx_t data, tx_t threshold); /* numeric elements strictly < threshold */
/* v0.48.38k — VEIN resource management: TAP opens a path ("r"/"w"/
   "a") returning a tagged handle (NULL on failure); ABSORB reads
   the full stream; INFUSE writes/appends returning "1"/"0"; SEAL
   closes and frees the handle returning "1"/"0". */
tx_t        plant_tap(tx_t path, tx_t mode);
tx_t        plant_absorb(tx_t vein);
tx_t        plant_infuse(tx_t vein, tx_t data);
tx_t        plant_seal(tx_t vein);
int         plant_storm_match(const char* thrown_type, const char* shelter_type);
int         plant_storm_is_known(const char* type);
const char* plant_storm_default_message(const char* type);
/* v0.48.37d — weather memory management and diagnostics */
int         plant_weather_register(PlantWeather* w, tx_t handle);
int         plant_weather_register_handle(tx_t handle); /* current frame */
int         plant_weather_defer_handle(tx_t handle);    /* queue within exit-list */
void        plant_weather_handling_begin(PlantWeather* w);
void        plant_weather_handling_end(PlantWeather* w);
void        plant_weather_shelter_enter(PlantWeather* w);
void        plant_weather_shelter_leave(PlantWeather* w);
tx_t        plant_weather_status(void);                 /* MAP telemetry */

/* ── v0.43.0: PlantArray type (dynamic string array for split results) ── */
#define PLANT_ARRAY_MAGIC 0x504C4152 /* "PLAR" */

typedef struct PlantArray {
    uint64_t magic;  /* PLANT_ARRAY_MAGIC for reliable type detection */
    int64_t  count;
    int64_t  capacity;
    char**   items;
    int8_t   kind;   /* 0 = LIST, 1 = MAP (metadata; set by constructors) */
} PlantArray;

PlantArray* plant_list_create(int64_t capacity);
void*       plant_list_get(PlantArray* list, int64_t index);
void        plant_list_set(PlantArray* list, int64_t index, void* value);
PlantArray* plant_list_push(PlantArray* list, void* value);
void*       plant_list_pop(PlantArray* list);   /* removes+returns last element; empty -> "" */
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
long        plant_lease_evict(void);              /* v0.48.37b: pressure-driven lease reclamation */
long        plant_persist_pressure(void);         /* v0.48.37b: memory pressure % (0-100+) */
tx_t        plant_dist_init(tx_t nodes);
tx_t        plant_dist_alloc(tx_t size, tx_t key);
tx_t        plant_dist_node(tx_t obj);
tx_t        plant_dist_release(tx_t obj);
tx_t        plant_dist_status(void);
tx_t        plant_safe_boundary_copy(tx_t chan, tx_t payload); /* copy/transfer enforcement */

/* ── v0.48.37c: True SAFE isolation — real worker processes ── */
void plant_safe_register(const char* name, tx_t (*fn)(int argc, tx_t* argv));
void plant_maybe_run_worker(void);   /* non-returning in --plant-worker mode */
tx_t plant_safe_call(const char* name, long argc, ...);

/* ── v0.48.37e: WAIT and LOCK Synchronization Primitives ── */
long plant_now_ms(void);             /* monotonic milliseconds */
tx_t plant_lock(tx_t key);           /* LOCK: register a locking flag */
tx_t plant_lock_release(tx_t key);   /* release a held lock */
tx_t plant_lock_held(tx_t key);      /* probe protection status */
tx_t plant_lock_status(void);        /* MAP: locked_count telemetry */

/* ═══════════════════════════════════════════════════════════════
   v0.49.59a — Abstract Runtime Interface (IRuntime)
   Context-driven execution contract. Standardizes execution,
   verification, error handling, and lifecycle management through
   function pointer structs bound to a shared execution context.
   Concrete runtimes (CLI, test harness, embedded) implement these
   pointers; callers interact through the abstract interface only.
   ═══════════════════════════════════════════════════════════════ */

typedef struct IRuntime IRuntime;
struct IRuntime {
    void* context;
    void (*execute)(void* ctx, const char* code);
    void (*verify)(void* ctx, const char* label, int condition);
    void (*verify_begin)(void* ctx);
    void (*verify_end)(void* ctx);
    void (*suite_setup)(void* ctx);
    void (*suite_teardown)(void* ctx);
    void (*error)(void* ctx, const char* msg);
    void (*warning)(void* ctx, const char* msg);
    void (*info)(void* ctx, const char* msg);
    void (*fatal)(void* ctx, const char* msg);
};

/* Default runtime: binds context to NULL, delegates to the global
   plant_error / plant_warning / plant_info / plant_fatal / plant_log
   implementations and the standard test-suite hooks. */
IRuntime* plant_runtime_default(void);
void      plant_runtime_free(IRuntime* rt);

/* Verify helpers that delegate through the IRuntime vtable */
void plant_runtime_verify(IRuntime* rt, const char* label, int condition);
void plant_runtime_verify_begin(IRuntime* rt);
void plant_runtime_verify_end(IRuntime* rt);

#endif
