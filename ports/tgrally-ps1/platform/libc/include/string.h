/* string.h: the port's C library (platform/libc) */
#ifndef PS1_STRING_H
#define PS1_STRING_H
#include <stddef.h>
void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
void  *memset(void *p, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, size_t n);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strchr(const char *s, int c);
char  *strcat(char *dst, const char *src);
char  *strtok(char *s, const char *delim);
char  *strtok_r(char *s, const char *delim, char **save);
char  *strstr(const char *h, const char *n);
void   bcopy(const void *src, void *dst, size_t n);
void   bzero(void *p, size_t n);
int    bcmp(const void *a, const void *b, size_t n);
#endif
