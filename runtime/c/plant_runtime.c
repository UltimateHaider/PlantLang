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
#include <limits.h>

/* Safe text colorizer utility */
char* plant_colorize(const char* text, const char* color) {
    static char buffer[1024];
    snprintf(buffer, sizeof(buffer), "%s%s%s", color, text, COLOR_RESET);
    return buffer;
}

/* v0.48.37c: cross-TU CLI state — plant_init_cli() runs in the generated
   program's own translation unit (via plant_compat.h), while
   plant_rw_spawn()/plant_maybe_run_worker() live here, so these must be
   real globals defined in this file, not per-TU statics in the header. */
char* g_cli_argv0 = 0;
int g_cli_worker_mode = 0;
static int verify_failures = 0;
static int verify_total = 0;

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

long g_bal_bytes = 0;   /* v0.48.37: BALANCED allocation counter */

/* Heap allocation wrapper */
void* plant_alloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) { plant_error("plant_alloc: out of memory"); }
    g_bal_bytes += (long)size;   /* v0.48.37: BALANCED counter */
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
    if (index < 0 || index >= cap) { char _eb[256]; snprintf(_eb, 256, "plant_array_get: index %lld out of bounds (cap %lld)", (long long)index, (long long)cap); plant_error(_eb); }
    return arr[index + 1];
}

void plant_array_set(int64_t* arr, int64_t index, int64_t value) {
    int64_t cap = arr[0];
    if (index < 0 || index >= cap) { char _eb[256]; snprintf(_eb, 256, "plant_array_set: index %lld out of bounds (cap %lld)", (long long)index, (long long)cap); plant_error(_eb); }
    arr[index + 1] = value;
}

/* ── v0.48.34: shared socket utilities (fd registry, pending reads) ─
   _plant_fd registry tracks fds closed through plant_net_close so a
   double close is a safe no-op instead of re-closing a recycled
   descriptor; entries are forgotten the moment the runtime creates a
   new socket with the same number. _plant_pending holds bytes that
   arrived in the same TCP segment as (but after) a HARVEST MAP-mode
   body read, so a later plant_net_read still sees them in order. */
static int _plant_closed_fds[128];
static int _plant_closed_n = 0;

static int _plant_fd_is_closed(int fd) {
    for (int i = 0; i < _plant_closed_n; i++)
        if (_plant_closed_fds[i] == fd) return 1;
    return 0;
}

static void _plant_fd_remember(int fd) {
    for (int i = 0; i < _plant_closed_n; i++)
        if (_plant_closed_fds[i] == fd) return;
    if (_plant_closed_n >= 128) _plant_closed_n = 127;
    _plant_closed_fds[_plant_closed_n++] = fd;
}

static void _plant_fd_forget(int fd) {
    for (int i = 0; i < _plant_closed_n; i++) {
        if (_plant_closed_fds[i] == fd) {
            for (int j = i; j + 1 < _plant_closed_n; j++)
                _plant_closed_fds[j] = _plant_closed_fds[j + 1];
            _plant_closed_n--;
            return;
        }
    }
}

static void _plant_close_raw(int fd) {
    if (fd < 0 || _plant_fd_is_closed(fd)) return;
    if (close(fd) == 0) _plant_fd_remember(fd);
}

static int _plant_send_all(int fd, const char* data) {
    if (!data) return 1;
    size_t n = strlen(data);
    size_t off = 0;
    while (off < n) {
        ssize_t s = send(fd, data + off, n - off, 0);
        if (s <= 0) return 0;
        off += (size_t)s;
    }
    return 1;
}

static char* _plant_pending_data[64];
static int _plant_pending_fd[64];
static int _plant_pending_n = 0;

static void _plant_pending_store(int fd, const char* data) {
    if (!data || !data[0]) return;
    for (int i = 0; i < _plant_pending_n; i++) {
        if (_plant_pending_fd[i] == fd) {
            char* old = _plant_pending_data[i];
            _plant_pending_data[i] = plant_str_concat(old, data);
            plant_free(old);
            return;
        }
    }
    if (_plant_pending_n >= 64) { plant_free(_plant_pending_data[0]); _plant_pending_n = 0; }
    _plant_pending_fd[_plant_pending_n] = fd;
    _plant_pending_data[_plant_pending_n] = plant_str_concat(data, "");
    _plant_pending_n++;
}

static char* _plant_pending_take(int fd) {
    for (int i = 0; i < _plant_pending_n; i++) {
        if (_plant_pending_fd[i] == fd) {
            char* r = _plant_pending_data[i];
            for (int j = i; j + 1 < _plant_pending_n; j++) {
                _plant_pending_fd[j] = _plant_pending_fd[j + 1];
                _plant_pending_data[j] = _plant_pending_data[j + 1];
            }
            _plant_pending_n--;
            return r;
        }
    }
    return NULL;
}

/* parse a tx_t sock reference (decimal string) into an fd; -2 = invalid */
static int _plant_fd_of(tx_t s) {
    if (!s) return -2;
    const char* d = (const char*)s;
    if (!(d[0] == '-' || (d[0] >= '0' && d[0] <= '9'))) return -2;
    char* end = NULL;
    long v = strtol(d, &end, 10);
    if (end == d) return -2;
    return (int)v;
}

/* ── v0.48.32/34: plant_net_harvest — HARVEST HTTP client ──────
   Low-level socket client (socket/connect/send/recv) targeting the
   standard HTTP (80) / HTTPS (443) ports. Builds an HTTP/1.1
   request with Host + Content-Length, optional custom header MAP
   (pair list) and payload body, plus a Connection: close header in
   the default (non-MAP) mode. Timeout (seconds, 0 → 5s default) is
   enforced via SO_SNDTIMEO/SO_RCVTIMEO so a dead peer cannot hang
   the caller. Returns a response MAP:
     ok      "TRUE" for 2xx responses, else "FALSE"
     status  HTTP status code (string)
     body    response payload
     headers nested MAP of response headers
     sock    (MAP mode only) the live connection's descriptor as a
             decimal string, for plant_net_read/write/close; "-1"
             when the connection could not be kept alive
   Malformed responses and connection failures yield ok "FALSE"
   with status "0" and whatever payload was readable. */
