#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  const char *p = s;
  while (*p) {
    p++;
  }
  return (size_t)(p - s);
}

char *strcpy(char *dst, const char *src) {
  char *ret = dst;
  while ((*dst++ = *src++) != '\0')
    ;
  return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
  char *ret = dst;

  while (n && *src) {
    *dst++ = *src++;
    n--;
  }

  while (n) {
    *dst++ = '\0';
    n--;
  }

  return ret;
}

char *strcat(char *dst, const char *src) {
  char *ret = dst;

  while (*dst)
    dst++;

  while ((*dst++ = *src++) != '\0')
    ;

  return ret;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  while (n > 0) {
    unsigned char c1 = (unsigned char)*s1;
    unsigned char c2 = (unsigned char)*s2;

    if (c1 != c2)
      return c1 - c2;

    if (c1 == '\0')
      return 0;

    s1++;
    s2++;
    n--;
  }
  return 0;
}

void *memset(void *s, int c, size_t n) {
  unsigned char *p = (unsigned char *)s;
  while (n--) {
    *p++ = (unsigned char)c;
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;

  if (d == s || n == 0)
    return dst;

  if (d < s) {
    // forward copy
    for (size_t i = 0; i < n; i++) {
      d[i] = s[i];
    }
  } else {
    // backward copy (handle overlap)
    for (size_t i = n; i != 0; i--) {
      d[i - 1] = s[i - 1];
    }
  }

  return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
  unsigned char *dst = (unsigned char *)out;
  const unsigned char *src = (const unsigned char *)in;

  for (size_t i = 0; i < n; i++) {
    dst[i] = src[i];
  }

  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;

  while (n > 0) {
    if (*p1 != *p2)
      return *p1 - *p2;

    p1++;
    p2++;
    n--;
  }
  return 0;
}

#endif
