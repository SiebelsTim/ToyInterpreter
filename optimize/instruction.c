#include "instruction.h"
#include "../util.h"

Instruction* code_to_instructions(Function* fn, size_t* n)
{
    assert(n && "nullptr given");
    size_t size = 0;
    Instruction* ret = calloc(sizeof(Instruction), fn->codesize + 1); // This can only get smaller
    codepoint_t* ip = fn->code;
    while ((size_t)(ip - fn->code) < fn->codesize) {
        ret[size].operator = (Operator)*ip;
        switch (*ip++) {
            case OP_STR:
            case OP_ASSIGN:
            case OP_LOOKUP:
                assert(*ip < fn->strlen);
                ret[size].data.str = fn->strs[fetch16(ip)];
                ip += 2;
                break;
            case OP_JMP:
            case OP_JMPZ:
                ret[size].data.addr = fetch32(ip);
                ip += 4;
                break;
            case OP_LONG:
                ret[size].data.lint = fetch64(ip);
                ip += 8;
                break;
            case OP_CALL:
                ret[size].data.lint = fetch8(ip++);
                break;
            case OP_CAST:
                ret[size].data.type = (VARIANTTYPE) fetch8(ip++);
                break;
            default:
                break;
        }
        size++;
    }

    *n = size;

    return ret;
}

void print_instruction(Instruction ins)
{
    printf("%s ", get_Operator_name(ins.operator));
    char* dest;
    switch (ins.operator) {
        case OP_STR:
        case OP_ASSIGN:
        case OP_LOOKUP:
            dest = malloc((strlen(ins.data.str) + 1) * 2);
            printf("\"%s\"", escaped_str(dest, ins.data.str));
            free(dest);
            break;
        case OP_JMP:
        case OP_JMPZ:
            printf("%" PRId32, ins.data.addr);
            break;
        case OP_LONG:
        case OP_CALL:
            printf("%" PRId64, ins.data.lint);
            break;
        case OP_CAST:
            printf("(%s)", get_VARIANTTYPE_name(ins.data.type));
            break;
        default:
            break;
    }
}

void print_instructions(Instruction* ins, size_t n)
{
    for (size_t i = 0; i < n; ++i, ++ins) {
        print_instruction(*ins);
        printf("\n");
    }
}