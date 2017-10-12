#ifndef PHPINTERP_OPT_THREEADDRCODE_H
#define PHPINTERP_OPT_THREEADDRCODE_H

#include "../stack.h"
#include "instruction.h"


typedef struct Stack {
    Instruction* ins;
    size_t depth;
} Stack;

int get_sp_change(Instruction ins);
Stack* decorate_instructions(Instruction* ins, size_t ins_len);
Stack* recalculate_stackdepths(Stack* stack, size_t len);
void print_decorated_instructions(Stack* stack, size_t len);

#endif // PHPINTERP_OPT_THREEADDRCODE_H