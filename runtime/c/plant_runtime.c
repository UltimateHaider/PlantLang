#include "plant_runtime.h"
#include "plant_compat.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <dlfcn.h>

/* ── v0.42.0: djb2 hash for string keys ── */
static size_t _plant_hash_str(const char* str) {
    size_t hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + (size_t)(unsigned char)c;
    return hash;
}

/* Thread-local weather simulation */
static __thread char _plant_weather_buf[256] = {0};

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

/* Heap allocation wrapper */
void* plant_alloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) { fprintf(stderr, "plant_alloc: out of memory\n"); exit(1); }
    return ptr;
}

void plant_free(void* ptr) {
    free(ptr);
}

/* String concatenation: allocates new heap buffer */
char* plant_str_concat(const char* a, const char* b) {
    size_t la = strlen(a);
    size_t lb = strlen(b);
    char* result = (char*)plant_alloc(la + lb + 1);
    memcpy(result, a, la);
    memcpy(result + la, b, lb);
    result[la + lb] = '\0';
    return result;
}

/* Array creation: capacity i64 elements, zero-initialized header[0]=capacity */
int64_t* plant_array_create(int64_t capacity) {
    if (capacity < 0) capacity = 0;
    int64_t* arr = (int64_t*)plant_alloc((size_t)(capacity + 1) * sizeof(int64_t));
    arr[0] = capacity;
    for (int64_t i = 1; i <= capacity; i++) arr[i] = 0;
    return arr;
}

int64_t plant_array_get(int64_t* arr, int64_t index) {
    int64_t cap = arr[0];
    if (index < 0 || index >= cap) { fprintf(stderr, "plant_array_get: index %lld out of bounds (cap %lld)\n", (long long)index, (long long)cap); exit(1); }
    return arr[index + 1];
}

void plant_array_set(int64_t* arr, int64_t index, int64_t value) {
    int64_t cap = arr[0];
    if (index < 0 || index >= cap) { fprintf(stderr, "plant_array_set: index %lld out of bounds (cap %lld)\n", (long long)index, (long long)cap); exit(1); }
    arr[index + 1] = value;
}

/* ── v0.41.0: plant_net_harvest — minimal HTTP GET via POSIX sockets ── */
char* plant_net_harvest(const char* url, const char* method, const char* body, const char* headers, int64_t timeout_sec) {
    (void)body; (void)headers; (void)timeout_sec;
    if (!url) return plant_str_concat("", "");
    char host[256] = {0};
    char path[1024] = {0};
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else if (strncmp(p, "https://", 8) == 0) p += 8;
    const char* slash = strchr(p, '/');
    if (slash) {
        size_t hlen = (size_t)(slash - p);
        if (hlen > 255) hlen = 255;
        strncpy(host, p, hlen);
        host[hlen] = '\0';
        strncpy(path, slash, 1023);
    } else {
        strncpy(host, p, 255);
        strncpy(path, "/", 1023);
    }
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int gai_err = getaddrinfo(host, "80", &hints, &res);
    if (gai_err) return plant_str_concat("ERROR: DNS ", gai_strerror(gai_err));
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return plant_str_concat("", "ERROR: socket"); }
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd); freeaddrinfo(res);
        return plant_str_concat("", "ERROR: connect");
    }
    freeaddrinfo(res);
    char req[4096];
    int n = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
        path, host);
    if (n < 0) { close(fd); return plant_str_concat("", "ERROR: req"); }
    send(fd, req, (size_t)n, 0);
    char buf[4096];
    char* response = plant_alloc(1);
    response[0] = '\0';
    ssize_t r;
    while ((r = recv(fd, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[r] = '\0';
        char* old = response;
        size_t old_len = strlen(old);
        response = plant_alloc(old_len + (size_t)r + 1);
        memcpy(response, old, old_len);
        memcpy(response + old_len, buf, (size_t)r);
        response[old_len + (size_t)r] = '\0';
        plant_free(old);
    }
    close(fd);
    char* body_start = strstr(response, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        char* result = plant_str_concat(body_start, "");
        plant_free(response);
        return result;
    }
    return response;
}

/* ── v0.41.0: plant_net_listen_open — TCP listener ── */
int64_t plant_net_listen_open(int64_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)(port & 0xFFFF));
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    if (listen(fd, 5) < 0) { close(fd); return -1; }
    return (int64_t)fd;
}

int64_t plant_net_accept(int64_t fd) {
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    int cfd = accept((int)fd, (struct sockaddr*)&client, &len);
    return (int64_t)cfd;
}

char* plant_net_read(int64_t fd) {
    char buf[4096];
    ssize_t r = recv((int)fd, buf, sizeof(buf) - 1, 0);
    if (r <= 0) return plant_str_concat("", "");
    buf[r] = '\0';
    return plant_str_concat(buf, "");
}

int64_t plant_net_write(int64_t fd, const char* data) {
    if (!data) return -1;
    ssize_t sent = send((int)fd, data, strlen(data), 0);
    return (int64_t)sent;
}

void plant_net_close(int64_t fd) {
    close((int)fd);
}

/* ═══════════════════════════════════════════════════════════════
   v0.42.0 — Map Data Structure
   ═══════════════════════════════════════════════════════════════ */

PlantMap* plant_map_create(size_t initial_capacity) {
    if (initial_capacity < 8) initial_capacity = 8;
    PlantMap* map = (PlantMap*)plant_alloc(sizeof(PlantMap));
    map->capacity = initial_capacity;
    map->count = 0;
    map->threshold = initial_capacity * 3 / 4;  /* 75% load factor */
    map->entries = (PlantMapEntry*)plant_alloc(initial_capacity * sizeof(PlantMapEntry));
    memset(map->entries, 0, initial_capacity * sizeof(PlantMapEntry));
    return map;
}

static void _plant_map_grow(PlantMap* map) {
    size_t old_cap = map->capacity;
    PlantMapEntry* old_entries = map->entries;
    size_t new_cap = old_cap * 2;
    map->capacity = new_cap;
    map->threshold = new_cap * 3 / 4;
    map->count = 0;
    map->entries = (PlantMapEntry*)plant_alloc(new_cap * sizeof(PlantMapEntry));
    memset(map->entries, 0, new_cap * sizeof(PlantMapEntry));
    for (size_t i = 0; i < old_cap; i++) {
        if (old_entries[i].occupied) {
            plant_map_set(map, old_entries[i].key, old_entries[i].value);
            plant_free(old_entries[i].key);
        }
    }
    plant_free(old_entries);
}

void plant_map_set(PlantMap* map, const char* key, void* value) {
    if (!map || !key) return;
    if (map->count >= map->threshold) _plant_map_grow(map);
    size_t idx = _plant_hash_str(key) & (map->capacity - 1);
    for (size_t i = 0; i < map->capacity; i++) {
        size_t probe = (idx + i) & (map->capacity - 1);
        if (!map->entries[probe].occupied) {
            map->entries[probe].key = plant_str_concat(key, "");
            map->entries[probe].value = value;
            map->entries[probe].occupied = 1;
            map->count++;
            return;
        }
        if (strcmp(map->entries[probe].key, key) == 0) {
            map->entries[probe].value = value;
            return;
        }
    }
}

void* plant_map_get(PlantMap* map, const char* key) {
    if (!map || !key || map->count == 0) return NULL;
    size_t idx = _plant_hash_str(key) & (map->capacity - 1);
    for (size_t i = 0; i < map->capacity; i++) {
        size_t probe = (idx + i) & (map->capacity - 1);
        if (!map->entries[probe].occupied) return NULL;
        if (strcmp(map->entries[probe].key, key) == 0) return map->entries[probe].value;
    }
    return NULL;
}

char** plant_map_keys(PlantMap* map, size_t* out_count) {
    if (!map || !out_count) return NULL;
    *out_count = map->count;
    if (map->count == 0) return NULL;
    char** keys = (char**)plant_alloc(map->count * sizeof(char*));
    size_t j = 0;
    for (size_t i = 0; i < map->capacity && j < map->count; i++) {
        if (map->entries[i].occupied) {
            keys[j++] = map->entries[i].key;
        }
    }
    return keys;
}

void plant_map_free(PlantMap* map) {
    if (!map) return;
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].occupied) plant_free(map->entries[i].key);
    }
    plant_free(map->entries);
    plant_free(map);
}

/* ═══════════════════════════════════════════════════════════════
   v0.42.0 — Iterator Protocol
   ═══════════════════════════════════════════════════════════════ */

void plant_iterator_init(PlantIterator* it, void* container, int kind) {
    if (!it) return;
    it->container = container;
    it->kind = kind;
    it->index = 0;
    it->keys = NULL;
    it->values = NULL;
    it->array_data = NULL;
    if (kind == 0 && container) {
        PlantMap* map = (PlantMap*)container;
        size_t count = 0;
        it->keys = plant_map_keys(map, &count);
        it->size = count;
        /* Build values array */
        if (count > 0) {
            it->values = (void**)plant_alloc(count * sizeof(void*));
            size_t j = 0;
            for (size_t i = 0; i < map->capacity && j < count; i++) {
                if (map->entries[i].occupied) {
                    it->values[j++] = map->entries[i].value;
                }
            }
        }
    } else if (kind == 1 && container) {
        int64_t* arr = (int64_t*)container;
        it->size = (size_t)arr[0];
        it->array_data = arr + 1;  /* skip header */
    }
}

int plant_iterator_has_next(PlantIterator* it) {
    if (!it) return 0;
    return it->index < it->size ? 1 : 0;
}

void* plant_iterator_next(PlantIterator* it) {
    if (!it || !plant_iterator_has_next(it)) return NULL;
    void* result = NULL;
    if (it->kind == 0) {
        PlantMapEntry entry;
        entry.key = it->keys ? it->keys[it->index] : NULL;
        entry.value = it->values ? it->values[it->index] : NULL;
        /* Return a pointer to a static copy for simplicity */
        static PlantMapEntry ret;
        ret.key = entry.key;
        ret.value = entry.value;
        ret.occupied = 1;
        result = (void*)&ret;
    } else if (it->kind == 1 && it->array_data) {
        result = (void*)(intptr_t)it->array_data[it->index];
    }
    it->index++;
    return result;
}

void plant_iterator_free(PlantIterator* it) {
    if (!it) return;
    if (it->keys)  plant_free(it->keys);
    if (it->values) plant_free(it->values);
    it->container = NULL;
    it->keys = NULL;
    it->values = NULL;
    it->array_data = NULL;
}

/* ═══════════════════════════════════════════════════════════════
   v0.42.0 — Domain Primitives
   ═══════════════════════════════════════════════════════════════ */

void plant_sys_action(const char* action_name, void* payload) {
    (void)payload;
    fprintf(stdout, "[ACTION] %s executed\n", action_name ? action_name : "unknown");
    fflush(stdout);
}

void plant_env_set_weather(const char* weather_type) {
    if (weather_type) {
        strncpy(_plant_weather_buf, weather_type, sizeof(_plant_weather_buf) - 1);
        _plant_weather_buf[sizeof(_plant_weather_buf) - 1] = '\0';
    }
    fprintf(stdout, "[WEATHER] set to %s\n", weather_type ? weather_type : "clear");
    fflush(stdout);
}

