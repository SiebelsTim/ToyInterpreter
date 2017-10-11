#ifndef PHPINTERP_STD_H
#define PHPINTERP_STD_H


#include "../stack.h"

typedef struct CFunctionPair {
    char* name;
    CFunction* cfunction;
} CFunctionPair;

void builtin_gettype(Runtime* R);

static CFunctionPair std_functions[] = {
        {"gettype", &builtin_gettype}
};

#endif //PHPINTERP_STD_H
