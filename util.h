/* See LICENSE file for copyright and license details. */
#ifndef DMENU_UTIL_H
#define DMENU_UTIL_H
#include "clip.h"
#include <stdio.h>
#include <stdlib.h>

/* Debugging macros: debug(...), warn_if(...), assert(x) and assert2(x,...) */
#ifndef NDEBUG
#define debug(...) \
  { \
    fprintf(stderr, "%s:%i:Debug: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); \
    fputc('\n', stderr); \
    fflush(stderr); \
  }
#else

/* Disable debug, i.e. make `debug(...)' a no-op */
#define debug(...)
#endif /* NDEBUG */

#define warning(...) \
  { \
    fprintf(stderr, "%s:%i:Warning: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); \
    fputc('\n', stderr); \
    fflush(stderr); \
  }

/* Pring warning message to stderr, if `x' evaluates non-zero. */
#define warn_if(x,...) \
  { \
    if((x)) { \
      warning(__VA_ARGS__); \
     } \
   }

/* Abort program, if `x' evaluates to zero. */
#define assert(x) \
  { \
    if(!(x)) { \
      fprintf(stderr, "%s:%i:Error: Assertion `%s' failed.\n", __FILE__, __LINE__, #x); \
      fflush(stderr); \
      abort(); \
    } \
  }

#define assert2(x,...) \
  { \
    if(!(x)) { \
      fprintf(stderr, "%s:%i:Error: Assertion `%s' failed: ", __FILE__, __LINE__, #x); \
      fprintf(stderr, __VA_ARGS__); \
      fputc('\n', stderr); \
      fflush(stderr); \
      abort(); \
    } \
  }

#define die(...)  \
  { \
    fprintf(stderr, "%s:%i:Error: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); \
    fputc('\n', stderr); \
    fflush(stderr); \
    abort(); \
    /* exit(EXIT_FAILURE); */ \
  }

/* Pring error message to stderr, if `x' evaluates non-zero. */
#define die_if(x,...) \
  { \
    if((x)) { \
      die(__VA_ARGS__); \
    } \
  }

/* Fail-safe replacement for `malloc(3)'. If it returns, the array was allocated. */
void *xmalloc(const size_t s);
void *xrealloc(void *ptr, const size_t s);

/* Fail-safe replacement for `strdup(3)'. Based on `xmalloc'. */
char *xstrdup(const char *s);

/* Fail-safe replacement for `strndup(3)'. Based on `xmalloc'. */
char *xstrndup(const char *s, const size_t n);
#endif /* DMENU_UTIL_H */
