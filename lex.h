#ifndef PHPINTERP_LEX_H
#define PHPINTERP_LEX_H

#include <stdio.h>

#define arrcount(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef enum TOKEN {
    TK_OPENTAG = 256, // Make them outside ascii range
    TK_ECHO,
    TK_STRING,
    TK_LONG,
    TK_FUNCTION,
    TK_IF,
    TK_ELSE,
    TK_TRUE,
    TK_FALSE,
    TK_VAR,

    TK_AND,
    TK_OR,
    TK_EQ,

    TK_HTML,
    TK_WHILE,
    TK_FOR,

    TK_PLUSPLUS,
    TK_MINUSMINUS,

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

typedef struct State {
    enum MODE mode;
    int lineno;
    FILE* file;
    int token;
    int lexchar;
    VALTYPE val;
    union {
        char* string;
        int64_t lint;
    } u;
    char* error;
} State;

State* new_state(FILE*);
int destroy_state(State*);

int get_token(State*);
char* get_token_name(int);
void print_tokenstream(State*);

static inline void state_set_string(State* S, char* str)
{
    if (S->val == MALLOCSTR) {
        free(S->u.string);
    }

    S->val = MALLOCSTR;
    S->u.string = str;
}

static inline void state_set_long(State* S, int64_t n)
{
    if (S->val == MALLOCSTR) {
        free(S->u.string);
    }

    S->val = LONGVAL;
    S->u.lint = n;
}

#endif //PHPINTERP_LEX_H
