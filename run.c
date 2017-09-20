#include <stdlib.h>
#include <stdnoreturn.h>
#include <memory.h>
#include <assert.h>
#include <stdbool.h>
#include <lzma.h>
#include "run.h"
#include "parse.h"
#include "scope.h"
#include "compile.h"
#include "array-util.h"
#include "optimize/optimize.h"

Variant cpy_var(Variant var)
{
    Variant ret = var;
    if (ret.type == STRING) {
        ret.u.str = strdup(var.u.str);
    }

    return ret;
}

void free_var(Variant var)
{
    if (var.type == STRING) {
        free(var.u.str);
    }
}

noreturn void runtimeerror(char* fmt)
{
    puts(fmt);
    abort();
}


static Variant* stackidx(Runtime* R, long idx)
{
    if (idx < 0) {
        idx = R->stacksize + idx;
    }

    assert(idx < (int)R->stacksize && idx >= 0);
    return &R->stack[idx];
}

static Runtime* create_runtime()
{
    Runtime* ret = malloc(sizeof(Runtime));

    ret->stackcapacity = 10;
    ret->stacksize = 0;
    ret->stack = calloc(ret->stackcapacity, sizeof(*ret->stack));
    ret->scope = create_scope();

    return ret;
}

static void destroy_runtime(Runtime* R)
{
    for (int i = 0; i < (int) R->stacksize; ++i) {
        if (stackidx(R, i)->type == STRING) {
            free(stackidx(R, i)->u.str);
        }
    }
    free(R->stack);
    destroy_scope(R->scope);
    free(R);
}

static inline void try_stack_resize(Runtime* R)
{
    try_resize(&R->stackcapacity, R->stacksize, (void*)&R->stack,
               sizeof(*R->stack), runtimeerror);
}

static void push(Runtime* R, Variant val)
{
    try_stack_resize(R);
    R->stack[R->stacksize++] = cpy_var(val);
}

static inline Variant* top(Runtime* R)
{
    assert(R->stacksize > 0);
    return stackidx(R, -1);
}

static inline void popn(Runtime *R, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        Variant* ret = top(R);

        if (ret->type == STRING) {
            free(ret->u.str);
        }

        R->stacksize--;
        assert(R->stacksize >= 0);
    }
}

static inline void pop(Runtime* R) {
    popn(R, 1);
}

static inline void pushstr(Runtime* R, char* str)
{
    Variant var;
    var.type = STRING;
    var.u.str = str;
    push(R, var);
}


static inline void pushlong(Runtime* R, int64_t n)
{
    Variant var;
    var.type = LONG;
    var.u.lint = n;
    push(R, var);
}

static inline void pushbool(Runtime* R, bool b)
{
    Variant var;
    var.type = LONG;
    var.u.lint = b;
    push(R, var);
}

static char* tostring(Runtime* R, int idx)
{
    char* buf;
    Variant* var = stackidx(R, idx);
    switch (var->type) {
        case STRING:
            assert(var->u.str);
            return strdup(var->u.str);
        case LONG:
            buf = malloc(sizeof(char) * 20); // Let this be "enough"
            snprintf(buf, sizeof(char) * 20, "%" PRId64, var->u.lint);
            return buf;
        case UNDEFINED:
            return strdup("<UNDEFINED>");
    }

    puts("May not reach end of tostring function.");
    abort();
}

static int64_t tolong(Runtime* R, int idx)
{
    Variant* var = stackidx(R, idx);
    switch (var->type) {
        case UNDEFINED:
            return 0;
        case STRING:
            for (int i = 0; var->u.str[i] != 0; ++i) {
                if (var->u.str[i] != '0' && var->u.str[i] != ' ') {
                    return 1;
                }
            }
            return 0;
        case LONG:
            return var->u.lint;
        default:
            runtimeerror("tolong for undefined value.");
            return 0;
    }
}


static void run_echo(Runtime* R)
{
    char* str = tostring(R, -1);
    printf("%s", str);
    free(str);

    pop(R);
}

static void run_stringaddexpr(Runtime* R)
{
    char* rhs = tostring(R, -1);
    pop(R);
    char* lhs = tostring(R, -1);
    pop(R);

    char* ret = calloc((strlen(lhs) + strlen(rhs) + 1), sizeof(char));
    strcat(ret, lhs);
    strcat(ret, rhs);
    pushstr(R, ret);
    free(ret);
    free(lhs);
    free(rhs);
}

