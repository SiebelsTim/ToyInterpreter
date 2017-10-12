#include "optimize.h"
#include "../util.h"
#include "instruction.h"

static Stack* find_affected_element(Stack* stack, size_t current)
{
    Stack initial = stack[current];
    for (long i = current-1; i >= 0; --i) {
        if (stack[i].depth == initial.depth - 1 -  get_sp_change(initial.ins)) {
            return &stack[i];
        }
    }

    return NULL;
}

void remove_add0(Stack* stack, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        Stack* s = &stack[i];
        if (s->ins.operator == OP_ADD) {
            Stack* param1 = &stack[i-1];
            assert(find_affected_element(stack, i) != NULL);
            Stack* param2 = find_affected_element(stack, i);
            if (param1->ins.operator == OP_LONG && param1->ins.data.lint == 0) {
                param1->ins.operator = OP_NOP;
                s->ins.operator = OP_NOP;
                recalculate_stackdepths(stack, len);
            } else if (param2->ins.operator == OP_LONG && param2->ins.data.lint == 0) {
                param2->ins.operator = OP_NOP;
                s->ins.operator = OP_NOP;
                recalculate_stackdepths(stack, len);
            }
        }
    }
}

void remove_cast(Stack* stack, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        Stack* s = &stack[i];
        if (s->ins.operator == OP_CAST) {
            Stack* param1 = &stack[i - 1];
            if (param1->ins.operator == OP_TRUE ||
                param1->ins.operator == OP_FALSE ||
                param1->ins.operator == OP_AND ||
                param1->ins.operator== OP_OR) {
                s->ins.operator= OP_NOP;
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

    // TODO: Stack into function
    print_decorated_instructions(stack, ins_len);

    free(stack);
    free(ins);
}