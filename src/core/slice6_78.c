/* slice6_78.c -- BRD3D.dll, packet 78 (slice 6).  See slice6_78.h for how the
 * targets were chosen, what was declined and why, the signature conflicts
 * found on the way, and the original defects preserved.
 *
 * WHY THIS FILE INCLUDES ALMOST NOTHING
 * -------------------------------------
 * Seven of the sixteen entry points forward to a body owned by another
 * module, and those owners' headers cannot all coexist in one translation
 * unit: they carry conflicting partial models of the same objects.
 * port/host/brally.c hit this first and packets 74, 76 and 77 followed it.
 * Every cross-module declaration below is copied VERBATIM from the owning
 * header and tagged with it, so a later divergence shows up as a compile
 * error at the owner rather than as silent disagreement here.
 *
 * Struct types that only appear behind a pointer are declared as incomplete
 * tag types.  That is enough for the signature, it is the SAME type the owner
 * declares, and it commits this file to no layout of its own.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdarg.h>
#include "br_path.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice6_78.h"

/* ==========================================================================
 * 0. Cross-module declarations (see the banner)
 * ========================================================================== */

/* slice1_05.h:214 -- 0x1002F900.  slice2_18.h calls the same address with its
 * own name for the command pair; both structs are {uint32_t w0, w1;}. */
struct BrGfxWords;
extern void BrRdpSetCombineLERP(struct BrGfxWords *pOut,
                                int a0,  int b0,  int c0,  int d0,
                                int Aa0, int Ab0, int Ac0, int Ad0,
                                int a1,  int b1,  int c1,  int d1,
                                int Aa1, int Ab1, int Ac1, int Ad1);

/* slice5_63.h -- 0x10019260, 0x10019270, 0x100192F0. */
extern void BrSub_10019260(void);
extern void BrSub_10019270(void);
extern void BrSub_100192F0(int size);
extern uint8_t g_br4B0358;
extern int g_br4B0348;

/* slice1_03.h:138 -- 0x100192A0. */
extern void BrTextSetColors(int a1, int a2, int a3, int a4, int a5, int a6);

/* slice4_52.h:95 -- 0x1003BD50. */
extern int BrRandom(void);

/* slice2_16.h:649 -- 0x1002BA00. */
extern void BrSwapU16x4Array(void *pv, int count);

/* slice1_01.h -- 0x10003390 CHK_AllocateMemory, and the 0x10220CE0 trace
 * flag both file helpers below read. */
extern void *BrChkAlloc(size_t size, const char *pWhat);
extern int   BrChkVerbose;

/* br_crt.h:19 -- 0x1007DFE0, operator new (_nh_malloc(cb,1)).  Does NOT
 * zero, which is why the buffer in BrErrorf is written before it is used. */
extern void *BrOperatorNew(uint32_t cb);

/* br_data.c / slice2_11.h -- the CD module's globals. */
extern int g_brCdEnabled;     /* 0x100940A4, ships as 2 */
extern int g_brCdPlaying;     /* 0x10220CD0 */
extern int g_brCdTrackCur;    /* 0x10220CD4 */

/* ==========================================================================
 * 1. Storage owned here
 *
 * All three are past the end of .data's raw bytes in the original
 * (.data raw ends at 0x100C1400), so zero is the image's value and not an
 * unexamined default.
 * ========================================================================== */

int32_t g_br675540;      /* 0x10675540 -- see the ALIAS note in the header */
uint8_t g_br4B0360;      /* 0x104B0360 */
int32_t g_brCdMediaOk;   /* 0x10220C3C */

/* The 0x100024C0 seam; see the header. */
int (*g_pfnBrCdTrackGet0024C0)(void) = NULL;

/* ==========================================================================
 * 3. 0x10008CC0 -- BrErrorf, format and die
 * ==========================================================================
 *
 * Twelve call sites.  The whole body is:
 *
 *     buf = operator new(0x400)      ; 0x1007DFE0
 *     vsprintf(buf, fmt, &fmt + 1)   ; 0x1007E100
 *     exit(1)                        ; 0x1007CC00
 *
 * There is no output call and no free.  Whatever the caller wanted reported is
 * formatted into a leaked kilobyte and then thrown away, and the process dies.
 * That is the shipped behaviour of every "error" message routed through this
 * function, including br_pod's "Memory Corrupted!".  Preserved.
 *
 * The stub this replaces returned 0 and let the caller continue past a
 * condition the original treats as fatal.
 */
void BrErrorf(const char *pszFmt, ...)
{
    char *pBuf = (char *)BrOperatorNew(0x400u);

    if (pBuf != NULL) {          /* DEVIATION: the original does not check. */
        va_list ap;
        va_start(ap, pszFmt);
        /* DEVIATION: the original is vsprintf into a fixed 0x400 buffer and
         * can overflow it.  An overflow cannot be reproduced safely, so the
         * write is bounded; nothing reads the result either way. */
        vsnprintf(pBuf, 0x400u, pszFmt, ap);
        va_end(ap);
    }

    /* No print, no free.  Both omissions are the original's. */
    exit(1);
}
/* 0x10008B90 (BrPodWriterMakeName) now lives in port/src/br_path.c.
 * It was moved out because br_pod.c needs it too, and linking this
 * packet just to reach one 40-line path splitter dragged in BrChkAlloc,
 * BrOperatorNew, BrRandom and a dozen more. One original address still
 * has exactly one body; it just lives somewhere both callers can reach
 * without inheriting a packet. */