static tx_t _plant_net_harvest_ex(tx_t url, tx_t method, tx_t body, tx_t headers, int64_t timeout, int keep_sock) {
    const char* u = url ? (const char*)url : "";
    PlantArray* out = plant_list_create(8);
    out = plant_list_push(out, strdup("ok"));
    out = plant_list_push(out, strdup("FALSE"));
    out = plant_list_push(out, strdup("status"));
    out = plant_list_push(out, strdup("0"));
    out = plant_list_push(out, strdup("body"));
    out = plant_list_push(out, strdup(""));
    out = plant_list_push(out, strdup("headers"));
    out = plant_list_push(out, (void*)plant_list_create(0));
    if (!u[0]) return (tx_t)out;

    char host[256] = {0};
    char path[2048] = {0};
    int port = 80;
    const char* p = u;
    if (strncmp(p, "https://", 8) == 0) { p += 8; port = 443; }
    else if (strncmp(p, "http://", 7) == 0) p += 7;
    const char* slash = strchr(p, '/');
    const char* h_end = slash ? slash : p + strlen(p);
    size_t hlen = (size_t)(h_end - p);
    if (hlen > 255) hlen = 255;
    memcpy(host, p, hlen);
    host[hlen] = '\0';
    const char* colon = strchr(host, ':');
    if (colon) {
        long cp = strtol(colon + 1, NULL, 10);
        if (cp > 0 && cp < 65536) port = (int)cp;
        host[colon - host] = '\0';
    }
    if (slash) {
        size_t plen = strlen(slash);
        if (plen > 2047) plen = 2047;
        memcpy(path, slash, plen);
        path[plen] = '\0';
    } else {
        strcpy(path, "/");
    }

    char mbuf[16];
    const char* m = method ? (const char*)method : "GET";
    size_t ml = strlen(m);
    if (ml > 15) ml = 15;
    for (size_t i = 0; i < ml; i++) mbuf[i] = (char)toupper((unsigned char)m[i]);
    mbuf[ml] = '\0';

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char sport[8];
    snprintf(sport, sizeof(sport), "%d", port);
    if (getaddrinfo(host, sport, &hints, &res) != 0 || !res)
        return (tx_t)out;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return (tx_t)out; }
    _plant_fd_forget(fd);
    long tv_sec = timeout > 0 ? (long)timeout : 5L;
    struct timeval tv = { .tv_sec = tv_sec, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd); freeaddrinfo(res); return (tx_t)out;
    }
    freeaddrinfo(res);

    char hdr_map[4096];
    hdr_map[0] = '\0';
    if (headers) {
        PlantArray* hm = (PlantArray*)headers;
        if (hm->magic == PLANT_ARRAY_MAGIC) {
            for (int64_t i = 0; i + 1 < hm->count; i += 2) {
                const char* k = (const char*)hm->items[i];
                const char* v = (const char*)hm->items[i + 1];
                if (!k || !v) continue;
                if (strlen(hdr_map) + strlen(k) + strlen(v) + 6 > sizeof(hdr_map) - 4) break;
                strcat(hdr_map, k);
                strcat(hdr_map, ": ");
                strcat(hdr_map, v);
                strcat(hdr_map, "\r\n");
            }
        }
    }
    const char* b = body ? (const char*)body : "";
    char req[8192];
    int n = snprintf(req, sizeof(req),
        "%s %s HTTP/1.1\r\nHost: %s\r\n%s%sContent-Length: %zu\r\n\r\n",
        mbuf, path, host, hdr_map,
        keep_sock ? "" : "Connection: close\r\n",
        strlen(b));
    if (n < 0) { close(fd); return (tx_t)out; }
    _plant_send_all(fd, req);
    if (b[0] && strcmp(mbuf, "POST") == 0)
        _plant_send_all(fd, b);

    char buf[8192];
    char* response = plant_alloc(1);
    response[0] = '\0';
    ssize_t r;
    long cl_need = -1;
    int got_hdr = 0;
    while ((r = recv(fd, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[r] = '\0';
        char* old = response;
        size_t old_len = strlen(old);
        response = plant_alloc(old_len + (size_t)r + 1);
        memcpy(response, old, old_len);
        memcpy(response + old_len, buf, (size_t)r);
        response[old_len + (size_t)r] = '\0';
        plant_free(old);
        if (keep_sock && !got_hdr) {
            const char* sep0 = strstr(response, "\r\n\r\n");
            if (sep0) {
                got_hdr = 1;
                const char* cl0 = strstr(response, "Content-Length:");
                if (cl0 && cl0 < sep0) {
                    cl_need = strtoll(cl0 + 15, NULL, 10);
                    if (cl_need < 0) cl_need = 0;
                } else {
                    cl_need = -2;
                }
            }
        }
        if (keep_sock && got_hdr && cl_need >= 0) {
            const char* sep1 = strstr(response, "\r\n\r\n");
            size_t have = (size_t)(response + strlen(response) - (sep1 + 4));
            if (have >= (size_t)cl_need) break;
        }
        if (strlen(response) > 1048576) break;
    }

    /* MAP mode keeps the connection alive only when the header block
       and a complete Content-Length body arrived; anything else drains
       to EOF/timeout and is closed (sock reported as -1). */
    int sock_report = -1;
    if (keep_sock && got_hdr && cl_need >= 0) {
        const char* sep2 = strstr(response, "\r\n\r\n");
        size_t have = (size_t)(response + strlen(response) - (sep2 + 4));
        if (have >= (size_t)cl_need) {
            sock_report = fd;
            size_t over_len = have - (size_t)cl_need;
            char* over = plant_alloc(over_len + 1);
            memcpy(over, sep2 + 4 + (size_t)cl_need, over_len);
            over[over_len] = '\0';
            if (over_len > 0) _plant_pending_store(fd, over);
            plant_free(over);
        }
    }
    if (!keep_sock || sock_report < 0)
        close(fd);

    const char* sep = strstr(response, "\r\n\r\n");
    const char* head_end = sep ? sep : response + strlen(response);
    const char* resp_body = sep ? sep + 4 : response;

    const char* nl = memchr(response, '\r', (size_t)(head_end - response));
    const char* status_line = response;
    size_t sl_len = nl ? (size_t)(nl - response) : (size_t)(head_end - response);
    char sl[128];
    if (sl_len > 127) sl_len = 127;
    memcpy(sl, status_line, sl_len);
    sl[sl_len] = '\0';
    long status = 0;
    const char* sp = strchr(sl, ' ');
    if (sp) status = strtol(sp + 1, NULL, 10);
    if (status <= 0 || strncmp(sl, "HTTP/", 5) != 0) status = 0;

    char st[16];
    snprintf(st, sizeof(st), "%ld", status);

    PlantArray* hdrs = plant_list_create(4);
    const char* lp = response;
    const char* hend = head_end;
    while (lp < hend) {
        const char* le = memchr(lp, '\r', (size_t)(hend - lp));
        if (!le) break;
        if (le == lp) break;
        size_t ln = (size_t)(le - lp);
        if (ln > 0 && ln < 512) {
            const char* cc = memchr(lp, ':', ln);
            if (cc) {
                char hn[256], hv[512];
                size_t kn = (size_t)(cc - lp);
                if (kn > 255) kn = 255;
                memcpy(hn, lp, kn); hn[kn] = '\0';
                const char* vp = cc + 1;
                while (vp < le && (*vp == ' ' || *vp == '\t')) vp++;
                size_t vn = (size_t)(le - vp);
                if (vn > 511) vn = 511;
                memcpy(hv, vp, vn); hv[vn] = '\0';
                if (kn > 0) {
                    hdrs = plant_list_push(hdrs, strdup(hn));
                    hdrs = plant_list_push(hdrs, strdup(hv));
                }
            }
        }
        lp = le + 2;
        if (lp > hend) break;
    }

    /* MAP mode: body is exactly the Content-Length bytes (anything
       beyond has been stashed for plant_net_read). */
    char* body_out = NULL;
    if (sock_report >= 0 && cl_need > 0) {
        body_out = plant_alloc((size_t)cl_need + 1);
        memcpy(body_out, resp_body, (size_t)cl_need);
        body_out[cl_need] = '\0';
    }
    plant_list_set(out, 1, strdup(status >= 200 && status <= 299 ? "TRUE" : "FALSE"));
    plant_list_set(out, 3, strdup(st));
    plant_list_set(out, 5, strdup(body_out ? body_out : resp_body));
    plant_list_set(out, 7, (void*)hdrs);
    if (keep_sock) {
        char sfd[16];
        snprintf(sfd, sizeof(sfd), "%d", sock_report);
        out = plant_list_push(out, strdup("sock"));
        out = plant_list_push(out, strdup(sfd));
    }
    plant_free(body_out);
    plant_free(response);
    return (tx_t)out;
}

tx_t plant_net_harvest(tx_t url, tx_t method, tx_t body, tx_t headers, int64_t timeout) {
    return _plant_net_harvest_ex(url, method, body, headers, timeout, 0);
}

/* v0.48.34 — HARVEST ... AS resp MAP.: same request/response handling
   as plant_net_harvest but the connection is kept alive and its
   descriptor is exposed in the response MAP under "sock" (decimal
   string) for plant_net_read / plant_net_write / plant_net_close. */
tx_t plant_net_harvest_map(tx_t url, tx_t method, tx_t body, tx_t headers, int64_t timeout) {
    return _plant_net_harvest_ex(url, method, body, headers, timeout, 1);
}

/* v0.49.3 — HARVEST ... AS resp JSON.: run the request as
   plant_net_harvest, then replace the raw "body" string in the
   response MAP with the parsed PlantJson structure (nested access via
   json_get / json_at; json_val for scalar leaves). A body that does
   not parse as JSON is replaced with the empty string (falsy). */
tx_t plant_net_harvest_json(tx_t url, tx_t method, tx_t body, tx_t headers, int64_t timeout) {
    tx_t resp = plant_net_harvest(url, method, body, headers, timeout);
    PlantArray* m = (PlantArray*)resp;
    if (!m || m->magic != PLANT_ARRAY_MAGIC) return resp;
    const char* raw = "";
    for (int64_t i = 0; i + 1 < m->count; i += 2)
        if (m->items[i] && strcmp((const char*)m->items[i], "body") == 0) {
            raw = (const char*)m->items[i + 1];
            break;
        }
    tx_t parsed = json_parse((tx_t)raw);
    for (int64_t i = 0; i + 1 < m->count; i += 2)
        if (m->items[i] && strcmp((const char*)m->items[i], "body") == 0) {
            plant_list_set(m, i + 1, parsed ? parsed : (tx_t)strdup(""));
            break;
        }
    return resp;
}

/* ── v0.48.33: plant_net_listen / plant_net_respond — HTTP server ─
   LISTEN opens a listening socket on the port, waits for ONE client
   connection, reads (5s receive timeout) and parses the HTTP request
   into a request MAP:
     ok      "TRUE" once a connection was accepted, else "FALSE"
     method  HTTP method ("GET", "POST", ...; "" if malformed)
     path    request target ("/foo"; "" if malformed)
     headers nested MAP (pair list) of request headers
     body    request payload (per Content-Length, "" if none)
     sock    decimal-string descriptor consumed by plant_net_respond
   The listening socket is closed right after the accept (single-request
   server). plant_net_respond sends an HTTP/1.1 200 OK response with
   Content-Type: text/plain and Content-Length, then closes the
   connection; a NULL or sock-less request is a safe no-op.
   v0.49.2: plant_net_listen_timeout(port, timeout) sets SO_RCVTIMEO on
   the listening socket, so a client-less accept() fails with EAGAIN
   after `timeout` seconds and the request MAP comes back ok=FALSE. */
static tx_t _plant_net_listen_ex(int64_t port, int64_t timeout) {
    PlantArray* req = plant_list_create(10);
    req = plant_list_push(req, strdup("ok"));
    req = plant_list_push(req, strdup("FALSE"));
    req = plant_list_push(req, strdup("method"));
    req = plant_list_push(req, strdup(""));
    req = plant_list_push(req, strdup("path"));
    req = plant_list_push(req, strdup(""));
    req = plant_list_push(req, strdup("headers"));
    req = plant_list_push(req, (void*)plant_list_create(0));
    req = plant_list_push(req, strdup("body"));
    req = plant_list_push(req, strdup(""));
    int fd = plant_net_listen_open((int)port);
    if (fd < 0) return (tx_t)req;
    if (timeout > 0) {
        struct timeval tv = { .tv_sec = timeout, .tv_usec = 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    int cfd = plant_net_accept(fd);
    _plant_close_raw(fd);
    if (cfd < 0) return (tx_t)req;
    _plant_fd_forget(cfd);
    long tv_sec = 5L;
    struct timeval tv = { .tv_sec = tv_sec, .tv_usec = 0 };
    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char buf[8192];
    char* raw = plant_alloc(1);
    raw[0] = '\0';
    int64_t need = -1;
    for (;;) {
        ssize_t r = recv(cfd, buf, sizeof(buf) - 1, 0);
        if (r <= 0) break;
        buf[r] = '\0';
        char* old = raw;
        size_t ol = strlen(old);
        raw = plant_alloc(ol + (size_t)r + 1);
        memcpy(raw, old, ol);
        memcpy(raw + ol, buf, (size_t)r);
        raw[ol + (size_t)r] = '\0';
        plant_free(old);
        if (ol + (size_t)r > 1048576) break;
        const char* sep = strstr(raw, "\r\n\r\n");
        if (sep) {
            if (need < 0) {
                const char* cl = strstr(raw, "Content-Length:");
                if (cl && cl < sep) {
                    need = strtoll(cl + 15, NULL, 10);
                    if (need < 0) need = 0;
                } else {
                    need = 0;
                }
            }
            size_t got = (size_t)(raw + strlen(raw) - (sep + 4));
            if (got >= (size_t)need) break;
        }
    }

    const char* sep = strstr(raw, "\r\n\r\n");
    const char* resp_body = sep ? sep + 4 : raw + strlen(raw);
    size_t body_len = (size_t)(raw + strlen(raw) - resp_body);
    char* body_copy = plant_alloc(body_len + 1);
    memcpy(body_copy, resp_body, body_len);
    body_copy[body_len] = '\0';

    char* method = plant_alloc(1);
    char* path = plant_alloc(1);
    method[0] = '\0';
    path[0] = '\0';
    const char* line_end = raw + strlen(raw);
    const char* nl = memchr(raw, '\r', strlen(raw));
    if (nl) line_end = nl;
    size_t ll = (size_t)(line_end - raw);
    if (ll > 0 && ll < 2048) {
        char line[2048];
        memcpy(line, raw, ll);
        line[ll] = '\0';
        char* sp1 = strchr(line, ' ');
        char* sp2 = sp1 ? strchr(sp1 + 1, ' ') : NULL;
        if (sp1 && sp2) {
            size_t ml = (size_t)(sp1 - line);
            size_t pl = (size_t)(sp2 - sp1 - 1);
            if (ml > 0 && ml < 64) {
                method = plant_alloc(ml + 1);
                memcpy(method, line, ml);
                method[ml] = '\0';
            }
            if (pl > 0 && pl < 1024) {
                path = plant_alloc(pl + 1);
                memcpy(path, sp1 + 1, pl);
                path[pl] = '\0';
            }
        }
    }

    PlantArray* hdrs = plant_list_create(4);
    const char* lp = raw;
    const char* hend = sep ? sep : raw + strlen(raw);
    while (lp < hend) {
        const char* le = memchr(lp, '\r', (size_t)(hend - lp));
        if (!le) break;
        if (le == lp) break;
        size_t ln = (size_t)(le - lp);
        if (ln > 0 && ln < 512) {
            const char* cc = memchr(lp, ':', ln);
            if (cc) {
                char hn[256], hv[512];
                size_t kn = (size_t)(cc - lp);
                if (kn > 255) kn = 255;
                memcpy(hn, lp, kn); hn[kn] = '\0';
                const char* vp = cc + 1;
                while (vp < le && (*vp == ' ' || *vp == '\t')) vp++;
                size_t vn = (size_t)(le - vp);
                if (vn > 511) vn = 511;
                memcpy(hv, vp, vn); hv[vn] = '\0';
                if (kn > 0) {
                    hdrs = plant_list_push(hdrs, strdup(hn));
                    hdrs = plant_list_push(hdrs, strdup(hv));
                }
            }
        }
        lp = le + 2;
        if (lp > hend) break;
    }

    char sfd[16];
    snprintf(sfd, sizeof(sfd), "%d", cfd);
    req = plant_list_push(req, strdup("sock"));
    req = plant_list_push(req, strdup(sfd));
    plant_list_set(req, 1, strdup("TRUE"));
    plant_list_set(req, 3, strdup(method));
    plant_list_set(req, 5, strdup(path));
    plant_list_set(req, 7, (void*)hdrs);
    plant_list_set(req, 9, strdup(body_copy));
    plant_free(raw);
    plant_free(method);
    plant_free(path);
    plant_free(body_copy);
    return (tx_t)req;
}

tx_t plant_net_listen(int64_t port) {
    return _plant_net_listen_ex(port, 0);
}

tx_t plant_net_listen_timeout(int64_t port, int64_t timeout) {
    return _plant_net_listen_ex(port, timeout);
}

static int _plant_net_respond_ex(tx_t req, tx_t body, const char* ctype) {
    PlantArray* m = (PlantArray*)req;
    if (!m || m->magic != PLANT_ARRAY_MAGIC) return 0;
    int64_t svi = -1;
    for (int64_t i = 0; i + 1 < m->count; i += 2) {
        if (m->items[i] && strcmp((const char*)m->items[i], "sock") == 0) {
            svi = i + 1;
            break;
        }
    }
    if (svi < 0) return 0;
    int fd = (int)strtol((const char*)m->items[svi], NULL, 10);
    const char* b = body ? (const char*)body : "";
    char hdr[256];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
        ctype, strlen(b));
    if (n > 0) _plant_send_all(fd, hdr);
    if (b[0]) _plant_send_all(fd, b);
    _plant_close_raw(fd);
    return 0;
}

tx_t plant_net_respond(tx_t req, tx_t body) {
    return (tx_t)_plant_net_respond_ex(req, body, "text/plain");
}

/* v0.49.3: GIVE ... AS RESPONSE JSON — serialize the body (a PlantJson
   from json_parse, a pair-list MAP, or a plain LIST) and reply with
   Content-Type: application/json. */
tx_t plant_net_respond_json(tx_t req, tx_t body) {
    char* ser = json_stringify(body);
    if (!ser) ser = strdup("null");
    int r = _plant_net_respond_ex(req, (tx_t)ser, "application/json");
    free(ser);
    return (tx_t)r;
}

/* ── v0.41.0: plant_net_listen_open — TCP listener ── */
int64_t plant_net_listen_open(int64_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    _plant_fd_forget(fd);
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

/* ── v0.48.34: plant_net_read / plant_net_write / plant_net_close ─
   Low-level socket utilities callable from the language with a sock
   reference (decimal string, e.g. the "sock" key of a HARVEST ...
   AS MAP response or a LISTEN request). plant_net_read drains the
   pending buffer (bytes that over-read past a Content-Length body),
   then accumulates recv data under a 500ms idle/SO_RCVTIMEO window
   so slow peers cannot hang the caller; closed/invalid/negative
   descriptors yield the empty string. plant_net_write transmits the
   full payload via a send-all loop and reports "TRUE"/"FALSE".
   plant_net_close releases the descriptor through the closed-fd
   registry so double closes are safe no-ops reporting "TRUE". */
tx_t plant_net_read(tx_t fd) {
    int fdx = _plant_fd_of(fd);
    if (fdx < 0 || _plant_fd_is_closed(fdx)) return strdup("");
    char* pend = _plant_pending_take(fdx);
    if (pend) return pend;
    struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };
    setsockopt(fdx, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    char buf[8192];
    char* data = plant_alloc(1);
    data[0] = '\0';
    for (;;) {
        ssize_t r = recv(fdx, buf, sizeof(buf) - 1, 0);
        if (r <= 0) break;
        buf[r] = '\0';
        char* old = data;
        size_t ol = strlen(old);
        data = plant_alloc(ol + (size_t)r + 1);
        memcpy(data, old, ol);
        memcpy(data + ol, buf, (size_t)r);
        data[ol + (size_t)r] = '\0';
        plant_free(old);
        if (ol + (size_t)r > 1048576) break;
    }
    return data;
}

tx_t plant_net_write(tx_t fd, tx_t data) {
    int fdx = _plant_fd_of(fd);
    if (fdx < 0 || _plant_fd_is_closed(fdx)) return strdup("FALSE");
    if (!_plant_send_all(fdx, data ? (const char*)data : ""))
        return strdup("FALSE");
    return strdup("TRUE");
}

tx_t plant_net_close(tx_t fd) {
    int fdx = _plant_fd_of(fd);
    if (fdx < 0) return strdup("TRUE");
    if (_plant_fd_is_closed(fdx)) return strdup("TRUE");
    if (close(fdx) == 0) _plant_fd_remember(fdx);
    return strdup("TRUE");
}

/* ═══════════════════════════════════════════════════════════════
   v0.42.0 — Map Data Structure
   ═══════════════════════════════════════════════════════════════ */

PlantMap* plant_map_hash_create(size_t initial_capacity) {
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
            plant_map_hash_set(map, old_entries[i].key, old_entries[i].value);
            plant_free(old_entries[i].key);
        }
    }
    plant_free(old_entries);
}

void plant_map_hash_set(PlantMap* map, const char* key, void* value) {
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

/* ═══════════════════════════════════════════════════════════════
   v0.49.5 — native map literal API ({k: v})
   plant_map_create() / plant_map_set() build the language's pair-list
   MAP representation (PlantArray, kind = 1) — the same form LINK,
   _map_get, plant_map_to_string, json_stringify and the LISTEN /
   HARVEST request maps all consume. set upserts (an existing key is
   replaced in place, matching plant_link) and returns the map so the
   compiler can chain calls: plant_map_set(plant_map_set(create(), k1,
   v1), k2, v2). Keys are stored as tx_t text; values pass through as
   tx_t (the _from_long wraps already happened at the call site).
   ═══════════════════════════════════════════════════════════════ */

tx_t plant_map_create(void) {
    PlantArray* m = plant_list_create(8);
    m->kind = 1;
    return (tx_t)m;
}

tx_t plant_map_set(tx_t map, tx_t key, tx_t value) {
    PlantArray* m = (PlantArray*)map;
    if (!m || m->magic != PLANT_ARRAY_MAGIC) m = plant_list_create(8);
    m->kind = 1;
    const char* k = key ? _S(key) : "";
    int64_t i = 0;
    for (; i + 1 < m->count; i += 2) {
        const char* ok = (const char*)m->items[i];
        if (ok && strcmp(ok, k) == 0) break;
    }
    if (i + 1 < m->count) m->items[i + 1] = value;
    else {
        m = plant_list_push(m, (void*)k);
        m = plant_list_push(m, value);
        m->kind = 1;
    }
    return (tx_t)m;
}

/* v0.49.11: method-call get — dispatches on representation. The
   language's pair-list MAPs (PlantArray, PLANT_ARRAY_MAGIC — map
   literals, plant_list_make pairs) are linear-scanned ("" when
   missing, matching _map_get); struct/FFI maps (PlantMap hash)
   use the probe below (NULL when missing, preserving the struct
   marshalling contract). */
void* plant_map_get(PlantMap* map, const char* key) {
    if (!map || !key) return NULL;
    if (((PlantArray*)map)->magic == PLANT_ARRAY_MAGIC) {
        PlantArray* m = (PlantArray*)map;
        for (int64_t i = 0; i + 1 < m->count; i += 2) {
            const char* ok = (const char*)m->items[i];
            if (ok && strcmp(ok, key) == 0) return m->items[i + 1];
        }
        return "";
    }
    size_t idx = _plant_hash_str(key) & (map->capacity - 1);
    for (size_t i = 0; i < map->capacity; i++) {
        size_t probe = (idx + i) & (map->capacity - 1);
        if (!map->entries[probe].occupied) return NULL;
        if (strcmp(map->entries[probe].key, key) == 0) return map->entries[probe].value;
    }
    return NULL;
}

int plant_map_has(PlantMap* map, const char* key) {
    if (!map || !key) return 0;
    if (((PlantArray*)map)->magic == PLANT_ARRAY_MAGIC) {
        PlantArray* m = (PlantArray*)map;
        for (int64_t i = 0; i + 1 < m->count; i += 2) {
            const char* ok = (const char*)m->items[i];
            if (ok && strcmp(ok, key) == 0) return 1;
        }
        return 0;
    }
    size_t idx = _plant_hash_str(key) & (map->capacity - 1);
    for (size_t i = 0; i < map->capacity; i++) {
        size_t probe = (idx + i) & (map->capacity - 1);
        if (!map->entries[probe].occupied) return 0;
        if (strcmp(map->entries[probe].key, key) == 0) return 1;
    }
    return 0;
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
/* v0.48.37d — the frame whose SHELTER dispatch is currently executing
   (it is popped from the active stack before handlers run so a handler
   THROW propagates outward). Weather-scoped registration targets this
   frame while set, else the active head, so handler temporaries are
   tracked and purged by the shelter cleanup hooks. */
static PlantWeather* _plant_weather_handling = NULL;
static void plant_weather_teardown(PlantWeather* w);

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

void plant_weather_enter(PlantWeather* w, int storm_handlers) {
    if (!w) return;
    w->next = _plant_weather_head;
    w->raised = 0;
    w->handled = 0;
    w->exc_type = NULL;
    w->exc_msg = NULL;
    w->exc_obj = NULL;
    w->exit_count = 0;
    w->storm_handlers = storm_handlers;
    w->shelter_mark = -1;
    _plant_weather_head = w;
}

/* v0.48.37d — deterministic teardown. Pops the frame and walks the
   block's assigned exit-list, freeing every registered resource
   handle (deferred deallocations first, then live objects; ARC
   objects are destroyed with their edges and bookkeeping, plain
   allocations go through plant_mem_free). The ARC heap's deferred
   deallocation queue (pending_frees) is drained at the same time, so
   protected scopes reclaim systemically on every exit path. */
void plant_weather_leave(PlantWeather* w) {
    if (!w) return;
    if (_plant_weather_head == w) {
        _plant_weather_head = w->next;
    }
    plant_weather_teardown(w);
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
    tx_t obj = pending ? (tx_t)w->exc_obj : NULL;
    plant_weather_leave(w);
    /* v0.48.38a — factory storms propagate as objects: the frame was
       the owner of the reference, so ownership moves outward without
       any retain/release cycle; plant_throw_obj only re-arms the outer
       checkpoint with the same object. */
    if (pending && obj) plant_throw_obj(obj);
    if (pending && !obj) plant_throw(t, m);
}

void plant_throw(const char* type, const char* msg) {
    PlantWeather* w = _plant_weather_head;
    if (msg == NULL) msg = plant_storm_default_message(type);
    if (w == NULL) {
        char _wb[256]; snprintf(_wb, 256, "[WEATHER] unhandled storm: %s %s",
                type ? type : "(none)", msg ? msg : "");
        plant_warning(_wb);
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

/* ================================================================
   v0.49.25 - IMPORT loader (user-level file imports)
   plant_import_load(entry) returns the fully expanded program text:
     - IMPORT "path". lines are replaced by the target file's text
     - paths resolve against the importing file's directory; ".plant"
       is appended when missing; absolute paths pass through
     - each file is expanded exactly once (dedup by canonical path)
     - a reference to a file that is still being expanded (i.e., a
       cycle) yields the sentinel-prefixed diagnostic
         "@@E@@Error: import cycle detected: <path>"
       unreadable files yield "@@E@@Error: cannot read import: <path>"
   ================================================================ */

typedef struct { char** v; long n, cap; } _ImpList;
static void _imp_list_push(_ImpList* l, const char* s) {
    if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 8; l->v = (char**)realloc(l->v, sizeof(char*) * l->cap); }
    l->v[l->n++] = strdup(s);
}
static int _imp_list_has(_ImpList* l, const char* s) {
    for (long i = 0; i < l->n; i++) if (strcmp(l->v[i], s) == 0) return 1;
    return 0;
}
static void _imp_list_remove(_ImpList* l, const char* s) {
    for (long i = 0; i < l->n; i++) if (strcmp(l->v[i], s) == 0) { free(l->v[i]); memmove(&l->v[i], &l->v[i+1], sizeof(char*)*(l->n-i-1)); l->n--; return; }
}

typedef struct { char* buf; size_t len, cap; } _ImpBuf;
static void _imp_buf_add(_ImpBuf* b, const char* s, size_t n) {
    if (b->len + n + 1 > b->cap) { b->cap = (b->len + n + 1) * 2 + 64; b->buf = (char*)realloc(b->buf, b->cap); }
    memcpy(b->buf + b->len, s, n); b->len += n; b->buf[b->len] = '\0';
}

static char* _imp_dirname(const char* p) {
    const char* slash = strrchr(p, '/');
    if (!slash) return strdup("");
    return strndup(p, slash - p);
}

static char* _imp_resolve(const char* dir, const char* rel) {
    char full[4096];
    snprintf(full, sizeof(full), "%s", rel);
    size_t fl = strlen(full);
    const char* base = strrchr(full, '/');
    base = base ? base + 1 : full;
    if (strchr(base, '.') == NULL) snprintf(full + fl, sizeof(full) - fl, ".plant");
    if (dir[0] && full[0] != '/') { char tmp[4600]; snprintf(tmp, sizeof(tmp), "%s/%s", dir, full); snprintf(full, sizeof(full), "%s", tmp); }
    return strdup(full);
}

/* forward decl for mutual recursion */
static int _imp_expand(const char* text, const char* curdir, _ImpList* pending, _ImpList* done, _ImpBuf* out, char* err, size_t errsz);

static int _imp_scan_file(const char* path, _ImpList* pending, _ImpList* done, _ImpBuf* out, char* err, size_t errsz) {
    char* body = plant_file_read(path);
    if (!body) { snprintf(err, errsz, "Error: cannot read import: %s", path); return 0; }
    int ok = _imp_expand(body, _imp_dirname(path), pending, done, out, err, errsz);
    free(body);
    return ok;
}

static int _imp_expand(const char* text, const char* curdir, _ImpList* pending, _ImpList* done, _ImpBuf* out, char* err, size_t errsz) {
    const char* p = text;
    while (*p) {
        const char* eol = strchr(p, '\n');
        size_t linelen = eol ? (size_t)(eol - p) : strlen(p);
        int is_import = (linelen >= 7 && strncmp(p, "IMPORT ", 7) == 0);
        if (is_import) {
            const char* q1 = memchr(p + 7, '"', linelen - 7);
            if (q1) {
                const char* q2 = q1 + 1;
                while (q2 < p + linelen && *q2 != '"') q2++;
                if (q2 < p + linelen) {
                    char rel[1024];
                    size_t rl = (size_t)(q2 - q1 - 1);
                    if (rl >= sizeof(rel)) rl = sizeof(rel) - 1;
                    memcpy(rel, q1 + 1, rl); rel[rl] = '\0';
                    char* full = _imp_resolve(curdir, rel);
                    if (_imp_list_has(done, full)) { free(full); goto emit_line; }
                    if (_imp_list_has(pending, full)) {
                        snprintf(err, errsz, "Error: import cycle detected: %s", full);
                        free(full); return 0;
                    }
                    _imp_list_push(pending, full);
                    int okr = _imp_scan_file(full, pending, done, out, err, errsz);
                    _imp_list_remove(pending, full);
                    if (!okr) { free(full); return 0; }
                    _imp_list_push(done, full);
                    free(full);
                    goto skip_line;   /* import line replaced by file text */
                }
            }
        }
emit_line:
        _imp_buf_add(out, p, linelen);
        _imp_buf_add(out, "\n", 1);
skip_line:
        if (!eol) break;
        p = eol + 1;
    }
    return 1;
}

tx_t plant_species_create(tx_t names) {
    PlantArray* n = (PlantArray*)names;
    tx_t o = plant_map_create();
    if (!n || n->magic != PLANT_ARRAY_MAGIC) return o;
    for (int64_t i = 0; i < n->count; i++) {
        if (!n->items[i]) continue;
        o = plant_map_set(o, n->items[i], "");
    }
    return o;
}

/* v0.49.28 - SPECIES registry */
#define PLANT_SPECIES_MAX 64
static char* g_sp_names[PLANT_SPECIES_MAX];
static tx_t g_sp_fields[PLANT_SPECIES_MAX];
static char* g_sp_parents[PLANT_SPECIES_MAX];
static long g_sp_n = 0;
static char* g_sp_parents[PLANT_SPECIES_MAX];
void plant_species_register(tx_t name, tx_t fields, tx_t parent) {
    if (g_sp_n >= PLANT_SPECIES_MAX) return;
    g_sp_names[g_sp_n] = strdup(_S(name));
    g_sp_fields[g_sp_n] = fields;
    g_sp_parents[g_sp_n] = _S(parent)[0] ? strdup(_S(parent)) : NULL;
    g_sp_n++;
}
static tx_t _sp_build(const char* want) {
    for (long i = 0; i < g_sp_n; i++) {
        if (strcmp(g_sp_names[i], want) != 0) continue;
        tx_t o = plant_map_create();
        if (g_sp_parents[i]) {
            tx_t po = _sp_build(g_sp_parents[i]);
            PlantArray* pa = (PlantArray*)po;
            if (pa && pa->magic == PLANT_ARRAY_MAGIC)
                for (int64_t k = 0; k + 1 < pa->count; k += 2)
                    o = plant_map_set(o, pa->items[k], pa->items[k+1]);
        }
        PlantArray* fa = (PlantArray*)g_sp_fields[i];
        if (fa && fa->magic == PLANT_ARRAY_MAGIC)
            for (int64_t k = 0; k < fa->count; k++)
                o = plant_map_set(o, fa->items[k], "");
        return o;
    }
    return plant_map_create();
}
/* v0.49.33 - interface conformance */
static char* g_impl_sp[PLANT_SPECIES_MAX];
static char* g_impl_if[PLANT_SPECIES_MAX];
static long g_impl_n = 0;
void plant_impl_iface(tx_t sp, tx_t iface) {
    if (g_impl_n >= PLANT_SPECIES_MAX) return;
    g_impl_sp[g_impl_n] = strdup(_S(sp));
    g_impl_if[g_impl_n] = strdup(_S(iface));
    g_impl_n++;
}
tx_t plant_is_a(tx_t obj, tx_t iface_name) {
    PlantArray* m = (PlantArray*)obj;
    if (!m || m->magic != PLANT_ARRAY_MAGIC) return "0";
    const char* sp_name = "";
    for (int64_t k = 0; k + 1 < m->count; k += 2)
        if (strcmp(m->items[k], "__species") == 0) { sp_name = _S(m->items[k+1]); break; }
    if (!sp_name[0]) return "0";
    for (long round = 0; round < 16; round++) {
        for (long i2 = 0; i2 < g_impl_n; i2++) {
            if (strcmp(g_impl_sp[i2], sp_name) == 0 && strcmp(g_impl_if[i2], _S(iface_name)) == 0)
                return "1";
        }
        const char* par = NULL;
        for (long i3 = 0; i3 < g_sp_n; i3++) {
            if (strcmp(g_sp_names[i3], sp_name) == 0) { par = g_sp_parents[i3]; break; }
        }
        if (!par || !par[0]) break;
        sp_name = par;
    }
    return "0";
}
tx_t plant_species_create_by_name(tx_t name) {
    tx_t o = _sp_build(_S(name));
    return plant_map_set(o, "__species", name);
}

long plant_unique_seq(void) {
    static long seq = 0;
    return ++seq;
}

tx_t plant_import_load(tx_t entry) {
    static char errbuf[1024];
    errbuf[0] = '\0';
    _ImpList pending = {0}, done = {0};
    _ImpBuf out = {0};
    const char* ep = _S(entry);
    char* dir = _imp_dirname(ep);
    char* body = plant_file_read(ep);
    if (!body) { free(dir); return strdup("@@E@@Error: cannot read import: entry file"); }
    _imp_list_push(&pending, ep);
    int ok = _imp_expand(body, dir, &pending, &done, &out, errbuf, sizeof(errbuf));
    _imp_list_remove(&pending, ep);
    free(body); free(dir);
    for (long i = 0; i < pending.n; i++) free(pending.v[i]);
    free(pending.v);
    for (long i = 0; i < done.n; i++) free(done.v[i]);
    free(done.v);
    if (!ok) {
        char msg[1200];
        snprintf(msg, sizeof(msg), "@@E@@%s", errbuf);
        free(out.buf);
        return strdup(msg);
    }
    if (!out.buf) return strdup("");
    return out.buf;   /* NUL-terminated by _imp_buf_add */
}

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
    arr->kind = 0;
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
    list->kind = 0;
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

void* plant_list_pop(PlantArray* list) {
    if (!list || list->count == 0) return "";
    return list->items[--list->count];
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

/* ── v0.48.30 — NULL-safe list mutation ───────────────────────
   plant_list_add appends value at the end of the collection; a NULL
   (or otherwise invalid) list reference is instantiated on the fly
   so PUT works against uninitialized targets. plant_list_remove
   locates and removes only the FIRST matching occurrence: string
   elements match by strcmp, container elements (PlantArray magic)
   by pointer identity. NULL lists, empty collections, and absent
   values are safe no-ops returning the list unchanged. */

tx_t plant_list_add(tx_t list, tx_t value) {
    PlantArray* a = (PlantArray*)list;
    if (!a || a->magic != PLANT_ARRAY_MAGIC)
        a = plant_list_create(0);
    return (tx_t)plant_list_push(a, value);
}

tx_t plant_list_remove(tx_t list, tx_t value) {
    PlantArray* a = (PlantArray*)list;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0) return list;
    for (int64_t i = 0; i < a->count; i++) {
        void* el = a->items[i];
        int eq = 0;
        if (el && ((PlantArray*)el)->magic == PLANT_ARRAY_MAGIC) {
            eq = (el == value);
        } else {
            const char* s = (const char*)el;
            const char* v = (const char*)value;
            if (s && v) eq = strcmp(s, v) == 0;
            else if (!s && !v) eq = 1;
        }
        if (eq) {
            memmove(&a->items[i], &a->items[i + 1],
                    (size_t)(a->count - i - 1) * sizeof(void*));
            a->count--;
            break;
        }
    }
    return list;
}

/* ── v0.48.31 — BRAID / LINK ─────────────────────────────────
   plant_braid zips two lists into a fresh pair list
   [k0, v0, k1, v1, …]: when the inputs differ in length only the
   min(countL, countR) leading pairs are produced and excess
   elements of the longer list are safely ignored. NULL/invalid
   inputs yield an empty pair list.
   plant_braid_map builds a map from the same parallel lists: keys
   are the first list, values the second; duplicate keys collapse
   to a single entry with the LAST value (updated in place).
   plant_link upserts into an existing map (pair list): an existing
   key's value is replaced in place, otherwise a new key/value pair
   is appended; a NULL/uninitialized target is instantiated. */

tx_t plant_braid(tx_t left, tx_t right) {
    PlantArray* a = (PlantArray*)left;
    PlantArray* b = (PlantArray*)right;
    PlantArray* out = plant_list_create(0);
    if (!a || a->magic != PLANT_ARRAY_MAGIC || !b || b->magic != PLANT_ARRAY_MAGIC)
        return (tx_t)out;
    int64_t n = a->count < b->count ? a->count : b->count;
    for (int64_t i = 0; i < n; i++) {
        out = plant_list_push(out, a->items[i]);
        out = plant_list_push(out, b->items[i]);
    }
    return (tx_t)out;
}

tx_t plant_braid_map(tx_t left, tx_t right) {
    PlantArray* a = (PlantArray*)left;
    PlantArray* b = (PlantArray*)right;
    PlantArray* out = plant_list_create(0);
    if (!a || a->magic != PLANT_ARRAY_MAGIC || !b || b->magic != PLANT_ARRAY_MAGIC)
        return (tx_t)out;
    int64_t n = a->count < b->count ? a->count : b->count;
    for (int64_t i = 0; i < n; i++) {
        const char* k = (const char*)a->items[i];
        if (!k) continue;
        int64_t j = 0;
        int found = 0;
        for (; j + 1 < out->count; j += 2) {
            const char* ok = (const char*)out->items[j];
            if (ok && strcmp(ok, k) == 0) { found = 1; break; }
        }
        if (found) out->items[j + 1] = b->items[i];
        else {
            out = plant_list_push(out, (void*)k);
            out = plant_list_push(out, b->items[i]);
        }
    }
    return (tx_t)out;
}

tx_t plant_link(tx_t map, tx_t key, tx_t value) {
    PlantArray* m = (PlantArray*)map;
    if (!m || m->magic != PLANT_ARRAY_MAGIC) m = plant_list_create(0);
    const char* k = key ? (const char*)key : "";
    int64_t i = 0;
    for (; i + 1 < m->count; i += 2) {
        const char* ok = (const char*)m->items[i];
        if (ok && strcmp(ok, k) == 0) break;
    }
    if (i + 1 < m->count) m->items[i + 1] = value;
    else {
        m = plant_list_push(m, (void*)k);
        m = plant_list_push(m, value);
    }
    return (tx_t)m;
}

/* ── v0.48.29 — SORT / SHAKE ──────────────────────────────────
   plant_sort: qsort over the list's items. spec is one of
     ""            plain element sort, ascending
     "DESC"        plain element sort, descending
     "f:ASC,g:DESC"  multi-field sort: each comma-separated entry
                     is a field name (for pair-list MAP elements)
                     followed by ":" and ASC or DESC.
   Comparison is numeric-aware: values that fully parse as doubles
   compare numerically (numbers sort before non-numbers), otherwise
   strcmp. Elements that are not maps contribute "" for any field.
   plant_shuffle: Fisher-Yates with rand(), uniform over the
   n! permutations (seeded once from time ^ pid). */

typedef struct SortKey { char field[64]; int desc; } SortKey;
static SortKey g_sort_keys[16];
static int     g_sort_nkeys = 0;
static int     g_sort_desc  = 0;

static int _sort_cmp_vals(const char* a, const char* b) {
    const char* x = a ? a : "", *y = b ? b : "";
    char* ex = NULL, *ey = NULL;
    double dx = strtod(x, &ex), dy = strtod(y, &ey);
    int nx = (ex != x && *ex == '\0'), ny = (ey != y && *ey == '\0');
    if (nx && ny) return dx < dy ? -1 : (dx > dy ? 1 : 0);
    if (nx) return -1;
    if (ny) return 1;
    return strcmp(x, y);
}

static const char* _sort_field_of(tx_t el, const char* key) {
    PlantArray* m = (PlantArray*)el;
    if (!el || m->magic != PLANT_ARRAY_MAGIC) return NULL;
    for (int64_t i = 0; i + 1 < m->count; i += 2) {
        const char* k = (const char*)plant_list_get(m, i);
        if (k && strcmp(k, key) == 0)
            return (const char*)plant_list_get(m, i + 1);
    }
    return NULL;
}

static int _sort_cmp(const void* pa, const void* pb) {
    tx_t a = *(const tx_t*)pa, b = *(const tx_t*)pb;
    if (g_sort_nkeys == 0) {
        int r = _sort_cmp_vals(_S(a), _S(b));
        return g_sort_desc ? -r : r;
    }
    for (int i = 0; i < g_sort_nkeys; i++) {
        const char* va = _sort_field_of(a, g_sort_keys[i].field);
        const char* vb = _sort_field_of(b, g_sort_keys[i].field);
        int r = _sort_cmp_vals(va, vb);
        if (r != 0) return g_sort_keys[i].desc ? -r : r;
    }
    return 0;
}

tx_t plant_sort(tx_t list, tx_t spec) {
    PlantArray* a = (PlantArray*)list;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count < 2) return list;
    const char* s = spec ? _S(spec) : "";
    g_sort_nkeys = 0;
    g_sort_desc = 0;
    if (strcmp(s, "DESC") == 0) {
        g_sort_desc = 1;
    } else if (s[0]) {
        const char* p = s;
        while (*p && g_sort_nkeys < 16) {
            const char* colon = strchr(p, ':');
            if (!colon) break;
            size_t flen = (size_t)(colon - p);
            if (flen > 63) flen = 63;
            memcpy(g_sort_keys[g_sort_nkeys].field, p, flen);
            g_sort_keys[g_sort_nkeys].field[flen] = '\0';
            g_sort_keys[g_sort_nkeys].desc = strncmp(colon + 1, "DESC", 4) == 0;
            g_sort_nkeys++;
            const char* comma = strchr(colon + 1, ',');
            if (!comma) break;
            p = comma + 1;
        }
    }
    qsort(a->items, (size_t)a->count, sizeof(tx_t), _sort_cmp);
    return list;
}

tx_t plant_shuffle(tx_t list) {
    PlantArray* a = (PlantArray*)list;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count < 2) return list;
    static int seeded = 0;
    if (!seeded) { srand((unsigned)(time(NULL) ^ (long)getpid())); seeded = 1; }
    for (int64_t i = a->count - 1; i > 0; i--) {
        int64_t j = rand() % (i + 1);
        tx_t tmp = a->items[i];
        a->items[i] = a->items[j];
        a->items[j] = tmp;
    }
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
        plant_warning("plant_unwrap: called on None/Err");
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

/* v0.49.3: scalar value → JSON text for plain (non-PlantJson)
   serialization: null/true/false/number stay raw, everything else is
   quoted. Shared by the pair-list MAP and plain-LIST array paths. */
static char* _json_scalar_string(const char* vs) {
    if (!vs || !*vs) return strdup("null");
    if (strcmp(vs, "true") == 0 || strcmp(vs, "false") == 0) return strdup(vs);
    char* endp = NULL;
    strtod(vs, &endp);
    if (endp != vs && *endp == 0) return strdup(vs);
    return _json_quote_string(vs);
}

tx_t json_stringify(tx_t val) {
    if (!val) return strdup("null");
    PlantJson* j = (PlantJson*)val;
    if (j->kind >= 0 && j->kind <= 5) return _json_stringify_value(j);
    PlantArray* a = (PlantArray*)val;
    if (a->magic == PLANT_ARRAY_MAGIC) {
        /* v0.49.3: a plain LIST (odd element count) is not a pair-list
           MAP, so serialize it as a JSON array instead of misreading
           every other element as an object key. */
        if ((a->count & 1) == 1) {
            size_t cap = 64, len = 0;
            char* out = (char*)malloc(cap);
            if (!out) return NULL;
            out[len++] = '[';
            for (int64_t i = 0; i < a->count; i++) {
                if (i > 0) { if (len + 2 > cap) { cap *= 2; out = (char*)realloc(out, cap); } out[len++] = ','; }
                char* v = _json_scalar_string((const char*)a->items[i]);
                size_t vl = v ? strlen(v) : 4;
                while (len + vl + 2 > cap) cap *= 2;
                out = (char*)realloc(out, cap);
                if (v) { memcpy(out + len, v, vl); free(v); }
                else { memcpy(out + len, "null", 4); vl = 4; }
                len += vl;
            }
            out[len++] = ']';
            out[len] = 0;
            return out;
        }
        /* defensive: native pair-list MAP of strings → JSON object */
        size_t cap = 64, len = 0;
        char* out = (char*)malloc(cap);
        if (!out) return NULL;
        out[len++] = '{';
        for (int64_t i = 0; i + 1 < a->count; i += 2) {
            if (i > 0) { if (len + 2 > cap) { cap *= 2; out = (char*)realloc(out, cap); } out[len++] = ','; }
            char* k = _json_quote_string((char*)a->items[i]);
            char* v = _json_scalar_string((const char*)a->items[i + 1]);
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

static int _plant_math_num(tx_t x, double* out);
static tx_t _plant_math_result(double v);

tx_t math_min(tx_t a, tx_t b) {
    double da = NAN, db = NAN;
    int su = _plant_math_num(a, &da);
    int sv = _plant_math_num(b, &db);
    if (su && sv) {
        return _plant_math_result(da <= db ? da : db);
    }
    if (su) return _plant_math_result(da);
    if (sv) return _plant_math_result(db);
    return "0";
}

tx_t math_max(tx_t a, tx_t b) {
    double da = NAN, db = NAN;
    int su = _plant_math_num(a, &da);
    int sv = _plant_math_num(b, &db);
    if (su && sv) {
        return _plant_math_result(da >= db ? da : db);
    }
    if (su) return _plant_math_result(da);
    if (sv) return _plant_math_result(db);
    return "0";
}

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
    /* v0.49.19: tagged small ints arrive as raw pointer values —
       _plant_math_num decodes them safely; other payloads keep the
       legacy _num(_S(x)) path (string tx_t). */
    double v = NAN;
    if (!_plant_math_num(x, &v)) v = _num(_S(x));
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

/* ── v0.49.17 Extended math library ──────────────────────────── */
tx_t math_tan(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(tan(v));
}

tx_t math_atan(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(atan(v));
}

tx_t math_cot(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(1.0 / tan(v));
}

tx_t math_asin(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    if (v < -1.0 || v > 1.0) return strdup("-nan");
    return _plant_math_result(asin(v));
}

tx_t math_acos(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    if (v < -1.0 || v > 1.0) return strdup("-nan");
    return _plant_math_result(acos(v));
}

tx_t math_atan2(tx_t x, tx_t y) {
    double a = NAN, b = NAN;
    _plant_math_num(x, &a);
    _plant_math_num(y, &b);
    return _plant_math_result(atan2(a, b));
}

tx_t math_sinh(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(sinh(v));
}

tx_t math_cosh(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(cosh(v));
}

tx_t math_tanh(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(tanh(v));
}

tx_t math_exp(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(exp(v));
}

tx_t math_expm1(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(expm1(v));
}

tx_t math_log10(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    if (v <= 0.0) return strdup("-nan");
    return _plant_math_result(log10(v));
}

tx_t math_log2(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    if (v <= 0.0) return strdup("-nan");
    return _plant_math_result(log2(v));
}

tx_t math_log1p(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    if (v <= -1.0) return strdup("-nan");
    return _plant_math_result(log1p(v));
}

tx_t math_hypot(tx_t x, tx_t y) {
    double a = NAN, b = NAN;
    _plant_math_num(x, &a);
    _plant_math_num(y, &b);
    return _plant_math_result(hypot(a, b));
}

/* ── v0.49.18 Advanced math library ──────────────────────────── */
// Reciprocal trigonometric functions
tx_t math_sec(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    // 1/cos(x) — handle cos(x) == 0 gracefully
    double cv = cos(v);
    if (cv == 0.0) return strdup("-nan");
    return _plant_math_result(1.0 / cv);
}

tx_t math_csc(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    // 1/sin(x) — handle sin(x) == 0 gracefully
    double sv = sin(v);
    if (sv == 0.0) return strdup("-nan");
    return _plant_math_result(1.0 / sv);
}

// Inverse hyperbolic functions
tx_t math_asinh(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(asinh(v));
}

tx_t math_acosh(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    // acosh domain: x >= 1; return -nan otherwise
    if (v < 1.0) return strdup("-nan");
    return _plant_math_result(acosh(v));
}

tx_t math_atanh(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    // atanh domain: |x| < 1; return -nan otherwise
    if (v <= -1.0 || v >= 1.0) return strdup("-nan");
    return _plant_math_result(atanh(v));
}

// Special functions
tx_t math_erf(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(erf(v));
}

tx_t math_erfc(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(erfc(v));
}

// Statistical functions
tx_t math_gamma(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    // gamma domain: x > 0; return -nan otherwise
    if (v <= 0.0) return strdup("-nan");
    return _plant_math_result(tgamma(v));
}

tx_t math_lgamma(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    // lgamma domain: x > 0; return -nan otherwise
    if (v <= 0.0) return strdup("-nan");
    return _plant_math_result(lgamma(v));
}

// Computational utilities
tx_t math_exp2(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(exp2(v));
}

tx_t math_log_base(tx_t x, tx_t b) {
    double a = NAN, c = NAN;
    _plant_math_num(x, &a);
    _plant_math_num(b, &c);
    /* domain: x > 0, b > 0, b != 1 — violations yield "-nan" */
    if (a <= 0.0 || c <= 0.0 || c == 1.0) return strdup("-nan");
    return _plant_math_result(log(a) / log(c));
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
   v0.48.36 — NOW / ANALYZE / TYPEOF introspection & time utils
   Implementations; signatures in plant_compat.h
   ═══════════════════════════════════════════════════════════════ */

/* _plant_val_kind: unified runtime value classification shared by
   plant_typeof and plant_analyze. PlantLang values are untagged
   C pointers, so the kind is inferred structurally: NULL pointers
   and empty strings → "null"; small integer literals (raw long
   bits from numeric expressions, e.g. 42 or a loop counter) →
   "int"; PlantArray* with a "type"/"closure" header → "closure";
   even-length pair lists → "map"; odd-length lists → "list";
   pointers registered by plant_env_alloc → "closure" (boxed
   closure environments); numeric text → "int"; anything else →
   "string". */
static PlantArray* _env_registry = NULL;

void plant_env_register(void* p) {
    if (!p) return;
    if (!_env_registry) _env_registry = plant_list_create(64);
    _env_registry = plant_list_add(_env_registry, (tx_t)p);
}

static int _is_env_ptr(void* p) {
    if (!_env_registry) return 0;
    for (int64_t i = 0; i < _env_registry->count; i++)
        if (_env_registry->items[i] == p) return 1;
    return 0;
}

static tx_t _plant_val_kind(tx_t v) {
    if (!v) return strdup("null");
    if ((uintptr_t)v < 4096) return strdup("int");
    PlantArray* p = (PlantArray*)v;
    if (p->magic == PLANT_ARRAY_MAGIC) {
        if (p->count >= 2 && p->items[0] && strcmp(_S(p->items[0]), "type") == 0
            && p->items[1] && strcmp(_S(p->items[1]), "closure") == 0)
            return strdup("closure");
        if (p->kind == 1) return strdup("map");
        return strdup("list");
    }
    if (_is_env_ptr(v)) return strdup("closure");
    const char* s = _S(v);
    if (!s || s[0] == 0) return strdup("null");
    const char* q = s;
    if (*q == '-') q++;
    if (*q == 0) return strdup("string");
    int dot = 0;
    for (; *q; q++) {
        if (*q == '.') { if (dot) return strdup("string"); dot = 1; continue; }
        if (*q < '0' || *q > '9') return strdup("string");
    }
    return strdup("int");
}

/* plant_now: temporal query router. "DATE" → YYYY-MM-DD, "TIME" →
   HH:MM:SS, "YEAR" → YYYY, "STAMP" (and the bare "" default) →
   epoch seconds; any other format name is reported verbatim as
   "bad-format:<name>" so unsupported formats stay deterministic. */
tx_t plant_now(tx_t format) {
    const char* fmt = _S(format);
    if (!fmt) fmt = "";
    if (strcmp(fmt, "DATE") == 0) return time_format(time_now(), "%Y-%m-%d");
    if (strcmp(fmt, "TIME") == 0) return time_format(time_now(), "%H:%M:%S");
    if (strcmp(fmt, "YEAR") == 0) return time_format(time_now(), "%Y");
    if (strcmp(fmt, "STAMP") == 0 || strcmp(fmt, "") == 0) return time_now();
    char b[64];
    snprintf(b, sizeof(b), "bad-format:%s", fmt);
    return strdup(b);
}

/* plant_analyze: structural introspection returning a uniform MAP
   {type, size, keys}: type from _plant_val_kind; size is the byte
   length for scalars, the pair count for maps, the element count
   for lists (and the node entry count for closures); keys is the
   flat key list for maps, the element list for lists, and an empty
   list for scalars/closures. NULL/empty targets → type null, size
   0, keys []. */
tx_t plant_analyze(tx_t v) {
    tx_t kind = _plant_val_kind(v);
    char sb[64];
    PlantArray* keys = plant_list_make(0);
    if (strcmp(_S(kind), "null") == 0) {
        snprintf(sb, sizeof(sb), "0");
    } else if (strcmp(_S(kind), "int") == 0 || strcmp(_S(kind), "string") == 0) {
        const char* s = _S(v);
        snprintf(sb, sizeof(sb), "%zu", strlen(s ? s : ""));
    } else {
        PlantArray* p = (PlantArray*)v;
        if (p && p->magic == PLANT_ARRAY_MAGIC) {
            if (strcmp(_S(kind), "map") == 0) {
                snprintf(sb, sizeof(sb), "%lld", (long long)(p->count / 2));
                for (int64_t i = 0; i + 1 < p->count; i += 2)
                    keys = plant_list_add(keys, p->items[i] ? _S(p->items[i]) : "");
            } else if (strcmp(_S(kind), "closure") == 0) {
                snprintf(sb, sizeof(sb), "%lld", (long long)p->count);
            } else {
                snprintf(sb, sizeof(sb), "%lld", (long long)p->count);
                for (int64_t i = 0; i < p->count; i++)
                    keys = plant_list_add(keys, p->items[i] ? _S(p->items[i]) : "");
            }
        } else {
            /* boxed closure environment: opaque, no element count */
            snprintf(sb, sizeof(sb), "0");
        }
    }
    PlantArray* _analyze_meta = plant_list_make(6, "type", _S(kind), "size", strdup(sb), "keys", keys);
    _analyze_meta->kind = 1;
    return (tx_t)_analyze_meta;
}

/* plant_typeof: type string only — thin wrapper over the shared
   classification. */
tx_t plant_typeof(tx_t v) {
    return _plant_val_kind(v);
}

/* _plant_ser: recursive map/list serializer used by
   plant_map_to_string. Maps render "{k=v, k2=v2}", lists render
   "[e1, e2]" and nested containers recurse; a depth cap of 8 keeps
   accidental cycles from hanging the program. */
static tx_t _plant_ser(tx_t v, int depth) {
    if (depth > 8) return strdup("...");
    if (!v) return strdup("null");
    PlantArray* p = (PlantArray*)v;
    if (p->magic != PLANT_ARRAY_MAGIC) {
        const char* s = _S(v);
        return strdup(s ? s : "");
    }
    if (p->count == 0) return strdup(p->kind == 1 ? "{}" : "[]");
    int is_map = (p->kind == 1);
    tx_t res = strdup(is_map ? "{" : "[");
    for (int64_t i = 0; i < p->count; i++) {
        if (is_map && i % 2 == 0) {
            if (i > 0) res = _cat(res, ", ");
            res = _cat(res, p->items[i] ? _S(p->items[i]) : "null");
            res = _cat(res, " = ");
        } else {
            if (!is_map && i > 0) res = _cat(res, ", ");
            tx_t sv = p->items[i] ? _plant_ser(p->items[i], depth + 1) : strdup("null");
            res = _cat(res, _S(sv));
        }
    }
    res = _cat(res, is_map ? "}" : "]");
    return res;
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.38c — JOIN(list, delim) built-in
   Concatenates the elements of a list into one string separated by
   delim. An empty (or NULL) list yields ""; a NULL delim is treated
   as "". Element conversion: tx_t values are strings in this model —
   the NUM/SCL/FACT casts (the directive's _from_long/_from_double/
   TRUE-FALSE translation) already happened at the call site, so
   numeric and boolean elements arrive pre-converted and pass through
   unchanged. Nested PlantArray elements (MAP/LIST) are serialized
   through plant_map_to_string, the runtime's object serializer (a
   standalone plant_to_string does not exist, so the serializer
   stands in; NULL elements render as "").
   ═══════════════════════════════════════════════════════════════ */

tx_t plant_join(tx_t list, tx_t delim) {
    PlantArray* a = (PlantArray*)list;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0) return strdup("");
    const char* d = delim ? _S(delim) : "";
    tx_t res = strdup("");
    for (int64_t i = 0; i < a->count; i++) {
        if (i > 0) res = _cat(res, d);
        tx_t el = a->items[i];
        if (el && ((PlantArray*)el)->magic == PLANT_ARRAY_MAGIC) {
            res = _cat(res, _S(plant_map_to_string(el)));
        } else {
            const char* s = el ? _S(el) : "";
            res = _cat(res, s);
        }
    }
    return res;
}

tx_t plant_map_to_string(tx_t v) {
    return _plant_ser(v, 0);
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.38d — FIRST / LAST / SUM list operations
   plant_first / plant_last return the boundary element of a list;
   empty (or NULL / non-array) lists yield "". plant_sum aggregates
   the numeric elements of a list: NUM/SCL elements arrive as
   pre-converted tx_t text (_from_long/_from_double casts happen at
   the call site), parsable strings ("2", "2.5") are converted with
   a full-consumption strtod scan, and everything else — non-
   parsable strings ("a", "TRUE" from bare booleans inside list
   literals), nested MAP/LIST containers, NULLs, empty strings — is
   skipped without interrupting the accumulation. An empty (or
   NULL) list sums to "0". The result is "%.10g"-formatted unless
   integral, when it renders as a long integer.
   ═══════════════════════════════════════════════════════════════ */

tx_t plant_first(tx_t list) {
    PlantArray* a = (PlantArray*)list;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0) return strdup("");
    return a->items[0] ? a->items[0] : strdup("");
}

tx_t plant_last(tx_t list) {
    PlantArray* a = (PlantArray*)list;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0) return strdup("");
    return a->items[a->count - 1] ? a->items[a->count - 1] : strdup("");
}

tx_t plant_sum(tx_t list) {
    PlantArray* a = (PlantArray*)list;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0) return strdup("0");
    double acc = 0.0;
    for (int64_t i = 0; i < a->count; i++) {
        tx_t el = a->items[i];
        if (!el) continue;
        if ((uintptr_t)el < 4096) { acc += (double)(intptr_t)el; continue; }
        if (((PlantArray*)el)->magic == PLANT_ARRAY_MAGIC) continue; /* MAP/LIST */
        const char* s = _S(el);
        if (!s || s[0] == '\0') continue;
        char* end = NULL;
        double v = strtod(s, &end);
        if (end == s || *end != '\0') continue; /* non-parsable: skip */
        acc += v;
    }
    if (acc == (double)(long long)acc) return _from_long((long long)acc);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.10g", acc);
    return strdup(buf);
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.38e — UPPER / LOWER string case operations
   ASCII-safe case conversion: each character is cast to unsigned
   char before toupper/tolower, avoiding undefined behavior for
   high-bit bytes on platforms with signed char. NULL and empty
   inputs return "".
   ═══════════════════════════════════════════════════════════════ */

tx_t plant_upper(tx_t text) {
    const char* s = text ? _S(text) : "";
    if (!s || s[0] == '\0') return strdup("");
    size_t len = strlen(s);
    char* out = (char*)plant_alloc(len + 1);
    for (size_t i = 0; i < len; i++) out[i] = (char)toupper((unsigned char)s[i]);
    out[len] = '\0';
    return out;
}

tx_t plant_lower(tx_t text) {
    const char* s = text ? _S(text) : "";
    if (!s || s[0] == '\0') return strdup("");
    size_t len = strlen(s);
    char* out = (char*)plant_alloc(len + 1);
    for (size_t i = 0; i < len; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[len] = '\0';
    return out;
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.38e (extension) — TRIM / REVERSE string utilities
   plant_trim strips ' ', '\t', '\n', '\r' from both boundaries of
   the input (an all-whitespace or empty/NULL input yields "");
   plant_reverse writes the characters into a fresh buffer in
   reverse index order ("" for empty/NULL inputs). Both allocate
   through the ARC/arena framework (plant_alloc).
   ═══════════════════════════════════════════════════════════════ */

static int _plant_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

tx_t plant_trim(tx_t text) {
    const char* s = text ? _S(text) : "";
    if (!s || s[0] == '\0') return strdup("");
    size_t len = strlen(s);
    size_t start = 0;
    while (start < len && _plant_ws(s[start])) start++;
    size_t end = len;
    while (end > start && _plant_ws(s[end - 1])) end--;
    if (start >= end) return strdup("");
    char* out = (char*)plant_alloc(end - start + 1);
    memcpy(out, s + start, end - start);
    out[end - start] = '\0';
    return out;
}

tx_t plant_reverse(tx_t text) {
    const char* s = text ? _S(text) : "";
    if (!s || s[0] == '\0') return strdup("");
    size_t len = strlen(s);
    char* out = (char*)plant_alloc(len + 1);
    for (size_t i = 0; i < len; i++) out[i] = s[len - 1 - i];
    out[len] = '\0';
    return out;
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.38f — Math built-ins
   ABS / ROUND / POW / CEIL / FLOOR / RANDOM / SIN / COS / SQRT.
   tx_t operands are coerced to double: small raw integers (both
   signs — call sites pass integer literals unwrapped, e.g.
   plant_abs(-5)), numeric strings ("2", "3.7", pre-converted NUM
   variables via _from_long) parse with a full-consumption strtod
   scan. Unparseable inputs coerce to NaN. Results render as a long
   integer when integral, otherwise "%.10g" ("nan" for NaN).
   ═══════════════════════════════════════════════════════════════ */

static int _plant_math_num(tx_t x, double* out) {
    /* small raw ints first — 0 is also the NULL sentinel */
    if ((intptr_t)x > -4096 && (intptr_t)x < 4096) { *out = (double)(intptr_t)x; return 1; }
    if (!x) return 0;
    const char* s = _S(x);
    if (!s || s[0] == '\0') return 0;
    char* end = NULL;
    double v = strtod(s, &end);
    if (end == s || *end != '\0') return 0;
    *out = v;
    return 1;
}

static tx_t _plant_math_result(double v) {
    if (v == (double)(long long)v) return _from_long((long long)v);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.10g", v);
    return strdup(buf);
}

tx_t plant_abs(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(fabs(v));
}

tx_t plant_round(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(round(v));
}

tx_t plant_pow(tx_t x, tx_t y) {
    double a = NAN, b = NAN;
    _plant_math_num(x, &a);
    _plant_math_num(y, &b);
    return _plant_math_result(pow(a, b));
}

tx_t plant_ceil(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(ceil(v));
}

tx_t plant_floor(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(floor(v));
}

tx_t plant_random(void) {
    /* [0.0, 1.0) — the +1.0 keeps the upper bound exclusive */
    return _plant_math_result((double)rand() / ((double)RAND_MAX + 1.0));
}

tx_t plant_sin(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(sin(v));
}

tx_t plant_cos(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    return _plant_math_result(cos(v));
}

tx_t plant_sqrt(tx_t x) {
    double v = NAN;
    _plant_math_num(x, &v);
    if (v < 0.0) return strdup("nan");
    return _plant_math_result(sqrt(v));
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.38g — Conditional list built-ins: HAS / ANY / ALL
   plant_has(list, value) reports "1" when value is present (both
   sides are canonicalized to text — raw small integers convert via
   _from_long so HAS([1, 2, 3], 2) matches the "2" element). ANY /
   ALL evaluate a runtime condition string ("<op> <num>", op one of
   > < >= <= == != =) against each numeric element; elements that
   do not coerce to a number simply fail the predicate. ANY returns
   "0" for empty lists; ALL is vacuously "1" for empty lists.
   ═══════════════════════════════════════════════════════════════ */

static const char* _plant_el_text(tx_t el, char* buf) {
    if ((intptr_t)el > -4096 && (intptr_t)el < 4096) {
        snprintf(buf, 32, "%ld", (long)(intptr_t)el);
        return buf;
    }
    const char* s = el ? _S(el) : "";
    return s ? s : "";
}

tx_t plant_has(tx_t list, tx_t value) {
    PlantArray* a = (PlantArray*)list;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0) return strdup("0");
    char vb[32];
    const char* v = _plant_el_text(value, vb);
    for (int64_t i = 0; i < a->count; i++) {
        char eb[32];
        const char* e = _plant_el_text(a->items[i], eb);
        if (strcmp(e, v) == 0) return strdup("1");
    }
    return strdup("0");
}

/* _plant_cond_match: evaluate "el <op> target" from a cond string
   like "> 2" or "<= 0.5"; malformed conds match nothing. */
static int _plant_cond_match(double el, const char* cond) {
    if (!cond) return 0;
    while (*cond == ' ' || *cond == '\t') cond++;
    char op[4] = "";
    int oi = 0;
    while (*cond && *cond != ' ' && *cond != '\t' && oi < 3) {
        op[oi++] = *cond++;
    }
    while (*cond == ' ' || *cond == '\t') cond++;
    char* end = NULL;
    double target = strtod(cond, &end);
    if (end == cond) return 0;
    if (strcmp(op, ">") == 0) return el > target;
    if (strcmp(op, "<") == 0) return el < target;
    if (strcmp(op, ">=") == 0) return el >= target;
    if (strcmp(op, "<=") == 0) return el <= target;
    if (strcmp(op, "==") == 0 || strcmp(op, "=") == 0) return el == target;
    if (strcmp(op, "!=") == 0) return el != target;
    return 0;
}

tx_t plant_any(tx_t list, tx_t cond) {
    PlantArray* a = (PlantArray*)list;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0) return strdup("0");
    const char* cs = cond ? _S(cond) : "";
    for (int64_t i = 0; i < a->count; i++) {
        double elv;
        if (_plant_math_num(a->items[i], &elv) && _plant_cond_match(elv, cs))
            return strdup("1");
    }
    return strdup("0");
}

tx_t plant_all(tx_t list, tx_t cond) {
    PlantArray* a = (PlantArray*)list;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0) return strdup("1");
    const char* cs = cond ? _S(cond) : "";
    for (int64_t i = 0; i < a->count; i++) {
        double elv;
        if (!_plant_math_num(a->items[i], &elv) || !_plant_cond_match(elv, cs))
            return strdup("0");
    }
    return strdup("1");
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.38h — Ternary built-in: PICK(cond, true_val, false_val)
   cond is truthy when it is a nonzero raw small integer literal
   (the small-int check must precede the NULL guard — literal 0 is
   indistinguishable from the NULL sentinel and falls through to the
   text path) or a non-empty string other than "0"/"false"/"FALSE".
   Returned values are canonicalized like list elements: raw small
   integers render as decimal text via _from_long, everything else
   stringifies.
   ═══════════════════════════════════════════════════════════════ */

static tx_t _plant_pick_ret(tx_t v) {
    if ((intptr_t)v > -4096 && (intptr_t)v < 4096) {
        char buf[32];
        snprintf(buf, 32, "%ld", (long)(intptr_t)v);
        return strdup(buf);
    }
    const char* s = v ? _S(v) : "";
    return strdup(s ? s : "");
}

tx_t plant_pick(tx_t cond, tx_t true_val, tx_t false_val) {
    if (cond != NULL && (intptr_t)cond > -4096 && (intptr_t)cond < 4096)
        return _plant_pick_ret(true_val);
    const char* c = cond ? _S(cond) : "";
    int truthy = c && *c != '\0' && strcmp(c, "0") != 0 &&
                 strcmp(c, "false") != 0 && strcmp(c, "FALSE") != 0;
    return _plant_pick_ret(truthy ? true_val : false_val);
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.38j — String analysis built-ins: FIND / COUNT_OF
   plant_find(text, sub) returns the 0-based index of the first
   occurrence of sub inside text ("0" for an empty sub, "-1" when
   text is empty or the sub is absent). plant_count_of(text, sub)
   counts non-overlapping occurrences via strstr with
   pos + strlen(sub) pointer advancement ("0" when either argument
   is empty). Results are returned as text via _from_long.
   ═══════════════════════════════════════════════════════════════ */

tx_t plant_find(tx_t text, tx_t sub) {
    const char* t = text ? _S(text) : "";
    if (!t || !*t) return strdup("-1");
    const char* s = sub ? _S(sub) : "";
    if (!s || !*s) return strdup("0");
    const char* p = strstr(t, s);
    if (!p) return strdup("-1");
    return _from_long((long)(p - t));
}

tx_t plant_count_of(tx_t text, tx_t sub) {
    const char* t = text ? _S(text) : "";
    if (!t || !*t) return strdup("0");
    const char* s = sub ? _S(sub) : "";
    if (!s || !*s) return strdup("0");
    long n = 0;
    const char* p = t;
    size_t slen = strlen(s);
    while ((p = strstr(p, s)) != NULL) {
        n++;
        p += slen;
    }
    return _from_long(n);
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.38i — Universal sequence slicing: SLICE(data, start, end)
   Slices either a string (tx_t) or a list (PlantArray, auto-
   dispatched via the magic tag) over the half-open range
   [start, end). Arguments accept raw small-integer literals (the
   literal 0 is indistinguishable from the NULL sentinel and is a
   real index, so the small-int check precedes everything else) or
   numeric strings. "Not given" arguments (NULL / empty /
   unparseable) default to 0 for start and the sequence length for
   end. A -1 bound is the "expansion" marker per the specification:
   start = -1 defaults to the beginning of the sequence, end = -1
   extends the slice to the end of the sequence. Other negative
   indices resolve relative to the length (length + index). Bounds
   clamp to [0, length]; end < start yields an empty result. List
   elements are canonicalized to text (raw small integers become
   decimal strings) so the result prints safely.
   ═══════════════════════════════════════════════════════════════ */

static long _slice_arg(tx_t v) {
    if ((intptr_t)v > -4096 && (intptr_t)v < 4096)
        return (long)(intptr_t)v;
    const char* s = v ? _S(v) : NULL;
    if (!s || !*s) return LONG_MIN;
    char* e = NULL;
    double d = strtod(s, &e);
    if (e != s && *e == '\0') return (long)d;
    return LONG_MIN;
}

static void _slice_resolve(long* s, long* e, long len) {
    if (*s == LONG_MIN || *s == -1) *s = 0;
    if (*e == LONG_MIN || *e == -1) *e = len;
    if (*s < 0) *s += len;
    if (*e < 0) *e += len;
    if (*s < 0) *s = 0;
    if (*s > len) *s = len;
    if (*e < 0) *e = 0;
    if (*e > len) *e = len;
    if (*e < *s) *e = *s;
}

tx_t plant_slice(tx_t data, tx_t start, tx_t end) {
    if (!data) return strdup("");
    PlantArray* a = (PlantArray*)data;
    if (a->magic == PLANT_ARRAY_MAGIC) {
        long len = a->count;
        long s = _slice_arg(start);
        long e = _slice_arg(end);
        _slice_resolve(&s, &e, len);
        PlantArray* out = plant_list_create(e - s);
        out->kind = a->kind;
        for (long i = s; i < e; i++) {
            char buf[32];
            const char* el = _plant_el_text(a->items[i], buf);
            plant_list_push(out, strdup(el));
        }
        return (tx_t)out;
    }
    const char* t = _S(data);
    if (!t) return strdup("");
    long len = (long)strlen(t);
    long s = _slice_arg(start);
    long e = _slice_arg(end);
    _slice_resolve(&s, &e, len);
    if (e <= s) return strdup("");
    char* out = malloc((size_t)(e - s) + 1);
    if (!out) return strdup("");
    memcpy(out, t + s, (size_t)(e - s));
    out[e - s] = '\0';
    return out;
}

/* ═══════════════════════════════════════════════════════════════
   v0.49.15 — List built-ins (batch 1): REVERSE / RANGE / SORT /
   INCLUDES / INDEX_OF / UNIQUE / AVERAGE / MEDIAN. List-aware
   helpers; non-list inputs dispatch to the string equivalents
   (plant_reverse / string_includes) so the existing string
   built-ins keep their behavior. Element comparison uses
   _plant_el_text (tagged small ints and strings), numeric
   extraction mirrors plant_sum (strtod, non-parsable skipped).
   ═══════════════════════════════════════════════════════════════ */

tx_t plant_list_reverse(tx_t data) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC) return plant_reverse(data);
    PlantArray* out = plant_list_create(a->count);
    out->kind = a->kind;
    for (int64_t i = a->count - 1; i >= 0; i--)
        out = plant_list_push(out, a->items[i]);
    return (tx_t)out;
}

tx_t plant_range_list(tx_t start, tx_t end) {
    long s = _slice_arg(start);
    long e = _slice_arg(end);
    if (e <= s) return (tx_t)plant_list_create(0);
    PlantArray* out = plant_list_create(e - s);
    for (long i = s; i < e; i++)
        out = plant_list_push(out, _from_long(i));
    return (tx_t)out;
}

tx_t plant_list_sort(tx_t data) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC) return data;
    return plant_sort(data, "");
}

tx_t plant_list_includes(tx_t data, tx_t item) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC) return string_includes(data, item);
    char vb[32];
    const char* v = _plant_el_text(item, vb);
    for (int64_t i = 0; i < a->count; i++) {
        char eb[32];
        const char* e = _plant_el_text(a->items[i], eb);
        if (e && v && strcmp(e, v) == 0) return strdup("1");
    }
    return strdup("0");
}

tx_t plant_list_index_of(tx_t data, tx_t item) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC) return strdup("-1");
    char vb[32];
    const char* v = _plant_el_text(item, vb);
    for (int64_t i = 0; i < a->count; i++) {
        char eb[32];
        const char* e = _plant_el_text(a->items[i], eb);
        if (e && v && strcmp(e, v) == 0) return _from_long((long)i);
    }
    return strdup("-1");
}

tx_t plant_list_unique(tx_t data) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC) return data;
    PlantArray* out = plant_list_create(a->count);
    out->kind = a->kind;
    for (int64_t i = 0; i < a->count; i++) {
        char eb[32];
        const char* e = _plant_el_text(a->items[i], eb);
        int seen = 0;
        for (int64_t j = 0; j < out->count; j++) {
            char ob[32];
            const char* o = _plant_el_text(out->items[j], ob);
            if (e && o && strcmp(e, o) == 0) { seen = 1; break; }
        }
        if (!seen) out = plant_list_push(out, a->items[i]);
    }
    return (tx_t)out;
}

