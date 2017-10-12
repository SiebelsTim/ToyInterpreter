#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <memory.h>
#include "compile.h"
#include "op_util.h"
#include "array-util.h"
#include "run.h"

DEFINE_ENUM(Operator, ENUM_OPERATOR);

Function* create_function()
{
    Function* ret = malloc(sizeof(Function));

    ret->paramlen = 0;
    ret->params = NULL;
    ret->lineno_defined = 0;

    ret->codesize = 0;
    ret->codecapacity = 1 << 8;
    ret->code = calloc(sizeof(*ret->code), ret->codecapacity);

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


    for (size_t i = 0; i < fn->paramlen; ++i) {
        free(fn->params[i]);
    }
    free(fn->params);

    free(fn);
}

FunctionWrapper wrap_function(Function* fn, char* name)
{
    FunctionWrapper ret;
    ret.type = FUNCTION;
    ret.name = name;
    ret.u.function = fn;

    return ret;
}

FunctionWrapper wrap_cfunction(CFunction* fn, char* name)
{
    FunctionWrapper ret;
    ret.type = CFUNCTION;
    ret.name = name;
    ret.u.cfunction = fn;

    return ret;
}

State* create_state()
{
    State* ret = malloc(sizeof(*ret));

    ret->funlen = 0;
    ret->funcapacity = 4;
    ret->functions = calloc(ret->funcapacity, sizeof(*ret->functions));

    return ret;
}

void destroy_state(State* S)
{
    for (size_t i = 0; i < S->funlen; ++i) {
        FunctionWrapper wrapper = S->functions[i];
        if (wrapper.type == FUNCTION) {
            free_function(wrapper.u.function);
        }
        free(wrapper.name);
    }
    free(S->functions);
    free(S);
}

_Noreturn void compiletimeerror(char* fmt, ...)
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

