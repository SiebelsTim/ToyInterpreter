#include <stdlib.h>
#include <stdnoreturn.h>
#include <memory.h>
#include <assert.h>
#include "run.h"
#include "parse.h"


noreturn static void runtimeerror(char* fmt)
{
    puts(fmt);
    abort();
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
    return &R->stack[R->stacksize-1];
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
    Variant* val = top(R);
    assert(val->type == STRING);
    puts(val->u.str);
    pop(R);
}

static void run_stringaddexpr(Runtime* R, AST* ast)
{
    assert(ast->left && ast->right);
    run(R, ast->left);
    char* lhs = strdup(top(R)->u.str);
    pop(R);
    run(R, ast->right);
    char* rhs = strdup(top(R)->u.str);
    pop(R);

    char* ret = calloc((strlen(lhs) + strlen(rhs) + 1), sizeof(char));
    strcat(ret, lhs);
    strcat(ret, rhs);
    pushstr(R, ret);
    free(ret);
    free(lhs);
    free(rhs);
}

static void run_stringexpr(Runtime* R, AST* ast)
{
    pushstr(R, ast->val);
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
        case STRINGBINOP:
            run_stringaddexpr(R, ast);
            break;
        case STRINGEXPR:
            run_stringexpr(R, ast);
            break;
    }
}

void run_file(FILE* file) {
    AST* ast = parse(file);
    print_ast(ast, 0);
    puts("EXECUTING \n\n");

    Runtime* R = create_runtime();
    run(R, ast);
    destroy_runtime(R);

    destroy_ast(ast);
}
