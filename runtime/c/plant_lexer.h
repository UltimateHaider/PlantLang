#ifndef PLANT_LEXER_H
#define PLANT_LEXER_H

#include <plant_types.h>

/* ═══════════════════════════════════════════════════════════════
   v0.49.60c — Abstract Lexer Interface (ILexer)
   Lexical analysis contract providing a uniform tokenization and
   stream-inspection interface bound to a shared context. Concrete
   lexers (PlantLang, future language frontends) implement these
   pointers; callers interact through the abstract interface only.

   The context pointer carries lexer-specific state (token buffer,
   cursor position) so callers never see the internals of any
   particular lexer implementation.

   v0.49.60c adds peek_at/consume_at/is_eof_at variants that accept
   explicit tokens+pos arguments, enabling random-access parsing
   through the abstract interface.
   ═══════════════════════════════════════════════════════════════ */

typedef struct ILexer ILexer;
struct ILexer {
    void* context;
    void* (*tokenize)(void* ctx, const char* source);
    void* (*peek)(void* ctx);
    void* (*consume)(void* ctx);
    const char* (*tok_type)(void* ctx, void* token);
    const char* (*tok_lex)(void* ctx, void* token);
    int (*is_eof)(void* ctx);
    /* v0.49.60c: random-access variants (explicit tokens+pos) */
    void* (*peek_at)(void* ctx, void* tokens, long pos);
    void* (*consume_at)(void* ctx, void* tokens, long pos);
    int   (*is_eof_at)(void* ctx, void* tokens, long pos);
};

/* Default lexer: wraps PlantLang's tokenize(), peek(), consume(),
   tok_type(), tok_lex(), is_eof() generated C functions. */
ILexer* PlantLexer_create(void* context);
void     PlantLexer_destroy(ILexer* lex);

/* Convenience helpers with null-safety (context-managed) */
void*       plant_iLexer_tokenize(ILexer* lex, const char* source);
void*       plant_iLexer_peek(ILexer* lex);
void*       plant_iLexer_consume(ILexer* lex);
const char* plant_iLexer_tok_type(ILexer* lex, void* token);
const char* plant_iLexer_tok_lex(ILexer* lex, void* token);
int         plant_iLexer_is_eof(ILexer* lex);

/* v0.49.60c: random-access convenience helpers (explicit tokens+pos) */
void*       plant_iLexer_peek_at(ILexer* lex, void* tokens, long pos);
void*       plant_iLexer_consume_at(ILexer* lex, void* tokens, long pos);
int         plant_iLexer_is_eof_at(ILexer* lex, void* tokens, long pos);

#endif
