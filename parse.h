#ifndef PHPINTERP_PARSE_H
#define PHPINTERP_PARSE_H

#include <stdio.h>
#include "lex.h"
#include "enum-util.h"

#define ENUM_ASTTYPE(ENUM_EL)   \
           ENUM_EL(BLOCKSTMT, =0)   \
           ENUM_EL(ECHOSTMT,)    \
           ENUM_EL(IFSTMT,)      \
           ENUM_EL(WHILESTMT,)   \
           ENUM_EL(FORSTMT,)     \
           ENUM_EL(STRINGEXPR,)  \
           ENUM_EL(BINOP,)       \
           ENUM_EL(POSTFIXOP,)   \
           ENUM_EL(PREFIXOP,)    \
           ENUM_EL(LONGEXPR,)    \
           ENUM_EL(VAREXPR,)     \
           ENUM_EL(ASSIGNMENTEXPR,) \
           ENUM_EL(HTMLEXPR,)    \

DECLARE_ENUM(ASTTYPE, ENUM_ASTTYPE);

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
