#ifndef PHPINTERP_LEX_H
#define PHPINTERP_LEX_H

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

typedef enum TOKEN {
    TK_OPENTAG = 256, // Make them outside ascii range
    TK_IDENTIFIER,
    TK_ECHO,
    TK_STRING,
    TK_LONG,
    TK_FUNCTION,
    TK_RETURN,
    TK_IF,
    TK_ELSE,
    TK_TRUE,
    TK_FALSE,
    TK_NULL,
    TK_VAR,

    TK_AND,
    TK_OR,
    TK_EQ,
    TK_LTEQ,
    TK_GTEQ,

    TK_WHILE,
    TK_FOR,

    TK_PLUSPLUS,
    TK_MINUSMINUS,
    TK_SHL,
    TK_SHR,

    TK_HTML,
    TK_END
} TOKEN;
enum MODE {
    PHP, NONPHP, EMITOPENTAG
};

typedef enum VALTYPE {
    NONE,
    MALLOCSTR,
    STATICSTR,
    LONGVAL,
    ERROR
} VALTYPE;

typedef uint32_t lineno_t;

typedef struct Lexer {
    enum MODE mode;
    lineno_t lineno;
    FILE* file;
    int token;
    int lexchar;
    VALTYPE val;
    union {
        char* string;
        int64_t lint;
    } u;
    char* error;
} Lexer;

Lexer* create_lexer(FILE *);
void destroy_lexer(Lexer *);

int get_token(Lexer*);
char* get_token_name(int);
void print_tokenstream(Lexer*);

static inline void state_set_string(Lexer* S, char* str)
{
    if (S->val == MALLOCSTR) {
        free(S->u.string);
    }

    S->val = MALLOCSTR;
    S->u.string = str;
}

static inline void state_set_long(Lexer* S, int64_t n)
{
    if (S->val == MALLOCSTR) {
        free(S->u.string);
    }

    S->val = LONGVAL;
    S->u.lint = n;
}

#endif //PHPINTERP_LEX_H
