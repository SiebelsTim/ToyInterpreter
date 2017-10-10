#ifndef PHPINTERP_COMPILE_H
#define PHPINTERP_COMPILE_H

#include <stdint.h>
#include <stdbool.h>
#include "parse.h"
#include "crossplatform/endian.h"
#include "crossplatform/stdnoreturn.h"
#include "crossplatform/std.h"

#define ENUM_OPERATOR(ENUM_EL) \
        ENUM_EL(OP_INVALID, = 0) \
        ENUM_EL(OP_RETURN,) \
        ENUM_EL(OP_CALL,)  \
        ENUM_EL(OP_ECHO,) \
        ENUM_EL(OP_STR,) \
        ENUM_EL(OP_LONG,) \
        ENUM_EL(OP_TRUE,) \
        ENUM_EL(OP_FALSE,) \
        ENUM_EL(OP_NULL,)  \
        ENUM_EL(OP_BIN,) \
        ENUM_EL(OP_LTE,)  \
        ENUM_EL(OP_GTE,)  \
        ENUM_EL(OP_NOT,)  \
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
        ENUM_EL(OP_MAX_VALUE,)

DECLARE_ENUM(Operator, ENUM_OPERATOR);

typedef uint8_t codepoint_t;

typedef struct Function {
    char* name;
    lineno_t lineno_defined;
    uint8_t paramlen;
    char** params;

    size_t codesize;
    size_t codecapacity;
    codepoint_t* code;

    char** strs;
    uint16_t strlen;
    uint16_t strcapacity;

    struct Function** functions;
    size_t funlen;
    size_t funcapacity;

    lineno_t lastline;
} Function;

typedef uint32_t RelAddr;
_Static_assert(sizeof(RelAddr) == sizeof(codepoint_t)*4, "RelAddr must be == 4 Operators in size");
_Static_assert((codepoint_t) OP_MAX_VALUE == OP_MAX_VALUE, "OP must fit into codepoint_t");

Function* create_function(char* name);
void free_function(Function* fn);

Function* compile(Function* fn, AST* root);

_Noreturn void compiletimeerror(char* fmt, ...);

void print_code(Function* fn);


static inline uint8_t fetch8(const codepoint_t* ip)
{
    return *ip;
}

static inline uint16_t fetch16(const codepoint_t* ip)
{
    return le16toh(*(uint16_t*)ip);
}

static inline uint32_t fetch32(const codepoint_t* ip)
{
    return le32toh(*(uint32_t*)ip);
}

static inline uint64_t fetch64(const codepoint_t* ip)
{
    return le64toh(*(uint64_t*)ip);
}

#endif //PHPINTERP_COMPILE_H
