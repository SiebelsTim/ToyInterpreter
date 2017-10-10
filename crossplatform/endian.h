#ifdef _WIN32
// FIXME: Do not assume windows uses little endian
# define htole16(x) x
# define htole32(x) x
# define htole64(x) x

# define le16toh(x) x
# define le32toh(x) x
# define le64toh(x) x
#else
# include <endian.h>
#endif