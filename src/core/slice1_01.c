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

/* ---------------------------------------------------------------------------
 * 0x10002DE0 -- 64x64 u16 grid sample.
 *
 * The four guards are `fcomp` against 0.0f (0x1008F09C) and 2048.0f
 * (0x1008F0A0), read back with fnstsw / `test ah,1`, i.e. the C0 bit, i.e.
 * "ST < operand". Order in the original is x>=0, x<2048, y>=0, y<2048; x is
 * the one loaded twice through [esp+8] once esi has been pushed.
 *
 * The scale is 0x1008F0A4 = 0.03125f = 1/32, exact in binary, so no rounding
 * question arises before the truncation. 0x1007C8A0 is __ftol (it sets the
 * x87 rounding field to 0xC00 = toward zero and does `fistp qword`).
 *
 * Only AL of each conversion is consumed (`movzx si, al` / `movzx ax, al`),
 * so a value of 256 or more would wrap -- unreachable given the 2048 guard,
 * but reproduced with the mask below rather than assumed away.
 *
 * DEVIATION: the grid base was the global at 0x106C7C6C; it is a parameter.
 */
/* WHAT IT DOES: looks up a place in the world on a coarse 64-by-64 grid --
 * each square covering thirty-two world units, so the grid spans a square
 * region a couple of thousand units across -- and hands back both that
 * square's value and the difference to the square next along, so a caller can
 * blend between the two. WHAT THE GRID HOLDS IS NOT ESTABLISHED HERE. A
 * position outside the covered region answers zero, which is indistinguishable
 * from a square whose value genuinely is zero. */
/* @implements 0x10002DE0 d3d BrGrid64Sample */
uint32_t BrGrid64Sample(const uint16_t *pGrid, float x, float y)
{
    unsigned int col, row, idx;
    uint32_t t0, t1, acc;

    /* Written as negated comparisons so NaN takes the reject path, which is
     * what the original does: fcomp with a NaN sets C0, and the first guard
     * rejects on C0. */
    if (!(x >= 0.0f))    { return 0u; }
    if (!(x < 2048.0f))  { return 0u; }
    if (!(y >= 0.0f))    { return 0u; }
    if (!(y < 2048.0f))  { return 0u; }

    col = (unsigned int)(long)(x * 0.03125f) & 0xFFu;
    row = (unsigned int)(long)(y * 0.03125f) & 0xFFu;

    /* esi holds row<<6 with the caller's leftover high bits still in it and
     * eax holds __ftol's high bits; both are discarded by `and 0xffff`. */
    idx = ((row << 6) + col) & 0xFFFFu;

    t0 = pGrid[idx];
    t1 = pGrid[(idx + 1u) & 0xFFFFu];

    /* Literally `t1 + t0*65535`, shifted up 16 -- the low 16 bits of that sum
     * are (t1 - t0) mod 65536, which is the per-cell step. */
    acc = (uint32_t)((t1 + t0 * 65535u) << 16);
    return acc | t0;
}

/* ---------------------------------------------------------------------------
 * 0x10002EF0 -- read one u16 through a cursor and advance it.
 *
 * Both cursor fields are rewritten from one dword: the decremented count is
 * built as `(remaining + 0xFFFF) << 16` and the incremented position is ORed
 * into the low half.
 *
 * BUG PRESERVED: pos + 1 is computed in 32 bits and ORed in before the split,
 * so a position of 0xFFFF sets bit 16 and corrupts the count into
 * (remaining - 1) | 1 instead of carrying. Unreachable for tables under 64K
 * entries, faithful either way.
 *
 * The original returns AX only; the upper half of EAX is left holding part of
 * the table pointer on the success path and untouched on the failure path
 * (`xor ax,ax` is a 16-bit clear). Hence the uint16_t return type.
 *
 * DEVIATION: the table base was the global at 0x106C7C68; it is a parameter.
 */
/* WHAT IT DOES: reads the next entry from a table and moves the reader on by
 * one, counting down how many are left. Running off the end answers zero and
 * leaves the reader where it was -- but zero is also a perfectly valid entry,
 * so a caller cannot tell the two apart. */
/* @implements 0x10002EF0 d3d BrU16CursorNext */
#ifdef BR_MATCHING_BUILD
/* The original takes only the cursor; the table is the global at 0x106C7C68.
 * The portable prototype keeps pTable as an explicit argument. */
const uint16_t *g_br6C7C68;   /* 0x106C7C68 */

uint16_t BrU16CursorNext(BrU16Cursor *pCur)
{
    uint16_t rem;
    uint16_t pos;
    uint32_t packed;

    rem = pCur->remaining;
    if (rem) {
        pos = pCur->pos;
        packed = ((uint32_t)rem + 0xFFFFu) << 16 | ((uint32_t)pos + 1u);
        pCur->pos       = (uint16_t)packed;
        pCur->remaining = (uint16_t)(packed >> 16);
        return g_br6C7C68[pos];
    }
    return 0;
}
#else
uint16_t BrU16CursorNext(const uint16_t *pTable, BrU16Cursor *pCur)
{
    uint32_t rem, pos, packed;

    rem = pCur->remaining;
    if (rem == 0u) {
        return 0u;
    }

    pos = pCur->pos;
    packed = (uint32_t)((rem + 0xFFFFu) << 16) | (pos + 1u);

    pCur->pos       = (uint16_t)(packed & 0xFFFFu);
    pCur->remaining = (uint16_t)((packed >> 16) & 0xFFFFu);

    return pTable[pos];
}
#endif
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

