#ifndef PHPINTERP_ENUM_UTIL_H
#define PHPINTERP_ENUM_UTIL_H

#define ENUM_ELEMENT(x, assign) x assign,
#define ENUM_CASEELEMENT(x, assign) case x: return #x;

#define DECLARE_ENUM(NAME, ENUM) typedef enum NAME { ENUM(ENUM_ELEMENT) } NAME; \
    const char* get_##NAME##_name(NAME);
#define DEFINE_ENUM(NAME, ENUM) const char* get_##NAME##_name(NAME val) {\
  switch (val) {\
    ENUM(ENUM_CASEELEMENT)\
	default: \
		assert(false); \
  } \
}

#endif // PHPINTERP_ENUM_UTIL_H
