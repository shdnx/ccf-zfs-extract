#pragma once

#include <cassert>
#include <cstdint>

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

#define KB (1024uL)
#define MB (KB * 1024uL)
#define GB (MB * 1024uL)
#define TB (GB * 1024uL)

extern "C" void exit(int code) throw() __attribute__((noreturn));

#define OUT /* empty */

#define CRITICAL(COND, ...)                                     \
  do {                                                          \
    if (!(COND)) {                                              \
      std::fprintf(stderr, "Critical failure in %s at %s:%d: ", \
                   __PRETTY_FUNCTION__, __FILE__, __LINE__);    \
      std::fprintf(stderr, __VA_ARGS__);                        \
      exit(1);                                                  \
    }                                                           \
  } while (0)

#define REINTERPRET(WHAT, TYPE) (*reinterpret_cast<TYPE *>(&(WHAT)))

#define _CONCAT2_IMPL(A, B) A##B
#define CONCAT2(A, B) _CONCAT2_IMPL(A, B)

#define _CONCAT3_IMPL(A, B, C) A##B##C
#define CONCAT3(A, B, C) _CONCAT3_IMPL(A, B, C)

#define _PADDING_IMPL(NAME, NBYTES) u8 NAME[NBYTES]
#define PADDING(NBYTES) _PADDING_IMPL(CONCAT2(_pad, __COUNTER__), NBYTES)
