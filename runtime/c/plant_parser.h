#ifndef PLANT_PARSER_H
#define PLANT_PARSER_H

#include <plant_types.h>

/* Forward declaration — IParser depends on ILexer */
typedef struct ILexer ILexer;

/* ═══════════════════════════════════════════════════════════════
   v0.49.59b — Abstract Parser Interface (IParser)
   Syntactic analysis contract relying on an underlying lexer
   instance. Defines uniform entry points for program parsing,
   statement dispatch, expression evaluation, and block handling.
   Concrete parsers (PlantLang recursive descent, future language
   frontends) implement these pointers; callers interact through
   the abstract interface only.

   IParser receives a reference to an ILexer at construction time,
   enforcing the compositional dependency between front-end phases.
   ═══════════════════════════════════════════════════════════════ */

typedef struct IParser IParser;
struct IParser {
    void* context;
    void* (*parse)(void* ctx, const char* source);
    void* (*parse_statement)(void* ctx);
    void* (*parse_expression)(void* ctx);
    void* (*parse_block)(void* ctx);
};

/* Default parser: wraps PlantLang's parse_program() and
   parse_statement() generated C functions. Holds a reference
   to the ILexer used for tokenization. */
IParser* PlantParser_create(void* context, ILexer* lexer);
void      PlantParser_destroy(IParser* parser);

/* Convenience helpers with null-safety */
void* plant_iParser_parse(IParser* parser, const char* source);
void* plant_iParser_parse_statement(IParser* parser);
void* plant_iParser_parse_expression(IParser* parser);
void* plant_iParser_parse_block(IParser* parser);

#endif
