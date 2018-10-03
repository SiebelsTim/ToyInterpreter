#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "crossplatform/std.h"
#include "stack.h"
#include "run.h"


DEFINE_ENUM(VARIANTTYPE, ENUM_VARIANTTYPE);

Variant* stackidx(Runtime* R, long idx)
{
    if (idx < 0) {
        idx = R->stacksize + idx;
    }

    assert(idx < (int)R->stacksize && idx >= 0);
    return &R->stack[idx];
}


void push(Runtime* R, Variant val)
{
    try_stack_resize(R);
    R->stack[R->stacksize++] = cpy_var(val);
}

void popn(Runtime *R, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        Variant* ret = top(R);

        free_var(*ret);

        assert(R->stacksize >= 1);
        R->stacksize--;
    }
}

void pushstr(Runtime* R, char* str)
{
    Variant var;
    var.type = TYPE_STRING;
    var.u.str = str;
    push(R, var);
}


void pushlong(Runtime* R, int64_t n)
{
    Variant var;
    var.type = TYPE_LONG;
    var.u.lint = n;
    push(R, var);
}

void pushbool(Runtime* R, bool b)
{
    Variant var;
    var.type = TYPE_BOOL;
    var.u.boolean = b;
    push(R, var);
}

void pushnull(Runtime* R)
{
    Variant var;
    var.type = TYPE_NULL;
    push(R, var);
}

void pushfunction(Runtime* R, Function* fn)
{
    Variant var;
    var.type = TYPE_FUNCTION;
    var.u.function = fn;
    push(R, var);
}

void pushcfunction(Runtime* R, CFunction* fn)
{
    Variant var;
    var.type = TYPE_CFUNCTION;
    var.u.cfunction = fn;
    push(R, var);
}

char* vartostring(Variant var)
{
    char* buf;
    switch (var.type) {
        case TYPE_STRING:
            assert(var.u.str);
            return strdup(var.u.str);
        case TYPE_LONG:
            buf = malloc(sizeof(char) * 20); // Let this be "enough"
            snprintf(buf, sizeof(char) * 20, "%" PRId64, var.u.lint);
            return buf;
        case TYPE_UNDEF:
            return strdup("<UNDEFINED>");
        case TYPE_NULL:
            return strdup("<null>");
        case TYPE_BOOL:
            return var.u.boolean ? strdup("1") : strdup("");
        case TYPE_FUNCTION:
        case TYPE_CFUNCTION:
            return strdup("function");
        case TYPE_MAX_VALUE:
            assert(false && "Undefined Type given");
            break;
    }

    die("Assertion failed: May not reach end of tostring function.");
    return NULL;
}

char* tostring(Runtime* R, int idx)
{
    return vartostring(*stackidx(R, idx));
}

int64_t vartolong(Variant var)
{
    long long ll = 0;
    switch (var.type) {
        case TYPE_UNDEF:
        case TYPE_NULL:
            return 0;
        case TYPE_BOOL:
            return var.u.boolean;
        case TYPE_STRING:
            ll = strtoll(var.u.str, NULL, 10);
            int64_t lint = (int64_t) ll;
            assert(ll == lint);
            return lint;
        case TYPE_LONG:
            return var.u.lint;
        case TYPE_FUNCTION:
        case TYPE_CFUNCTION:
            return 0;
        case TYPE_MAX_VALUE:
            assert(false && "Undefined Type given");
            break;
    }

    die("tolong for undefined value.");
    return 0;
}

int64_t tolong(Runtime* R, int idx)
{
    return vartolong(*stackidx(R, idx));
}

bool vartobool(Variant var)
{
    switch (var.type) {
        case TYPE_UNDEF:
        case TYPE_NULL:
            return false;
        case TYPE_LONG:
            return var.u.lint != 0;
        case TYPE_BOOL:
            return var.u.boolean;
        case TYPE_STRING:
            if (var.u.str[0] == '\0' || (var.u.str[0] == '0' && var.u.str[1] == '\0')) {
                return false;
            } else {
                return true;
            }
        case TYPE_FUNCTION:
        case TYPE_CFUNCTION:
            return false;
        case TYPE_MAX_VALUE:
            assert(false && "Undefined Type given");
            break;
    }

    assert(false);
    return false;
}

bool tobool(Runtime* R, int idx)
{
    return vartobool(*stackidx(R, idx));
}

Variant vartotype(Variant var, VARIANTTYPE type)
{
    if (var.type == type) {
        return cpy_var(var);
    }

    Variant ret = {.type = TYPE_UNDEF, .u.lint = 0};
    switch (type) {
        case TYPE_STRING:
            ret.type = TYPE_STRING;
            ret.u.str = vartostring(var);
            break;
        case TYPE_LONG:
            ret.type = TYPE_LONG;
            ret.u.lint = vartolong(var);
            break;
        case TYPE_NULL:
            ret.type = TYPE_NULL;
            break;
        case TYPE_UNDEF:
            ret.type = TYPE_UNDEF;
            break;
        case TYPE_BOOL:
            ret.type = TYPE_BOOL;
            ret.u.boolean = vartobool(var);
            break;
        case TYPE_FUNCTION:
        case TYPE_CFUNCTION:
            die("Cannot convert function");
            break;
        case TYPE_MAX_VALUE:
            assert(false && "Undefined Type given");
            break;
    }

    return ret;
}