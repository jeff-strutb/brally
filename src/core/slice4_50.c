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
 * 6. Network
 * ========================================================================== */

/* 0x100053F0 */
/* WHAT IT DOES: sends a status message out to the other machines in a network
 * game, but only once every player expected has actually turned up -- it
 * compares the number of cars in play against the number of players connected
 * and stays quiet if they disagree. */
/* @implements 0x100053F0 d3d BrNetSendFlush */
void BrNetSendFlush(void)
{
    uint32_t cActive;
    uint32_t flag;

    /* Orig: WaitForSingleObject(h, INFINITE); reload h; load flag; ReleaseMutex(h). */
#ifdef BR_MATCHING_BUILD
    WaitForSingleObject(g_brH221324, 0xffffffffu);
    flag = (uint32_t)g_br22AAA8;
    ReleaseMutex(g_brH221324);
    if (flag == 0) {
        return;
    }
#else
    BrNetMutexLock(g_brH221324);
    BrNetMutexUnlock(g_brH221324);

    if (g_br22AAA8 == 0) {
        return;
    }
#endif

#ifdef BR_MATCHING_BUILD
    cActive = BrEntityCountActive();
#else
    cActive = BrEntityCountActive(g_brPACEDB0, g_br0B36FC);
#endif
    if (cActive != BrDPlayGetCurrentPlayers()) {
        return;
    }

    /* Orig pushes the ADDRESS of g_brP277B40 and of g_brPB4E2E8 (offset,
     * not the pointer those globals hold). */
#ifdef BR_MATCHING_BUILD
    BrNetSend4760(&g_brP277B40, g_br094294, g_br22B34C,
                  g_brAD0854[0], g_brAD0854[1], g_brAD0854[2],
                  g_br277B48, (char *)&g_brPB4E2E8, 3, 0);
#else
    BrNetSend4760(&g_brP277B40, g_br094294, g_br22B34C,
                  g_brAD0854[0], g_brAD0854[1], g_brAD0854[2],
                  g_br277B48, g_brPB4E2E8, 3, 0);
#endif
}

/* ==========================================================================
 * 7. DirectPlay host / join / send
 * ========================================================================== */

/* 0x1003C150 */
/* WHAT IT DOES: sets this machine up as the host of a network game, so other
 * players can find and join it. If hosting fails it composes an explanatory
 * message and then throws it away without showing it to anyone -- the player
 * sees nothing. */
/* port-only body; Glide match is src/core/generated/0x100357E0.c */
void BrSub1003C150(void)
{
    unsigned char aDesc[BR50_DPDESC_SIZE];
    char          szMsg[BR50_DPMSG_SIZE];
    int32_t       hr;

    if (g_brP277B40 == NULL) {
        return;
    }

    memset(aDesc, 0, sizeof aDesc);     /* rep stosd, ecx = 0x33 */
    BrSub1003D130(aDesc);

    hr = BrSub1003C5C0(g_brP277B40, aDesc, g_brPA9D008);
    if (hr < 0) {
        /* Formatted into a stack buffer and dropped on the floor -- see the
         * header. Kept because the call to 0x1007C830 is observable. */
        BrSprintf(szMsg, "Could not host session because of error 0x%08X",
                  (unsigned int)hr);
        return;
    }

    g_br22AF18 = 2;
    BrSub10071550();
    BrSub10005B10(1);
}

/* 0x1003C260 */
/* WHAT IT DOES: joins a network game somebody else is hosting, under the
 * player's Windows user name. If that name is already taken it asks whether
 * to try again and does so with the same name -- which will fail the same way
 * unless something else changed it. As with hosting, the failure message it
 * builds is discarded rather than shown. */
/* @implements 0x1003C260 d3d BrSub1003C260 */
#ifdef BR_MATCHING_BUILD
/* Direct globals/callees; the 29D4 deref is unguarded as in the original;
 * the retry hook is a direct call; one shared return-1 tail. */
extern int   DAT_10273328;
extern int   DAT_10ac5d30;
extern char *DAT_10ac5d2c;
extern int   DAT_10ac4090;
extern int   DAT_10ac4098;
extern int   DAT_10226a48;
extern char  DAT_100aa5b0[];        /* "Could not join session ..." */
extern int   BrSub1003D030(void *pJoin);
extern int   BrSub1003C740(int hDp, void *pJoin, char *pszName, int a4);
extern int   BrSub100385E0(char *pszName);      /* glide 0x100385E0 */
extern void  BrSub100355F0(void);
extern void  BrSub100356B0(void);
extern void  BrSub10005B10(int v);
extern void  BrSub1003CE80(void);
__declspec(dllimport) int __stdcall GetUserNameA(char *, unsigned long *);

