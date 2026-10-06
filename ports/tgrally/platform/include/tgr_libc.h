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
#ifdef __x86_64__
/* The game's data lives at its N64 addresses in the arena (gen/arena.s),
 * which promise only natural alignment: D_80379568, the note-rate table, is 8
 * mod 16. On x86-64 clang takes any array of 16 bytes or more to be 16-byte
 * aligned, extern or not, and vectorizes with movapd, which faults on such an
 * address (Rosetta and so Wine on a Mac do not check, a real x86 CPU does).
 * An explicit alignment replaces that assumption, so every extern the core
 * declares says 8: no x86 instruction needs more of anything below 16.
 * arm64 makes no such assumption and is left alone. */
#define extern extern __attribute__((aligned(8)))
#endif
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
