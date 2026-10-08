/* libc.c -- what the compiler may call: memcpy, memset */
#include <stddef.h>
#include <stdint.h>

void *memcpy(void *d, const void *s, size_t n)
{
    uint8_t *a = d;
    const uint8_t *b = s;
    while (n--)
        *a++ = *b++;
    return d;
}

void *memset(void *d, int c, size_t n)
{
    uint8_t *a = d;
    while (n--)
        *a++ = (uint8_t)c;
    return d;
}

void *__aeabi_memcpy(void *d, const void *s, size_t n) { return memcpy(d, s, n); }
void *__aeabi_memcpy4(void *d, const void *s, size_t n) { return memcpy(d, s, n); }
void __aeabi_memclr(void *d, size_t n) { memset(d, 0, n); }
void __aeabi_memclr4(void *d, size_t n) { memset(d, 0, n); }
void __aeabi_memset(void *d, size_t n, int c) { memset(d, c, n); }
