/* adler32.c -- zlib 1.0.4's Adler-32 checksum (adler32.c), as the ROM has
 * it.
 *
 * zlib 1.0.4, Copyright (C) 1995-1996 Mark Adler.  For conditions of
 * distribution and use, see the notice in tgr/zinfl.h.  Altered from the
 * original: K&R header written as a prototype.
 */
#include "tgr/zinfl.h"

/* -- declarations -- */
#define BASE 65521L
#define NMAX 5552

#define DO1(buf,i)  {s1 += buf[i]; s2 += s1;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);
/* -- end declarations -- */

/* WHAT IT DOES: Update a running Adler-32 checksum with len bytes of buf
 * (1 for no buffer): the byte sum and the sum of sums, each mod 65521,
 * reduced every 5552 bytes and summed 16 bytes at a time. */
/* @implements 0x80242640 tgr adler32 */
uLong adler32(uLong adler, const Bytef *buf, uInt len)
{
    unsigned int s1 = adler & 0xffff;
    unsigned int s2 = (adler >> 16) & 0xffff;
    int k;

    if (buf == Z_NULL) return 1L;

    while (len > 0) {
        k = len < NMAX ? len : NMAX;
        len -= k;
        while (k >= 16) {
            DO16(buf);
            buf += 16;
            k -= 16;
        }
        if (k != 0) do {
            s1 += *buf++;
            s2 += s1;
        } while (--k);
        s1 %= BASE;
        s2 %= BASE;
    }
    return (s2 << 16) | s1;
}
