#ifndef PHPINTERP_ARRAY_UTIL_H
#define PHPINTERP_ARRAY_UTIL_H

#include <stddef.h>
#include <stdlib.h>
#include <memory.h>

static void try_resize(size_t* capacity, size_t size, void** ptr,
                       size_t elemsize, void (*error)(char*))
{
    if (*capacity < size + 1) {
        *capacity *= 2;
        void* tmp = realloc(*ptr, elemsize * (*capacity));
        if (!tmp) {
            error("Out of memory");
            return;
        }

        *ptr = tmp;
    }
}

#endif //PHPINTERP_ARRAY_UTIL_H
