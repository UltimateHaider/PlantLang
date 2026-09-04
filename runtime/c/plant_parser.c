/*
 * plant_parser.c — v0.49.59b: Abstract Parser Interface (IParser)
 *
 * Wraps the PlantLang-generated parser functions (parse_program,
 * parse_statement) in the IParser vtable so callers can interact
 * through the abstract interface without coupling to a particular
 * parser backend.
 *
 * IParser holds a reference to an ILexer instance, enforcing the
 * compositional dependency between front-end phases. The internal
 * context carries parser-specific state (lookup tables, scope flags)
 * that were previously threaded as separate parameters.
 */

#include "plant_parser.h"
#include "plant_lexer.h"
#include "plant_compat.h"
#include <stdlib.h>
#include <string.h>

/* ── PlantLang-generated parser functions (defined in the
   compiler's generated C code, linked at build time). ── */
extern tx_t parse_program(PlantArray* tokens);
extern tx_t parse_statement(PlantArray* tokens, long pos, tx_t clv,
                            PlantArray* ctab, PlantArray* rtab,
                            long bstart, tx_t emode);

/* ── Internal parser context ── */
typedef struct {
    ILexer*     lexer;       /* reference to the bound lexer */
    PlantArray* tokens;      /* cached token list from lexer */
    PlantArray* ctab;        /* closure variable table */
    PlantArray* rtab;        /* redeclaration table */
    tx_t        clv;         /* current lexical variant */
    long        bstart;      /* block start index */
    tx_t        emode;       /* elevation mode flag */
} _ParserContext;

/* ── IParser vtable implementations ── */

static void* _iparser_parse(void* ctx, const char* source) {
    _ParserContext* pc = (_ParserContext*)ctx;
    if (!pc || !pc->lexer) return NULL;

    /* Tokenize through the ILexer interface */
    pc->tokens = (PlantArray*)pc->lexer->tokenize(pc->lexer->context, source);
    if (!pc->tokens) return NULL;

    /* Parse the full program */
    return parse_program(pc->tokens);
}

static void* _iparser_parse_statement(void* ctx) {
    _ParserContext* pc = (_ParserContext*)ctx;
    if (!pc || !pc->tokens) return NULL;

    /* Peek current position through the lexer */
    long pos = 0;
    if (pc->lexer && pc->lexer->context) {
        /* Extract position from lexer context — the lexer's internal
           context holds the cursor. We access it through the peek
           interface to stay decoupled. For the default PlantLexer,
           the context _is_ the _LexerContext, but we don't depend
           on that layout here. */
        void* tok = pc->lexer->peek(pc->lexer->context);
        (void)tok;
    }

    return parse_statement(pc->tokens, pos, pc->clv, pc->ctab,
                           pc->rtab, pc->bstart, pc->emode);
}

static void* _iparser_parse_expression(void* ctx) {
    /* parse_expression is not a standalone generated function in
       PlantLang — expressions are parsed inline by parse_statement
       and its sub-parsers. This is a stub for future extension. */
    (void)ctx;
    return NULL;
}

static void* _iparser_parse_block(void* ctx) {
    /* parse_block is similarly handled inline by the recursive
       descent parser. Stub for future extension. */
    (void)ctx;
    return NULL;
}

/* ── Constructor / Destructor ── */

IParser* PlantParser_create(void* context, ILexer* lexer) {
    IParser* parser = (IParser*)malloc(sizeof(IParser));
    if (!parser) return NULL;

    _ParserContext* pc = (_ParserContext*)malloc(sizeof(_ParserContext));
    if (!pc) { free(parser); return NULL; }

    pc->lexer   = lexer;
    pc->tokens  = NULL;
    pc->ctab    = NULL;
    pc->rtab    = NULL;
    pc->clv     = NULL;
    pc->bstart  = 0;
    pc->emode   = NULL;

    parser->context          = context ? context : pc;
    parser->parse            = _iparser_parse;
    parser->parse_statement  = _iparser_parse_statement;
    parser->parse_expression = _iparser_parse_expression;
    parser->parse_block      = _iparser_parse_block;

    if (!context) parser->context = pc;

    return parser;
}

void PlantParser_destroy(IParser* parser) {
    if (!parser) return;
    if (parser->context) {
        _ParserContext* pc = (_ParserContext*)parser->context;
        /* Don't free pc->tokens/ctab/rtab — owned by PlantLang runtime */
        free(pc);
    }
    free(parser);
}

/* ── Convenience helpers ── */

void* plant_iParser_parse(IParser* parser, const char* source) {
    if (parser && parser->parse) return parser->parse(parser->context, source);
    return NULL;
}

void* plant_iParser_parse_statement(IParser* parser) {
    if (parser && parser->parse_statement)
        return parser->parse_statement(parser->context);
    return NULL;
}

void* plant_iParser_parse_expression(IParser* parser) {
    if (parser && parser->parse_expression)
        return parser->parse_expression(parser->context);
    return NULL;
}

void* plant_iParser_parse_block(IParser* parser) {
    if (parser && parser->parse_block)
        return parser->parse_block(parser->context);
    return NULL;
}
