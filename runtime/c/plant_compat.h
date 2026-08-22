#ifndef PLANT_COMPAT_H
#define PLANT_COMPAT_H

/* v0.48.29: tx_t lives in plant_types.h (pulled in via
   plant_runtime.h below and explicitly for standalone inclusion);
   plant_compat.h holds only FFI bindings and compatibility
   wrappers. */
#include <plant_types.h>
#define _GNU_SOURCE
#include <plant_runtime.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define _S(x) ((const char*)(x))
#define _P(x) ((PlantArray*)(x))
#define _L(x) ((long)(x))
#define _POS(x) _to_long(x)
static void plant_print(tx_t s) { printf("%s\n", _S(s)); }
static void* plant_env_alloc(size_t size) { void* p = malloc(size); if (p) plant_env_register(p); return p; }
void plant_env_register(void* p); /* v0.48.36: closure env registry for TYPEOF/ANALYZE */
/* v0.48.37: the _cat family allocates from the fixed-size (64B) string
   slab pool when the result fits, else from the heap; g_bal_bytes is
   the BALANCED allocation counter reported by plant_mem_report. */
static tx_t _cat(tx_t a, tx_t b) { const char* sa=_S(a), *sb=_S(b); if(!sa) sa=""; if(!sb) sb=""; size_t al=strlen(sa), bl=strlen(sb); size_t tot=al+bl+1; char *r=plant_str_slab_alloc(tot); if(!r)r=malloc(tot); if(r){memcpy(r,sa,al);memcpy(r+al,sb,bl+1);g_bal_bytes+=(long)tot;} return r?r:a; }
/* v0.48.22-patch4 — flattened concatenation: one malloc per group of
   3/4 segments instead of one per pair. _cat3/_cat4 mirror _cat's
   NULL-to-"" coercion and fall back to the first argument on OOM. */
static tx_t _cat3(tx_t a, tx_t b, tx_t c) { const char* sa=_S(a), *sb=_S(b), *sc=_S(c); if(!sa) sa=""; if(!sb) sb=""; if(!sc) sc=""; size_t al=strlen(sa), bl=strlen(sb), cl=strlen(sc); size_t tot=al+bl+cl+1; char *r=plant_str_slab_alloc(tot); if(!r)r=malloc(tot); if(r){memcpy(r,sa,al);memcpy(r+al,sb,bl);memcpy(r+al+bl,sc,cl+1);g_bal_bytes+=(long)tot;} return r?r:a; }
static tx_t _cat4(tx_t a, tx_t b, tx_t c, tx_t d) { const char* sa=_S(a), *sb=_S(b), *sc=_S(c), *sd=_S(d); if(!sa) sa=""; if(!sb) sb=""; if(!sc) sc=""; if(!sd) sd=""; size_t al=strlen(sa), bl=strlen(sb), cl=strlen(sc), dl=strlen(sd); size_t tot=al+bl+cl+dl+1; char *r=plant_str_slab_alloc(tot); if(!r)r=malloc(tot); if(r){memcpy(r,sa,al);memcpy(r+al,sb,bl);memcpy(r+al+bl,sc,cl);memcpy(r+al+bl+cl,sd,dl+1);g_bal_bytes+=(long)tot;} return r?r:a; }
static long _to_long(tx_t s) { const char* _s=_S(s); return _s ? atol(_s) : 0; }
/* v0.48.37c — SAFE worker adapter numeric args: the wire codec can
   only carry small ints (raw in the pointer, < 4096) or strings, so
   the codegen stringifies numeric SAFE args at the call site. This
   helper unpacks either form back to a long. */
static long plant_rw_arg_long(tx_t v) { if (!v) return 0; if ((uintptr_t)v < 4096) return (long)(intptr_t)v; return _to_long(v); }
/* v0.48.22-patch4 — single-digit (0-9) fast path: static table instead
   of snprintf. _from_long funnels through _from_digit so every numeric
   string conversion benefits; the compiler additionally emits
   _from_digit(...) directly for known single-digit literal segments.
   Static return is safe: the runtime never writes into tx_t strings. */
static tx_t _from_digit(long n) { static const char* _dg[10] = {"0","1","2","3","4","5","6","7","8","9"}; if (n >= 0 && n <= 9) return (tx_t)_dg[n]; char buf[64]; snprintf(buf,64,"%ld",n); return strdup(buf); }
static tx_t _from_long(long n) { return _from_digit(n); }
/* v0.49.6 — decimal literals: [1.5, 2.7] emit _from_double(...).
   %g prints integral doubles without the trailing dot (1.0 → "1"),
   so the number string round-trips through _to_long/atol too. */
