#ifndef PHPINTERP_OPTIMIZE_H
#define PHPINTERP_OPTIMIZE_H

#include "stack.h"

typedef void (Optimizer(Stack*, size_t));

void optimize_function(Function* fn);

#endif //PHPINTERP_OPTIMIZE_H
