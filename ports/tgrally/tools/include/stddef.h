#ifndef TGR_TOOLS_STDDEF_H
#define TGR_TOOLS_STDDEF_H
typedef unsigned long size_t;
#define NULL ((void *)0)
#define offsetof(t, m) __builtin_offsetof(t, m)
#endif
