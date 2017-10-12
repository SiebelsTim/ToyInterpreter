#ifndef PHPINTERP_OPT_INSTRUCTION_H
#define PHPINTERP_OPT_INSTRUCTION_H

#include "../compile.h"

typedef struct Instruction {
    Operator operator;
    union {
        int64_t lint;
        RelAddr addr;
        VARIANTTYPE type;
        char* str;
    } data;
} Instruction;

Instruction* code_to_instructions(Function* fn, size_t* n);
void print_instruction(Instruction ins);
void print_instructions(Instruction* ins, size_t n);


#endif //PHPINTERP_OPT_INSTRUCTION_H