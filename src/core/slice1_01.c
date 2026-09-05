/* slice1_01.c -- BRD3D.dll 0x10001000-0x10004910, a later pass. See slice1_01.h. */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_01.h"

#include <stdlib.h>

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

        /* DO16, then the remainder one byte at a time. */
        while (k >= 16u) {
            unsigned int i;
            for (i = 0u; i < 16u; ++i) {
                s1 += *pBuf++;
                s2 += s1;
            }
            k -= 16u;
        }
        while (k > 0u) {
            s1 += *pBuf++;
            s2 += s1;
            --k;
        }

        s1 %= 65521uL;
        s2 %= 65521uL;
    }

    return (s2 << 16) | s1;
}

extern int BrGetTimerState(void);
extern int DAT_1021c908;


/* ---------------------------------------------------------------------------
 * The CHK_* wrappers.
 *
 * DEVIATION (all five): the original formats into a 0x400-byte stack buffer
 * with sprintf and ships it to OutputDebugStringA. Here the message goes
 * straight to stderr -- no fixed buffer, so the %s cases cannot overflow it,
 * which the original could.
 */

int BrChkVerbose = 0;   /* 0x10220CE0 */

/* 0x100030E0  FCHK_FRead.
 *
 * GOTCHA: the fourth argument is a FILE **. The original does
 * `mov ecx, [eax]` on it before pushing it to fread.
 *
 * The byte count is a 32-bit `imul`, so it wraps; kept in uint32_t so a
 * wrapping size*count still produces the original's early-out and the
 * original's message.
 */
/* WHAT IT DOES: reads from a file and insists on getting everything asked
 * for. Reading nothing at all is reported to the caller as a plain failure --
 * that is how the game detects the end of a file -- but a short read, where
 * some but not all of the data arrived, is treated as the file being damaged
 * and kills the game with a message. */
/* @implements 0x100030E0 d3d BrFChkFRead */
#ifdef BR_MATCHING_BUILD
#include <windows.h>
#endif
/* RESIDUE (8 masked diffs, T3a, REGNORM 0+0): the original homes `size` in
 * ebx and `count` in edi; this build homes them the other way round, which
 * flips the two `push`es and the `imul` operands. Every instruction is the
 * original's. Writing the product `count * size` instead of `size * count`
 * changes nothing -- VC5 canonicalises the multiply the same way it does a
 * commutative add. */
int BrFChkFRead(void *pDst, size_t size, size_t count, FILE **ppFile)
{
#ifdef BR_MATCHING_BUILD
    /* The original formats the failure message into a 0x400-byte stack buffer
     * (allocated in the prologue) and ships it to OutputDebugStringA. */
    char buf[0x400];
#endif
    uint32_t wanted = (uint32_t)size * (uint32_t)count;
    size_t   got;

    if (wanted == 0u) {
        return 1;
    }

    got = fread(pDst, size, count, *ppFile);

    if (got == 0u) {
        return 0;
    }
    if (got == count) {
        return 1;
    }

#ifdef BR_MATCHING_BUILD
    wsprintfA(buf,
              "FCHK_FRead(): trying to read %d bytes, but got only %d bytes.\n",
              (int)wanted, (int)((uint32_t)got * (uint32_t)size));
    OutputDebugStringA(buf);
    exit(1);
#else
    fprintf(stderr,
            "FCHK_FRead(): trying to read %d bytes, but got only %d bytes.\n",
            (int)wanted, (int)((uint32_t)got * (uint32_t)size));
    exit(1);
#endif

    return 1;   /* the original falls through to the `mov eax,1` tail */
}

/* 0x10003170  CHK_FRead. Returns the destination buffer. */
/* WHAT IT DOES: the same read, for callers who cannot cope with the file
 * ending -- hitting the end of the file kills the game rather than being
 * reported back. Used where the data being read is required for the game to
 * carry on at all. */
/* port-only body; Glide match is src/core/generated/0x100034C0.c */
void *BrChkFRead(void *pDst, size_t size, size_t count, FILE **ppFile)
{
    if (BrFChkFRead(pDst, size, count, ppFile) == 0) {
        fprintf(stderr,
                "CHK_FRead(): trying to read %u bytes, but got EOF.\n",
                (unsigned int)((uint32_t)count * (uint32_t)size));
        exit(1);
    }
    return pDst;
}

/* 0x10003320  CHK_FileExists. */
int BrChkFileExists(const char *pPath)
{
    FILE *pFile;

    if (BrChkVerbose != 0) {
        fprintf(stderr, "CHK_FileExists(%s)\n", pPath);
    }

    /* The mode string is the literal "rb" at 0x10094110; the original goes
     * through _fsopen with _SH_DENYNO, which plain fopen matches closely
     * enough on a single-process port. */
    pFile = fopen(pPath, "rb");
    if (pFile == NULL) {
        return 0;
    }
    fclose(pFile);
    return 1;
}

/* 0x10003390  CHK_AllocateMemory. */
/* WHAT IT DOES: asks for memory and gives up on the whole game if there is
 * none, naming what it was trying to make room for so the player sees which
 * part of the loading failed. Asking for nothing quietly gets nothing back. */
/* port-only body; Glide match is src/core/generated/0x100036F0.c */
/* @n64 0x8021A9B4 located */
void *BrChkAlloc(size_t size, const char *pWhat)
{
    void *pMem;

    if (size == 0u) {
        /* The original returns without touching EAX, which still holds the
         * zero size -- so a NULL return, not an allocation of one byte. */
        return NULL;
    }

    pMem = malloc(size);
    if (pMem == NULL) {
        fprintf(stderr,
                "CHK_AllocateMemory(): Out of memory: couldn't allocate %s\n",
                pWhat);
        exit(1);
    }
    return pMem;
}

/* 0x100033F0  CHK_ReAllocateMemory. */
/* WHAT IT DOES: grows or shrinks a block of memory, again giving up on the
 * whole game if that cannot be done. Asking for a size of nothing loses the
 * block that was just handed back, which is a leak in the original and is
 * preserved. */
/* port-only body; Glide match is src/core/generated/0x10003760.c */
void *BrChkRealloc(void *pMem, size_t size, const char *pWhat)
{
    void *pNew;

    /* The call comes FIRST; the size check is at 0x1000340F, after it. */
    pNew = realloc(pMem, size);

    if (size == 0u) {
        /* BUG PRESERVED: with pMem == NULL this has just performed a
         * zero-size allocation whose result is now dropped on the floor. */
        return NULL;
    }

    if (pNew == NULL) {
        fprintf(stderr,
                "CHK_ReAllocateMemory(): Out of memory: couldn't reallocate %s\n",
                pWhat);
        exit(1);
    }
    return pNew;
}

