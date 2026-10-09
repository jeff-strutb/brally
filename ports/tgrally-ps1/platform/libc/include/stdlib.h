/* stdlib.h: the port's C library (platform/libc) */
#ifndef PS1_STDLIB_H
#define PS1_STDLIB_H
#include <stddef.h>
typedef struct { long quot, rem; } ldiv_t;
typedef struct { long long quot, rem; } lldiv_t;
char *getenv(const char *name);     /* the switches run.cfg sets (os/main_ps1.c) */
void  ps1_setenv(const char *name, const char *value);
int   atoi(const char *s);
int   abs(int x);
long  labs(long x);
ldiv_t  ldiv(long a, long b);
lldiv_t lldiv(long long a, long long b);
unsigned long strtoul(const char *s, char **end, int base);
long  strtol(const char *s, char **end, int base);
void *malloc(size_t n);
void *calloc(size_t n, size_t size);
void *realloc(void *p, size_t n);
void  free(void *p);
void  abort(void) __attribute__((noreturn));
void  exit(int code) __attribute__((noreturn));
void  qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));
#endif
