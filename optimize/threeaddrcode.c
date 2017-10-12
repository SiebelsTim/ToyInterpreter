#include "threeaddrcode.h"

int get_sp_change(Instruction ins)
{
    switch (ins.operator) {
        case OP_CALL:
            return (int8_t)ins.data.lint;
        case OP_STR:
        case OP_LONG:
        case OP_TRUE:
        case OP_FALSE:
        case OP_NULL:
        case OP_LOOKUP:
            return +1;
        case OP_ASSIGN:
        case OP_LTE:
        case OP_GTE:
        case OP_LT:
        case OP_GT:
        case OP_NOT:
        case OP_AND:
        case OP_OR:
        case OP_EQ:
        case OP_CONCAT:
        case OP_SUB:
        case OP_ADD:
        case OP_MUL:
        case OP_DIV:
        case OP_SHL:
        case OP_SHR:
        case OP_RETURN:
        case OP_ECHO:
            return -1;
        case OP_JMP:
        case OP_JMPZ:
        case OP_ADD1:
        case OP_SUB1:
        case OP_CAST:
        case OP_NOP:
            return 0;
        case OP_MAX_VALUE:
        case OP_INVALID:
            assert(false);
            break;
    }

    assert(false);
    return 0;
}

TAC* instructions_to_tac(Instruction* ins, size_t ins_len)
{
    int sp = 0;
    for (size_t i = 0; i < ins_len; ++i)
    {
        // TODO: Missing implementation
        sp += get_sp_change(ins[i]);
    }

    return NULL;
}

Stack* recalculate_stackdepths(Stack* stack, size_t len)
{
    int sp = 0;
    for (size_t i = 0; i < len; ++i) {
        sp += get_sp_change(stack[i].ins);
        stack[i].depth = (size_t)sp;
        assert(sp >= 0);
    }

    return stack;
}

Stack* decorate_instructions(Instruction* ins, size_t ins_len)
{
    Stack* ret = calloc(ins_len, sizeof(*ret));

    int sp = 0;
    for (size_t i = 0; i < ins_len; ++i) {
        sp += get_sp_change(ins[i]);
        ret[i].ins = ins[i];
        ret[i].depth = (size_t)sp;
        assert(sp >= 0);
    }

    return ret;
}

void print_decorated_instructions(Stack* stack, size_t len)
{
    printf("dest | Operator\n---------------\n");
    for (size_t i = 0; i < len; ++i) {
        printf("%zu    < ", stack[i].depth);
        print_instruction(stack[i].ins);
        puts("");
    }
}