static inline void try_code_resize(Function* fn, int n)
{
    if (fn->codesize+n >= fn->codecapacity) {
        while (fn->codesize+n >= fn->codecapacity) {
            fn->codecapacity *= 2;
        }
        codepoint_t* tmp = realloc(fn->code, sizeof(*fn->code) * fn->codecapacity);
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

static char* overtake_ast_str(AST* ast)
{
    assert(ast->val.str);
    char* ret = ast->val.str;
    ast->val.str = NULL;

    return ret;
}


// Returns position of inserted op
size_t emitraw8(Function* fn, uint8_t op)
{
    _Static_assert(OP_MAX_VALUE == (uint8_t)OP_MAX_VALUE, "Operator does not fit into 8 bit");
    try_code_resize(fn, 1);
    fn->code[fn->codesize] = op;

    return fn->codesize++;
}

size_t emitraw16(Function* fn, uint16_t op)
{
    uint16_t le = htole16(op);
    try_code_resize(fn, 2);
    *(uint16_t*)(fn->code + fn->codesize) = le;

    const size_t ret = fn->codesize;
    fn->codesize += 2;

    return ret;
}


size_t emitraw32(Function* fn, uint32_t op)
{
    uint32_t le = htole32(op);

    try_code_resize(fn, 4);
    *(uint32_t*)(fn->code + fn->codesize) = le;

    const size_t ret = fn->codesize;
    fn->codesize += 4;

    return ret;
}


size_t emitraw64(Function* fn, uint64_t op)
{
    uint64_t le = htole64(op);

    try_code_resize(fn, 8);
    *(uint64_t*)(fn->code + fn->codesize) = le;

    const size_t ret = fn->codesize;
    fn->codesize += 8;

    return ret;
}

size_t emit(Function* fn, Operator op)
{
    return emitraw8(fn, op);
}


static inline size_t emit_replace32(Function* fn, size_t position, uint32_t op)
{
    assert(position < fn->codesize - 1);
    *(uint32_t*)(fn->code + position) = htole32(op);

    return position;
}

void emitlong(Function* fn, int64_t lint)
{
    emit(fn, OP_LONG);
    _Static_assert(sizeof(codepoint_t) == sizeof(lint)/8, "Long is not twice as long as Operator");
    _Static_assert(sizeof(lint) == 8, "Long is not 8 bytes.");
    emitraw64(fn, (uint64_t) lint);
}

void emitcast(Function* fn, VARIANTTYPE type)
{
    _Static_assert((int8_t)TYPE_MAX_VALUE == TYPE_MAX_VALUE,
                   "VARIANTTYPE does not fit into 8 bit");
    emit(fn, OP_CAST);
    emitraw8(fn, type);
}

uint16_t addstring(Function* fn, char* str)
{
    try_strs_resize(fn);
    fn->strs[fn->strlen] = str;
    return fn->strlen++;
}

static size_t emitstring(Function* fn, char* str)
{
    size_t ret = emit(fn, OP_STR);
    emitraw16(fn, addstring(fn, str));

    return ret;
}

void addfunction(State* S, FunctionWrapper fn)
{
    if (!try_resize(&S->funcapacity, S->funlen,
                    (void**)&S->functions, sizeof(*S->functions), NULL)) {
        compiletimeerror("could not realloc functions");
        return;
    }

    assert(S->funlen < S->funcapacity);
    S->functions[S->funlen++] = fn;
}

static void compile_string(Function* fn, AST* ast)
{
    emitstring(fn, overtake_ast_str(ast));
}

static void compile_function(State* S, AST* ast)
{
    assert(ast->node1 && ast->node2 && ast->node3);
    AST* const name = ast->node1;
    AST* const params = ast->node2;
    AST* const body = ast->node3;
    Function* fn = create_function();
    fn->lineno_defined = name->lineno;
    size_t paramcount = ast_list_count(params);

    fn->paramlen = (uint8_t) paramcount;
    if (paramcount != fn->paramlen) {
        compiletimeerror("Failed to compile function with %d parameters.", paramcount);
        return;
    }

    fn->params = malloc(sizeof(*fn->params) * paramcount);
    AST* param = params->next;
    for (int i = 0; param; ++i, param = param->next) {
        assert(param->type == AST_ARGUMENT);
        fn->params[i] = overtake_ast_str(param);
    }

    addfunction(S, wrap_function(fn, overtake_ast_str(name)));
    compile(S, fn, body);

    emit(fn, OP_NULL); // Safeguard to guarantee that we have a return value
    emit(fn, OP_RETURN);
}

static void compile_call(State* S, Function* fn, AST* ast)
{
    assert(ast->node1);
    uint8_t argcount = 0;
    AST* args = ast->node1->next;
    while (args) { // Push args
        compile(S, fn, args);
        args = args->next;
        if (argcount + 1 < argcount) {
            compiletimeerror("Cannot compile functions with argument >255");
        }
        argcount++;
    }

    emitstring(fn, overtake_ast_str(ast));

    emit(fn, OP_CALL);
    emitraw8(fn, argcount); // Number of parameters
}

static void compile_blockstmt(State* S, Function* fn, AST* ast)
{
    assert(ast->type == AST_LIST);
    AST* current = ast->next;
    while (current) {
        compile(S, fn, current);
        current = current->next;
    }
}

static void compile_echostmt(State* S, Function* fn, AST* ast)
{
    assert(ast->type == AST_ECHO);
    assert(ast->node1);
    compile(S, fn, ast->node1);
    emit(fn, OP_ECHO);
}

static void compile_html(Function* fn, AST* ast)
{
    emitstring(fn, overtake_ast_str(ast));
    emit(fn, OP_ECHO);
}

static void compile_assignmentexpr(State* S, Function* fn, AST* ast)
{
    assert(ast->type == AST_ASSIGNMENT);
    assert(ast->node1);
    assert(ast->val.str);
    compile(S, fn, ast->node1);
    emit(fn, OP_ASSIGN);
    emitraw16(fn, addstring(fn, overtake_ast_str(ast)));
}

static void compile_varexpr(Function* fn, AST* ast)
{
    emit(fn, OP_LOOKUP);
    emitraw16(fn, addstring(fn, overtake_ast_str(ast)));
}

static void compile_ifstmt(State* S, Function* fn, AST* ast)
{
    assert(ast->type == AST_IF);
    assert(ast->node1 && ast->node2);
    compile(S, fn, ast->node1);
    emitcast(fn, TYPE_BOOL);
    emit(fn, OP_JMPZ); // Jump over code if false
    size_t placeholder = emitraw32(fn, OP_INVALID); // Placeholder
    compile(S, fn, ast->node2);
    emit_replace32(fn, placeholder, (Operator) emit(fn, OP_NOP)); // place to jump over if
    assert((Operator)fn->codesize == fn->codesize);

    if (ast->node3) {
        emit(fn, OP_JMP);
        size_t else_placeholder = emitraw32(fn, OP_INVALID); // placeholder
        // When we get here, we already assigned a jmp to here for the else branch
        // However, we need to increase it by one two jmp over this jmp
        emit_replace32(fn, placeholder, (Operator) fn->codesize);

        compile(S, fn, ast->node3);
        emit_replace32(fn, else_placeholder, (Operator) emit(fn, OP_NOP));
    }
}

static void compile_binop(State* S, Function* fn, AST* ast)
{
    assert(ast->type == AST_BINOP);
    assert(ast->node1 && ast->node2);
    compile(S, fn, ast->node1);
    compile(S, fn, ast->node2);
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
        case '.':
            emit(fn, OP_CONCAT);
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
        case '<':
            emit(fn, OP_LT);
            break;
        case '>':
            emit(fn, OP_GT);
            break;
        case TK_EQ:
            emit(fn, OP_EQ);
            break;
        case TK_AND:
            emit(fn, OP_AND);
            break;
        case TK_OR:
            emit(fn, OP_OR);
            break;
        default:
            assert(false && "Undefined BINOP");
            break;
    }
}

static void compile_prefixop(State* S, Function* fn, AST* ast)
{
    assert(ast->node1->type == AST_VAR);
    compile(S, fn, ast->node1);
    uint16_t nameidx = *(uint16_t*)(&fn->code[fn->codesize - 2]); // Next codepoint
    assert(nameidx < fn->strlen);
    emit(fn, OP_ADD1);
    emit(fn, OP_DUP); // one for the assignment, one for returning value
    emit(fn, OP_ASSIGN);
    emitraw16(fn, nameidx);
}
static void compile_postfixop(State* S, Function* fn, AST* ast)
{
    assert(ast->node1->type == AST_VAR);
    compile(S, fn, ast->node1);
    uint16_t nameidx = *(uint16_t*)(&fn->code[fn->codesize - 2]); // Next codepoint
    assert(nameidx < fn->strlen);
    emit(fn, OP_DUP); // For returning the previous value
    emit(fn, OP_ADD1);
    emit(fn, OP_ASSIGN);
    emitraw16(fn, nameidx);
}

static void compile_whilestmt(State* S, Function* fn, AST* ast)
{
    assert(ast->type == AST_WHILE);
    assert(ast->node1 && ast->node2);
    size_t while_start = fn->codesize;
    compile(S, fn, ast->node1);
    emitcast(fn, TYPE_BOOL);
    emit(fn, OP_JMPZ); // Jump over body if zero
    size_t placeholder = emitraw32(fn, OP_INVALID); // Placeholder
    compile(S, fn, ast->node2);

    emit(fn, OP_JMP); // Jump back to while start
    if ((uint32_t) while_start != while_start) {
        compiletimeerror("Jump address overflowed while compiling while statement");
    }
    emitraw32(fn, (uint32_t) while_start);

    emit_replace32(fn, placeholder, (Operator) emit(fn, OP_NOP));
}


static void compile_forstmt(State* S, Function* fn, AST* ast) {
    assert(ast->type == AST_FOR);
    assert(ast->node1 && ast->node2 && ast->node3 && ast->node4);
    compile(S, fn, ast->node1); // Init
    size_t for_start = fn->codesize;
    compile(S, fn, ast->node2); // Condition
    emitcast(fn, TYPE_BOOL);
    emit(fn, OP_JMPZ); // Jump over body if zero
    size_t placeholder = emitraw32(fn, OP_INVALID); // Placeholder
    compile(S, fn, ast->node4); // Body
    compile(S, fn, ast->node3); // Post expression

    emit(fn, OP_JMP); // Jump back to for start
    if ((uint32_t) for_start != for_start) {
        compiletimeerror("Jump address overflowed while compiling while statement");
    }
    emitraw32(fn, (uint32_t) for_start);

    emit_replace32(fn, placeholder, (Operator) emit(fn, OP_NOP));
}


Function* compile(State* S, Function* fn, AST* ast)
{
    switch (ast->type) {
        case AST_FUNCTION:
            compile_function(S, ast);
            break;
        case AST_RETURN:
            compile(S, fn, ast->node1);
            emit(fn, OP_RETURN);
            break;
        case AST_CALL:
            compile_call(S, fn, ast);
            break;
        case AST_LIST:
            compile_blockstmt(S, fn, ast);
            break;
        case AST_ECHO:
            compile_echostmt(S, fn, ast);
            break;
        case AST_IF:
            compile_ifstmt(S, fn, ast);
            break;
        case AST_STRING:
            compile_string(fn, ast);
            break;
        case AST_BINOP:
            compile_binop(S, fn, ast);
            break;
        case AST_PREFIXOP:
            compile_prefixop(S, fn, ast);
            break;
        case AST_POSTFIXOP:
            compile_postfixop(S, fn, ast);
            break;
        case AST_NOTOP:
            compile(S, fn, ast->node1);
            emit(fn, OP_NOT);
            break;
        case AST_LONG:
            emitlong(fn, ast->val.lint);
            break;
        case AST_NULL:
            emit(fn, OP_NULL);
            break;
        case AST_TRUE:
            emit(fn, OP_TRUE);
            break;
        case AST_FALSE:
            emit(fn, OP_FALSE);
            break;
        case AST_VAR:
            compile_varexpr(fn, ast);
            break;
        case AST_ASSIGNMENT:
            compile_assignmentexpr(S, fn, ast);
            break;
        case AST_HTML:
            compile_html(fn, ast);
            break;
        case AST_WHILE:
            compile_whilestmt(S, fn, ast);
            break;
        case AST_FOR:
            compile_forstmt(S, fn, ast);
            break;
        default:
            compiletimeerror("Unexpected Type '%s'", get_ASTTYPE_name(ast->type));
    }

    return fn;
}

void print_state(State* S)
{
    for (size_t i = 0; i < S->funlen; ++i) {
        FunctionWrapper fn = S->functions[i];
        if (S->functions[i].type == FUNCTION) {
            print_code(fn.u.function, fn.name);
        } else {
            printf("CFunction %s\n", fn.name);
        }
    }
}

void print_code(Function* fn, char* name)
{
    codepoint_t* ip = fn->code;
    int64_t lint;
    fprintf(stderr, "Function: %s (line %u)\n", name, fn->lineno_defined);

    fprintf(stderr, "\n-----------------------\n");
    while ((size_t)(ip - fn->code) < fn->codesize) {
        fprintf(stderr, "%04lx: ", ip - fn->code);
        Operator op = (Operator)*ip;
        const char* opname = get_Operator_name(op);
        size_t chars_written = 0;
        uint8_t bytes[9] = {op, 0}; // We have 8 bytes maximum per opcode
        if (!opname) {
            fprintf(stderr, "Unexpected op: %d", op);
        } else {
            chars_written += fprintf(stderr, "%s ", opname);
        }
        switch (*ip++) {
            case OP_STR:
                assert(*ip < fn->strlen);
                uint16_t strpos = fetch16(ip);
                bytes[1] = *ip++;
                bytes[2] = *ip++;
                char* escaped_string = malloc((strlen(fn->strs[strpos]) * 2 + 1) * sizeof(char));
                escaped_str(escaped_string, fn->strs[strpos]);
                chars_written += fprintf(stderr, "\"%s\"", escaped_string);
                free(escaped_string);
                break;
            case OP_CALL:
                bytes[1] = fetch8(ip);
                chars_written += fprintf(stderr, "%d", fetch8(ip));
                ++ip;
                break;
            case OP_LONG:
                lint = (int64_t) fetch64(ip);
                *(uint64_t*)(bytes + 1) = *(uint64_t*)ip;
                ip += 8;
                chars_written += fprintf(stderr, "%" PRId64, lint);
                break;
            case OP_ASSIGN:
                *(uint16_t*)(bytes + 1) = fetch16(ip);
                assert(*ip < fn->strlen);
                chars_written += fprintf(stderr, "$%s = pop()", fn->strs[fetch16(ip)]);
                ip += 2;
                break;
            case OP_LOOKUP:
                *(uint16_t*)(bytes + 1) = fetch16(ip);
                assert(*ip < fn->strlen);
                chars_written += fprintf(stderr, "$%s", fn->strs[fetch16(ip)]);
                ip += 2;
                break;
            case OP_JMP:
            case OP_JMPZ:
                *(uint32_t*)(bytes+1) = fetch32(ip);
                chars_written += fprintf(stderr, ":%04x", fetch32(ip));
                ip += 4;
                break;
            case OP_CAST:
                bytes[1] = fetch8(ip);
                chars_written += fprintf(stderr, "(%s)", get_VARIANTTYPE_name((VARIANTTYPE)fetch8(ip)));
                ++ip;
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
}
