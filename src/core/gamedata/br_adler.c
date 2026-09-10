/* br_adler.c -- gamedata: the Adler-32 checksum.
 *
 * A verbatim zlib adler32, used to fingerprint blocks of game data so a
 * corrupt read can be told from a good one. Filed out of slice1_01.c, which
 * was an address batch and not a module.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif

#include <stdlib.h>

#ifdef BR_MATCHING_BUILD

/* ---------------------------------------------------------------------------
 * 0x10001000 -- zlib adler32.
 *
 * Identified by the two magic constants the original carries verbatim:
 * 0x15B0 (NMAX = 5552) as the outer chunk cap and 0xFFF1 (BASE = 65521) as
 * the modulus, plus the DO16 unrolled body at 0x1000104E..0x100010E4.
 *
 * Argument order is the zlib one, traced through the prologue: after
 * `push esi` / `push edi` the reads at [esp+0xC] land on arg2 (buf) and arg1
 * (adler) respectively, and after `push ebx` the read at [esp+0x18] lands on
 * arg3 (len).
 *
 * The len == 0 path is reached by `test ebx,ebx / jbe`; after a `test` the
 * carry flag is clear, so jbe is just je -- it is an equality test, not the
 * signed/unsigned comparison it looks like.
 */
/* WHAT IT DOES: computes the Adler-32 checksum of a block of bytes -- a fast
 * running fingerprint used to tell whether the data arrived intact. */
/* @implements 0x10001000 glide BrAdler32 */
unsigned long BrAdler32(unsigned long adler, const unsigned char *pBuf,
                        unsigned int len)
{
    unsigned long s1 = adler & 0xFFFFuL;
    unsigned long s2 = (adler >> 16) & 0xFFFFuL;
    unsigned int  k;

    if (pBuf == NULL) {
        return 1uL;
    }

    while (len > 0u) {
        k = (len < 5552u) ? len : 5552u;
        len -= k;

        /* DO16, then the remainder one byte at a time.  The sixteen steps are
         * written out as indexed reads with a single pBuf += 16; the original
         * is fully unrolled, gates the block on a signed (int)k >= 16, and
         * counts the unrolled loop down to zero. */
        if ((int)k >= 16) {
            unsigned int count = k >> 4;
            do {
                s1 += pBuf[0]; s2 += s1;
                s1 += pBuf[1]; s2 += s1;
                s1 += pBuf[2]; s2 += s1;
                s1 += pBuf[3]; s2 += s1;
                s1 += pBuf[4]; s2 += s1;
                s1 += pBuf[5]; s2 += s1;
                s1 += pBuf[6]; s2 += s1;
                s1 += pBuf[7]; s2 += s1;
                s1 += pBuf[8]; s2 += s1;
                s1 += pBuf[9]; s2 += s1;
                s1 += pBuf[10]; s2 += s1;
                s1 += pBuf[11]; s2 += s1;
                s1 += pBuf[12]; s2 += s1;
                s1 += pBuf[13]; s2 += s1;
                s1 += pBuf[14]; s2 += s1;
                s1 += pBuf[15]; s2 += s1;
                pBuf += 16;
                k -= 16u;
            } while (--count != 0u);
        }
        while (k != 0u) {
            s1 += *pBuf++;
            s2 += s1;
            --k;
        }

        s1 %= 65521uL;
        s2 %= 65521uL;
    }

    return (s2 << 16) | s1;
}

#endif /* BR_MATCHING_BUILD */
