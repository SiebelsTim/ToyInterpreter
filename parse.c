#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include "crossplatform/stdnoreturn.h"
#include <stdarg.h>
#include <inttypes.h>
#include "parse.h"
#include "util.h"

DEFINE_ENUM(ASTTYPE, ENUM_ASTTYPE);

#define EXP0(type) new_ast(type, S->lineno, NULL, NULL, NULL, NULL)
#define EXP1(type, one) new_ast(type, S->lineno, one, NULL, NULL, NULL)
#define EXP2(type, one, two) new_ast(type, S->lineno, one, two, NULL, NULL)
#define EXP3(type, one, two, three) new_ast(type, S->lineno, one, two, three, NULL)
#define EXP4(type, one, two, three, four) new_ast(type, S->lineno, one, two, three, four)

static int get_next_token(Lexer* S)
{
    S->token = get_token(S);
    return S->token;
}

_Noreturn static inline void parseerror(Lexer* S, char* fmt, ...)
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

static inline bool accept(Lexer* S, int tok)
{
    if (S->token == tok) {
        get_next_token(S);
        return true;
    }

    return false;
}

static inline bool expect(Lexer* S, int tok)
{
    if (!accept(S, tok)) {
        parseerror(S, "Expected %s, got %s", get_token_name(tok), get_token_name(S->token));
        return false;
    }

    return true;
}

static inline bool expect_one_of(Lexer* S, int count, ...)
{
    va_list ap;
    va_start(ap, count);

    char expectstr[100] = {0};
    size_t expectstr_len = 0;

    for (int i = 0; i < count; ++i) {
        const int tok = va_arg(ap, int);
        if (accept(S, tok)) {
            return true;
        }
        const char* tok_name = get_token_name(tok);
        expectstr_len += strlen(tok_name) + 2;
        if (expectstr_len < arrcount(expectstr) - 1) {
            strcat(expectstr, tok_name);
            strcat(expectstr, ", ");
        }
    }

    parseerror(S, "Expected one of %s got %s", expectstr, get_token_name(S->token));
    return false;
}

static AST* new_ast(ASTTYPE type, lineno_t lineno, AST* one, AST* two, AST* three,
                    AST* four)
{
    AST* ret = malloc(sizeof(AST));

    ret->type = type;
    ret->node1 = one;
    ret->node2 = two;
    ret->node3 = three;
    ret->node4 = four;
    ret->next = NULL;
    ret->val.str = NULL;
    ret->lineno = lineno;

    return ret;
}

static AST* ast_list_append(AST *block, AST *ast)
{
    assert(block);
    assert(block->type == AST_LIST);
    AST* current = block;
    while (current->next) {
        current = current->next;
    }
    current->next = ast;
    return block;
}

static AST* parse_expr(Lexer* S);
static AST* parse_echostmt(Lexer* S);
static AST* parse_ifstmt(Lexer* S);
static AST* parse_whilestmt(Lexer* S);
static AST* parse_forstmt(Lexer* S);
static AST* parse_blockstmt(Lexer* S);
static AST* parse_function(Lexer* S);
static AST* parse_paramlist(Lexer* S);

static AST* parse_stmt(Lexer* S)
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
    if (accept(S, TK_FUNCTION)) {
        return parse_function(S);
    }
    if (accept(S, '{')) {
        return parse_blockstmt(S);
    }

    AST* ret = parse_expr(S);
    expect(S, ';');
    return ret;
}

static AST* parse_varexpr(Lexer* S)
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

