#ifndef PHPINTERP_PARSE_H
#define PHPINTERP_PARSE_H

#include <stdio.h>
#include "lex.h"
#include "enum-util.h"

#define ENUM_ASTTYPE(ENUM_EL)   \
           ENUM_EL(AST_BLOCK, =0)   \
           ENUM_EL(AST_ECHO,)    \
           ENUM_EL(AST_IF,)      \
           ENUM_EL(AST_WHILE,)   \
           ENUM_EL(AST_FOR,)     \
           ENUM_EL(AST_STRING,)  \
           ENUM_EL(AST_BINOP,)       \
           ENUM_EL(AST_POSTFIXOP,)   \
           ENUM_EL(AST_PREFIXOP,)    \
           ENUM_EL(AST_NOTOP,)       \
           ENUM_EL(AST_LONG,)    \
           ENUM_EL(AST_NULL,)    \
           ENUM_EL(AST_VAR,)     \
           ENUM_EL(AST_ASSIGNMENT,) \
           ENUM_EL(AST_FUNCTION,) \
           ENUM_EL(AST_CALL,) \
           ENUM_EL(AST_RETURN,) \
           ENUM_EL(AST_HTML,)    \

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
