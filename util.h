#ifndef PHPINTERP_UTIL_H
#define PHPINTERP_UTIL_H

#define arrcount(arr) (sizeof(arr) / sizeof((arr)[0]))

char* escaped_str(char* dest, const char* str);

#endif //PHPINTERP_UTIL_H
