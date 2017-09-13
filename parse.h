#ifndef PHPINTERP_PARSE_H
#define PHPINTERP_PARSE_H

#include <stdio.h>
#include "lex.h"

typedef enum ASTTYPE {
    BLOCKSTMT,
    ECHOSTMT,
    IFSTMT,
    STRINGEXPR,
    BINOP,
    LONGEXPR,
    VAREXPR,
    ASSIGNMENTEXPR
} ASTTYPE;

typedef struct AST {
    ASTTYPE type;
    union {
        char* str;
        long lint;
    } val;
    struct AST* left;
    struct AST* right;
    struct AST* extra;
    struct AST* next;
} AST;

AST* parse(FILE*);

void print_ast(AST*, int level);

void destroy_ast(AST*);

static inline char* overtake_str(State* S)
{
    assert(S->val == MALLOCSTR || S->val == STATICSTR);

    char* ret = S->u.string;

    S->val = NONE;
    S->u.string = NULL;

    return ret;
}

#endif //PHPINTERP_PARSE_H
