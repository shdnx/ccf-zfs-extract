#pragma once

#include <cstdint>

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using size_t = std::size_t;

#define KB (1024uL)
#define MB (KB * 1024uL)
#define GB (MB * 1024uL)
#define TB (GB * 1024uL)

#define OUT   /* empty */
#define INOUT /* empty */

void _assert_fail(const char *cond, const char *func, const char *file,
                  int line, const char *format, ...) __attribute__((noreturn));

void _unreachable_fail(const char *func, const char *file, int line,
                       const char *format, ...) __attribute__((noreturn));

// ASSERT(condition, message format, message args...)
#define ASSERT(COND, ...)                                          \
  do {                                                             \
    if (!(COND))                                                   \
      _assert_fail(#COND, __PRETTY_FUNCTION__, __FILE__, __LINE__, \
                   __VA_ARGS__);                                   \
  } while (0)

#define ASSERT0(COND) ASSERT(COND, "C'thulu has arisen (sanity check failure)")

// UNREACHABLE(message format, message args...)
#define UNREACHABLE(...)                                                     \
  do {                                                                       \
    _unreachable_fail(__PRETTY_FUNCTION__, __FILE__, __LINE__, __VA_ARGS__); \
    __builtin_unreachable();                                                 \
  } while (0)

// When you REALLY think you know what you're doing, such as turning a bunny
// into a refrigator.
#define REINTERPRET(WHAT, TYPE) (*reinterpret_cast<TYPE *>(&(WHAT)))

// Needed when you want to pass something containing commas as a single macro
// argument.
#define REFL(...) __VA_ARGS__

#define _CONCAT2_IMPL(A, B) A##B
#define CONCAT2(A, B) _CONCAT2_IMPL(A, B)

#define _CONCAT3_IMPL(A, B, C) A##B##C
#define CONCAT3(A, B, C) _CONCAT3_IMPL(A, B, C)

#define PADDING(NBYTES) u8 CONCAT2(_pad, __COUNTER__)[NBYTES]

#define flag_isset(BITSET, FLAG) \
  ((static_cast<u64>(BITSET) & static_cast<u64>(FLAG)) != 0)
