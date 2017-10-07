#ifndef PHPINTERP_RUN_H
#define PHPINTERP_RUN_H

#include <stdio.h>
#include <stdint.h>
#include "enum-util.h"
#include "compile.h"

#define ENUM_VARIANTTYPE(ELEMENT) \
    ELEMENT(TYPE_UNDEF, =0)         \
    ELEMENT(TYPE_STRING,)               \
    ELEMENT(TYPE_LONG,)                 \
    ELEMENT(TYPE_NULL,)

DECLARE_ENUM(VARIANTTYPE, ENUM_VARIANTTYPE);


typedef struct Variant {
    VARIANTTYPE type;
    union {
        char* str;
        int64_t lint;
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
    Operator* ip;
    Variant* stack;
    Scope* scope;
} Runtime;

void run_file(FILE*);
void run_function(Runtime*, Function*);
void runtimeerror(char* );
Variant cpy_var(Variant var);
void free_var(Variant var);

void print_stack(Runtime*);

#endif //PHPINTERP_RUN_H
