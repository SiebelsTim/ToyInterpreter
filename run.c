#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include "crossplatform/std.h"
#include "crossplatform/endian.h"
#include "run.h"
#include "scope.h"
#include "util.h"
#include "builtins/std.h"
#include "compile.h"


Variant cpy_var(Variant var)
{
    Variant ret = var;
    if (ret.type == TYPE_STRING) {
        ret.u.str = strdup(var.u.str);
    }

    return ret;
}

void free_var(Variant var)
{
    if (var.type == TYPE_STRING) {
        free(var.u.str);
    }
}

_Noreturn void runtimeerror(char* fmt)
{
    printf("Runtime Error: ");
    puts(fmt);
    abort();
}

_Noreturn void raise_fatal(Runtime* R, char* fmt, ...)
{
    printf("Fatal Error: ");
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf(" in %s:%zu\n", R->file, R->line);
    abort();
}


static Runtime* create_runtime(State* S)
{
    Runtime* ret = malloc(sizeof(Runtime));

    ret->stackcapacity = 10;
    ret->stacksize = 0;
    ret->stack = calloc(ret->stackcapacity, sizeof(*ret->stack));
    ret->scope = create_scope();
    ret->state = S;
    ret->line = 0;
    ret->file = NULL;

    return ret;
}

static void destroy_runtime(Runtime* R)
{
    for (int i = 0; i < (int) R->stacksize; ++i) {
        if (stackidx(R, i)->type == TYPE_STRING) {
            free(stackidx(R, i)->u.str);
        }
    }
    free(R->stack);
    destroy_scope(R->scope);
    free(R->file);
    free(R);
}

static FunctionWrapper* find_function(State* S, const char* name)
{
    for (size_t i = 0; i < S->funlen; ++i) {
        if (strcmp(S->functions[i].name, name) == 0) {
            return &S->functions[i];
        }
    }

    return NULL;
}


