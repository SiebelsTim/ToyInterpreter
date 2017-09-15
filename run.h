#ifndef PHPINTERP_RUN_H
#define PHPINTERP_RUN_H

#include <stdio.h>
#include <stdint.h>

enum VARIANTTYPE {
    UNDEFINED = 0,
    STRING,
    LONG
};

typedef struct Variant {
    enum VARIANTTYPE type;
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
    Variant* stack;
    Scope* scope;
} Runtime;

void run_file(FILE*);
void runtimeerror(char* );
Variant cpy_var(Variant var);
void free_var(Variant var);

void print_stack(Runtime*);

#endif //PHPINTERP_RUN_H