static int _list_num_compare(const void* pa, const void* pb) {
    double x = *(const double*)pa;
    double y = *(const double*)pb;
    return (x > y) - (x < y);
}

tx_t plant_list_average(tx_t data) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0) return strdup("0");
    double acc = 0.0;
    int64_t n = 0;
    for (int64_t i = 0; i < a->count; i++) {
        tx_t el = a->items[i];
        if (!el) continue;
        if ((uintptr_t)el < 4096) { acc += (double)(intptr_t)el; n++; continue; }
        if (((PlantArray*)el)->magic == PLANT_ARRAY_MAGIC) continue; /* MAP/LIST */
        const char* s = _S(el);
        if (!s || s[0] == '\0') continue;
        char* end = NULL;
        double v = strtod(s, &end);
        if (end == s || *end != '\0') continue; /* non-parsable: skip */
        acc += v;
        n++;
    }
    if (n == 0) return strdup("0");
    double avg = acc / (double)n;
    if (avg == (double)(long long)avg) return _from_long((long long)avg);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.10g", avg);
    return strdup(buf);
}

tx_t plant_list_median(tx_t data) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0) return strdup("0");
    double* vals = (double*)malloc((size_t)a->count * sizeof(double));
    if (!vals) return strdup("0");
    int64_t n = 0;
    for (int64_t i = 0; i < a->count; i++) {
        tx_t el = a->items[i];
        if (!el) continue;
        if ((uintptr_t)el < 4096) { vals[n++] = (double)(intptr_t)el; continue; }
        if (((PlantArray*)el)->magic == PLANT_ARRAY_MAGIC) continue; /* MAP/LIST */
        const char* s = _S(el);
        if (!s || s[0] == '\0') continue;
        char* end = NULL;
        double v = strtod(s, &end);
        if (end == s || *end != '\0') continue; /* non-parsable: skip */
        vals[n++] = v;
    }
    if (n == 0) { free(vals); return strdup("0"); }
    qsort(vals, (size_t)n, sizeof(double), _list_num_compare);
    double med;
    if (n % 2 == 1) {
        med = vals[n / 2];
    } else {
        med = (vals[n / 2 - 1] + vals[n / 2]) / 2.0;
    }
    free(vals);
    if (med == (double)(long long)med) return _from_long((long long)med);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.10g", med);
    return strdup(buf);
}

