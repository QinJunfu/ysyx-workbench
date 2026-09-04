#include <klib.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  size_t len = 0;
  while (s[len] != '\0') {
    len ++;
  }
  return len;
}

char *strcpy(char *dst, const char *src) {
  char *ret = dst;
  while ((*dst++ = *src++) != '\0') {
  }
  return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
  char *ret = dst;
  size_t i = 0;

  while (i < n && src[i] != '\0') {
    dst[i] = src[i];
    i ++;
  }
  while (i < n) {
    dst[i] = '\0';
    i ++;
  }
  return ret;
}

char *strcat(char *dst, const char *src) {
  char *ret = dst;
  while (*dst != '\0') {
    dst ++;
  }
  while ((*dst++ = *src++) != '\0') {
  }
  return ret;
}

int strcmp(const char *s1, const char *s2) {
  while ((unsigned char)*s1 == (unsigned char)*s2) {
    if (*s1 == '\0') {
      return 0;
    }
    s1 ++;
    s2 ++;
  }
  return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  while (n > 0 && (unsigned char)*s1 == (unsigned char)*s2) {
    if (*s1 == '\0') {
      return 0;
    }
    s1 ++;
    s2 ++;
    n --;
  }
  if (n == 0) {
    return 0;
  }
  return (unsigned char)*s1 - (unsigned char)*s2;
}

void *memset(void *s, int c, size_t n) {
  unsigned char *p = s;
  while (n > 0) {
    *p++ = (unsigned char)c;
    n --;
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  unsigned char *d = dst;
  const unsigned char *s = src;

  if (d == s || n == 0) {
    return dst;
  }

  size_t i = 0;
  while (i < n && d != s + i) {
    i ++;
  }

  if (i < n) {
    while (n > 0) {
      n --;
      d[n] = s[n];
    }
  } else {
    for (i = 0; i < n; i ++) {
      d[i] = s[i];
    }
  }
  return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
  unsigned char *dst = out;
  const unsigned char *src = in;
  for (size_t i = 0; i < n; i ++) {
    dst[i] = src[i];
  }
  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = s1;
  const unsigned char *p2 = s2;
  for (size_t i = 0; i < n; i ++) {
    if (p1[i] != p2[i]) {
      return p1[i] - p2[i];
    }
  }
  return 0;
}

char *strchr(const char *s, int c) {
  do {
    if ((unsigned char)*s == (unsigned char)c) return (char *)s;
    if (*s == '\0') break;
    s ++;
  } while (1);
  return NULL;
}

char *strrchr(const char *s, int c) {
  const char *p = s + strlen(s);
  do {
    if ((unsigned char)*p == (unsigned char)c) return (char *)p;
    if (s == p) break;
    p --;
  } while (1);
  return NULL;
}

#endif
