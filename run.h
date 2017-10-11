#ifndef PHPINTERP_RUN_H
#define PHPINTERP_RUN_H

#include <stdio.h>
#include <stdint.h>
#include "crossplatform/stdnoreturn.h"
#include "stack.h"


_Noreturn void runtimeerror(char* fmt);
_Noreturn void raise_fatal(Runtime* R, char*, ...);
void run_file(const char*);
void run_function(Runtime*, Function*);
Variant cpy_var(Variant var);
void free_var(Variant var);

void print_stack(Runtime*);

#endif //PHPINTERP_RUN_H
