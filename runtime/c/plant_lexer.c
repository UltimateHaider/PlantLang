/*
 * plant_lexer.c — v0.49.59b: Abstract Lexer Interface (ILexer)
 *
 * Wraps the PlantLang-generated lexer functions (tokenize, peek,
 * consume, tok_type, tok_lex, is_eof) in the ILexer vtable so
 * callers can interact through the abstract interface without
 * coupling to a particular lexer backend.
 *
 * The internal context struct carries the token buffer and cursor
 * position — state that was previously threaded as separate params
 * through every parser call.
 */

#include "plant_lexer.h"
#include "plant_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── PlantLang-generated lexer functions (defined in the
   compiler's generated C code, linked at build time). ── */
extern tx_t tokenize(tx_t src);
extern tx_t peek(PlantArray* tokens, long pos);
extern tx_t consume(PlantArray* tokens, long pos);
extern tx_t is_eof(PlantArray* tokens, long pos);
extern tx_t tok_type(PlantArray* tok);
extern tx_t tok_lex(PlantArray* tok);

/* ── Internal lexer context ── */
typedef struct {
    PlantArray* tokens;   /* token list from tokenize() */
    long        pos;      /* current cursor position */
} _LexerContext;

/* ── ILexer vtable implementations ── */

static void* _ilexer_tokenize(void* ctx, const char* source) {
    _LexerContext* lc = (_LexerContext*)ctx;
    if (!lc) return NULL;
    lc->tokens = (PlantArray*)tokenize((tx_t)source);
    lc->pos = 0;
    return lc->tokens;
}

static void* _ilexer_peek(void* ctx) {
    _LexerContext* lc = (_LexerContext*)ctx;
    if (!lc || !lc->tokens) return NULL;
    return peek(lc->tokens, lc->pos);
}

static void* _ilexer_consume(void* ctx) {
    _LexerContext* lc = (_LexerContext*)ctx;
    if (!lc || !lc->tokens) return NULL;
    tx_t result = consume(lc->tokens, lc->pos);
    /* consume() returns [token, pos+1] — advance our cursor */
    PlantArray* pair = (PlantArray*)result;
    if (pair && pair->count >= 2) {
        lc->pos = (long)(intptr_t)pair->items[1];
    } else {
        lc->pos++;
    }
    return result;
}

static const char* _ilexer_tok_type(void* ctx, void* token) {
    (void)ctx;
    return (const char*)tok_type((PlantArray*)token);
}

static const char* _ilexer_tok_lex(void* ctx, void* token) {
    (void)ctx;
    return (const char*)tok_lex((PlantArray*)token);
}

static int _ilexer_is_eof(void* ctx) {
    _LexerContext* lc = (_LexerContext*)ctx;
    if (!lc || !lc->tokens) return 1;
    return is_eof(lc->tokens, lc->pos) ? 1 : 0;
}

/* ── Constructor / Destructor ── */

ILexer* PlantLexer_create(void* context) {
    ILexer* lex = (ILexer*)malloc(sizeof(ILexer));
    if (!lex) return NULL;

    _LexerContext* lc = (_LexerContext*)malloc(sizeof(_LexerContext));
    if (!lc) { free(lex); return NULL; }

    lc->tokens = NULL;
    lc->pos = 0;

    lex->context   = context ? context : lc;
    lex->tokenize  = _ilexer_tokenize;
    lex->peek      = _ilexer_peek;
    lex->consume   = _ilexer_consume;
    lex->tok_type  = _ilexer_tok_type;
    lex->tok_lex   = _ilexer_tok_lex;
    lex->is_eof    = _ilexer_is_eof;

    /* If caller provided no context, use our internal ctx */
    if (!context) lex->context = lc;

    return lex;
}

void PlantLexer_destroy(ILexer* lex) {
    if (!lex) return;
    /* Free internal context if it was allocated by us */
    if (lex->context) {
        _LexerContext* lc = (_LexerContext*)lex->context;
        /* Don't free lc->tokens — it's owned by the PlantLang runtime */
        free(lc);
    }
    free(lex);
}

/* ── Convenience helpers ── */

void* plant_iLexer_tokenize(ILexer* lex, const char* source) {
    if (lex && lex->tokenize) return lex->tokenize(lex->context, source);
    return NULL;
}

void* plant_iLexer_peek(ILexer* lex) {
    if (lex && lex->peek) return lex->peek(lex->context);
    return NULL;
}

void* plant_iLexer_consume(ILexer* lex) {
    if (lex && lex->consume) return lex->consume(lex->context);
    return NULL;
}

const char* plant_iLexer_tok_type(ILexer* lex, void* token) {
    if (lex && lex->tok_type) return lex->tok_type(lex->context, token);
    return NULL;
}

const char* plant_iLexer_tok_lex(ILexer* lex, void* token) {
    if (lex && lex->tok_lex) return lex->tok_lex(lex->context, token);
    return NULL;
}

int plant_iLexer_is_eof(ILexer* lex) {
    if (lex && lex->is_eof) return lex->is_eof(lex->context);
    return 1;
}
