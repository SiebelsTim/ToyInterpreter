#ifdef __linux
# include <stdnoreturn.h>
# define _Noreturn noreturn
#else
# define _Noreturn __declspec(noreturn)
#endif