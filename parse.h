#ifndef PHPINTERP_PARSE_H
#define PHPINTERP_PARSE_H

#include <stdio.h>

typedef enum ASTTYPE {
    BLOCKSTMT,
    ECHOSTMT,
    STRINGEXPR,
    STRINGBINOP
} ASTTYPE;

typedef struct AST {
    ASTTYPE type;
    void* val;
    struct AST* left;
    struct AST* right;
    struct AST* next;
} AST;

AST* parse(FILE*);

void print_ast(AST*, int level);

void destroy_ast(AST*);

#endif //PHPINTERP_PARSE_H
