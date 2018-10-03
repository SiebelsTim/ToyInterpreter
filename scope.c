#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include "scope.h"
#include "array-util.h"

static const Variant undefined = {.type = TYPE_UNDEF};

Scope* create_scope()
{
    Scope* ret = malloc(sizeof(Scope));
    ret->capacity = 5;
    ret->size = 0;
    ret->vars = calloc(ret->capacity, sizeof(*ret->vars));
    ret->parent = NULL;

    return ret;
}

void destroy_scope(Scope* scope)
{
    for (size_t i = 0; i < scope->size; ++i) {
        free(scope->vars[i].name);
        free_var(scope->vars[i].value);
    }
    free(scope->vars);
    free(scope);
}

Variant lookup(Runtime *R, char *str)
{
    Variable* vars = R->scope->vars;
    for (size_t i = 0; i < R->scope->size; ++i) {
        if (strcmp(str, vars[i].name) == 0) {
            return vars[i].value;
        }
    }

    return undefined;
}

static void try_vars_resize(Scope* scope)
{
    try_resize(&scope->capacity, scope->size, (void*)&scope->vars,
               sizeof(*scope->vars), die);
}

Variable* set_var(Runtime* R, char* str, Variant var)
{
    Variable* vars = R->scope->vars;
    // existing variable
    for (size_t i = 0; i < R->scope->size; ++i) {
        if (strcmp(str, vars[i].name) == 0) {
            free_var(vars[i].value);
            vars[i].value = cpy_var(var);
            return &vars[i];
        }
    }

    try_vars_resize(R->scope);
    const size_t idx = R->scope->size++;
    vars[idx].name = strdup(str);
    vars[idx].value = cpy_var(var);

    return &vars[idx];
}