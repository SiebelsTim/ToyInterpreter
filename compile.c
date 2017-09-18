#include <stdlib.h>
#include <stdnoreturn.h>
#include <assert.h>
#include "compile.h"
#include "parse.h"

Function* create_function()
{
    Function* ret = malloc(sizeof(Function));

    ret->codesize = 0;
    ret->codecapacity = 8;
    ret->code = calloc(sizeof(*ret->code), ret->codecapacity);
    ret->ip = ret->code;

    ret->strcapacity = 4;
    ret->strlen = 0;
    ret->strs = calloc(sizeof(*ret->strs), ret->strcapacity);

    return ret;
}

void free_function(Function* fn)
{
    free(fn->code);

    for (int i = 0; i < fn->strlen; ++i) {
        free(fn->strs[i]);
    }

    free(fn->strs);
    free(fn);
}

noreturn void compiletimeerror(char* fmt, ...)
{
    va_list ap;
    char msgbuf[256];
    va_start(ap, fmt);
    vsnprintf(msgbuf, arrcount(msgbuf), fmt, ap);
    va_end(ap);

    char buf[256];
    snprintf(buf, arrcount(buf), "Parse error: %s.", msgbuf);

    puts(buf);

    abort();
}

static inline void try_code_resize(Function* fn)
{
    if (fn->codesize+1 >= fn->codecapacity) {
        fn->codecapacity *= 2;
        Operator* tmp = realloc(fn->code, sizeof(*fn->code) * fn->codecapacity);
        if (!tmp) compiletimeerror("Out of memory");

        size_t diff = (fn->ip - fn->code); // adjust IP
        fn->code = tmp;
        fn->ip = fn->code + diff;
    }
}

static inline void try_strs_resize(Function* fn)
{
    if (fn->strlen+1 >= fn->strcapacity) {
        fn->strcapacity *= 2;
        char** tmp = realloc(fn->strs, sizeof(*fn->strs) * fn->strcapacity);
        if (!tmp) compiletimeerror("Out of memory");

        fn->strs = tmp;
    }
}

// Returns position of inserted op
static inline size_t emit(Function* fn, Operator op)
{
    try_code_resize(fn);
    fn->code[fn->codesize] = op;

    return fn->codesize++;
}

static inline size_t emit_replace(Function* fn, size_t position, Operator op)
{
    assert(position < fn->codesize);
    fn->code[position] = op;

    return position;
}

static void emitlong(Function* fn, int64_t lint)
{
    emit(fn, OP_LONG);
    _Static_assert(sizeof(Operator) == sizeof(lint)/2, "Long is not twice as long as Operator");
    _Static_assert(sizeof(lint) == 8, "Long is not 8 bytes.");
    emit(fn, (Operator) ((lint & 0xffffffff00000000) >> 4));
    emit(fn, (Operator) (lint & 0xffffffff));
}

static void addstring(Function* fn, char* str)
{
    try_strs_resize(fn);
    fn->strs[fn->strlen] = str;
    if (fn->strlen != (Operator) fn->strlen) {
        compiletimeerror("Overflow into OP encoding");
    }
    emit(fn, (Operator) fn->strlen++);
}

static void compile_blockstmt(Function* fn, AST* ast)
{
    assert(ast->type == BLOCKSTMT);
    AST* current = ast->next;
    while (current) {
        compile(fn, current);
        current = current->next;
    }
}

static void compile_echostmt(Function* fn, AST* ast)
{
    assert(ast->type == ECHOSTMT);
    assert(ast->node1);
    compile(fn, ast->node1);
    emit(fn, OP_ECHO);
}

static void compile_assignmentexpr(Function* fn, AST* ast)
{
    assert(ast->type == ASSIGNMENTEXPR);
    assert(ast->node1);
    assert(ast->val.str);
    compile(fn, ast->node1);
    emit(fn, OP_ASSIGN);
    addstring(fn, ast->val.str);
    ast->val.str = NULL;
}

static void compile_ifstmt(Function* fn, AST* ast)
{
    assert(ast->type == IFSTMT);
    assert(ast->node1 && ast->node2);
    compile(fn, ast->node1);
    emit(fn, OP_JMPZ); // Jump over code if false
    size_t placeholder = emit(fn, OP_INVALID); // Placeholder
    compile(fn, ast->node2);
    emit_replace(fn, placeholder, (Operator) emit(fn, OP_NOP)); // place to jump over if
    assert((Operator)fn->codesize == fn->codesize);

    if (ast->node3) {
        emit(fn, OP_JMP);
        size_t else_placeholder = emit(fn, OP_INVALID); // placeholder
        // When we get here, we already assigned a jmp to here for the else branch
        // However, we need to increase it by one two jmp over this jmp
        emit_replace(fn, placeholder, (Operator) fn->codesize);

        compile(fn, ast->node3);
        emit_replace(fn, else_placeholder, (Operator) emit(fn, OP_NOP));
    }
}

