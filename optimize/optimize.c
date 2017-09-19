#include <stdbool.h>
#include <memory.h>
#include "optimize.h"
#include "../array-util.h"

static void optimizeerror(char* str) {
    compiletimeerror(str);
}

static Operator* next_instruction(Operator** opptr)
{
        switch (**opptr) {
            case OP_STR:
            case OP_BIN:
            case OP_ASSIGN:
            case OP_LOOKUP:
            case OP_JMP:
            case OP_JMPZ:
                (*opptr) += 1; // Skip
                break;
            case OP_LONG:
                (*opptr) += 2; // Longs are two operators in size
                break;

            // Next, list all cases without parameters, so we get warnings if we forget
            // to update this switch
            case OP_ECHO:
            case OP_TRUE:
            case OP_FALSE:
            case OP_SUB:
            case OP_ADD:
            case OP_ADD1:
            case OP_SUB1:
            case OP_MUL:
            case OP_DIV:
            case OP_NOP:
                break;
            case OP_INVALID:
                compiletimeerror("OP Invalid encountered");
                break;
        }
    (*opptr) += 1;
    return *opptr;
}

Instruction* code_to_instructions(Function* fn)
{
    size_t size = 0;
    Instruction* ret = calloc(sizeof(Instruction), fn->codesize + 1); // This can only get smaller
    while ((size_t)(fn->ip - fn->code) < fn->codesize) {
        ret[size].operator = fn->ip;
        switch (*fn->ip) {
            case OP_STR:
            case OP_ASSIGN:
            case OP_LOOKUP:
                fn->ip++;
                assert(*fn->ip < fn->strlen);
                ret[size].data.str = fn->strs[*fn->ip];
                break;
            case OP_BIN:
                ret[size].data.lint = *++fn->ip;
                break;
            case OP_JMP:
            case OP_JMPZ:
                ret[size].data.addr = *++fn->ip;
                break;
            case OP_LONG:
                ret[size].data.lint = *++fn->ip << 4;
                ret[size].data.lint |= *++fn->ip;
                break;
        }
        size++;
        fn->ip++;
    }
    fn->ip = fn->code;

    ret[size].operator = NULL; // End of array

    return ret;
}

static int64_t fetch_long(Operator* op)
{
    assert(*op == OP_LONG);
    ++op;
    int64_t ret = *(op++) << 4;
    ret |= *op;

    return ret;
}

static bool is_push_operation(Operator op)
{
    return op != OP_NOP && op != OP_JMPZ && op != OP_JMP && op != OP_ECHO;
}


static Instruction* find_instruction_with_addr(Function* fn, Instruction* ins, int8_t addr)
{
    while (ins->operator != NULL) {
        if (ins->operator - fn->code == addr) {
            return ins;
        }
        ++ins;
    }

    return NULL;
}


static inline Block* new_block(Operator* start, Operator* end)
{
    Block* ret = calloc(1, sizeof(Block));
    ret->start = start;
    ret->end = end;

    return ret;
}

static inline void free_block(Block* block)
{
    assert(block);
    if (block->next) {
        free_block(block->next);
    }
    free(block);
}


static int greater_than(const void* p1, const void* p2)
{
    return *(Operator**)p1 > *(Operator**)p2;
}

static size_t remove_duplicates(Instruction** array, size_t size)
{
    int i;
    int last = 0;
    if (size == 0)
        return size;

    for (i = 1; i < size; i++)
    {
        if (array[i] != array[last])
            array[++last] = array[i];
    }

    return size;
}

static Block* identify_basic_blocks(Function* fn)
{
    size_t capacity = 5;
    size_t size = 0;
    Instruction** leaders = calloc(capacity, sizeof(*leaders));
    Instruction* ins = code_to_instructions(fn);
    Instruction* ins_start = ins;

    while (ins->operator != NULL) {
        if (*ins->operator == OP_JMP || *ins->operator == OP_JMPZ) {
            try_resize(&capacity, size, (void**)&leaders, sizeof(*leaders), optimizeerror);
            leaders[size++] = find_instruction_with_addr(fn, ins_start, ins->data.addr);
            assert(leaders[size-1] != NULL);
            try_resize(&capacity, size, (void**)&leaders, sizeof(*leaders), optimizeerror);
            leaders[size++] = (ins+1);
        }
        ins++;
    }
    fn->ip = fn->code;
    qsort(leaders, size, sizeof(*leaders), greater_than);
    size = remove_duplicates(leaders, size);

    Block* ret = new_block(fn->code, NULL);
    Block* last_block = ret;
    for (size_t i = 0; i < size; ++i) {
        Instruction* leader = leaders[i];
        last_block->end = (leader - 1)->operator; // previous
        last_block->next = new_block(leader->operator, NULL);
        last_block = last_block->next;
    }
    last_block->end = &fn->code[fn->codesize - 1];
    free(leaders);
    free(ins_start);
    return ret;
}

static void insert_nop(Operator* op)
{
    switch (*op) {
        case OP_STR:
        case OP_BIN:
        case OP_ASSIGN:
        case OP_LOOKUP:
        case OP_JMP:
        case OP_JMPZ:
            *op++ = OP_NOP;
            break;
        case OP_LONG:
            *op++ = OP_NOP;
            *op++ = OP_NOP; // Longs are two operators in size
            break;
        default:
            break;
    }

    *op = OP_NOP;
}

static void remove_add0(Function* fn, Block* blk)
{
    // last two longs
    Operator* last = NULL;
    Operator* last2 = NULL;
    Operator* current = blk->start;
    while (current <= blk->end) {
        if (*current == OP_ADD) {
            assert(last && last2);
            if (*last2 == OP_LONG && fetch_long(last2) == 0) {
                insert_nop(last2);
                insert_nop(current); // For OP_LONG
            } else if (*last == OP_LONG && fetch_long(last) == 0) {
                insert_nop(last);
                insert_nop(current); // For OP_LONG
            }
        }

        if (is_push_operation(*current)) {
            last = last2;
            last2 = current;
        }

        next_instruction(&current);
    }
}


// Lookup $a => assign $a is identical to Lookup $a
static void remove_lookup_assign(Function* fn, Block* blk)
{
    // last stack operator
    Operator* last = NULL;
    Operator* current = blk->start;
    while (current < blk->end) {
        if (*current == OP_ASSIGN) {
            assert(last);
            if (*last == OP_LOOKUP) {
                assert(*(last+1) < fn->strlen);
                const char* lookupname = fn->strs[*(last+1)];
                assert(*(current+1) < fn->strlen);
                const char* assignname = fn->strs[*(current+1)];
                if (strcmp(lookupname, assignname) == 0) {
                    insert_nop(current);
                }
            }
        }

        if (is_push_operation(*current)) {
            last = current;
        }
        next_instruction(&current);
    }
}

static Optimizer optimizations[] = {
        remove_add0,
        remove_lookup_assign
};

void optimize(Function* fn)
{
    Block* blocks = identify_basic_blocks(fn);

    for (int i = 0; i < arrcount(optimizations); ++i) {
        Block* current = blocks;
        while (current) {
            optimizations[i](fn, current);
            current = current->next;
        }
    }

    free_block(blocks);
}