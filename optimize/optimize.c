#include <stdbool.h>
#include <memory.h>
#include "optimize.h"
#include "../array-util.h"
#include "../op_util.h"
#include "../macros.h"


#define ITERATE_BLOCK(blk) Operator* current = (blk)->start; \
    while (current <= (blk)->end) {
#define END_ITERATE_BLOCK next_instruction(&current); }

static void optimizeerror(char* str) {
    compiletimeerror(str);
}

static Operator* next_instruction(Operator** opptr)
{
    assert(**opptr != OP_INVALID);
    size_t len = op_len(**opptr);
    (*opptr) += len;

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
            default:
                break;
        }
        size++;
        fn->ip++;
    }
    fn->ip = fn->code;

    ret[size].operator = NULL; // End of array

    return ret;
}

static int64_t fetch_long(const Operator* op)
{
    assert(*op == OP_LONG);
    ++op;
    int64_t ret = *(op++) << 4;
    ret |= *op;

    return ret;
}

static const char* fetch_str(Function* fn, const Operator* op)
{
    assert(*op == OP_STR || *op == OP_LOOKUP || *op == OP_ASSIGN);
    ++op;
    size_t idx = *op;
    assert(idx < fn->strlen);
    return fn->strs[idx];
}

// Returns true if possible to fetch long from op, return long in out param
static bool op_to_long(Function* fn, const Operator* op, int64_t* out)
{
    int64_t _;
    if (!out) { // maybe we only want to use it to check if it would succeed
        out = &_;
    }

    const char* str;
    char* end;
    switch (*op) {
        case OP_STR:
            str = fetch_str(fn, op);
            if (*str == '\0') *out = 0;
            *out = strtoll(str, &end, 10);
            if (*end != '\0') *out = 1; // strtoll did not succeed entirely.
                                        // non-empty strs are truthy
            break;
        case OP_LONG:
            *out = fetch_long(op);
            break;
        case OP_TRUE:
            *out = 1;
            break;
        case OP_FALSE:
            *out = 0;
            break;
        default:
            return false;
    }
    return true;
}


static Instruction* find_instruction_with_addr(Function* fn, Instruction* ins, RelAddr addr)
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
    int last = 0;
    if (size == 0)
        return size;

    for (size_t i = 1; i < size; i++)
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
        if (is_jmp(*ins->operator)) {
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
    size_t len = op_len(*op);
    while (len--) {
        *op++ = OP_NOP;
    }
}

static void remove_add0(Function* fn, Block* blk)
{
    UNUSED(fn);
    // last two longs
    Operator* last = NULL;
    Operator* last2 = NULL;
    ITERATE_BLOCK(blk)
    {
        if (*current == OP_ADD) {
            assert(last && last2);
            int64_t lint;
            if (*last2 == OP_LONG && op_to_long(fn, last2, &lint) && lint == 0) {
                insert_nop(last2);
                insert_nop(current); // For OP_LONG
            } else if (*last == OP_LONG && op_to_long(fn, last, &lint) && lint == 0) {
                insert_nop(last);
                insert_nop(current); // For OP_LONG
            }
        }

        if (affects_stack(*current)) {
            last = last2;
            last2 = current;
        }
    }
    END_ITERATE_BLOCK
}


// Lookup $a => assign $a is identical to Lookup $a
static void remove_lookup_assign(Function* fn, Block* blk)
{
    // last stack operator
    Operator* last = NULL;
    ITERATE_BLOCK(blk)
    {
        if (*current == OP_ASSIGN) {
            assert(last);
            if (*last == OP_LOOKUP) {
                const char* lookupname = fetch_str(fn, last);
                const char* assignname = fetch_str(fn, current);
                if (strcmp(lookupname, assignname) == 0) {
                    insert_nop(current);
                }
            }
        }

        if (affects_stack(*current)) {
            last = current;
        }
    }
    END_ITERATE_BLOCK
}


static void remove_jmpz_always_true(Function* fn, Block* blk)
{
    // Last stack op
    Operator* last = NULL;
    ITERATE_BLOCK(blk)
    {
        int64_t lint;
        if (*current == OP_JMPZ && op_to_long(fn, last, &lint)) {
            if (lint) { // Jump is never taken, simply remove it
                insert_nop(last);
                insert_nop(current);
            } else { // Jump is always taken. Can be JMP
                insert_nop(last);
                *current = OP_JMP;
            }
        }

        if (affects_stack(*current)) {
            last = current;
        }
    }
    END_ITERATE_BLOCK
}


static bool jmplist_contains(Operator** jmps, size_t size, RelAddr rel_addr)
{
    for (size_t i = 0; i < size; ++i) {
        if (*jmps[i] == rel_addr) {
            return true;
        }
    }

    return false;
}

static void adjust_jumps(Operator** jmps, size_t size, RelAddr removed_nop_rel, const Operator* op)
{
    for (size_t i = 0; i < size; ++i) {
        assert(*jmps[i] != removed_nop_rel);
        if (*jmps[i] >= removed_nop_rel) {
            (*jmps[i]) -= 1; // Jump address moved
        }
        if (jmps[i] >= op) {
            jmps[i] -= 1; // The pointer will move
        }
    }
}

static void remove_nops(Function* fn)
{
    size_t capacity = 4;
    size_t size = 0;
    Operator** jmps = calloc(sizeof(*jmps), capacity);
    while ((size_t)(fn->ip - fn->code) < fn->codesize) {
        if (*fn->ip == OP_JMP || *fn->ip == OP_JMPZ) {
            try_resize(&capacity, size, (void**) &jmps, sizeof(*jmps), optimizeerror);
            jmps[size++] = ++fn->ip;
            fn->ip++; // Skip over jump addr
        }
        next_instruction(&fn->ip);
    }
    fn->ip = fn->code;


    while ((size_t)(fn->ip - fn->code) < fn->codesize) {
        if (*fn->ip == OP_NOP && !jmplist_contains(jmps, size, fn->ip - fn->code)) {
            adjust_jumps(jmps, size, (RelAddr)(fn->ip - fn->code), fn->ip);
            memmove(fn->ip, fn->ip+1, (fn->codesize - (fn->ip - fn->code)) * sizeof(*fn->ip));
            fn->codesize--;
        } else {
            next_instruction(&fn->ip);
        }
    }
    fn->ip = fn->code;

    free(jmps);
}

static Optimizer localoptimizations[] = {
        remove_add0,
        remove_lookup_assign,
        remove_jmpz_always_true
};

static GlobalOptimizer globaloptimizations[] = {
        remove_nops // This must be last, as it changes Blocks
};

void optimize(Function* fn)
{
    Block* blocks = identify_basic_blocks(fn);

    for (size_t i = 0; i < arrcount(localoptimizations); ++i) {
        Block* current = blocks;
        while (current) {
            localoptimizations[i](fn, current);
            current = current->next;
        }
    }

    free_block(blocks);


    for (size_t i = 0; i < arrcount(globaloptimizations); ++i) {
        globaloptimizations[i](fn);
    }
}