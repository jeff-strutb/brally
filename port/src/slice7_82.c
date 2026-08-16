/* slice7_82.c -- packet 82.  See slice7_82.h for what each function is, why
 * it was still a stub, the fifteen further stubs that are duplicate names for
 * bodies already in the tree, and the signature conflicts found on the way.
 *
 * The three transcribed bodies were read out of orig/BRGlide.dll (the
 * reference build) with tools/dumpasm.py and re-checked instruction for
 * instruction against orig/BRD3D.dll.  All three are `shared` in
 * config/shared.csv and the two builds differ only in the addresses of the
 * globals they touch:
 *
 *     0x1002BF40  (D3D)  ==  0x10019000  (Glide)   60 bytes
 *     0x10058700  (D3D)  ==  0x100515B0  (Glide)   72 bytes
 *     0x1003C520  (D3D)  ==  0x10035BB0  (Glide)   46 bytes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>

#include "slice7_82.h"

/* BrPtrList / BR_PTRLIST_MAX -- the display-list registry BrDlIsRegistered
 * searches.  slice1_05 owns the type, slice5_60 owns the pointer. */
#include "slice1_05.h"
/* g_aBrAA2538 (the eight slots) and g_brPA9D008 (the UI object whose +0x08
 * selects one).  BrSub10058700 reads both, exactly as slice2_25.c:743 does. */
#include "slice2_25.h"
/* BrDlOwner, for the 0x1003445A adapter. */
#include "slice2_19.h"
/* BrUiFn1003E0E0, the body 0x1003E0E0 already has. */
#include "slice2_23.h"

extern BrPtrList *g_pBrDlPtrList;      /* 0x1067B548 / 0x1067B550, slice5_60 */

/* ==========================================================================
 * 1. CRT leaves
 * ========================================================================== */

/* 0x1008C000  `_itoa` */
char *BrItoa(int value, char *pszBuf, int radix)
{
    /* 32 binary digits + sign + NUL is the widest any radix >= 2 can be. */
    char  aTmp[33];
    char *p = pszBuf;
    unsigned int u;
    int   n = 0;

    if (radix < 2 || radix > 36) {
        /* The CRT's contract is undefined here; refusing is the only answer
         * that cannot scribble on the caller's buffer. */
        *p = '\0';
        return pszBuf;
    }

    /* Only radix 10 is signed.  _itoa(-1, b, 16) is "ffffffff". */
    if (radix == 10 && value < 0) {
        *p++ = '-';
        /* Negate through unsigned so INT_MIN does not overflow. */
        u = (unsigned int)0 - (unsigned int)value;
    } else {
        u = (unsigned int)value;
    }

    do {
        unsigned int d = u % (unsigned int)radix;
        aTmp[n++] = (char)(d < 10u ? ('0' + (int)d) : ('a' + (int)d - 10));
        u /= (unsigned int)radix;
    } while (u != 0u);

    while (n > 0) {
        *p++ = aTmp[--n];
    }
    *p = '\0';
    return pszBuf;
}

/* 0x1007E8B0  `atexit` */
int BrXAtExit(void (*pfn)(void))
{
    if (pfn == NULL) {
        return -1;
    }
    return atexit(pfn);
}

/* ==========================================================================
 * 2. Platform leaves
 * ========================================================================== */

void (*g_pfnBrGlobalRelease)(void *pMem) = free;

void *BrGlobalHandle(void *pMem)
{
    /* GMEM_FIXED: the handle IS the pointer. */
    return pMem;
}

int BrGlobalUnlock(void *hMem)
{
    /* A fixed block's lock count is zero, so the real GlobalUnlock returns
     * FALSE and sets ERROR_NOT_LOCKED.  Both call sites ignore the result;
     * returning 0 is the ORIGINAL's answer, not a placeholder. */
    (void)hMem;
    return 0;
}

void *BrGlobalFree(void *hMem)
{
    if (hMem != NULL && g_pfnBrGlobalRelease != NULL) {
        g_pfnBrGlobalRelease(hMem);
    }
    return NULL;                    /* NULL on success, as GlobalFree does */
}

/* Monotonic microseconds.  See the header for why microseconds and not
 * nanoseconds. */
#define BR82_PERF_HZ  ((int64_t)1000000)

static int64_t br82_micros(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (int64_t)ts.tv_sec * BR82_PERF_HZ + (int64_t)(ts.tv_nsec / 1000);
}

int32_t BrPlatQueryPerfFreq(int64_t *pFreq)
{
    if (pFreq == NULL) {
        return 0;
    }
    *pFreq = BR82_PERF_HZ;
    return 1;                       /* a high-resolution counter exists */
}

int32_t BrPlatQueryPerfCounter(int64_t *pCount)
{
    if (pCount == NULL) {
        return 0;
    }
    *pCount = br82_micros();
    return 1;
}

uint32_t BrPlatTimeGetTime(void)
{
    /* timeGetTime is milliseconds in a 32-bit counter that wraps; the
     * truncation to uint32_t reproduces the wrap. */
    return (uint32_t)(br82_micros() / 1000);
}