static AST* parse_primary(Lexer* S)
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
        return EXP1(AST_NOTOP, parse_expr(S));
    }
    if (accept(S, TK_RETURN)) {
        return EXP1(AST_RETURN, parse_expr(S));
    }
    if (S->token == TK_IDENTIFIER) {
        char* name = overtake_str(S);
        expect(S, TK_IDENTIFIER);
        expect(S, '(');
        AST* paramlist = parse_paramlist(S);
        expect(S, ')');
        ret = EXP1(AST_CALL, paramlist);
        ret->val.str = name;
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


static AST* parse_expr(Lexer* S)
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

static AST* parse_echostmt(Lexer* S)
{
    AST* expr = parse_expr(S);

    expect(S, ';');

    AST* ret = EXP1(AST_ECHO, expr);
    return ret;
}

static AST* parse_blockstmt(Lexer* S)
{
    AST* ret = EXP0(AST_LIST);
    while (S->token != '}') {
        ast_list_append(ret, parse_stmt(S));
    }
    expect(S, '}');

    return ret;
}

static AST* parse_ifstmt(Lexer* S)
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

static AST* parse_whilestmt(Lexer* S)
{
    expect(S, '(');
    AST* expr = parse_expr(S);
    expect(S, ')');

    AST* body = parse_stmt(S);

    return EXP2(AST_WHILE, expr, body);
}

static AST* parse_forstmt(Lexer* S)
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

static AST* parse_identifier(Lexer* S)
{
    // This function is used so we can get better line numbers for error messages
    if (S->token != TK_IDENTIFIER) {
        expect(S, TK_IDENTIFIER); // Raise error
        return NULL;
    }

    AST* ret = EXP0(AST_IDENTIFIER);
    ret->val.str = overtake_str(S);
    expect(S, TK_IDENTIFIER); // Skip

    return ret;
}

static AST* parse_arglist(Lexer* S)
{
    AST* ret = EXP0(AST_LIST);

    if (S->token == ')') {
        return ret;
    }

    while (true) {
        if (S->token == TK_VAR) {
            AST* arg = EXP0(AST_ARGUMENT);
            arg->val.str = overtake_str(S);
            ast_list_append(ret, arg);
        }
        expect(S, TK_VAR);

        if (S->token != ',') {
            break;
        }
        get_next_token(S);
    }

    return ret;
}

static AST* parse_paramlist(Lexer* S)
{
    AST* ret = EXP0(AST_LIST);

    if (S->token == ')') {
        return ret;
    }

    while (true) {
        ast_list_append(ret, parse_expr(S));

        if (S->token != ',') {
            break;
        }
        get_next_token(S);
    }

    return ret;
}

static AST* parse_function(Lexer* S)
{
    if (S->token != TK_IDENTIFIER) {
        parseerror(S, "Expected IDENTIFIER, %s given.", get_token_name(S->token));
        return NULL;
    }
    AST* name = parse_identifier(S);
    expect(S, '(');
    AST* parameters = parse_arglist(S);
    expect(S, ')');
    AST* body = parse_stmt(S);

    AST* ret = EXP3(AST_FUNCTION, name, parameters, body);

    return ret;
}

AST* parse(FILE* file)
{
    Lexer* S = create_lexer(file);
    AST* ret = EXP0(AST_LIST);
    get_next_token(S); // init
    while (S->token != TK_END) {
        if (S->token == TK_HTML) {
            AST* html = EXP0(AST_HTML);
            html->val.str = S->u.string;
            ast_list_append(ret, html);
        }
        if (S->mode != PHP || S->token == TK_OPENTAG) {
            get_next_token(S);
            continue;
        }

        ast_list_append(ret, parse_stmt(S));
    }
    destroy_lexer(S);

    return ret;
}

size_t ast_list_count(AST* ast)
{
    assert(ast->type == AST_LIST);
    size_t ret = 0;

    while (ast->next) {
        ret++;
        ast = ast->next;
    }

    return ret;
}

void destroy_ast(AST* ast)
{
    if (ast->type == AST_STRING || ast->type == AST_VAR ||
        ast->type == AST_ASSIGNMENT || ast->type == AST_ARGUMENT) {
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

    char* escaped;
    switch (ast->type) {
        case AST_STRING:
        case AST_VAR:
        case AST_ASSIGNMENT:
        case AST_FUNCTION:
        case AST_CALL:
            escaped = malloc((strlen(ast->val.str) * 2 + 1) * sizeof(char));
            puts(escaped_str(escaped, ast->val.str));
            free(escaped);
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

    if (ast->type == AST_LIST) {
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

    if (ast->node4) {
        print_ast(ast->node4, level+1);
    }
}