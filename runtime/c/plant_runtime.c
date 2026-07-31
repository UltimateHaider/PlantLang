#include "plant_runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

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