void BrScrSleep(uint32_t ms)
{
    if (ms == 0u) {
        /* Sleep(0) on Windows yields the timeslice rather than sleeping. */
        (void)sched_yield();
        return;
    }
    /* usleep's argument is bounded at 1000000 on some hosts, so sleep whole
     * seconds first. */
    while (ms >= 1000u) {
        (void)usleep(999999);
        ms -= 1000u;
    }
    if (ms != 0u) {
        (void)usleep((useconds_t)ms * 1000u);
    }
}

int32_t BrPlatGetUserName(char *pszBuf, uint32_t *pcb)
{
    const char *psz;
    size_t      n;

    if (pszBuf == NULL || pcb == NULL || *pcb == 0u) {
        return 0;
    }

    psz = getenv("USER");
    if (psz == NULL) {
        psz = getenv("LOGNAME");
    }
    if (psz == NULL) {
        psz = "player";
    }

    n = strlen(psz);
    if (n + 1u > (size_t)*pcb) {
        /* GetUserNameA fails and reports the size it wanted, without
         * touching the buffer. */
        *pcb = (uint32_t)(n + 1u);
        return 0;
    }

    memcpy(pszBuf, psz, n + 1u);
    *pcb = (uint32_t)(n + 1u);      /* Win32 counts the NUL */
    return 1;
}

/* ==========================================================================
 * 3. Transcribed bodies
 * ========================================================================== */

/* 0x1002BF40 / Glide 0x10019000, 60 bytes.
 *
 *     test esi,esi / je -> mov eax,1 ; ret       pv == NULL  -> 1
 *     mov edx,[count] / test / jle -> xor eax    n <= 0      -> 0
 *     loop: cmp [ecx],esi / je -> mov eax,1      found       -> 1
 *           inc eax / add ecx,4 / cmp / jl
 *     xor eax,eax                                not found   -> 0
 */
int BrDlIsRegistered(const void *pv)
{
    const BrPtrList *pList;
    int i;

    if (pv == NULL) {
        return 1;                   /* tested BEFORE the table is read */
    }

    pList = g_pBrDlPtrList;
    if (pList == NULL) {
        /* DEVIATION (slice5_60's, applied to the reader): no storage yet is
         * treated as an empty table, which is the same answer the original
         * gives for a count of zero. */
        return 0;
    }

    for (i = 0; i < pList->n; ++i) {
        if (pList->ap[i] == pv) {
            return 1;
        }
    }
    return 0;
}

/* 0x10058700 / Glide 0x100515B0, 72 bytes.
 *
 * The original's loop bound is the address 0x10AA2598, reached from
 * 0x10AA2538 in steps of 12 -- eight records, which is br_slots.h's
 * BR_SLOT_COUNT.  The match key is the record's FIRST dword (the id) against
 * the UI object's +0x08.
 */
int BrSub10058700(void)
{
    int i;

    if (g_brPA9D008 == NULL) {
        return 0;
    }

    for (i = 0; i < BR_SLOT_COUNT; ++i) {
        if (g_aBrAA2538[i].id == g_brPA9D008->f08) {
            /* `sete cl` on the OLD value, store, then re-read for the
             * return -- the original really does load it back. */
            g_aBrAA2538[i].a = (g_aBrAA2538[i].a == 0) ? 1 : 0;
            return g_aBrAA2538[i].a;
        }
    }
    return 0;
}

/* 0x1003C520 / Glide 0x10035BB0, 46 bytes. */

int32_t (*g_pfnBrCoCreateDPlay)(void **ppOut) = NULL;

int32_t BrSub1003C520(struct BrDPlay **ppDPlay)
{
    void   *pOut = NULL;            /* `mov dword ptr [esp], 0` */
    int32_t hr;

    hr = (g_pfnBrCoCreateDPlay != NULL)
       ? g_pfnBrCoCreateDPlay(&pOut)
       : BR82_HR_CLASSNOTREG;

    /* Written unconditionally, failure included -- see the GOTCHA in the
     * header.  slice6_70.c:126 depends on it. */
    if (ppDPlay != NULL) {
        *ppDPlay = (struct BrDPlay *)pOut;
    }
    return hr;
}

/* ==========================================================================
 * 4. Adapters onto bodies that already exist
 * ========================================================================== */

/* 0x1003445A -- one address, one body (slice2_19.c:268 BrDlOwnerFixup). */
void BrSub1003445A(void *pv)
{
    BrDlOwnerFixup((BrDlOwner *)pv);
}

/* 0x1003E0E0 -- one address, one body (slice2_23.c:320 BrUiFn1003E0E0). */

void *g_pBrActiveFlags82 = NULL;

int32_t BrExt_1003E0E0(void)
{
    /* All-zero stand-in used only while the host has not aimed the pointer.
     * It is NOT storage for the nine originals -- creating a second instance
     * of those is the aliased-storage bug -- it is a neutral input that makes
     * the "anything active" half answer 0.  See the DEVIATION note in the
     * header. */
    static const BrActiveFlags s_zero;

    const BrActiveFlags *pFlags = (const BrActiveFlags *)g_pBrActiveFlags82;

    return BrUiFn1003E0E0(pFlags != NULL ? pFlags : &s_zero);
}

/* 0x10043CD0 -- one address, one body (slice2_25.c:819 BrOptOpen2940).
 * Shaped exactly like slice6_76.c:223, which does this for 0x10043BF0. */
void BrExt_10043CD0(int32_t a)
{
    (void)a;                        /* unread in the original */
    (void)BrOptOpen2940(NULL);
}
