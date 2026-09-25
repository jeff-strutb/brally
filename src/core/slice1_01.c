/* slice1_01.c -- BRD3D.dll 0x10001000-0x10004910, a later pass. See slice1_01.h. */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_01.h"

#include <stdlib.h>

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

