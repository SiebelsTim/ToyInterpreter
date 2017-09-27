#include <stdlib.h>
#include <stdnoreturn.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <memory.h>
#include "compile.h"
#include "op_util.h"

DEFINE_ENUM(Operator, ENUM_OPERATOR);

Function* create_function()
{
    Function* ret = malloc(sizeof(Function));

    ret->name = "<unnamed>";
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
    assert(ast->node1->type == VAREXPR);
    compile(fn, ast->node1);
    int nameidx = fn->code[fn->codesize - 1];
    assert(nameidx < fn->strlen);
    emit(fn, OP_ADD1);
    emit(fn, OP_ASSIGN);
    emitraw(fn, nameidx);
}
static void compile_postfixop(Function* fn, AST* ast)
{
    assert(ast->node1->type == VAREXPR);
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
    assert(ast->type == WHILESTMT);
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
    emitraw(fn, for_start);

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
        case PREFIXOP:
            compile_prefixop(fn, ast);
            break;
        case POSTFIXOP:
            compile_postfixop(fn, ast);
            break;
        case NOTOP:
            compile(fn, ast->node1);
            emit(fn, OP_NOT);
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
            compiletimeerror("Unexpected Type '%s'", get_ASTTYPE_name(ast->type));
    }

    return fn;
}

static char escape_chars[] = {
    '0', 0, 0, 0, 0, 0, 0, 0, // 07
    0, 't', 'n', 0, 0, 'r', 0, 0, // 017
    0, 0, 0, 0, 0, 0, 0, 0, // 027
    0, 0, 0, 0, 0, 0, 0
};

static char* escaped_str(const char* str)
{
    _Static_assert(arrcount(escape_chars) == 0x1f, "Not enough escape chars");
    size_t pos = 0;
    char* ret = malloc((strlen(str) * 2 + 1) * sizeof(char));
    while (*str != '\0') {
        if (*str < 0x20 && escape_chars[(unsigned char)*str] != 0) { // First 0x20 chars are special chars
            ret[pos++] = '\\';
            ret[pos++] = escape_chars[(unsigned char)*str];
        } else {
            ret[pos++] = *str;
        }
        str++;
    }
    ret[pos++] = '\0';
    return ret;
}

void print_code(Function* fn)
{
    Operator* prev_ip = fn->ip;
    fn->ip = fn->code;
    int64_t lint;
    fprintf(stderr, "Function: %s\n-----------------------\n", fn->name);
    while ((size_t)(fn->ip - fn->code) < fn->codesize) {
        fprintf(stderr, "%02lx: ", fn->ip - fn->code);
        Operator op = *fn->ip;
        const char* opname = get_Operator_name(op);
        size_t chars_written = 0;
        unsigned char bytes[3] = {op, 0, 0}; // We have three bytes maximum per opcode
        if (!opname) {
            fprintf(stderr, "Unexpected op: %d", op);
        } else {
            chars_written += fprintf(stderr, "%s ", opname);
        }
        switch (*fn->ip++) {
            case OP_STR:
                assert(*fn->ip < fn->strlen);
                bytes[1] = *fn->ip;
                char* escaped_string = escaped_str(fn->strs[*fn->ip++]);
                chars_written += fprintf(stderr, "\"%s\"", escaped_string);
                free(escaped_string);
                break;
            case OP_LONG:
                bytes[1] = *fn->ip;
                lint = *fn->ip++ << 4;
                bytes[2] = *fn->ip;
                lint |= *fn->ip++;
                chars_written += fprintf(stderr, "%" PRId64, lint);
                break;
            case OP_BIN:
                bytes[1] = *fn->ip;
                chars_written += fprintf(stderr, "%s", get_token_name(*fn->ip++));
                break;
            case OP_ASSIGN:
                bytes[1] = *fn->ip;
                assert(*fn->ip < fn->strlen);
                chars_written += fprintf(stderr, "$%s = pop()", fn->strs[*fn->ip++]);
                break;
            case OP_LOOKUP:
                bytes[1] = *fn->ip;
                assert(*fn->ip < fn->strlen);
                chars_written += fprintf(stderr, "$%s", fn->strs[*fn->ip++]);
                break;
            case OP_JMP:
            case OP_JMPZ:
                bytes[1] = *fn->ip;
                chars_written += fprintf(stderr, ":%02lx", (unsigned long)*fn->ip++);
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
    fn->ip = prev_ip;
}
