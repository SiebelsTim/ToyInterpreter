#ifndef PHPINTERP_RUN_H
#define PHPINTERP_RUN_H

#include <stdio.h>
#include <stdint.h>
#include "enum-util.h"
#include "compile.h"
#include "crossplatform/stdnoreturn.h"

#define ENUM_VARIANTTYPE(ELEMENT)   \
    ELEMENT(TYPE_UNDEF, =0)         \
    ELEMENT(TYPE_STRING,)           \
    ELEMENT(TYPE_LONG,)             \
    ELEMENT(TYPE_BOOL,)             \
    ELEMENT(TYPE_NULL,)             \
    ELEMENT(TYPE_CFUNCTION,)        \
    ELEMENT(TYPE_FUNCTION,)         \
    ELEMENT(TYPE_MAX_VALUE,)

DECLARE_ENUM(VARIANTTYPE, ENUM_VARIANTTYPE);

typedef struct Variant {
    VARIANTTYPE type;
    union {
        char* str;
        int64_t lint;
        bool boolean;
        Function* function;
        CFunction* cfunction;
    } u;
} Variant;

typedef struct Variable {
    char* name;
    Variant value;
} Variable;

typedef struct Scope Scope;

typedef struct Runtime {
    size_t stacksize;
    size_t stackcapacity;
    codepoint_t* ip;
    Variant* stack;
    Scope* scope;
    State* state; // non-owning ptr

    size_t line;
    char* file;
} Runtime;

void run_file(const char*);
void run_function(Runtime*, Function*);
_Noreturn void runtimeerror(char* );
Variant cpy_var(Variant var);
void free_var(Variant var);

void print_stack(Runtime*);

#endif //PHPINTERP_RUN_H