int BrSub1003C260(void)
{
    unsigned long cbName;
    unsigned char aJoin[0x10];
    char          szName[0x320];
    char          szMsg[0x400];
    int           hr;

    if (DAT_10273328 == 0)
        return 0;

    if (DAT_10ac5d30 == 0)
        return 1;
    if (*(unsigned short *)(DAT_10ac5d2c + 0x1E164) <= 0u)
        return 1;

    if (DAT_10ac4090 == 0) {
        hr = BrSub1003D030(aJoin);
        if (hr >= 0) {
            memset(szName, 0, sizeof(szName));
            cbName = 0xC8;
            GetUserNameA(szName, &cbName);

            hr = BrSub1003C740(DAT_10273328, aJoin, szName, DAT_10ac4098);
            if (hr == (int)0x88770820) {
                if (BrSub100385E0(szName) == 0)
                    return 0;
                hr = BrSub1003C740(DAT_10273328, aJoin, szName,
                                   DAT_10ac4098);
            }
        }
        if (hr < 0) {
            BrSub100355F0();
            BrSub100356B0();
            sprintf(szMsg, DAT_100aa5b0, hr);
            return 0;
        }
    }

    DAT_10226a48 = 1;
    BrSub10005B10(1);
    BrSub1003CE80();
    return 1;
}
#else
/* @implements 0x1003C260 d3d BrSub1003C260 */
int BrSub1003C260(void)
{
    unsigned char aJoin[BR50_DPJOIN_SIZE];
    char          szName[BR50_DPNAME_SIZE];
    char          szMsg[BR50_DPMSG_SIZE];
    uint32_t      cbName;
    int32_t       hr;

    if (g_brP277B40 == NULL) {
        return 0;
    }

    /* GOTCHA: 29D8 is the one that is null-tested; 29D4 is then dereferenced
     * unguarded. DEVIATION: guarded, folded into the same early-out. */
    if (g_brPAA29D8 == NULL || g_brPAA29D4 == NULL) {
        return 1;
    }
    /* `cmp word ptr [eax+0x1E164], 0 / jbe` -- unsigned, so this is == 0. */
    if (g_brPAA29D4->f1E164 == 0) {
        return 1;
    }

    if (g_brA9D000 == 0) {
        hr = BrSub1003D030(aJoin);
        if (hr >= 0) {
            memset(szName, 0, sizeof szName);   /* rep stosd, ecx = 0xC8 */
            cbName = BR50_DPNAME_CB;
            (void)BrPlatGetUserName(szName, &cbName);

            hr = BrSub1003C740(g_brP277B40, aJoin, szName, g_brPA9D008);

            /* DPERR_USERCANCEL: run 0x10042AF0 on the name and, if it says
             * yes, retry the join with the same arguments. If it says no the
             * original returns immediately -- WITHOUT the 0x1003BF60 /
             * 0x1003C020 teardown the ordinary failure path runs. */
            if (hr == (int32_t)0x88770820u) {
                if (g_brPfn42AF0_1 == NULL || g_brPfn42AF0_1(szName) == 0) {
                    return 0;
                }
                hr = BrSub1003C740(g_brP277B40, aJoin, szName, g_brPA9D008);
            }
        }

        if (hr < 0) {
            BrSub1003BF60();
            BrSub1003C020();
            BrSprintf(szMsg,
                      "Could not join session because of error 0x%08X",
                      (unsigned int)hr);
            return 0;
        }
    }

    g_br22AF18 = 1;
    BrSub10005B10(1);
    BrSub1003CE80();
    return 1;
}
#endif

/* ==========================================================================
 * 8. Millisecond clock
 * ========================================================================== */

/* (counter * 1000 + 500) / frequency, with _allmul / _alldiv semantics. */
static int32_t BrPerfToMs(int64_t counter)
{
    if (g_br18AB120 == 0) {
        return 0;               /* DEVIATION: the original divides by zero */
    }
    return (int32_t)((counter * 1000 + 500) / g_br18AB120);
}

