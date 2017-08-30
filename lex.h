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
    TK_HTML,
    TK_END
} TOKEN;
enum MODE {
    PHP, NONPHP
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
        long lint;
    } u;
    char* error;
} State;

State* new_state(FILE*);
int destroy_state(State*);

int get_token(State*);
char* get_token_name(int);
void print_tokenstream(State*);

#endif //PHPINTERP_LEX_H
