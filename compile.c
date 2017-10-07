#include <stdlib.h>
#include <stdnoreturn.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <memory.h>
#include "compile.h"
#include "op_util.h"
#include "array-util.h"
#include "parse.h"

DEFINE_ENUM(Operator, ENUM_OPERATOR);

Function* create_function(char* name)
{
    Function* ret = malloc(sizeof(Function));

    ret->name = name ? name : strdup("<unnamed>");
    ret->paramlen = 0;
    ret->codesize = 0;
    ret->codecapacity = 8;
    ret->code = calloc(sizeof(*ret->code), ret->codecapacity);

    ret->strcapacity = 4;
    ret->strlen = 0;
    ret->strs = calloc(sizeof(*ret->strs), ret->strcapacity);

    ret->funcapacity = 0;
    ret->funlen = 0;
    ret->functions = NULL;

    return ret;
}

void free_function(Function* fn)
{
    free(fn->name);
    free(fn->code);

    for (int i = 0; i < fn->strlen; ++i) {
        free(fn->strs[i]);
    }
    free(fn->strs);

    for (size_t i = 0; i < fn->funlen; ++i) {
        free_function(fn->functions[i]);
    }
    free(fn->functions);

    free(fn);
}

noreturn void compiletimeerror(char* fmt, ...)
{
    va_list ap;
    char msgbuf[256];
    va_start(ap, fmt);
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
    va_end(ap);

    char buf[256];
    snprintf(buf, sizeof(buf), "Compiler error: %s.", msgbuf);

    puts(buf);

    abort();
}

