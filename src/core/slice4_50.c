/* slice4_50.c -- a later pass, slice 4. See slice4_50.h for the packet's
 * mispaired-listing problem, the three forwarders, and the two skips.
 *
 * Every function here was read out of the asm/ dumps at the address its WANTED
 * name encodes, and carries that address in its comment.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "slice4_50.h"
#include "br_gamestep.h"   /* 0x10034C51 == BRGlide 0x1002E302 -- one slot, one owner */

#ifdef BR_MATCHING_BUILD
/* Orig inlines KERNEL32 IAT WaitForSingleObject / ReleaseMutex (FF 15). */
__declspec(dllimport) int __stdcall WaitForSingleObject(void *, unsigned int);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);
#endif

/* ==========================================================================
 * Cross-slice callees. Each is already declared, with this exact signature,
 * by the header named beside it; repeated here rather than including those
 * headers so that this translation unit stays free of their type models.
 * ========================================================================== */

/* XSLICE 0x10035BBA -- slice2_19.h */
extern void     BrLogSet(void *p);
/* XSLICE 0x10072820 -- slice1_08.h */
extern int32_t  BrSndPlayGroup(int32_t group, uint32_t packed, int32_t loop);
/* XSLICE 0x1007DFE0 -- slice2_26.h / slice3_33.h. operator new; does NOT
 * zero the block (contract). */
extern void    *BrOperatorNew(uint32_t cb);
/* XSLICE 0x100419D0 -- slice2_26.h */
extern void     BrExt_100419D0(void *p);
/* XSLICE 0x10005470 -- slice2_12.h. The original reads its two operands from
 * 0x10ACEDB0 and 0x100B36FC; that port takes them as parameters. */
#ifdef BR_MATCHING_BUILD
/* Orig reads 0x10ACEDB0 / 0x100B36FC from inside the callee -- no args. */
extern uint32_t BrEntityCountActive(void);
#else
extern uint32_t BrEntityCountActive(const void *pvRecords, int32_t cRecords);
#endif
/* XSLICE 0x1000C670 -- slice2_13.h. 0xFFFF is its failure sentinel. */
extern uint32_t BrDPlayGetCurrentPlayers(void);
/* DEVIATION -- slice1_02.h. The original inlines KERNEL32
 * WaitForSingleObject(h, INFINITE) / ReleaseMutex(h); that header already
 * routes the identical pattern through these two hooks. */
extern void     BrNetMutexLock(void *hMutex);
extern void     BrNetMutexUnlock(void *hMutex);

/* ==========================================================================
 * Globals this module owns (see slice4_50.h for each one's address and for
 * the two type conflicts it resolves)
 * ========================================================================== */

/* 0x106C0964 used to have storage here.  It is the SAME original dword as
 * BRGlide 0x106E79F4, which br_gamestep.c owns and whose three accessors
 * (0x1002E302/17/24) are byte-identical to 0x10034C51/66/73.  A second host
 * object for it was the aliased-storage bug CONVENTIONS.md describes, and
 * nothing here could have found it -- the two names have no text in common. */
BrOptEnterHooks  g_brOptEnterHooks = { NULL, NULL };

int32_t   g_brACEE8C  = 0;
int32_t   g_brACEE94  = 0;
int32_t   g_brAA28CC  = 0;
int32_t   g_brAA28C8  = 0;
BrOptObj *g_brPAA2968 = NULL;
BrOptObj *g_brPAA2958 = NULL;
void     *g_brP0AD300 = NULL;

void     *g_brH221324 = NULL;
int32_t   g_br22AAA8  = 0;
void     *g_brPACEDB0 = NULL;
int32_t   g_br0B36FC  = 0;
/* 0x10094294.  CORRECTED by packet 76, which needed this global for its port
 * of 0x10005D30 (six bytes: load it and return it).  The initialiser was 0;
 * the DLL image holds 0xFFFFFFFF at 0x10094294, i.e. -1.  That matters: -1 is
 * the "empty" sentinel and 0 is a VALID slot index, and slice5_62.c feeds this
 * value straight to BrNetSlotGetF02C as an index.  Zero here meant "slot 0"
 * from the first frame instead of "no slot". */
int32_t   g_br094294  = -1;
uint8_t   g_brAD0854[3] = { 0, 0, 0 };
int32_t   g_br277B48  = 0;
char     *g_brPB4E2E8 = NULL;

int32_t (*g_brPfn42AF0_1)(void *p0) = NULL;

/* Non-zero in the shipped image: the calibration block below must run on the
 * first call. */
int32_t g_br0BBAD4  = 1;
int64_t g_br18AB120 = 0;
int32_t g_br18AB128 = 0;
int32_t g_br18AB130 = 0;

/* ==========================================================================
 * 1. Logging / formatting
 * ========================================================================== */

/* 0x10035BBA */
void BrFatal(const char *pszMsg)
{
    /* Not fatal -- see the header. 0x10035BBA is slice2_19.c's BrLogSet, so
     * this forwards rather than growing a second body for one address. */
    BrLogSet((void *)(uintptr_t)pszMsg);
}

/* 0x1007C830 */
/* WHAT IT DOES: builds a piece of text from a pattern and some values -- the
 * game's general-purpose "write this number into that message" routine. It
 * has no idea how big the destination is and cannot be given one, so it is
 * the caller's job to make sure the result fits. */
/* @d3donly 0x1007C830 BrSprintf -- absent from BRGlide (D3D-only / dynamically-imported CRT); no Glide twin exists */
int BrSprintf(char *pszDest, const char *pszFmt, ...)
{
    va_list ap;
    int     n;

    /* DEVIATION: the original is MSVC's sprintf built on a stack FILE with
     * _cnt = 0x7FFFFFFF, i.e. genuinely unbounded. vsprintf is the same
     * contract; nothing here can bound it without changing behaviour, because
     * no call site passes a size. */
    va_start(ap, pszFmt);
    n = vsprintf(pszDest, pszFmt, ap);
    va_end(ap);
    return n;
}


