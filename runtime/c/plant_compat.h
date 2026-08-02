#ifndef PLANT_COMPAT_H
#define PLANT_COMPAT_H

#define _GNU_SOURCE
#include <plant_runtime.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef void* tx_t;
#define _S(x) ((const char*)(x))
#define _P(x) ((PlantArray*)(x))
#define _L(x) ((long)(x))
#define _POS(x) _to_long(x)
static void plant_print(tx_t s) { printf("%s\n", _S(s)); }
static void* plant_env_alloc(size_t size) { return malloc(size); }
static tx_t _cat(tx_t a, tx_t b) { const char* sa=_S(a), *sb=_S(b); size_t al=strlen(sa), bl=strlen(sb); char *r=malloc(al+bl+1); if(r){memcpy(r,sa,al);memcpy(r+al,sb,bl+1);} return r?r:a; }
static long _to_long(tx_t s) { const char* _s=_S(s); return _s ? atol(_s) : 0; }
static tx_t _from_long(long n) { char buf[64]; snprintf(buf,64,"%ld",n); return strdup(buf); }
static tx_t _at(tx_t s, long i) { PlantArray*_p=_P(s); if(_p&&_p->magic==PLANT_ARRAY_MAGIC)return plant_list_get(_p,i); const char*_s=_S(s); if(!_s||i<0||i>=(long)strlen(_s))return""; char _b[2]; _b[0]=_s[i]; _b[1]=0; return strdup(_b); }
static int _cli_argc; static char **_cli_argv;
static void plant_init_cli(int c, char **v) { _cli_argc=c; _cli_argv=v; }
static tx_t get_cli_arg(int i) { if(i+1<_cli_argc) return _cli_argv[i+1]; return ""; }
static tx_t fs_EXISTS(tx_t p) { FILE*f=fopen(_S(p),"rb"); if(!f)return"0"; fclose(f);return"1"; }
static tx_t fs_READ(tx_t p) { FILE*f=fopen(_S(p),"rb"); if(!f)return""; fseek(f,0,SEEK_END);long sz=ftell(f);rewind(f);char*b=malloc(sz+1);if(!b){fclose(f);return"";}fread(b,1,sz,f);b[sz]=0;fclose(f);return b; }
static tx_t fs_WRITE(tx_t p, tx_t c) { FILE*f=fopen(_S(p),"w"); if(f){fputs(_S(c),f);fclose(f);return"1";}return"0"; }
static tx_t strings_LENGTH(tx_t s) { if(!s)return"0";char b[64];snprintf(b,64,"%zu",strlen(_S(s)));return strdup(b); }
static tx_t strings_REPLACE(tx_t s, tx_t a, tx_t b) {
  const char* _s=_S(s), *_a=_S(a), *_b=_S(b);
  if (!_s || !_a || !_b || !*_a) return _s ? strdup(_s) : NULL;
  size_t al = strlen(_a), bl = strlen(_b);
  int count = 0; const char *p = _s;
  while ((p = strstr(p, _a)) != NULL) { count++; p += al; }
  if (count == 0) return strdup(_s);
  size_t sl = strlen(_s);
  size_t new_len = sl + count * (bl - al);
  char *buf = malloc(new_len + 1);
  if (!buf) return strdup(_s);
  char *dst = buf; const char *src = _s; const char *end;
  while ((end = strstr(src, _a)) != NULL) {
    size_t n = end - src;
    memcpy(dst, src, n); dst += n;
    memcpy(dst, _b, bl); dst += bl;
    src = end + al;
  }
  strcpy(dst, src);
  return buf;
}
static tx_t char_at(tx_t s, long i) {
  const char*_s=_S(s);
  static char buf[2];
  if (!_s || i < 0 || i >= (long)strlen(_s)) return "";
  buf[0] = _s[i]; buf[1] = 0;
  return strdup(buf);
}
static tx_t substring(tx_t s, long start, long end) {
  const char*_s=_S(s);
  if (!_s || start < 0) return "";
  long len = (long)strlen(_s);
  if (start >= len || end <= start) return "";
  if (end > len) end = len;
  long n = end - start;
  char *buf = malloc((size_t)(n + 1));
  if (!buf) return "";
  memcpy(buf, _s + start, (size_t)n);
  buf[n] = 0;
  return buf;
}
static long find_any(tx_t s, tx_t delims) {
  const char*_s=_S(s), *_d=_S(delims);
  if (!_s || !_d) return -1;
  const char *p = _s;
  while (*p) { if (strchr(_d, *p)) return (long)(p - _s); p++; }
  return -1;
}
static tx_t handle_brackets(tx_t expr) {
  const char*_e=_S(expr);
  if (!_e || !*_e) return _e ? strdup(_e) : NULL;
  size_t len = strlen(_e);
  char *buf = malloc(len * 4 + 1);
  if (!buf) return strdup(_e);
  char *out = buf;
  const char *p = _e;
  while (*p) {
    if (*p == '[' && p > _e) {
      const char *id = p - 1;
      while (id >= _e && (*id == ' ' || *id == '\t')) id--;
      while (id >= _e && (*id == '_' || (*id >= 'a' && *id <= 'z') || (*id >= 'A' && *id <= 'Z') || (*id >= '0' && *id <= '9'))) id--;
      id++;
      if (id < p && ((*id >= 'a' && *id <= 'z') || (*id >= 'A' && *id <= 'Z') || *id == '_')) {
        size_t idl = (size_t)(p - id);
        out -= idl;
        memcpy(out, "plant_list_get(", 15); out += 15;
        const char *idend = id;
        while (idend < p && (*idend == '_' || (*idend >= 'a' && *idend <= 'z') || (*idend >= 'A' && *idend <= 'Z') || (*idend >= '0' && *idend <= '9'))) idend++;
        memcpy(out, id, (size_t)(idend - id)); out += idend - id;
        memcpy(out, ", ", 2); out += 2;
        p++;
        int depth = 1;
        while (*p && depth > 0) {
          if (*p == '[') depth++;
          if (*p == ']') depth--;
          if (depth > 0) { *out++ = *p++; }
        }
        if (*p == ']') p++;
        *out++ = ')';
        _e = p;
        continue;
      }
    }
    *out++ = *p++;
  }
  *out = 0;
  tx_t result = strdup(buf);
  free(buf);
  return result;
}
static tx_t str_eq(tx_t a, tx_t b) { const char*x=_S(a),*y=_S(b); return (x&&y&&strcmp(x,y)==0)?"1":"0"; }
static tx_t handle_strcmp(tx_t expr) {  const char*_e=_S(expr);
  if (!_e || !*_e) return _e ? strdup(_e) : NULL;
  size_t len = strlen(_e);
  char *buf = malloc(len * 8 + 100);
  if (!buf) return strdup(_e);
  char *out = buf; *out = 0;
  size_t i = 0;
  size_t last_out = 0, last_in = 0;
  while (i < len) {
    if (_e[i] == '"') {
      size_t j = i + 1;
      while (j < len && _e[j] != '"') { if (_e[j] == '\\') j++; j++; }
      if (j < len) j++;
      memcpy(out, _e + i, j - i); out += j - i;
      i = j;
      continue;
    }
    int op_len = 0; const char *sfx = NULL;
    if (i+1 < len && _e[i] == '=' && _e[i+1] == '=') { op_len = 2; sfx = ") == 0"; }
    else if (i+1 < len && _e[i] == '!' && _e[i+1] == '=') { op_len = 2; sfx = ") != 0"; }
    else if (i+1 < len && _e[i] == '>' && _e[i+1] == '=') { op_len = 2; sfx = ") >= 0"; }
    else if (i+1 < len && _e[i] == '<' && _e[i+1] == '=') { op_len = 2; sfx = ") <= 0"; }
    else if (_e[i] == '>') { op_len = 1; sfx = ") > 0"; }
    else if (_e[i] == '<') { op_len = 1; sfx = ") < 0"; }
    if (op_len > 0) {
      int d = 0, bd = 0, has_str = 0, lfound = 0;
      size_t left_start = 0;
      size_t ls = i; while (ls > 0) { ls--; if (_e[ls] == ')' && d == 0 && bd == 0) { d++; continue; } if (_e[ls] == '(') { d--; if (d < 0) { ls++; break; } continue; } if (_e[ls] == ']' && d == 0) { bd++; continue; } if (_e[ls] == '[' && d == 0) { bd--; if (bd < 0) { ls++; break; } continue; } if (d == 0 && bd == 0) { if ((ls+1 < len && _e[ls] == '&' && _e[ls+1] == '&') || (ls+1 < len && _e[ls] == '|' && _e[ls+1] == '|')) { ls += 2; break; } if (_e[ls] == ',') { ls++; break; } } }
      while (ls <= i && (_e[ls] == ' ' || _e[ls] == '\t')) { ls++; if (ls > i) break; }
      size_t le = i;
      while (le > ls && (_e[le-1] == ' ' || _e[le-1] == '\t')) le--;
      int has_lit = 0;
      for (size_t k = ls; k < le; k++) if (_e[k] == '"') { has_lit = 1; break; }
      size_t pos = i + op_len;
      while (pos < len && (_e[pos] == ' ' || _e[pos] == '\t')) pos++;
      int d2 = 0, bd2 = 0;
      size_t rs = pos;
      while (rs < len) {
        if (_e[rs] == '"') { rs++; while (rs < len && _e[rs] != '"') { if (_e[rs] == '\\') rs++; rs++; } if (rs < len) rs++; continue; }
        if (_e[rs] == '(') d2++; else if (_e[rs] == ')') { if (d2 <= 0) break; d2--; }
        else if (_e[rs] == '[') bd2++; else if (_e[rs] == ']') { if (bd2 <= 0) break; bd2--; }
        else if (d2 == 0 && bd2 == 0) { if ((rs+1 < len && _e[rs] == '&' && _e[rs+1] == '&') || (rs+1 < len && _e[rs] == '|' && _e[rs+1] == '|') || _e[rs] == ',') break; }
        rs++;
      }
      size_t rre = rs;
      while (rre > pos && (_e[rre-1] == ' ' || _e[rre-1] == '\t')) rre--;
      for (size_t k = pos; k < rre; k++) if (_e[k] == '"') { has_lit = 1; break; }
      if (has_lit) {
        out = buf + (last_out + (ls - last_in));
        out += sprintf(out, "strcmp(");
        memcpy(out, _e + ls, le - ls); out += le - ls;
        *out++ = ',';
        memcpy(out, _e + pos, rre - pos); out += rre - pos;
        out += sprintf(out, "%s", sfx);
        if (rs > rre) { memcpy(out, _e + rre, rs - rre); out += rs - rre; }
        last_out = (size_t)(out - buf);
        last_in = rs;
        i = rs;
        continue;
      }
    }
    *out++ = _e[i++];
  }
  *out = 0;
  tx_t result = strdup(buf);
  free(buf);
  return result;
}