static tx_t _from_double(double d) { char buf[64]; snprintf(buf, 64, "%.10g", d); return strdup(buf); }
/* v0.48.5 — FFI numeric results: C functions returning `long` hand
   raw integer bits back in tx_t. On this runtime intptr_t == void*,
   so the conversion is identical to _from_long; this single choke
   point lets a future ABI that distinguishes FFI nums diverge. */
static tx_t _from_ffi_num(long long n) { return _from_long((long)n); }
/* v0.48.6 — ENUM member display. An ENUM-typed variable holds the raw
   member int in its tx_t slot; names is the comma-separated member list
   the codegen emitted for the enum typedef ("RED,GREEN,BLUE"). Returns a
   copy of the member name for that value, else the numeric string.
   v0.48.11 — idempotent: when the value is already a member-name string
   (it crossed an enum-typed function boundary, whose GIVE wraps with
   _from_enum), it is returned unchanged instead of re-reading the
   string pointer as an enum index. Plausible-pointer check (>= 64KB)
   keeps small raw ints on the index path without dereferencing them. */
static tx_t _from_enum(tx_t v, tx_t names) {
  long idx = _L(v);
  const char* s = _S(names);
  if (!s || idx < 0) return _from_ffi_num(idx);
  if (idx >= 65536) {
    const char* vp = _S(v);
    long vl = 0;
    if (vp) { while (vp[vl] && vl < 64) vl++; }
    const char* p2 = s;
    while (vl > 0 && *p2) {
      const char* c2 = strchr(p2, ',');
      size_t n2 = c2 ? (size_t)(c2 - p2) : strlen(p2);
      if ((long)n2 == vl && strncmp(vp, p2, n2) == 0) return v;
      if (!c2) break;
      p2 = c2 + 1;
    }
  }
  const char* p = s; long cur = 0;
  while (cur < idx && *p) {
    while (*p && *p != ',') p++;
    if (*p == ',') { p++; cur++; }
    else break;
  }
  const char* start = p;
  while (*p && *p != ',') p++;
  if (p == start) return _from_ffi_num(idx);
  size_t n = (size_t)(p - start);
  char* buf = malloc(n + 1);
  if (!buf) return _from_ffi_num(idx);
  memcpy(buf, start, n); buf[n] = 0;
  return buf;
}
/* v0.48.12 — ENUM FFI params. External C functions expect the raw
   member int. PlantLang enum values are either the raw int (member
   constants, struct/map field reads) or the member-name string (they
   crossed an enum-typed function boundary). Small values pass through
   unchanged; member-name strings map back to their index; unknown
   strings pass through untouched. */
static tx_t _to_enum(tx_t v, tx_t names) {
  long idx = _L(v);
  const char* s = _S(names);
  if (!s || idx < 65536) return v;
  {
    const char* vp = _S(v);
    long vl = 0;
    if (vp) { while (vp[vl] && vl < 64) vl++; }
    const char* p = s; long cur = 0;
    while (vl > 0 && *p) {
      const char* c = strchr(p, ',');
      size_t n = c ? (size_t)(c - p) : strlen(p);
      if ((long)n == vl && strncmp(vp, p, n) == 0) return (tx_t)(intptr_t)cur;
      if (!c) break;
      p = c + 1; cur++;
    }
  }
  return v;
}
static tx_t _at(tx_t s, long i) { PlantArray*_p=_P(s); if(_p&&_p->magic==PLANT_ARRAY_MAGIC)return plant_list_get(_p,i); const char*_s=_S(s); if(!_s||i<0||i>=(long)strlen(_s))return""; char _b[2]; _b[0]=_s[i]; _b[1]=0; return strdup(_b); }
static int _cli_argc; static char **_cli_argv;
extern char* g_cli_argv0;                  /* v0.48.37c: exec-self path for worker spawns (defined in plant_runtime.c; shared across TUs — plant_init_cli runs in the program TU, plant_rw_spawn here) */
extern int g_cli_worker_mode;              /* v0.48.37c: true when running as --plant-worker (shared across TUs) */
static void plant_init_cli(int c, char **v) {
  _cli_argc=c; _cli_argv=v;
  if (c > 0 && v && v[0]) g_cli_argv0 = v[0];
  g_cli_worker_mode = (c > 1 && v && v[1] && strcmp(v[1], "--plant-worker") == 0);
}
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
static int is_idc(char c);
/* v0.49.6 — multi-pass rightmost-first rewrite so chained indices
   compose: a [ 1 ] [ 0 ] → plant_list_get(plant_list_get(a, 1), 0)
   and plant_map_get(m, "a")[0] works too. Each pass finds the
   RIGHTMOST indexable "[" (the spans before it are still verbatim,
   so the base is copied straight out of the input), rewrites it into
   a fresh output, and repeats — inner indices get rewritten on the
   following passes. The base walker accepts an identifier (dotted
   paths included), a balanced ) group (call results: f(x), (x + 1),
   f([1, 2])), or a balanced ] group (nested indices: a [ 1 ]), so
   any sub-expression is indexable. */
