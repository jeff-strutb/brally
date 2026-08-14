/* slice1_01.c -- BRD3D.dll 0x10001000-0x10004910, a later pass. See slice1_01.h. */

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
 * 0x10002A20 (partial) -- 8-bit volume to the backend's 0..10000 scale.
 *
 * The original builds 10000*v with four `lea [r+r*4]` (x5 each, so x625) and
 * a `shl 4`, then divides by 255 with the signed magic 0x80808081 / sar 7 /
 * sign fixup. v is masked to 8 bits before any of that, so the input is
 * always non-negative and the sign fixup never fires.
 */
int BrCdVolumeScale(int vol)
{
    return (10000 * (vol & 0xFF)) / 255;
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

/* ---------------------------------------------------------------------------
 * 0x10003460 -- milliseconds to 30 Hz ticks.
 *
 * The original divides by 100 twice: once with a real `div` whose quotient is
 * thrown away (only the remainder is wanted) and once with the unsigned magic
 * 0x51EB851F >> 5. The second magic, 0x3E0F83E1 >> 3, is division by 33.
 *
 * DEVIATION: the original read the current tick from 0x118AB118 through
 * 0x100750F0 and subtracted the origin at 0x10220DD8. The subtraction is the
 * caller's job here.
 */
uint32_t BrTicks30FromMs(uint32_t elapsedMs)
{
    return 3u * (elapsedMs / 100u) + (elapsedMs % 100u) / 33u;
}

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
int BrFChkFRead(void *pDst, size_t size, size_t count, FILE **ppFile)
{
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

    fprintf(stderr,
            "FCHK_FRead(): trying to read %d bytes, but got only %d bytes.\n",
            (int)wanted, (int)((uint32_t)got * (uint32_t)size));
    exit(1);

    return 1;   /* the original falls through to the `mov eax,1` tail */
}

/* 0x10003170  CHK_FRead. Returns the destination buffer. */
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
