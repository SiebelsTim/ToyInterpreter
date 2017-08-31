#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include "parse.h"
#include "lex.h"

#define EXP0(type) new_ast(type, NULL, NULL, NULL)
#define EXP1(type, lhs) new_ast(type, lhs, NULL, NULL)
#define EXP2(type, lhs, rhs) new_ast(type, lhs, rhs, NULL)
#define EXP3(type, lhs, rhs, extra) new_ast(type, lhs, rhs, extra);

static int get_next_token(State* S)
{
    S->token = get_token(S);
    return S->token;
}

noreturn static inline void parseerror(State* S, char* fmt, ...)
{
    va_list ap;
    char msgbuf[256];
    va_start(ap, fmt);
    vsnprintf(msgbuf, arrcount(msgbuf), fmt, ap);
    va_end(ap);

    char buf[256];
    snprintf(buf, arrcount(buf), "Parse error: %s on line %d.", msgbuf, S->lineno);

    S->val = ERROR;
    S->error = strdup(buf);
    puts(S->error);

    abort();
}

static inline bool accept(State* S, int tok)
{
    if (S->token == tok) {
        get_next_token(S);
        return true;
    }

    return false;
}

static inline bool expect(State* S, int tok)
{
    if (!accept(S, tok)) {
        parseerror(S, "Expected %s, got %s", get_token_name(tok), get_token_name(S->token));
        return false;
    }

    return true;
}



static AST* new_ast(ASTTYPE type, AST* left, AST* right, AST* extra)
{
    AST* ret = malloc(sizeof(AST));

    ret->type = type;
    ret->left = left;
    ret->right = right;
    ret->extra = extra;
    ret->next = NULL;
    ret->val.str = NULL;

    return ret;
}

static AST* block_append(AST* block, AST* ast)
{
    assert(block);
    assert(block->type == BLOCKSTMT);
    AST* current = block;
    while (current->next) {
        current = current->next;
    }
    current->next = ast;
    return block;
}

static AST* parse_expr(State* S);
static AST* parse_echostmt(State* S);
static AST* parse_ifstmt(State* S);
static AST* parse_blockstmt(State* S);
static AST* parse_assignstmt(State* S);

static AST* parse_stmt(State* S)
{
    if (accept(S, TK_ECHO)) {
        return parse_echostmt(S);
    }
    if (accept(S, TK_IF)) {
        return parse_ifstmt(S);
    }
    if (accept(S, '{')) {
        return parse_blockstmt(S);
    }
    if (S->token == TK_VAR) {
        return parse_assignstmt(S);
    }

    parseerror(S, "Unexpected token '%s', (0x%x).", get_token_name(S->token), S->token);
    return NULL;
}

static AST* parse_primary(State* S)
{
    AST* ret;
    if (S->token == TK_STRING) {
        ret = EXP0(STRINGEXPR);
        ret->val.str = strdup(S->u.string);
        expect(S, TK_STRING);
        return ret;
    }

    if (S->token == TK_LONG) {
        ret = EXP0(LONGEXPR);
        ret->val.lint = S->u.lint;
        expect(S, TK_LONG);
        return ret;
    }

    if (accept(S, TK_TRUE)) {
        ret = EXP0(LONGEXPR);
        ret->val.lint = 1;
        return ret;
    }
    if (accept(S, TK_FALSE)) {
        ret = EXP0(LONGEXPR);
        ret->val.lint = 0;
        return ret;
    }

    if (accept(S, TK_VAR)) {
        ret = EXP0(VAREXPR);
        ret->val.str = strdup(S->u.string);
        return ret;
    }

    if (accept(S, '(')) {
        ret = parse_expr(S);
        expect(S, ')');
        return ret;
    }

    parseerror(S, "Unexpected token '%s', expected primary", get_token_name(S->token));
}


