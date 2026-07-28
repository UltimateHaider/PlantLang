#include <plant_runtime.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_SOURCE 1048576

static char source[MAX_SOURCE];
static long src_len;
static long pos;
static char ch;

static void next_char(void) {
  pos++;
  ch = (pos < src_len) ? source[pos] : '\0';
}

static void skip_ws(void) {
  while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') next_char();
}

static int read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  src_len = fread(source, 1, sizeof(source) - 1, f);
  fclose(f);
  source[src_len] = '\0';
  pos = -1;
  next_char();
  return 1;
}

static int match_kw(const char *kw) {
  skip_ws();
  long saved = pos;
  for (const char *p = kw; *p; p++) {
    if (ch != *p) { pos = saved; return 0; }
    next_char();
  }
  if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
      (ch >= '0' && ch <= '9') || ch == '_') {
    pos = saved;
    return 0;
  }
  return 1;
}

static void emit_str(FILE *out) {
  if (ch == '"') next_char();
  fputc('"', out);
  while (ch && ch != '"') {
    if (ch == '\\') { fputc('\\', out); next_char(); }
    fputc(ch, out);
    next_char();
  }
  if (ch == '"') next_char();
  fputc('"', out);
}

static void emit_expr(FILE *out) {
  skip_ws();
  if (ch == '"') { emit_str(out); return; }
  while (ch && ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' &&
         ch != '.' && ch != ',' && ch != ')' && ch != '}' && ch != ']') {
    if (ch == '"') { emit_str(out); return; }
    if (ch == '\\') break;
    fputc(ch, out);
    next_char();
  }
}

static void skip_stmt(void) {
  long depth = 0;
  while (ch) {
    if (ch == '.' && depth == 0) { next_char(); return; }
    if (ch == '(') depth++;
    if (ch == ')') depth--;
    if (ch == '"') { next_char(); while (ch && ch != '"') { if (ch == '\\') next_char(); next_char(); } if (ch == '"') next_char(); }
    else next_char();
  }
}

static void add_wrapper(FILE *out) {
  fprintf(out, "static void plant_print(const char *s) { printf(\"%%s\\n\", s); }\n\n");
}

static void gen_show(FILE *out) {
  skip_ws();
  fprintf(out, "  plant_print(");
  emit_expr(out);
  fprintf(out, ");\n");
  if (ch == '.') next_char();
}

static void gen_create(FILE *out) {
  skip_ws();
  fprintf(out, "  ");
  while (ch && ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' && ch != '(') {
    fputc(ch, out); next_char();
  }
  if (ch == '(') {
    while (ch && ch != ')') next_char();
    if (ch == ')') next_char();
  }
  skip_ws();
  if (ch == 'T') { next_char(); if (ch == 'O') next_char(); }
  else if (ch == '=') next_char();
  skip_ws();
  fprintf(out, " = ");
  emit_expr(out);
  fprintf(out, ";\n");
  if (ch == '.') next_char();
}

static void gen_set(FILE *out) {
  fprintf(out, "  ");
  skip_ws();
  while (ch && ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
    fputc(ch, out); next_char();
  }
  skip_ws();
  if (ch == 'T') { next_char(); if (ch == 'O') next_char(); }
  skip_ws();
  fputc('=', out);
  emit_expr(out);
  fprintf(out, ";\n");
  if (ch == '.') next_char();
}

static int compile(const char *inpath, const char *outpath) {
  if (!read_file(inpath)) {
    fprintf(stderr, "Error: Could not read '%s'\n", inpath);
    return 1;
  }

  FILE *out = fopen(outpath, "w");
  if (!out) {
    fprintf(stderr, "Error: Could not write '%s'\n", outpath);
    return 1;
  }

  fprintf(out, "#include <plant_runtime.h>\n#include <string.h>\n#include <stdio.h>\n");
  add_wrapper(out);
  fprintf(out, "int main() {\n");

  while (ch) {
    skip_ws();
    if (!ch || ch == '#') {
      if (ch == '#') while (ch && ch != '\n') next_char();
      continue;
    }

    if (match_kw("SHOW"))       { gen_show(out); continue; }
    if (match_kw("CREATE"))     { gen_create(out); continue; }
    if (match_kw("SET"))        { gen_set(out); continue; }
    if (match_kw("GIVE"))       { skip_ws(); fprintf(out, "  return "); emit_expr(out); fprintf(out, ";\n"); if (ch == '.') next_char(); continue; }
    if (match_kw("BREAK"))      { fprintf(out, "  break;\n"); skip_stmt(); continue; }
    if (match_kw("CONTINUE"))   { fprintf(out, "  continue;\n"); skip_stmt(); continue; }
    if (match_kw("IF"))         { skip_ws(); fprintf(out, "  if ("); while (ch && ch != ',') { if (ch == '"') emit_str(out); else { fputc(ch, out); next_char(); } } if (ch == ',') next_char(); fprintf(out, ") {\n"); continue; }
    if (match_kw("SEASON"))     { skip_ws(); fprintf(out, "  while ("); while (ch && ch != ',') { if (ch == '"') emit_str(out); else { fputc(ch, out); next_char(); } } if (ch == ',') next_char(); fprintf(out, ") {\n"); continue; }
    if (match_kw("CALM") || match_kw("/IF") || match_kw("/SEASON")) { fprintf(out, "  }\n"); if (ch == '.') next_char(); continue; }

    if (match_kw("PLANT") || match_kw("IMPORT") || match_kw("ACTION") || match_kw("ENUM") || match_kw("SHAPE") || match_kw("CHOICE")) { skip_stmt(); continue; }

    if (ch) { next_char(); }
  }

  fprintf(out, "  return 0;\n}\n");
  fclose(out);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <source.plant> [output.c]\n", argv[0]);
    return 1;
  }
  const char *inpath = argv[1];
  const char *outpath = (argc > 2) ? argv[2] : NULL;
  char default_out[1024];
  if (!outpath) {
    snprintf(default_out, sizeof(default_out), "%s", inpath);
    char *dot = strrchr(default_out, '.');
    if (dot && strcmp(dot, ".plant") == 0) strcpy(dot, ".c");
    else strcat(default_out, ".c");
    outpath = default_out;
  }
  return compile(inpath, outpath);
}
