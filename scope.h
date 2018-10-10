#ifndef PHPINTERP_SCOPE_H
#define PHPINTERP_SCOPE_H

#include "run.h"


typedef struct Scope {
    size_t capacity;
    size_t size;
    Variable* vars;
    struct Scope* parent;
} Scope;

Scope* create_scope();
void destroy_scope(Scope*);
Variant lookup(Runtime* R, char* str);
Variant lookupWithFlags(Runtime* R, char* str, int flags);
Variable* set_var(Runtime* R, char* str, Variant var, int flags);

#endif //PHPINTERP_SCOPE_H