/* ==========================================================================
 * 3. Hooks
 * ========================================================================== */

/* 0x10034C51 */
/* WHAT IT DOES: answers whether a particular screen or handler is the one
 * currently installed -- how a menu asks "am I the one on screen right now?"
 * before acting. */
/* ONE BODY: br_gamestep.c's, which carries BRGlide's 0x1002E302 for it.  The
 * `const void *` here rather than a function pointer is this range's model of
 * the slot -- slice2_19.c's `g_BrPadHookFn` is a literal code ADDRESS -- and
 * that address is BRD3D 0x1002C500, which shared.csv pairs with BRGlide
 * 0x10019A70: the race step.  So this call asks "are we in a race". */
int BrHookIsCurrent(const void *pfn)
{
    return BrGameStepIsAddr(pfn);
}

/* ==========================================================================
 * 4. Projection
 * ========================================================================== */

/* 0x10030930 */
/* WHAT IT DOES: sets up the camera lens -- how wide a view the player sees
 * and how near and far things can be before they are cut off. One of the
 * seven values it is handed, a scale, is passed along and then never looked
 * at by anything. */
/* @implements 0x10030930 d3d BrMat4Perspective7 */
/* @n64 0x80260E30 located */
int BrMat4Perspective7(BrMat4 *pM, uint16_t *pPerspNorm,
                       float fovyDegrees, float aspect,
                       float n, float f, float scale)
{
#ifdef BR_MATCHING_BUILD
    /* pi/360 as a double so the half-angle multiply is `fmul qword`. fptan,
     * two fchs, eight-arg call (scale is pushed and unused by Frustum).
     * Return is the perspNorm pointer, not Frustum's status. */
    float ty, h, w;
    ty = (float)tan((double)fovyDegrees * 0.0087266462599716477);
    h = n * ty;
    w = h * aspect;
    ((int (*)(BrMat4 *, float, float, float, float, float, float, float))BrMat4Frustum)
        (pM, -w, w, -h, h, n, f, scale);
    *pPerspNorm = 1;
    return (int)pPerspNorm;
#else
    (void)scale;
    return BrMat4Perspective(pM, (unsigned short *)pPerspNorm,
                             fovyDegrees, aspect, n, f);
#endif
}

/* ==========================================================================
 * 5. Screen-object installers
 * ========================================================================== */

/* The body 0x10044E20 and 0x10043BF0 share verbatim: lazily create the
 * 0xC8-byte object, publish it in both its own slot and 0x10AA2904, install
 * the enter hook in pfn04, call it, then set f0C and f68.
 *
 * DEVIATION (memory safety, twice): on `operator new` failure the original
 * stores NULL into both slots and returns without touching them again -- that
 * part is faithful -- but on the success path it calls pfn04 with no null
 * check on the hook itself. The hook is guarded here so an unwired
 * g_brOptEnterHooks cannot fault. */
static void BrOptInstall(BrOptObj **ppSlot, BrOptObjFn pfnEnter)
{
    BrOptObj *p;

    if (*ppSlot != NULL) {
        g_brPAA2904 = *ppSlot;      /* cached path: republish and leave */
        return;
    }

    p = (BrOptObj *)BrOperatorNew(BR50_ALLOC(BrOptObj, BR50_OPTOBJ_ORIG_SIZE));
    p = (p != NULL) ? BrOptObjCtor(p) : NULL;

    *ppSlot     = p;
    g_brPAA2904 = p;
    if (p == NULL) {
        return;
    }

    p->pfnEnter = pfnEnter;
    if (pfnEnter != NULL) {
        /* The original re-reads the slot global for both the argument and the
         * indirect call, and re-reads 0x10AA2904 for each of the two field
         * writes; identical here because nothing in between changes them. */
        (*ppSlot)->pfnEnter(*ppSlot);
    }
    g_brPAA2904->f0C = 1;
    g_brPAA2904->f68 = 1;
}

/* 0x10044E20 */
/* WHAT IT DOES: opens one of the menu screens, building it the first time it
 * is asked for and reusing it afterwards, then making it the screen the game
 * is showing. It also copies two settings into place before doing anything
 * else, on every call and not just the first. Which screen this is was not
 * established. */
/* port-only body; Glide match is src/core/cpp/0x1003E370.cpp */
void BrMenuSub10044E20(int32_t n)
{
    (void)n;                        /* the original reads no argument */

    /* Unconditional, and before the cached-path test. */
    g_brAA28CC = g_brACEE8C;
    g_brAA28C8 = g_brACEE94;

    BrOptInstall(&g_brPAA2968, g_brOptEnterHooks.p1005A6E0);
}

/* 0x10043BF0 */
/* WHAT IT DOES: opens a different menu screen the same way, first committing
 * the choices the player has made so far so the new screen sees them. Which
 * screen this is was not established either. */
/* port-only body; Glide match is src/core/cpp/0x1003D140.cpp */
void BrSub10043BF0(BrGameObj *p)
{
    (void)p;                        /* the original reads no argument */

    BrExt_100419D0(g_brP0AD300);
    BrSub1003E510();

    BrOptInstall(&g_brPAA2958, g_brOptEnterHooks.p100563E0);
}

/* ==========================================================================
 * 8. Millisecond clock
 * ========================================================================== */

/* (counter * 1000 + 500) / frequency, with _allmul / _alldiv semantics. */
/* 0x10075020 BrSub10075020, with the static helper only it used, now lives
 * in src/core/startup/br_timer.c. */

