#ifndef PHPINTERP_STACK_H
#define PHPINTERP_STACK_H

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include "crossplatform/stdnoreturn.h"
#include "enum-util.h"
#include "array-util.h"

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

typedef struct State State;
typedef struct Scope Scope;
typedef struct Function Function;
typedef struct Runtime Runtime;
typedef void (CFunction(Runtime*));

typedef uint8_t codepoint_t;

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


typedef struct Runtime {
    size_t stacksize;
    size_t stackcapacity;
    Function* function;
    codepoint_t* ip;
    Variant* stack;
    Scope* scope;
    State* state; // non-owning ptr
    bool hasError;

    char* file;
} Runtime;


static _Noreturn void die(char* msg) {
    puts(msg);
    abort();
}
static inline void try_stack_resize(Runtime* R)
{
    try_resize(&R->stackcapacity, R->stacksize, (void*)&R->stack,
               sizeof(*R->stack), die);
}

Variant* stackidx(Runtime* R, long idx);
void push(Runtime* R, Variant val);

static inline Variant* top(Runtime* R)
{
    assert(R->stacksize >= 1);
    return stackidx(R, -1);
}

void popn(Runtime *R, size_t count);

static inline void pop(Runtime* R) {
    popn(R, 1);
}

void pushstr(Runtime* R, char* str);


void pushlong(Runtime* R, int64_t n);
void pushbool(Runtime* R, bool b);
void pushnull(Runtime* R);
void pushfunction(Runtime* R, Function* fn);
void pushcfunction(Runtime* R, CFunction* fn);

char* vartostring(Variant var);
char* tostring(Runtime* R, int idx);

int64_t vartolong(Variant var);
int64_t tolong(Runtime* R, int idx);

bool vartobool(Variant var);
bool tobool(Runtime* R, int idx);

Variant vartotype(Variant var, VARIANTTYPE type);

#endif //PHPINTERP_STACK_H
