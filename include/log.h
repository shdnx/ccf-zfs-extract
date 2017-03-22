#pragma once

#ifdef NDEBUG
#define LOG(...) /* empty */
#else
#include <cstdio>
#define LOG(...) std::fprintf(stderr, __VA_ARGS__)
#endif