/* ── v0.49.16 — List built-ins (batch 2) ─────────────────────────
   FLATTEN / CHUNK / ZIP / FILTER_GT / FILTER_LT. Single-level
   flatten (maps and non-list elements pass through), CHUNK
   subdivides into max-size sub-lists, ZIP pairs element-wise and
   truncates to the shorter list, and the filters keep elements
   strictly greater / strictly less than the numeric threshold
   (non-numeric elements are dropped, mirroring AVERAGE/MEDIAN). */

static double _list_num(tx_t el, int* ok) {
    *ok = 0;
    if ((intptr_t)el > -4096 && (intptr_t)el < 4096) {
        *ok = 1;
        return (double)(intptr_t)el;
    }
    if (((PlantArray*)el)->magic == PLANT_ARRAY_MAGIC) return 0.0; /* MAP/LIST */
    const char* s = _S(el);
    if (!s || s[0] == '\0') return 0.0;
    char* end = NULL;
    double v = strtod(s, &end);
    if (end == s || *end != '\0') return 0.0; /* non-parsable */
    *ok = 1;
    return v;
}

/* ═══════════════════════════════════════════════════════════════
   v0.49.21 — statistical & array aggregation built-ins
   All seven accept one list argument; non-parsable elements are
   filtered (via _list_num), NULL/magic-checked, and every empty /
   no-valid-element case has an explicit default per spec:
   variance/stddev/min/max/range → "0", product → "1", mode → "".
   ═══════════════════════════════════════════════════════════════ */

static void _stat_scan(tx_t data, double* sum, double* sq, int64_t* n,
                       double* mn, double* mx) {
    *sum = 0.0; *sq = 0.0; *n = 0; *mn = 0.0; *mx = 0.0;
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC) return;
    for (int64_t i = 0; i < a->count; i++) {
        tx_t el = a->items[i];
        if (!el) continue;
        int ok = 0;
        double v = _list_num(el, &ok);
        if (!ok) continue;
        if (*n == 0) { *mn = v; *mx = v; }
        if (v < *mn) *mn = v;
        if (v > *mx) *mx = v;
        *sum += v;
        *sq += v * v;
        *n += 1;
    }
}

tx_t plant_list_variance(tx_t data) {
    double sum, sq, mn, mx; int64_t n;
    _stat_scan(data, &sum, &sq, &n, &mn, &mx);
    if (n == 0) return strdup("0");
    double mean = sum / (double)n;
    double var = sq / (double)n - mean * mean;   /* population */
    if (var < 0.0) var = 0.0;                    /* float-fuzz guard */
    return _plant_math_result(var);
}

tx_t plant_list_stddev(tx_t data) {
    double sum, sq, mn, mx; int64_t n;
    _stat_scan(data, &sum, &sq, &n, &mn, &mx);
    if (n == 0) return strdup("0");
    double mean = sum / (double)n;
    double var = sq / (double)n - mean * mean;
    if (var < 0.0) var = 0.0;
    return _plant_math_result(sqrt(var));
}

tx_t plant_list_product(tx_t data) {
    double sum, sq, mn, mx; int64_t n;
    _stat_scan(data, &sum, &sq, &n, &mn, &mx);
    (void)n;
    double acc = 1.0;
    PlantArray* a = (PlantArray*)data;
    if (a && a->magic == PLANT_ARRAY_MAGIC) {
        for (int64_t i = 0; i < a->count; i++) {
            tx_t el = a->items[i];
            if (!el) continue;
            int ok = 0;
            double v = _list_num(el, &ok);
            if (ok) acc *= v;
        }
    }
    return _plant_math_result(acc);
}

tx_t plant_list_min(tx_t data) {
    double sum, sq, mn, mx; int64_t n;
    _stat_scan(data, &sum, &sq, &n, &mn, &mx);
    if (n == 0) return strdup("0");
    return _plant_math_result(mn);
}

tx_t plant_list_max(tx_t data) {
    double sum, sq, mn, mx; int64_t n;
    _stat_scan(data, &sum, &sq, &n, &mn, &mx);
    if (n == 0) return strdup("0");
    return _plant_math_result(mx);
}

tx_t plant_list_range(tx_t data) {
    double sum, sq, mn, mx; int64_t n;
    _stat_scan(data, &sum, &sq, &n, &mn, &mx);
    if (n == 0) return strdup("0");
    return _plant_math_result(mx - mn);
}

tx_t plant_list_mode(tx_t data) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0) return strdup("");
    tx_t best = NULL;
    long bestc = 0;
    for (int64_t i = 0; i < a->count; i++) {
        tx_t el = a->items[i];
        if (!el) continue;
        int ok = 0;
        double v = _list_num(el, &ok);
        if (!ok) continue;
        long c = 1;
        for (int64_t j = i + 1; j < a->count; j++) {
            tx_t ej = a->items[j];
            if (!ej) continue;
            int okj = 0;
            double vj = _list_num(ej, &okj);
            if (okj && vj == v) c++;
        }
        if (c > bestc) { bestc = c; best = el; }
    }
    if (!best || bestc < 1) return strdup("");
    if ((uintptr_t)best < 4096) return _from_long((intptr_t)best);
    return strdup(_S(best));
}

/* ================================================================
   v0.49.22 - matrix & linear algebra built-ins
   DOT CROSS NORM TRANSPOSE MATRIX_MULT INVERSE DET over nested
   lists (LIST of LIST). Rectangularity/dimension violations,
   ragged input, non-parsable elements and singular inverses all
   return the string "ERR". Cells coerce via _list_num; numeric
   results render through _plant_math_result.
   ================================================================ */

#define LA_MAXD 64

static PlantArray* _la_matrix(tx_t data, int64_t* rows, int64_t* cols) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0) return NULL;
    if (a->count > LA_MAXD) return NULL;
    for (int64_t i = 0; i < a->count; i++) {
        tx_t r = a->items[i];
        if (!r) return NULL;
        PlantArray* ra = (PlantArray*)r;
        if (ra->magic != PLANT_ARRAY_MAGIC || ra->count == 0) return NULL;
        if (i == 0) { *cols = ra->count; if (*cols > LA_MAXD) return NULL; }
        else if (ra->count != *cols) return NULL;      /* ragged */
    }
    *rows = a->count;
    return a;
}

static double _la_get(PlantArray* m, int64_t i, int64_t j, int* ok) {
    PlantArray* row = (PlantArray*)m->items[i];
    return _list_num(row->items[j], ok);
}

static int _la_vec(tx_t v, double* out, int64_t* n) {
    PlantArray* a = (PlantArray*)v;
    *n = 0;
    if (!a || a->magic != PLANT_ARRAY_MAGIC || a->count == 0 || a->count > LA_MAXD) return 0;
    for (int64_t i = 0; i < a->count; i++) {
        int ok = 0;
        double d = _list_num(a->items[i], &ok);
        if (!ok) return 0;
        out[(*n)++] = d;
    }
    return 1;
}

tx_t plant_dot(tx_t v1, tx_t v2) {
    double a[LA_MAXD], b[LA_MAXD];
    int64_t na, nb;
    if (!_la_vec(v1, a, &na) || !_la_vec(v2, b, &nb)) return strdup("ERR");
    if (na != nb) return strdup("ERR");
    double acc = 0.0;
    for (int64_t i = 0; i < na; i++) acc += a[i] * b[i];
    return _plant_math_result(acc);
}

tx_t plant_cross(tx_t v1, tx_t v2) {
    double a[4], b[4];
    int64_t na, nb;
    if (!_la_vec(v1, a, &na) || na != 3) return strdup("ERR");
    if (!_la_vec(v2, b, &nb) || nb != 3) return strdup("ERR");
    return (tx_t)plant_list_make(3,
        _plant_math_result(a[1]*b[2] - a[2]*b[1]),
        _plant_math_result(a[2]*b[0] - a[0]*b[2]),
        _plant_math_result(a[0]*b[1] - a[1]*b[0]));
}

tx_t plant_norm(tx_t v) {
    double a[LA_MAXD];
    int64_t n;
    if (!_la_vec(v, a, &n)) return strdup("ERR");
    double acc = 0.0;
    for (int64_t i = 0; i < n; i++) acc += a[i] * a[i];
    return _plant_math_result(sqrt(acc));
}

tx_t plant_transpose(tx_t m) {
    int64_t r, c;
    PlantArray* M = _la_matrix(m, &r, &c);
    if (!M) return strdup("ERR");
    PlantArray* out = plant_list_create(c);
    for (int64_t j = 0; j < c; j++) {
        PlantArray* row = plant_list_create(r);
        for (int64_t i = 0; i < r; i++) {
            int ok = 0;
            double v = _la_get(M, i, j, &ok);
            if (!ok) return strdup("ERR");
            row = plant_list_push(row, _plant_math_result(v));
        }
        out = plant_list_push(out, (tx_t)row);
    }
    return (tx_t)out;
}

tx_t plant_matrix_mult(tx_t m1, tx_t m2) {
    int64_t r1, c1, r2, c2;
    PlantArray* A = _la_matrix(m1, &r1, &c1);
    PlantArray* B = _la_matrix(m2, &r2, &c2);
    if (!A || !B || c1 != r2) return strdup("ERR");
    PlantArray* out = plant_list_create(r1);
    for (int64_t i = 0; i < r1; i++) {
        PlantArray* row = plant_list_create(c2);
        for (int64_t j = 0; j < c2; j++) {
            double acc = 0.0;
            for (int64_t k = 0; k < c1; k++) {
                int o1 = 0, o2 = 0;
                double x = _la_get(A, i, k, &o1);
                double y = _la_get(B, k, j, &o2);
                if (!o1 || !o2) return strdup("ERR");
                acc += x * y;
            }
            row = plant_list_push(row, _plant_math_result(acc));
        }
        out = plant_list_push(out, (tx_t)row);
    }
    return (tx_t)out;
}

/* Gaussian elimination core shared by DET / INVERSE.
   Fills aug (n x 2n for inverse work); returns determinant sign-
   aware product, or set *singular. */
static double _gauss(double aug[LA_MAXD][2*LA_MAXD], int n, int cols, int* singular) {
    double det = 1.0;
    *singular = 0;
    for (int col = 0; col < n; col++) {
        int piv = col;
        double best = fabs(aug[col][col]);
        for (int rr = col + 1; rr < n; rr++)
            if (fabs(aug[rr][col]) > best) { best = fabs(aug[rr][col]); piv = rr; }
        if (best < 1e-12) { *singular = 1; return 0.0; }
        if (piv != col) {
            for (int cc = 0; cc < cols; cc++) { double t = aug[col][cc]; aug[col][cc] = aug[piv][cc]; aug[piv][cc] = t; }
            det = -det;
        }
        double d = aug[col][col];
        det *= d;
        for (int cc = col; cc < cols; cc++) aug[col][cc] /= d;
        for (int rr = 0; rr < n; rr++) {
            if (rr == col) continue;
            double f = aug[rr][col];
            if (f == 0.0) continue;
            for (int cc = col; cc < cols; cc++) aug[rr][cc] -= f * aug[col][cc];
        }
    }
    return det;
}

tx_t plant_inverse(tx_t m) {
    int64_t r, c;
    PlantArray* M = _la_matrix(m, &r, &c);
    if (!M || r != c) return strdup("ERR");
    int n = (int)r;
    static __thread double aug[LA_MAXD][2*LA_MAXD];
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            int ok = 0;
            aug[i][j] = _la_get(M, i, j, &ok);
            if (!ok) return strdup("ERR");
            aug[i][j+n] = (i == j) ? 1.0 : 0.0;
        }
    int singular = 0;
    _gauss(aug, n, 2*n, &singular);
    if (singular) return strdup("ERR");
    PlantArray* out = plant_list_create(n);
    for (int i = 0; i < n; i++) {
        PlantArray* row = plant_list_create(n);
        for (int j = 0; j < n; j++)
            row = plant_list_push(row, _plant_math_result(aug[i][j+n]));
        out = plant_list_push(out, (tx_t)row);
    }
    return (tx_t)out;
}

tx_t plant_det(tx_t m) {
    int64_t r, c;
    PlantArray* M = _la_matrix(m, &r, &c);
    if (!M || r != c) return strdup("ERR");
    int n = (int)r;
    static __thread double aug[LA_MAXD][2*LA_MAXD];
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            int ok = 0;
            aug[i][j] = _la_get(M, i, j, &ok);
            if (!ok) return strdup("ERR");
        }
    int singular = 0;
    double det = _gauss(aug, n, n, &singular);
    if (singular) return _plant_math_result(0.0);
    return _plant_math_result(det);
}

/* ================================================================
   v0.49.23 - numerical analysis built-ins
   LU EIGEN SVD SOLVE COND. Shapes: LU -> [L, U] (PA = LU, partial
   pivoting folded into row order); EIGEN -> [values, vectors]
   (symmetric matrices only, Jacobi rotations, values sorted
   descending, vectors as columns); SVD -> [U, S, V] (via the
   symmetric eigenproblem of A^T A, sigma = sqrt(lambda), null-
   space columns of U Gram-Schmidt filled); SOLVE(m, b) -> x
   (partial-pivot elimination); COND(m) -> norm(m)*norm(inv(m))
   (Euclidean/Frobenius). Every invalid state returns "ERR".
   ================================================================ */

static PlantArray* _la_build(int rows, int cols, double src[LA_MAXD][2*LA_MAXD]) {
    PlantArray* out = plant_list_create(rows);
    for (int i = 0; i < rows; i++) {
        PlantArray* row = plant_list_create(cols);
        for (int j = 0; j < cols; j++)
            row = plant_list_push(row, _plant_math_result(src[i][j]));
        out = plant_list_push(out, (tx_t)row);
    }
    return out;
}

static int _la_symmetric(PlantArray* M, int n) {
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            int o1 = 0, o2 = 0;
            double a = _la_get(M, i, j, &o1);
            double b = _la_get(M, j, i, &o2);
            if (!o1 || !o2 || fabs(a - b) > 1e-9) return 0;
        }
    return 1;
}

/* cyclic Jacobi eigenrotation for symmetric matrices; fills eval
   (diagonal) and evec (columns are eigenvectors) */
static void _jacobi(double A[LA_MAXD][LA_MAXD], double V[LA_MAXD][LA_MAXD], int n) {
    for (int i = 0; i < n; i++) V[i][i] = 1.0;
    for (int sweep = 0; sweep < 200; sweep++) {
        double off = 0.0;
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++) off += A[i][j] * A[i][j];
        if (off < 1e-22) break;
        for (int p = 0; p < n; p++)
            for (int q = p + 1; q < n; q++) {
                if (fabs(A[p][q]) < 1e-15) continue;
                double theta = (A[q][q] - A[p][p]) / (2.0 * A[p][q]);
                double t = (theta >= 0 ? 1.0 : -1.0) /
                           (fabs(theta) + sqrt(theta * theta + 1.0));
                double cs = 1.0 / sqrt(t * t + 1.0);
                double sn = t * cs;
                for (int k = 0; k < n; k++) {
                    double akp = A[k][p], akq = A[k][q];
                    A[k][p] = cs * akp - sn * akq;
                    A[k][q] = sn * akp + cs * akq;
                }
                for (int k = 0; k < n; k++) {
                    double apk = A[p][k], aqk = A[q][k];
                    A[p][k] = cs * apk - sn * aqk;
                    A[q][k] = sn * apk + cs * aqk;
                    double vkp = V[k][p], vkq = V[k][q];
                    V[k][p] = cs * vkp - sn * vkq;
                    V[k][q] = sn * vkp + cs * vkq;
                }
            }
    }
}

