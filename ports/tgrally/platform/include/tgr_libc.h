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
#endif
#endif
