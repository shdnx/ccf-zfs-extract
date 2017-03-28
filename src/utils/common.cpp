#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "common.h"

// TODO: make a nicer, more robust solution - also read the value of the TERM
// environment variable to figure out if colors are even supported
#define FCODE(CODE) "\033[" #CODE "m"
#define FCODE_ALIAS(NAME, CODE) \
  static void NAME() { std::fprintf(stderr, FCODE(CODE)); }

FCODE_ALIAS(start_red, 31);
FCODE_ALIAS(start_lightred, 91);
FCODE_ALIAS(start_bold, 1);
FCODE_ALIAS(clear_format, 0);

void _assert_fail(const char *cond, const char *func, const char *file,
                  int line, const char *format, ...) {
  std::fprintf(stderr, "\nASSERT failed: ");

  start_red();
  start_bold();
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  va_end(args);
  clear_format();

  std::fprintf(stderr, "\n - Location: ");

  start_bold();
  std::fprintf(stderr, "%s:%d\n", file, line);
  clear_format();

  std::fprintf(stderr, " - Function: ");

  start_bold();
  std::fprintf(stderr, "%s\n", func);
  clear_format();

  std::fprintf(stderr, " - Failed condition: ");

  start_lightred();
  std::fprintf(stderr, "%s\n", cond);
  clear_format();

  std::raise(SIGINT);
  std::abort();
}

void _unreachable_fail(const char *func, const char *file, int line,
                       const char *format, ...) {
  std::fprintf(stderr, "\nUNREACHABLE executed: ");

  start_red();
  start_bold();
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  va_end(args);
  clear_format();

  std::fprintf(stderr, "\n - Location: ");

  start_bold();
  std::fprintf(stderr, "%s:%d\n", file, line);
  clear_format();

  std::fprintf(stderr, " - Function: ");

  start_bold();
  std::fprintf(stderr, "%s\n", func);
  clear_format();

  std::raise(SIGINT);
  std::abort();
}