static int64_t plant_array_length(PlantArray* a) { return a ? a->count : 0; }
static PlantArray* strings_SPLIT(tx_t s, tx_t d) {
  const char*_s=_S(s), *_d=_S(d);
  if (!_s || !_d) return plant_list_create(0);
  PlantArray *r = plant_list_create(0);
  const char *st = _s, *en; size_t dl = strlen(_d);
  while ((en = strstr(st, _d)) != NULL) {
    size_t nl = en - st; char *p = malloc(nl + 1);
    if (p) { memcpy(p, st, nl); p[nl] = 0; r = plant_list_push(r, p); }
    st = en + dl;
  }
  r = plant_list_push(r, (char*)st);
  return r;
}
static tx_t _map_get(PlantArray* m, tx_t k) {
  const char*_k=_S(k);
  if (!m) return "";
  for (int64_t i = 0; i + 1 < m->count; i += 2)
    if (strcmp(m->items[i], _k) == 0) return m->items[i + 1];
  return "";
}

/* ═══════════════════════════════════════════════════════════════
   v0.47.0 — Core Standard Library (std/*)
   Signatures here; implementations in plant_runtime.c
   ═══════════════════════════════════════════════════════════════ */

/* ── std/json ── */
typedef struct PlantJson {
    int   kind;   /* 0=null 1=bool 2=num 3=str 4=arr 5=obj */
    void* val;    /* char* for kinds 1/2/3; PlantArray* for 4/5 */
} PlantJson;

