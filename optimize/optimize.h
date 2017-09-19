#ifndef PHPINTERP_OPTIMIZE_H
#define PHPINTERP_OPTIMIZE_H

#include "../compile.h"

typedef struct Block {
    Operator* start;
    Operator* end;
    struct Block* next;
} Block;

typedef struct Instruction {
    Operator* operator;
    union {
        int64_t lint;
        RelAddr addr;
        char* str;
    } data;
} Instruction;

typedef void (*Optimizer)(Function*, Block*);
typedef void (*GlobalOptimizer)(Function*);

Instruction* code_to_instructions(Function* fn);
void optimize(Function* fn);

#endif //PHPINTERP_OPTIMIZE_H
