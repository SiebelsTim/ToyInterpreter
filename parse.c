#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <inttypes.h>
#include "parse.h"

DEFINE_ENUM(ASTTYPE, ENUM_ASTTYPE);

#define EXP0(type) new_ast(type, NULL, NULL, NULL, NULL)
#define EXP1(type, one) new_ast(type, one, NULL, NULL, NULL)
#define EXP2(type, one, two) new_ast(type, one, two, NULL, NULL)
#define EXP3(type, one, two, three) new_ast(type, one, two, three, NULL)
#define EXP4(type, one, two, three, four) new_ast(type, one, two, three, four)

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
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
    va_end(ap);

    char buf[256];
    snprintf(buf, sizeof(buf), "Parse error: %s on line %d.", msgbuf, S->lineno);

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



static AST* new_ast(ASTTYPE type, AST* one, AST* two, AST* three, AST* four)
{
    AST* ret = malloc(sizeof(AST));

    ret->type = type;
    ret->node1 = one;
    ret->node2 = two;
    ret->node3 = three;
    ret->node4 = four;
    ret->next = NULL;
    ret->val.str = NULL;

    return ret;
}

static AST* block_append(AST* block, AST* ast)
{
    assert(block);
    assert(block->type == AST_BLOCK);
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
static AST* parse_whilestmt(State* S);
static AST* parse_forstmt(State* S);
static AST* parse_blockstmt(State* S);

static AST* parse_stmt(State* S)
{
    if (accept(S, TK_ECHO)) {
        return parse_echostmt(S);
    }
    if (accept(S, TK_IF)) {
        return parse_ifstmt(S);
    }
    if (accept(S, TK_WHILE)) {
        return parse_whilestmt(S);
    }
    if (accept(S, TK_FOR)) {
        return parse_forstmt(S);
    }
    if (accept(S, '{')) {
        return parse_blockstmt(S);
    }

    AST* ret = parse_expr(S);
    expect(S, ';');
    return ret;
}

static AST* parse_varexpr(State* S)
{
    if (S->token == TK_VAR) {
        AST* ret;
        ret = EXP0(AST_VAR);
        ret->val.str = overtake_str(S);
        expect(S, TK_VAR); // Skip
        if (accept(S, '=')) {
            ret->type = AST_ASSIGNMENT;
            ret->node1 = parse_expr(S);
        }
        return ret;
    }

    return NULL;
}

static AST* parse_primary(State* S)
{
    AST* ret;
    if (S->token == TK_STRING) {
        ret = EXP0(AST_STRING);
        ret->val.str = overtake_str(S);
        expect(S, TK_STRING);
        return ret;
    }

    if (S->token == TK_LONG) {
        ret = EXP0(AST_LONG);
        ret->val.lint = S->u.lint;
        expect(S, TK_LONG);
        return ret;
    }

    if (accept(S, TK_TRUE)) {
        ret = EXP0(AST_LONG);
        ret->val.lint = 1;
        return ret;
    }
    if (accept(S, TK_FALSE)) {
        ret = EXP0(AST_LONG);
        ret->val.lint = 0;
        return ret;
    }
    if (accept(S, TK_NULL)) {
        return EXP0(AST_NULL);
    }
    if (accept(S, TK_PLUSPLUS)) {
        if (S->token == TK_VAR) {
            ret = EXP1(AST_PREFIXOP, parse_varexpr(S));
            ret->val.lint = '+';
            return ret;
        } else {
            expect(S, TK_VAR); // Error
            return NULL;
        }
    }
    if (accept(S, TK_MINUSMINUS)) {
        if (S->token == TK_VAR) {
            ret = EXP1(AST_PREFIXOP, parse_varexpr(S));
            ret->val.lint = '-';
            return ret;
        } else {
            expect(S, TK_VAR); // Error
            return NULL;
        }
    }
    if (accept(S, '!')) {
        ret = EXP1(AST_NOTOP, parse_expr(S));
        return ret;
    }

    AST* varexpr = NULL;
    if ((varexpr = parse_varexpr(S)) != NULL) {
        return varexpr;
    }

    if (accept(S, '(')) {
        ret = parse_expr(S);
        expect(S, ')');
        return ret;
    }

    parseerror(S, "Unexpected token '%s', expected primary", get_token_name(S->token));
    return NULL;
}


static AST* parse_expr(State* S)
{

    AST* ret = parse_primary(S);

    if (accept(S, '.')) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = '.';
    }
    if (accept(S, '+')) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = '+';
    }
    if (accept(S, '-')) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = '-';
    }
    if (accept(S, '*')) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = '*';
    }
    if (accept(S, '/')) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = '/';
    }
    if (accept(S, TK_SHL)) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = TK_SHL;
    }
    if (accept(S, TK_SHR)) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = TK_SHR;
    }

    if (accept(S, TK_AND)) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = TK_AND;
    }
    if (accept(S, TK_OR)) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = TK_OR;
    }

    if (accept(S, TK_EQ)) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = TK_EQ;
    }
    if (accept(S, '<')) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = '<';
    }
    if (accept(S, '>')) {
        ret = EXP2(AST_BINOP, parse_expr(S), ret);
        ret->val.lint = '<';
    }
    if (accept(S, TK_LTEQ)) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = TK_LTEQ;
    }
    if (accept(S, TK_GTEQ)) {
        ret = EXP2(AST_BINOP, ret, parse_expr(S));
        ret->val.lint = TK_GTEQ;
    }

    if (accept(S, TK_PLUSPLUS)) {
        ret = EXP1(AST_POSTFIXOP, ret);
        ret->val.lint = '+';
    }
    if (accept(S, TK_MINUSMINUS)) {
        ret = EXP1(AST_POSTFIXOP, ret);
        ret->val.lint = '-';
    }

    return ret;
}

