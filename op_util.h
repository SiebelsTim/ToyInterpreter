#ifndef PHPINTERP_OP_UTIL_H
#define PHPINTERP_OP_UTIL_H

#include <memory.h>
#include "compile.h"
#include "util.h"

static inline bool is_push_op(Operator op)
{
    switch (op) {
        case OP_RETURN:
        case OP_CALL:
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
        case OP_NULL:
        case OP_CAST:
            return true;
        case OP_JMP:
        case OP_JMPZ:
        case OP_NOP:
        case OP_INVALID:
        case OP_ECHO:
            return false;
        case OP_MAX_VALUE:
            assert(false && "Encountered OP_MAX_VALUE");
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
        case OP_NULL:
            return true;
        case OP_INVALID:
        case OP_CALL:
        case OP_RETURN:
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
        case OP_CAST:
            return false;
        case OP_MAX_VALUE:
            assert(false && "Encountered OP_MAX_VALUE");
            return false;
    }
}

static inline size_t op_len(Operator op)
{
    switch (op) {
        case OP_STR:
        case OP_ASSIGN:
        case OP_LOOKUP:
        case OP_BIN:
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

static inline void swap_adjacent_ops(codepoint_t* op1, codepoint_t* op2)
{
    size_t len1 = op_len((Operator)*op1);
    size_t len2 = op_len((Operator)*op2);
    assert(op1 + len1 == op2 || op2 + len2 == op1);
    if (len1 == len2) {
        while (len1--) {
            codepoint_t tmp = op2[len1];
            op2[len1] = op1[len1];
            op1[len1] = tmp;
        }
    } else if (len1 > len2) {
        const size_t cpy_len = len1 * sizeof(codepoint_t);
        codepoint_t* cpy = malloc(cpy_len);
        memcpy(cpy, op1, cpy_len);
        for (size_t i = 0; i < len2; ++i) {
            op1[i] = op2[i];
        }
        for (size_t i = 0; i < cpy_len / sizeof(*cpy); ++i) {
            op1[len2 + i] = cpy[i];
        }
    } else {
        swap_adjacent_ops(op2, op1);
    }
}

static inline void insert_nop(codepoint_t* op)
{
    size_t len = op_len((Operator) *op);
    while (len--) {
        *op++ = OP_NOP;
    }
}

#endif //PHPINTERP_OP_UTIL_H