tx_t plant_lu(tx_t m) {
    int64_t r, c;
    PlantArray* M = _la_matrix(m, &r, &c);
    if (!M || r != c) return strdup("ERR");
    int n = (int)r;
    static __thread double aug[LA_MAXD][2*LA_MAXD];
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            int ok = 0;
            aug[i][j] = _la_get(M, i, j, &ok);
            if (!ok) return strdup("ERR");
        }
    /* in-place Doolittle with partial pivoting; unit lower L */
    double det_sign = 1.0;
    for (int col = 0; col < n; col++) {
        int piv = col;
        double best = fabs(aug[col][col]);
        for (int rr = col + 1; rr < n; rr++)
            if (fabs(aug[rr][col]) > best) { best = fabs(aug[rr][col]); piv = rr; }
        if (best < 1e-12) continue;   /* degenerate column: proceed */
        if (piv != col)
            for (int cc = 0; cc < n; cc++) { double t = aug[col][cc]; aug[col][cc] = aug[piv][cc]; aug[piv][cc] = t; }
        for (int rr = col + 1; rr < n; rr++) {
            double f = aug[rr][col] / aug[col][col];
            aug[rr][col] = f;                       /* store L factor */
            for (int cc = col + 1; cc < n; cc++) aug[rr][cc] -= f * aug[col][cc];
        }
        (void)det_sign;
    }
    /* split: strict-lower triangle + diagonal = L; upper incl diag = U */
    static __thread double Lb[LA_MAXD][2*LA_MAXD];
    static __thread double Ub[LA_MAXD][2*LA_MAXD];
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            Lb[i][j] = (j < i) ? aug[i][j] : (j == i ? 1.0 : 0.0);
            Ub[i][j] = (j >= i) ? aug[i][j] : 0.0;
        }
    PlantArray* out = plant_list_create(2);
    out = plant_list_push(out, (tx_t)_la_build(n, n, Lb));
    out = plant_list_push(out, (tx_t)_la_build(n, n, Ub));
    return (tx_t)out;
}

static PlantArray* _eig_impl(tx_t m, int want_vectors) {
    int64_t r, c;
    PlantArray* M = _la_matrix(m, &r, &c);
    if (!M || r != c || !_la_symmetric(M, (int)r)) return NULL;
    int n = (int)r;
    static __thread double A[LA_MAXD][LA_MAXD];
    static __thread double V[LA_MAXD][LA_MAXD];
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            int ok = 0;
            A[i][j] = _la_get(M, i, j, &ok);
            if (!ok) return NULL;
            V[i][j] = (i == j) ? 1.0 : 0.0;
        }
    _jacobi(A, V, n);
    /* sort eigenpairs descending */
    int idx[LA_MAXD];
    for (int i = 0; i < n; i++) idx[i] = i;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (A[idx[j]][idx[j]] > A[idx[i]][idx[i]]) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
    PlantArray* vals = plant_list_create(n);
    for (int i = 0; i < n; i++)
        vals = plant_list_push(vals, _plant_math_result(A[idx[i]][idx[i]]));
    if (!want_vectors) return vals;
    PlantArray* vecs = plant_list_create(n);   /* columns of V */
    for (int j = 0; j < n; j++) {
        PlantArray* col = plant_list_create(n);
        for (int i = 0; i < n; i++)
            col = plant_list_push(col, _plant_math_result(V[i][idx[j]]));
        vecs = plant_list_push(vecs, (tx_t)col);
    }
    PlantArray* out = plant_list_create(2);
    out = plant_list_push(out, (tx_t)vals);
    out = plant_list_push(out, (tx_t)vecs);
    return out;
}

tx_t plant_eigen(tx_t m) {
    PlantArray* o = _eig_impl(m, 1);
    if (!o) return strdup("ERR");
    return (tx_t)o;
}

tx_t plant_svd(tx_t m) {
    int64_t rm, cm;
    PlantArray* M = _la_matrix(m, &rm, &cm);
    if (!M) return strdup("ERR");
    int mm = (int)rm, nn = (int)cm;
    /* AtA = M^T M (n x n, symmetric) */
    static __thread double AtA[LA_MAXD][LA_MAXD];
    static __thread double V[LA_MAXD][LA_MAXD];
    for (int i = 0; i < nn; i++)
        for (int j = 0; j < nn; j++) {
            double acc = 0.0;
            for (int k = 0; k < mm; k++) {
                int o1 = 0, o2 = 0;
                acc += _la_get(M, k, i, &o1) * _la_get(M, k, j, &o2);
            }
            AtA[i][j] = acc;
            V[i][j] = (i == j) ? 1.0 : 0.0;
        }
    _jacobi(AtA, V, nn);
    double sv[LA_MAXD];
    int idx[LA_MAXD];
    for (int i = 0; i < nn; i++) { sv[i] = sqrt(AtA[i][i] > 0 ? AtA[i][i] : 0.0); idx[i] = i; }
    for (int i = 0; i < nn; i++)
        for (int j = i + 1; j < nn; j++)
            if (sv[idx[j]] > sv[idx[i]]) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
    /* U columns: Av/sigma for sigma > eps; Gram-Schmidt fill rest */
    static __thread double U[LA_MAXD][LA_MAXD];
    for (int c = 0; c < nn; c++) {
        int si = idx[c];
        if (sv[si] > 1e-9) {
            for (int i = 0; i < mm; i++) {
                double acc = 0.0;
                for (int j = 0; j < nn; j++) {
                    int ok = 0;
                    acc += _la_get(M, i, j, &ok) * V[j][si];
                }
                U[i][c] = acc / sv[si];
            }
        } else {
            for (int i = 0; i < mm; i++) U[i][c] = 0.0;
            /* canonical basis vector orthogonalized against prior cols */
            int placed = 0;
            for (int basis = 0; basis < mm && !placed; basis++) {
                for (int i = 0; i < mm; i++) U[i][c] = (i == basis) ? 1.0 : 0.0;
                for (int pc = 0; pc < c && !placed; pc++) {
                    double dot = 0.0;
                    for (int i = 0; i < mm; i++) dot += U[i][pc] * U[i][c];
                    if (fabs(dot) > 1e-12) break;
                    if (pc == c - 1) placed = 1;
                }
                if (placed || c == 0) placed = 1;
            }
            for (int pc = 0; pc < c; pc++) {
                double dot = 0.0;
                for (int i = 0; i < mm; i++) dot += U[i][pc] * U[i][c];
                for (int i = 0; i < mm; i++) U[i][c] -= dot * U[i][pc];
            }
            double len = 0.0;
            for (int i = 0; i < mm; i++) len += U[i][c] * U[i][c];
            if (len > 1e-20) for (int i = 0; i < mm; i++) U[i][c] /= sqrt(len);
        }
    }
    PlantArray* Uo = plant_list_create(mm);
    for (int i = 0; i < mm; i++) {
        PlantArray* row = plant_list_create(nn);
        for (int j = 0; j < nn; j++) row = plant_list_push(row, _plant_math_result(U[i][j]));
        Uo = plant_list_push(Uo, (tx_t)row);
    }
    PlantArray* So = plant_list_create(nn);
    for (int c = 0; c < nn; c++) So = plant_list_push(So, _plant_math_result(sv[idx[c]]));
    PlantArray* Vo = plant_list_create(nn);   /* columns of V */
    for (int c = 0; c < nn; c++) {
        PlantArray* col = plant_list_create(nn);
        for (int i = 0; i < nn; i++) col = plant_list_push(col, _plant_math_result(V[i][idx[c]]));
        Vo = plant_list_push(Vo, (tx_t)col);
    }
    PlantArray* out = plant_list_create(3);
    out = plant_list_push(out, (tx_t)Uo);
    out = plant_list_push(out, (tx_t)So);
    out = plant_list_push(out, (tx_t)Vo);
    return (tx_t)out;
}

tx_t plant_solve(tx_t m, tx_t bv) {
    int64_t r, c;
    PlantArray* M = _la_matrix(m, &r, &c);
    if (!M || r != c) return strdup("ERR");
    double b[LA_MAXD];
    int64_t nb;
    if (!_la_vec(bv, b, &nb) || nb != r) return strdup("ERR");
    int n = (int)r;
    static __thread double aug[LA_MAXD][2*LA_MAXD];
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            int ok = 0;
            aug[i][j] = _la_get(M, i, j, &ok);
            if (!ok) return strdup("ERR");
        }
        aug[i][n] = b[i];
    }
    for (int col = 0; col < n; col++) {
        int piv = col;
        double best = fabs(aug[col][col]);
        for (int rr = col + 1; rr < n; rr++)
            if (fabs(aug[rr][col]) > best) { best = fabs(aug[rr][col]); piv = rr; }
        if (best < 1e-12) return strdup("ERR");   /* singular system */
        if (piv != col)
            for (int cc = 0; cc <= n; cc++) { double t = aug[col][cc]; aug[col][cc] = aug[piv][cc]; aug[piv][cc] = t; }
        for (int rr = col + 1; rr < n; rr++) {
            double f = aug[rr][col] / aug[col][col];
            for (int cc = col; cc <= n; cc++) aug[rr][cc] -= f * aug[col][cc];
        }
    }
    double x[LA_MAXD];
    for (int i = n - 1; i >= 0; i--) {
        double s = aug[i][n];
        for (int j = i + 1; j < n; j++) s -= aug[i][j] * x[j];
        x[i] = s / aug[i][i];
    }
    PlantArray* out = plant_list_create(n);
    for (int i = 0; i < n; i++) out = plant_list_push(out, _plant_math_result(x[i]));
    return (tx_t)out;
}

tx_t plant_cond(tx_t m) {
    tx_t inv = plant_inverse(m);
    /* distinguish a matrix result from the "ERR" string by magic */
    PlantArray* I = (PlantArray*)inv;
    if (!I || I->magic != PLANT_ARRAY_MAGIC) return strdup("ERR");
    int64_t r, c;
    PlantArray* M = _la_matrix(m, &r, &c);
    if (!M) return strdup("ERR");
    double nm = 0.0, ni = 0.0;
    for (int64_t i = 0; i < r; i++)
        for (int64_t j = 0; j < c; j++) {
            int ok = 0;
            double v = _la_get(M, i, j, &ok);
            nm += v * v;
            v = _la_get(I, i, j, &ok);
            ni += v * v;
        }
    return _plant_math_result(sqrt(nm) * sqrt(ni));
}

tx_t plant_list_flatten(tx_t data) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC) return data;
    PlantArray* out = plant_list_create(a->count);
    for (int64_t i = 0; i < a->count; i++) {
        tx_t el = a->items[i];
        if (el && ((PlantArray*)el)->magic == PLANT_ARRAY_MAGIC &&
            ((PlantArray*)el)->kind == 0) {
            PlantArray* sub = (PlantArray*)el;
            for (int64_t j = 0; j < sub->count; j++)
                out = plant_list_push(out, sub->items[j]);
        } else {
            out = plant_list_push(out, el);
        }
    }
    return (tx_t)out;
}

tx_t plant_list_chunk(tx_t data, tx_t size) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC) return data;
    long sz = _slice_arg(size);
    if (sz < 1 || a->count == 0) return (tx_t)plant_list_create(0);
    PlantArray* out = plant_list_create(a->count / sz + 1);
    for (int64_t i = 0; i < a->count; i += sz) {
        PlantArray* sub = plant_list_create(sz);
        for (long j = 0; j < sz && i + j < a->count; j++)
            sub = plant_list_push(sub, a->items[i + j]);
        out = plant_list_push(out, (tx_t)sub);
    }
    return (tx_t)out;
}

tx_t plant_list_zip(tx_t left, tx_t right) {
    PlantArray* a = (PlantArray*)left;
    PlantArray* b = (PlantArray*)right;
    if (!a || a->magic != PLANT_ARRAY_MAGIC ||
        !b || b->magic != PLANT_ARRAY_MAGIC) {
        return (tx_t)plant_list_create(0);
    }
    int64_t n = a->count < b->count ? a->count : b->count;
    PlantArray* out = plant_list_create(n);
    for (int64_t i = 0; i < n; i++) {
        PlantArray* pair = plant_list_create(2);
        pair = plant_list_push(pair, a->items[i]);
        pair = plant_list_push(pair, b->items[i]);
        out = plant_list_push(out, (tx_t)pair);
    }
    return (tx_t)out;
}

tx_t plant_list_filter_gt(tx_t data, tx_t threshold) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC) return data;
    int tok = 0;
    double t = _list_num(threshold, &tok);
    if (!tok) return (tx_t)plant_list_create(0);
    PlantArray* out = plant_list_create(a->count);
    for (int64_t i = 0; i < a->count; i++) {
        if (!a->items[i]) continue;
        int ok = 0;
        double v = _list_num(a->items[i], &ok);
        if (ok && v > t) out = plant_list_push(out, a->items[i]);
    }
    return (tx_t)out;
}