static AST* parse_echostmt(State* S)
{
    AST* expr = parse_expr(S);

    expect(S, ';');

    AST* ret = EXP1(AST_ECHO, expr);
    return ret;
}

static AST* parse_blockstmt(State* S)
{
    AST* ret = EXP0(AST_BLOCK);
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

    AST* ret = EXP3(AST_IF, expr, body, else_body);
    return ret;
}

static AST* parse_whilestmt(State* S)
{
    expect(S, '(');
    AST* expr = parse_expr(S);
    expect(S, ')');

    AST* body = parse_stmt(S);

    return EXP2(AST_WHILE, expr, body);
}

static AST* parse_forstmt(State* S)
{
    expect(S, '(');
    AST* init = parse_expr(S);
    expect(S, ';');
    AST* condition = parse_expr(S);
    expect(S, ';');
    AST* post = parse_expr(S);
    expect(S, ')');

    AST* body = parse_stmt(S);
    return EXP4(AST_FOR, init, condition, post, body);
}

AST* parse(FILE* file)
{
    State* S = new_state(file);
    AST* ret = EXP0(AST_BLOCK);
    get_next_token(S); // init
    while (S->token != TK_END) {
        if (S->token == TK_HTML) {
            AST* html = EXP0(AST_HTML);
            html->val.str = S->u.string;
            block_append(ret, html);
        }
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
    if (ast->type == AST_STRING || ast->type == AST_VAR || ast->type == AST_ASSIGNMENT) {
        free(ast->val.str);
    }
    if (ast->next) {
        destroy_ast(ast->next);
    }

    if (ast->node1) {
        destroy_ast(ast->node1);
    }
    if (ast->node2) {
        destroy_ast(ast->node2);
    }

    if (ast->node3) {
        destroy_ast(ast->node3);
    }

    if (ast->node4) {
        destroy_ast(ast->node4);
    }

    free(ast);
}


void print_ast(AST* ast, int level)
{
    for (int i = 0; i < level; ++i) {
        putchar('|');
        putchar(' ');
    }
    printf("%p %s: ", ast, get_ASTTYPE_name(ast->type));

    switch (ast->type) {
        case AST_STRING:
        case AST_VAR:
        case AST_ASSIGNMENT:
            puts(ast->val.str);
            break;
        case AST_LONG:
            printf("%" PRId64 "\n", ast->val.lint);
            break;
        case AST_BINOP:
            printf("%c\n", (char) ast->val.lint);
            break;
        default:
            puts("");
            break;
    }

    if (ast->type == AST_BLOCK) {
        while (ast->next) {
            print_ast(ast->next, level+1);
            ast = ast->next;
        }
        return;
    }

    if (ast->node1) {
        print_ast(ast->node1, level+1);
    }

    if (ast->node2) {
        print_ast(ast->node2, level+1);
    }

    if (ast->node3) {
        print_ast(ast->node3, level+1);
    }
}