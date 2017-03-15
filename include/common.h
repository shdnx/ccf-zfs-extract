#pragma once

#include <cstdint>

extern "C" void exit(int code) throw() __attribute__((noreturn));

#define OUT /* empty */

#define CRITICAL(COND, ...) do { \
    if (!(COND)) { \
      std::fprintf(stderr, "Critical failure in %s at %s:%d: ", __PRETTY_FUNCTION__, __FILE__, __LINE__); \
      std::fprintf(stderr, __VA_ARGS__); \
      exit(1); \
    } \
  } while (0)

#define REINTERPRET(WHAT, TYPE) (*reinterpret_cast<TYPE *>(&(WHAT)))