/* ==========================================================================
 * 6. The CD track query
 * ========================================================================== */

/* 0x10002910.  Both arms are tail jumps; the dispatcher adds nothing. */
int BrCdTrackGet(void)
{
    if (g_brCdEnabled == 1) {
        /* DEVIATION: 0x100024C0 is an MCI_STATUS through mciSendCommandA.
         * A NULL hook returns 0.  The shipped g_brCdEnabled is 2, so this arm
         * is not taken -- and the test is `== 1`, not `!= 0`. */
        return (g_pfnBrCdTrackGet0024C0 != NULL)
                   ? g_pfnBrCdTrackGet0024C0()
                   : 0;
    }
    return BrCdTrackGetEar();
}

/* ==========================================================================
 * 7. Single-store functions
 * ========================================================================== */



/* 0x10019250 -- `mov byte [0x104B0360], 0` */
void BrSub_10019250(void)
{
    g_br4B0360 = 0u;
}

/* ==========================================================================
 * 8. Adapters -- the body already exists, this only wires the stub name to it
 * ========================================================================== */

/* 0x1002F900, 33 call sites -- the highest-demand stub still standing after
 * packet 76 wired the OTHER name for this address (BrSub_1002F900).  Two stub
 * names, one body: slice1_05.c's BrRdpSetCombineLERP.  Prototype copied
 * verbatim from slice2_18.h:139. */
void BrGfx2F900(uint32_t *pCmd,
                int32_t a01, int32_t a02, int32_t a03, int32_t a04,
                int32_t a05, int32_t a06, int32_t a07, int32_t a08,
                int32_t a09, int32_t a10, int32_t a11, int32_t a12,
                int32_t a13, int32_t a14, int32_t a15, int32_t a16)
{
    /* Two names, one 8-byte object; slice2_18.h calls it a pair of dwords and
     * slice1_05.h calls it BrGfxWords {uint32_t w0, w1;}. */
    BrRdpSetCombineLERP((struct BrGfxWords *)(void *)pCmd,
                        (int)a01, (int)a02, (int)a03, (int)a04,
                        (int)a05, (int)a06, (int)a07, (int)a08,
                        (int)a09, (int)a10, (int)a11, (int)a12,
                        (int)a13, (int)a14, (int)a15, (int)a16);
}

/* 0x100192F0, 27 call sites.  slice5_63.c routes it through BrTextGetState()
 * so the scale stays shared with BrTextDraw. */
/* WHAT IT DOES: sets the size text is drawn at from here on. This is a
 * wrapper; the body lives with the rest of the text drawing so the size
 * stays shared with the routine that draws. */
/* @d3donly 0x100192F0 BrTextSetSize -- glide twin 0x100168B0 claimed by br_font.c */
void BrTextSetSize(int size)
{
    g_br4B0348 = size;
}


/* 0x10019270, 9 call sites. */
void BrTextAlignCentre(void)
{
    BrSub_10019270();
}

/* 0x100192A0, 3 call sites.  slice1_03.c has the body; the six arguments are
 * two triples and keep their positional names in both places. */
void BrTextSetColor6(int a, int b, int c, int d, int e, int f)
{
    BrTextSetColors(a, b, c, d, e, f);
}

/* 0x1003BD50, 20 call sites.  slice4_52.c's BrRandom is this address: it
 * steps slice2_22.c's BrDPlayRandStep over the seed at 0x10A9BFD0.
 *
 * The stub this replaces returned a constant 0, which slice2_20.c fed into
 * particle placement and slice2_15.c into the whole weather system. */
int BrRand(void)
{
    return BrRandom();
}

/* 0x1002BA00, 1 call site.  slice2_16.c's BrSwapU16x4Array.  Its sibling
 * 0x1002BA80 could NOT be adapted -- see the decline in the header. */
void BrSwapRec8Array(void *pv, int n)
{
    BrSwapU16x4Array(pv, n);
}

#ifdef BR_MATCHING_BUILD
__declspec(dllimport) void * __stdcall CreateMutexA(void *, int, const char *);
__declspec(dllimport) int __stdcall CloseHandle(void *);
#endif

int32_t g_br18A9878;
int32_t g_br18AA098;
void   *g_br18AA0A0;

/* 0x10074F20 BrMutexCreateAA0A0 and 0x10074F40 BrMutexCloseAA0A0 now live
 * in src/core/startup/br_ringmutex.c; the three globals they share with the
 * ring readers below stay defined here. */

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern char DAT_118ec998;
extern char DAT_118ec99c;
#ifndef BR_FUNCPTR_DEFINED
#define BR_FUNCPTR_DEFINED
typedef int (*funcptr)();
#endif
extern funcptr DAT_118ed1d0;
__declspec(dllimport) int __stdcall WaitForSingleObject(void *hHandle, unsigned int dwMilliseconds);
__declspec(dllimport) int __stdcall ReleaseMutex(void *hMutex);
extern int DAT_1021c788;



#endif /* BR_MATCHING_BUILD */