static AST* parse_expr(State* S)
{

    AST* ret = parse_primary(S);

    if (accept(S, '.')) {
        ret = EXP2(BINOP, ret, parse_expr(S));
        ret->val.lint = '.';
    }
    if (accept(S, '+')) {
        ret = EXP2(BINOP, ret, parse_expr(S));
        ret->val.lint = '+';
    }
    if (accept(S, '-')) {
        ret = EXP2(BINOP, ret, parse_expr(S));
        ret->val.lint = '-';
    }
    if (accept(S, '*')) {
        ret = EXP2(BINOP, ret, parse_expr(S));
        ret->val.lint = '*';
    }
    if (accept(S, '/')) {
        ret = EXP2(BINOP, ret, parse_expr(S));
        ret->val.lint = '/';
    }
    return ret;
}

static AST* parse_echostmt(State* S)
{
    AST* expr = parse_expr(S);

    expect(S, ';');

    AST* ret = EXP1(ECHOSTMT, expr);
    return ret;
}

static AST* parse_blockstmt(State* S)
{
    AST* ret = EXP0(BLOCKSTMT);
    while (S->token != '}') {
        block_append(ret, parse_stmt(S));
    }
    expect(S, '}');

    return ret;
}

static AST* parse_ifstmt(State* S)
{
    expect(S, '(');
    AST* expr = parse_expr(S);

    expect(S, ')');

    AST* body = parse_stmt(S);

    AST* else_body = NULL;
    if (accept(S, TK_ELSE)) {
        else_body = parse_stmt(S);
    }

    AST* ret = EXP3(IFSTMT, expr, body, else_body);
    return ret;
}

static AST* parse_assignstmt(State* S)
{
    assert(S->token == TK_VAR);
    char* name = strdup(S->u.string);
    get_next_token(S); // Skip TK_VAR

    expect(S, '=');
    AST* val = parse_expr(S);
    AST* ret = EXP1(ASSIGNMENTEXPR, val);
    ret->val.str = name;

    expect(S, ';');

    return ret;
}

AST* parse(FILE* file)
{
    State* S = new_state(file);
    AST* ret = EXP0(BLOCKSTMT);
    get_next_token(S); // init
    while (S->token != TK_END) {
        if (S->mode != PHP || S->token == TK_OPENTAG) {
            get_next_token(S);
            continue;
        }

        block_append(ret, parse_stmt(S));
    }
    destroy_state(S);

    return ret;
}

void destroy_ast(AST* ast)
{
    if (ast->type == STRINGEXPR || ast->type == VAREXPR || ast->type == ASSIGNMENTEXPR) {
        free(ast->val.str);
    }
    if (ast->next) {
        destroy_ast(ast->next);
    }

    if (ast->left) {
        destroy_ast(ast->left);
    }
    if (ast->right) {
        destroy_ast(ast->right);
    }

    if (ast->extra) {
        destroy_ast(ast->extra);
    }

    free(ast);
}

static char* get_ast_typename(ASTTYPE type)
{
    switch (type) {
        case BLOCKSTMT:
            return "BLOCKSTMT";
        case ECHOSTMT:
            return "ECHOSTMT";
        case IFSTMT:
            return "IFSTMT";
        case STRINGEXPR:
            return "STRINGEXPR";
        case LONGEXPR:
            return "LONGEXPR";
        case BINOP:
            return "BINOP";
        case VAREXPR:
            return "VAREXPR";
        case ASSIGNMENTEXPR:
            return "ASSIGNMENTEXPR";
        default:
            return "UNKNOWNTYPE";
    }
}

void print_ast(AST* ast, int level)
{
    for (int i = 0; i < level; ++i) {
        putchar(' ');
        putchar(' ');
    }
    printf("%s: ", get_ast_typename(ast->type));

    switch (ast->type) {
        case STRINGEXPR:
            puts(ast->val.str);
            break;
        case LONGEXPR:
            printf("%ld\n", ast->val.lint);
            break;
        default:
            puts("");
            break;
    }

    if (ast->type == BLOCKSTMT) {
        while (ast->next) {
            print_ast(ast->next, level+1);
            ast = ast->next;
        }
        return;
    }

    if (ast->left) {
        print_ast(ast->left, level+1);
    }

    if (ast->right) {
        print_ast(ast->right, level+1);
    }

    if (ast->extra) {
        print_ast(ast->extra, level+1);
    }
}