tx_t json_parse(tx_t str);          /* PlantJson* | NULL (invalid JSON → safe nil, no crash) */
tx_t json_stringify(tx_t val);      /* TX JSON text (NULL → "null"; raw pair-list MAP also ok) */
tx_t json_get(tx_t obj, tx_t key);  /* object member → PlantJson* | NULL (missing) */
tx_t json_at(tx_t arr, long idx);   /* array element → PlantJson* | NULL (OOB) */
long json_len(tx_t val);            /* array/object element count; 0 otherwise */
long json_kind(tx_t val);           /* 0..5; NULL → 0 */
tx_t json_val(tx_t val);            /* scalar value text ("true"/number/string); "" otherwise */

/* ── std/string ── */
tx_t string_repeat(tx_t str, long count);   /* str repeated count times (count<=0 → "") */
tx_t string_reverse(tx_t str);              /* reversed character order */
tx_t string_pad(tx_t str, long length, tx_t pad_char); /* right-pad to length */

/* ── std/fs ── */
tx_t file_copy(tx_t src, tx_t dest);   /* "1" ok | "0" error */
tx_t file_move(tx_t src, tx_t dest);   /* "1" ok | "0" error (rename, copy+unlink fallback) */
tx_t file_stat(tx_t path);             /* PlantArray* pair-list MAP (size/mtime/mode) | NULL error */

