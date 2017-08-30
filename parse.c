#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include "parse.h"
#include "lex.h"

#define EXP0(type) new_ast(type, NULL, NULL)
#define EXP1(type, lhs) new_ast(type, lhs, NULL)
#define EXP2(type, lhs, rhs) new_ast(type, lhs, rhs)

static int get_next_token(State* S)
{
    return S->token = get_token(S);
}

noreturn static inline void parseerror(State* S, char* fmt, ...)
{
    va_list ap;
    char msgbuf[256];
    va_start(ap, fmt);
    vsnprintf(msgbuf, arrcount(msgbuf), fmt, ap);
    va_end(ap);

    char buf[256];
    snprintf(buf, arrcount(buf), "%s on line %d.", msgbuf, S->lineno);

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



static AST* new_ast(ASTTYPE type, AST* left, AST* right)
{
    AST* ret = malloc(sizeof(AST));

    ret->type = type;
    ret->left = left;
    ret->right = right;
    ret->next = NULL;
    ret->val = NULL;

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

static AST* parse_echostmt(State* S);

static AST* parse_stmt(State* S)
{
    if (accept(S, TK_ECHO)) {
        return parse_echostmt(S);
    }

    parseerror(S, "Unexpected token '%s', (0x%x).", get_token_name(S->token), S->token);
    return NULL;
}

static AST* parse_primary(State* S)
{
    AST* ret;
    if (S->token == TK_STRING) {
        ret = EXP0(STRINGEXPR);
        ret->val = strdup(S->u.string);
        expect(S, TK_STRING);
        return ret;
    }

    parseerror(S, "Unexpected token '%s', expected primary", get_token_name(S->token));
}


static AST* parse_expr(State* S)
{
    AST* ret = parse_primary(S);
    if (accept(S, '.')) {
        ret = EXP2(STRINGBINOP, ret, parse_expr(S));
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

AST* parse(FILE* file)
{
    State* S = new_state(file);
    AST* ret = EXP0(BLOCKSTMT);
    int tok;
    while ((tok = get_next_token(S)) != TK_END) {
        if (S->mode != PHP || tok == TK_OPENTAG) {
            continue;
        }

        block_append(ret, parse_stmt(S));
    }
    destroy_state(S);

    return ret;
}

void destroy_ast(AST* ast)
{
    free(ast->val);
    if (ast->type == BLOCKSTMT) {
        destroy_ast(ast->next);
    }

    if (ast->left) {
        destroy_ast(ast->left);
    }
    if (ast->right) {
        destroy_ast(ast->right);
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
        case STRINGEXPR:
            return "STRINGEXPR";
        case STRINGBINOP:
            return "STRINGBINOP";
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
    if (ast->val) {
        puts(ast->val);
    } else {
        puts("");
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
}