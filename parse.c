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

#define EXP0(type, token) new_ast(type, token, NULL, NULL, NULL, NULL)
#define EXP1(type, token, one) new_ast(type, token, one, NULL, NULL, NULL)
#define EXP2(type, token, one, two) new_ast(type, token, one, two, NULL, NULL)
#define EXP3(type, token, one, two, three) new_ast(type, token, one, two, three, NULL)
#define EXP4(type, token, one, two, three, four) new_ast(type, token, one, two, three, four)

static Token get_next_token(Lexer* S)
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
    if (S->token.type == tok) {
        get_next_token(S);
        return true;
    }

    return false;
}

static inline bool expect(Lexer* S, int tok)
{
    if (!accept(S, tok)) {
        parseerror(S, "Expected %s, got %s", get_token_name(tok), get_token_name(S->token.type));
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

    parseerror(S, "Expected one of %s got %s", expectstr, get_token_name(S->token.type));
    return false;
}

static AST* new_ast(ASTTYPE type, Token token, AST* one, AST* two, AST* three,
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
    ret->lineno = token.lineno;

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
static AST* parse_const(Lexer* S);
static AST* parse_ifstmt(Lexer* S);
static AST* parse_whilestmt(Lexer* S);
static AST* parse_forstmt(Lexer* S);
static AST* parse_blockstmt(Lexer* S);
static AST* parse_function(Lexer* S);
static AST* parse_paramlist(Lexer* S);
static AST* parse_identifier(Lexer* S);

static AST* parse_stmt(Lexer* S)
{
    if (accept(S, TK_ECHO)) {
        return parse_echostmt(S);
    }
    if (accept(S, TK_CONST)) {
        return parse_const(S);
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
    if (S->token.type == TK_VAR) {
        AST* ret;
        ret = EXP0(AST_VAR, S->token);
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
    if (S->token.type == TK_STRING) {
        ret = EXP0(AST_STRING, S->token);
        ret->val.str = overtake_str(S);
        expect(S, TK_STRING);
        return ret;
    }

    if (S->token.type == TK_LONG) {
        ret = EXP0(AST_LONG, S->token);
        ret->val.lint = S->u.lint;
        expect(S, TK_LONG);
        return ret;
    }

    if (accept(S, TK_TRUE)) {
        return EXP0(AST_TRUE, S->token);
    }
    if (accept(S, TK_FALSE)) {
        return EXP0(AST_FALSE, S->token);
    }
    if (accept(S, TK_NULL)) {
        return EXP0(AST_NULL, S->token);
    }
    if (accept(S, TK_PLUSPLUS)) {
        if (S->token.type == TK_VAR) {
            Token token = S->token;
            ret = EXP1(AST_PREFIXOP, token, parse_varexpr(S));
            ret->val.lint = '+';
            return ret;
        } else {
            expect(S, TK_VAR); // Error
            return NULL;
        }
    }
    if (accept(S, TK_MINUSMINUS)) {
        if (S->token.type == TK_VAR) {
            Token token = S->token;
            ret = EXP1(AST_PREFIXOP, token, parse_varexpr(S));
            ret->val.lint = '-';
            return ret;
        } else {
            expect(S, TK_VAR); // Error
            return NULL;
        }
    }
    if (accept(S, '!')) {
        Token token = S->token;
        return EXP1(AST_NOTOP, token, parse_expr(S));
    }
    if (accept(S, TK_RETURN)) {
        Token token = S->token;
        return EXP1(AST_RETURN, token, parse_expr(S));
    }
    if (S->token.type == TK_IDENTIFIER) {
        char* name = overtake_str(S);
        expect(S, TK_IDENTIFIER);
        if (accept(S, '(')) { // function
            AST* paramlist = parse_paramlist(S);
            expect(S, ')');
            Token token = S->token;
            ret = EXP1(AST_CALL, token, paramlist);
            ret->val.str = name;
            return ret;
        } else { // constant
            Token token = S->token;
            ret = EXP0(AST_IDENTIFIER, token);
            ret->val.str = name;
            return ret;
        }
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

    parseerror(S, "Unexpected token '%s', expected primary", get_token_name(S->token.type));
    return NULL;
}


static AST* parse_expr(Lexer* S)
{
    AST* ret = parse_primary(S);
    Token token = S->token;

    if (accept(S, '.')) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = '.';
    }
    if (accept(S, '+')) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = '+';
    }
    if (accept(S, '-')) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = '-';
    }
    if (accept(S, '*')) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = '*';
    }
    if (accept(S, '/')) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = '/';
    }
    if (accept(S, TK_SHL)) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = TK_SHL;
    }
    if (accept(S, TK_SHR)) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = TK_SHR;
    }

    if (accept(S, TK_AND)) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = TK_AND;
    }
    if (accept(S, TK_OR)) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = TK_OR;
    }

    if (accept(S, TK_EQ)) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = TK_EQ;
    }
    if (accept(S, '<')) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = '<';
    }
    if (accept(S, '>')) {
        ret = EXP2(AST_BINOP, token, parse_expr(S), ret);
        ret->val.lint = '<';
    }
    if (accept(S, TK_LTEQ)) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = TK_LTEQ;
    }
    if (accept(S, TK_GTEQ)) {
        ret = EXP2(AST_BINOP, token, ret, parse_expr(S));
        ret->val.lint = TK_GTEQ;
    }

    if (accept(S, TK_PLUSPLUS)) {
        ret = EXP1(AST_POSTFIXOP, token, ret);
        ret->val.lint = '+';
    }
    if (accept(S, TK_MINUSMINUS)) {
        ret = EXP1(AST_POSTFIXOP, token, ret);
        ret->val.lint = '-';
    }

    return ret;
}

