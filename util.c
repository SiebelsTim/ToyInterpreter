#include <memory.h>
#include <assert.h>
#include "util.h"

static char escape_chars[] = {
        '0', 0, 0, 0, 0, 0, 0, 0, // 07
        0, 't', 'n', 0, 0, 'r', 0, 0, // 017
        0, 0, 0, 0, 0, 0, 0, 0, // 027
        0, 0, 0, 0, 0, 0, 0
};

char* escaped_str(char* dest, const char* str)
{
    assert(dest && "dest == NULL");
    _Static_assert(arrcount(escape_chars) == 0x1f, "Not enough escape chars");
    size_t pos = 0;
    while (*str != '\0') {
        if (*str < 0x20 && escape_chars[(unsigned char)*str] != 0) { // First 0x20 chars are special chars
            dest[pos++] = '\\';
            dest[pos++] = escape_chars[(unsigned char)*str];
        } else {
            dest[pos++] = *str;
        }
        str++;
    }
    dest[pos++] = '\0';
    return dest;
}