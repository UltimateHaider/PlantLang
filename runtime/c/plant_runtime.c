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
static void plant_msleep(long ms) {
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

tx_t plant_async_ctx_create(long adaptive, long cap) {
    plant_async_init();
    plant_actx* ctx = (plant_actx*)calloc(1, sizeof(plant_actx));
    if (!ctx) return NULL;
    ctx->magic = 0xA51A4C7;
    ctx->adaptive = (int)adaptive;
    ctx->cap = cap > 0 ? cap : 64;
    ctx->priority = 1;
    ctx->deadline_scale = 100;
    ctx->congested_since = -1;
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
    }
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
    g_dctx = (plant_actx*)plant_async_ctx_create(1, 64);
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
