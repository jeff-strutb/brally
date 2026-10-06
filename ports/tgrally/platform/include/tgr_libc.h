/* tgr_libc.h: the C library the game was linked with, where it differs
 * from the host's.  libultra's sprintf formats numbers its own way, so the
 * core's sprintf is libultra's (platform/libc/xprintf.c). */
#ifndef TGR_LIBC_H
#define TGR_LIBC_H
#include <stdarg.h>

int tgr_sprintf(char *dst, const char *fmt, ...);
int tgr_vsprintf(char *dst, const char *fmt, va_list ap);

#ifdef TGR_CORE
#define sprintf tgr_sprintf
#ifdef _WIN32
/* libultra's BSD memory calls, which the Windows C library lacks
 * (platform/libc/bstring.c), declared as a BSD libc does */
#include <stddef.h>
void bcopy(const void *src, void *dst, size_t len);
void bzero(void *p, size_t len);
int  bcmp(const void *a, const void *b, size_t len);
/* the controller status field libultra names errno, which this C library
 * makes a macro */
#undef errno
#endif
#endif
#endif
