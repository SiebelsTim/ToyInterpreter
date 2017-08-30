//
// Created by tim on 29.08.17.
//

#ifndef PHPINTERP_RUN_H
#define PHPINTERP_RUN_H

#include <stdio.h>

enum VARIANTTYPE {
    UNDEFINED = 0,
    STRING,
    LONG
};

typedef struct Variant {
    enum VARIANTTYPE type;
    union {
        char* str;
        long lint;
    } u;
} Variant;

typedef struct Runtime {
    size_t stacksize;
    size_t stackcapacity;
    Variant* stack;
} Runtime;

void run_file(FILE*);

#endif //PHPINTERP_RUN_H
