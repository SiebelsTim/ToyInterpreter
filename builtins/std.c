#include "std.h"
#include "../run.h"


void builtin_gettype(Runtime* R)
{
    if (R->stacksize != 1) {
        raise_fatal(R, "Expected 1 argument, %d given.", R->stacksize);
        return;
    }

    Variant* var = top(R);

    char* typename = NULL;
    switch (var->type) {
        case TYPE_UNDEF:
        case TYPE_MAX_VALUE:
            assert(false);
        case TYPE_CFUNCTION:
        case TYPE_FUNCTION:
            typename = "function";
            break;
        case TYPE_STRING:
            typename = "string";
            break;
        case TYPE_LONG:
            typename = "integer";
            break;
        case TYPE_BOOL:
            typename = "boolean";
            break;
        case TYPE_NULL:
            typename = "NULL";
            break;
    }

    pushstr(R, typename);
}