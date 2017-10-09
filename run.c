#include <stdlib.h>
#include <stdnoreturn.h>
#include <memory.h>
#include <assert.h>
#include <stdbool.h>
#include <lzma.h>
#include "run.h"
#include "scope.h"
#include "array-util.h"

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

noreturn void runtimeerror(char* fmt)
{
    printf("Runtime Error: ");
    puts(fmt);
    abort();
}

noreturn void raise_fatal(Runtime* R, char* fmt, ...)
{
    printf("Fatal Error: ");
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf(" in %s:%zu\n", R->file, R->line);
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

        if (ret->type == TYPE_STRING) {
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
    var.type = TYPE_STRING;
    var.u.str = str;
    push(R, var);
}


static inline void pushlong(Runtime* R, int64_t n)
{
    Variant var;
    var.type = TYPE_LONG;
    var.u.lint = n;
    push(R, var);
}

static inline void pushbool(Runtime* R, bool b)
{
    Variant var;
    var.type = TYPE_LONG;
    var.u.lint = b;
    push(R, var);
}

static inline void pushnull(Runtime* R)
{
    Variant var;
    var.type = TYPE_NULL;
    push(R, var);
}

static char* tostring(Runtime* R, int idx)
{
    char* buf;
    Variant* var = stackidx(R, idx);
    switch (var->type) {
        case TYPE_STRING:
            assert(var->u.str);
            return strdup(var->u.str);
        case TYPE_LONG:
            buf = malloc(sizeof(char) * 20); // Let this be "enough"
            snprintf(buf, sizeof(char) * 20, "%" PRId64, var->u.lint);
            return buf;
        case TYPE_UNDEF:
            return strdup("<UNDEFINED>");
        case TYPE_NULL:
            return strdup("<null>");
    }

    runtimeerror("Assertion failed: May not reach end of tostring function.");
    return NULL;
}

static int64_t tolong(Runtime* R, int idx)
{
    Variant* var = stackidx(R, idx);
    switch (var->type) {
        case TYPE_UNDEF:
            return 0;
        case TYPE_STRING:
            for (int i = 0; var->u.str[i] != 0; ++i) {
                if (var->u.str[i] != '0' && var->u.str[i] != ' ') {
                    return 1;
                }
            }
            return 0;
        case TYPE_LONG:
            return var->u.lint;
        default:
            runtimeerror("tolong for undefined value.");
            return 0;
    }
}

static Function* find_function(Function* parent, const char* name)
{
    for (size_t i = 0; i < parent->funlen; ++i) {
        if (strcmp(parent->functions[i]->name, name) == 0) {
            return parent->functions[i];
        }
    }

    return NULL;
}


static void run_call(Runtime* R, Function* fn)
{
    const char* fnname = tostring(R, -1);
    pop(R);

    Function* callee = find_function(fn, fnname);
    if (!callee) {
        raise_fatal(R, "Call to undefined function %s()", fnname);
        free((void*)fnname);
        return;
    }
    free((void*)fnname);
    const uint8_t param_count = fetch8(R->ip++);
    if (param_count != callee->paramlen) {
        raise_fatal(R, "Parameter number mismatch. %u expected, %u given",
                    callee->paramlen, param_count);
        return;
    }

    Runtime* newruntime = create_runtime();
    for (int i = 0; i < param_count; ++i) {
        set_var(newruntime, callee->params[i], *top(R)); // Transfer arguments
        pop(R);
    }
    run_function(newruntime, callee);
    push(R, *top(newruntime)); // Return variable
    destroy_runtime(newruntime);
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
        case TYPE_UNDEF:
            result = rhs->type == TYPE_UNDEF;
            break;
        case TYPE_NULL:
            result = rhs->type == TYPE_NULL;
            break;
        case TYPE_STRING:
            if (rhs->type == TYPE_STRING) {
                result = strcmp(lhs->u.str, rhs->u.str) == 0;
            } else if (rhs->type == TYPE_LONG) {
                long long ll = strtoll(lhs->u.str, NULL, 10);
                int64_t lint = (int64_t) ll;
                assert(ll == lint);
                result = lint == rhs->u.lint;
            }
            break;
        case TYPE_LONG:
            if (rhs->type == TYPE_LONG) {
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
        switch (op) {
            case OP_NOP:
                break;
            case OP_RETURN:
                return; // Finish executing Function
            case OP_CALL:
                run_call(R, fn);
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
            case OP_BIN:
                run_binop(R, fetch16(R->ip));
                R->ip += 2;
                break;
            case OP_LTE:
                run_binop(R, TK_LTEQ);
                break;
            case OP_GTE:
                run_binop(R, TK_GTEQ);
                break;
            case OP_NOT:
                run_notop(R);
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
            case OP_INVALID:
                runtimeerror("OP_INVALID");
                break;
            default:
                runtimeerror("Unexpected OP");
        }
    }
}

void run_file(const char* filepath) {
    FILE* handle = fopen(filepath, "r");
    AST* ast = parse(handle);

    Function* fn = create_function(strdup("<pseudomain>"));
    compile(fn, ast);

    Runtime* R = create_runtime();
    R->file = strdup(filepath);
    print_code(fn);
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
            case TYPE_UNDEF:
                printf("#%d UNDEFINED", i);
                break;
            case TYPE_STRING:
                printf("#%d STRING: %s", i, var->u.str);
                break;
            case TYPE_LONG:
                printf("#%d LONG: %" PRId64,i, var->u.lint);
                break;
            case TYPE_NULL:
                printf("#%d NULL", i);
                break;
        }
        puts("");
    }
}