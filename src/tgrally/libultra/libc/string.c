/* n64-cflags: -O3 */
/* string.c -- libultra's string functions (libc/string.c).
 */

/* -- declarations -- */
typedef unsigned int size_t;
#define NULL 0
/* -- end declarations -- */

/* WHAT IT DOES: Copy n bytes from s2 to s1, a byte at a time; returns s1. */
/* @implements 0x80260B20 tgr memcpy */
void *memcpy(void *s1, const void *s2, size_t n)
{
	unsigned char *su1 = (unsigned char *)s1;
	const unsigned char *su2 = (const unsigned char *)s2;

	while (n > 0) {
		*su1++ = *su2++;
		n--;
	}
	return s1;
}

/* WHAT IT DOES: The length of a C string. */
/* @implements 0x80260B4C tgr strlen */
size_t strlen(const char *s)
{
	const char *sc = s;

	while (*sc)
		sc++;
	return (size_t)(sc - s);
}

/* WHAT IT DOES: The first place c occurs in a C string, or 0. */
/* @implements 0x80260B74 tgr strchr */
char *strchr(const char *s, int c)
{
	const char ch = c;

	while (*s != ch) {
		if (*s == '\0')
			return NULL;
		s++;
	}
	return (char *)s;
}
