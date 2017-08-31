#include <stdlib.h>
#include <stdnoreturn.h>
#include <memory.h>
#include <assert.h>
#include <tgmath.h>
#include <stdbool.h>
#include "run.h"
#include "parse.h"
#include "lex.h"


noreturn static void runtimeerror(char* fmt)
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
    free(R);
}

static inline void try_stack_resize(Runtime* R)
{
    if (R->stackcapacity < R->stacksize+1) {
        R->stackcapacity *= 2;
        Variant* tmp = realloc(R->stack, sizeof(*R->stack) * R->stackcapacity);
        if (!tmp) runtimeerror("Out of memory");

        memset(&tmp[R->stacksize], 0, R->stackcapacity - R->stacksize);
        R->stack = tmp;
    }
}

static void push(Runtime* R, Variant val)
{
    try_stack_resize(R);
    R->stack[R->stacksize++] = val;
}

static inline Variant* top(Runtime* R)
{
    assert(R->stacksize > 0);
    return stackidx(R, -1);
}

static inline void pop(Runtime* R) {
    Variant *ret = top(R);

    if (ret->type == STRING) {
        free(ret->u.str);
    }

    R->stacksize--;
    assert(R->stacksize >= 0);
}

static inline void pushstr(Runtime* R, char* str)
{
    Variant var;
    var.type = STRING;
    var.u.str = strdup(str);
    push(R, var);
}


static inline void pushlong(Runtime* R, long n)
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
            snprintf(buf, sizeof(char) * 20, "%ld", var->u.lint);
            return buf;
        case UNDEFINED:
            return strdup("<UNDEFINED>");
    }

    puts("May not reach end of tostring function.");
    abort();
}

static long tolong(Runtime* R, int idx)
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
    }
}

static void run(Runtime* R, AST* ast);

static void run_blockstmt(Runtime* R, AST* ast)
{
    while (ast->next) {
        run(R, ast->next);
        ast = ast->next;
    }
}

static void run_echostmt(Runtime* R, AST* ast)
{
    assert(ast->left);
    run(R, ast->left);
    char* str = tostring(R, -1);
    printf("%s", str);
    free(str);

    pop(R);
}

static void run_ifstmt(Runtime* R, AST* ast)
{
    assert(ast->left);
    run(R, ast->left);
    long boolean = tolong(R, -1);
    pop(R);
    if (boolean) {
        run(R, ast->right);
    } else if (ast->extra) {
        run(R, ast->extra);
    }
}

static void run_stringaddexpr(Runtime* R, AST* ast)
{
    assert(ast->left && ast->right);
    run(R, ast->left);
    char* lhs = tostring(R, -1);
    pop(R);
    run(R, ast->right);
    char* rhs = tostring(R, -1);
    pop(R);

    char* ret = calloc((strlen(lhs) + strlen(rhs) + 1), sizeof(char));
    strcat(ret, lhs);
    strcat(ret, rhs);
    pushstr(R, ret);
    free(ret);
    free(lhs);
    free(rhs);
}

static void run_binop(Runtime* R, AST* ast)
{
    if (ast->val.lint == '.') {
        return run_stringaddexpr(R, ast);
    }

    if (ast->val.lint == '+' || ast->val.lint == '-' || ast->val.lint == '*' ||
        ast->val.lint == '/') {
        run(R, ast->left);
        long lhs = tolong(R, -1);
        pop(R);
        run(R, ast->right);
        long rhs = tolong(R, -1);
        pop(R);
        long result;
        switch (ast->val.lint) {
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
            default:
                assert(false);
        }
        pushlong(R, result);
        return;
    }



    runtimeerror("Undefined BINOP");
}

static void run_stringexpr(Runtime* R, AST* ast)
{
    pushstr(R, ast->val.str);
}

static void run_longexpr(Runtime* R, AST* ast)
{
    pushlong(R, ast->val.lint);
}

static void run(Runtime* R, AST* ast)
{
    switch (ast->type) {
        case BLOCKSTMT:
            run_blockstmt(R, ast);
            break;
        case ECHOSTMT:
            run_echostmt(R, ast);
            break;
        case IFSTMT:
            run_ifstmt(R, ast);
            break;
        case STRINGEXPR:
            run_stringexpr(R, ast);
            break;
        case BINOP:
            run_binop(R, ast);
            break;
        case LONGEXPR:
            run_longexpr(R, ast);
            break;
        default:
            runtimeerror("Unexpected AST Type");
            break;
    }
}

void run_file(FILE* file) {
    AST* ast = parse(file);

    Runtime* R = create_runtime();
    run(R, ast);
    destroy_runtime(R);

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
                printf("#%d LONG: %ld",i, var->u.lint);
                break;
        }
        puts("");
    }
}