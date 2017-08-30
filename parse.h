#ifndef PHPINTERP_PARSE_H
#define PHPINTERP_PARSE_H

#include <stdio.h>

typedef enum ASTTYPE {
    BLOCKSTMT,
    ECHOSTMT,
    IFSTMT,
    STRINGEXPR,
    STRINGBINOP,
    LONGEXPR
} ASTTYPE;

typedef struct AST {
    ASTTYPE type;
    union {
        char* str;
        long lint;
    } val;
    struct AST* left;
    struct AST* right;
    struct AST* next;
} AST;

AST* parse(FILE*);

void print_ast(AST*, int level);

void destroy_ast(AST*);

#endif //PHPINTERP_PARSE_H
