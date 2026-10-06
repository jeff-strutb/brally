/* bstring.c: libultra's BSD memory calls, for a C library without them
 * (Windows); elsewhere the host's own are used. */
#ifdef _WIN32
#include <string.h>

void bcopy(const void *src, void *dst, size_t len) { memmove(dst, src, len); }
void bzero(void *p, size_t len) { memset(p, 0, len); }
int  bcmp(const void *a, const void *b, size_t len) { return memcmp(a, b, len); }
#endif