static bool compare_equal(Variant* lhs, Variant* rhs)
{
    bool result = false;
    switch (lhs->type) {
        case UNDEFINED:
            result = rhs->type == UNDEFINED;
            break;
        case STRING:
            if (rhs->type == STRING) {
                result = strcmp(lhs->u.str, rhs->u.str) == 0;
            } else if (rhs->type == LONG) {
                long long ll = strtoll(lhs->u.str, NULL, 10);
                int64_t lint = (int64_t) ll;
                assert(ll == lint);
                result = lint == rhs->u.lint;
            }
            break;
        case LONG:
            if (rhs->type == LONG) {
                result = lhs->u.lint == rhs->u.lint;
            } else {
                result = compare_equal(rhs, lhs);
            }
            break;
    }

    return result;
}


static void run_binop(Runtime* R, int op)
{
    if (op == '.') {
        return run_stringaddexpr(R);
    }

    if (op == '+' || op == '-' || op == '*' ||
        op == '/' || op == '<' || op == TK_AND || op == TK_OR) {
        int64_t rhs = tolong(R, -1);
        pop(R);
        int64_t lhs = tolong(R, -1);
        pop(R);
        int64_t result;
        switch (op) {
            case '+':
                result = lhs + rhs;
                break;
            case '-':
                result = lhs - rhs;
                break;
            case '*':
                result = lhs * rhs;
                break;
            case '/':
                result = lhs / rhs;
                break;
            case '<':
                result = lhs < rhs;
                break;
            case TK_AND:
                result = lhs && rhs;
                break;
            case TK_OR:
                result = lhs || rhs;
                break;
            default:
                assert(false);
        }
        pushlong(R, result);
        return;
    } else if (op == TK_EQ) {
        Variant* rhs = stackidx(R, -1);
        Variant* lhs = stackidx(R, -2);

        bool result = compare_equal(lhs, rhs);
        popn(R, 2);
        pushbool(R, result);
        return;
    }

    runtimeerror("Undefined BINOP");
}


static void run_assignmentexpr(Runtime* R, Function* fn)
{
    Variant* val = top(R);
    set_var(R, fn->strs[*fn->ip++], *val);
    // result of running the expr still lies on the stack.
}

static void run_function(Runtime* R, Function* fn)
{
    while ((size_t)(fn->ip - fn->code) < fn->codesize) {
        Operator op = *fn->ip++;
        int64_t lint;
        switch (op) {
            case OP_NOP:
                break;
            case OP_ECHO:
                run_echo(R);
                break;
            case OP_STR:
                pushstr(R, fn->strs[*fn->ip++]);
                break;
            case OP_LONG:
                lint = (*fn->ip++) << 4;
                lint |= (*fn->ip++);
                pushlong(R, lint);
                break;
            case OP_TRUE:
                pushbool(R, 1);
                break;
            case OP_FALSE:
                pushbool(R, 0);
                break;
            case OP_BIN:
                run_binop(R, *fn->ip++);
                break;
            case OP_ADD:
                run_binop(R, '+');
                break;
            case OP_SUB:
                run_binop(R, '-');
                break;
            case OP_MUL:
                run_binop(R, '*');
                break;
            case OP_DIV:
                run_binop(R, '/');
                break;
            case OP_ADD1:
                lint = tolong(R, -1);
                pop(R);
                pushlong(R, lint + 1);
                break;
            case OP_SUB1:
                lint = tolong(R, -1);
                pop(R);
                pushlong(R, lint - 1);
                break;
            case OP_LOOKUP:
                push(R, lookup(R, fn->strs[*fn->ip++]));
                break;
            case OP_ASSIGN:
                run_assignmentexpr(R, fn);
                break;
            case OP_JMP:
                fn->ip = fn->code + *fn->ip;
                break;
            case OP_JMPZ:
                if (tolong(R, -1) == 0) {
                    fn->ip = fn->code + *fn->ip;
                } else {
                    fn->ip++; // jump over jmpaddr
                }
                break;
            case OP_INVALID:
                runtimeerror("OP_INVALID");
                break;
            default:
                runtimeerror("Unexpected OP");
        }
    }
}

void run_file(FILE* file) {
    AST* ast = parse(file);

    Function* fn = create_function();
    compile(fn, ast);

    Runtime* R = create_runtime();
//    puts("BEFORE\n");
//    print_code(fn);
    optimize(fn);
//    puts("AFTER\n");
//    print_code(fn);
    run_function(R, fn);
    destroy_runtime(R);

    free_function(fn);

    destroy_ast(ast);
}

void print_stack(Runtime* R)
{
    printf("Stack, size: %zu\n", R->stacksize);
    puts("===========================\n");
    for (int i = 0; i < (int) R->stacksize; ++i) {
        Variant* var = stackidx(R, i);
        switch (var->type) {
            case UNDEFINED:
                printf("#%d UNDEFINED", i);
                break;
            case STRING:
                printf("#%d STRING: %s", i, var->u.str);
                break;
            case LONG:
                printf("#%d LONG: %" PRId64,i, var->u.lint);
                break;
        }
        puts("");
    }
}