static AST* parse_echostmt(Lexer* S)
{
    Token token = S->token;
    AST* expr = parse_expr(S);

    expect(S, ';');

    AST* ret = EXP1(AST_ECHO, token, expr);
    return ret;
}

static AST* parse_const(Lexer* S) 
{
    Token token = S->token;
    AST* identifier = parse_identifier(S);
    expect(S, '=');
    AST* expr = parse_expr(S);
    expect(S, ';');

    return EXP2(AST_CONSTDECL, token, identifier, expr);
}

static AST* parse_blockstmt(Lexer* S)
{
    AST* ret = EXP0(AST_LIST, S->token);
    while (S->token.type != '}') {
        ast_list_append(ret, parse_stmt(S));
    }
    expect(S, '}');

    return ret;
}

static AST* parse_ifstmt(Lexer* S)
{
    Token token = S->token;
    expect(S, '(');
    AST* expr = parse_expr(S);
    expect(S, ')');

    AST* body = parse_stmt(S);

    AST* else_body = NULL;
    if (accept(S, TK_ELSE)) {
        else_body = parse_stmt(S);
    }

    AST* ret = EXP3(AST_IF, token, expr, body, else_body);
    return ret;
}

static AST* parse_whilestmt(Lexer* S)
{
    Token token = S->token;
    expect(S, '(');
    AST* expr = parse_expr(S);
    expect(S, ')');

    AST* body = parse_stmt(S);

    return EXP2(AST_WHILE, token, expr, body);
}

static AST* parse_forstmt(Lexer* S)
{
    Token token = S->token;
    expect(S, '(');
    AST* init = parse_expr(S);
    expect(S, ';');
    AST* condition = parse_expr(S);
    expect(S, ';');
    AST* post = parse_expr(S);
    expect(S, ')');

    AST* body = parse_stmt(S);
    return EXP4(AST_FOR, token, init, condition, post, body);
}

static AST* parse_identifier(Lexer* S)
{
    // This function is used so we can get better line numbers for error messages
    if (S->token.type != TK_IDENTIFIER) {
        expect(S, TK_IDENTIFIER); // Raise error
        return NULL;
    }

    AST* ret = EXP0(AST_IDENTIFIER, S->token);
    ret->val.str = overtake_str(S);
    expect(S, TK_IDENTIFIER); // Skip

    return ret;
}

static AST* parse_arglist(Lexer* S)
{
    AST* ret = EXP0(AST_LIST, S->token);

    if (S->token.type == ')') {
        return ret;
    }

    while (true) {
        if (S->token.type == TK_VAR) {
            AST* arg = EXP0(AST_ARGUMENT, S->token);
            arg->val.str = overtake_str(S);
            ast_list_append(ret, arg);
        }
        expect(S, TK_VAR);

        if (S->token.type != ',') {
            break;
        }
        get_next_token(S);
    }

    return ret;
}

static AST* parse_paramlist(Lexer* S)
{
    AST* ret = EXP0(AST_LIST, S->token);

    if (S->token.type == ')') {
        return ret;
    }

    while (true) {
        ast_list_append(ret, parse_expr(S));

        if (S->token.type != ',') {
            break;
        }
        get_next_token(S);
    }

    return ret;
}

static AST* parse_function(Lexer* S)
{
    if (S->token.type != TK_IDENTIFIER) {
        parseerror(S, "Expected IDENTIFIER, %s given.", get_token_name(S->token.type));
        return NULL;
    }
    Token token = S->token;
    AST* name = parse_identifier(S);
    expect(S, '(');
    AST* parameters = parse_arglist(S);
    expect(S, ')');
    AST* body = parse_stmt(S);

    AST* ret = EXP3(AST_FUNCTION, token, name, parameters, body);

    return ret;
}

AST* parse(FILE* file)
{
    Lexer* S = create_lexer(file);
    AST* ret = EXP0(AST_LIST, S->token);
    get_next_token(S); // init
    while (S->token.type != TK_END) {
        if (S->token.type == TK_HTML) {
            AST* html = EXP0(AST_HTML, S->token);
            html->val.str = S->u.string;
            ast_list_append(ret, html);
        }
        if (S->mode != PHP || S->token.type == TK_OPENTAG) {
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
    printf("%p %s (%d): ", ast, get_ASTTYPE_name(ast->type), ast->lineno);

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