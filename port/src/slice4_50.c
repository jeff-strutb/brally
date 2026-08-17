/* slice4_50.c -- a later pass, slice 4. See slice4_50.h for the packet's
 * mispaired-listing problem, the three forwarders, and the two skips.
 *
 * Every function here was read out of the asm/ dumps at the address its WANTED
 * name encodes, and carries that address in its comment.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "slice4_50.h"

/* ==========================================================================
 * Cross-slice callees. Each is already declared, with this exact signature,
 * by the header named beside it; repeated here rather than including those
 * headers so that this translation unit stays free of their type models.
 * ========================================================================== */

/* XSLICE 0x10035BBA -- slice2_19.h */
extern void     BrLogSet(void *p);
/* XSLICE 0x10072AF0 -- slice1_08.h */
extern int32_t  BrSndPlaySimple(int32_t group, uint32_t packed);
/* XSLICE 0x1007DFE0 -- slice2_26.h / slice3_33.h. operator new; does NOT
 * zero the block (contract). */
extern void    *BrOperatorNew(uint32_t cb);
/* XSLICE 0x100419D0 -- slice2_26.h */
extern void     BrExt_100419D0(void *p);
/* XSLICE 0x10005470 -- slice2_12.h. The original reads its two operands from
 * 0x10ACEDB0 and 0x100B36FC; that port takes them as parameters. */
extern uint32_t BrEntityCountActive(const void *pvRecords, int32_t cRecords);
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

void            *g_brHook6C0964 = NULL;
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
/* @implements 0x1007C830 d3d BrSprintf */
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
 * 2. Sound
 * ========================================================================== */

/* 0x10072AF0 */
/* WHAT IT DOES: plays a sound effect from one of the game's sound banks, in
 * the simple case where the caller does not care about the result. A second
 * name for a routine whose body lives in the sound module. */
/* @implements 0x10072AF0 d3d BrSub10072AF0 */
void BrSub10072AF0(int a, int b)
{
    /* BrSndPlayGroup(a, b, 0) == BrSndPlayEx(a, 1, b, 0). The two callers
     * discard the result. */
    (void)BrSndPlaySimple((int32_t)a, (uint32_t)b);
}

/* ==========================================================================
 * 3. Hooks
 * ========================================================================== */

/* 0x10034C51 */
/* WHAT IT DOES: answers whether a particular screen or handler is the one
 * currently installed -- how a menu asks "am I the one on screen right now?"
 * before acting. */
/* @implements 0x10034C51 d3d BrHookIsCurrent */
int BrHookIsCurrent(const void *pfn)
{
    return (g_brHook6C0964 == pfn) ? 1 : 0;
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
int BrMat4Perspective7(BrMat4 *pM, uint16_t *pPerspNorm,
                       float fovyDegrees, float aspect,
                       float n, float f, float scale)
{
    /* `scale` is pushed by the original and never read by 0x10030810; see the
     * header. Consumed here so the argument list stays the caller's. */
    (void)scale;

    /* DEVIATION: returns BrMat4Frustum's status rather than the reloaded
     * pPerspNorm pointer the original happens to leave in eax. */
    return BrMat4Perspective(pM, (unsigned short *)pPerspNorm,
                             fovyDegrees, aspect, n, f);
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
/* @implements 0x10044E20 d3d BrMenuSub10044E20 */
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
/* @implements 0x10043BF0 d3d BrSub10043BF0 */
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

    /* Taken and immediately released -- a rendezvous, not a critical
     * section. See the header. */
    BrNetMutexLock(g_brH221324);
    BrNetMutexUnlock(g_brH221324);

    if (g_br22AAA8 == 0) {
        return;
    }

    cActive = BrEntityCountActive(g_brPACEDB0, g_br0B36FC);
    if (cActive != BrDPlayGetCurrentPlayers()) {
        return;
    }

    /* The original pushes the ADDRESS 0x10277B40, i.e. the pointer global
     * itself, not the interface it holds. */
    BrNetSend4760(&g_brP277B40, g_br094294, g_br22B34C,
                  g_brAD0854[0], g_brAD0854[1], g_brAD0854[2],
                  g_br277B48, g_brPB4E2E8, 3, 0);
}

/* ==========================================================================
 * 7. DirectPlay host / join / send
 * ========================================================================== */

/* 0x1003C150 */
/* WHAT IT DOES: sets this machine up as the host of a network game, so other
 * players can find and join it. If hosting fails it composes an explanatory
 * message and then throws it away without showing it to anyone -- the player
 * sees nothing. */
/* @implements 0x1003C150 d3d BrSub1003C150 */
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

/* 0x1003D950 */
/* WHAT IT DOES: sends one small message to the other players from a menu
 * screen -- a fixed tag and one number. What the number means was not
 * established. Whether it sends at all is gated on a global, so the same call
 * can silently do nothing. */
/* @implements 0x1003D950 d3d BrSub1003D950 */
void BrSub1003D950(BrOptUi *pUi, int a)
{
    /* CONFLICT: slice2_25.h models BrOptUi as three int32_t, but +0x00 is
     * dereferenced as an object pointer and +0x08 is passed on as a pointer.
     * A pointer-sized view of the same object is used so a 64-bit host does
     * not truncate them; see the header. */
    void *const *aSlot = (void *const *)pUi;
    void        *pObj;
    void        *pArg;
    int32_t      aPacket[2];

    if (pUi == NULL) {
        return;
    }
    pObj = aSlot[0];
    if (pObj == NULL) {
        return;
    }
    if (g_brAA288C != 0) {
        return;
    }
    pArg = aSlot[2];

    aPacket[0] = (int32_t)0x60000002u;
    aPacket[1] = (int32_t)a;

    /* (pObj, pArg, 0, 1, &packet, 8) -- IDirectPlay4A::Send through
     * slice1_03.h's critical-section wrapper. The original discards the
     * HRESULT. */
    (void)BrComCallLocked68((BrComObj *)pObj, pArg,
                            (void *)(uintptr_t)0u,
                            (void *)(uintptr_t)1u,
                            aPacket,
                            (void *)(uintptr_t)8u);
}

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
