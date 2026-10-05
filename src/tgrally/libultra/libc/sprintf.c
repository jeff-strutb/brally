/* n64-cflags: -O3 */
/* sprintf.c -- libultra's sprintf (libc/sprintf.c).
 */

/* -- declarations -- */
typedef unsigned int size_t;
typedef char *va_list;
#define _VA_ALIGN(p, a) (((unsigned int)(((char *)p) + ((a) > 4 ? (a) : 4) - 1)) & -((a) > 4 ? (a) : 4))
#define va_start(vp, parmN) (vp = ((va_list)&parmN + sizeof(parmN)))
#define va_end(p)
void *memcpy(void *s1, const void *s2, size_t n);
int _Printf(void *(*prout)(void *, const char *, size_t), char *arg, const char *fmt, va_list args);
/* -- end declarations -- */

/* WHAT IT DOES: sprintf's output routine: append size bytes to the
 * buffer, returning the new end. */
/* @implements 0x80260DB0 tgr proutSprintf */
void *proutSprintf(void *dst, const char *fmt, size_t size)
{
	return (char *)memcpy(dst, fmt, size) + size;
}

/* WHAT IT DOES: Format into dst (0-terminated when formatting succeeds);
 * returns the length. */
/* @implements 0x80260DD4 tgr sprintf */
int sprintf(char *dst, const char *fmt, ...)
{
	int ans;
	va_list ap;

	va_start(ap, fmt);
	ans = _Printf(proutSprintf, dst, fmt, ap);
	if (ans >= 0)
		dst[ans] = 0;
	return ans;
}