static void run_call(Runtime* R)
{
    const char* fnname = tostring(R, -1);
    pop(R);

    FunctionWrapper* callee = find_function(R->state, fnname);
    if (!callee) {
        raise_fatal(R, "Call to undefined function %s()", fnname);
        free((void*)fnname);
        return;
    }
    free((void*)fnname);
    const uint8_t param_count = fetch8(R->ip++);

    if (callee->type == FUNCTION) {
        if (param_count != callee->u.function->paramlen) {
            raise_fatal(R, "Parameter number mismatch. %u expected, %u given",
                        callee->u.function->paramlen, param_count);
            return;
        }

        Runtime* newruntime = create_runtime(R->state);
        for (int i = 0; i < param_count; ++i) {
            set_var(newruntime, callee->u.function->params[i], *top(R)); // Transfer arguments
            pop(R);
        }
        run_function(newruntime, callee->u.function);
        push(R, *top(newruntime)); // Return variable
        destroy_runtime(newruntime);
    } else {
        Runtime* newruntime = create_runtime(R->state);
        for (int i = 0; i < param_count; ++i) {
            push(newruntime, *top(R)); // Transfer arguments
            pop(R);
        }
        callee->u.cfunction(newruntime);
        push(R, *top(newruntime)); // Return variable
        destroy_runtime(newruntime);
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

// https://github.com/php/php-langspec/blob/1dc4793ede53b12a2b193698d9ef44709bcf9b10/spec/10-expressions.md#relational-operators
static bool compare_equal(Variant lhs, Variant rhs)
{
    switch (lhs.type) {
        case TYPE_UNDEF:
            return rhs.type == TYPE_UNDEF;
        case TYPE_NULL:
            if (lhs.type == rhs.type) {
                return true;
            }

            return compare_equal(vartotype(lhs, rhs.type), rhs);
        case TYPE_STRING:
            if (rhs.type == TYPE_STRING) {
                return strcmp(lhs.u.str, rhs.u.str) == 0;
            }
            if (rhs.type == TYPE_NULL) {
                return compare_equal(lhs, vartotype(rhs, TYPE_STRING));
            } else {
                return compare_equal(vartotype(lhs, rhs.type), rhs);
            }
        case TYPE_LONG:
            if (rhs.type == TYPE_LONG) {
                return lhs.u.lint == rhs.u.lint;
            }
            if (rhs.type == TYPE_STRING || rhs.type == TYPE_NULL) {
                return compare_equal(lhs, vartotype(rhs, TYPE_LONG));
            } else {
                return compare_equal(vartotype(lhs, rhs.type), rhs);
            }
        case TYPE_BOOL:
            if (rhs.type == TYPE_BOOL) {
                return lhs.u.boolean == rhs.u.boolean;
            }
            return compare_equal(lhs, vartotype(rhs, TYPE_BOOL));
        case TYPE_CFUNCTION:
            return rhs.type == TYPE_CFUNCTION && rhs.u.cfunction == lhs.u.cfunction;
        case TYPE_FUNCTION:
            return rhs.type == TYPE_FUNCTION && rhs.u.function == lhs.u.function;
        case TYPE_MAX_VALUE:
            assert(false && "Undefined Type given");
            break;
    }

    assert(false);
    return false;
}


static void run_binop(Runtime* R, int op)
{
    if (op == '+' || op == '-' || op == '*' || op == '/' || op == '<' ||
        op == TK_AND || op == TK_OR || op == TK_SHL || op == TK_SHR ||
        op == TK_LTEQ || op == TK_GTEQ) {
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
            case TK_SHL:
                result = lhs << rhs;
                break;
            case TK_SHR:
                result = lhs >> rhs;
                break;
            case TK_LTEQ:
                result = lhs <= rhs;
                break;
            case TK_GTEQ:
                result = lhs >= rhs;
                break;
            default:
                assert(false);
                return;
        }
        pushlong(R, result);
        return;
    } else if (op == TK_EQ) {
        Variant* rhs = stackidx(R, -1);
        Variant* lhs = stackidx(R, -2);

        bool result = compare_equal(*lhs, *rhs);
        popn(R, 2);
        pushbool(R, result);
        return;
    }

    runtimeerror("Undefined AST_BINOP");
}

static void run_notop(Runtime* R)
{
    int64_t lint = tolong(R, -1);
    pop(R);
    pushlong(R, !lint);
}

static void run_assignmentexpr(Runtime* R, Function* fn)
{
    Variant* val = top(R);
    set_var(R, fn->strs[fetch16(R->ip)], *val);
    R->ip += 2;
    // result of running the expr still lies on the stack.
}

void run_function(Runtime* R, Function* fn)
{
    R->ip = fn->code;
    while ((size_t)(R->ip - fn->code) < fn->codesize) {
        Operator op = (Operator) *R->ip++;
        int64_t lint;
        Variant var;
        switch (op) {
            case OP_NOP:
                break;
            case OP_RETURN:
                return; // Finish executing Function
            case OP_CALL:
                run_call(R);
                break;
            case OP_ECHO:
                run_echo(R);
                break;
            case OP_STR:
                pushstr(R, fn->strs[fetch16(R->ip)]);
                R->ip += 2;
                break;
            case OP_LONG:
                lint = (int64_t) fetch64(R->ip);
                R->ip += 8;
                pushlong(R, lint);
                break;
            case OP_TRUE:
                pushbool(R, 1);
                break;
            case OP_FALSE:
                pushbool(R, 0);
                break;
            case OP_NULL:
                pushnull(R);
                break;
            case OP_LTE:
                run_binop(R, TK_LTEQ);
                break;
            case OP_GTE:
                run_binop(R, TK_GTEQ);
                break;
            case OP_LT:
                run_binop(R, '<');
                break;
            case OP_GT:
                run_binop(R, '>');
                break;
            case OP_NOT:
                run_notop(R);
                break;
            case OP_AND:
                run_binop(R, TK_AND);
                break;
            case OP_OR:
                run_binop(R, TK_OR);
                break;
            case OP_EQ:
                run_binop(R, TK_EQ);
                break;
            case OP_CONCAT:
                run_stringaddexpr(R);
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
            case OP_SHL:
                run_binop(R, TK_SHL);
                break;
            case OP_SHR:
                run_binop(R, TK_SHR);
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
                push(R, lookup(R, fn->strs[fetch16(R->ip)]));
                R->ip += 2;
                break;
            case OP_ASSIGN:
                run_assignmentexpr(R, fn);
                break;
            case OP_JMP:
                R->ip = fn->code + fetch32(R->ip);
                break;
            case OP_JMPZ:
                if (tolong(R, -1) == 0) {
                    R->ip = fn->code + fetch32(R->ip);
                } else {
                    R->ip += 4; // jump over jmpaddr
                }
                break;
            case OP_CAST:
                var = vartotype(*stackidx(R, -1), (VARIANTTYPE) fetch8(R->ip++));
                pop(R);
                push(R, var);
                free_var(var);
                break;
            case OP_INVALID:
                runtimeerror("OP_INVALID");
                break;
            default:
                runtimeerror("Unexpected OP");
        }
    }
}

static void init_builtin_functions(State* S)
{
    for (size_t i = 0; i < arrcount(std_functions); ++i) {
        CFunctionPair pair = std_functions[i];
        addfunction(S, wrap_cfunction(pair.cfunction, strdup(pair.name)));
    }
}

void run_file(const char* filepath) {
    FILE* handle = fopen(filepath, "r");
    AST* ast = parse(handle);

    Function* fn = create_function();
    State* S = create_state();
    addfunction(S, wrap_function(fn, strdup("<pseudomain>")));
    init_builtin_functions(S);
    compile(S, fn, ast);

    Runtime* R = create_runtime(S);
    R->file = strdup(filepath);
    //print_code(fn, "<pseudomain>");
    run_function(R, fn);
    destroy_state(S);
    destroy_runtime(R);

    destroy_ast(ast);
}

void print_stack(Runtime* R)
{
    printf("Stack, size: %zu\n", R->stacksize);
    puts("===========================\n");
    for (int i = 0; i < (int) R->stacksize; ++i) {
        Variant* var = stackidx(R, i);
        printf("#%d ", i);
        switch (var->type) {
            case TYPE_UNDEF:
                printf("UNDEFINED");
                break;
            case TYPE_STRING:
                printf("STRING: %s", var->u.str);
                break;
            case TYPE_LONG:
                printf("LONG: %" PRId64, var->u.lint);
                break;
            case TYPE_NULL:
                printf("NULL");
                break;
            case TYPE_BOOL:
                printf(var->u.boolean ? "TRUE" : "FALSE");
                break;
            case TYPE_FUNCTION:
            case TYPE_CFUNCTION:
                printf("FUNCTION");
                break;
            case TYPE_MAX_VALUE:
                assert(false && "Undefined Type given");
                break;
        }
        puts("");
    }
}
