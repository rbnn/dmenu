/* See LICENSE file for copyright and license details. */
#include "util.h"
#include <stdlib.h>
#include <string.h>

void *xmalloc(const size_t s)
{
  void *ptr = s ? malloc(s) : NULL;
  assert2(!s || ptr, "Cannot allocate %lu bytes: %m", s);

  return ptr;
}

void *xrealloc(void *ptr, const size_t s)
{
  void *new_ptr = realloc(ptr, s);
  assert2(!s || new_ptr, "Cannot re-allocate %lu bytes: %m", s);

  return new_ptr;
}

char *xstrdup(const char *s)
{
  assert(s);

  return xstrndup(s, strlen(s));
}

char *xstrndup(const char *s, const size_t n)
{
  assert(s);

  char *cpy = (char*)xmalloc(1 + n);
  memcpy(cpy, s, n);
  *(cpy + n) = '\0';

  return cpy;
}