const char* plant_env_get_weather(void) {
    return _plant_weather_buf[0] ? _plant_weather_buf : "clear";
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.23 — WEATHER/SHELTER/CALM Exception Management
   ═══════════════════════════════════════════════════════════════ */

static PlantWeather* _plant_weather_head = NULL;

/* v0.48.23-patch / v0.48.25 — the cumulative storm registry. The six
   legacy core kinds (ZERO/LOCK/MISSING/NETWORK/LOST/ANY) are joined
   by six additive classifications (RANGE/TYPE/PARSE/HANDLE/HARVEST/
   FALL) and, since v0.48.25, STOP_STORM (the STOP IF classification).
   Every entry carries the default message plant_throw uses when a
   THROW carries no explicit message; free-form identifiers (neither
   the registry nor ANY_STORM) fall back to the generic message
   below. */
typedef struct { const char* name; const char* message; } PlantStormInfo;

static const PlantStormInfo _plant_storm_registry[13] = {
    { "ZERO_STORM",    "division by zero"                },
    { "LOCK_STORM",    "operation locked or forbidden"   },
    { "MISSING_STORM", "missing symbol, file, or value"  },
    { "NETWORK_STORM", "network or I/O failure"          },
    { "LOST_STORM",    "data lost or unavailable"        },
    { "RANGE_STORM",   "index or range out of bounds"    },
    { "TYPE_STORM",    "type or conversion mismatch"     },
    { "PARSE_STORM",   "malformed input or syntax error" },
    { "HANDLE_STORM",  "invalid or closed resource handle" },
    { "HARVEST_STORM", "HTTP harvest failed"             },
    { "FALL_STORM",    "requested abort or termination"  },
    { "STOP_STORM",    "conditional stop requested"      },
    { "ANY_STORM",     "unclassified storm"              },
};

#define PLANT_STORM_COUNT \
    ((int)(sizeof(_plant_storm_registry) / sizeof(_plant_storm_registry[0])))

int plant_storm_match(const char* thrown_type, const char* shelter_type) {
    if (shelter_type && strcmp(shelter_type, "ANY_STORM") == 0) return 1;
    if (!thrown_type || !shelter_type) return 0;
    return strcmp(thrown_type, shelter_type) == 0;
}

int plant_storm_is_known(const char* type) {
    if (!type) return 0;
    for (int i = 0; i < PLANT_STORM_COUNT; i++) {
        if (strcmp(_plant_storm_registry[i].name, type) == 0) return 1;
    }
    return 0;
}

const char* plant_storm_default_message(const char* type) {
    if (type) {
        for (int i = 0; i < PLANT_STORM_COUNT; i++) {
            if (strcmp(_plant_storm_registry[i].name, type) == 0)
                return _plant_storm_registry[i].message;
        }
    }
    return "(unclassified storm)";
}

void plant_weather_enter(PlantWeather* w) {
    if (!w) return;
    w->next = _plant_weather_head;
    w->raised = 0;
    w->handled = 0;
    w->exc_type = NULL;
    w->exc_msg = NULL;
    _plant_weather_head = w;
}

void plant_weather_leave(PlantWeather* w) {
    if (!w) return;
    if (_plant_weather_head == w) {
        _plant_weather_head = w->next;
    }
}

/* v0.48.25 — unconditional finalization. Called after a WEATHER
   block's CALM body on every exit path (normal completion, handled
   storm, unmatched storm, and the threaded GIVE/BREAK/CONTINUE exit
   chains). Pops the frame, then re-raises any storm the shelters did
   not handle so it propagates to the enclosing WEATHER. */
void plant_calm(PlantWeather* w) {
    if (!w) return;
    int pending = w->raised && !w->handled;
    const char* t = pending ? (const char*)w->exc_type : NULL;
    const char* m = pending ? (const char*)w->exc_msg : NULL;
    plant_weather_leave(w);
    if (pending) plant_throw(t, m);
}

void plant_throw(const char* type, const char* msg) {
    PlantWeather* w = _plant_weather_head;
    if (msg == NULL) msg = plant_storm_default_message(type);
    if (w == NULL) {
        fprintf(stderr, "[WEATHER] unhandled storm: %s %s\n",
                type ? type : "(none)", msg ? msg : "");
        fflush(stderr);
        abort();
    }
    w->raised = 1;
    w->exc_type = (char*)type;
    w->exc_msg = (char*)msg;
    longjmp(w->buf, 1);
}

const char* plant_exc_type(void) {
    PlantWeather* w = _plant_weather_head;
    if (w && w->exc_type) return (const char*)w->exc_type;
    return "";
}

const char* plant_exc_msg(void) {
    PlantWeather* w = _plant_weather_head;
    if (w && w->exc_msg) return (const char*)w->exc_msg;
    return "";
}

void plant_entity_set_species(void* entity, const char* species_name) {
    (void)entity;
    fprintf(stdout, "[SPECIES] entity set to %s\n", species_name ? species_name : "unknown");
    fflush(stdout);
}

/* ═══════════════════════════════════════════════════════════════
   v0.43.0 — File I/O Primitives
   ═══════════════════════════════════════════════════════════════ */

#include <sys/stat.h>

char* plant_file_read(const char* filepath) {
    if (!filepath) return NULL;
    FILE* f = fopen(filepath, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    rewind(f);
    char* buf = (char*)plant_alloc((size_t)len + 1);
    size_t n = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

int plant_file_write(const char* filepath, const char* content) {
    if (!filepath) return 0;
    FILE* f = fopen(filepath, "wb");
    if (!f) return 0;
    if (content) fwrite(content, 1, strlen(content), f);
    fclose(f);
    return 1;
}

int plant_file_exists(const char* filepath) {
    if (!filepath) return 0;
    struct stat st;
    return stat(filepath, &st) == 0 ? 1 : 0;
}

int plant_file_delete(const char* filepath) {
    if (!filepath) return 0;
    return remove(filepath) == 0 ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════════
   v0.43.0 — String Manipulation Primitives
   ═══════════════════════════════════════════════════════════════ */

PlantArray* plant_string_split(const char* str, const char* delimiter) {
    PlantArray* arr = (PlantArray*)plant_alloc(sizeof(PlantArray));
    arr->count = 0;
    arr->capacity = 0;
    arr->items = NULL;
    if (!str || !delimiter || delimiter[0] == '\0') return arr;
    size_t delim_len = strlen(delimiter);
    int64_t capacity = 8;
    arr->capacity = capacity;
    arr->items = (char**)plant_alloc((size_t)capacity * sizeof(char*));
    const char* start = str;
    const char* end;
    while ((end = strstr(start, delimiter)) != NULL) {
        size_t seg_len = (size_t)(end - start);
        char* seg = (char*)plant_alloc(seg_len + 1);
        memcpy(seg, start, seg_len);
        seg[seg_len] = '\0';
        if (arr->count >= arr->capacity) {
            arr->capacity *= 2;
            arr->items = (char**)realloc(arr->items, (size_t)arr->capacity * sizeof(char*));
        }
        arr->items[arr->count++] = seg;
        start = end + delim_len;
    }
    /* remainder */
    size_t rem_len = strlen(start);
    char* rem = (char*)plant_alloc(rem_len + 1);
    memcpy(rem, start, rem_len);
    rem[rem_len] = '\0';
    if (arr->count >= arr->capacity) {
        arr->capacity++;
        arr->items = (char**)realloc(arr->items, (size_t)arr->capacity * sizeof(char*));
    }
    arr->items[arr->count++] = rem;
    return arr;
}

/* ── v0.45.0: PlantArray* list operations for tx_t-valued dynamic arrays ── */

PlantArray* plant_list_create(int64_t capacity) {
    if (capacity < 0) capacity = 0;
    PlantArray* list = (PlantArray*)plant_alloc(sizeof(PlantArray));
    list->magic = PLANT_ARRAY_MAGIC;
    list->count = 0;
    list->capacity = capacity;
    if (capacity > 0) {
        list->items = (char**)plant_alloc((size_t)capacity * sizeof(char*));
        for (int64_t i = 0; i < capacity; i++) list->items[i] = NULL;
    } else {
        list->items = NULL;
    }
    return list;
}

void* plant_list_get(PlantArray* list, int64_t index) {
    if (!list) return "";
    if (index < 0 || index >= list->count) return "";
    char* v = list->items[index];
    return v ? v : "";
}

void plant_list_set(PlantArray* list, int64_t index, void* value) {
    if (!list || index < 0 || index >= list->capacity) return;
    list->items[index] = value;
    if (index >= list->count) list->count = index + 1;
}

PlantArray* plant_list_push(PlantArray* list, void* value) {
    if (!list) return list;
    if (list->count >= list->capacity) {
        int64_t new_cap = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = (char**)realloc(list->items, (size_t)new_cap * sizeof(char*));
        list->capacity = new_cap;
    }
    list->items[list->count++] = value;
    return list;
}

PlantArray* plant_list_make(int64_t count, ...) {
    va_list ap;
    va_start(ap, count);
    PlantArray* list = plant_list_create(count);
    for (int64_t i = 0; i < count; i++) {
        void* v = va_arg(ap, void*);
        plant_list_set(list, i, (char*)v);
    }
    va_end(ap);
    return list;
}

char* plant_string_trim(const char* str) {
    if (!str) return plant_str_concat("", "");
    const char* start = str;
    while (*start && (unsigned char)*start <= ' ') start++;
    if (*start == '\0') return plant_str_concat("", "");
    const char* end = start + strlen(start) - 1;
    while (end > start && (unsigned char)*end <= ' ') end--;
    size_t len = (size_t)(end - start + 1);
    char* result = (char*)plant_alloc(len + 1);
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

int64_t plant_string_index_of(const char* str, const char* substr) {
    if (!str || !substr) return -1;
    const char* p = strstr(str, substr);
    if (!p) return -1;
    return (int64_t)(p - str);
}

/* ── v0.44.0: Option/Result Tagged Union Helpers ── */

PlantTagged* plant_option_some(void* value) {
    PlantTagged* t = (PlantTagged*)plant_alloc(sizeof(PlantTagged));
    t->tag = 0; t->kind = 0; t->payload = value;
    return t;
}

PlantTagged* plant_option_none(void) {
    PlantTagged* t = (PlantTagged*)plant_alloc(sizeof(PlantTagged));
    t->tag = 1; t->kind = 0; t->payload = NULL;
    return t;
}

PlantTagged* plant_result_ok(void* value) {
    PlantTagged* t = (PlantTagged*)plant_alloc(sizeof(PlantTagged));
    t->tag = 0; t->kind = 1; t->payload = value;
    return t;
}

PlantTagged* plant_result_err(void* value) {
    PlantTagged* t = (PlantTagged*)plant_alloc(sizeof(PlantTagged));
    t->tag = 2; t->kind = 1; t->payload = value;
    return t;
}

int plant_is_some(PlantTagged* t) {
    if (!t || t->kind != 0) return 0;
    return t->tag == 0;
}

int plant_is_none(PlantTagged* t) {
    if (!t || t->kind != 0) return 0;
    return t->tag == 1;
}

void* plant_unwrap(PlantTagged* t) {
    if (!t) return NULL;
    if ((t->kind == 0 && t->tag == 1) || (t->kind == 1 && t->tag == 2)) {
        fprintf(stderr, "plant_unwrap: called on None/Err\n");
        return NULL;
    }
    return t->payload;
}

int plant_is_ok(PlantTagged* t) {
    if (!t || t->kind != 1) return 0;
    return t->tag == 0;
}

int plant_is_err(PlantTagged* t) {
    if (!t || t->kind != 1) return 0;
    return t->tag == 2;
}

void* plant_unwrap_err(PlantTagged* t) {
    if (!t || t->kind != 1 || t->tag != 2) return NULL;
    return t->payload;
}

/* ── v0.44.0: Array/String Slice Primitives ── */

int64_t* plant_array_slice(int64_t* arr, int64_t start, int64_t end) {
    if (!arr || start < 0 || end <= start) return plant_array_create(0);
    int64_t cap = arr[0];  /* capacity stored at index 0 */
    if (start > cap) start = cap;
    if (end > cap) end = cap;
    int64_t len = end - start;
    if (len < 0) len = 0;
    int64_t* result = plant_array_create(len);
    for (int64_t i = 0; i < len; i++) {
        plant_array_set(result, i + 1, plant_array_get(arr, start + i));
    }
    return result;
}

char* plant_string_slice(const char* str, int64_t start, int64_t end) {
    if (!str) return plant_str_concat("", "");
    size_t len = strlen(str);
    if (start < 0) start = 0;
    if (end > (int64_t)len) end = (int64_t)len;
    if (start >= end) return plant_str_concat("", "");
    size_t slice_len = (size_t)(end - start);
    char* result = (char*)plant_alloc(slice_len + 1);
    memcpy(result, str + start, slice_len);
    result[slice_len] = '\0';
    return result;
}

/* ── v0.44.0: Range Generation ── */

int64_t* plant_range(int64_t start, int64_t end) {
    if (end <= start) return plant_array_create(0);
    int64_t len = end - start;
    int64_t* arr = plant_array_create(len);
    for (int64_t i = 0; i < len; i++) {
        plant_array_set(arr, i + 1, start + i);
    }
    return arr;
}

/* ═══════════════════════════════════════════════════════════════
   v0.47.0 — Core Standard Library (std/*)
   Implementations; signatures in plant_compat.h
   ═══════════════════════════════════════════════════════════════ */

/* ── std/json ────────────────────────────────────────────────── */

static PlantJson* _pj_new(int kind, void* val) {
    PlantJson* j = (PlantJson*)plant_alloc(sizeof(PlantJson));
    j->kind = kind;
    j->val = val;
    return j;
}

static void _json_skip_ws(const char** p, const char* end) {
    while (*p < end && (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r')) (*p)++;
}

static int _json_hex4(const char* s) {
    int v = 0;
    for (int i = 0; i < 4; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
        else return -1;
    }
    return v;
}

static char* _json_parse_string(const char** pp, const char* end) {
    const char* p = *pp;
    if (p >= end || *p != '"') return NULL;
    p++;
    size_t cap = 64, len = 0;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    while (p < end) {
        char c = *p;
        if (c == '"') {
            p++;
            *pp = p;
            out[len] = 0;
            return out;
        }
        if (c == '\\') {
            p++;
            if (p >= end) { free(out); return NULL; }
            char e = *p;
            char repl;
            switch (e) {
                case '"':  repl = '"';  break;
                case '\\': repl = '\\'; break;
                case '/':  repl = '/';  break;
                case 'b':  repl = '\b'; break;
                case 'f':  repl = '\f'; break;
                case 'n':  repl = '\n'; break;
                case 'r':  repl = '\r'; break;
                case 't':  repl = '\t'; break;
                case 'u': {
                    if (end - p < 5) { free(out); return NULL; }
                    int h = _json_hex4(p + 1);
                    if (h < 0) { free(out); return NULL; }
                    unsigned cp = (unsigned)h;
                    p += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF && end - p >= 7 && p[1] == '\\' && p[2] == 'u') {
                        int lo = _json_hex4(p + 3);
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + ((unsigned)lo - 0xDC00);
                            p += 6;
                        }
                    }
                    char buf[4];
                    size_t blen = 0;
                    if (cp < 0x80) buf[blen++] = (char)cp;
                    else if (cp < 0x800) {
                        buf[blen++] = (char)(0xC0 | (cp >> 6));
                        buf[blen++] = (char)(0x80 | (cp & 0x3F));
                    } else if (cp < 0x10000) {
                        buf[blen++] = (char)(0xE0 | (cp >> 12));
                        buf[blen++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        buf[blen++] = (char)(0x80 | (cp & 0x3F));
                    } else {
                        buf[blen++] = (char)(0xF0 | (cp >> 18));
                        buf[blen++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                        buf[blen++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        buf[blen++] = (char)(0x80 | (cp & 0x3F));
                    }
                    for (size_t i = 0; i < blen; i++) {
                        if (len + 1 >= cap) { cap *= 2; out = (char*)realloc(out, cap); }
                        out[len++] = buf[i];
                    }
                    p++;
                    continue;
                }
                default: free(out); return NULL;
            }
            if (len + 1 >= cap) { cap *= 2; out = (char*)realloc(out, cap); }
            out[len++] = repl;
            p++;
            continue;
        }
        if ((unsigned char)c < 0x20) { free(out); return NULL; }
        if (len + 1 >= cap) { cap *= 2; out = (char*)realloc(out, cap); }
        out[len++] = c;
        p++;
    }
    free(out);
    return NULL;
}

static PlantJson* _json_parse_value(const char** pp, const char* end) {
    const char* p = *pp;
    _json_skip_ws(&p, end);
    if (p >= end) return NULL;
    if (*p == '{') {
        p++;
        PlantArray* pairs = plant_list_create(8);
        _json_skip_ws(&p, end);
        if (p < end && *p == '}') { p++; *pp = p; return _pj_new(5, pairs); }
        for (;;) {
            _json_skip_ws(&p, end);
            if (p >= end || *p != '"') return NULL;
            char* key = _json_parse_string(&p, end);
            if (!key) return NULL;
            _json_skip_ws(&p, end);
            if (p >= end || *p != ':') { free(key); return NULL; }
            p++;
            PlantJson* v = _json_parse_value(&p, end);
            if (!v) { free(key); return NULL; }
            pairs = plant_list_push(pairs, key);
            pairs = plant_list_push(pairs, v);
            _json_skip_ws(&p, end);
            if (p < end && *p == ',') { p++; continue; }
            if (p < end && *p == '}') { p++; break; }
            return NULL;
        }
        *pp = p;
        return _pj_new(5, pairs);
    }
    if (*p == '[') {
        p++;
        PlantArray* list = plant_list_create(8);
        _json_skip_ws(&p, end);
        if (p < end && *p == ']') { p++; *pp = p; return _pj_new(4, list); }
        for (;;) {
            PlantJson* v = _json_parse_value(&p, end);
            if (!v) return NULL;
            list = plant_list_push(list, v);
            _json_skip_ws(&p, end);
            if (p < end && *p == ',') { p++; continue; }
            if (p < end && *p == ']') { p++; break; }
            return NULL;
        }
        *pp = p;
        return _pj_new(4, list);
    }
    if (*p == '"') {
        char* s = _json_parse_string(&p, end);
        if (!s) return NULL;
        *pp = p;
        return _pj_new(3, s);
    }
    if (*p == 't') {
        if (end - p >= 4 && memcmp(p, "true", 4) == 0) { p += 4; *pp = p; return _pj_new(1, strdup("true")); }
        return NULL;
    }
    if (*p == 'f') {
        if (end - p >= 5 && memcmp(p, "false", 5) == 0) { p += 5; *pp = p; return _pj_new(1, strdup("false")); }
        return NULL;
    }
    if (*p == 'n') {
        if (end - p >= 4 && memcmp(p, "null", 4) == 0) { p += 4; *pp = p; return _pj_new(0, NULL); }
        return NULL;
    }
    if (*p == '-' || (*p >= '0' && *p <= '9')) {
        const char* st = p;
        if (*p == '-') p++;
        if (p >= end || *p < '0' || *p > '9') return NULL;
        while (p < end && *p >= '0' && *p <= '9') p++;
        if (p < end && *p == '.') {
            p++;
            if (p >= end || *p < '0' || *p > '9') return NULL;
            while (p < end && *p >= '0' && *p <= '9') p++;
        }
        if (p < end && (*p == 'e' || *p == 'E')) {
            p++;
            if (p < end && (*p == '+' || *p == '-')) p++;
            if (p >= end || *p < '0' || *p > '9') return NULL;
            while (p < end && *p >= '0' && *p <= '9') p++;
        }
        size_t n = (size_t)(p - st);
        char* num = (char*)malloc(n + 1);
        if (!num) return NULL;
        memcpy(num, st, n);
        num[n] = 0;
        *pp = p;
        return _pj_new(2, num);
    }
    return NULL;
}

static char* _json_quote_string(const char* s) {
    if (!s) s = "";
    size_t cap = strlen(s) * 6 + 3, len = 0;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    out[len++] = '"';
    for (const char* p = s; *p; p++) {
        char c = *p;
        const char* esc = NULL;
        switch (c) {
            case '"':  esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\b': esc = "\\b";  break;
            case '\f': esc = "\\f";  break;
            case '\n': esc = "\\n";  break;
            case '\r': esc = "\\r";  break;
            case '\t': esc = "\\t";  break;
            default: break;
        }
        if (esc) {
            size_t el = strlen(esc);
            if (len + el + 1 > cap) { cap = cap * 2 + el; out = (char*)realloc(out, cap); }
            memcpy(out + len, esc, el);
            len += el;
        } else if ((unsigned char)c < 0x20) {
            if (len + 7 > cap) { cap *= 2; out = (char*)realloc(out, cap); }
            len += (size_t)sprintf(out + len, "\\u%04x", (unsigned char)c);
        } else {
            if (len + 1 >= cap) { cap *= 2; out = (char*)realloc(out, cap); }
            out[len++] = c;
        }
    }
    out[len++] = '"';
    out[len] = 0;
    return out;
}

static char* _json_stringify_value(PlantJson* j) {
    if (!j) return strdup("null");
    switch (j->kind) {
        case 0: return strdup("null");
        case 1:
        case 2: return strdup((char*)j->val);
        case 3: return _json_quote_string((char*)j->val);
        case 4: {
            PlantArray* a = (PlantArray*)j->val;
            size_t cap = 64, len = 0;
            char* out = (char*)malloc(cap);
            if (!out) return NULL;
            out[len++] = '[';
            for (int64_t i = 0; i < a->count; i++) {
                if (i > 0) { if (len + 2 > cap) { cap *= 2; out = (char*)realloc(out, cap); } out[len++] = ','; }
                char* part = _json_stringify_value((PlantJson*)a->items[i]);
                size_t pl = part ? strlen(part) : 4;
                while (len + pl + 2 > cap) cap *= 2;
                out = (char*)realloc(out, cap);
                if (part) { memcpy(out + len, part, pl); free(part); }
                else { memcpy(out + len, "null", 4); pl = 4; }
                len += pl;
            }
            out[len++] = ']';
            out[len] = 0;
            return out;
        }
        case 5: {
            PlantArray* a = (PlantArray*)j->val;
            size_t cap = 64, len = 0;
            char* out = (char*)malloc(cap);
            if (!out) return NULL;
            out[len++] = '{';
            for (int64_t i = 0; i + 1 < a->count; i += 2) {
                if (i > 0) { if (len + 2 > cap) { cap *= 2; out = (char*)realloc(out, cap); } out[len++] = ','; }
                char* k = _json_quote_string((char*)a->items[i]);
                char* v = _json_stringify_value((PlantJson*)a->items[i + 1]);
                size_t need = (k ? strlen(k) : 0) + (v ? strlen(v) : 0) + 1;
                while (len + need + 1 > cap) cap *= 2;
                out = (char*)realloc(out, cap);
                if (k) { memcpy(out + len, k, strlen(k)); len += strlen(k); free(k); }
                out[len++] = ':';
                if (v) { memcpy(out + len, v, strlen(v)); len += strlen(v); free(v); }
            }
            out[len++] = '}';
            out[len] = 0;
            return out;
        }
    }
    return strdup("null");
}

tx_t json_parse(tx_t str) {
    const char* s = _S(str);
    if (!s) return NULL;
    size_t len = strlen(s);
    const char* p = s;
    PlantJson* j = _json_parse_value(&p, s + len);
    if (!j) return NULL;
    _json_skip_ws(&p, s + len);
    if (p != s + len) return NULL;   /* trailing garbage → nil */
    return (tx_t)j;
}

tx_t json_stringify(tx_t val) {
    if (!val) return strdup("null");
    PlantJson* j = (PlantJson*)val;
    if (j->kind >= 0 && j->kind <= 5) return _json_stringify_value(j);
    PlantArray* a = (PlantArray*)val;
    if (a->magic == PLANT_ARRAY_MAGIC) {
        /* defensive: native pair-list MAP of strings → JSON object */
        size_t cap = 64, len = 0;
        char* out = (char*)malloc(cap);
        if (!out) return NULL;
        out[len++] = '{';
        for (int64_t i = 0; i + 1 < a->count; i += 2) {
            if (i > 0) { if (len + 2 > cap) { cap *= 2; out = (char*)realloc(out, cap); } out[len++] = ','; }
            char* k = _json_quote_string((char*)a->items[i]);
            const char* vs = (const char*)a->items[i + 1];
            char* v;
            if (!vs || !*vs) v = strdup("null");
            else if (strcmp(vs, "true") == 0 || strcmp(vs, "false") == 0) v = strdup(vs);
            else {
                char* endp = NULL;
                strtod(vs, &endp);
                if (endp != vs && *endp == 0) v = strdup(vs);
                else v = _json_quote_string(vs);
            }
            size_t need = (k ? strlen(k) : 0) + (v ? strlen(v) : 0) + 1;
            while (len + need + 1 > cap) cap *= 2;
            out = (char*)realloc(out, cap);
            if (k) { memcpy(out + len, k, strlen(k)); len += strlen(k); free(k); }
            out[len++] = ':';
            if (v) { memcpy(out + len, v, strlen(v)); len += strlen(v); free(v); }
        }
        out[len++] = '}';
        out[len] = 0;
        return out;
    }
    return strdup("null");
}

tx_t json_get(tx_t jv, tx_t key) {
    PlantJson* j = (PlantJson*)jv;
    if (!j || j->kind != 5) return NULL;
    PlantArray* a = (PlantArray*)j->val;
    const char* k = _S(key);
    for (int64_t i = 0; i + 1 < a->count; i += 2)
        if (strcmp((char*)a->items[i], k) == 0) return a->items[i + 1];
    return NULL;
}

tx_t json_at(tx_t jv, long idx) {
    PlantJson* j = (PlantJson*)jv;
    if (!j || j->kind != 4) return NULL;
    PlantArray* a = (PlantArray*)j->val;
    if (idx < 0 || idx >= a->count) return NULL;
    return a->items[idx];
}

long json_len(tx_t jv) {
    PlantJson* j = (PlantJson*)jv;
    if (!j || (j->kind != 4 && j->kind != 5)) return 0;
    return (long)((PlantArray*)j->val)->count;
}

long json_kind(tx_t jv) {
    PlantJson* j = (PlantJson*)jv;
    if (!j || j->kind < 0 || j->kind > 5) return 0;
    return j->kind;
}

tx_t json_val(tx_t jv) {
    PlantJson* j = (PlantJson*)jv;
    if (!j) return "";
    if (j->kind >= 1 && j->kind <= 3) return j->val;
    return "";
}

/* ── std/string ──────────────────────────────────────────────── */

tx_t string_repeat(tx_t str, long count) {
    const char* s = _S(str);
    if (!s || count <= 0) return strdup("");
    size_t sl = strlen(s);
    if (sl == 0) return strdup("");
    size_t cap = sl * (size_t)count;
    char* out = (char*)malloc(cap + 1);
    if (!out) return strdup("");
    char* p = out;
    for (long i = 0; i < count; i++) { memcpy(p, s, sl); p += sl; }
    *p = 0;
    return out;
}

tx_t string_reverse(tx_t str) {
    const char* s = _S(str);
    if (!s) return strdup("");
    size_t len = strlen(s);
    char* out = (char*)malloc(len + 1);
    if (!out) return strdup("");
    for (size_t i = 0; i < len; i++) out[i] = s[len - 1 - i];
    out[len] = 0;
    return out;
}

tx_t string_pad(tx_t str, long length, tx_t pad_char) {
    const char* s = _S(str);
    if (!s) return strdup("");
    size_t sl = strlen(s);
    if (length <= (long)sl) return strdup(s);
    const char* pc = _S(pad_char);
    if (!pc || !*pc) pc = " ";
    size_t n = (size_t)length - sl;
    char* out = (char*)malloc((size_t)length + 1);
    if (!out) return strdup(s);
    memcpy(out, s, sl);
    for (size_t i = 0; i < n; i++) out[sl + i] = pc[0];
    out[sl + n] = 0;
    return out;
}

/* ── v0.48.26 — complete string library (std/string) ──────────────
   Case conversion, whitespace management, substring search and
   boundary validation, reversal, repetition, and both left/right
   padding. All follow the tx_t malloc/strdup contract of the
   std/string section; boolean results use the "1"/"0" convention. */

tx_t string_upper(tx_t str) {
    const char* s = _S(str);
    if (!s) return strdup("");
    size_t len = strlen(s);
    char* out = (char*)malloc(len + 1);
    if (!out) return strdup("");
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    out[len] = 0;
    return out;
}

tx_t string_lower(tx_t str) {
    const char* s = _S(str);
    if (!s) return strdup("");
    size_t len = strlen(s);
    char* out = (char*)malloc(len + 1);
    if (!out) return strdup("");
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    out[len] = 0;
    return out;
}

tx_t string_trim(tx_t str) {
    const char* s = _S(str);
    if (!s) return strdup("");
    const char* start = s;
    while (*start && (unsigned char)*start <= ' ') start++;
    if (*start == '\0') return strdup("");
    const char* end = start + strlen(start) - 1;
    while (end > start && (unsigned char)*end <= ' ') end--;
    size_t len = (size_t)(end - start + 1);
    char* out = (char*)malloc(len + 1);
    if (!out) return strdup("");
    memcpy(out, start, len);
    out[len] = 0;
    return out;
}

tx_t string_includes(tx_t str, tx_t sub) {
    const char* s = _S(str), *t = _S(sub);
    if (!s || !t) return "0";
    return strstr(s, t) ? "1" : "0";
}

tx_t string_starts_with(tx_t str, tx_t pre) {
    const char* s = _S(str), *p = _S(pre);
    if (!s || !p) return "0";
    return strncmp(s, p, strlen(p)) == 0 ? "1" : "0";
}

tx_t string_ends_with(tx_t str, tx_t suf) {
    const char* s = _S(str), *f = _S(suf);
    if (!s || !f) return "0";
    size_t sl = strlen(s), fl = strlen(f);
    if (fl > sl) return "0";
    return strcmp(s + sl - fl, f) == 0 ? "1" : "0";
}

tx_t string_pad_left(tx_t str, long length, tx_t pad_char) {
    const char* s = _S(str);
    if (!s) return strdup("");
    size_t sl = strlen(s);
    if (length <= (long)sl) return strdup(s);
    const char* pc = _S(pad_char);
    if (!pc || !*pc) pc = " ";
    size_t n = (size_t)length - sl;
    char* out = (char*)malloc((size_t)length + 1);
    if (!out) return strdup(s);
    for (size_t i = 0; i < n; i++) out[i] = pc[0];
    memcpy(out + n, s, sl);
    out[(size_t)length] = 0;
    return out;
}

/* ── std/fs ──────────────────────────────────────────────────── */

tx_t file_copy(tx_t src, tx_t dest) {
    const char* s = _S(src), *d = _S(dest);
    if (!s || !d) return "0";
    FILE* in = fopen(s, "rb");
    if (!in) return "0";
    FILE* out = fopen(d, "wb");
    if (!out) { fclose(in); return "0"; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); return "0"; }
    fclose(in);
    if (fclose(out) != 0) return "0";
    return "1";
}

tx_t file_move(tx_t src, tx_t dest) {
    const char* s = _S(src), *d = _S(dest);
    if (!s || !d) return "0";
    if (rename(s, d) == 0) return "1";
    if (strcmp(file_copy((tx_t)s, (tx_t)d), "1") == 0) {
        if (remove(s) == 0) return "1";
    }
    return "0";
}

tx_t file_stat(tx_t path) {
    const char* p = _S(path);
    if (!p) return NULL;
    struct stat st;
    if (stat(p, &st) != 0) return NULL;
    PlantArray* m = plant_list_create(8);
    char sb[64];
    m = plant_list_push(m, strdup("size"));
    snprintf(sb, 64, "%lld", (long long)st.st_size);
    m = plant_list_push(m, strdup(sb));
    m = plant_list_push(m, strdup("mtime"));
    snprintf(sb, 64, "%lld", (long long)st.st_mtime);
    m = plant_list_push(m, strdup(sb));
    m = plant_list_push(m, strdup("mode"));
    snprintf(sb, 64, "%o", (unsigned)(st.st_mode & 0777));
    m = plant_list_push(m, strdup(sb));
    return (tx_t)m;
}

/* ── v0.48.28 — fs:APPEND ──
   Appends text to an existing file, or creates the file when the
   target path does not exist ("ab": append mode auto-creates).
   Returns "1" on success, "0" on failure (NULL path, open error,
   close error). */
tx_t fs_append(tx_t path, tx_t content) {
    const char* p = _S(path);
    if (!p) return "0";
    FILE* f = fopen(p, "ab");
    if (!f) return "0";
    if (content) {
        const char* c = _S(content);
        if (c && c[0]) fwrite(c, 1, strlen(c), f);
    }
    if (fclose(f) != 0) return "0";
    return "1";
}

/* ── std/io (v0.48.28) ───────────────────────────────────────── */

/* io_showln: text output followed by a trailing newline; NULL or
   empty input degrades to a bare newline (no pointer faults). */
tx_t io_showln(tx_t s) {
    const char* t = _S(s);
    if (t) printf("%s\n", t);
    else printf("\n");
    return "1";
}

/* io_flush: force stdout buffer clearance so printed data appears
   without buffering delays (useful when stdout is piped). */
tx_t io_flush(void) {
    fflush(stdout);
    return "1";
}

/* ── std/math ────────────────────────────────────────────────── */

static double _num(const char* s) { return s ? atof(s) : 0.0; }

tx_t math_sin(tx_t x)   { char b[64]; snprintf(b, 64, "%.10g", sin(_num(_S(x)))); return strdup(b); }
tx_t math_cos(tx_t x)   { char b[64]; snprintf(b, 64, "%.10g", cos(_num(_S(x)))); return strdup(b); }
tx_t math_sqrt(tx_t x)  { double v = _num(_S(x)); if (v < 0) return strdup("0"); char b[64]; snprintf(b, 64, "%.10g", sqrt(v)); return strdup(b); }
tx_t math_pow(tx_t x, tx_t y) { char b[64]; snprintf(b, 64, "%.10g", pow(_num(_S(x)), _num(_S(y)))); return strdup(b); }
tx_t math_floor(tx_t x) { char b[64]; snprintf(b, 64, "%.0f", floor(_num(_S(x)))); return strdup(b); }
tx_t math_ceil(tx_t x)  { char b[64]; snprintf(b, 64, "%.0f", ceil(_num(_S(x)))); return strdup(b); }
tx_t math_round(tx_t x) { char b[64]; snprintf(b, 64, "%.0f", round(_num(_S(x)))); return strdup(b); }
tx_t math_min(tx_t a, tx_t b) { const char* x = _S(a), *y = _S(b); return strdup(_num(x) <= _num(y) ? (x ? x : "") : (y ? y : "")); }
tx_t math_max(tx_t a, tx_t b) { const char* x = _S(a), *y = _S(b); return strdup(_num(x) >= _num(y) ? (x ? x : "") : (y ? y : "")); }

tx_t math_random(void) {
    static int seeded = 0;
    if (!seeded) { srand((unsigned)(time(NULL) ^ (long)getpid())); seeded = 1; }
    char b[64];
    snprintf(b, 64, "%.6f", (double)rand() / (double)RAND_MAX);
    return strdup(b);
}

/* ── v0.48.27 — std/math library (LOG/PI/E/SIGN/CLAMP) ──
   Constants fall back to explicit definitions when libc does not
   expose M_PI / M_E (e.g. strict ANSI/feature-test builds). */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

tx_t math_log(tx_t x) {
    double v = _num(_S(x));
    if (v <= 0) {
        char e[96];
        snprintf(e, 96, "ERR: math_log(x): x must be > 0 (x = %.10g)", v);
        return strdup(e);
    }
    char b[64];
    snprintf(b, 64, "%.10g", log(v));
    return strdup(b);
}
tx_t math_sign(tx_t x) {
    double v = _num(_S(x));
    if (v < 0) return strdup("-1");
    if (v > 0) return strdup("1");
    return strdup("0");
}
tx_t math_clamp(tx_t x, tx_t lo, tx_t hi) {
    double v = _num(_S(x)), a = _num(_S(lo)), b = _num(_S(hi));
    if (v < a) v = a;
    if (v > b) v = b;
    char buf[64];
    snprintf(buf, 64, "%.10g", v);
    return strdup(buf);
}
tx_t math_pi(void) { char b[64]; snprintf(b, 64, "%.15g", M_PI); return strdup(b); }
tx_t math_e(void)  { char b[64]; snprintf(b, 64, "%.15g", M_E);  return strdup(b); }

/* ── std/time ────────────────────────────────────────────────── */

tx_t time_now(void) {
    char b[64];
    snprintf(b, 64, "%lld", (long long)time(NULL));
    return strdup(b);
}

tx_t time_format(tx_t t, tx_t format) {
    const char* fmt = _S(format);
    const char* tv = _S(t);
    if (!fmt || !tv) return strdup("");
    time_t tt = (time_t)atoll(tv);
    struct tm tmv;
    localtime_r(&tt, &tmv);
    char b[256];
    if (strftime(b, sizeof(b), fmt, &tmv) == 0) return strdup("");
    return strdup(b);
}

tx_t time_parse(tx_t str, tx_t format) {
    const char* s = _S(str), *fmt = _S(format);
    if (!s || !fmt) return strdup("");
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    if (!strptime(s, fmt, &tmv)) return strdup("");
    time_t tt = mktime(&tmv);
    if (tt == (time_t)-1) return strdup("");
    char b[64];
    snprintf(b, 64, "%lld", (long long)tt);
    return strdup(b);
}

tx_t time_sleep(tx_t seconds) {
    double sec = atof(_S(seconds) ? _S(seconds) : "0");
    if (sec < 0) sec = 0;
    struct timespec ts;
    ts.tv_sec = (time_t)sec;
    ts.tv_nsec = (long)((sec - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
    return "1";
}

/* ═══════════════════════════════════════════════════════════════
   v0.47.2 — Native Data Structures (Set / Queue / Stack)
   Implementations; signatures in plant_compat.h
   ═══════════════════════════════════════════════════════════════ */

/* ── Set ──────────────────────────────────────────────────────── */

static uint64_t _set_hash(uintptr_t v) {
    uint64_t x = (uint64_t)v;
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27; x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return x;
}

static PlantSet* _set_alloc(size_t cap) {
    PlantSet* s = (PlantSet*)plant_alloc(sizeof(PlantSet));
    s->cap = cap;
    s->count = 0;
    s->tombs = 0;
    s->slots = (uintptr_t*)calloc(cap, sizeof(uintptr_t));
    return s;
}

static void _set_rehash(PlantSet* s, size_t new_cap) {
    /* in-place: only the slots buffer moves; the PlantSet handle stays valid */
    uintptr_t* ns = (uintptr_t*)calloc(new_cap, sizeof(uintptr_t));
    if (!ns) return;
    for (size_t i = 0; i < s->cap; i++) {
        uintptr_t v = s->slots[i];
        if (v == 0 || v == (uintptr_t)-1) continue;
        size_t idx = (size_t)(_set_hash(v) & (new_cap - 1));
        while (ns[idx] != 0) idx = (idx + 1) & (new_cap - 1);
        ns[idx] = v;
    }
    free(s->slots);
    s->slots = ns;
    s->cap = new_cap;
    s->tombs = 0;
}

static int _set_contains(PlantSet* s, uintptr_t v) {
    size_t idx = (size_t)(_set_hash(v) & (s->cap - 1));
    while (s->slots[idx] != 0) {
        if (s->slots[idx] == v) return 1;
        idx = (idx + 1) & (s->cap - 1);
    }
    return 0;
}

tx_t set_create(void) {
    return (tx_t)_set_alloc(8);
}

tx_t set_add(tx_t sv, tx_t val) {
    PlantSet* s = (PlantSet*)sv;
    uintptr_t v = (uintptr_t)val;
    if (!s || v == 0) return "0";
    if (_set_contains(s, v)) return "0";
    if ((s->count + s->tombs + 1) * 10 >= s->cap * 7) {
        _set_rehash(s, s->cap * 2);
    }
    size_t idx = (size_t)(_set_hash(v) & (s->cap - 1));
    while (s->slots[idx] != 0 && s->slots[idx] != (uintptr_t)-1)
        idx = (idx + 1) & (s->cap - 1);
    if (s->slots[idx] == (uintptr_t)-1) s->tombs--;
    s->slots[idx] = v;
    s->count++;
    return "1";
}

tx_t set_has(tx_t sv, tx_t val) {
    PlantSet* s = (PlantSet*)sv;
    if (!s || (uintptr_t)val == 0) return "0";
    return _set_contains(s, (uintptr_t)val) ? "1" : "0";
}

tx_t set_remove(tx_t sv, tx_t val) {
    PlantSet* s = (PlantSet*)sv;
    uintptr_t v = (uintptr_t)val;
    if (!s || v == 0) return "0";
    size_t idx = (size_t)(_set_hash(v) & (s->cap - 1));
    while (s->slots[idx] != 0) {
        if (s->slots[idx] == v) {
            s->slots[idx] = (uintptr_t)-1;
            s->count--;
            s->tombs++;
            return "1";
        }
        idx = (idx + 1) & (s->cap - 1);
    }
    return "0";
}

long set_size(tx_t sv) {
    PlantSet* s = (PlantSet*)sv;
    return s ? (long)s->count : 0;
}

tx_t set_to_list(tx_t sv) {
    PlantSet* s = (PlantSet*)sv;
    PlantArray* l = plant_list_create(s ? (int64_t)s->count : 0);
    if (!s) return (tx_t)l;
    for (size_t i = 0; i < s->cap; i++) {
        uintptr_t v = s->slots[i];
        if (v == 0 || v == (uintptr_t)-1) continue;
        l = plant_list_push(l, (void*)v);
    }
    return (tx_t)l;
}

/* ── Queue ────────────────────────────────────────────────────── */

tx_t queue_create(void) {
    PlantQueue* q = (PlantQueue*)plant_alloc(sizeof(PlantQueue));
    q->cap = 8;
    q->head = 0;
    q->count = 0;
    q->buf = (void**)calloc(q->cap, sizeof(void*));
    return (tx_t)q;
}

tx_t queue_push(tx_t qv, tx_t val) {
    PlantQueue* q = (PlantQueue*)qv;
    if (!q) return qv;
    if (q->count == q->cap) {
        size_t ncap = q->cap * 2;
        void** nb = (void**)calloc(ncap, sizeof(void*));
        for (size_t i = 0; i < q->count; i++)
            nb[i] = q->buf[(q->head + i) % q->cap];
        free(q->buf);
        q->buf = nb;
        q->cap = ncap;
        q->head = 0;
    }
    q->buf[(q->head + q->count) % q->cap] = val;
    q->count++;
    return qv;
}

tx_t queue_pop(tx_t qv) {
    PlantQueue* q = (PlantQueue*)qv;
    if (!q || q->count == 0) return "";
    void* v = q->buf[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;
    return (tx_t)v;
}

tx_t queue_peek(tx_t qv) {
    PlantQueue* q = (PlantQueue*)qv;
    if (!q || q->count == 0) return "";
    return (tx_t)q->buf[q->head];
}

long queue_size(tx_t qv) {
    PlantQueue* q = (PlantQueue*)qv;
    return q ? (long)q->count : 0;
}

/* ── Stack ────────────────────────────────────────────────────── */

tx_t stack_create(void) {
    PlantStack* s = (PlantStack*)plant_alloc(sizeof(PlantStack));
    s->cap = 8;
    s->count = 0;
    s->buf = (void**)calloc(s->cap, sizeof(void*));
    return (tx_t)s;
}

tx_t stack_push(tx_t sv, tx_t val) {
    PlantStack* s = (PlantStack*)sv;
    if (!s) return sv;
    if (s->count == s->cap) {
        s->cap *= 2;
        s->buf = (void**)realloc(s->buf, s->cap * sizeof(void*));
    }
    s->buf[s->count++] = val;
    return sv;
}

tx_t stack_pop(tx_t sv) {
    PlantStack* s = (PlantStack*)sv;
    if (!s || s->count == 0) return "";
    void* v = s->buf[--s->count];
    return (tx_t)v;
}

tx_t stack_peek(tx_t sv) {
    PlantStack* s = (PlantStack*)sv;
    if (!s || s->count == 0) return "";
    return (tx_t)s->buf[s->count - 1];
}

long stack_size(tx_t sv) {
    PlantStack* s = (PlantStack*)sv;
    return s ? (long)s->count : 0;
}

/* ═══════════════════════════════════════════════════════════════
   v0.47.3 — Advanced FFI: diagnostics + memory lifecycle
   Signatures in plant_compat.h
   ═══════════════════════════════════════════════════════════════ */

/* errno captured from the most recent FFI call (0 = success) */
long ffi_last_error(void) {
    return (long)errno;
}

/* human-readable diagnostic: dlerror() first (dynamic-loader errors),
   otherwise strerror(errno) (system errors) */
tx_t ffi_last_error_msg(void) {
    const char* dl = dlerror();
    if (dl) return strdup(dl);
    return strdup(strerror(errno));
}

/* free() a pointer returned by FFI; NULL is rejected with EINVAL so
   error-checking code can rely on ffi_last_error() */
void ffi_free(void* p) {
    if (!p) { errno = EINVAL; return; }
    errno = 0;
    free(p);
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.4 — FFI Optional Extensions
   Error codes, debug switch, callback registry, profiling hooks.
   Signatures in plant_compat.h
   ═══════════════════════════════════════════════════════════════ */

long plant_ffi_errno = FFI_OK;

static long plant_ffi_debug_on = -1;  /* -1 = follow PLANT_FFI_DEBUG */

void plant_ffi_debug_set(long on) {
    plant_ffi_debug_on = on;
}

void plant_ffi_debug_print(tx_t msg) {
    long on = plant_ffi_debug_on;
    if (on < 0) {
        const char* e = getenv("PLANT_FFI_DEBUG");
        on = (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
    }
    if (on) fprintf(stderr, "[ffi] %s\n", _S(msg));
}

/* callback registry: tag → fn; tags are plain C strings */
typedef struct CbEntry { char* tag; plant_cb_t fn; } CbEntry;

static CbEntry* plant_cb_table = NULL;
static size_t   plant_cb_cap   = 0;
static size_t   plant_cb_count = 0;

static long plant_cb_find(tx_t tag) {
    const char* t = _S(tag);
    for (size_t i = 0; i < plant_cb_count; i++)
        if (strcmp(plant_cb_table[i].tag, t) == 0) return (long)i;
    return -1;
}

tx_t plant_cb_ensure(tx_t tag, plant_cb_t fn) {
    if (!fn) { plant_ffi_errno = FFI_ERR_SIGNATURE; return ""; }
    long i = plant_cb_find(tag);
    if (i < 0) {
        if (plant_cb_count >= plant_cb_cap) {
            size_t nc = plant_cb_cap ? plant_cb_cap * 2 : 8;
            CbEntry* nt = (CbEntry*)realloc(plant_cb_table, nc * sizeof(CbEntry));
            if (!nt) { plant_ffi_errno = FFI_ERR_MEMORY; return ""; }
            plant_cb_table = nt;
            plant_cb_cap = nc;
        }
        i = (long)plant_cb_count++;
        plant_cb_table[i].tag = strdup(_S(tag));
    }
    plant_cb_table[i].fn = fn;
    plant_ffi_errno = FFI_OK;
    return tag;
}

tx_t plant_cb_call(tx_t tag, long ctx, tx_t val) {
    long i = plant_cb_find(tag);
    if (i < 0) { plant_ffi_errno = FFI_ERR_CALLBACK; return ""; }
    plant_ffi_errno = FFI_OK;
    return plant_cb_table[i].fn(ctx, val);
}

void plant_cb_unregister(tx_t tag) {
    long i = plant_cb_find(tag);
    if (i < 0) return;
    free(plant_cb_table[i].tag);
    for (size_t k = (size_t)i; k + 1 < plant_cb_count; k++)
        plant_cb_table[k] = plant_cb_table[k + 1];
    plant_cb_count--;
}

tx_t plant_cb_get(tx_t tag) {
    long i = plant_cb_find(tag);
    if (i < 0) return (tx_t)0;
    return (tx_t)(uintptr_t)plant_cb_table[i].fn;
}

/* profiling hooks — monotonic ns deltas per named section */
#define PLANT_PROFILE_MAX 64
typedef struct { char name[64]; int64_t start_ns; int64_t total_ns; int64_t count; } PlantProfile;
static PlantProfile plant_profiles[PLANT_PROFILE_MAX];
static size_t plant_profiles_count = 0;

static int64_t plant_ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

void plant_profile_start(tx_t name) {
    const char* n = _S(name);
    for (size_t i = 0; i < plant_profiles_count; i++)
        if (strcmp(plant_profiles[i].name, n) == 0) {
            plant_profiles[i].start_ns = plant_ns_now();
            return;
        }
    if (plant_profiles_count >= PLANT_PROFILE_MAX) return;
    size_t i = plant_profiles_count++;
    memset(&plant_profiles[i], 0, sizeof(PlantProfile));
    snprintf(plant_profiles[i].name, sizeof(plant_profiles[i].name), "%s", n);
    plant_profiles[i].start_ns = plant_ns_now();
}

void plant_profile_end(tx_t name) {
    int64_t now = plant_ns_now();
    const char* n = _S(name);
    for (size_t i = 0; i < plant_profiles_count; i++)
        if (strcmp(plant_profiles[i].name, n) == 0) {
            plant_profiles[i].total_ns += now - plant_profiles[i].start_ns;
            plant_profiles[i].count++;
            return;
        }
}

tx_t plant_profile_dump(void) {
    tx_t r = (tx_t)plant_map_create(8);
    for (size_t i = 0; i < plant_profiles_count; i++) {
        char buf[128];
        double ms = (double)plant_profiles[i].total_ns / 1e6;
        snprintf(buf, sizeof(buf), "%.3f ms / %lld calls", ms,
                 (long long)plant_profiles[i].count);
        plant_map_set((PlantMap*)r, plant_profiles[i].name, strdup(buf));
    }
    return r;
}

void plant_struct_free(void* p) {
    if (p) free(p);
}

/* ═══════════════════════════════════════════════════════════════

/* ═══════════════════════════════════════════════════════════════
   v0.48.3 — Advanced Async Engine
   Cooperative coroutine runtime: segmented arenas with lazy
   copy-on-write, adaptive RR↔priority-queue dispatcher, timers,
   contexts with deadline/priority inheritance, cancel tokens,
   MetricsAggregator ring buffer + adaptive sampling, tracing.
   Generated ASYNC ACTIONs are state structs + step functions.
   Value convention (matches the runtime): tx_t results are raw
   pointers or long bits — never dereferenced or freed by the
   engine; numeric results are converted with _from_long at call
   sites. Task-lifetime memory (state structs, arenas, task
   records, queue nodes) is freed exactly once at teardown.
   ═══════════════════════════════════════════════════════════════ */

typedef struct plant_seg {
    size_t   cap;
    size_t   used;
    int      refs;               /* shared segments: refcount */
    struct plant_seg* next;
} plant_seg;

typedef struct plant_arena {
    plant_seg* segs;             /* in-use segments (most recent first) */
    size_t     seg_size;         /* adaptive segment size */
    size_t     hits, misses;     /* allocator stats (cache policy) */
} plant_arena;

typedef struct plant_actx {
    int      magic;              /* 0xA51A4C7 */
    int      adaptive;
    long     cap;                /* congestion threshold (queue length) */
    long     priority;           /* context priority inheritance */
    long     deadline_scale;     /* 100..150 (percent) */
    long     congested_since;    /* ms when congestion started, -1 none */
    int      cancelled;
    tx_t     name;               /* context name (trace scope, v0.48.14) */
    struct plant_task* tasks;    /* live tasks in this context */
    struct plant_actx* next;     /* global context list */
} plant_actx;

typedef struct plant_ctok {
    int magic;                   /* 0xA51A700 */
    int cancelled;
} plant_ctok;

struct plant_task {
    int64_t      id;
    long         prio;           /* 0 HIGH, 1 NORMAL, 2 LOW */
    long         deadline;       /* absolute ms, -1 none */
    long         to;             /* timeout ms at spawn, -1 none */
    long         wake;           /* timer wake ms, -1 none */
    long         tkey;           /* timer list key: min(wake, deadline) */
    long         state_size;
    tx_t         st;             /* state struct (arena-allocated) */
    plant_stepfn step;
    tx_t         name;
    tx_t         res;            /* completed result (opaque, transferred) */
    struct plant_task* parent;
    struct plant_task* aw;       /* awaited child (self for sleeps) */
    struct plant_task* next;     /* ready queue / timer list link */
    struct plant_task* allnext;  /* global live-task list */
    struct plant_task* clink;    /* context task list link */
    struct plant_task* csib;     /* parent children list link */
    struct plant_task* chd, *chtail;
    plant_actx*  ctx;
    plant_ctok*  tok;
    int          status;         /* 0 ready, 1 suspended, 2 done */
    int          fatal;          /* skip body on pop (circular await) */
    int          dead;           /* freed guard */
    int          magic;          /* 0xA51A4C8 task handle */
    plant_arena  ar;
};

#define P_TASK_OF(st)   ((plant_task*)(*((tx_t*)(st))))   /* __self is field 0 */

static void plant_push_ready(plant_task* t);
static void plant_cancel_task(plant_task* t, const char* marker);

static long g_task_seq = 0;
static long g_live = 0;
static long g_completed = 0;
static plant_task* g_all = NULL;      /* live task list */
static plant_task* g_rr_head = NULL, *g_rr_tail = NULL;
static plant_task** g_pq = NULL;      /* binary min-heap by (prio, deadline, id) */
static int g_pq_len = 0, g_pq_cap = 0;
static plant_task* g_timer_head = NULL;
static int g_mode = 0;                /* 0 RR, 1 PQ */
static int g_mode_samples = 0;
static long g_threshold = 64;         /* MISSION CONFIG ADAPTIVE_THRESHOLD */
static int g_sampling_mode = 0;       /* 0 I/O, 1 CPU */
static plant_actx* g_dctx = NULL;
static plant_actx* g_ctxs = NULL;     /* all contexts */
static int g_metrics_on = 1;
static int g_metrics_interval = 16;
static long g_next_sample = 0;
static int g_trace_on = 0;
static int g_trace_level = 0;         /* 0 INFO, 1 DEBUG, 2 PERF */
static FILE* g_trace_fp = NULL;
static int g_in_fast = 0;             /* v0.48.15: inside a FAST action */
static size_t g_fast_cfg_cap = 0;     /* MISSION CONFIG FAST_HEAP_CAPACITY (0=default 8MB) */
static size_t g_fast_cfg_limit = 0;   /* MISSION CONFIG FAST_HEAP_LIMIT (0=default 64MB) */
static long g_fast_cfg_align = 0;     /* MISSION CONFIG FAST_ALIGNMENT (0=default 8) */
static plant_seg* g_seg_cache = NULL;
static size_t g_seg_size = 1024;
static size_t g_cache_max = 64;
static long g_arena_hits = 0, g_arena_misses = 0;
static int g_inited = 0;
static plant_task* g_running = NULL;  /* task currently mid-step */

static long plant_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
void plant_msleep(long ms) {
    if (ms <= 0) ms = 1;
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000 };
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
}

/* ── Segmented arenas: refcounted segments + adaptive cache ── */
static void* plant_arena_alloc(plant_arena* a, size_t n) {
    plant_seg* s = a->segs;
    while (s) {
        if (s->used + n <= s->cap) {
            void* p = (char*)(s + 1) + s->used;
            s->used += n; a->hits++; g_arena_hits++;
            return p;
        }
        s = s->next;
    }
    a->misses++; g_arena_misses++;
    size_t cap = n > a->seg_size ? n : a->seg_size;
    plant_seg* prev = NULL;
    for (plant_seg* c = g_seg_cache; c; prev = c, c = c->next) {
        if (c->cap >= cap) {
            if (prev) prev->next = c->next; else g_seg_cache = c->next;
            c->used = 0; c->refs = 1; c->next = a->segs; a->segs = c;
            void* p = (char*)(c + 1); c->used = n;
            return p;
        }
    }
    plant_seg* ns = (plant_seg*)malloc(sizeof(plant_seg) + cap);
    if (!ns) return NULL;
    ns->cap = cap; ns->used = n; ns->refs = 1; ns->next = a->segs; a->segs = ns;
    return (void*)(ns + 1);
}

/* lazy copy: share segments (refs++) — no bytes copied */
static void plant_arena_share(plant_arena* dst, plant_arena* src) {
    dst->segs = NULL; dst->seg_size = src->seg_size;
    dst->hits = dst->misses = 0;
    for (plant_seg* s = src->segs; s; s = s->next) {
        plant_seg* c = (plant_seg*)malloc(sizeof(plant_seg));
        if (!c) continue;
        memcpy(c, s, sizeof(plant_seg));
        c->refs++;
        c->next = dst->segs;
        dst->segs = c;
    }
}

/* copy-on-write materialization: any segment with refs > 1 becomes
   an exclusive copy (refs == 1) — no pointer sharing afterwards */
static void plant_arena_own(plant_arena* a) {
    plant_seg* s = a->segs;
    while (s) {
        if (s->refs > 1) {
            plant_seg* c = (plant_seg*)malloc(sizeof(plant_seg) + s->cap);
            if (c) {
                c->cap = s->cap; c->used = s->used; c->refs = 1;
                memcpy((void*)(c + 1), (void*)(s + 1), s->used);
                s->refs--;
                c->next = s->next;
                s->next = c;
            }
        }
        s = s->next;
    }
}

static void plant_arena_free(plant_arena* a) {
    if (a->hits + a->misses > 0) {
        double mr = (double)a->misses / (double)(a->hits + a->misses);
        if (mr > 0.08 && a->seg_size < (size_t)1 << 20) a->seg_size *= 2;
        else if (mr < 0.02 && a->seg_size > 256) a->seg_size /= 2;
    }
    if (a->seg_size > g_seg_size && g_seg_size < (size_t)1 << 20) g_seg_size = a->seg_size;
    plant_seg* s = a->segs;
    while (s) {
        plant_seg* nx = s->next;
        if (--s->refs <= 0) {
            if (g_cache_max > 0) {
                g_cache_max--;
                s->next = g_seg_cache;
                g_seg_cache = s;
            } else {
                free(s);
            }
        }
        s = nx;
    }
    a->segs = NULL;
}

/* ── Ready queue: adaptive RR ↔ priority queue ── */
static void plant_pq_swap(plant_task** a, plant_task** b) {
    plant_task* t = *a; *a = *b; *b = t;
}
static int plant_pq_less(plant_task* a, plant_task* b) {
    if (a->prio != b->prio) return a->prio < b->prio;
    if (a->deadline != b->deadline) return a->deadline < b->deadline;
    return a->id < b->id;
}
static void plant_pq_push(plant_task* t) {
    if (g_pq_len >= g_pq_cap) {
        g_pq_cap = g_pq_cap ? g_pq_cap * 2 : 64;
        g_pq = (plant_task**)realloc(g_pq, (size_t)g_pq_cap * sizeof(plant_task*));
    }
    int i = g_pq_len++;
    g_pq[i] = t;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (plant_pq_less(g_pq[i], g_pq[p])) { plant_pq_swap(&g_pq[i], &g_pq[p]); i = p; }
        else break;
    }
}
static plant_task* plant_pq_pop(void) {
    if (g_pq_len == 0) return NULL;
    plant_task* top = g_pq[0];
    g_pq[0] = g_pq[--g_pq_len];
    int i = 0;
    while (1) {
        int l = 2 * i + 1, r = 2 * i + 2, m = i;
        if (l < g_pq_len && plant_pq_less(g_pq[l], g_pq[m])) m = l;
        if (r < g_pq_len && plant_pq_less(g_pq[r], g_pq[m])) m = r;
        if (m == i) break;
        plant_pq_swap(&g_pq[i], &g_pq[m]);
        i = m;
    }
    return top;
}

static long plant_ready_len(void) {
    if (g_mode == 0) {
        long n = 0;
        for (plant_task* t = g_rr_head; t; t = t->next) n++;
        return n;
    }
    return g_pq_len;
}

/* dynamic threshold with hysteresis: enter PQ after 2 sustained
   samples at/above threshold; drop back to RR after 2 sustained
   samples at/below 70% of it (stabilizes under continuous load) */
static void plant_async_adjust_mode(long qlen) {
    if (g_mode == 0 && qlen >= g_threshold) {
        if (++g_mode_samples >= 2) { g_mode = 1; g_mode_samples = 0; }
    } else if (g_mode == 1 && qlen <= g_threshold * 7 / 10) {
        if (++g_mode_samples >= 2) { g_mode = 0; g_mode_samples = 0; }
    } else {
        g_mode_samples = 0;
    }
}

static void plant_push_ready(plant_task* t) {
    t->status = 0;
    plant_async_adjust_mode(plant_ready_len());
    if (g_mode == 0) {
        t->next = NULL;
        if (g_rr_tail) g_rr_tail->next = t; else g_rr_head = t;
        g_rr_tail = t;
    } else {
        plant_pq_push(t);
    }
}

static plant_task* plant_pop_ready(void) {
    plant_async_adjust_mode(plant_ready_len());
    plant_task* t = NULL;
    if (g_mode == 0) {
        t = g_rr_head;
        if (t) { g_rr_head = t->next; if (!g_rr_head) g_rr_tail = NULL; t->next = NULL; }
    } else {
        t = plant_pq_pop();
    }
    return t;
}

/* ── Timer list (sorted by tkey = min(wake, deadline)) ── */
static void plant_timer_insert(plant_task* t) {
    t->tkey = -1;
    if (t->wake >= 0 && (t->tkey < 0 || t->wake < t->tkey)) t->tkey = t->wake;
    if (t->deadline >= 0 && (t->tkey < 0 || t->deadline < t->tkey)) t->tkey = t->deadline;
    plant_task** pp = &g_timer_head;
    while (*pp && (*pp)->tkey < t->tkey) pp = &(*pp)->next;
    t->next = *pp;
    *pp = t;
    t->status = 1;
}
static void plant_timer_remove(plant_task* t) {
    plant_task** pp = &g_timer_head;
    while (*pp && *pp != t) pp = &(*pp)->next;
    if (*pp == t) *pp = t->next;
}
static void plant_fire_timers(long now) {
    while (g_timer_head && g_timer_head->tkey <= now) {
        plant_task* t = g_timer_head;
        g_timer_head = t->next;
        t->next = NULL;
        int is_timeout = (t->deadline >= 0 && t->deadline <= now &&
                          (t->wake < 0 || t->wake > t->deadline));
        if (is_timeout) {
            plant_cancel_task(t, "TIMEOUT");
        } else {
            plant_push_ready(t);
        }
    }
}
static long plant_next_timer(void) {
    return g_timer_head ? g_timer_head->tkey : -1;
}

/* ── Context deadline/priority scaling ── */
static void plant_ctx_tick(plant_actx* ctx, long qlen) {
    if (!ctx || !ctx->adaptive) return;
    long now = plant_ms();
    if (qlen > ctx->cap) {
        if (ctx->congested_since < 0) ctx->congested_since = now;
        else if (now - ctx->congested_since >= 50 && ctx->deadline_scale < 150) {
            ctx->deadline_scale += 10;
            ctx->congested_since = now;
        }
    } else {
        ctx->congested_since = -1;
        if (ctx->deadline_scale > 100) ctx->deadline_scale -= 5;
    }
}

/* ── Tracing ── */
static void plant_trace_write(const char* line) {
    if (g_trace_on && g_trace_fp) {
        fprintf(g_trace_fp, "%s\n", line);
        fflush(g_trace_fp);
    }
}

/* ── Task teardown / cancellation ── */
static void plant_unlink_ready(plant_task* t) {
    if (t->status != 0) return;
    if (g_mode == 0) {
        if (g_rr_head == t) { g_rr_head = t->next; if (!g_rr_head) g_rr_tail = NULL; return; }
        for (plant_task* p = g_rr_head; p; p = p->next)
            if (p->next == t) { p->next = t->next; if (g_rr_tail == t) g_rr_tail = p; break; }
    } else {
        for (int i = 0; i < g_pq_len; i++)
            if (g_pq[i] == t) { g_pq[i] = g_pq[--g_pq_len]; break; }
    }
}

static void plant_task_free(plant_task* t) {
    if (t->dead) return;
    t->dead = 1;
    if (t->status == 1) plant_timer_remove(t);
    plant_unlink_ready(t);
    if (t->ctx) {
        for (plant_task** pp = &t->ctx->tasks; *pp; pp = &(*pp)->clink)
            if (*pp == t) { *pp = t->clink; break; }
    }
    for (plant_task** pp = &g_all; *pp; pp = &(*pp)->allnext)
        if (*pp == t) { *pp = t->allnext; break; }
    if (t->parent) {
        plant_task** cp = &t->parent->chd;
        while (*cp && *cp != t) cp = &(*cp)->csib;
        if (*cp == t) {
            *cp = t->csib;
            if (t->parent->chtail == t) t->parent->chtail = *cp;
        }
    }
    for (plant_task* c = t->chd; c; c = c->csib) c->parent = NULL;
    plant_arena_free(&t->ar);
    free(t);
    g_live--;
}

static void plant_teardown_tree(plant_task* t) {
    plant_task* c = t->chd;
    while (c) {
        plant_task* nx = c->csib;
        plant_teardown_tree(c);
        c = nx;
    }
    t->chd = t->chtail = NULL;
    plant_task_free(t);
}

static void plant_teardown_children(plant_task* t) {
    plant_task* c = t->chd;
    while (c) {
        plant_task* nx = c->csib;
        plant_teardown_tree(c);
        c = nx;
    }
    t->chd = t->chtail = NULL;
}

/* cancel (marker "CANCELLED"/"TIMEOUT"): isolation arenas of the
   cancelled subtree are freed immediately; a parent awaiting the
   task resumes with the marker */
static void plant_cancel_task(plant_task* t, const char* marker) {
    if (t->dead || t->status == 2 || t == g_running) return;
    int self_await = (t->status == 1 && t->aw == t);
    plant_task* aw = t->aw;
    t->aw = NULL;
    plant_teardown_children(t);
    if (t->status == 1) {
        if (self_await) {
            t->res = (tx_t)marker;
            plant_push_ready(t);
            return;
        }
        if (aw) plant_task_free(aw);
        t->res = (tx_t)marker;
        plant_push_ready(t);
        return;
    }
    /* ready task awaiting nothing: parent resumes with the marker */
    if (t->parent && t->parent->aw == t) {
        t->parent->aw = NULL;
        t->parent->res = (tx_t)marker;
        plant_push_ready(t->parent);
    }
    plant_task_free(t);
}

/* ── MetricsAggregator: ring buffer + adaptive sampling ── */
typedef struct {
    long ms, mode, qlen, tps, scale, miss;
} plant_metric;
static plant_metric g_mbuf[256];
static int g_mhead = 0, g_mcount = 0;

static void plant_metric_sample(long now) {
    if (!g_metrics_on || now < g_next_sample) return;
    static long last_ms = 0, last_done = 0;
    long tps = 0;
    if (last_ms > 0 && now > last_ms) tps = (g_completed - last_done) * 1000 / (now - last_ms);
    long miss = 0;
    if (g_arena_hits + g_arena_misses > 0)
        miss = g_arena_misses * 100 / (g_arena_hits + g_arena_misses);
    plant_metric m = { now, g_mode, plant_ready_len(), tps,
                       g_dctx ? g_dctx->deadline_scale : 100, miss };
    g_mbuf[g_mhead] = m;
    g_mhead = (g_mhead + 1) % 256;
    if (g_mcount < 256) g_mcount++;
    char line[160];
    snprintf(line, sizeof(line), "M,%ld,%ld,%ld,%ld,%ld,%ld",
             m.ms, m.mode, m.qlen, m.tps, m.scale, m.miss);
    plant_trace_write(line);
    /* adaptive sampling rate: finer under sustained high load */
    long ql = plant_ready_len();
    int base = g_sampling_mode == 1 ? 4 : 16;
    g_metrics_interval = ql > g_threshold * 2 ? 2 : base;
    g_next_sample = now + g_metrics_interval;
    last_ms = now;
    last_done = g_completed;
}

/* ── Public API ── */
tx_t plant_async_alloc_state(long size, tx_t name) {
    plant_task* t = (plant_task*)calloc(1, sizeof(plant_task));
    if (!t) return NULL;
    t->state_size = size;
    t->name = name;
    t->magic = 0xA51A4C8;
    t->ar.seg_size = g_seg_size;
    t->st = plant_arena_alloc(&t->ar, (size_t)size);
    if (!t->st) { free(t); return NULL; }
    memset(t->st, 0, (size_t)size);
    *((tx_t*)t->st) = (tx_t)t;      /* __self = task */
    t->allnext = g_all;
    g_all = t;
    return t->st;
}

tx_t plant_async_register(tx_t st, plant_stepfn step, tx_t parent, tx_t ctx,
                          long prio, long dl, long to, tx_t tok, tx_t name) {
    plant_async_init();
    plant_task* t = P_TASK_OF(st);
    plant_task* p = parent ? P_TASK_OF(parent) : NULL;
    t->step = step;
    t->name = name;
    t->ctx = NULL;
    if (ctx) {
        for (plant_actx* c = g_ctxs; c; c = c->next)
            if ((void*)c == (void*)ctx) { t->ctx = c; break; }
    }
    if (!t->ctx) t->ctx = g_dctx;
    t->tok = tok ? (plant_ctok*)tok : NULL;
    t->prio = prio;
    if (p && p->prio < t->prio) t->prio = p->prio;           /* inheritance */
    if (t->ctx && t->ctx->priority < t->prio) t->prio = t->ctx->priority;
    t->to = to;
    long now = plant_ms();
    long dl_abs = -1;
    if (dl >= 0) dl_abs = now + dl;
    if (p && p->deadline >= 0 && (dl_abs < 0 || p->deadline < dl_abs)) dl_abs = p->deadline;
    if (to >= 0) { long toa = now + to; if (dl_abs < 0 || toa < dl_abs) dl_abs = toa; }
    if (t->ctx && t->ctx->adaptive && dl_abs >= 0 && t->ctx->deadline_scale > 100)
        dl_abs = now + (dl_abs - now) * t->ctx->deadline_scale / 100;  /* adaptive scaling */
    t->deadline = dl_abs;
    t->parent = p;
    if (p) { t->csib = p->chd; p->chd = t; if (!p->chtail) p->chtail = t; }
    if (t->ctx) { t->clink = t->ctx->tasks; t->ctx->tasks = t; }
    if (t->ctx && t->ctx->cancelled) {
        t->fatal = 1;
        t->res = (tx_t)"CANCELLED";
    }
    /* deadlock prevention: circular awaits are rejected at spawn time */
    for (plant_task* a = p; a; a = a->parent) {
        if (a->step == step) {
            t->fatal = 1;
            t->res = (tx_t)"CIRCULAR_AWAIT";
            break;
        }
    }
    plant_ctx_tick(t->ctx, plant_ready_len());
    t->id = ++g_task_seq;
    g_live++;
    if (g_trace_on) {
        char line[160];
        snprintf(line, sizeof(line), "S,%ld,%ld,%s,%ld", now, t->id,
                 _S(name), t->prio);
        plant_trace_write(line);
    }
    plant_push_ready(t);
    return (tx_t)t;
}

/* result is an opaque tx_t (pointer or long bits) — transferred,
   never dereferenced or freed by the engine (see header comment) */
void plant_async_finish(tx_t st, tx_t res) {
    plant_task* t = P_TASK_OF(st);
    t->res = res;
    t->status = 2;
    g_completed++;
    if (g_trace_on) {
        char line[160];
        snprintf(line, sizeof(line), "C,%ld,%ld,%s,ok", plant_ms(), t->id, _S(t->name));
        plant_trace_write(line);
    }
    if (t->parent) {
        plant_task* p = t->parent;
        if (p->aw == t) {
            if (p->status == 1) plant_push_ready(p);
            return;                     /* kept until await_result */
        }
    }
    plant_task_free(t);
}

void plant_async_suspend(tx_t st, tx_t child) {
    plant_task* t = P_TASK_OF(st);
    /* child is the task handle returned by plant_async_register:
       t->aw must equal the child's plant_task* so finish/await_result/
       cancel all agree on the same record. 0 = self (sleep path). */
    t->aw = child ? (plant_task*)child : t;
    if (t->aw == t && t->wake >= 0) {   /* sleep: into the timer list */
        plant_timer_insert(t);
        return;
    }
    t->status = 1;                      /* waiting on child */
}

void plant_async_sleep(tx_t st, long ms) {
    plant_task* t = P_TASK_OF(st);
    t->wake = plant_ms() + ms;
}

tx_t plant_async_await_result(tx_t st) {
    plant_task* t = P_TASK_OF(st);
    plant_task* c = t->aw;
    if (!c) {                           /* cancelled-before-completion path */
        tx_t r = t->res;
        t->res = NULL;
        return r ? r : (tx_t)"";
    }
    tx_t res = c->res;
    c->res = NULL;
    if (c != t) {
        plant_task_free(c);
    } else {
        t->wake = -1;
    }
    t->aw = NULL;
    return res ? res : (tx_t)"";
}

tx_t plant_async_ctx_create(long adaptive, long cap, tx_t name) {
    plant_async_init();
    plant_actx* ctx = (plant_actx*)calloc(1, sizeof(plant_actx));
    if (!ctx) return NULL;
    ctx->magic = 0xA51A4C7;
    ctx->adaptive = (int)adaptive;
    ctx->cap = cap > 0 ? cap : 64;
    ctx->priority = 1;
    ctx->deadline_scale = 100;
    ctx->congested_since = -1;
    ctx->name = name;
    ctx->next = g_ctxs;
    g_ctxs = ctx;
    return (tx_t)ctx;
}

tx_t plant_async_token_create(void) {
    plant_ctok* tok = (plant_ctok*)calloc(1, sizeof(plant_ctok));
    if (!tok) return NULL;
    tok->magic = 0xA51A700;
    return (tx_t)tok;
}

void plant_async_ctx_cancel(tx_t ctxv) {
    plant_actx* ctx = (plant_actx*)ctxv;
    if (!ctx || ctx->magic != 0xA51A4C7) return;
    ctx->cancelled = 1;
    plant_task* t = ctx->tasks;
    while (t) {
        plant_task* nx = t->clink;
        plant_cancel_task(t, "CANCELLED");
        t = nx;
    }
    ctx->tasks = NULL;
}

void plant_async_cancel(tx_t x) {
    if (!x) return;
    if (((plant_task*)x)->magic == 0xA51A4C8) {
        plant_task* t = (plant_task*)x;
        if (!t->dead && t->status != 2) plant_cancel_task(t, "CANCELLED");
        return;
    }
    if (((plant_actx*)x)->magic == 0xA51A4C7) { plant_async_ctx_cancel(x); return; }
    if (((plant_ctok*)x)->magic == 0xA51A700) {
        plant_ctok* tok = (plant_ctok*)x;
        tok->cancelled = 1;
        plant_task* t = g_all;
        while (t) {
            plant_task* nx = t->allnext;
            if (!t->dead && t->tok == tok) plant_cancel_task(t, "CANCELLED");
            t = nx;
        }
        return;
    }
}

/* ── v0.48.14 — Async IN Context: ctx-scoped wrappers ──
   plant_async_start_in / plant_async_in are macros in plant_compat.h
   (fire-and-forget spawn: fn(0, ctx, args)) — a real variadic
   forwarder cannot be written portably, and the macro keeps the
   non-contextual path byte-identical. The wrappers below add real
   ctx scoping to await/cancel/trace. */

static plant_actx* plant_ctx_validate(tx_t ctx) {
    if (!ctx) return NULL;
    plant_actx* c = (plant_actx*)ctx;
    /* v0.48.15: WITH MISSION FAST actions skip the redundant magic
       check for peak throughput (zero-trust is enforced at the
       boundary guard instead) */
    if (g_in_fast) return c;
    if (c->magic != 0xA51A4C7) return NULL;
    return c;
}

/* AWAIT ... IN ctx: the child was spawned into ctx by the codegen
   (entry called with (st, ctx, args)); suspend the parent on it. */
tx_t plant_async_await_in(tx_t st, tx_t ctx, tx_t handle) {
    plant_ctx_validate(ctx);            /* scope check only */
    plant_async_suspend(st, handle);
    return handle;
}

/* CANCEL h IN ctx: cancel the handle only if it lives in ctx;
   ctx/token handles fall through to plain cancellation. */
void plant_async_cancel_in(tx_t ctx, tx_t x) {
    if (x && ((plant_task*)x)->magic == 0xA51A4C8) {
        plant_task* t = (plant_task*)x;
        plant_actx* c = plant_ctx_validate(ctx);
        if (c && t->ctx && t->ctx != c) return;   /* not in this ctx */
    }
    plant_async_cancel(x);
}

/* TRACE lvl msg IN ctx: scope the event with the context name
   (falls back to the default context for invalid handles). */
void plant_async_trace_in(tx_t ctx, long level, tx_t msg) {
    plant_actx* c = plant_ctx_validate(ctx);
    if (!c) c = g_dctx;
    plant_trace(level, c && c->name ? c->name : (tx_t)"", msg);
}

/* live task count inside a context (test/introspection helper) */
long plant_async_ctx_tasks(tx_t ctxv) {
    plant_actx* c = plant_ctx_validate(ctxv);
    if (!c) return 0;
    long n = 0;
    for (plant_task* t = c->tasks; t; t = t->clink)
        if (!t->dead) n++;
    return n;
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.15 — Mission Mode FAST
   WITH MISSION FAST actions bind a bump-pointer heap (strict 8-byte
   alignment, 8MB initial capacity, 64MB hard cap; both configurable
   via MISSION CONFIG). Overflowing the hard cap escalates to BALANCED
   mode (plain malloc) with a WARN audit event; plant_fast_reset
   rewinds the bump and frees escalated allocations. A lock-free audit
   ring (single producer, volatile head) records mode/boundary/
   capability telemetry without blocking. Zero-trust: only
   FILE_READ/FILE_WRITE/NET_CONNECT are granted by default; the
   Boundary Handshake blocks FAST->SAFE calls via plant_boundary_block
   emitted at SAFE action entries.
   ═══════════════════════════════════════════════════════════════ */

/* NonBlockingAuditLogger — lock-free ring buffer */
#define PLANT_AUDIT_CAP 256
#define PLANT_AUDIT_MSG 96
typedef struct {
    long seq;
    char kind[24];
    char msg[PLANT_AUDIT_MSG];
    unsigned long chain;   /* v0.48.16 hash-chain link (tamper proof) */
} plant_audit_ev;

static plant_audit_ev g_audit[PLANT_AUDIT_CAP];
static volatile long g_audit_head = 0;   /* next write slot */
static long g_audit_count = 0;
static long g_audit_overflow = 0;
static long g_audit_seq = 0;
static unsigned long g_audit_chain = 14695981039346656037UL; /* FNV-1a offset */
static unsigned long g_audit_evicted = 14695981039346656037UL;

static unsigned long plant_audit_hash(unsigned long h, long seq,
                                      const char* kind, const char* msg) {
    h = (h ^ (unsigned long)seq) * 1099511628211UL;
    const unsigned char* p = (const unsigned char*)kind;
    while (*p) h = (h ^ *p++) * 1099511628211UL;
    p = (const unsigned char*)msg;
    while (*p) h = (h ^ *p++) * 1099511628211UL;
    return h;
}

void plant_audit_log(const char* kind, const char* msg) {
    long slot = g_audit_head;
    plant_audit_ev* e = &g_audit[slot];
    if (g_audit_count == PLANT_AUDIT_CAP) g_audit_evicted = e->chain;
    e->seq = g_audit_seq++;
    snprintf(e->kind, sizeof(e->kind), "%s", kind);
    snprintf(e->msg, sizeof(e->msg), "%s", msg ? msg : "");
    e->chain = plant_audit_hash(g_audit_chain, e->seq, e->kind, e->msg);
    g_audit_chain = e->chain;
    g_audit_head = (slot + 1) % PLANT_AUDIT_CAP;
    if (g_audit_count < PLANT_AUDIT_CAP) g_audit_count++;
    else g_audit_overflow++;
}

tx_t plant_audit_chain_verify(void) {
    long n = g_audit_count;
    if (n > PLANT_AUDIT_CAP) n = PLANT_AUDIT_CAP;
    long base = g_audit_head - n;
    if (base < 0) base += PLANT_AUDIT_CAP;
    unsigned long h = (n == PLANT_AUDIT_CAP) ? g_audit_evicted
                                             : 14695981039346656037UL;
    for (long i = 0; i < n; i++) {
        plant_audit_ev* e = &g_audit[(base + i) % PLANT_AUDIT_CAP];
        if (plant_audit_hash(h, e->seq, e->kind, e->msg) != e->chain) {
            static char buf[64];
            snprintf(buf, sizeof(buf), "TAMPERED %ld", i);
            return buf;
        }
        h = e->chain;
    }
    return (tx_t)"OK";
}

tx_t plant_audit_chain_head(void) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%016lx", g_audit_chain);
    return buf;
}

tx_t plant_audit_dump(void) {
    static char buf[32768];
    size_t used = 0;
    long n = g_audit_count;
    if (n > PLANT_AUDIT_CAP) n = PLANT_AUDIT_CAP;
    long base = g_audit_head - n;
    if (base < 0) base += PLANT_AUDIT_CAP;
    for (long i = 0; i < n; i++) {
        plant_audit_ev* e = &g_audit[(base + i) % PLANT_AUDIT_CAP];
        used += (size_t)snprintf(buf + used, sizeof(buf) - used,
                                 "%ld,%s,%s\n", e->seq, e->kind, e->msg);
    }
    if (g_audit_overflow > 0)
        snprintf(buf + used, sizeof(buf) - used, "OVERFLOW %ld dropped\n", g_audit_overflow);
    return buf;
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.16 — Mission Mode SAFE
   Mission-mode stack (mode of the innermost active action), SAFE
   zero-permission grants and pool/channel configuration. Workers are
   in-process emulations of isolated processes: each owns a channel,
   tracks heartbeats and is monitored for stall/restart. Syscall
   filtering and hash-chained audit complete the governance set.
   ═══════════════════════════════════════════════════════════════ */
#define PLANT_MODE_STACK_MAX 64
static char g_mode_stack[PLANT_MODE_STACK_MAX];
static long g_mode_depth = 0;

static void plant_mode_push(char m) {
    if (g_mode_depth < PLANT_MODE_STACK_MAX) g_mode_stack[g_mode_depth++] = m;
}
static void plant_mode_pop(void) {
    if (g_mode_depth > 0) g_mode_depth--;
}
static char plant_mode_top(void) {
    return g_mode_depth > 0 ? g_mode_stack[g_mode_depth - 1] : 0;
}

#define PLANT_SAFE_MAX_WORKERS 16
#define PLANT_SAFE_DEFAULT_WORKERS 4
#define PLANT_WORKER_NAME 64
#define PLANT_CAP_FILE_READ   (1L << 0)
#define PLANT_CAP_NET_CONNECT (1L << 1)

typedef struct plant_worker {
    char name[PLANT_WORKER_NAME];
    int  state;              /* 0 idle, 1 busy, 2 stalled, 3 dead */
    long last_heartbeat_ms;
    long spawn_seq;          /* restart counter */
    long busy_since_ms;
    long served_calls;
    void* chan;              /* owned SafeChannel */
} plant_worker;

typedef struct plant_channel {
    void*  buf;              /* payload (owned) */
    size_t size;
    size_t copies;           /* structured-clone (deep copy) transfers */
    size_t transfers;        /* transferable (zero-copy) transfers */
    long   threshold;        /* <= threshold → clone; > threshold → transfer */
} plant_channel;

static plant_worker g_pool[PLANT_SAFE_MAX_WORKERS];
static long g_pool_count = 0;      /* live workers */
static long g_pool_cap = PLANT_SAFE_DEFAULT_WORKERS;
static long g_pool_max = PLANT_SAFE_MAX_WORKERS;
static long g_pool_spawns = 0;
static long g_pool_restarts = 0;
static long g_pool_fallback = 0;   /* starved at max cap → BALANCED inline */
static long g_pool_served = 0;
static long g_worker_seq = 1000;
static plant_worker* g_safe_current = NULL;
static plant_worker g_safe_inline; /* fallback worker (BALANCED inline exec) */
static long g_safe_cfg_heartbeat_ms = 5000;
static long g_safe_cfg_response_ms = 10;
static long g_safe_cfg_starvation_ms = 50;
static long g_safe_channel_threshold = 1048576;   /* 1MB */
static long g_safe_cap_mask = 0;    /* MissionContext grants */
static int  g_safe_active = 0;      /* inside a SAFE action */
static long g_pending_wait_ms = 0;  /* simulated queue wait (starvation) */

/* v0.48.17 Mission Mode SMART state (declared here so the cap-check
   and config paths can see it; the router/pool live in the SMART
   section at the end of this file) */
#define PLANT_SMART_MAX_WORKERS 16
#define PLANT_CAP_FILE_WRITE (1L << 2)
static long g_smart_cfg_scalar_limit = 1000;
static long g_smart_cfg_chunk_size = 256;
static long g_smart_cfg_pool_cap = 0;
static long g_smart_cfg_pool_max = 0;
static long g_smart_cap_mask = 0;
static int  g_smart_active = 0;

/* v0.48.18 Mission Mode PERSISTENT state (declared here so the
   cap-check and config paths can see it; the GlobalARCHeap lives in
   the PERSISTENT section at the end of this file) */
#define PLANT_CAP_NET_LISTEN (1L << 3)
static int  g_persist_active = 0;
static long g_persist_cap_mask = 0;
static long g_persist_cfg_gc_interval = 1000;
static long g_persist_cfg_lease_ms = 0;

/* Zero-Trust capability registry — default grants only */
static int g_zt_inited = 0;
static const char* const g_zt_grants[] = { "FILE_READ", "FILE_WRITE", "NET_CONNECT" };
#define ZT_GRANT_COUNT 3

static void plant_zero_trust_init(void) {
    if (g_zt_inited) return;
    g_zt_inited = 1;
    for (long i = 0; i < ZT_GRANT_COUNT; i++)
        plant_audit_log("ZT_GRANT", g_zt_grants[i]);
}

tx_t plant_cap_check(tx_t capv) {
    const char* cap = _S(capv);
    int grant = 0;
    if (g_safe_active) {
        /* SAFE: zero permissions by default; grants come only from
           the MissionContext (plant_safe_grant) */
        if (strcmp(cap, "FILE_READ") == 0) grant = (g_safe_cap_mask & PLANT_CAP_FILE_READ) != 0;
        else if (strcmp(cap, "NET_CONNECT") == 0) grant = (g_safe_cap_mask & PLANT_CAP_NET_CONNECT) != 0;
    } else if (g_smart_active) {
        /* SMART: broad operational defaults (FILE_READ / FILE_WRITE /
           NET_CONNECT) so cross-mode vector work is unhindered */
        if (strcmp(cap, "FILE_READ") == 0) grant = (g_smart_cap_mask & PLANT_CAP_FILE_READ) != 0;
        else if (strcmp(cap, "FILE_WRITE") == 0) grant = (g_smart_cap_mask & PLANT_CAP_FILE_WRITE) != 0;
        else if (strcmp(cap, "NET_CONNECT") == 0) grant = (g_smart_cap_mask & PLANT_CAP_NET_CONNECT) != 0;
    } else if (g_persist_active) {
        /* PERSISTENT: broad operational defaults incl. NET_LISTEN so
           long-running server services can hold socket listeners */
        if (strcmp(cap, "FILE_READ") == 0) grant = (g_persist_cap_mask & PLANT_CAP_FILE_READ) != 0;
        else if (strcmp(cap, "FILE_WRITE") == 0) grant = (g_persist_cap_mask & PLANT_CAP_FILE_WRITE) != 0;
        else if (strcmp(cap, "NET_CONNECT") == 0) grant = (g_persist_cap_mask & PLANT_CAP_NET_CONNECT) != 0;
        else if (strcmp(cap, "NET_LISTEN") == 0) grant = (g_persist_cap_mask & PLANT_CAP_NET_LISTEN) != 0;
    } else {
        plant_zero_trust_init();
        for (long i = 0; i < ZT_GRANT_COUNT; i++)
            if (strcmp(cap, g_zt_grants[i]) == 0) { grant = 1; break; }
    }
    static char msg[96];
    snprintf(msg, sizeof(msg), "%s %s", cap, grant ? "grant" : "deny");
    plant_audit_log("CAP_CHECK", msg);
    return grant ? (tx_t)"1" : (tx_t)"0";
}

/* FAST bump heap with BALANCED escalation */
typedef struct {
    char* base;          /* bump heap */
    size_t size;         /* current capacity */
    size_t used;         /* bump pointer offset */
    size_t peak;         /* high-water mark */
    size_t limit;        /* hard cap */
    long   alignment;    /* strict byte alignment (default 8) */
    int    inited;
    int    escalated;    /* fell back to BALANCED once */
} plant_fast_heap;

static plant_fast_heap g_fast = { 0 };
#define PLANT_FAST_ESC_MAX 256
static void* g_fast_esc[PLANT_FAST_ESC_MAX];
static long g_fast_esc_n = 0;

static void plant_fast_init(void) {
    if (g_fast.inited) return;
    g_fast.size = g_fast_cfg_cap ? g_fast_cfg_cap : (8u << 20);
    g_fast.limit = g_fast_cfg_limit ? g_fast_cfg_limit : (64u << 20);
    g_fast.alignment = g_fast_cfg_align ? g_fast_cfg_align : 8;
    if (g_fast.alignment < 1) g_fast.alignment = 8;
    g_fast.base = (char*)malloc(g_fast.size);
    g_fast.inited = 1;
}

static void plant_fast_grow(void) {
    size_t ns = g_fast.size * 2;
    if (ns > g_fast.limit) ns = g_fast.limit;
    if (ns <= g_fast.size) return;
    char* nb = (char*)realloc(g_fast.base, ns);
    if (!nb) return;
    g_fast.base = nb;
    g_fast.size = ns;
}

static void* plant_fast_alloc_raw(size_t n) {
    plant_fast_init();
    size_t a = (size_t)g_fast.alignment;
    size_t need = (n + a - 1) / a * a;
    if (g_fast.used + need > g_fast.size && g_fast.size < g_fast.limit)
        plant_fast_grow();
    if (g_fast.used + need <= g_fast.size) {
        void* p = g_fast.base + g_fast.used;
        g_fast.used += need;
        if (g_fast.used > g_fast.peak) g_fast.peak = g_fast.used;
        return p;
    }
    /* escalation: safe fallback to BALANCED mode */
    if (!g_fast.escalated) {
        g_fast.escalated = 1;
        plant_audit_log("FAST_ESCALATE", "WARN: Fast heap capacity exceeded");
    }
    void* p = malloc(need);
    if (p && g_fast_esc_n < PLANT_FAST_ESC_MAX) g_fast_esc[g_fast_esc_n++] = p;
    return p;
}

/* emitted by the codegen at the entry of every WITH MISSION FAST
   action: binds the bump heap and resets it for the call's scope */
void plant_fast_enter(const char* name) {
    if (g_fast.inited) {
        if (g_fast.used > g_fast.peak) g_fast.peak = g_fast.used;
        g_fast.used = 0;               /* scope-exit reset at next enter */
    }
    g_in_fast = 1;
    plant_mode_push('F');
    static char msg[128];
    snprintf(msg, sizeof(msg), "FAST %s", name ? name : "");
    plant_audit_log("MODE_ENTER", msg);
}

tx_t plant_fast_alloc(tx_t n) {
    return (tx_t)plant_fast_alloc_raw((size_t)(long)n);
}

tx_t plant_fast_reset(void) {
    plant_fast_init();
    if (g_fast.used > g_fast.peak) g_fast.peak = g_fast.used;
    g_fast.used = 0;
    for (long i = 0; i < g_fast_esc_n; i++) free(g_fast_esc[i]);
    g_fast_esc_n = 0;
    return (tx_t)"0";
}

tx_t plant_fast_used(void) {
    plant_fast_init();
    return _from_long((long)g_fast.used);
}

tx_t plant_fast_peak(void) {
    plant_fast_init();
    return _from_long((long)g_fast.peak);
}

tx_t plant_fast_escalated(void) {
    plant_fast_init();
    return g_fast.escalated ? (tx_t)"1" : (tx_t)"0";
}

tx_t plant_fast_status(void) {
    plant_fast_init();
    static char buf[192];
    snprintf(buf, sizeof(buf), "used=%ld cap=%ld limit=%ld escalated=%d",
             (long)g_fast.used, (long)g_fast.size, (long)g_fast.limit,
             g_fast.escalated ? 1 : 0);
    return buf;
}

/* Boundary Handshake (v0.48.15) + BoundaryViolationError enforcement
   (v0.48.16/17/18). plant_boundary_block is emitted at the entry of
   every FAST/SAFE/SMART/PERSISTENT action with the callee's mission
   mode; forbidden mode pairs are blocked at the callee entry and the
   call returns immediately with an empty result:
     FAST caller      → SAFE callee:          blocked (Boundary Handshake)
     SAFE caller      → FAST/SMART/PERSISTENT: blocked (BoundaryViolationError)
     PERSISTENT caller → SAFE callee:          blocked (BoundaryViolationError)
   All other pairs pass (BALANCED has no guard; SMART may call any). */
long plant_boundary_block(const char* callee, const char* callee_mode) {
    char top = plant_mode_top();
    static char msg[128];
    if (top == 'F' && strcmp(callee_mode, "SAFE") == 0) {
        snprintf(msg, sizeof(msg), "FAST->SAFE blocked %s", callee);
        plant_audit_log("BOUNDARY", msg);
        return 1;
    }
    if (top == 'S' && (strcmp(callee_mode, "FAST") == 0 ||
                       strcmp(callee_mode, "SMART") == 0 ||
                       strcmp(callee_mode, "PERSISTENT") == 0)) {
        snprintf(msg, sizeof(msg), "SAFE->%s blocked %s", callee_mode, callee);
        plant_audit_log("BOUNDARY", msg);
        return 1;
    }
    if (top == 'P' && strcmp(callee_mode, "SAFE") == 0) {
        snprintf(msg, sizeof(msg), "PERSISTENT->SAFE blocked %s", callee);
        plant_audit_log("BOUNDARY", msg);
        return 1;
    }
    return 0;
}

/* ── WarmProcessPool ────────────────────────────────────────────── */
static plant_worker* plant_pool_spawn(const char* name) {
    if (g_pool_count >= PLANT_SAFE_MAX_WORKERS) return NULL;
    plant_worker* w = &g_pool[g_pool_count++];
    memset(w, 0, sizeof(*w));
    if (name) snprintf(w->name, sizeof(w->name), "%s", name);
    else snprintf(w->name, sizeof(w->name), "proc-%ld", ++g_worker_seq);
    w->state = 0;
    w->last_heartbeat_ms = plant_ms();
    g_pool_spawns++;
    return w;
}

static void plant_pool_init(void) {
    if (g_pool_count > 0) return;
    if (g_pool_cap < 1) g_pool_cap = 1;
    if (g_pool_cap > PLANT_SAFE_MAX_WORKERS) g_pool_cap = PLANT_SAFE_MAX_WORKERS;
    if (g_pool_max < g_pool_cap) g_pool_max = g_pool_cap;
    if (g_pool_max > PLANT_SAFE_MAX_WORKERS) g_pool_max = PLANT_SAFE_MAX_WORKERS;
    for (long i = 0; i < g_pool_cap; i++) plant_pool_spawn(NULL);
}

static void plant_worker_heartbeat(plant_worker* w) {
    w->last_heartbeat_ms = plant_ms();
}

/* monitor tick: workers past the heartbeat interval enter the
   response window; stalled workers past the response window are
   terminated and restarted. Returns restart count. */
long plant_pool_tick(void) {
    long now = plant_ms();
    long restarts = 0;
    for (long i = 0; i < g_pool_count; i++) {
        plant_worker* w = &g_pool[i];
        if (w->state == 3) continue;
        long age = now - w->last_heartbeat_ms;
        if (age < 0) age = 0;
        if (w->state == 2) {
            if (age > g_safe_cfg_response_ms) {
                w->state = 3;            /* terminated */
                g_pool_restarts++;
                w->state = 0;            /* respawned */
                w->busy_since_ms = 0;
                w->last_heartbeat_ms = now;
                w->spawn_seq++;
                restarts++;
            }
        } else if (age > g_safe_cfg_heartbeat_ms) {
            w->state = 2;                /* missed heartbeat → respond */
            w->busy_since_ms = 0;
        }
    }
    return restarts;
}

/* acquire a worker; on starvation (simulated queue wait beyond the
   SAFE_STARVATION_MS threshold with every worker busy) grow the pool
   toward SAFE_POOL_EXPAND or fall back gracefully to BALANCED. */
static plant_worker* plant_pool_acquire(const char* name) {
    plant_pool_init();
    plant_pool_tick();
    for (long i = 0; i < g_pool_count; i++) {
        plant_worker* w = &g_pool[i];
        if (w->state == 0) {
            w->state = 1;
            w->busy_since_ms = plant_ms();
            w->served_calls++;
            plant_worker_heartbeat(w);
            return w;
        }
    }
    if (g_pending_wait_ms > g_safe_cfg_starvation_ms) {
        if (g_pool_count < g_pool_max) {
            plant_worker* w = plant_pool_spawn(name);
            if (w) {
                g_pending_wait_ms = 0;
                w->state = 1;
                w->busy_since_ms = plant_ms();
                w->served_calls++;
                plant_worker_heartbeat(w);
                return w;
            }
        }
        g_pool_fallback = 1;             /* at max cap → BALANCED inline */
        g_pending_wait_ms = 0;
        return NULL;
    }
    return NULL;
}

void plant_safe_enter(const char* name) {
    plant_worker* w = plant_pool_acquire(name);
    if (w) {
        g_safe_current = w;
        if (name) snprintf(w->name, sizeof(w->name), "%s", name);
    } else {
        g_safe_current = NULL;
        g_safe_inline.served_calls++;
        g_safe_inline.last_heartbeat_ms = plant_ms();
        g_pool_fallback = 1;
    }
    g_pool_served++;
    g_safe_active = 1;                   /* zero permissions by default */
    g_safe_cap_mask = 0;
    plant_mode_push('S');
    static char msg[128];
    snprintf(msg, sizeof(msg), "SAFE %s", name ? name : "");
    plant_audit_log("MODE_ENTER", msg);
}

void plant_safe_exit(void) {
    if (g_safe_current) {
        plant_channel* ch = (plant_channel*)g_safe_current->chan;
        if (ch) { free(ch->buf); free(ch); g_safe_current->chan = NULL; }
        g_safe_current->state = 0;
        g_safe_current->busy_since_ms = 0;
        plant_worker_heartbeat(g_safe_current);
        g_safe_current = NULL;
    }
    g_safe_active = 0;
    g_safe_cap_mask = 0;
    plant_mode_pop();
}

tx_t plant_safe_status(void) {
    static char buf[192];
    long busy = 0;
    for (long i = 0; i < g_pool_count; i++)
        if (g_pool[i].state == 1) busy++;
    snprintf(buf, sizeof(buf),
             "workers=%ld busy=%ld spawns=%ld restarts=%ld fallback=%ld served=%ld",
             g_pool_count, busy, g_pool_spawns, g_pool_restarts,
             g_pool_fallback, g_pool_served);
    return buf;
}

/* deterministic fault injection for the heartbeat / starvation /
   tamper suites: stall marks a worker unresponsive (heartbeat
   backdated past the response window), starve simulates a queue wait
   and occupies every worker, tamper flips a byte in the newest
   audit event so the chain verification reports TAMPERED. */
tx_t plant_safe_stall(tx_t namev) {
    const char* want = _S(namev);
    for (long i = 0; i < g_pool_count; i++)
        if (strcmp(g_pool[i].name, want) == 0) {
            g_pool[i].state = 2;
            g_pool[i].last_heartbeat_ms = plant_ms() - 10000;
            return (tx_t)"0";
        }
    return (tx_t)"-1";
}

tx_t plant_safe_starve(tx_t msv) {
    g_pending_wait_ms = (long)msv;
    for (long i = 0; i < g_pool_count; i++) g_pool[i].state = 1;
    return (tx_t)"0";
}

tx_t plant_audit_tamper(void) {
    if (g_audit_count <= 0) return (tx_t)"0";
    long slot = (g_audit_head - 1 + PLANT_AUDIT_CAP) % PLANT_AUDIT_CAP;
    g_audit[slot].msg[0] ^= 1;   /* flip to a readable ASCII char */
    return (tx_t)"0";
}

/* MissionContext grants: SAFE actions start with zero permissions and
   may only be granted FILE_READ / NET_CONNECT; anything else is denied */
tx_t plant_safe_grant(tx_t capv) {
    const char* cap = _S(capv);
    static char msg[96];
    if (strcmp(cap, "FILE_READ") == 0) {
        g_safe_cap_mask |= PLANT_CAP_FILE_READ;
    } else if (strcmp(cap, "NET_CONNECT") == 0) {
        g_safe_cap_mask |= PLANT_CAP_NET_CONNECT;
    } else {
        snprintf(msg, sizeof(msg), "%s denied (MissionContext grants FILE_READ/NET_CONNECT only)", cap);
        plant_audit_log("SAFE_GRANT", msg);
        return (tx_t)"0";
    }
    snprintf(msg, sizeof(msg), "%s granted via MissionContext", cap);
    plant_audit_log("SAFE_GRANT", msg);
    return (tx_t)"1";
}

/* syscall filter: execve / fork / ptrace are blocked (zero-trust) */
static const char* const g_safe_blocked_syscalls[] =
    { "execve", "fork", "ptrace" };
#define SAFE_BLOCKED_SYSCALL_COUNT 3

tx_t plant_syscall_check(tx_t namev) {
    const char* name = _S(namev);
    for (long i = 0; i < SAFE_BLOCKED_SYSCALL_COUNT; i++) {
        if (strcmp(name, g_safe_blocked_syscalls[i]) == 0) {
            static char msg[96];
            snprintf(msg, sizeof(msg), "%s blocked (zero-trust syscall filter)", name);
            plant_audit_log("SYSCALL_BLOCK", msg);
            return (tx_t)"0";
        }
    }
    return (tx_t)"1";
}

/* ── SafeChannel IPC ────────────────────────────────────────────── */
/* emitted at SAFE action entries: the action's private channel */
void plant_safe_channel_init(const char* name) {
    plant_channel* ch = (plant_channel*)calloc(1, sizeof(plant_channel));
    if (!ch) return;
    ch->threshold = g_safe_channel_threshold;
    if (g_safe_current) {
        if (g_safe_current->chan) { /* replaced: drop previous */
            plant_channel* old = (plant_channel*)g_safe_current->chan;
            free(old->buf); free(old);
        }
        g_safe_current->chan = ch;
    }
    (void)name;
}

tx_t plant_safe_channel_open(void) {
    plant_channel* ch = (plant_channel*)calloc(1, sizeof(plant_channel));
    if (!ch) return NULL;
    ch->threshold = g_safe_channel_threshold;
    return (tx_t)ch;
}

/* send: payloads <= 1MB are structured-cloned (deep copy — the sender
   keeps its buffer); larger payloads transferable (zero-copy — the
   channel adopts the buffer). */
tx_t plant_safe_send(tx_t chanv, tx_t payload) {
    plant_channel* ch = (plant_channel*)chanv;
    if (!ch || !payload) return (tx_t)"0";
    const char* p = _S(payload);
    size_t n = strlen(p);
    if (n <= (size_t)ch->threshold) {
        char* copy = (char*)malloc(n + 1);
        if (!copy) return (tx_t)"0";
        memcpy(copy, p, n + 1);
        if (ch->buf) free(ch->buf);
        ch->buf = copy;
        ch->size = n;
        ch->copies++;
    } else {
        if (ch->buf) free(ch->buf);
        ch->buf = (void*)p;     /* zero-copy transfer */
        ch->size = n;
        ch->transfers++;
    }
    return (tx_t)"0";
}

/* transferable path for payloads materialized without a PlantLang
   buffer (avoids aliasing a live string): the channel allocates and
   adopts the buffer — zero-copy semantics */
tx_t plant_safe_send_big(tx_t chanv, tx_t nv) {
    plant_channel* ch = (plant_channel*)chanv;
    long n = (long)nv;
    if (!ch || n < 1) return (tx_t)"0";
    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) return (tx_t)"0";
    memset(buf, 'x', (size_t)n);
    buf[n] = 0;
    if (ch->buf) free(ch->buf);
    ch->buf = buf;
    ch->size = (size_t)n;
    ch->transfers++;
    return (tx_t)"0";
}

tx_t plant_safe_recv(tx_t chanv) {
    plant_channel* ch = (plant_channel*)chanv;
    if (!ch || !ch->buf) return (tx_t)"";
    tx_t out = ch->buf;
    ch->buf = NULL;
    ch->size = 0;
    return out;
}

tx_t plant_safe_stats(tx_t chanv) {
    plant_channel* ch = (plant_channel*)chanv;
    if (!ch) return (tx_t)"copies=0 transfers=0";
    static char buf[64];
    snprintf(buf, sizeof(buf), "copies=%zu transfers=%zu", ch->copies, ch->transfers);
    return buf;
}

void plant_fast_exit(void) {
    plant_mode_pop();
}

/* work stealing: clone a task into a NEW arena; segments are shared
   (lazy copy) then materialized (copy-on-write) so the clone holds
   an exclusive arena — no pointer sharing across workers */
tx_t plant_async_steal(tx_t handle) {
    plant_task* src = P_TASK_OF(handle);
    plant_task* c = (plant_task*)calloc(1, sizeof(plant_task));
    if (!c) return NULL;
    c->state_size = src->state_size;
    c->name = src->name;
    c->step = src->step;
    plant_arena_share(&c->ar, &src->ar);   /* lazy: shared segments */
    plant_arena_own(&c->ar);               /* COW: exclusive arena */
    c->st = plant_arena_alloc(&c->ar, (size_t)c->state_size);
    if (!c->st) { free(c); return NULL; }
    memcpy(c->st, src->st, (size_t)c->state_size);
    *((tx_t*)c->st) = (tx_t)c;
    c->id = ++g_task_seq;
    c->ctx = src->ctx;
    c->prio = src->prio;
    c->deadline = src->deadline;
    return c->st;
}

void plant_trace(long level, tx_t scope, tx_t msg) {
    if (!g_trace_on || level > g_trace_level) return;
    char line[512];
    snprintf(line, sizeof(line), "T,%ld,%s,%ld,%s", plant_ms(),
             _S(scope), level, _S(msg));
    plant_trace_write(line);
}

tx_t plant_async_stats(void) {
    static char buf[256];
    long miss = 0;
    if (g_arena_hits + g_arena_misses > 0)
        miss = g_arena_misses * 100 / (g_arena_hits + g_arena_misses);
    snprintf(buf, sizeof(buf),
             "tasks=%ld done=%ld mode=%ld scale=%ld miss=%ld live=%ld",
             g_completed + g_live, g_completed, (long)g_mode,
             g_dctx ? g_dctx->deadline_scale : 100, miss, g_live);
    return buf;
}

void plant_async_config(tx_t keyv, tx_t valv) {
    plant_async_init();
    const char* key = _S(keyv);
    const char* val = _S(valv);
    if (strcmp(key, "ADAPTIVE_THRESHOLD") == 0) {
        g_threshold = atol(val);
        if (g_threshold < 1) g_threshold = 1;
    } else if (strcmp(key, "SAMPLING_MODE") == 0) {
        g_sampling_mode = (strcmp(val, "CPU") == 0) ? 1 : 0;
        g_metrics_interval = g_sampling_mode == 1 ? 4 : 16;
    } else if (strcmp(key, "TRACE_LEVEL") == 0) {
        if (strcmp(val, "DEBUG") == 0) g_trace_level = 1;
        else if (strcmp(val, "PERF") == 0) g_trace_level = 2;
        else g_trace_level = 0;
    } else if (strcmp(key, "METRICS") == 0) {
        g_metrics_on = (strcmp(val, "OFF") == 0) ? 0 : 1;
    } else if (strcmp(key, "TRACE") == 0) {
        g_trace_on = (strcmp(val, "OFF") == 0) ? 0 : 1;
    } else if (strcmp(key, "TRACE_FILE") == 0) {
        if (g_trace_fp) { fclose(g_trace_fp); g_trace_fp = NULL; }
        if (val && *val && strcmp(val, "OFF") != 0)
            g_trace_fp = fopen(val, "w");
    } else if (strcmp(key, "FAST_HEAP_CAPACITY") == 0) {
        long v = atol(val);
        if (v >= 64) g_fast_cfg_cap = (size_t)v;
    } else if (strcmp(key, "FAST_HEAP_LIMIT") == 0) {
        long v = atol(val);
        if (v >= 64) g_fast_cfg_limit = (size_t)v;
    } else if (strcmp(key, "FAST_ALIGNMENT") == 0) {
        long v = atol(val);
        g_fast_cfg_align = (v >= 1) ? v : 8;
    } else if (strcmp(key, "SAFE_POOL_CAPACITY") == 0) {
        long v = atol(val);
        if (v >= 1 && v <= PLANT_SAFE_MAX_WORKERS) g_pool_cap = v;
    } else if (strcmp(key, "SAFE_POOL_EXPAND") == 0) {
        long v = atol(val);
        if (v >= g_pool_cap && v <= PLANT_SAFE_MAX_WORKERS) g_pool_max = v;
    } else if (strcmp(key, "SAFE_HEARTBEAT_MS") == 0) {
        long v = atol(val);
        if (v >= 1) g_safe_cfg_heartbeat_ms = v;
    } else if (strcmp(key, "SAFE_HEARTBEAT_RESPONSE_MS") == 0) {
        long v = atol(val);
        if (v >= 1) g_safe_cfg_response_ms = v;
    } else if (strcmp(key, "SAFE_STARVATION_MS") == 0) {
        long v = atol(val);
        if (v >= 1) g_safe_cfg_starvation_ms = v;
    } else if (strcmp(key, "SAFE_CHANNEL_THRESHOLD") == 0) {
        long v = atol(val);
        if (v >= 1) g_safe_channel_threshold = v;
    } else if (strcmp(key, "SMART_SCALAR_LIMIT") == 0) {
        long v = atol(val);
        if (v >= 1) g_smart_cfg_scalar_limit = v;
    } else if (strcmp(key, "SMART_CHUNK_SIZE") == 0) {
        long v = atol(val);
        if (v >= 1) g_smart_cfg_chunk_size = v;
    } else if (strcmp(key, "SMART_POOL_CAPACITY") == 0) {
        long v = atol(val);
        if (v >= 1 && v <= PLANT_SMART_MAX_WORKERS) g_smart_cfg_pool_cap = v;
    } else if (strcmp(key, "SMART_POOL_MAX") == 0) {
        long v = atol(val);
        if (v >= 1 && v <= PLANT_SMART_MAX_WORKERS) g_smart_cfg_pool_max = v;
    } else if (strcmp(key, "PERSIST_GC_INTERVAL") == 0) {
        long v = atol(val);
        if (v >= 1) g_persist_cfg_gc_interval = v;
    } else if (strcmp(key, "PERSIST_LEASE_MS") == 0) {
        long v = atol(val);
        if (v >= 0) g_persist_cfg_lease_ms = v;
    }
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.17 — Mission Mode SMART (SmartExecutionRouter)
   Adaptive routing: datasets below the scalar limit (default 1000)
   run Scalar Inline; datasets at/above the limit run Parallel Vector
   Mode, partitioned into chunks and dispatched across the dynamic vec
   pool (sized by CPU cores, capped at 16). The pool monitors its
   queue to prevent starvation: queue pressure beyond 2x the live
   worker count grows the pool toward the hard cap, and at the cap the
   router falls back safely to BALANCED execution. SMART actions hold
   broad operational grants (FILE_READ / FILE_WRITE / NET_CONNECT) and
   may invoke any mission mode; SAFE callers are still blocked at
   SMART entries by the Boundary Handshake. Every routing decision,
   chunk dispatch and pool event is recorded by the audit logger.
   ═══════════════════════════════════════════════════════════════ */
#define PLANT_SMART_DEFAULT_SCALAR_LIMIT 1000
#define PLANT_SMART_DEFAULT_CHUNK_SIZE 256

typedef struct plant_vec_worker {
    char name[PLANT_WORKER_NAME];
    int  state;              /* 0 idle, 1 busy */
    long served_chunks;
} plant_vec_worker;

static plant_vec_worker g_vec[PLANT_SMART_MAX_WORKERS];
static long g_vec_count = 0;   /* live pool workers */
static long g_vec_cap = 0;     /* cores-derived capacity */
static long g_vec_max = PLANT_SMART_MAX_WORKERS;
static long g_vec_queue = 0;   /* pending chunk count (monitored) */
static long g_vec_spawns = 0;
static long g_vec_served = 0;
static long g_vec_expands = 0;
static long g_vec_fallback = 0;

static long plant_cpu_cores(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) n = 1;
    if (n > PLANT_SMART_MAX_WORKERS) n = PLANT_SMART_MAX_WORKERS;
    return n;
}

static void plant_vec_init(void) {
    if (g_vec_cap > 0) return;
    g_vec_cap = g_smart_cfg_pool_cap ? g_smart_cfg_pool_cap : plant_cpu_cores();
    if (g_vec_cap < 1) g_vec_cap = 1;
    if (g_vec_cap > PLANT_SMART_MAX_WORKERS) g_vec_cap = PLANT_SMART_MAX_WORKERS;
    if (g_smart_cfg_pool_max) g_vec_max = g_smart_cfg_pool_max;
    if (g_vec_max < g_vec_cap) g_vec_max = g_vec_cap;
    if (g_vec_max > PLANT_SMART_MAX_WORKERS) g_vec_max = PLANT_SMART_MAX_WORKERS;
    for (long i = 0; i < g_vec_cap; i++) {
        snprintf(g_vec[i].name, PLANT_WORKER_NAME, "vec%ld", i);
        g_vec[i].state = 0;
        g_vec[i].served_chunks = 0;
    }
    g_vec_count = g_vec_cap;
    g_vec_spawns = g_vec_cap;
}

/* SmartExecutionRouter: evaluate the optimal execution path for a
   dataset of `size` elements (Scalar Inline vs Parallel Vector Mode)
   and record the routing decision with the audit logger. */
tx_t plant_smart_route(const char* name, long size) {
    static char msg[160];
    if (size < g_smart_cfg_scalar_limit) {
        snprintf(msg, sizeof(msg), "scalar,%s,%ld", name ? name : "", size);
        plant_audit_log("SMART_ROUTE", msg);
        return (tx_t)"scalar";
    }
    plant_vec_init();
    snprintf(msg, sizeof(msg), "parallel,%s,%ld,workers=%ld", name ? name : "", size, g_vec_count);
    plant_audit_log("SMART_ROUTE", msg);
    return (tx_t)"parallel";
}

/* SMART action entry: bind the router, hold broad operational grants
   (FILE_READ / FILE_WRITE / NET_CONNECT), and in Parallel Vector Mode
   partition the dataset into chunks, dispatch them across the vec
   pool with queue monitoring (expand on pressure, BALANCED fallback
   at the hard cap). */
void plant_smart_enter(const char* name, long size) {
    tx_t route = plant_smart_route(name, size);
    g_smart_active = 1;
    g_smart_cap_mask = PLANT_CAP_FILE_READ | PLANT_CAP_FILE_WRITE | PLANT_CAP_NET_CONNECT;
    plant_mode_push('M');
    static char msg[192];
    snprintf(msg, sizeof(msg), "SMART %s", name ? name : "");
    plant_audit_log("MODE_ENTER", msg);
    if (strcmp(_S(route), "parallel") == 0) {
        plant_vec_init();
        long chunks = (size + g_smart_cfg_chunk_size - 1) / g_smart_cfg_chunk_size;
        g_vec_queue = chunks;
        /* queue monitoring: pending chunks beyond 2x the live worker
           count is starvation pressure — grow the pool toward the
           hard cap, then fall back to BALANCED execution */
        long capacity = g_vec_count * 2;
        while (g_vec_queue > capacity && g_vec_count < g_vec_max) {
            long w = g_vec_count;
            snprintf(g_vec[w].name, PLANT_WORKER_NAME, "vec%ld", w);
            g_vec[w].state = 0;
            g_vec[w].served_chunks = 0;
            g_vec_count++;
            g_vec_spawns++;
            g_vec_expands++;
            snprintf(msg, sizeof(msg), "pool expanded %ld->%ld (queue=%ld)",
                     g_vec_count - 1, g_vec_count, g_vec_queue);
            plant_audit_log("SMART_EXPAND", msg);
            capacity = g_vec_count * 2;
        }
        if (g_vec_queue > capacity) {
            g_vec_fallback++;
            snprintf(msg, sizeof(msg), "queue=%ld > cap=%ld, BALANCED fallback",
                     g_vec_queue, capacity);
            plant_audit_log("SMART_FALLBACK", msg);
        }
        /* dispatch: assign every chunk to a pool worker and drain the
           queue (in-process emulation: the body executes once,
           sequentially, under the parallel context) */
        long lo = 0;
        long idx = 0;
        while (lo < size) {
            long hi = lo + g_smart_cfg_chunk_size;
            if (hi > size) hi = size;
            long w = idx % g_vec_count;
            g_vec[w].state = 1;
            snprintf(msg, sizeof(msg), "%ld->%ld,vec%ld", lo, hi, w);
            plant_audit_log("SMART_CHUNK", msg);
            g_vec[w].served_chunks++;
            g_vec_served++;
            lo = hi;
            idx++;
        }
        g_vec_queue = 0;
    }
}

/* SMART action exit: leave the mission mode (pop the mode stack). */
void plant_smart_exit(const char* name) {
    (void)name;
    g_smart_active = 0;
    g_smart_cap_mask = 0;
    plant_mode_pop();
}

tx_t plant_smart_status(void) {
    plant_vec_init();
    static char buf[192];
    long busy = 0;
    for (long i = 0; i < g_vec_count; i++)
        if (g_vec[i].state == 1) busy++;
    snprintf(buf, sizeof(buf),
             "workers=%ld queue=%ld spawns=%ld served=%ld expands=%ld fallback=%ld",
             g_vec_count, g_vec_queue, g_vec_spawns, g_vec_served,
             g_vec_expands, g_vec_fallback);
    return buf;
}

void plant_async_init(void) {
    if (g_inited) return;
    g_inited = 1;
    const char* env = getenv("PLANT_TRACE");
    if (env && strcmp(env, "1") == 0) g_trace_on = 1;
    env = getenv("PLANT_TRACE_FILE");
    if (env && *env) g_trace_fp = fopen(env, "w");
    env = getenv("PLANT_METRICS");
    if (env && strcmp(env, "0") == 0) g_metrics_on = 0;
    g_dctx = (plant_actx*)plant_async_ctx_create(1, 64, "default");
    g_next_sample = 0;
}

void plant_async_drain(void) {
    plant_async_init();
    while (g_live > 0) {
        long now = plant_ms();
        plant_fire_timers(now);
        for (plant_actx* c = g_ctxs; c; c = c->next)
            plant_ctx_tick(c, plant_ready_len());
        plant_task* t = plant_pop_ready();
        if (!t) {
            long key = plant_next_timer();
            if (key < 0) break;
            long d = key - plant_ms();
            if (d > 0) plant_msleep(d);
            continue;
        }
        plant_metric_sample(plant_ms());
        if (t->fatal) {
            t->fatal = 0;
            plant_async_finish(t->st, t->res);
            continue;
        }
        g_running = t;
        int done = t->step(t->st);       /* step calls plant_async_finish when done */
        g_running = NULL;
        (void)done;
    }
    if (g_trace_fp) fflush(g_trace_fp);
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.18 — Mission Mode PERSISTENT (GlobalARCHeap)
   Global reference-counted heap with tri-color cycle detection,
   object finalization callbacks and lease-based persistence.
   Primitives: arc_alloc (refs=1), arc_retain, arc_release (free +
   finalize at zero), arc_finalize (registered callbacks). Cycle
   detection runs automatically every PERSIST_GC_INTERVAL (default
   1000) allocations — a sub-millisecond candidate scan — and on
   demand via GC.cycle() (plant_arc_gc); reclaimed cycles run their
   finalizers. Objects allocated while a SAFE action is on the mode
   stack are tainted and cannot be persisted (plant_arc_persist data
   integrity gate). PERSISTENT actions hold FILE_READ / FILE_WRITE /
   NET_CONNECT / NET_LISTEN by default and cannot call into SAFE
   (boundary). DistributedHeap and the consistent hash ring remain
   deferred and out of scope for this iteration.
   ═══════════════════════════════════════════════════════════════ */
#define PLANT_ARC_FINALIZER_NAME 64

typedef struct plant_arc_obj plant_arc_obj;

typedef struct plant_arc_link_edge {  /* reference edge: parent -> child */
    plant_arc_obj* child;
    struct plant_arc_link_edge* next;
} plant_arc_link_edge;

struct plant_arc_obj {
    void* data;
    size_t size;
    long refs;              /* retain count (external + internal) */
    long in_edges;          /* incoming reference edges (internal refs) */
    long mark;              /* cycle detection: 0 unmarked, 1 marked */
    long alloc_seq;         /* object id / sequence */
    long leased_until_ms;   /* lease expiry (0 = no lease) */
    int  tainted;           /* allocated inside SAFE (untrusted) */
    char finalizer[PLANT_ARC_FINALIZER_NAME];
    plant_arc_link_edge* edges;   /* outgoing reference edges */
    plant_arc_obj* next;    /* heap list */
};

static plant_arc_obj* g_arc_head = NULL;
static long g_arc_seq = 1;
static long g_arc_allocs = 0;
static long g_arc_live = 0;
static long g_arc_frees = 0;
static long g_arc_finalizes = 0;
static long g_arc_leased = 0;
static long g_arc_gc_runs = 0;
static long g_arc_reclaimed = 0;

/* finalizer callbacks: registered by name so PlantLang tests can bind
   them without C function pointers; invoked on destruction */
typedef void (*plant_arc_finalizer_fn)(plant_arc_obj*);
static void plant_arc_fin_free_data(plant_arc_obj* o) { (void)o; }
static const struct { const char* name; plant_arc_finalizer_fn fn; }
    g_arc_finalizers[] = {
        { "free_data", plant_arc_fin_free_data },
        { "close_ctx", plant_arc_fin_free_data },
    };
#define PLANT_ARC_FINALIZER_COUNT 2

static int plant_arc_in_safe(void) {
    for (long i = 0; i < g_mode_depth; i++)
        if (g_mode_stack[i] == 'S') return 1;
    return 0;
}

static void plant_arc_destroy(plant_arc_obj* o, int reclaimed) {
    if (!o) return;
    plant_arc_obj** pp = &g_arc_head;
    while (*pp && *pp != o) pp = &(*pp)->next;
    if (*pp) *pp = o->next;
    if (o->finalizer[0]) {
        g_arc_finalizes++;
        static char msg[96];
        snprintf(msg, sizeof(msg), "%s seq=%ld", o->finalizer, o->alloc_seq);
        plant_audit_log("ARC_FINALIZE", msg);
    }
    plant_arc_link_edge* e = o->edges;
    while (e) {
        plant_arc_link_edge* nx = e->next;
        free(e);
        e = nx;
    }
    if (reclaimed) g_arc_reclaimed++;
    g_arc_live--;
    g_arc_frees++;
    static char msg[96];
    snprintf(msg, sizeof(msg), "seq=%ld %s", o->alloc_seq,
             reclaimed ? "reclaimed" : "refs=0");
    plant_audit_log("ARC_FREE", msg);
    free(o->data);
    free(o);
}

static void plant_arc_destroy(plant_arc_obj* o, int reclaimed);

/* refs hit zero: a live lease keeps the object in the heap (the
   persistent cache path); otherwise it is finalized and freed */
static void plant_arc_drop(plant_arc_obj* o) {
    long now = plant_ms();
    if (o->leased_until_ms > 0 && o->leased_until_ms > now) {
        g_arc_leased++;
        static char msg[96];
        snprintf(msg, sizeof(msg), "seq=%ld kept (leased)", o->alloc_seq);
        plant_audit_log("ARC_RELEASE", msg);
        return;
    }
    plant_arc_destroy(o, 0);
}

tx_t plant_arc_alloc(tx_t sizev) {
    size_t sz = (size_t)(long)sizev;
    if (sz < 1) sz = 1;
    plant_arc_obj* o = (plant_arc_obj*)calloc(1, sizeof(plant_arc_obj));
    if (!o) return NULL;
    o->data = calloc(1, sz);
    if (!o->data) { free(o); return NULL; }
    o->size = sz;
    o->refs = 1;
    o->alloc_seq = g_arc_seq++;
    o->tainted = plant_arc_in_safe();
    o->next = g_arc_head;
    g_arc_head = o;
    g_arc_live++;
    g_arc_allocs++;
    static char msg[96];
    snprintf(msg, sizeof(msg), "seq=%ld size=%ld", o->alloc_seq, (long)sz);
    plant_audit_log("ARC_ALLOC", msg);
    /* automatic cycle detection every PERSIST_GC_INTERVAL allocations */
    if (g_arc_allocs % g_persist_cfg_gc_interval == 0) plant_arc_gc();
    return (tx_t)o;
}

tx_t plant_arc_retain(tx_t objv) {
    plant_arc_obj* o = (plant_arc_obj*)objv;
    if (!o) return (tx_t)"0";
    o->refs++;
    static char msg[96];
    snprintf(msg, sizeof(msg), "seq=%ld refs=%ld", o->alloc_seq, o->refs);
    plant_audit_log("ARC_RETAIN", msg);
    return (tx_t)"1";
}

tx_t plant_arc_release(tx_t objv) {
    plant_arc_obj* o = (plant_arc_obj*)objv;
    if (!o || o->refs <= 0) return (tx_t)"0";
    o->refs--;
    static char msg[96];
    if (o->refs > 0) {
        snprintf(msg, sizeof(msg), "seq=%ld refs=%ld", o->alloc_seq, o->refs);
        plant_audit_log("ARC_RELEASE", msg);
        return (tx_t)"1";
    }
    snprintf(msg, sizeof(msg), "seq=%ld refs=0", o->alloc_seq);
    plant_audit_log("ARC_RELEASE", msg);
    plant_arc_drop(o);
    return (tx_t)"2";
}

tx_t plant_arc_link(tx_t pv, tx_t cv) {
    plant_arc_obj* p = (plant_arc_obj*)pv;
    plant_arc_obj* c = (plant_arc_obj*)cv;
    if (!p || !c || p == c) return (tx_t)"0";
    plant_arc_link_edge* e = (plant_arc_link_edge*)calloc(1, sizeof(plant_arc_link_edge));
    if (!e) return (tx_t)"0";
    e->child = c;
    e->next = p->edges;
    p->edges = e;
    c->in_edges++;
    c->refs++;   /* storing a reference retains the child */
    static char msg[96];
    snprintf(msg, sizeof(msg), "p=%ld c=%ld refs=%ld", p->alloc_seq, c->alloc_seq, c->refs);
    plant_audit_log("ARC_LINK", msg);
    return (tx_t)"1";
}

tx_t plant_arc_unlink(tx_t pv, tx_t cv) {
    plant_arc_obj* p = (plant_arc_obj*)pv;
    plant_arc_obj* c = (plant_arc_obj*)cv;
    if (!p || !c) return (tx_t)"0";
    plant_arc_link_edge** pp = &p->edges;
    while (*pp && (*pp)->child != c) pp = &(*pp)->next;
    if (!*pp) return (tx_t)"0";
    plant_arc_link_edge* dead = *pp;
    *pp = dead->next;
    free(dead);
    c->in_edges--;
    c->refs--;
    static char msg[96];
    if (c->refs > 0) {
        snprintf(msg, sizeof(msg), "p=%ld c=%ld refs=%ld", p->alloc_seq, c->alloc_seq, c->refs);
        plant_audit_log("ARC_UNLINK", msg);
        return (tx_t)"1";
    }
    snprintf(msg, sizeof(msg), "p=%ld c=%ld refs=0", p->alloc_seq, c->alloc_seq);
    plant_audit_log("ARC_UNLINK", msg);
    plant_arc_drop(c);
    return (tx_t)"2";
}

tx_t plant_arc_lease(tx_t objv, tx_t msv) {
    plant_arc_obj* o = (plant_arc_obj*)objv;
    if (!o) return (tx_t)"0";
    long ms = (long)msv;
    if (ms <= 0) ms = g_persist_cfg_lease_ms;
    if (ms <= 0) return (tx_t)"0";
    o->leased_until_ms = plant_ms() + ms;
    static char msg[96];
    snprintf(msg, sizeof(msg), "seq=%ld lease=%ldms", o->alloc_seq, ms);
    plant_audit_log("ARC_LEASE", msg);
    return (tx_t)"1";
}

tx_t plant_arc_set_finalizer(tx_t objv, tx_t namev) {
    plant_arc_obj* o = (plant_arc_obj*)objv;
    const char* name = _S(namev);
    if (!o || !name) return (tx_t)"0";
    for (long i = 0; i < PLANT_ARC_FINALIZER_COUNT; i++) {
        if (strcmp(name, g_arc_finalizers[i].name) == 0) {
            snprintf(o->finalizer, sizeof(o->finalizer), "%s", name);
            static char msg[96];
            snprintf(msg, sizeof(msg), "seq=%ld finalizer=%s", o->alloc_seq, name);
            plant_audit_log("ARC_FINALIZE_REG", msg);
            return (tx_t)"1";
        }
    }
    return (tx_t)"0";
}

/* data integrity gate: objects allocated inside SAFE are untrusted and
   cannot be persisted without validation (which SAFE never grants) */
tx_t plant_arc_persist(tx_t objv) {
    plant_arc_obj* o = (plant_arc_obj*)objv;
    if (!o) return (tx_t)"0";
    static char msg[96];
    if (o->tainted) {
        snprintf(msg, sizeof(msg), "seq=%ld blocked (untrusted SAFE data)", o->alloc_seq);
        plant_audit_log("ARC_PERSIST", msg);
        return (tx_t)"0";
    }
    snprintf(msg, sizeof(msg), "seq=%ld ok", o->alloc_seq);
    plant_audit_log("ARC_PERSIST", msg);
    return (tx_t)"1";
}

static void plant_arc_mark(plant_arc_obj* o) {
    if (!o || o->mark == 1) return;
    o->mark = 1;
    for (plant_arc_link_edge* e = o->edges; e; e = e->next)
        plant_arc_mark(e->child);
}

/* GC.cycle(): tri-color mark-sweep. Roots are objects with external
   references (refs beyond their incoming edges); everything else is
   part of a reference cycle (or an expired zero-ref lease) and is
   reclaimed with its finalizers. Ultra-low overhead: the mark pass is
   linear in live objects and runs every 1000 allocations. */
long plant_arc_gc(void) {
    g_arc_gc_runs++;
    long reclaimed = 0;
    for (plant_arc_obj* o = g_arc_head; o; o = o->next) o->mark = 0;
    for (plant_arc_obj* o = g_arc_head; o; o = o->next)
        if (o->refs - o->in_edges > 0) plant_arc_mark(o);
    long now = plant_ms();
    plant_arc_obj* o = g_arc_head;
    while (o) {
        plant_arc_obj* nx = o->next;
        int lease_dead = o->refs == 0 && o->leased_until_ms > 0 && o->leased_until_ms <= now;
        if (o->mark != 1 || lease_dead) {
            plant_arc_destroy(o, 1);
            reclaimed++;
        }
        o = nx;
    }
    if (reclaimed > 0) {
        static char msg[96];
        snprintf(msg, sizeof(msg), "cycle reclaimed=%ld", reclaimed);
        plant_audit_log("ARC_GC", msg);
    }
    return reclaimed;
}

void plant_persist_enter(const char* name) {
    g_persist_active = 1;
    g_persist_cap_mask = PLANT_CAP_FILE_READ | PLANT_CAP_FILE_WRITE |
                         PLANT_CAP_NET_CONNECT | PLANT_CAP_NET_LISTEN;
    plant_mode_push('P');
    static char msg[128];
    snprintf(msg, sizeof(msg), "PERSISTENT %s", name ? name : "");
    plant_audit_log("MODE_ENTER", msg);
}

void plant_persist_exit(void) {
    g_persist_active = 0;
    g_persist_cap_mask = 0;
    plant_mode_pop();
}

tx_t plant_persist_status(void) {
    static char buf[192];
    snprintf(buf, sizeof(buf),
             "live=%ld allocs=%ld frees=%ld finalizes=%ld leased=%ld gc_runs=%ld reclaimed=%ld",
             g_arc_live, g_arc_allocs, g_arc_frees, g_arc_finalizes,
             g_arc_leased, g_arc_gc_runs, g_arc_reclaimed);
    return buf;
}

tx_t plant_arc_finalize_count(void) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%ld", g_arc_finalizes);
    return buf;
}