tx_t plant_list_filter_lt(tx_t data, tx_t threshold) {
    PlantArray* a = (PlantArray*)data;
    if (!a || a->magic != PLANT_ARRAY_MAGIC) return data;
    int tok = 0;
    double t = _list_num(threshold, &tok);
    if (!tok) return (tx_t)plant_list_create(0);
    PlantArray* out = plant_list_create(a->count);
    for (int64_t i = 0; i < a->count; i++) {
        if (!a->items[i]) continue;
        int ok = 0;
        double v = _list_num(a->items[i], &ok);
        if (ok && v < t) out = plant_list_push(out, a->items[i]);
    }
    return (tx_t)out;
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.38k — VEIN resource & file management: TAP / ABSORB /
   INFUSE / SEAL. TAP opens a path with standard modes ("r", "w",
   "a") and encapsulates the FILE* inside a heap block tagged with a
   magic so the other three operations can validate the handle
   before touching it. Failure to open returns NULL (falsy). ABSORB
   reads the entire stream into a freshly allocated tx_t; INFUSE
   writes/appends and reports "1"/"0"; SEAL closes the stream, zeroes
   the tag, frees the handle, and reports "1"/"0".
   ═══════════════════════════════════════════════════════════════ */

typedef struct { uint32_t magic; FILE* fp; } Vein;

#define VEIN_MAGIC 0x56454E31U

tx_t plant_tap(tx_t path, tx_t mode) {
    const char* p = path ? _S(path) : "";
    const char* m = mode ? _S(mode) : "";
    if (!p || !*p || !m || !*m) return NULL;
    FILE* fp = fopen(p, m);
    if (!fp) return NULL;
    Vein* v = malloc(sizeof(Vein));
    if (!v) { fclose(fp); return NULL; }
    v->magic = VEIN_MAGIC;
    v->fp = fp;
    return (tx_t)v;
}

tx_t plant_absorb(tx_t vein) {
    Vein* v = (Vein*)vein;
    if (!v || v->magic != VEIN_MAGIC || !v->fp) return strdup("");
    if (fseek(v->fp, 0, SEEK_END) != 0) return strdup("");
    long sz = ftell(v->fp);
    if (sz < 0) return strdup("");
    if (fseek(v->fp, 0, SEEK_SET) != 0) return strdup("");
    char* buf = malloc((size_t)sz + 1);
    if (!buf) return strdup("");
    size_t rd = fread(buf, 1, (size_t)sz, v->fp);
    buf[rd] = '\0';
    return buf;
}

tx_t plant_infuse(tx_t vein, tx_t data) {
    Vein* v = (Vein*)vein;
    if (!v || v->magic != VEIN_MAGIC || !v->fp) return strdup("0");
    const char* d = data ? _S(data) : "";
    size_t dl = strlen(d);
    if (dl > 0 && fwrite(d, 1, dl, v->fp) != dl) return strdup("0");
    return strdup("1");
}

tx_t plant_seal(tx_t vein) {
    Vein* v = (Vein*)vein;
    if (!v || v->magic != VEIN_MAGIC) return strdup("0");
    int rc = (v->fp && fclose(v->fp) == 0) ? 1 : 0;
    v->magic = 0;
    v->fp = NULL;
    free(v);
    return strdup(rc ? "1" : "0");
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
    if (on) { char _fb[256]; snprintf(_fb, 256, "[ffi] %s", _S(msg)); plant_info(_fb); }
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
    tx_t r = (tx_t)plant_map_hash_create(8);
    for (size_t i = 0; i < plant_profiles_count; i++) {
        char buf[128];
        double ms = (double)plant_profiles[i].total_ns / 1e6;
        snprintf(buf, sizeof(buf), "%.3f ms / %lld calls", ms,
                 (long long)plant_profiles[i].count);
        plant_map_hash_set((PlantMap*)r, plant_profiles[i].name, strdup(buf));
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
static long g_arena_live_bytes = 0;   /* v0.48.37 */
static int g_inited = 0;
static plant_task* g_running = NULL;  /* task currently mid-step */

static long plant_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
/* v0.48.37e — monotonic milliseconds for WAIT timing verification */
long plant_now_ms(void) {
    return plant_ms();
}
/* v0.48.37e — millisecond sleep via POSIX nanosleep: the requested
   duration is split into whole seconds plus the nanosecond remainder
   (ms % 1000 * 1000000). Zero and negative durations are invalid
   timing arguments and return immediately (no-op). */
void plant_msleep(long ms) {
    if (ms <= 0) return;
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
            g_arena_live_bytes += (long)n;   /* v0.48.37: live arena bytes */
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
            g_arena_live_bytes -= (long)s->used;   /* v0.48.37 */
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
static long g_persist_cfg_pressure = 0;   /* v0.48.37b: forced pressure (0 = auto) */

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
static size_t g_fast_esc_sz[PLANT_FAST_ESC_MAX];   /* v0.48.37: per-block size */
static long g_fast_esc_n = 0;
static long g_fast_esc_bytes = 0;                  /* v0.48.37 */
static long g_fast_shrinks = 0;                    /* v0.48.37 */
static long g_fast_shrink_cycles = 0;              /* v0.48.37: consecutive low-pressure scopes */

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
    if (p && g_fast_esc_n < PLANT_FAST_ESC_MAX) {
        g_fast_esc[g_fast_esc_n] = p;
        g_fast_esc_sz[g_fast_esc_n] = need;
        g_fast_esc_n++;
        g_fast_esc_bytes += (long)need;
    }
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
    /* v0.48.37: adaptive shrink — after 4 consecutive low-pressure
       scopes (used < size/4) halve the heap down to the base cap */
    size_t base_cap = g_fast_cfg_cap ? g_fast_cfg_cap : (8u << 20);
    if (g_fast.used * 4 < g_fast.size && g_fast.size > base_cap) {
        if (++g_fast_shrink_cycles >= 4) {
            size_t ns = g_fast.size / 2;
            char* nb = (char*)realloc(g_fast.base, ns);
            if (nb) {
                g_fast.base = nb; g_fast.size = ns;
                g_fast_shrink_cycles = 0; g_fast_shrinks++;
                plant_audit_log("FAST_SHRINK", "heap halved on low pressure");
            }
        }
    } else {
        g_fast_shrink_cycles = 0;
    }
    g_fast.used = 0;
    for (long i = 0; i < g_fast_esc_n; i++) {
        free(g_fast_esc[i]);
        g_fast_esc_bytes -= (long)g_fast_esc_sz[i];
    }
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
static long plant_rw_recover_sweep(void);   /* v0.48.37c: real-process recovery */
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
    restarts += plant_rw_recover_sweep();
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

/* NOTE: plant_safe_status / plant_safe_stall / plant_safe_starve are
   defined in the v0.48.37c section below, once the real worker-pool
   types (plant_rw, g_rw) are in scope. */

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

/* v0.48.37 — DistributedHeap forward declarations (used by the
   MISSION CONFIG handler above; bodies in the v0.48.37 section) */
#define PLANT_DIST_MAX_NODES 64
#define PLANT_DIST_VPTS      64
static long     g_dist_nodes = 4;
static long     g_dist_vpts = PLANT_DIST_VPTS;
static uint64_t g_dist_ring[PLANT_DIST_MAX_NODES * PLANT_DIST_VPTS];
static long     g_dist_ring_node[PLANT_DIST_MAX_NODES * PLANT_DIST_VPTS];
static long     g_dist_ring_n = 0;
static long     g_dist_node_bytes[PLANT_DIST_MAX_NODES];
static long     g_dist_node_cap[PLANT_DIST_MAX_NODES];
static long     g_dist_allocs = 0;
static long     g_dist_evicts = 0;
static void     plant_dist_ring_build(void);

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
    } else if (strcmp(key, "DIST_NODES") == 0) {          /* v0.48.37 */
        long v = atol(val);
        if (v >= 1 && v <= PLANT_DIST_MAX_NODES) { g_dist_nodes = v; plant_dist_ring_build(); }
    } else if (strcmp(key, "DIST_NODE_CAP") == 0) {       /* v0.48.37 */
        long v = atol(val);
        if (v >= 1) for (long i = 0; i < PLANT_DIST_MAX_NODES; i++) g_dist_node_cap[i] = v;
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
    } else if (strcmp(key, "PERSIST_PRESSURE") == 0) {
        long v = atol(val);
        if (v >= 0 && v <= 100) g_persist_cfg_pressure = v;
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
    long node;              /* v0.48.37: DistributedHeap node (-1 = local) */
    char deferred;          /* v0.48.37b: queued for early lease eviction */
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
static long g_arc_live_bytes = 0;   /* v0.48.37 */
#define PLANT_ARC_DEFERRED_MAX 512
static plant_arc_obj* g_arc_deferred[PLANT_ARC_DEFERRED_MAX];
static long g_pending_frees = 0;    /* deferred deallocations queued */
static long g_persist_evicts = 0;   /* objects reclaimed by lease_evict */

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
    g_arc_live_bytes -= (long)o->size;   /* v0.48.37 */
    free(o->data);
    free(o);
}

static void plant_arc_destroy(plant_arc_obj* o, int reclaimed);
long plant_persist_pressure(void);          /* v0.48.37b */
long plant_lease_evict(void);               /* v0.48.37b */

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
    o->node = -1;
    g_arc_live_bytes += sz;   /* v0.48.37 */
    o->next = g_arc_head;
    g_arc_head = o;
    g_arc_live++;
    g_arc_allocs++;
    static char msg[96];
    snprintf(msg, sizeof(msg), "seq=%ld size=%ld", o->alloc_seq, (long)sz);
    plant_audit_log("ARC_ALLOC", msg);
    /* automatic cycle detection every PERSIST_GC_INTERVAL allocations */
    if (g_arc_allocs % g_persist_cfg_gc_interval == 0) plant_arc_gc();
    /* v0.48.37b: proactive lease eviction under memory pressure */
    if (plant_persist_pressure() >= 80) plant_lease_evict();
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
   linear in live objects and runs every PERSIST_GC_INTERVAL allocations
   (default 1000; MISSION CONFIG PERSIST_GC_INTERVAL = N adjusts the
   schedule). The scan also drains the v0.48.37b deferred-eviction
   queue as queued leases expire. */
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
        if ((o->mark != 1 || lease_dead) && !o->deferred) {
            plant_arc_destroy(o, 1);
            reclaimed++;
        }
        o = nx;
    }
    /* v0.48.37b: drain the deferred-eviction queue as leases expire */
    for (long i = 0; i < g_pending_frees; ) {
        plant_arc_obj* d = g_arc_deferred[i];
        if (d->leased_until_ms <= now) {
            plant_arc_destroy(d, 1);
            reclaimed++;
            g_pending_frees--;
            g_arc_deferred[i] = g_arc_deferred[g_pending_frees];
        } else {
            i++;
        }
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

/* v0.48.37b — memory pressure: % of the FAST bump heap in use
   (secondary: ARC live bytes vs a 64MB soft cap). PERSIST_PRESSURE
   overrides the computed value when configured (> 0). */
long plant_persist_pressure(void) {
    if (g_persist_cfg_pressure > 0) return g_persist_cfg_pressure;
    plant_fast_init();
    long p = 0;
    if (g_fast.size > 0) p = (long)(g_fast.used * 100 / g_fast.size);
    long arc_pct = (long)(g_arc_live_bytes * 100 / (64L * 1024 * 1024));
    if (arc_pct > p) p = arc_pct;
    return p;
}

/* v0.48.37b — proactive lease eviction. Under memory pressure:
   - < 80% : leases run to natural expiry (no action).
   - 80-89%: zero-ref leased objects that are expired or within the
     PERSIST_LEASE_MS margin are QUEUED for reclamation (deferred);
     the queue drains as the leases expire.
   - >= 90%: every zero-ref leased object is released early (before
     expiry) and the deferred queue is drained immediately.
   Returns the number of objects reclaimed. */
long plant_lease_evict(void) {
    long p = plant_persist_pressure();
    if (p < 80) return 0;
    long now = plant_ms();
    long margin = g_persist_cfg_lease_ms;
    int critical = p >= 90;
    long reclaimed = 0;
    plant_arc_obj* o = g_arc_head;
    while (o) {
        plant_arc_obj* nx = o->next;
        if (o->refs == 0 && o->leased_until_ms > 0 && !o->deferred) {
            int expired = o->leased_until_ms <= now;
            int near = margin > 0 && o->leased_until_ms - now <= margin;
            if (expired || near || critical) {
                if (critical || expired) {
                    plant_arc_destroy(o, 1);
                    reclaimed++;
                } else if (g_pending_frees < PLANT_ARC_DEFERRED_MAX) {
                    g_arc_deferred[g_pending_frees++] = o;
                    o->deferred = 1;
                    static char msg[96];
                    snprintf(msg, sizeof(msg), "seq=%ld queued margin=%ld", o->alloc_seq, margin);
                    plant_audit_log("LEASE_EVICT", msg);
                }
            }
        }
        o = nx;
    }
    if (critical && g_pending_frees > 0) {
        while (g_pending_frees > 0) {
            plant_arc_destroy(g_arc_deferred[--g_pending_frees], 1);
            reclaimed++;
        }
    }
    if (reclaimed > 0) {
        static char msg[96];
        snprintf(msg, sizeof(msg), "pressure=%ld reclaimed=%ld", p, reclaimed);
        plant_audit_log("LEASE_EVICT", msg);
        g_persist_evicts += reclaimed;
    }
    return reclaimed;
}

/* v0.48.37b — persistent heap diagnostics: a structured MAP with
   live_objects (active allocations), gc_runs (cumulative GC cycles),
   leased_count (objects under an active lease) and pending_frees
   (deferred deallocations queued for reclamation). */
tx_t plant_persist_status(void) {
    long leased = 0;
    for (plant_arc_obj* o = g_arc_head; o; o = o->next)
        if (o->leased_until_ms > 0) leased++;
    PlantArray* _ps_meta = plant_list_make(8,
        "live_objects", _from_long(g_arc_live),
        "gc_runs", _from_long(g_arc_gc_runs),
        "leased_count", _from_long(leased),
        "pending_frees", _from_long(g_pending_frees));
    _ps_meta->kind = 1;
    return (tx_t)_ps_meta;
}

tx_t plant_arc_finalize_count(void) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%ld", g_arc_finalizes);
    return buf;
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.37d — Weather Memory Management and Exception Cleanup
   Every PlantWeather frame carries an exit-list (exit_list[]) of
   resource handles registered while its protected body ran.
   plant_weather_leave walks the list on every exit path — normal
   completion, handled storms, unmatched propagation and the threaded
   GIVE/BREAK/CONTINUE chains — freeing each handle ARC-aware and
   draining the ARC heap's deferred-deallocation queue. SHELTER bodies
   additionally run under a shelter mark: plant_weather_shelter_leave
   purges the temporaries they registered before the shelter scope is
   exited. plant_weather_status aggregates frame, object and handler
   telemetry into a structured MAP.
   ═══════════════════════════════════════════════════════════════ */

/* ARC-aware handle release: ARC objects are destroyed with their edges
   and heap bookkeeping; everything else falls through to plant_mem_free
   (slab / PlantArray / heap strings). */
static void plant_weather_free_handle(tx_t handle) {
    if (!handle) return;
    plant_arc_obj* o = (plant_arc_obj*)handle;
    for (plant_arc_obj* h = g_arc_head; h; h = h->next) {
        if (h == o) {
            plant_arc_destroy(o, 0);
            return;
        }
    }
    plant_mem_free(handle);
}

/* walk the block's assigned exit-list and free every registered handle
   (deferred deallocations first, then live objects), then drain the ARC
   heap's deferred-deallocation queue. Idempotent: exit_count is reset so
   a second teardown on the same frame is a no-op. */
static void plant_weather_teardown(PlantWeather* w) {
    if (!w) return;
    for (int i = 0; i < w->exit_count; i++) {
        if (w->exit_deferred[i]) plant_weather_free_handle(w->exit_list[i]);
    }
    for (int i = 0; i < w->exit_count; i++) {
        if (!w->exit_deferred[i]) plant_weather_free_handle(w->exit_list[i]);
    }
    w->exit_count = 0;
    w->shelter_mark = -1;
    while (g_pending_frees > 0) {
        plant_arc_destroy(g_arc_deferred[--g_pending_frees], 1);
    }
}

/* register a resource handle on a frame's exit-list (deduped) */
int plant_weather_register(PlantWeather* w, tx_t handle) {
    if (!w || !handle) return 0;
    for (int i = 0; i < w->exit_count; i++)
        if (w->exit_list[i] == handle) return 1;
    if (w->exit_count >= PLANT_WEATHER_EXIT_MAX) return 0;
    w->exit_list[w->exit_count] = handle;
    w->exit_deferred[w->exit_count] = 0;
    w->exit_count++;
    return 1;
}

/* register against the frame whose protected scope is currently active
   (a running shelter dispatch wins, else the innermost active frame) */
int plant_weather_register_handle(tx_t handle) {
    PlantWeather* w = _plant_weather_handling ? _plant_weather_handling
                                              : _plant_weather_head;
    return plant_weather_register(w, handle);
}

/* queue a registered handle as a deferred deallocation within its
   exit-list: it moves out of live_objects into pending_frees and is
   reclaimed by teardown */
int plant_weather_defer_handle(tx_t handle) {
    PlantWeather* w = _plant_weather_handling ? _plant_weather_handling
                                              : _plant_weather_head;
    if (!w || !handle) return 0;
    for (int i = 0; i < w->exit_count; i++) {
        if (!w->exit_deferred[i] && w->exit_list[i] == handle) {
            w->exit_deferred[i] = 1;
            return 1;
        }
    }
    return 0;
}

/* the shelter dispatch makes the (popped) frame the registration target
   so handler temporaries are tracked for cleanup */
void plant_weather_handling_begin(PlantWeather* w) {
    if (!w) return;
    _plant_weather_handling = w;
}

void plant_weather_handling_end(PlantWeather* w) {
    if (w && _plant_weather_handling == w) _plant_weather_handling = NULL;
}

void plant_weather_shelter_enter(PlantWeather* w) {
    if (!w) return;
    w->shelter_mark = w->exit_count;
}

/* purge every handle registered since the shelter body began, so
   handler temporaries, scratch buffers and ARC links are deallocated
   before the shelter scope exits */
void plant_weather_shelter_leave(PlantWeather* w) {
    if (!w) return;
    int mark = (w->shelter_mark < 0) ? 0 : w->shelter_mark;
    if (mark > w->exit_count) mark = w->exit_count;
    for (int i = mark; i < w->exit_count; i++) {
        plant_weather_free_handle(w->exit_list[i]);
    }
    w->exit_count = mark;
    w->shelter_mark = -1;
}

/* v0.48.37d — weather memory telemetry: a structured MAP with
   active_frames (currently tracked execution frames), live_objects
   (active allocations protected within weather scopes), pending_frees
   (deferred deallocations queued within exit-lists) and storm_handlers
   (registered active exception handler hooks). A frame currently
   running a shelter dispatch is counted alongside the active stack. */
tx_t plant_weather_status(void) {
    long frames = 0, live = 0, pending = 0, handlers = 0;
    int handling_in_chain = 0;
    for (PlantWeather* w = _plant_weather_head; w; w = w->next) {
        frames++;
        handlers += w->storm_handlers;
        if (w == _plant_weather_handling) handling_in_chain = 1;
        for (int i = 0; i < w->exit_count; i++) {
            if (w->exit_deferred[i]) pending++;
            else live++;
        }
    }
    if (_plant_weather_handling && !handling_in_chain) {
        PlantWeather* w = _plant_weather_handling;
        frames++;
        handlers += w->storm_handlers;
        for (int i = 0; i < w->exit_count; i++) {
            if (w->exit_deferred[i]) pending++;
            else live++;
        }
    }
    PlantArray* _wt_meta = plant_list_make(8,
        "active_frames", _from_long(frames),
        "live_objects", _from_long(live),
        "pending_frees", _from_long(pending),
        "storm_handlers", _from_long(handlers));
    _wt_meta->kind = 1;
    return (tx_t)_wt_meta;
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.37e — WAIT and LOCK Synchronization Primitives
   plant_msleep (above) implements execution throttling over POSIX
   nanosleep; plant_lock manages a centralized Lock Table: a fixed-
   capacity registry of locking flags keyed by variable value. A key
   marked "locked" refuses further lock attempts (the concurrency
   guard against concurrent access or modification), and can be
   released or probed without disturbing other keys. plant_now_ms
   exposes monotonic milliseconds for timing verification.
   ═══════════════════════════════════════════════════════════════ */
#define PLANT_LOCK_MAX 64

typedef struct {
    tx_t key;
    int  locked;
} PlantLockEntry;

static PlantLockEntry g_lock_table[PLANT_LOCK_MAX];
static long g_lock_count = 0;

/* register a locking flag against the provided variable key. Returns
   "1" on acquisition, "0" when the key is already locked (concurrent
   access prevented), "ERR:undefined" for a null/empty key (undefined
   or out-of-scope variable value) and "ERR:full" when the table is
   exhausted. */
tx_t plant_lock(tx_t key) {
    if (!key || _S(key)[0] == '\0') return (tx_t)"ERR:undefined";
    for (long i = 0; i < g_lock_count; i++) {
        if (g_lock_table[i].locked && strcmp(_S(g_lock_table[i].key), _S(key)) == 0)
            return (tx_t)"0";
    }
    if (g_lock_count >= PLANT_LOCK_MAX) return (tx_t)"ERR:full";
    g_lock_table[g_lock_count].key = key;
    g_lock_table[g_lock_count].locked = 1;
    g_lock_count++;
    return (tx_t)"1";
}

/* release a locked key. Returns "1" when released, "0" when the key
   was not locked (release of an unheld lock is a no-op refusal). */
tx_t plant_lock_release(tx_t key) {
    if (!key || _S(key)[0] == '\0') return (tx_t)"0";
    for (long i = 0; i < g_lock_count; i++) {
        if (g_lock_table[i].locked && strcmp(_S(g_lock_table[i].key), _S(key)) == 0) {
            g_lock_table[i] = g_lock_table[g_lock_count - 1];
            g_lock_count--;
            return (tx_t)"1";
        }
    }
    return (tx_t)"0";
}

/* probe whether a key is currently locked (protected): "1" means any
   access or modification attempt on the resource would be blocked. */
tx_t plant_lock_held(tx_t key) {
    if (!key || _S(key)[0] == '\0') return (tx_t)"0";
    for (long i = 0; i < g_lock_count; i++) {
        if (g_lock_table[i].locked && strcmp(_S(g_lock_table[i].key), _S(key)) == 0)
            return (tx_t)"1";
    }
    return (tx_t)"0";
}

/* lock-table telemetry: a structured MAP with the count of held locks */
tx_t plant_lock_status(void) {
    PlantArray* _lk_meta = plant_list_make(2, "locked_count", _from_long(g_lock_count));
    _lk_meta->kind = 1;
    return (tx_t)_lk_meta;
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.38a — storm() Exception Factory
   plant_storm builds a standardized exception object: a MAP carrying
   {type, message} whose backing memory is registered on the ARC heap
   (a wrapper allocation whose payload is the MAP pointer). The object
   therefore persists across setjmp/longjmp stack unwinding instead of
   dying with the throwing call frame. Ownership model: the created
   object carries one reference; plant_throw_obj transfers it to the
   innermost WEATHER checkpoint; unmatched storms move the same
   reference outward frame by frame through plant_calm; a matching
   SHELTER consumes it — the generated dispatch calls
   plant_storm_release after the handler body runs, dropping the count
   to zero so the ARC heap finalizes the object. Classic
   THROW <type> "<msg>". storms never allocate and keep the legacy
   string-based frame fields. Zero/empty type or message arguments are
   normalized: the type falls back to ANY_STORM, the message to the
   registry default (or "(unclassified storm)" for unconventional
   types), mirroring plant_throw's NULL-message behavior.
   v0.48.38b — location backfill: the factory takes file/line/column
   source metadata and packs each field conditionally (non-NULL file,
   positive line/column) into the object, so THROW storm(...). objects
   expose their compile site through SHELTER AS bindings.
   ═══════════════════════════════════════════════════════════════ */

tx_t plant_storm(tx_t type, tx_t msg, tx_t file, long line, long column) {
    tx_t storm_type = type ? type : (tx_t)"ANY_STORM";
    if (_S(storm_type)[0] == '\0') storm_type = (tx_t)"ANY_STORM";
    tx_t smsg = msg;
    if (!smsg || _S(smsg)[0] == '\0')
        smsg = (tx_t)plant_storm_default_message(_S(storm_type));
    PlantArray* list = plant_list_make(4, "type", storm_type, "message", smsg);
    list->kind = 1;
    /* v0.48.38b — conditional source metadata: each field is packed
       only when meaningful (file non-NULL, line/column > 0), keeping
       objects built from legacy two-argument calls byte-identical to
       v0.48.38a output. */
    if (file) {
        plant_list_push(list, (char*)"file");
        plant_list_push(list, (char*)file);
    }
    if (line > 0) {
        plant_list_push(list, (char*)"line");
        plant_list_push(list, (char*)_from_long(line));
    }
    if (column > 0) {
        plant_list_push(list, (char*)"column");
        plant_list_push(list, (char*)_from_long(column));
    }
    tx_t map = (tx_t)list;
    plant_arc_obj* o = (plant_arc_obj*)plant_arc_alloc(sizeof(tx_t));
    if (!o) return map;               /* degraded: unmanaged fallback */
    free(o->data);                    /* payload becomes the MAP itself */
    o->data = (char*)map;             /* destroy() frees the MAP */
    return map;
}

/* raise a factory storm object through the innermost checkpoint. The
   type/message strings are extracted from the MAP and cached in the
   frame (so plant_exc_type/plant_exc_msg and the generated __et/__em
   captures stay uniform), and the object reference is transferred to
   the frame's ownership for the propagation window. */
void plant_throw_obj(tx_t obj) {
    PlantWeather* w = _plant_weather_head;
    const char* type = "ANY_STORM";
    const char* msg = "(unclassified storm)";
    if (obj) {
        tx_t t = _map_get((PlantArray*)obj, (tx_t)"type");
        tx_t m = _map_get((PlantArray*)obj, (tx_t)"message");
        if (t && _S(t)[0] != '\0') type = _S(t);
        if (m && _S(m)[0] != '\0') msg = _S(m);
    }
    if (w == NULL) {
        char _wb[256]; snprintf(_wb, 256, "[WEATHER] unhandled storm: %s %s", type, msg);
        plant_warning(_wb);
        fflush(stderr);
        abort();
    }
    w->raised = 1;
    w->exc_obj = obj;
    w->exc_type = (char*)type;
    w->exc_msg = (char*)msg;
    longjmp(w->buf, 1);
}

/* the binding value a SHELTER AS-clause receives: the factory object
   when the storm was raised as one, else the classic message string
   (preserving the pre-v0.48.38a semantics for AS e). */
tx_t plant_exc_val(void) {
    PlantWeather* w = _plant_weather_head;
    if (w && w->exc_obj) return (tx_t)w->exc_obj;
    if (w && w->exc_msg) return (tx_t)w->exc_msg;
    return (tx_t)"";
}

/* drop the factory object's reference once the handler logic has run.
   The scan locates the ARC wrapper whose payload is the MAP; classic
   message strings (not factory objects) are never in the ARC heap, so
   release is a safe no-op for them. */
void plant_storm_release(tx_t obj) {
    if (!obj) return;
    for (plant_arc_obj* o = g_arc_head; o; o = o->next) {
        if (o->size == sizeof(tx_t) && o->data &&
            *(tx_t*)o->data == obj) {
            plant_arc_release((tx_t)o);
            return;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.37 — Memory Safety Layer (EVAPORATE)
   Fixed-size string slabs (the _cat family allocates from a 64-byte
   block pool when the result fits), explicit deallocation (the FREE
   statement maps to plant_mem_free), allocator byte accounting for
   plant_mem_report, the audit scanner (plant_mem_scan), the
   DistributedHeap with a consistent-hash ring over ARC segments, and
   SAFE boundary copy/transfer enforcement.
   ═══════════════════════════════════════════════════════════════ */
#define PLANT_SLAB_BLOCK   64
#define PLANT_SLAB_BLOCKS  1024
static char* g_slab_region = NULL;
static void* g_slab_free = NULL;      /* singly-linked free-block list */
static long  g_slab_live = 0;
static long  g_slab_blocks = PLANT_SLAB_BLOCKS;

static void plant_slab_init(void) {
    if (g_slab_region) return;
    g_slab_region = (char*)malloc((size_t)PLANT_SLAB_BLOCK * (size_t)g_slab_blocks);
    if (!g_slab_region) { g_slab_blocks = 0; return; }
    char* head = NULL;
    for (long i = 0; i < g_slab_blocks; i++) {
        char* b = g_slab_region + (size_t)i * PLANT_SLAB_BLOCK;
        *(char**)b = head;             /* free-list next pointer */
        head = b;
    }
    g_slab_free = head;                /* last block next = NULL (terminates) */
}

/* pop a block; returns NULL when the pool is empty (caller falls
   back to malloc). Blocks are 64 bytes — enough for small strings. */
char* plant_str_slab_alloc(size_t n) {
    if (n == 0 || n > (size_t)PLANT_SLAB_BLOCK) return NULL;
    plant_slab_init();
    if (!g_slab_free) return NULL;
    void* p = g_slab_free;
    g_slab_free = *(void**)p;
    g_slab_live++;
    return (char*)p;
}

static int plant_in_slab(const void* p) {
    return g_slab_region &&
           (const char*)p >= g_slab_region &&
           (const char*)p < g_slab_region + (size_t)PLANT_SLAB_BLOCK * (size_t)g_slab_blocks;
}

static void plant_slab_free(void* p) {
    *(void**)p = g_slab_free;
    g_slab_free = p;
    g_slab_live--;
}

/* FREE statement target: returns NULL after deallocating so the
   codegen can write the variable to NULL (double-free safe). Small
   integers / static digit strings (< 64KB) are refused; PlantArray
   containers are freed shallowly (the element strings remain the
   caller's). String literals are NOT detectable — freeing one is a
   user error, as in C. */
tx_t plant_mem_free(tx_t v) {
    if (!v) return NULL;
    if ((uintptr_t)v < 65536) return v;          /* small int / static table */
    if (plant_in_slab(v)) {
        plant_slab_free(v);
        g_bal_bytes -= PLANT_SLAB_BLOCK;
        return NULL;
    }
    PlantArray* p = (PlantArray*)v;
    if (p->magic == PLANT_ARRAY_MAGIC) {
        if (p->items) free(p->items);
        g_bal_bytes -= (long)sizeof(PlantArray) + p->capacity * (long)sizeof(char*);
        free(p);
        return NULL;
    }
    free(v);
    return NULL;
}

/* unified MAP of live bytes by allocator owner:
   arena / fast / arc / balanced / slab */
tx_t plant_mem_report(void) {
    plant_fast_init();
    plant_slab_init();
    PlantArray* _mr_meta = plant_list_make(10,
        "arena", _from_long(g_arena_live_bytes),
        "fast", _from_long((long)(g_fast.used + g_fast_esc_bytes)),
        "arc", _from_long(g_arc_live_bytes),
        "balanced", _from_long(g_bal_bytes),
        "slab", _from_long(g_slab_live * PLANT_SLAB_BLOCK));
    _mr_meta->kind = 1;
    return (tx_t)_mr_meta;
}

/* audit scanner: flags recurrent FAST_ESCALATE events, ARC churn
   (allocations without matching frees), slab exhaustion and the
   arena miss ratio. Returns a MAP with counters + warnings string. */
tx_t plant_mem_scan(void) {
    plant_fast_init();
    long escal = 0;
    long n = g_audit_count;
    if (n > PLANT_AUDIT_CAP) n = PLANT_AUDIT_CAP;
    long base = g_audit_head - n;
    if (base < 0) base += PLANT_AUDIT_CAP;
    for (long i = 0; i < n; i++) {
        plant_audit_ev* e = &g_audit[(base + i) % PLANT_AUDIT_CAP];
        if (strcmp(e->kind, "FAST_ESCALATE") == 0) escal++;
    }
    long miss = 0;
    if (g_arena_hits + g_arena_misses > 0)
        miss = g_arena_misses * 100 / (g_arena_hits + g_arena_misses);
    char warn[160] = "";
    long wl = 0;
    if (escal > 0) wl += (long)snprintf(warn + wl, sizeof(warn) - (size_t)wl, "fast_escalations=%ld", escal);
    if (g_arc_allocs > 100 && g_arc_allocs - g_arc_frees > g_arc_allocs / 10) {
        wl += (long)snprintf(warn + wl, sizeof(warn) - (size_t)wl, "%sarc_churn:allocs=%ld,frees=%ld", wl ? "," : "", g_arc_allocs, g_arc_frees);
    }
    if (g_slab_live >= g_slab_blocks)
        wl += (long)snprintf(warn + wl, sizeof(warn) - (size_t)wl, "%sslab_exhausted", wl ? "," : "");
    if (!wl) snprintf(warn, sizeof(warn), "clean");
    PlantArray* _ms_meta = plant_list_make(12,
        "fast_escalations", _from_long(escal),
        "arc_allocs", _from_long(g_arc_allocs),
        "arc_frees", _from_long(g_arc_frees),
        "arc_live", _from_long(g_arc_live),
        "arena_miss_pct", _from_long(miss),
        "slab_blocks", _from_long(g_slab_live),
        "warnings", warn);
    _ms_meta->kind = 1;
    return (tx_t)_ms_meta;
}

/* ── DistributedHeap: consistent-hash ring over ARC segments ──── */
static uint64_t plant_dist_hash(const char* s) {
    uint64_t h = 14695981039346656037ULL;
    if (!s) return h;
    while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; }
    return h;
}

void plant_dist_ring_build(void) {
    long nn = g_dist_nodes;
    if (nn < 1) nn = 1;
    if (nn > PLANT_DIST_MAX_NODES) nn = PLANT_DIST_MAX_NODES;
    g_dist_nodes = nn;
    long k = 0;
    for (long i = 0; i < nn; i++) {
        for (long v = 0; v < g_dist_vpts; v++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%ld:%ld", i, v);
            g_dist_ring[k] = plant_dist_hash(buf);
            g_dist_ring_node[k] = i;
            k++;
        }
    }
    /* insertion sort over the (hash, node) pairs */
    for (long i = 1; i < k; i++) {
        uint64_t t = g_dist_ring[i];
        long tn = g_dist_ring_node[i];
        long j = i - 1;
        while (j >= 0 && g_dist_ring[j] > t) {
            g_dist_ring[j + 1] = g_dist_ring[j];
            g_dist_ring_node[j + 1] = g_dist_ring_node[j];
            j--;
        }
        g_dist_ring[j + 1] = t;
        g_dist_ring_node[j + 1] = tn;
    }
    g_dist_ring_n = k;
}

/* deterministic placement: first ring point >= hash(key), wrapping
   around the ring — consistent hashing gives stable placement */
static long plant_dist_place(const char* key) {
    if (!g_dist_ring_n) plant_dist_ring_build();
    uint64_t h = plant_dist_hash(key);
    long lo = 0, hi = g_dist_ring_n;
    while (lo < hi) {
        long mid = (lo + hi) / 2;
        if (g_dist_ring[mid] < h) lo = mid + 1;
        else hi = mid;
    }
    if (lo >= g_dist_ring_n) lo = 0;
    return g_dist_ring_node[lo];
}

/* lease-based eviction: node over its byte cap reclaims objects with
   zero refs or an expired lease */
static long plant_dist_evict(long node) {
    long now = plant_ms();
    long reclaimed = 0;
    plant_arc_obj* o = g_arc_head;
    while (o) {
        plant_arc_obj* nx = o->next;
        if (o->node == node && !o->deferred &&
            (o->refs <= 0 || (o->leased_until_ms > 0 && o->leased_until_ms <= now))) {
            g_dist_node_bytes[node] -= (long)o->size;
            plant_arc_destroy(o, 1);
            reclaimed++;
        }
        o = nx;
    }
    return reclaimed;
}

tx_t plant_dist_init(tx_t nodesv) {
    long n = (long)nodesv;
    if (n >= 1 && n <= PLANT_DIST_MAX_NODES) g_dist_nodes = n;
    memset(g_dist_node_bytes, 0, sizeof(g_dist_node_bytes));
    plant_dist_ring_build();
    static char msg[96];
    snprintf(msg, sizeof(msg), "nodes=%ld points=%ld", g_dist_nodes, g_dist_ring_n);
    plant_audit_log("DIST_INIT", msg);
    return (tx_t)"1";
}

tx_t plant_dist_alloc(tx_t sizev, tx_t keyv) {
    const char* key = _S(keyv);
    if (!key || !*key) key = "default";
    long node = plant_dist_place(key);
    tx_t obj = plant_arc_alloc(sizev);
    if (!obj) return NULL;
    plant_arc_obj* o = (plant_arc_obj*)obj;
    o->node = node;
    g_dist_node_bytes[node] += (long)o->size;
    g_dist_allocs++;
    static char msg[96];
    snprintf(msg, sizeof(msg), "seq=%ld node=%ld size=%ld key=%s", o->alloc_seq, node, (long)o->size, key);
    plant_audit_log("DIST_ALLOC", msg);
    if (g_dist_node_cap[node] > 0 && g_dist_node_bytes[node] > g_dist_node_cap[node]) {
        long ev = plant_dist_evict(node);
        if (ev > 0) {
            g_dist_evicts += ev;
            snprintf(msg, sizeof(msg), "node=%ld reclaimed=%ld", node, ev);
            plant_audit_log("DIST_EVICT", msg);
        }
    }
    return obj;
}

tx_t plant_dist_node(tx_t objv) {
    if (!objv) return _from_long(-1);
    return _from_long(((plant_arc_obj*)objv)->node);
}

tx_t plant_dist_release(tx_t objv) {
    plant_arc_obj* o = (plant_arc_obj*)objv;
    if (!o) return (tx_t)"0";
    if (o->node >= 0 && o->node < g_dist_nodes)
        g_dist_node_bytes[o->node] -= (long)o->size;
    return plant_arc_release(objv);
}

tx_t plant_dist_status(void) {
    static char buf[256];
    long used = 0;
    for (long i = 0; i < g_dist_nodes; i++) used += g_dist_node_bytes[i];
    snprintf(buf, sizeof(buf), "nodes=%ld points=%ld allocs=%ld evicts=%ld live_bytes=%ld",
             g_dist_nodes, g_dist_ring_n, g_dist_allocs, g_dist_evicts, used);
    return buf;
}

/* SAFE boundary enforcement: payloads at or below the channel
   threshold must cross as copies (no shared buffers); larger payloads
   may transfer zero-copy. Verifies the channel chose correctly and
   flags violations in the audit ring. */
tx_t plant_safe_boundary_copy(tx_t chanv, tx_t payload) {
    plant_channel* ch = (plant_channel*)chanv;
    if (!ch || !payload) return (tx_t)"0";
    const char* p = _S(payload);
    size_t n = strlen(p);
    if (n <= (size_t)ch->threshold) {
        if (ch->buf == payload) {
            plant_audit_log("BOUNDARY_COPY", "copy required, shared buffer observed");
            return (tx_t)"0";
        }
        return (tx_t)"1";
    }
    return (tx_t)"1";   /* zero-copy transfer allowed above threshold */
}

/* ═══════════════════════════════════════════════════════════════
   v0.48.37c: True SAFE Isolation — real worker processes.

   The WarmProcessPool emulation is replaced at the SAFE boundary by
   genuine OS isolation: each SAFE action call dispatches to a
   dedicated worker process (fork + exec of the same binary with
   --plant-worker), talking IPC over a SOCK_SEQPACKET socketpair.
   The program binary acts as both client and server: the parent
   registers SAFE adapters in plant_safe_register, then
   plant_maybe_run_worker() turns a worker instance into the server
   loop. Data crosses the boundary through a versioned wire codec
   (NULL/INT/STR/LIST); payloads above the 1MB channel threshold are
   zero-copy transferred via memfd + SCM_RIGHTS instead of copies.
   A per-job output pipe relays the worker's stdout/stderr into the
   parent. Heartbeat recovery uses waitpid(WNOHANG|WUNTRACED):
   stopped (SIGSTOP-injected) or dead workers are SIGKILLed, reaped
   and respawned in place (restart counters preserved).
   ═══════════════════════════════════════════════════════════════ */
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>

/* memfd_create: glibc provides sys/memfd.h from 2.27 on; older
   toolchains need the raw syscall instead. */
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif
#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002U
#endif
#ifndef memfd_create
static inline int plant_memfd_create(const char* name, unsigned int flags) {
#if defined(__linux__) && defined(SYS_memfd_create)
    return (int)syscall(SYS_memfd_create, name, flags);
#else
    (void)name; (void)flags; errno = ENOSYS; return -1;
#endif
}
#define memfd_create plant_memfd_create
#endif

#define PLANT_RW_MAX         16
#define PLANT_RW_WIRE_MAGIC  0x504C5346L            /* "PLSF" */
#define PLANT_RW_JOB         0
#define PLANT_RW_RESULT      1
#define PLANT_RW_ERROR       2
#define PLANT_RW_PING        3
#define PLANT_RW_PONG        4
#define PLANT_RW_READY       5
#define PLANT_RW_MAXARG      64
#define PLANT_RW_MAXFD       16
#define PLANT_RW_BUFSZ       (2097152 + 48)
#define PLANT_RW_READY_WAIT  250                    /* ms to wait for an in-flight spawn */

typedef struct plant_rw_hdr {
    long magic;
    long kind;
    long idx;
    long argc;
    long size;
    long nfd;
} plant_rw_hdr;                                    /* 48 bytes */

typedef struct plant_rw {
    pid_t pid;
    int   sock;                                    /* parent end of the job socketpair */
    long  state;                                   /* 0 idle, 1 busy, 2 stalled */
    long  ready;                                   /* READY handshake received */
    long  dead;                                    /* reaped / failed */
    long  served;
    char  job_name[64];
    long  last_msg_ms;
} plant_rw;

static plant_rw g_rw[PLANT_RW_MAX];
static long g_rw_count = 0;
static long g_rw_spawns = 0;
static long g_rw_fallback = 0;
static long g_rw_served = 0;
static int  g_rw_inited = 0;

typedef tx_t (*plant_safe_adapter)(int argc, tx_t* argv);
typedef struct { const char* name; plant_safe_adapter fn; } plant_safe_reg;
static plant_safe_reg g_safe_regs[64];
static long g_safe_reg_n = 0;

void plant_safe_register(const char* name, plant_safe_adapter fn) {
    if (!name || !fn || g_safe_reg_n >= 64) return;
    for (long i = 0; i < g_safe_reg_n; i++)
        if (strcmp(g_safe_regs[i].name, name) == 0) return;
    g_safe_regs[g_safe_reg_n].name = name;
    g_safe_regs[g_safe_reg_n].fn = fn;
    g_safe_reg_n++;
}

static long plant_safe_index(const char* name) {
    for (long i = 0; i < g_safe_reg_n; i++)
        if (strcmp(g_safe_regs[i].name, name) == 0) return i;
    return -1;
}

/* ffi-facing telemetry for the real worker pool (same format string as
   v0.48.37b so the regression .expected files stay stable). */
tx_t plant_safe_status(void) {
    static char buf[192];
    long workers = 0;
    long busy = 0;
    for (long i = 0; i < PLANT_RW_MAX; i++) {
        plant_rw* w = &g_rw[i];
        if (w->dead || w->pid <= 0) continue;
        workers++;
        if (w->state == 1) busy++;
    }
    snprintf(buf, sizeof(buf),
             "workers=%ld busy=%ld spawns=%ld restarts=%ld fallback=%ld served=%ld",
             workers, busy, g_rw_spawns, g_pool_restarts,
             g_rw_fallback, g_rw_served);
    return buf;
}

/* deterministic fault injection: stall SIGSTOPs the real worker that is
   currently serving <name> and marks it stalled; the next heartbeat
   sweep reaps it and respawns in place. starve SIGSTOPs every live
   worker so a queue wait forms and the pool grows via starvation. */
tx_t plant_safe_stall(tx_t namev) {
    const char* want = _S(namev);
    for (long i = 0; i < PLANT_RW_MAX; i++) {
        plant_rw* w = &g_rw[i];
        if (w->dead || w->pid <= 0) continue;
        if (strcmp(w->job_name, want) == 0) {
            kill(w->pid, SIGSTOP);
            /* SIGSTOP delivery is asynchronous; peek (WNOWAIT, non-consuming)
               until the stop is observable so the recovery sweep's
               waitpid(WNOHANG|WUNTRACED) reliably sees it. */
            siginfo_t si;
            for (int t = 0; t < 500; t++) {
                memset(&si, 0, sizeof(si));
                if (waitid(P_PID, (id_t)w->pid, &si,
                           WEXITED | WSTOPPED | WCONTINUED | WNOWAIT) == 0 &&
                    si.si_pid == w->pid &&
                    (si.si_code == CLD_STOPPED || si.si_code == CLD_EXITED ||
                     si.si_code == CLD_KILLED || si.si_code == CLD_DUMPED)) break;
                usleep(2000);
            }
            w->state = 2;
            w->last_msg_ms = plant_ms() - 10000;
            return (tx_t)"0";
        }
    }
    return (tx_t)"-1";
}

tx_t plant_safe_starve(tx_t msv) {
    g_pending_wait_ms = (long)msv;
    for (long i = 0; i < PLANT_RW_MAX; i++) {
        plant_rw* w = &g_rw[i];
        if (w->dead || w->pid <= 0) continue;
        kill(w->pid, SIGSTOP);
        w->state = 2;
    }
    return (tx_t)"0";
}

/* ── wire codec ────────────────────────────────────────────────── */
/* tags: 'N' null · 'I' int64 LE · 'S' len(4B)+bytes · 'A' count(4B)+children · 'F' len(8B)+memfd */
static long plant_rw_strlen_safe(tx_t v) {
    const char* s = _S(v);
    return s ? (long)strlen(s) : 0;
}

/* encode one value; returns bytes written (or -1 when unserializable).
   values at/above threshold become 'F' entries carried by fds[] via cmsg. */
static long plant_rw_encode(char* buf, tx_t v, long depth,
                            int* fds, long* nfd, long threshold) {
    if (!v) { buf[0] = 'N'; return 1; }
    if ((uintptr_t)v < 4096) {
        buf[0] = 'I';
        long n = (long)(intptr_t)v;
        memcpy(buf + 1, &n, 8);
        return 9;
    }
    PlantArray* p = (PlantArray*)v;
    if (p->magic == PLANT_ARRAY_MAGIC && depth < 4) {
        buf[0] = 'A';
        long cnt = p->count;
        memcpy(buf + 1, &cnt, 4);
        long off = 5;
        for (int64_t i = 0; i < p->count; i++) {
            long sz = plant_rw_encode(buf + off, p->items[i], depth + 1, fds, nfd, threshold);
            if (sz < 0) return -1;
            off += sz;
            if (off > PLANT_RW_BUFSZ - 64) return -1;
        }
        return off;
    }
    long len = plant_rw_strlen_safe(v);
    if (len > threshold && *nfd < PLANT_RW_MAXFD) {
        int fd = memfd_create("plsf", 0);
        if (fd < 0) return -1;
        const char* s = _S(v);
        long left = len;
        const char* wp = s;
        while (left > 0) {
            ssize_t wr = write(fd, wp, (size_t)left);
            if (wr <= 0) { close(fd); return -1; }
            wp += wr; left -= wr;
        }
        lseek(fd, 0, SEEK_SET);
        buf[0] = 'F';
        memcpy(buf + 1, &len, 8);
        fds[*nfd] = fd;
        (*nfd)++;
        return 9;
    }
    if (len > PLANT_RW_BUFSZ - 64) return -1;
    buf[0] = 'S';
    memcpy(buf + 1, &len, 4);
    memcpy(buf + 5, _S(v), (size_t)len);
    return 5 + len;
}

/* decode one value from buf at *off; 'F' entries pull data from fds[*fidx] */
static tx_t plant_rw_decode(const char* buf, long* off,
                            int* fds, long nfd, long* fidx) {
    char tag = buf[*off];
    (*off)++;
    if (tag == 'N') return NULL;
    if (tag == 'I') {
        long n;
        memcpy(&n, buf + *off, 8);
        *off += 8;
        return (tx_t)(intptr_t)n;
    }
    if (tag == 'F') {
        long len;
        memcpy(&len, buf + *off, 8);
        *off += 8;
        char* out = malloc((size_t)len + 1);
        if (!out) return NULL;
        long got = 0;
        int fd = (*fidx < nfd) ? fds[(*fidx)++] : -1;
        if (fd >= 0) {
            while (got < len) {
                ssize_t r = read(fd, out + got, (size_t)(len - got));
                if (r <= 0) break;
                got += r;
            }
            close(fd);
        }
        out[len] = 0;
        return out;
    }
    if (tag == 'S') {
        long len = 0;
        memcpy(&len, buf + *off, 4);
        *off += 4;
        char* out = malloc((size_t)len + 1);
        if (!out) return NULL;
        memcpy(out, buf + *off, (size_t)len);
        *off += len;
        out[len] = 0;
        return out;
    }
    if (tag == 'A') {
        long cnt = 0;
        memcpy(&cnt, buf + *off, 4);
        *off += 4;
        PlantArray* arr = plant_list_create(16);
        for (long i = 0; i < cnt; i++) {
            tx_t el = plant_rw_decode(buf, off, fds, nfd, fidx);
            arr = plant_list_add(arr, el);
        }
        return arr;
    }
    return NULL;
}

/* ── process spawning ─────────────────────────────────────────── */
static long plant_rw_spawn(int slot) {
    if (slot < 0) {
        for (long i = 0; i < PLANT_RW_MAX; i++)
            if (g_rw[i].dead) { slot = (int)i; break; }
    }
    if (slot < 0) {
        for (long i = 0; i < PLANT_RW_MAX; i++)
            if (g_rw[i].pid <= 0 && !g_rw[i].ready && g_rw[i].sock <= 0) { slot = (int)i; break; }
    }
    if (slot < 0) return -1;
    plant_rw* w = &g_rw[slot];
    if (w->sock > 0) close(w->sock);
    w->sock = 0;
    w->ready = 0;
    w->dead = 0;
    w->state = 0;
    w->served = 0;
    w->job_name[0] = 0;

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) != 0) return -1;
    int nullfd = open("/dev/null", O_WRONLY);
    pid_t pid = fork();
    if (pid < 0) {
        close(sv[0]); close(sv[1]);
        if (nullfd >= 0) close(nullfd);
        return -1;
    }
    if (pid == 0) {
        dup2(sv[1], 0);
        if (nullfd >= 0) { dup2(nullfd, 1); dup2(nullfd, 2); }
        close(sv[0]);
        close(sv[1]);
        if (nullfd >= 0) close(nullfd);
        if (g_cli_argv0 && g_cli_argv0[0]) {
            char* const av[] = { g_cli_argv0, (char*)"--plant-worker", NULL };
            execv(g_cli_argv0, av);
        }
        _exit(127);
    }
    close(sv[1]);
    if (nullfd >= 0) close(nullfd);
    w->pid = pid;
    w->sock = sv[0];
    w->last_msg_ms = plant_ms();
    g_rw_count++;
    g_rw_spawns++;
    return slot;
}

static void plant_rw_init(void) {
    if (g_rw_inited) return;
    g_rw_inited = 1;
    if (g_pool_cap < 1) g_pool_cap = 1;
    if (g_pool_cap > PLANT_SAFE_MAX_WORKERS) g_pool_cap = PLANT_SAFE_MAX_WORKERS;
    if (g_pool_max < g_pool_cap) g_pool_max = g_pool_cap;
    if (g_pool_max > PLANT_SAFE_MAX_WORKERS) g_pool_max = PLANT_SAFE_MAX_WORKERS;
    for (long i = 0; i < g_pool_cap; i++) plant_rw_spawn(-1);
}

/* non-blocking drain of READY/PONG handshakes (a spawned worker's
   first message) — marks workers ready so acquire() can reuse them */
static void plant_rw_drain_readies(long max_ms) {
    long deadline = plant_ms() + max_ms;
    do {
        int progressed = 0;
        for (long i = 0; i < PLANT_RW_MAX; i++) {
            plant_rw* w = &g_rw[i];
            if (w->dead || w->pid <= 0 || w->sock <= 0) continue;
            for (;;) {
                char buf[512];
                struct iovec iov = { buf, sizeof(buf) };
                char cmsg_b[CMSG_SPACE(sizeof(int) * PLANT_RW_MAXFD)];
                struct msghdr mh;
                memset(&mh, 0, sizeof(mh));
                mh.msg_iov = &iov;
                mh.msg_iovlen = 1;
                mh.msg_control = cmsg_b;
                mh.msg_controllen = sizeof(cmsg_b);
                ssize_t n = recvmsg(w->sock, &mh, MSG_DONTWAIT);
                if (n < (ssize_t)sizeof(plant_rw_hdr)) break;
                plant_rw_hdr* h = (plant_rw_hdr*)buf;
                if (h->magic != PLANT_RW_WIRE_MAGIC) break;
                if (h->kind == PLANT_RW_READY) { w->ready = 1; w->last_msg_ms = plant_ms(); progressed = 1; }
                else if (h->kind == PLANT_RW_PONG) { w->last_msg_ms = plant_ms(); progressed = 1; }
                else break;
            }
        }
        if (progressed) return;
        plant_msleep(2);
    } while (plant_ms() < deadline);
}

/* acquire a ready idle worker; spawns when the pool is under max and
   nothing is ready (starvation growth); returns -1 when starved at
   max (caller falls back to inline execution) */
static long plant_rw_acquire(const char* name) {
    if (!g_rw_inited) plant_rw_init();
    for (long i = 0; i < PLANT_RW_MAX; i++) {
        plant_rw* w = &g_rw[i];
        if (w->dead || w->pid <= 0) continue;
        if (w->ready && w->state == 0) {
            w->state = 1;
            if (name) snprintf(w->job_name, sizeof(w->job_name), "%s", name);
            w->last_msg_ms = plant_ms();
            return i;
        }
    }
    /* wait for an in-flight spawn/respawn to become ready */
    plant_rw_drain_readies(PLANT_RW_READY_WAIT);
    for (long i = 0; i < PLANT_RW_MAX; i++) {
        plant_rw* w = &g_rw[i];
        if (w->dead || w->pid <= 0) continue;
        if (w->ready && w->state == 0) {
            w->state = 1;
            if (name) snprintf(w->job_name, sizeof(w->job_name), "%s", name);
            w->last_msg_ms = plant_ms();
            return i;
        }
    }
    if (g_rw_count < g_pool_max) {
        long i = plant_rw_spawn(-1);
        if (i >= 0) {
            g_rw[i].state = 1;
            if (name) snprintf(g_rw[i].job_name, sizeof(g_rw[i].job_name), "%s", name);
            return i;
        }
    }
    return -1;
}

/* ── worker server loop (child side, never returns) ────────────── */
static void plant_rw_send_simple(long kind) {
    plant_rw_hdr h;
    memset(&h, 0, sizeof(h));
    h.magic = PLANT_RW_WIRE_MAGIC;
    h.kind = kind;
    struct iovec iov = { &h, sizeof(h) };
    struct msghdr mh;
    memset(&mh, 0, sizeof(mh));
    mh.msg_iov = &iov;
    mh.msg_iovlen = 1;
    sendmsg(0, &mh, 0);
}

static void plant_rw_worker_loop(void) {
    /* handshake: this worker is up and its registry is populated */
    plant_rw_send_simple(PLANT_RW_READY);
    char* buf = malloc(PLANT_RW_BUFSZ);
    if (!buf) plant_fatal("plant_rw_worker_loop: malloc failed for buf");
    for (;;) {
        struct iovec iov = { buf, PLANT_RW_BUFSZ };
        char cmsg_b[CMSG_SPACE(sizeof(int) * PLANT_RW_MAXFD)];
        struct msghdr mh;
        memset(&mh, 0, sizeof(mh));
        mh.msg_iov = &iov;
        mh.msg_iovlen = 1;
        mh.msg_control = cmsg_b;
        mh.msg_controllen = sizeof(cmsg_b);
        ssize_t n = recvmsg(0, &mh, 0);
        if (n < (ssize_t)sizeof(plant_rw_hdr)) {
            if (n <= 0) _exit(0);
            continue;
        }
        plant_rw_hdr* h = (plant_rw_hdr*)buf;
        if (h->magic != PLANT_RW_WIRE_MAGIC) continue;
        if (h->kind == PLANT_RW_PING) { plant_rw_send_simple(PLANT_RW_PONG); continue; }
        if (h->kind != PLANT_RW_JOB) continue;

        int fds[PLANT_RW_MAXFD];
        long nfd = 0;
        struct cmsghdr* cm;
        for (cm = CMSG_FIRSTHDR(&mh); cm; cm = CMSG_NXTHDR(&mh, cm)) {
            if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS) {
                size_t cnt = (cm->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                memcpy(fds + nfd, CMSG_DATA(cm), cnt * sizeof(int));
                nfd += (long)cnt;
            }
        }
        long fidx = 0;
        if (nfd > 0 && fidx < nfd) {   /* fds[0] = per-job output pipe */
            dup2(fds[0], 1);
            dup2(fds[0], 2);
            if (fds[0] > 2) close(fds[0]);  /* never close the pipe fd itself when it landed on 0/1/2 */
            fidx = 1;
        }
        long idx = h->idx;
        long argc = h->argc;
        tx_t argv[PLANT_RW_MAXARG];
        memset(argv, 0, sizeof(argv));
        long off = sizeof(plant_rw_hdr);
        for (long i = 0; i < argc && i < PLANT_RW_MAXARG; i++)
            argv[i] = plant_rw_decode(buf, &off, fds, nfd, &fidx);

        tx_t res = NULL;
        if (idx >= 0 && idx < g_safe_reg_n && g_safe_regs[idx].fn)
            res = g_safe_regs[idx].fn((int)argc, argv);
        /* push any stdio-buffered worker output (plant_print uses printf)
           into the out-pipe before the parent consumes the RESULT */
        fflush(stdout);
        fflush(stderr);

        int rfds[PLANT_RW_MAXFD];
        long rnfd = 0;
        char* rbuf = malloc(PLANT_RW_BUFSZ);
        if (!rbuf) { plant_fatal("plant_rw_worker_loop: malloc failed for rbuf"); }
        long rsz = plant_rw_encode(rbuf + sizeof(plant_rw_hdr), res, 0,
                                   rfds, &rnfd, g_safe_channel_threshold);
        plant_rw_hdr rh;
        memset(&rh, 0, sizeof(rh));
        rh.magic = PLANT_RW_WIRE_MAGIC;
        rh.kind = (rsz < 0) ? PLANT_RW_ERROR : PLANT_RW_RESULT;
        rh.size = (rsz < 0) ? 0 : rsz;
        rh.nfd = rnfd;
        memcpy(rbuf, &rh, sizeof(rh));
        if (rsz < 0) rsz = 0;
        struct iovec riov = { rbuf, sizeof(plant_rw_hdr) + (size_t)rsz };
        char rcmsg_b[CMSG_SPACE(sizeof(int) * PLANT_RW_MAXFD)];
        memset(rcmsg_b, 0, sizeof(rcmsg_b));
        struct msghdr rmh;
        memset(&rmh, 0, sizeof(rmh));
        rmh.msg_iov = &riov;
        rmh.msg_iovlen = 1;
        if (rnfd > 0) {
            rmh.msg_control = rcmsg_b;
            rmh.msg_controllen = CMSG_SPACE(sizeof(int) * rnfd);
            struct cmsghdr* rcm = (struct cmsghdr*)rcmsg_b;
            rcm->cmsg_level = SOL_SOCKET;
            rcm->cmsg_type = SCM_RIGHTS;
            rcm->cmsg_len = CMSG_LEN(sizeof(int) * rnfd);
            memcpy(CMSG_DATA(rcm), rfds, sizeof(int) * rnfd);
        }
        sendmsg(0, &rmh, 0);
        free(rbuf);
        /* free decoded string arguments (big memfd transfers included);
           result encoding is already done, so nothing retains them */
        for (long i = 0; i < argc && i < PLANT_RW_MAXARG; i++) {
            tx_t av = argv[i];
            if (!av || (uintptr_t)av < 4096) continue;
            PlantArray* ap = (PlantArray*)av;
            if (ap->magic == PLANT_ARRAY_MAGIC) continue;
            free(av);
        }
        /* release this job's stdout/stderr so the parent's relay pipe
           reaches EOF; the next job dup2()s a fresh pipe over them */
        close(1);
        close(2);
    }
}

void plant_maybe_run_worker(void) {
    if (!g_cli_worker_mode) return;
    plant_rw_worker_loop();
    _exit(0);
}

/* ── recovery: reaps stopped/dead workers, respawns them ───────── */
static void plant_rw_respawn(long slot) {
    if (slot < 0 || slot >= PLANT_RW_MAX) return;
    g_rw[slot].dead = 1;
    plant_rw_spawn((int)slot);
}

long plant_rw_recover_sweep(void) {
    long restarts = 0;
    for (long i = 0; i < PLANT_RW_MAX; i++) {
        plant_rw* w = &g_rw[i];
        if (w->dead || w->pid <= 0) continue;
        int st = 0;
        pid_t r = waitpid(w->pid, &st, WNOHANG | WUNTRACED);
        if (r != w->pid) continue;
        if (WIFSTOPPED(st)) {
            kill(w->pid, SIGKILL);
            waitpid(w->pid, &st, 0);
            plant_rw_respawn(i);
            g_pool_restarts++;
            restarts++;
        } else if (WIFEXITED(st) || WIFSIGNALED(st)) {
            plant_rw_respawn(i);
            g_pool_restarts++;
            restarts++;
        }
    }
    return restarts;
}

/* ── result receive with interleaved output relay ──────────────── */
static void plant_rw_relay_append(char** acc, long* accn, long* acccap,
                                  const char* data, long len) {
    if (len <= 0) return;
    if (*accn + len + 1 > *acccap) {
        while (*accn + len + 1 > *acccap) *acccap *= 2;
        *acc = realloc(*acc, (size_t)*acccap);
    }
    memcpy(*acc + *accn, data, (size_t)len);
    *accn += len;
    (*acc)[*accn] = 0;
}

/* returns the decoded result (or NULL for ERROR/refusal); *ok = 1 on
   success, 0 on stall/death (caller recovers + retries). */
static tx_t plant_rw_recv_result(plant_rw* w, int out_r, long* ok) {
    *ok = 0;
    long timeout = g_safe_cfg_response_ms < 2000 ? 2000 : g_safe_cfg_response_ms;
    long deadline = plant_ms() + timeout;
    char* rbuf = malloc(PLANT_RW_BUFSZ);
    if (!rbuf) { if (out_r > 0) close(out_r); return NULL; }
    char* acc = malloc(65536);
    long accn = 0, acccap = 65536;
    int out_open = (out_r > 0);
    for (;;) {
        long now = plant_ms();
        if (now >= deadline) {
            free(rbuf); free(acc);
            if (out_open) close(out_r);
            return NULL;
        }
        struct pollfd pf[2];
        pf[0].fd = w->sock;
        pf[0].events = POLLIN;
        pf[0].revents = 0;
        pf[1].fd = out_r;
        pf[1].events = POLLIN;
        pf[1].revents = 0;
        int pr = poll(pf, out_open ? 2 : 1, (int)(deadline - now));
        if (pr < 0) continue;
        if (pr == 0) { free(rbuf); free(acc); if (out_open) close(out_r); return NULL; }
        if (out_open && (pf[1].revents & (POLLIN | POLLHUP))) {
            char tmp[65536];
            ssize_t n = read(out_r, tmp, sizeof(tmp));
            if (n > 0) plant_rw_relay_append(&acc, &accn, &acccap, tmp, (long)n);
            else { close(out_r); out_open = 0; }
        }
        if (!(pf[0].revents & POLLIN)) continue;
        char cbuf[CMSG_SPACE(sizeof(int) * PLANT_RW_MAXFD)];
        struct iovec iov = { rbuf, PLANT_RW_BUFSZ };
        struct msghdr mh;
        memset(&mh, 0, sizeof(mh));
        mh.msg_iov = &iov;
        mh.msg_iovlen = 1;
        mh.msg_control = cbuf;
        mh.msg_controllen = sizeof(cbuf);
        ssize_t n = recvmsg(w->sock, &mh, 0);
        if (n < (ssize_t)sizeof(plant_rw_hdr)) continue;
        plant_rw_hdr* h = (plant_rw_hdr*)rbuf;
        if (h->magic != PLANT_RW_WIRE_MAGIC) continue;
        if (h->kind == PLANT_RW_READY) { w->ready = 1; w->last_msg_ms = plant_ms(); continue; }
        if (h->kind == PLANT_RW_PONG) { w->last_msg_ms = plant_ms(); continue; }
        if (h->kind != PLANT_RW_RESULT && h->kind != PLANT_RW_ERROR) continue;

        int fds[PLANT_RW_MAXFD];
        long nfd = 0;
        struct cmsghdr* cm;
        for (cm = CMSG_FIRSTHDR(&mh); cm; cm = CMSG_NXTHDR(&mh, cm)) {
            if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS) {
                size_t cnt = (cm->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                memcpy(fds + nfd, CMSG_DATA(cm), cnt * sizeof(int));
                nfd += (long)cnt;
            }
        }
        tx_t res = NULL;
        if (h->kind == PLANT_RW_RESULT) {
            long fidx = 0;
            long off = sizeof(plant_rw_hdr);
            res = plant_rw_decode(rbuf, &off, fds, nfd, &fidx);
        }
        w->last_msg_ms = plant_ms();
        if (accn > 0) { fwrite(acc, 1, (size_t)accn, stdout); fflush(stdout); }
        if (out_open) close(out_r);
        free(rbuf);
        free(acc);
        *ok = 1;
        return res;
    }
}

/* ── the SAFE boundary (parent side) ───────────────────────────── */
tx_t plant_safe_call(const char* name, long argc, ...) {
    if (!name || !g_safe_reg_n) return NULL;
    long idx = plant_safe_index(name);
    if (idx < 0) return NULL;
    /* v0.48.37c: the boundary handshake now runs at the dispatch site
       (the callee prologue still runs inside the worker, where the mode
       stack is empty). FAST and PERSISTENT callers are blocked before
       any process is spawned — the blocked body never executes. */
    if (plant_boundary_block(name, "SAFE")) return NULL;
    {
        static char msg[128];
        snprintf(msg, sizeof(msg), "SAFE %s", name);
        plant_audit_log("MODE_ENTER", msg);
    }

    tx_t args[PLANT_RW_MAXARG];
    memset(args, 0, sizeof(args));
    va_list va;
    va_start(va, argc);
    for (long i = 0; i < argc && i < PLANT_RW_MAXARG; i++) args[i] = va_arg(va, tx_t);
    va_end(va);

    if (!g_rw_inited) plant_rw_init();

    int fds[PLANT_RW_MAXFD];
    long nfd = 0;
    char* pbuf = malloc(PLANT_RW_BUFSZ);
    if (!pbuf) return NULL;
    long psz = 0;
    char* data = pbuf + sizeof(plant_rw_hdr);
    for (long i = 0; i < argc; i++) {
        long sz = plant_rw_encode(data + psz, args[i], 0, fds, &nfd, g_safe_channel_threshold);
        if (sz < 0) { free(pbuf); return NULL; }
        psz += sz;
    }

    /* attempt the dispatch; one retry after recovery, then inline */
    tx_t result = NULL;
    for (int attempt = 0; attempt < 2; attempt++) {
        long wid = plant_rw_acquire(name);
        if (wid < 0) {
            g_rw_fallback++;
            free(pbuf);
            return g_safe_regs[idx].fn((int)argc, args);   /* BALANCED inline fallback */
        }
        plant_rw* w = &g_rw[wid];
        int op[2];
        if (pipe(op) != 0) {
            g_rw_fallback++;
            free(pbuf);
            return g_safe_regs[idx].fn((int)argc, args);
        }
        plant_rw_hdr h;
        memset(&h, 0, sizeof(h));
        h.magic = PLANT_RW_WIRE_MAGIC;
        h.kind = PLANT_RW_JOB;
        h.idx = idx;
        h.argc = argc;
        h.size = psz;
        h.nfd = nfd + 1;
        memcpy(pbuf, &h, sizeof(h));

        char cmsg_b[CMSG_SPACE(sizeof(int) * PLANT_RW_MAXFD)];
        struct iovec iov = { pbuf, sizeof(plant_rw_hdr) + (size_t)psz };
        struct msghdr mh;
        memset(&mh, 0, sizeof(mh));
        mh.msg_iov = &iov;
        mh.msg_iovlen = 1;
        mh.msg_control = cmsg_b;
        mh.msg_controllen = sizeof(cmsg_b);
        {
            struct cmsghdr* cm = (struct cmsghdr*)cmsg_b;
            cm->cmsg_level = SOL_SOCKET;
            cm->cmsg_type = SCM_RIGHTS;
            long tot = 1 + nfd;
            cm->cmsg_len = CMSG_LEN(sizeof(int) * tot);
            int* fp = (int*)CMSG_DATA(cm);
            fp[0] = op[1];
            for (long i = 0; i < nfd; i++) fp[1 + i] = fds[i];
            mh.msg_controllen = CMSG_SPACE(sizeof(int) * tot);
        }
        ssize_t sn = sendmsg(w->sock, &mh, 0);
        close(op[1]);
        if (sn < 0) { close(op[0]); }
        long ok = 0;
        result = plant_rw_recv_result(w, op[0], &ok);
        if (ok) {
            w->state = 0;
            w->served++;
            g_rw_served++;
            free(pbuf);
            return result;
        }
        /* stall/death: recover this worker, then retry once */
        if (w->state == 2) { kill(w->pid, SIGKILL); waitpid(w->pid, NULL, 0); }
        plant_rw_respawn((int)wid);
        g_pool_restarts++;
    }
    g_rw_fallback++;
    free(pbuf);
    return g_safe_regs[idx].fn((int)argc, args);
}

/* ================================================================
   v0.49.43 - TLS/HTTPS support via curl subprocess
   Uses the system curl binary for HTTPS client operations.
   No OpenSSL headers required at compile time.
   Returns JSON-like result map: {body, status, ok}
   ================================================================ */
tx_t plant_net_harvest_https_url(tx_t url, tx_t method) {
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
        "curl -s -w '\\n%%{http_code}' -X %.100s --max-time 30 '%.4000s' 2>/dev/null",
        _S(method), _S(url));
    FILE *fp = popen(cmd, "r");
    if (!fp) return strdup("{\"ok\":\"0\"}");
    char body[65536];
    size_t n = fread(body, 1, sizeof(body)-1, fp);
    int rc = pclose(fp);
    body[n] = 0;
    /* split last line as status code */
    char *last_nl = strrchr(body, '\n');
    char status[16] = "0";
    if (last_nl) { snprintf(status, sizeof(status), "%s", last_nl+1); *last_nl = 0; }
    /* escape body for JSON string value */
    tx_t esc_body = strdup(body);
    tx_t result = plant_map_create();
    result = plant_map_set(result, "body", esc_body);
    result = plant_map_set(result, "status", status);
    result = plant_map_set(result, "ok", strstr(status, "2") == status ? "1" : "0");
    free(esc_body);
    return result;
}

/* v0.49.46 - WebSocket minimal client (RFC 6455) */
static void _ws_sha1(const unsigned char*d,size_t l,unsigned char*h){
    uint32_t H[5]={0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0};
    size_t pl=((l+8)/64+1)*64;unsigned char*m=calloc(pl,1);memcpy(m,d,l);m[l]=0x80;
    uint64_t bl=(uint64_t)l*8;for(int i=0;i<8;i++)m[pl-1-i]=(bl>>(i*8))&0xFF;
    uint32_t w[80];
    for(size_t o=0;o<pl;o+=64){for(int j=0;j<16;j++)w[j]=(m[o+j*4]<<24)|(m[o+j*4+1]<<16)|(m[o+j*4+2]<<8)|m[o+j*4+3];
    for(int j=16;j<80;j++)w[j]=w[j-3]^w[j-8]^w[j-14]^w[j-16];
    uint32_t a=H[0],b=H[1],c=H[2],d=H[3],e=H[4],f,k,t;
    for(int j=0;j<80;j++){if(j<20){f=(b&c)|((~b)&d);k=0x5A827999;}else if(j<40){f=b^c^d;k=0x6ED9EBA1;}
    else if(j<60){f=(b&c)|(b&d)|(c&d);k=0x8F1BBCDC;}else{f=b^c^d;k=0xCA62C1D6;}
    t=(a<<5|a>>27)+f+e+k+w[j];e=d;d=c;c=(b<<30|b>>2)|0;b=a;a=t;}
    H[0]+=a;H[1]+=b;H[2]+=c;H[3]+=d;H[4]+=e;}
    for(int i=0;i<4;i++){h[i]=(H[0]>>(24-i*8))&0xFF;h[4+i]=(H[1]>>(24-i*8))&0xFF;
    h[8+i]=(H[2]>>(24-i*8))&0xFF;h[12+i]=(H[3]>>(24-i*8))&0xFF;h[16+i]=(H[4]>>(24-i*8))&0xFF;}
    free(m);
}
static char*_ws_b64(const unsigned char*d,size_t l){
    static const char*T="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char*o=malloc(4*((l+2)/3)+1);size_t oi=0;
    for(size_t i=0;i<l;i+=3){uint32_t n=d[i]<<16|(i+1<l?d[i+1]:0)<<8|(i+2<l?d[i+2]:0);
    o[oi++]=T[(n>>18)&63];o[oi++]=T[(n>>12)&63];o[oi++]=(i+1<l)?T[(n>>6)&63]:'=';o[oi++]=(i+2<l)?T[n&63]:'=';}
    o[oi]=0;return o;}
#define WS_MAX 16
static int g_ws_fd[WS_MAX];
static int g_ws_act[WS_MAX];

tx_t plant_ws_connect(tx_t url) {
    const char*u=_S(url);const char*hp=strstr(u,"://");
    if(!hp)return strdup("-1");hp+=3;
    char host[256]="";int port=80;char path[256]="/";
    if(sscanf(hp,"%255[^:/]:%d/%255s",host,&port,path+1)<1)
        sscanf(hp,"%255[^/]/%255s",host,path);
    struct hostent*he=gethostbyname(host);
    if(!he)return strdup("-2");
    int fd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons(port);
    memcpy(&a.sin_addr,he->h_addr_list[0],he->h_length);
    if(connect(fd,(struct sockaddr*)&a,sizeof(a))<0){close(fd);return strdup("-3");}
    unsigned char key[16];for(int i=0;i<16;i++)key[i]=rand()&0xFF;
    unsigned char sha[20];_ws_sha1(key,16,sha);
    char*b64=_ws_b64(sha,20);
    char req[2048];snprintf(req,sizeof(req),
        "GET /%s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n",path,host,b64);
    send(fd,req,strlen(req),0);
    char resp[4096];size_t got=0;
    while(got<sizeof(resp)-1){char c;if(recv(fd,&c,1,0)<=0){close(fd);return strdup("-4");}resp[got++]=c;
    if(got>=4&&memcmp(resp+got-4,"\r\n\r\n",4)==0)break;}
    resp[got]=0;
    if(!strstr(resp,"101")){close(fd);return strdup("-5");}
    for(int i=0;i<WS_MAX;i++)if(!g_ws_act[i]){g_ws_fd[i]=fd;g_ws_act[i]=1;return strdup(_from_long(i));}
    close(fd);return strdup("-6");
}
tx_t plant_ws_send(tx_t conn_id, tx_t msg) {
    long slot=_to_long(conn_id);if(slot<0||slot>=WS_MAX||!g_ws_act[slot])return strdup("0");
    size_t len=strlen(_S(msg));
    unsigned char hdr[10];size_t hl=0;hdr[hl++]=0x81;
    if(len<=125){hdr[hl++]=(unsigned char)(len|0x80);}
    else{hdr[hl++]=126|0x80;hdr[hl++]=(len>>8)&0xFF;hdr[hl++]=len&0xFF;}
    unsigned char mask[4]={1,2,3,4};memcpy(hdr+hl,mask,4);hl+=4;
    send(g_ws_fd[slot],hdr,hl,0);
    char*mc=malloc(len);for(size_t i=0;i<len;i++)mc[i]=((char*)_S(msg))[i]^mask[i%4];
    send(g_ws_fd[slot],mc,len,0);free(mc);
    return strdup("1");
}
tx_t plant_ws_recv(tx_t conn_id) {
    long slot=_to_long(conn_id);if(slot<0||slot>=WS_MAX||!g_ws_act[slot])return strdup("");
    unsigned char h[2];if(recv(g_ws_fd[slot],h,2,MSG_WAITALL)!=2)return strdup("");
    if((h[0]&0x0F)==0x8)return strdup("__CLOSED__");
    int masked=(h[1]&0x80)!=0;size_t len=h[1]&0x7F;unsigned char mask[4];
    if(len==126){unsigned char e[2];recv(g_ws_fd[slot],e,2,MSG_WAITALL);len=(e[0]<<8)|e[1];}
    if(masked)recv(g_ws_fd[slot],mask,4,MSG_WAITALL);
    char*buf=malloc(len+1);size_t got=0;
    while(got<len){int n=recv(g_ws_fd[slot],buf+got,len-got,0);if(n<=0){free(buf);return strdup("");}got+=n;}
    buf[len]=0;if(masked)for(size_t i=0;i<len;i++)buf[i]^=mask[i%4];
    tx_t result=strdup(buf);free(buf);return result;
}
tx_t plant_ws_close(tx_t conn_id) {
    long slot=_to_long(conn_id);if(slot<0||slot>=WS_MAX||!g_ws_act[slot])return strdup("0");
    unsigned char close_frame[]={0x88,0x00};
    send(g_ws_fd[slot],close_frame,2,0);close(g_ws_fd[slot]);
    g_ws_act[slot]=0;return strdup("1");
}

/* v0.49.47 - WebSocket server (minimal RFC 6455) */
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
static int g_ws_lfd[WS_MAX];
static int g_ws_lact[WS_MAX];

tx_t plant_net_listen_ws(tx_t port) {
    int port_num=(int)_to_long(port);
    int slot=-1;
    for(int i=0;i<WS_MAX;i++)if(!g_ws_lact[i]){slot=i;break;}
    if(slot<0)return strdup("-1");
    int fd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;
    a.sin_addr.s_addr=INADDR_ANY;a.sin_port=htons(port_num);
    if(bind(fd,(struct sockaddr*)&a,sizeof(a))<0||listen(fd,10)<0){close(fd);return strdup("-2");}
    g_ws_lfd[slot]=fd;g_ws_lact[slot]=1;
    return strdup(_from_long(slot));
}
tx_t plant_net_ws_accept(tx_t listener) {
    long lslot=_to_long(listener);
    if(lslot<0||lslot>=WS_MAX||!g_ws_lact[lslot])return strdup("");
    struct sockaddr_in cli;socklen_t clen=sizeof(cli);
    int cfd=accept(g_ws_lfd[lslot],(struct sockaddr*)&cli,&clen);
    if(cfd<0)return strdup("");
    /* read HTTP upgrade request */
    char req[4096];size_t got=0;
    while(got<sizeof(req)-1){char c;if(recv(cfd,&c,1,0)<=0){close(cfd);return strdup("");}req[got++]=c;
    if(got>=4&&memcmp(req+got-4,"\r\n\r\n",4)==0)break;}
    req[got]=0;
    /* extract Sec-WebSocket-Key */
    char*key_start=strstr(req,"Sec-WebSocket-Key:");
    if(!key_start){close(cfd);return strdup("");}
    key_start+=18;
    while(*key_start==' ')key_start++;
    char*key_end=strstr(key_start,"\r\n");
    if(!key_end){close(cfd);return strdup("");}
    size_t klen=key_end-key_start;
    char ws_key[128];if(klen>=sizeof(ws_key))klen=127;memcpy(ws_key,key_start,klen);ws_key[klen]=0;
    /* compute accept = b64(sha1(key + GUID)) */
    char concat[256];snprintf(concat,sizeof(concat),"%s%s",ws_key,WS_GUID);
    unsigned char sha[20];_ws_sha1((unsigned char*)concat,strlen(concat),sha);
    char*accept_b64=_ws_b64(sha,20);
    /* send 101 */
    char resp[512];snprintf(resp,sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",
        accept_b64);
    free(accept_b64);
    send(cfd,resp,strlen(resp),0);
    /* store as active connection */
    for(int i=0;i<WS_MAX;i++)if(!g_ws_act[i]){g_ws_fd[i]=cfd;g_ws_act[i]=1;return strdup(_from_long(i));}
    close(cfd);return strdup("");
}

void plant_verify(tx_t label, tx_t cond) {
    extern int verify_failures, verify_total;
    verify_total++;
    const char* cs = (const char*)cond;
    if (!cs || strcmp(cs, "0") == 0 || strcmp(cs, "") == 0) {
        verify_failures++;
        plant_log(PLANT_ERROR, "VERIFY FAILED: %s", (const char*)label);
    }
}

void plant_verify_begin(void) {
    extern int verify_failures, verify_total;
    verify_failures = 0;
    verify_total = 0;
}

void plant_verify_end(void) {
    extern int verify_failures, verify_total;
    if (verify_total == 0) return;
    if (verify_failures == 0) {
        printf("%sAll %d assertions passed.%s\n", COLOR_GREEN, verify_total, COLOR_RESET);
    } else {
        char _vrb[256]; snprintf(_vrb, 256, "%d assertions failed out of %d.", verify_failures, verify_total);
        plant_log(PLANT_ERROR, "%s", _vrb);
        plant_error("verification failed");
    }
}

void plant_suite_setup(void) {
    plant_info("[SETUP] Initializing test suite.");
}

void plant_suite_teardown(void) {
    plant_info("[TEARDOWN] Cleaning up test suite.");
}

void plant_suite_setup_hook(tx_t expr) {
    char _shb[256]; snprintf(_shb, 256, "[SETUP] %s", (const char*)expr);
    plant_info(_shb);
}

void plant_suite_teardown_hook(tx_t expr) {
    char _thb[256]; snprintf(_thb, 256, "[TEARDOWN] %s", (const char*)expr);
    plant_info(_thb);
}

/* ═══════════════════════════════════════════════════════════════
   v0.49.59a — Abstract Runtime Interface (IRuntime) Implementation
   Binds the IRuntime vtable to the concrete global functions so
   callers can interact through the abstract interface without
   coupling to a particular runtime backend.
   ═══════════════════════════════════════════════════════════════ */

static void _iruntime_execute(void* ctx, const char* code) {
    (void)ctx; (void)code;
    /* Stub — execution requires the full compiler pipeline.
       Provided so the vtable is non-NULL for test harnesses. */
}

static void _iruntime_verify(void* ctx, const char* label, int condition) {
    (void)ctx;
    plant_verify((tx_t)label, condition ? "1" : "0");
}

static void _iruntime_verify_begin(void* ctx) {
    (void)ctx;
    plant_verify_begin();
}

static void _iruntime_verify_end(void* ctx) {
    (void)ctx;
    plant_verify_end();
}

static void _iruntime_suite_setup(void* ctx) {
    (void)ctx;
    plant_suite_setup();
}

static void _iruntime_suite_teardown(void* ctx) {
    (void)ctx;
    plant_suite_teardown();
}

static void _iruntime_error(void* ctx, const char* msg) {
    (void)ctx;
    plant_error(msg);
}

static void _iruntime_warning(void* ctx, const char* msg) {
    (void)ctx;
    plant_warning(msg);
}

static void _iruntime_info(void* ctx, const char* msg) {
    (void)ctx;
    plant_info(msg);
}

static void _iruntime_fatal(void* ctx, const char* msg) {
    (void)ctx;
    plant_fatal(msg);
}

IRuntime* plant_runtime_default(void) {
    static IRuntime _default_rt = {0};
    static int initialized = 0;
    if (!initialized) {
        _default_rt.context        = NULL;
        _default_rt.execute        = _iruntime_execute;
        _default_rt.verify         = _iruntime_verify;
        _default_rt.verify_begin   = _iruntime_verify_begin;
        _default_rt.verify_end     = _iruntime_verify_end;
        _default_rt.suite_setup    = _iruntime_suite_setup;
        _default_rt.suite_teardown = _iruntime_suite_teardown;
        _default_rt.error          = _iruntime_error;
        _default_rt.warning        = _iruntime_warning;
        _default_rt.info           = _iruntime_info;
        _default_rt.fatal          = _iruntime_fatal;
        initialized = 1;
    }
    return &_default_rt;
}

void plant_runtime_free(IRuntime* rt) {
    if (!rt) return;
    /* Default runtime is static — only free heap-allocated instances */
    if (rt != plant_runtime_default()) free(rt);
}

void plant_runtime_verify(IRuntime* rt, const char* label, int condition) {
    if (rt && rt->verify) rt->verify(rt->context, label, condition);
    else plant_verify((tx_t)label, condition ? "1" : "0");
}

void plant_runtime_verify_begin(IRuntime* rt) {
    if (rt && rt->verify_begin) rt->verify_begin(rt->context);
    else plant_verify_begin();
}

void plant_runtime_verify_end(IRuntime* rt) {
    if (rt && rt->verify_end) rt->verify_end(rt->context);
    else plant_verify_end();
}

/* ═══════════════════════════════════════════════════════════════
   v0.49.60a — IRuntime Convenience Helpers & Factory Binding
   Public helpers with null-safety, plus PlantRuntime_create()
   factory that binds helpers to the vtable for clean DIP
   compliance.
   ═══════════════════════════════════════════════════════════════ */

void plant_iRuntime_execute(IRuntime* rt, const char* code) {
    if (rt && rt->execute) rt->execute(rt->context, code);
}

void plant_iRuntime_verify(IRuntime* rt, const char* label, int condition) {
    if (rt && rt->verify) rt->verify(rt->context, label, condition);
    else plant_verify((tx_t)label, condition ? "1" : "0");
}

void plant_iRuntime_verify_begin(IRuntime* rt) {
    if (rt && rt->verify_begin) rt->verify_begin(rt->context);
    else plant_verify_begin();
}

void plant_iRuntime_verify_end(IRuntime* rt) {
    if (rt && rt->verify_end) rt->verify_end(rt->context);
    else plant_verify_end();
}

void plant_iRuntime_suite_setup(IRuntime* rt) {
    if (rt && rt->suite_setup) rt->suite_setup(rt->context);
    else plant_suite_setup();
}

void plant_iRuntime_suite_teardown(IRuntime* rt) {
    if (rt && rt->suite_teardown) rt->suite_teardown(rt->context);
    else plant_suite_teardown();
}

void plant_iRuntime_error(IRuntime* rt, const char* msg) {
    if (rt && rt->error) rt->error(rt->context, msg);
    else plant_error(msg);
}

void plant_iRuntime_warning(IRuntime* rt, const char* msg) {
    if (rt && rt->warning) rt->warning(rt->context, msg);
    else plant_warning(msg);
}

void plant_iRuntime_info(IRuntime* rt, const char* msg) {
    if (rt && rt->info) rt->info(rt->context, msg);
    else plant_info(msg);
}

void plant_iRuntime_fatal(IRuntime* rt, const char* msg) {
    if (rt && rt->fatal) rt->fatal(rt->context, msg);
    else plant_fatal(msg);
}

/* ── Factory: bind helpers to vtable ── */

static IRuntime* _default_runtime = NULL;

IRuntime* PlantRuntime_create(void* context) {
    IRuntime* rt = (IRuntime*)malloc(sizeof(IRuntime));
    if (!rt) return NULL;
    rt->context        = context;
    rt->execute        = _iruntime_execute;
    rt->verify         = (void (*)(void*, const char*, int))_iruntime_verify;
    rt->verify_begin   = (void (*)(void*))_iruntime_verify_begin;
    rt->verify_end     = (void (*)(void*))_iruntime_verify_end;
    rt->suite_setup    = (void (*)(void*))_iruntime_suite_setup;
    rt->suite_teardown = (void (*)(void*))_iruntime_suite_teardown;
    rt->error          = (void (*)(void*, const char*))_iruntime_error;
    rt->warning        = (void (*)(void*, const char*))_iruntime_warning;
    rt->info           = (void (*)(void*, const char*))_iruntime_info;
    rt->fatal          = (void (*)(void*, const char*))_iruntime_fatal;
    return rt;
}

void PlantRuntime_destroy(IRuntime* rt) {
    if (!rt) return;
    if (rt == _default_runtime) _default_runtime = NULL;
    free(rt);
}

/* ═══════════════════════════════════════════════════════════════
   v0.49.60b — Global DI Accessors
   Provide get/set functions for the global IRuntime and IReport
   instances used by generated code. These are the entry points
   for the Dependency Inversion pattern in the codegen layer.
   ═══════════════════════════════════════════════════════════════ */

static IRuntime* _global_runtime = NULL;
static IReport*  _global_report  = NULL;

IRuntime* get_runtime(void) {
    if (!_global_runtime) _global_runtime = PlantRuntime_create(NULL);
    return _global_runtime;
}

void set_runtime(IRuntime* rt) {
    _global_runtime = rt;
}

IReport* get_report(void) {
    if (!_global_report) {
        PlantReport* r = plant_report_create("default");
        if (r) _global_report = PlantReport_create(r);
    }
    return _global_report;
}

void set_report(IReport* rep) {
    _global_report = rep;
}