/* 0x10075020 */
/* WHAT IT DOES: tells the caller how many milliseconds have passed since the
 * game started. On its first call it works out how to read the machine's
 * high-resolution clock and notes the starting point; from then on it uses
 * that, falling back to the ordinary Windows clock on any machine where the
 * precise one is unavailable. */
/* @implements 0x10075020 d3d BrSub10075020 */
#ifdef BR_MATCHING_BUILD
/* Direct IAT calls and globals; the ms conversion is inline __int64 math
 * ((t*1000+500)/freq via __allmul/__alldiv), truncated to int. The QPC
 * import pointer is CSE'd across both arms. */
extern int     DAT_100bb2dc;
extern __int64 DAT_118ee238;        /* frequency  */
extern int     DAT_118ee240;        /* QPF result */
extern int     DAT_118ee248;        /* baseline ms */
__declspec(dllimport) int __stdcall QueryPerformanceCounter(__int64 *);
__declspec(dllimport) int __stdcall QueryPerformanceFrequency(__int64 *);
__declspec(dllimport) unsigned long __stdcall timeGetTime(void);

int32_t BrSub10075020(void)
{
    __int64 t;

    if (DAT_100bb2dc != 0) {
        DAT_118ee240 = QueryPerformanceFrequency(&DAT_118ee238);
        QueryPerformanceCounter(&t);
        DAT_118ee248 = (int)((t * 1000 + 500) / DAT_118ee238);
        DAT_100bb2dc = 0;
    }
    if (DAT_118ee240 != 0) {
        if (QueryPerformanceCounter(&t) != 0)
            return (int)((t * 1000 + 500) / DAT_118ee238) - DAT_118ee248;
    }
    return (int)timeGetTime();
}
#else
/* @implements 0x10075020 d3d BrSub10075020 */
int32_t BrSub10075020(void)
{
    int64_t now;

    if (g_br0BBAD4 != 0) {
        g_br18AB128 = BrPlatQueryPerfFreq(&g_br18AB120);
        now = 0;
        /* The original ignores this call's return value. */
        (void)BrPlatQueryPerfCounter(&now);
        g_br18AB130 = BrPerfToMs(now);
        g_br0BBAD4  = 0;
    }

    if (g_br18AB128 == 0) {
        return (int32_t)BrPlatTimeGetTime();
    }
    now = 0;
    if (BrPlatQueryPerfCounter(&now) == 0) {
        return (int32_t)BrPlatTimeGetTime();
    }
    return BrPerfToMs(now) - g_br18AB130;
}
#endif

/* ==========================================================================
 * 0x10004C20
 * ========================================================================== */

#ifdef BR_MATCHING_BUILD
/* KERNEL32. dllimport emits `call dword ptr [IAT]` rather than a thunk.
 * Timeout is `(unsigned long)-1` so the push is `6A FF` (INFINITE). */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(
    void *hHandle, unsigned long dwMilliseconds);
__declspec(dllimport) int __stdcall ReleaseMutex(void *hMutex);

void    *g_brH220DDC;  /* 0x10220DDC -- mutex that guards g_br221314 */
int32_t  g_br221314;   /* 0x10221314 */

/* WHAT IT DOES: takes the network mutex at 0x10220DDC, turns the flag at
 * 0x10221314 on if it is currently off, then releases the mutex. Always
 * returns 1; the previous value of the flag is thrown away. */
/* @implements 0x10004C20 d3d BrNetLockSetIfZero221314 */
int32_t BrNetLockSetIfZero221314(void)
{
    WaitForSingleObject(g_brH220DDC, (unsigned long)-1);
    if (g_br221314 == 0) {
        g_br221314 = 1;
    }
    ReleaseMutex(g_brH220DDC);
    return 1;
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
#include <windows.h>
extern int DAT_10078828;
extern int DAT_10078858;

/* WHAT IT DOES: create a COM object via CoCreateInstance and return its interface pointer. */
/* @implements 0x10035BB0 glide BrComCreateInstance */

int BrComCreateInstance(int *param_1)

{
  LPVOID local_4;
  
  local_4 = (LPVOID)0x0;
  CoCreateInstance((IID *)&DAT_10078858,(LPUNKNOWN)0x0,1,(IID *)&DAT_10078828,&local_4);
  *param_1 = local_4;
  return;
}

#endif /* BR_MATCHING_BUILD */
