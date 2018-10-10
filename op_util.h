#ifndef PHPINTERP_OP_UTIL_H
#define PHPINTERP_OP_UTIL_H

#include <memory.h>
#include "compile.h"
#include "util.h"


static inline size_t op_len(Operator op)
{
    switch (op) {
        case OP_STR:
        case OP_ASSIGN:
        case OP_LOOKUP:
        case OP_CLOOKUP:
        case OP_CONSTDECL:
            return 3;
        case OP_CALL:
        case OP_CAST:
            return 2;
        case OP_JMP:
        case OP_JMPZ:
            return 5;
        case OP_LONG:
            return 9;
        default:
            return 1;
    }
}

#endif //PHPINTERP_OP_UTIL_H