static inline void try_code_resize(Function* fn)
{
    if (fn->codesize+1 >= fn->codecapacity) {
        fn->codecapacity *= 2;
        Operator* tmp = realloc(fn->code, sizeof(*fn->code) * fn->codecapacity);
        if (!tmp) compiletimeerror("Out of memory");

        fn->code = tmp;
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

static inline size_t emitraw(Function* fn, long op)
{
    if (op != (Operator) op) {
        compiletimeerror("Error encoding operator: %d", op);
        return 0;
    }

    return emit(fn, (Operator) op);
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
    emitraw(fn, fn->strlen++);
}

static void addfunction(Function* parent, Function* fn)
{
    if (!try_resize(&parent->funcapacity, parent->funlen,
                    (void**)&parent->functions, sizeof(*parent->functions), NULL)) {
        compiletimeerror("could not realloc functions");
        return;
    }

    assert(parent->funlen < parent->funcapacity);
    parent->functions[parent->funlen++] = fn;
}

static void compile_function(Function* parent, AST* ast)
{
    Function* fn = create_function(ast->val.str);
    ast->val.str = NULL;

    addfunction(parent, fn);
    compile(fn, ast->node2);
}

static void compile_blockstmt(Function* fn, AST* ast)
{
    assert(ast->type == AST_BLOCK);
    AST* current = ast->next;
    while (current) {
        compile(fn, current);
        current = current->next;
    }
}

static void compile_echostmt(Function* fn, AST* ast)
{
    assert(ast->type == AST_ECHO);
    assert(ast->node1);
    compile(fn, ast->node1);
    emit(fn, OP_ECHO);
}

static void compile_assignmentexpr(Function* fn, AST* ast)
{
    assert(ast->type == AST_ASSIGNMENT);
    assert(ast->node1);
    assert(ast->val.str);
    compile(fn, ast->node1);
    emit(fn, OP_ASSIGN);
    addstring(fn, ast->val.str);
    ast->val.str = NULL;
}

static void compile_ifstmt(Function* fn, AST* ast)
{
    assert(ast->type == AST_IF);
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
    assert(ast->type == AST_BINOP);
    assert(ast->node1 && ast->node2);
    compile(fn, ast->node1);
    compile(fn, ast->node2);
    switch (ast->val.lint) {
        case '+':
            emit(fn, OP_ADD);
            break;
        case '-':
            emit(fn, OP_SUB);
            break;
        case '/':
            emit(fn, OP_DIV);
            break;
        case '*':
            emit(fn, OP_MUL);
            break;
        case TK_SHL:
            emit(fn, OP_SHL);
            break;
        case TK_SHR:
            emit(fn, OP_SHR);
            break;
        case TK_LTEQ:
            emit(fn, OP_LTE);
            break;
        case TK_GTEQ:
            emit(fn, OP_GTE);
            break;
        default:
            emit(fn, OP_BIN);
            emitraw(fn, ast->val.lint);
            break;
    }
}

static void compile_prefixop(Function* fn, AST* ast)
{
    assert(ast->node1->type == AST_VAR);
    compile(fn, ast->node1);
    int nameidx = fn->code[fn->codesize - 1];
    assert(nameidx < fn->strlen);
    emit(fn, OP_ADD1);
    emit(fn, OP_ASSIGN);
    emitraw(fn, nameidx);
}
static void compile_postfixop(Function* fn, AST* ast)
{
    assert(ast->node1->type == AST_VAR);
    compile(fn, ast->node1);
    int nameidx = fn->code[fn->codesize - 1];
    assert(nameidx < fn->strlen);
    emit(fn, OP_ADD1);
    emit(fn, OP_ASSIGN);
    emitraw(fn, nameidx);
    emit(fn, OP_SUB1);
}

static void compile_whilestmt(Function* fn, AST* ast)
{
    assert(ast->type == AST_WHILE);
    assert(ast->node1 && ast->node2);
    size_t while_start = fn->codesize;
    compile(fn, ast->node1);
    emit(fn, OP_JMPZ); // Jump over body if zero
    size_t placeholder = emit(fn, OP_INVALID); // Placeholder
    compile(fn, ast->node2);

    emit(fn, OP_JMP); // Jump back to while start
    emitraw(fn, while_start);

    emit_replace(fn, placeholder, (Operator) emit(fn, OP_NOP));
}


static void compile_forstmt(Function* fn, AST* ast) {
    assert(ast->type == AST_FOR);
    assert(ast->node1 && ast->node2 && ast->node3 && ast->node4);
    compile(fn, ast->node1); // Init
    size_t for_start = fn->codesize;
    compile(fn, ast->node2); // Condition
    emit(fn, OP_JMPZ); // Jump over body if zero
    size_t placeholder = emit(fn, OP_INVALID); // Placeholder
    compile(fn, ast->node4); // Body
    compile(fn, ast->node3); // Post expression

    emit(fn, OP_JMP); // Jump back to for start
    emitraw(fn, for_start);

    emit_replace(fn, placeholder, (Operator) emit(fn, OP_NOP));
}

Function* compile(Function* fn, AST* ast)
{
    switch (ast->type) {
        case AST_FUNCTION:
            compile_function(fn, ast);
            break;
        case AST_RETURN:
            compile(fn, ast->node1);
            emit(fn, OP_RETURN);
            break;
        case AST_CALL:
            emit(fn, OP_STR);
            assert(ast->val.str);
            addstring(fn, ast->val.str);
            ast->val.str = NULL;
            emit(fn, OP_CALL);
            emitraw(fn, 0); // Number of parameters
            break;
        case AST_BLOCK:
            compile_blockstmt(fn, ast);
            break;
        case AST_ECHO:
            compile_echostmt(fn, ast);
            break;
        case AST_IF:
            compile_ifstmt(fn, ast);
            break;
        case AST_STRING:
            emit(fn, OP_STR);
            assert(ast->val.str);
            addstring(fn, ast->val.str);
            ast->val.str = NULL;
            break;
        case AST_BINOP:
            compile_binop(fn, ast);
            break;
        case AST_PREFIXOP:
            compile_prefixop(fn, ast);
            break;
        case AST_POSTFIXOP:
            compile_postfixop(fn, ast);
            break;
        case AST_NOTOP:
            compile(fn, ast->node1);
            emit(fn, OP_NOT);
            break;
        case AST_LONG:
            emitlong(fn, ast->val.lint);
            break;
        case AST_NULL:
            emit(fn, OP_NULL);
            break;
        case AST_VAR:
            emit(fn, OP_LOOKUP);
            assert(ast->val.str);
            addstring(fn, ast->val.str);
            ast->val.str = NULL;
            break;
        case AST_ASSIGNMENT:
            compile_assignmentexpr(fn, ast);
            break;
        case AST_HTML:
            emit(fn, OP_STR);
            assert(ast->val.str);
            addstring(fn, ast->val.str);
            ast->val.str = NULL;
            emit(fn, OP_ECHO);
            break;
        case AST_WHILE:
            compile_whilestmt(fn, ast);
            break;
        case AST_FOR:
            compile_forstmt(fn, ast);
            break;
        default:
            compiletimeerror("Unexpected Type '%s'", get_ASTTYPE_name(ast->type));
    }

    return fn;
}

void print_code(Function* fn)
{
    Operator* ip = fn->code;
    int64_t lint;
    fprintf(stderr, "Function: %s\n-----------------------\n", fn->name);
    while ((size_t)(ip - fn->code) < fn->codesize) {
        fprintf(stderr, "%02lx: ", ip - fn->code);
        Operator op = *ip;
        const char* opname = get_Operator_name(op);
        size_t chars_written = 0;
        unsigned char bytes[3] = {op, 0, 0}; // We have three bytes maximum per opcode
        if (!opname) {
            fprintf(stderr, "Unexpected op: %d", op);
        } else {
            chars_written += fprintf(stderr, "%s ", opname);
        }
        switch (*ip++) {
            case OP_STR:
                assert(*ip < fn->strlen);
                bytes[1] = *ip;
                char* escaped_string = malloc((strlen(fn->strs[*ip]) * 2 + 1) * sizeof(char));
                escaped_str(escaped_string, fn->strs[*ip++]);
                chars_written += fprintf(stderr, "\"%s\"", escaped_string);
                free(escaped_string);
                break;
            case OP_CALL:
                bytes[1] = *ip;
                chars_written += fprintf(stderr, "%d", *ip++);
                break;
            case OP_LONG:
                bytes[1] = *ip;
                lint = *ip++ << 4;
                bytes[2] = *ip;
                lint |= *ip++;
                chars_written += fprintf(stderr, "%" PRId64, lint);
                break;
            case OP_BIN:
                bytes[1] = *ip;
                chars_written += fprintf(stderr, "%s", get_token_name(*ip++));
                break;
            case OP_ASSIGN:
                bytes[1] = *ip;
                assert(*ip < fn->strlen);
                chars_written += fprintf(stderr, "$%s = pop()", fn->strs[*ip++]);
                break;
            case OP_LOOKUP:
                bytes[1] = *ip;
                assert(*ip < fn->strlen);
                chars_written += fprintf(stderr, "$%s", fn->strs[*ip++]);
                break;
            case OP_JMP:
            case OP_JMPZ:
                bytes[1] = *ip;
                chars_written += fprintf(stderr, ":%02lx", (unsigned long)*ip++);
                break;

            default:
                break;
        }
        for (size_t i = 0; i < 40 - chars_written; ++i) {
            fputc(' ', stderr);
        }
        fputc(';', stderr);
        for (size_t i = 0; i < op_len(op); ++i) {
            fprintf(stderr, " %02x", bytes[i]);
        }
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "\n\n");

    for (size_t i = 0; i < fn->funlen; ++i) {
        print_code(fn->functions[i]);
    }
}
