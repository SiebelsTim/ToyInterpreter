#ifndef PHPINTERP_OP_UTIL_H
#define PHPINTERP_OP_UTIL_H

#include "compile.h"

static inline bool is_push_op(Operator op)
{
    switch (op) {
        case OP_STR:
        case OP_LONG:
        case OP_TRUE:
        case OP_FALSE:
        case OP_BIN:
        case OP_LTE:
        case OP_GTE:
        case OP_NOT:
        case OP_SUB:
        case OP_ADD:
        case OP_ADD1:
        case OP_SUB1:
        case OP_MUL:
        case OP_DIV:
        case OP_ASSIGN:
        case OP_LOOKUP:
        case OP_SHL:
        case OP_SHR:
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

static inline bool affects_stack(Operator op)
{
    return op != OP_JMP && op != OP_NOP;
}

static inline bool is_jmp(Operator op)
{
    return op == OP_JMP || op == OP_JMPZ;
}

static inline bool is_immediate(Operator op)
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
        case OP_NOT:
        case OP_LTE:
        case OP_GTE:
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
        case OP_SHL:
        case OP_SHR:
            return false;
    }
}

static inline size_t op_len(Operator op)
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

static inline void swap_adjacent_ops(Operator* op1, Operator* op2)
{
    size_t len1 = op_len(*op1);
    size_t len2 = op_len(*op2);
    assert(op1 + len1 == op2 || op2 + len2 == op1);
    if (len1 == len2) {
        while (len1--) {
            Operator tmp = op2[len1];
            op2[len1] = op1[len1];
            op1[len1] = tmp;
        }
    } else if (len1 > len2) {
        Operator cpy[len1];
        memcpy(cpy, op1, sizeof(cpy));
        for (size_t i = 0; i < len2; ++i) {
            op1[i] = op2[i];
        }
        for (size_t i = 0; i < arrcount(cpy); ++i) {
            op1[len2 + i] = cpy[i];
        }
    } else {
        swap_adjacent_ops(op2, op1);
    }
}


static inline void insert_nop(Operator* op)
{
    size_t len = op_len(*op);
    while (len--) {
        *op++ = OP_NOP;
    }
}

#endif //PHPINTERP_OP_UTIL_H
