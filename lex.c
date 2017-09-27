#include <stdlib.h>
#include <ctype.h>
#include <memory.h>
#include <assert.h>
#include <stdbool.h>
#include "lex.h"


static char* resize_str(char* str, size_t previous_size, size_t new_size)
{
    assert(new_size > previous_size);
    char* tmp = realloc(str, new_size);
    if (!tmp) {
        perror("resizing str failed.");
        free(str);
        return NULL;
    }
    memset(&tmp[previous_size], 0, new_size - 1 - previous_size);

    return tmp;
}

static int is_not_open_tag(int c)
{
    return c != '<';
}


static int is_whitespace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int get_next_char(State* S)
{
    S->lexchar = fgetc(S->file);
    if (S->lexchar == '\n') {
        S->lineno++;
    }

    return S->lexchar;
}

static inline int is_ident_start(int c)
{
    return isalpha(c);
}

static inline int is_str_start(int c)
{
    return c == '"';
}

static inline int is_num_start(int c)
{
    return isdigit(c);
}

static inline int is_var_start(int c)
{
    return c == '$';
}

static inline int is_str(int c)
{
    return isalnum(c) || c == '_';
}

static TOKEN syntax_error(State* S, char* fmt, ...)
{
    va_list ap;
    char msgbuf[256];
    va_start(ap, fmt);
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
    va_end(ap);

    char buf[256];
    snprintf(buf, sizeof(buf), "%s on line %d.", msgbuf, S->lineno);

    S->val = ERROR;
    S->error = strdup(buf);
    puts(S->error);

    abort();
}


static char* str_append(char* str, char append, size_t* size, size_t* capacity)
{
    assert(capacity != NULL && size != NULL);
    if (*size >= *capacity) {
        char* tmp = resize_str(str, *capacity, *capacity * 2);
        if (!tmp) return NULL;
        *capacity *= 2;
        str = tmp;
    }

    str[(*size)++] = append;

    return str;
}

static char* fetch_str(State* S, int (*proceed_condition)(int))
{
    size_t pos = 0;
    size_t capacity = 5;
    char* ret = calloc(capacity, sizeof(char));
    while (proceed_condition(S->lexchar)) {
        ret = str_append(ret, (char) S->lexchar, &pos, &capacity);
        get_next_char(S);
    }

    ret = str_append(ret, '\0', &pos, &capacity);

    return ret;
}


static int lex_ident(State* S)
{
    char* str = fetch_str(S, is_str);
    if (!str) {
        return syntax_error(S, "Fetching str failed.");
    }

    TOKEN ret;
    if (strcmp(str, "echo") == 0) {
        ret = TK_ECHO;
    } else if (strcmp(str, "function") == 0) {
        ret = TK_FUNCTION;
    } else if (strcmp(str, "if") == 0) {
        ret = TK_IF;
    } else if (strcmp(str, "else") == 0) {
        ret = TK_ELSE;
    } else if (strcmp(str, "true") == 0) {
        ret = TK_TRUE;
    } else if (strcmp(str, "false") == 0) {
        ret = TK_FALSE;
    } else if (strcmp(str, "while") == 0) {
        ret = TK_WHILE;
    } else if (strcmp(str, "for") == 0) {
        ret = TK_FOR;
    } else {
        char* msg = malloc((strlen("Unknown identifier '%s'") + strlen(str)) * sizeof(char));
        sprintf(msg, "Unknown identifier '%s'", str);
        ret = syntax_error(S, msg);
        free(msg);
    }

    free(str);
    return ret;
}

static TOKEN lex_var(State* S)
{
    assert(is_var_start(S->lexchar));
    get_next_char(S); // Skip $
    char* str = fetch_str(S, is_str);
    state_set_string(S, str);

    return TK_VAR;
}

static TOKEN lex_str(State* S)
{
    assert(S->lexchar == '"');
    int escape = false;
    size_t pos = 0;
    size_t capacity = 5;
    char* str = calloc(capacity, sizeof(char));
    int c = get_next_char(S);
    while ((c != '"' || escape) && c != EOF) {
        if (S->lexchar == '\\') {
            escape = true;
        } else {
            if (escape) {
                switch (S->lexchar) {
                    case 'n':
                        S->lexchar = '\n';
                        break;
                    case 't':
                        S->lexchar = '\t';
                        break;
                    case 'r':
                        S->lexchar = '\r';
                        break;
                    default:
                        str = str_append(str, '\\', &pos, &capacity);
                        break;
                }
            }
            str = str_append(str, (char) S->lexchar, &pos, &capacity);
            escape = false;
        }
        c = get_next_char(S);
    }

    str = str_append(str, '\0', &pos, &capacity);

    if (S->lexchar != '"') {
        char errormsg[strlen("Expected \", got ")+4];
        if (S->lexchar == EOF) {
            sprintf(errormsg, "Expected \", got %s", "EOF");
        } else {
            sprintf(errormsg, "Expected \", got %c", S->lexchar);
        }
        free(str);
        return syntax_error(S, errormsg);
    }

    state_set_string(S, str);

    get_next_char(S); // Skip "

    return TK_STRING;
}

int lex_num(State* S)
{
    char* numstr = fetch_str(S, isdigit);
    int64_t n = (int64_t) strtoll(numstr, NULL, 10);
    free(numstr);
    state_set_long(S, n);

    return TK_LONG;
}

#define LEX_TWICE(current, expected, TOKEN)                                    \
    if ((current) == (expected)) {                                             \
        if ((expected) == get_next_char(S)) {                                  \
            get_next_char(S);                                                  \
            return TOKEN;                                                      \
        } else                                                                 \
            return (expected);                                                 \
    }

