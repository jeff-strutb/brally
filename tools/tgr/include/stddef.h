#ifndef _STDDEF_H
#define _STDDEF_H
typedef unsigned long size_t;
typedef long ptrdiff_t;
#ifndef NULL
#define NULL 0
#endif
#define offsetof(t,m) ((size_t)&(((t*)0)->m))
#endif
