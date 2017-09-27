#ifndef PHPINTERP_COMPILE_H
#define PHPINTERP_COMPILE_H

#include <stdint.h>
#include <stdbool.h>
#include "parse.h"

#define ENUM_OPERATOR(ENUM_EL) \
        ENUM_EL(OP_INVALID, = 0) \
        ENUM_EL(OP_ECHO,) \
        ENUM_EL(OP_STR,) \
        ENUM_EL(OP_LONG,) \
        ENUM_EL(OP_TRUE,) \
        ENUM_EL(OP_FALSE,) \
        ENUM_EL(OP_BIN,) \
        ENUM_EL(OP_SUB,) \
        ENUM_EL(OP_ADD,) \
        ENUM_EL(OP_ADD1,) \
        ENUM_EL(OP_SUB1,) \
        ENUM_EL(OP_MUL,) \
        ENUM_EL(OP_DIV,) \
        ENUM_EL(OP_SHL,) \
        ENUM_EL(OP_SHR,) \
        ENUM_EL(OP_ASSIGN,) \
        ENUM_EL(OP_LOOKUP,) \
        ENUM_EL(OP_JMP,) \
        ENUM_EL(OP_JMPZ,) \
        ENUM_EL(OP_NOP,) \

DECLARE_ENUM(Operator, ENUM_OPERATOR);

typedef struct Function {
    char* name;
    Operator* ip;
    size_t codesize;
    size_t codecapacity;
    Operator* code;

    char** strs;
    uint16_t strlen;
    uint16_t strcapacity;
} Function;

typedef uint32_t RelAddr;
_Static_assert(sizeof(RelAddr) == sizeof(Operator), "RelAddr must be == Operator in size");

Function* create_function();
void free_function(Function* fn);

Function* compile(Function* fn, AST* root);

void compiletimeerror(char* fmt, ...);

void print_code(Function* fn);

#endif //PHPINTERP_COMPILE_H