/* ── std/math (decimal TX args/returns) ── */
tx_t math_sin(tx_t x);
tx_t math_cos(tx_t x);
tx_t math_sqrt(tx_t x);   /* negative → "0" */
tx_t math_pow(tx_t x, tx_t y);
tx_t math_floor(tx_t x);
tx_t math_ceil(tx_t x);
tx_t math_round(tx_t x);
tx_t math_min(tx_t a, tx_t b);
tx_t math_max(tx_t a, tx_t b);
tx_t math_random(void);   /* uniform [0,1) */

/* ── std/time ── */
tx_t time_now(void);                       /* epoch seconds as decimal TX */
tx_t time_format(tx_t t, tx_t format);     /* strftime ("" on failure) */
tx_t time_parse(tx_t str, tx_t format);    /* strptime → epoch seconds TX ("" on failure) */
tx_t time_sleep(tx_t seconds);             /* fractional seconds supported; → "1" */

/* ═══════════════════════════════════════════════════════════════
   v0.47.2 — Native Data Structures (Set / Queue / Stack)
   Signatures here; implementations in plant_runtime.c
   ═══════════════════════════════════════════════════════════════ */

/* ── Set (unique unordered collection; open-addressing hash table) ──
   Uniqueness is by raw value identity (pointer / NUM bits); value 0/NULL
   is reserved (nil) and not storable. */

typedef struct PlantSet {
    uintptr_t* slots;    /* 0 = empty, (uintptr_t)-1 = tombstone */
    size_t     cap;      /* always a power of two */
    size_t     count;    /* live entries (excl. tombstones) */
    size_t     tombs;    /* tombstone count */
} PlantSet;

tx_t  set_create(void);                    /* → PlantSet* */
tx_t  set_add(tx_t s, tx_t val);           /* "1" added | "0" already present */
tx_t  set_has(tx_t s, tx_t val);           /* "1" | "0" */
tx_t  set_remove(tx_t s, tx_t val);        /* "1" removed | "0" absent */
long  set_size(tx_t s);                    /* unique element count */
tx_t  set_to_list(tx_t s);                 /* → PlantArray* LIST of values */

/* ── Queue (FIFO ring buffer) ── */

typedef struct PlantQueue {
    void**   buf;
    size_t   cap;
    size_t   head;      /* read index */
    size_t   count;     /* live items */
} PlantQueue;

tx_t queue_create(void);                   /* → PlantQueue* */
tx_t queue_push(tx_t q, tx_t val);         /* → q (for chaining) */
tx_t queue_pop(tx_t q);                    /* front value | "" if empty (safe) */
tx_t queue_peek(tx_t q);                   /* front value | "" if empty (safe) */
long queue_size(tx_t q);                   /* item count */

/* ── Stack (LIFO dynamic array) ── */

typedef struct PlantStack {
    void**   buf;
    size_t   cap;
    size_t   count;     /* top index = count - 1 */
} PlantStack;

tx_t stack_create(void);                   /* → PlantStack* */
tx_t stack_push(tx_t s, tx_t val);         /* → s (for chaining) */
tx_t stack_pop(tx_t s);                    /* top value | "" if empty (safe) */
tx_t stack_peek(tx_t s);                   /* top value | "" if empty (safe) */
long stack_size(tx_t s);                   /* item count */

