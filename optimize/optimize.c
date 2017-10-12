#include "optimize.h"
#include "../util.h"
#include "instruction.h"

static Stack* find_param1(Stack *stack, size_t current)
{
    Stack initial = stack[current];
    for (long i = current-1; i >= 0; --i) {
        if (stack[i].depth == initial.depth - 1 -  get_sp_change(*initial.ins)) {
            return &stack[i];
        }
    }

    return NULL;
}

static Stack* find_param2(Stack *stack, size_t current)
{
    for (long i = current-1; i >= 0; --i) {
        if (stack[i].ins->operator != OP_NOP && stack[i].ins->operator != OP_JMP) {
            return &stack[i];
        }
    }

    return NULL;
}

void remove_add0(Stack* stack, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        Stack* s = &stack[i];
        if (s->ins->operator == OP_ADD) {
            assert(find_param1(stack, i) != NULL && find_param2(stack, i) != NULL);
            Stack* param1 = find_param1(stack, i);
            Stack* param2 = find_param2(stack, i);
            if (param1->ins->operator == OP_LONG && param1->ins->data.lint == 0) {
                param1->ins->operator = OP_NOP;
                param1->ins->width = 8;
                s->ins->operator = OP_NOP;
                recalculate_stackdepths(stack, len);
            } else if (param2->ins->operator == OP_LONG && param2->ins->data.lint == 0) {
                param2->ins->operator = OP_NOP;
                param2->ins->width = 8;
                s->ins->operator = OP_NOP;
                recalculate_stackdepths(stack, len);
            }
        }
    }
}

void remove_cast(Stack* stack, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        Stack* s = &stack[i];
        if (s->ins->operator == OP_CAST) {
            Stack* param1 = &stack[i - 1];
            if (param1->ins->operator == OP_TRUE ||
                param1->ins->operator == OP_FALSE ||
                param1->ins->operator == OP_AND ||
                param1->ins->operator== OP_OR) {
                s->ins->operator= OP_NOP;
                s->ins->width = 2;
            }
            // We do not need to recalculate stackdepths, as OP_CAST does +-0
        }
    }
}

static Optimizer* optimizations[] = {
        remove_add0,
        remove_cast
};

// TODO: Operate on Basic Block level
void optimize_function(Function* fn)
{
    size_t ins_len;
    Instruction* ins = code_to_instructions(fn, &ins_len);
    Stack* stack = decorate_instructions(ins, ins_len);

    for (size_t i = 0; i < arrcount(optimizations); ++i) {
        optimizations[i](stack, ins_len);
    }

    free(stack);

    fn->codesize = 0;
    for (size_t i = 0; i < fn->strlen; ++i) {
        free(fn->strs[i]);
    }
    fn->strlen = 0;

    for (size_t i = 0; i < ins_len; ++i) {
        emit_instruction(fn, ins[i]);
    }

    for (size_t i = 0; i < ins_len; ++i) {
        free_instruction(ins[i]);
    }
    free(ins);
}