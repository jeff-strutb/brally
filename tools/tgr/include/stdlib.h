#ifndef _STDLIB_H
#define _STDLIB_H
#include <stddef.h>
void *malloc(size_t); void *calloc(size_t,size_t); void *realloc(void*,size_t);
void free(void*); int atoi(const char*); double atof(const char*);
long strtol(const char*,char**,int); int abs(int); void exit(int);
int rand(void); void srand(unsigned);
void qsort(void*,size_t,size_t,int(*)(const void*,const void*));
#endif
