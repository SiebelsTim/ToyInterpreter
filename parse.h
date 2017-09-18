#ifndef PHPINTERP_PARSE_H
#define PHPINTERP_PARSE_H

#include <stdio.h>
#include "lex.h"

typedef enum ASTTYPE {
    BLOCKSTMT,
    ECHOSTMT,
    IFSTMT,
    WHILESTMT,
    FORSTMT,
    STRINGEXPR,
    BINOP,
    LONGEXPR,
    VAREXPR,
    ASSIGNMENTEXPR,
    HTMLEXPR
} ASTTYPE;

typedef struct AST {
    ASTTYPE type;
    union {
        char* str;
        int64_t lint;
    } val;
    struct AST* node1;
    struct AST* node2;
    struct AST* node3;
    struct AST* node4;
    struct AST* next;
} AST;

AST* parse(FILE*);

char* get_ast_typename(ASTTYPE ast);
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