static const char *bracket_base(const char *cur, const char *p, const char **bs_out) {
  const char *id = p - 1;
  while (id >= cur && (*id == ' ' || *id == '\t')) id--;
  if (id < cur) return NULL;
  const char *bs = NULL;
  if (is_idc(*id)) {
    while (id >= cur && (is_idc(*id) || *id == '.')) id--;
    bs = id + 1;
    if (!((*bs >= 'a' && *bs <= 'z') || (*bs >= 'A' && *bs <= 'Z') || *bs == '_')) return NULL;
  } else if (*id == ']' || *id == ')') {
    char close = *id, open = (*id == ')') ? '(' : '[';
    const char *q = id;
    int d = 0, ok = 0;
    while (q >= cur) {
      if (*q == close) d++;
      if (*q == open) { d--; if (d == 0) { ok = 1; break; } }
      q--;
    }
    if (!ok) return NULL;
    const char *n = q - 1;
    while (n >= cur && (*n == ' ' || *n == '\t')) n--;
    if (n >= cur && is_idc(*n)) {
      while (n >= cur && (is_idc(*n) || *n == '.')) n--;
      bs = n + 1;
    } else {
      bs = q;
    }
  } else {
    return NULL;
  }
  *bs_out = bs;
  return bs;
}
static tx_t handle_brackets(tx_t expr) {
  const char*_e=_S(expr);
  if (!_e || !*_e) return _e ? strdup(_e) : NULL;
  size_t len = strlen(_e);
  char *buf = malloc(len * 64 + 1);
  if (!buf) return strdup(_e);
  char *cur = strdup(_e);
  for (int pass = 0; pass < 32; pass++) {
    /* rightmost indexable "[": scan from the end, skipping "["s
       whose base walker fails (standalone list literals) */
    const char *pend = cur + strlen(cur);
    const char *p = pend - 1;
    const char *bs = NULL;
    while (p >= cur && !bs) {
      if (*p == '[' && p > cur) bracket_base(cur, p, &bs);
      p--;
    }
    p++;
    if (!bs) { tx_t r = strdup(cur); free(cur); return r; }
    /* bs..p is verbatim in the input — copy [cur..bs), emit the
       rewrite, copy the tail */
    char *out = buf;
    size_t pre = (size_t)(bs - cur);
    memcpy(out, cur, pre); out += pre;
    memcpy(out, "plant_list_get(", 15); out += 15;
    memcpy(out, bs, (size_t)(p - bs)); out += (size_t)(p - bs);
    memcpy(out, ", ", 2); out += 2;
    const char *ip = p + 1;
    int depth = 1;
    while (*ip && depth > 0) {
      if (*ip == '[') depth++;
      if (*ip == ']') depth--;
      if (depth > 0) { *out++ = *ip++; }
    }
    if (*ip == ']') ip++;
    *out++ = ')';
    while (*ip) *out++ = *ip++;
    *out = 0;
    free(cur);
    cur = strdup(buf);
  }
  free(cur);
  tx_t result = strdup(buf);
  free(buf);
  return result;
}
static int is_idc(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

/* v0.48.38c — boolean-literal sanitization. Replaces the standalone
   tokens TRUE/FALSE with 1/0 ONLY outside double-quoted string
   literals, so strings like "hello | TRUE" stay pristine (the old
   naive text replace corrupted literal text tokens). Quote handling
   is backslash-escape aware, and identifier-boundary checks keep
   words like TRUESTATE intact. */
static tx_t plant_sanitize_bools(tx_t expr) {
  const char*_e=_S(expr);
  if (!_e || !*_e) return _e ? strdup(_e) : NULL;
  size_t len = strlen(_e);
  char *buf = malloc(len + 1);
  if (!buf) return strdup(_e);
  size_t o = 0, i = 0;
  int in = 0;
  for (i = 0; i < len; i++) {
    char c = _e[i];
    if (c == '"' && (i == 0 || _e[i-1] != '\\')) { in = !in; buf[o++] = c; continue; }
    if (!in && c == 'T' && i + 4 <= len && strncmp(_e + i, "TRUE", 4) == 0 &&
        (i == 0 || !is_idc(_e[i-1])) && (i + 4 >= len || !is_idc(_e[i+4]))) {
      buf[o++] = '1'; i += 3; continue;
    }
    if (!in && c == 'F' && i + 5 <= len && strncmp(_e + i, "FALSE", 5) == 0 &&
        (i == 0 || !is_idc(_e[i-1])) && (i + 5 >= len || !is_idc(_e[i+5]))) {
      buf[o++] = '0'; i += 4; continue;
    }
    buf[o++] = c;
  }
  buf[o] = 0;
  tx_t r = strdup(buf);
  free(buf);
  return r;
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
/* v0.49.7 — MATCH pattern equality. Numeric-aware compare: raw
   small ints (< 4096 inline range) and pure-numeric strings compare
   numerically (so a subject holding the string "5" matches the
   literal pattern 5 and vice versa); everything else compares as
   text. NULL == NULL; NULL never equals a value. */
static tx_t _match_eq(tx_t a, tx_t b) {
  const char *sa = _S(a), *sb = _S(b);
  if (!sa || !sb) return (sa == sb) ? (tx_t)1 : (tx_t)0;
  long la = plant_rw_arg_long(a), lb = plant_rw_arg_long(b);
  int na = 0, nb = 0;
  if ((uintptr_t)a < 4096) { na = 1; }
  else { char *ea = NULL; errno = 0; long va = strtol(sa, &ea, 10); if (ea != sa && *ea == '\0' && errno == 0) { na = 1; } }
  if ((uintptr_t)b < 4096) { nb = 1; }
  else { char *eb = NULL; errno = 0; long vb = strtol(sb, &eb, 10); if (eb != sb && *eb == '\0' && errno == 0) { nb = 1; } }
  if (na && nb) return (la == lb) ? (tx_t)1 : (tx_t)0;
  return strcmp(sa, sb) == 0 ? (tx_t)1 : (tx_t)0;
}
/* v0.49.7 — MATCH binding extraction: value is the [tag, payload]
   pair-list form (as produced by _from_enum and the Option/Result
   helpers); pattern is the tag text. Returns the payload when the
   tag matches, else NULL. */
static tx_t _match_extract(tx_t value, tx_t pattern) {
  if (!value) return NULL;
  if (plant_array_length(value) == 2 && _match_eq(plant_list_get(value, 0), pattern))
    return plant_list_get(value, 1);
  return NULL;
}
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
/* v0.48.26 — complete string library */
tx_t string_upper(tx_t str);                /* fully uppercase */
tx_t string_lower(tx_t str);                /* fully lowercase */
tx_t string_trim(tx_t str);                 /* strip leading/trailing whitespace */
tx_t string_includes(tx_t str, tx_t sub);   /* "1" if str contains sub */
tx_t string_starts_with(tx_t str, tx_t pre); /* "1" if str starts with pre */
tx_t string_ends_with(tx_t str, tx_t suf);  /* "1" if str ends with suf */
tx_t string_pad_left(tx_t str, long length, tx_t pad_char); /* left-pad to length */

/* ── v0.48.26: strings: FFI module bindings ───────────────────────
   Thin wrappers over the std/string runtime (plant_runtime.c); the
   compiler rewrites `module:FUNC` calls to `module_FUNC`. */
static tx_t strings_UPPER(tx_t s)        { return string_upper(s); }
static tx_t strings_LOWER(tx_t s)        { return string_lower(s); }
static tx_t strings_TRIM(tx_t s)         { return string_trim(s); }
static tx_t strings_INCLUDES(tx_t s, tx_t t)    { return string_includes(s, t); }
static tx_t strings_STARTS_WITH(tx_t s, tx_t t) { return string_starts_with(s, t); }
static tx_t strings_ENDS_WITH(tx_t s, tx_t t)   { return string_ends_with(s, t); }
static tx_t strings_REVERSE(tx_t s)      { return string_reverse(s); }
static tx_t strings_REPEAT(tx_t s, long n)      { return string_repeat(s, n); }
static tx_t strings_PAD(tx_t s, long n, tx_t c) { return string_pad(s, n, c); }
static tx_t strings_PAD_LEFT(tx_t s, long n, tx_t c) { return string_pad_left(s, n, c); }

/* ── v0.49.15: list built-in runtime helpers (extern linkage) ────
   Bare expression built-ins (REVERSE / RANGE / SORT / INCLUDES /
   INDEX_OF / UNIQUE / AVERAGE / MEDIAN) map to these in
   _handle_func_paren; implementations live in plant_runtime.c. */
extern tx_t plant_list_reverse(tx_t data);
extern tx_t plant_range_list(tx_t start, tx_t end);
extern tx_t plant_list_sort(tx_t data);
extern tx_t plant_list_includes(tx_t data, tx_t item);
extern tx_t plant_list_index_of(tx_t data, tx_t item);
extern tx_t plant_list_unique(tx_t data);
extern tx_t plant_list_average(tx_t data);
extern tx_t plant_list_median(tx_t data);

/* ── v0.49.16: list built-in batch 2 (extern linkage) ──────────── */
extern tx_t plant_list_flatten(tx_t data);
extern tx_t plant_list_chunk(tx_t data, tx_t size);
extern tx_t plant_list_zip(tx_t left, tx_t right);
extern tx_t plant_list_filter_gt(tx_t data, tx_t threshold);
extern tx_t plant_list_filter_lt(tx_t data, tx_t threshold);

/* ── std/fs ── */
tx_t file_copy(tx_t src, tx_t dest);   /* "1" ok | "0" error */
tx_t file_move(tx_t src, tx_t dest);   /* "1" ok | "0" error (rename, copy+unlink fallback) */
tx_t file_stat(tx_t path);             /* PlantArray* pair-list MAP (size/mtime/mode) | NULL error */
tx_t fs_append(tx_t path, tx_t content); /* "1" ok | "0" error ("ab": creates if missing) */

/* ── std/io (v0.48.28) ── */
tx_t io_showln(tx_t s);   /* prints text + trailing newline; NULL/"" → bare newline */
tx_t io_flush(void);      /* fflush(stdout); → "1" */

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
tx_t math_log(tx_t x);    /* x <= 0 → "ERR: math_log(x): …" */
tx_t math_sign(tx_t x);   /* "-1" | "0" | "1" */
tx_t math_clamp(tx_t x, tx_t lo, tx_t hi);
tx_t math_pi(void);
tx_t math_e(void);

/* ── std/math FFI bindings (v0.48.27) ── */
static tx_t math_LOG(tx_t x)           { return math_log(x); }
static tx_t math_SIGN(tx_t x)          { return math_sign(x); }
static tx_t math_CLAMP(tx_t x, tx_t lo, tx_t hi) { return math_clamp(x, lo, hi); }
static tx_t math_PI(void)              { return math_pi(); }
static tx_t math_E(void)               { return math_e(); }


/* ── v0.49.17 Extended math library ──────────────────────────── */
tx_t math_tan(tx_t x);
tx_t math_atan(tx_t x);
tx_t math_cot(tx_t x);
tx_t math_asin(tx_t x);
tx_t math_acos(tx_t x);
tx_t math_atan2(tx_t x, tx_t y);
tx_t math_sinh(tx_t x);
tx_t math_cosh(tx_t x);
tx_t math_tanh(tx_t x);
tx_t math_exp(tx_t x);
tx_t math_expm1(tx_t x);
tx_t math_log10(tx_t x);
tx_t math_log2(tx_t x);
tx_t math_log1p(tx_t x);
tx_t math_hypot(tx_t x, tx_t y);

/* ── v0.49.18 Advanced math library ──────────────────────────── */
tx_t math_sec(tx_t x);
tx_t math_csc(tx_t x);
tx_t math_asinh(tx_t x);
tx_t math_acosh(tx_t x);
tx_t math_atanh(tx_t x);
tx_t math_erf(tx_t x);
tx_t math_erfc(tx_t x);
tx_t math_gamma(tx_t x);
tx_t math_lgamma(tx_t x);
tx_t math_exp2(tx_t x);
tx_t math_log_base(tx_t x, tx_t b);

/* ── v0.49.21 statistical & array aggregations ───────────────── */
tx_t plant_list_variance(tx_t data);
tx_t plant_list_stddev(tx_t data);
tx_t plant_list_product(tx_t data);
tx_t plant_list_min(tx_t data);
tx_t plant_list_max(tx_t data);
tx_t plant_list_range(tx_t data);
tx_t plant_list_mode(tx_t data);

/* ── std/math FFI bindings (v0.49.19 — full module namespace) ──
   Every math-family endpoint is reachable as REAP … FROM math:FUNC.
   Legacy-eight endpoints route to their tagged-int-safe plant_*
   helpers; the v0.49.17/v0.49.18 tiers already are the safe ones. */
static tx_t math_ABS(tx_t x)           { return plant_abs(x); }
static tx_t math_ROUND(tx_t x)         { return plant_round(x); }
static tx_t math_POW(tx_t x, tx_t y)   { return plant_pow(x, y); }
static tx_t math_CEIL(tx_t x)          { return plant_ceil(x); }
static tx_t math_FLOOR(tx_t x)         { return plant_floor(x); }
static tx_t math_RANDOM(void)          { return plant_random(); }
static tx_t math_SIN(tx_t x)           { return plant_sin(x); }
static tx_t math_COS(tx_t x)           { return plant_cos(x); }
static tx_t math_SQRT(tx_t x)          { return plant_sqrt(x); }
static tx_t math_TAN(tx_t x)           { return math_tan(x); }
static tx_t math_ATAN(tx_t x)          { return math_atan(x); }
static tx_t math_COT(tx_t x)           { return math_cot(x); }
static tx_t math_ASIN(tx_t x)          { return math_asin(x); }
static tx_t math_ACOS(tx_t x)          { return math_acos(x); }
static tx_t math_ATAN2(tx_t x, tx_t y) { return math_atan2(x, y); }
static tx_t math_SINH(tx_t x)          { return math_sinh(x); }
static tx_t math_COSH(tx_t x)          { return math_cosh(x); }
static tx_t math_TANH(tx_t x)          { return math_tanh(x); }
static tx_t math_EXP(tx_t x)           { return math_exp(x); }
static tx_t math_EXPM1(tx_t x)         { return math_expm1(x); }
static tx_t math_EXP2(tx_t x)          { return math_exp2(x); }
static tx_t math_LOG10(tx_t x)         { return math_log10(x); }
static tx_t math_LOG2(tx_t x)          { return math_log2(x); }
static tx_t math_LOG1P(tx_t x)         { return math_log1p(x); }
static tx_t math_HYPOT(tx_t x, tx_t y) { return math_hypot(x, y); }
static tx_t math_MIN(tx_t a, tx_t b)   { return math_min(a, b); }
static tx_t math_MAX(tx_t a, tx_t b)   { return math_max(a, b); }
static tx_t math_SEC(tx_t x)           { return math_sec(x); }
static tx_t math_CSC(tx_t x)           { return math_csc(x); }
static tx_t math_ASINH(tx_t x)         { return math_asinh(x); }
static tx_t math_ACOSH(tx_t x)         { return math_acosh(x); }
static tx_t math_ATANH(tx_t x)         { return math_atanh(x); }
static tx_t math_ERF(tx_t x)           { return math_erf(x); }
static tx_t math_ERFC(tx_t x)          { return math_erfc(x); }
static tx_t math_GAMMA(tx_t x)         { return math_gamma(x); }
static tx_t math_LGAMMA(tx_t x)        { return math_lgamma(x); }
static tx_t math_LOG_BASE(tx_t x, tx_t b) { return math_log_base(x, b); }

/* ── std/io FFI bindings (v0.48.28) ── */
static tx_t io_SHOWLN(tx_t s)          { return io_showln(s); }
static tx_t io_FLUSH(void)             { return io_flush(); }

/* ── std/fs FFI bindings (v0.48.28) ── */
static tx_t fs_APPEND(tx_t p, tx_t c)  { return fs_append(p, c); }

/* ── std/time ── */
tx_t time_now(void);                       /* epoch seconds as decimal TX */
tx_t time_format(tx_t t, tx_t format);     /* strftime ("" on failure) */
tx_t time_parse(tx_t str, tx_t format);    /* strptime → epoch seconds TX ("" on failure) */
tx_t time_sleep(tx_t seconds);             /* fractional seconds supported; → "1" */

/* ── v0.48.36 — NOW / ANALYZE / TYPEOF introspection ── */
tx_t plant_now(tx_t format);       /* NOW FORMAT:x — DATE/TIME/STAMP/YEAR; "" → epoch; unknown → "bad-format:<x>" */
tx_t plant_analyze(tx_t v);        /* introspection MAP {type, size, keys} (null-safe) */
tx_t plant_typeof(tx_t v);         /* type string: "int"/"string"/"map"/"list"/"closure"/"null" */
tx_t plant_map_to_string(tx_t v);  /* recursive "{k=v, ...}" / "[e1, ...]" serializer */
tx_t plant_map_create(void);       /* v0.49.5: pair-list MAP (kind 1) factory for {k: v} literals */
tx_t plant_map_set(tx_t map, tx_t key, tx_t value); /* upsert; returns map for chaining */

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
tx_t  plant_async_ctx_create(long adaptive, long cap, tx_t name);
void  plant_async_ctx_cancel(tx_t ctx);          /* cascade cancel */
tx_t  plant_async_token_create(void);
void  plant_async_cancel(tx_t x);                /* ctx or token */

/* v0.48.14 — Async IN Context wrappers.
   START ... IN ctx / ASYNC IN ctx spawn into the context: the entry
   ABI is tx_t fn(tx_t __parent, tx_t __ctx, ...), so the spawn
   helpers are macros (a variadic forwarder is not portable); the
   macro keeps the non-contextual call path byte-identical. */
#define plant_async_start_in(ctx, fn, ...) ((tx_t)(fn)(0, (ctx), ##__VA_ARGS__))
#define plant_async_in(ctx, fn, ...)       plant_async_start_in(ctx, fn, ##__VA_ARGS__)

tx_t  plant_async_await_in(tx_t st, tx_t ctx, tx_t handle);
void  plant_async_cancel_in(tx_t ctx, tx_t x);   /* scoped cancel */
void  plant_async_trace_in(tx_t ctx, long level, tx_t msg);
long  plant_async_ctx_tasks(tx_t ctxv);          /* live task count */

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

/* ═══════════════════════════════════════════════════════════════
   v0.48.15 — Mission Mode FAST: bump heap + zero-trust + audit.
   plant_fast_enter is emitted by the codegen at the entry of every
   WITH MISSION FAST action; plant_boundary_block is emitted at the
   entry of every WITH MISSION SAFE action (callee-side handshake).
   ═══════════════════════════════════════════════════════════════ */
void  plant_fast_enter(const char* name); /* bind bump heap, reset on scope enter */
void  plant_fast_exit(void);              /* leave FAST mode (mode stack pop) */
tx_t  plant_fast_alloc(tx_t n);           /* 8-byte aligned bump; escalate on overflow */
tx_t  plant_fast_reset(void);             /* reset bump pointer (scope exit) */
tx_t  plant_fast_used(void);              /* current used bytes (text) */
tx_t  plant_fast_peak(void);              /* high-water mark (text) */
tx_t  plant_fast_escalated(void);         /* "1" once BALANCED fallback fired */
tx_t  plant_fast_status(void);            /* "used=.. cap=.. limit=.. escalated=.." */
long  plant_boundary_block(const char* callee, const char* callee_mode); /* 1 = forbidden pair blocked */
tx_t  plant_cap_check(tx_t cap);          /* zero-trust capability grant check */
tx_t  plant_audit_dump(void);             /* lock-free ring events, newest-last */
void  plant_audit_log(const char* kind, const char* msg); /* ring writer */
tx_t  plant_audit_chain_verify(void);     /* "OK" or "TAMPERED <idx>" */
tx_t  plant_audit_chain_head(void);       /* chain head hash (hex) */

/* ═══════════════════════════════════════════════════════════════
   v0.48.16 — Mission Mode SAFE: WarmProcessPool, SafeChannel IPC,
   BoundaryViolationError enforcement, syscall filter, hash-chained
   audit. Workers are in-process isolated-process emulations.
   ═══════════════════════════════════════════════════════════════ */
void  plant_safe_enter(const char* name);           /* pool acquire + zero-perm ctx */
void  plant_safe_exit(void);                        /* release worker + channel */
void  plant_safe_channel_init(const char* name);    /* action-private SafeChannel */
tx_t  plant_safe_channel_open(void);                /* standalone channel handle */
tx_t  plant_safe_send(tx_t chan, tx_t payload);     /* clone <=1MB, transfer >1MB */
tx_t  plant_safe_send_big(tx_t chan, tx_t n);       /* transferable buffer of n bytes */
tx_t  plant_safe_recv(tx_t chan);                   /* take current payload */
tx_t  plant_safe_stats(tx_t chan);                  /* "copies=.. transfers=.." */
tx_t  plant_safe_status(void);                      /* pool telemetry (text) */
tx_t  plant_safe_grant(tx_t cap);                   /* MissionContext grant (1/0) */
tx_t  plant_syscall_check(tx_t name);               /* execve/fork/ptrace filtered */
long  plant_pool_tick(void);                        /* monitor: restart stalled workers */
tx_t  plant_safe_stall(tx_t name);                  /* fault injection: mark worker stalled */
tx_t  plant_safe_starve(tx_t ms);                   /* fault injection: simulate queue wait */
tx_t  plant_audit_tamper(void);                     /* fault injection: flip newest audit byte */

/* ═══════════════════════════════════════════════════════════════
   v0.48.17 — Mission Mode SMART: SmartExecutionRouter + dynamic vec
   pool. Scalar Inline below the scalar limit, Parallel Vector Mode
   (chunked dispatch) at/above it, with queue monitoring, pool growth
   and BALANCED fallback at the hard cap. SMART holds broad operational
   grants (FILE_READ / FILE_WRITE / NET_CONNECT) and may invoke any
   mission mode.
   ═══════════════════════════════════════════════════════════════ */
void  plant_smart_enter(const char* name, long size); /* router bind + route */
void  plant_smart_exit(const char* name);             /* leave SMART ctx */
tx_t  plant_smart_route(const char* name, long size); /* "scalar" / "parallel" */
tx_t  plant_smart_status(void);                       /* vec pool telemetry */

/* ═══════════════════════════════════════════════════════════════
   v0.48.18 — Mission Mode PERSISTENT: GlobalARCHeap with tri-color
   cycle detection (auto every 1000 allocs + manual GC.cycle()),
   finalization callbacks, lease-based persistence, NET_LISTEN default
   capability, and the SAFE-data persistence gate. Objects are opaque
   handles (alloc_seq ids). DistributedHeap / hash ring: deferred.
   ═══════════════════════════════════════════════════════════════ */
void  plant_persist_enter(const char* name);          /* bind ARC heap ctx */
void  plant_persist_exit(void);                       /* leave PERSISTENT ctx */
tx_t  plant_arc_alloc(tx_t size);                     /* new object, refs=1 */
tx_t  plant_arc_retain(tx_t obj);                     /* refs++ */
tx_t  plant_arc_release(tx_t obj);                    /* refs-- (2 if leased-kept) */
tx_t  plant_arc_link(tx_t parent, tx_t child);        /* edge + internal retain */
tx_t  plant_arc_unlink(tx_t parent, tx_t child);      /* drop edge + refs-- */
tx_t  plant_arc_lease(tx_t obj, tx_t ms);             /* keep alive past refs=0 */
tx_t  plant_arc_set_finalizer(tx_t obj, tx_t name);   /* register callback */
tx_t  plant_arc_persist(tx_t obj);                    /* validation gate (1/0) */

/* v0.48.38b — storm() exception factory (also declared via
   plant_runtime.h; mirrored here for the FFI surface). file is the
   compile-site source path (tx_t), line/column are longs; fields are
   packed conditionally (non-NULL file, positive line/column). */
tx_t  plant_storm(tx_t type, tx_t msg, tx_t file, long line, long column);

/* v0.48.38c — JOIN(list, delim) built-in (also declared via
   plant_runtime.h; mirrored here for the FFI surface). */
tx_t  plant_join(tx_t list, tx_t delim);

/* v0.48.38d — FIRST / LAST / SUM list operations (also declared via
   plant_runtime.h; mirrored here for the FFI surface). */
tx_t  plant_first(tx_t list);
tx_t  plant_last(tx_t list);
tx_t  plant_sum(tx_t list);

/* v0.48.38e — UPPER / LOWER string case operations (also declared
   via plant_runtime.h; mirrored here for the FFI surface). */
tx_t  plant_upper(tx_t text);
tx_t  plant_lower(tx_t text);

/* v0.48.38e (extension) — TRIM / REVERSE string utilities (also
   declared via plant_runtime.h; mirrored here for the FFI surface). */
tx_t  plant_trim(tx_t text);
tx_t  plant_reverse(tx_t text);

/* v0.48.38f — math built-ins (also declared via plant_runtime.h;
   mirrored here for the FFI surface). */
tx_t  plant_abs(tx_t x);
tx_t  plant_round(tx_t x);
tx_t  plant_pow(tx_t x, tx_t y);
tx_t  plant_ceil(tx_t x);
tx_t  plant_floor(tx_t x);
tx_t  plant_random(void);
tx_t  plant_sin(tx_t x);
tx_t  plant_cos(tx_t x);
tx_t  plant_sqrt(tx_t x);

/* v0.48.38g — conditional list built-ins (also declared via
   plant_runtime.h; mirrored here for the FFI surface). */
tx_t  plant_has(tx_t list, tx_t value);
tx_t  plant_any(tx_t list, tx_t cond);
tx_t  plant_all(tx_t list, tx_t cond);

/* v0.48.38h — ternary built-in (also declared via plant_runtime.h;
   mirrored here for the FFI surface). */
tx_t  plant_pick(tx_t cond, tx_t true_val, tx_t false_val);

/* v0.48.38j — string analysis built-ins (also declared via
   plant_runtime.h; mirrored here for the FFI surface). */
tx_t  plant_find(tx_t text, tx_t sub);
tx_t  plant_count_of(tx_t text, tx_t sub);

/* v0.48.38i — universal sequence slicing (also declared via
   plant_runtime.h; mirrored here for the FFI surface). */
tx_t  plant_slice(tx_t data, tx_t start, tx_t end);

/* v0.48.38k — VEIN resource management (also declared via
   plant_runtime.h; mirrored here for the FFI surface). */
tx_t  plant_tap(tx_t path, tx_t mode);
tx_t  plant_absorb(tx_t vein);
tx_t  plant_infuse(tx_t vein, tx_t data);
tx_t  plant_seal(tx_t vein);
long  plant_arc_gc(void);                             /* GC.cycle(): reclaim */
tx_t  plant_persist_status(void);                     /* heap telemetry (text) */
tx_t  plant_arc_finalize_count(void);                 /* finalize counter */
void  plant_msleep(long ms);                          /* sleep helper */

#endif