int get_token(State* S)
{
    if (S->lexchar == EOF) {
        return TK_END;
    }

    if (S->mode == NONPHP) {
        char* html = fetch_str(S, is_not_open_tag);
        if (get_next_char(S) != '?') {
            free(html);
            return syntax_error(S, "Expected ? after <.");
        }
        int c1 = get_next_char(S);
        int c2 = get_next_char(S);
        int c3 = get_next_char(S);
        if (c1 != 'p' || c2 != 'h' || c3 != 'p') {
            free(html);
            return syntax_error(S, "Expected php after <?");
        }

        S->mode = EMITOPENTAG;
        get_next_char(S); // Skip over last p
        if (*html == '\0') { // We do not need to emit an empty str
            free(html);
            return get_token(S);
        }
        S->u.string = html;
        return TK_HTML;
    } else if (S->mode == EMITOPENTAG) {
        S->mode = PHP;
        return TK_OPENTAG;
    }

    int c = S->lexchar;
    while (is_whitespace(c)) {
        c = get_next_char(S); // Consume whitespace
        if (c == EOF) {
            return TK_END;
        }
    }

    if (is_ident_start(c)) {
        return lex_ident(S);
    }

    if (is_var_start(c)) {
        return lex_var(S);
    }

    if (is_str_start(c)) {
        return lex_str(S);
    }

    if (is_num_start(c)) {
        return lex_num(S);
    }

    LEX_TWICE(c, '&', TK_AND);
    LEX_TWICE(c, '|', TK_OR);
    LEX_TWICE(c, '=', TK_EQ);
    LEX_TWICE(c, '+', TK_PLUSPLUS);
    LEX_TWICE(c, '-', TK_MINUSMINUS);

    if (c == '<') {
        c = get_next_char(S);
        if (c == '=') {
            get_next_char(S);
            return TK_LTEQ;
        } else if (c == '<') {
            get_next_char(S);
            return TK_SHL;
        }

        return '<';
    }

    if (c == '>') {
        c = get_next_char(S);
        if (c == '=') {
            get_next_char(S);
            return TK_GTEQ;
        } else if (c == '>') {
            get_next_char(S);
            return TK_SHR;
        }

        return '>';
    }

    if (c == '/') {
        if ('/' == get_next_char(S)) {
            while (get_next_char(S) != '\n')
                ;
            S->lineno++;
            return get_token(S);
        } else {
            return '/';
        }
    }

    get_next_char(S);
    return c;
}

State* new_state(FILE* file)
{
    State* ret = malloc(sizeof(State));
    if (!ret) {
        perror("malloc for state failed.");
        return NULL;
    }
    ret->val = NONE;
    ret->lineno = 1;
    ret->mode = NONPHP;
    ret->file = file;
    get_next_char(ret);
    return ret;
}

int destroy_state(State* S)
{
    fclose(S->file);
    if (S->val == MALLOCSTR) {
        free(S->u.string);
    }
    free(S);

    return 1;
}

static char* tokennames[] = {
    "EOF",
    "'\\x01'", "'\\x02'", "'\\x03'", "'\\x04'", "'\\x05'", "'\\x06'", "'\\x07'",
    "'\\x08'", "'\\x09'", "'\\x0A'", "'\\x0B'", "'\\x0C'", "'\\x0D'", "'\\x0E'", "'\\x0F'",
    "'\\x10'", "'\\x11'", "'\\x12'", "'\\x13'", "'\\x14'", "'\\x15'", "'\\x16'", "'\\x17'",
    "'\\x18'", "'\\x19'", "'\\x1A'", "'\\x1B'", "'\\x1C'", "'\\x1D'", "'\\x1E'", "'\\x1F'",

    "' '", "'!'", "'\"'", "'#'", "'$'", "'%'", "'&'", "'\\''",
    "'('", "')'", "'*'", "'+'", "','", "'-'", "'.'", "'/'",
    "'0'", "'1'", "'2'", "'3'", "'4'", "'5'", "'6'", "'7'",
    "'8'", "'9'", "':'", "';'", "'<'", "'='", "'>'", "'?'",
    "'@'", "'A'", "'B'", "'C'", "'D'", "'E'", "'F'", "'G'",
    "'H'", "'I'", "'J'", "'K'", "'L'", "'M'", "'N'", "'O'",
    "'P'", "'Q'", "'R'", "'S'", "'T'", "'U'", "'V'", "'W'",
    "'X'", "'Y'", "'Z'", "'['", "'\'", "']'", "'^'", "'_'",
    "'`'", "'a'", "'b'", "'c'", "'d'", "'e'", "'f'", "'g'",
    "'h'", "'i'", "'j'", "'k'", "'l'", "'m'", "'n'", "'o'",
    "'p'", "'q'", "'r'", "'s'", "'t'", "'u'", "'v'", "'w'",
    "'x'", "'y'", "'z'", "'{'", "'|'", "'}'", "'~'", "'\\x7F'",

    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, // Ends with 255

    "OPENTAG", "ECHO", "STRING", "LONG", "FUNCTION", "IF", "ELSE",
    "TRUE", "FALSE", "VAR",
    "AND", "OR", "EQ", "LTEQ", "GTEQ",
    "WHILE", "FOR",
    "++", "--", "<<", ">>",
    "HTML", "END"
};

char* get_token_name(int tok)
{
    if (tok >= 0 && tok < (int)arrcount(tokennames)) {
        if (tokennames[tok]) {
            return tokennames[tok];
        }
    }
    return "unknown";
}

void print_tokenstream(State* S)
{
    int tok;
    while ((tok = get_token(S)) != TK_END) {
        char* name = get_token_name(tok);
        if (name) {
            puts(name);
        } else {
            putchar(tok);
        }
    }
}