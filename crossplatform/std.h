#ifdef _WIN32
# define _Static_assert(expr, msg) _STATIC_ASSERT(expr)
# define strdup _strdup
#endif