/* ═══════════════════════════════════════════════════════════════
   v0.47.3 — Advanced FFI: diagnostics + memory lifecycle
   Signatures here; implementations in plant_runtime.c
   ═══════════════════════════════════════════════════════════════ */

long ffi_last_error(void);              /* errno from the last FFI call (0 = ok) */
tx_t ffi_last_error_msg(void);          /* dlerror() | strerror(errno) */
void ffi_free(void* p);                 /* free() with NULL → EINVAL guard */

/* ═══════════════════════════════════════════════════════════════
   v0.48.3 — Advanced Async Engine
   Signatures here; implementations in plant_runtime.c
   Generated async actions compile to state-struct + step functions
   driven by the cooperative AsyncDispatcher in plant_runtime.c.
   ═══════════════════════════════════════════════════════════════ */

typedef int (*plant_stepfn)(tx_t st);   /* returns 1 done, 0 suspended */

typedef struct plant_task plant_task;   /* opaque: runtime-internal */

/* state allocation + task registration (generated by the compiler) */
tx_t  plant_async_alloc_state(long size, tx_t name);
tx_t  plant_async_register(tx_t st, plant_stepfn step, tx_t parent,
                           tx_t ctx, long prio, long dl, long to,
                           tx_t tok, tx_t name);
void  plant_async_finish(tx_t st, tx_t res);     /* complete w/ result */
void  plant_async_suspend(tx_t st, tx_t child);  /* wait for child (self = sleep) */
void  plant_async_sleep(tx_t st, long ms);       /* timer suspension */
tx_t  plant_async_await_result(tx_t st);         /* child result / marker */

/* contexts + cancel tokens */
tx_t  plant_async_ctx_create(long adaptive, long cap);
void  plant_async_ctx_cancel(tx_t ctx);          /* cascade cancel */
tx_t  plant_async_token_create(void);
void  plant_async_cancel(tx_t x);                /* ctx or token */

/* work stealing / lazy copy-on-write cloning */
tx_t  plant_async_steal(tx_t t);                 /* exclusive arena clone */

/* observability */
void  plant_trace(long level, tx_t scope, tx_t msg);
tx_t  plant_async_stats(void);                   /* summary text for tests */
void  plant_async_config(tx_t key, tx_t val);
void  plant_async_init(void);
void  plant_async_drain(void);                   /* run dispatcher to idle */

/* ═══════════════════════════════════════════════════════════════
   v0.48.4 — FFI Optional Extensions
   Signatures here; implementations in plant_runtime.c
   ═══════════════════════════════════════════════════════════════ */

/* FFI error codes — plant_ffi_errno after an FFI-extension call */
#define FFI_OK         0
#define FFI_ERR_TYPE   1   /* unsupported field type in a struct conversion */
#define FFI_ERR_DEPTH  2   /* struct nesting deeper than 3 levels */
#define FFI_ERR_CALLBACK 3 /* callback tag not registered */
#define FFI_ERR_MEMORY 4   /* allocation failure during marshalling */
#define FFI_ERR_SIGNATURE 5 /* callback wrapper ABI mismatch */

extern long plant_ffi_errno;
void plant_ffi_debug_set(long on);           /* runtime override for PLANT_FFI_DEBUG */
void plant_ffi_debug_print(tx_t msg);        /* stderr debug line (env-gated) */

/* callback ABI: handlers are PlantLang ACTIONs compiled to
   tx_t name(long ctx, tx_t val) and wrapped by the compiler */
typedef tx_t (*plant_cb_t)(long ctx, tx_t val);
tx_t plant_cb_ensure(tx_t tag, plant_cb_t fn);    /* register; → tag */
tx_t plant_cb_call(tx_t tag, long ctx, tx_t val); /* invoke; FFI_ERR_CALLBACK if missing */
void plant_cb_unregister(tx_t tag);
tx_t plant_cb_get(tx_t tag);                      /* registered fn | 0 */

/* profiling hooks (monotonic ns) */
void plant_profile_start(tx_t name);
void plant_profile_end(tx_t name);
tx_t plant_profile_dump(void);

/* struct helpers used by generated marshalling code */
void plant_struct_free(void* p);              /* free() a marshalled copy */

#endif