static void compile_binop(Function* fn, AST* ast)
{
    assert(ast->type == BINOP);
    assert(ast->node1 && ast->node2);
    compile(fn, ast->node1);
    compile(fn, ast->node2);
    emit(fn, OP_BIN);
    emit(fn, (Operator) ast->val.lint);
}

static void compile_whilestmt(Function* fn, AST* ast)
{
    assert(ast->type == WHILESTMT);
    assert(ast->node1 && ast->node2);
    size_t while_start = fn->codesize;
    compile(fn, ast->node1);
    emit(fn, OP_JMPZ); // Jump over body if zero
    size_t placeholder = emit(fn, OP_INVALID); // Placeholder
    compile(fn, ast->node2);

    emit(fn, OP_JMP); // Jump back to while start
    emit(fn, (Operator) while_start);

    emit_replace(fn, placeholder, (Operator) emit(fn, OP_NOP));
}


static void compile_forstmt(Function* fn, AST* ast) {
    assert(ast->type == FORSTMT);
    assert(ast->node1 && ast->node2 && ast->node3 && ast->node4);
    compile(fn, ast->node1); // Init
    size_t for_start = fn->codesize;
    compile(fn, ast->node2); // Condition
    emit(fn, OP_JMPZ); // Jump over body if zero
    size_t placeholder = emit(fn, OP_INVALID); // Placeholder
    compile(fn, ast->node4); // Body
    compile(fn, ast->node3); // Post expression

    emit(fn, OP_JMP); // Jump back to for start
    emit(fn, (Operator) for_start);

    emit_replace(fn, placeholder, (Operator) emit(fn, OP_NOP));
}

Function* compile(Function* fn, AST* ast)
{
    switch (ast->type) {
        case BLOCKSTMT:
            compile_blockstmt(fn, ast);
            break;
        case ECHOSTMT:
            compile_echostmt(fn, ast);
            break;
        case IFSTMT:
            compile_ifstmt(fn, ast);
            break;
        case STRINGEXPR:
            emit(fn, OP_STR);
            assert(ast->val.str);
            addstring(fn, ast->val.str);
            ast->val.str = NULL;
            break;
        case BINOP:
            compile_binop(fn, ast);
            break;
        case LONGEXPR:
            emitlong(fn, ast->val.lint);
            break;
        case VAREXPR:
            emit(fn, OP_LOOKUP);
            assert(ast->val.str);
            addstring(fn, ast->val.str);
            ast->val.str = NULL;
            break;
        case ASSIGNMENTEXPR:
            compile_assignmentexpr(fn, ast);
            break;
        case HTMLEXPR:
            emit(fn, OP_STR);
            assert(ast->val.str);
            addstring(fn, ast->val.str);
            ast->val.str = NULL;
            emit(fn, OP_ECHO);
            break;
        case WHILESTMT:
            compile_whilestmt(fn, ast);
            break;
        case FORSTMT:
            compile_forstmt(fn, ast);
            break;
        default:
            compiletimeerror("Unexpected Type '%s'", get_ast_typename(ast->type));
    }

    return fn;
}


static char* opmap[] = {
        "OP_INVALID",
        "OP_ECHO",
        "OP_STR",
        "OP_LONG",
        "OP_TRUE",
        "OP_FALSE",
        "OP_BIN", 
        "OP_ASSIGN",
        "OP_LOOKUP",
        "OP_JMP",
        "OP_JMPZ",
        "OP_NOP"
};

char* get_op_name(Operator op)
{
    if (op < sizeof(opmap)/sizeof(*opmap)) {
        return opmap[op];
    }

    return NULL;
}

void print_code(Function* fn)
{
    int64_t lint;
    while ((size_t)(fn->ip - fn->code) < fn->codesize) {
        printf("%lu: ", fn->ip - fn->code);
        char* opname = get_op_name(*fn->ip);
        if (!opname) {
            printf("Unexpected op: %d", *fn->ip);
        } else {
            printf("%s ", opname);
        }
        switch (*fn->ip++) {
            case OP_STR:
                printf("\"%s\"", fn->strs[*fn->ip++]);
                break;
            case OP_LONG:
                lint = *fn->ip++ << 4;
                lint |= *fn->ip++;
                printf("%ld", lint);
                break;
            case OP_BIN:
                printf("%s", get_token_name(*fn->ip++));
                break;
            case OP_ASSIGN:
                printf("$%s = pop()", fn->strs[*fn->ip++]);
                break;
            case OP_LOOKUP:
                printf("$%s", fn->strs[*fn->ip++]);
                break;
            case OP_JMP:
            case OP_JMPZ:
                printf(":%d", *fn->ip++);
                break;

            default:
                break;
        }
        puts("");
    }

    fn->ip = fn->code;
}
