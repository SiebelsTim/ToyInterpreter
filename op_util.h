#ifndef PHPINTERP_OP_UTIL_H
#define PHPINTERP_OP_UTIL_H

#include "compile.h"

bool is_push_op(Operator op)
{
    switch (op) {
        case OP_STR:
        case OP_LONG:
        case OP_TRUE:
        case OP_FALSE:
        case OP_BIN:
        case OP_SUB:
        case OP_ADD:
        case OP_ADD1:
        case OP_SUB1:
        case OP_MUL:
        case OP_DIV:
        case OP_ASSIGN:
        case OP_LOOKUP:
            return true;
        case OP_JMP:
        case OP_JMPZ:
        case OP_NOP:
        case OP_INVALID:
        case OP_ECHO:
            return false;
    }

    compiletimeerror("is_push_op(): Did not handle %s\n", get_Operator_name(op));
    return false;
}

bool affects_stack(Operator op)
{
    return op != OP_JMP && op != OP_NOP;
}

bool is_jmp(Operator op)
{
    return op == OP_JMP || op == OP_JMPZ;
}

bool is_immediate(Operator op)
{
    switch (op) {
        case OP_STR:
        case OP_LONG:
        case OP_TRUE:
        case OP_FALSE:
            return true;
        case OP_INVALID:
        case OP_ECHO:
        case OP_BIN:
        case OP_SUB:
        case OP_ADD:
        case OP_ADD1:
        case OP_SUB1:
        case OP_MUL:
        case OP_DIV:
        case OP_ASSIGN:
        case OP_LOOKUP:
        case OP_JMP:
        case OP_JMPZ:
        case OP_NOP:
            return false;
    }
}

size_t op_len(Operator op)
{
    switch (op) {
        case OP_STR:
        case OP_BIN:
        case OP_ASSIGN:
        case OP_LOOKUP:
        case OP_JMP:
        case OP_JMPZ:
            return 2;
        case OP_LONG:
            return 3;
        default:
            return 1;
    }
}

#endif //PHPINTERP_OP_UTIL_H
