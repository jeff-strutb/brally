/* test_slice8_86.c -- packet 86.
 *
 * Everything slice8_86.c forwards to is a STAND-IN defined here, because the
 * point of an adapter test is "did the right callee get the right arguments",
 * which a real callee would only obscure. Nothing else is linked; see
 * build.d/test_slice8_86.deps.
 *
 * The assertions are properties of the original, not volume:
 *
 *   - the NULL binding is EXACTLY the retired stub (nothing runs, nothing is
 *     written), which is the claim the whole host-binding pattern rests on;
 *   - each of the five enter hooks reaches ITS OWN body -- a copy-paste swap
 *     between two of five near-identical adapters is the failure this module
 *     is most exposed to, and only a per-hook identity check catches it;
 *   - 0x1003D0B0's ownership rule: the block is freed on EVERY exit except
 *     the one that hands it to *ppOut, and *ppOut is untouched on all of them;
 *   - 0x1003D0B0 fetches the vtable slot ONCE (the original keeps it in ebp
 *     across both calls), checked by rewriting the vtable between the two;
 *   - the peer clear is bounded by `state < 5` at the boundary values 4 and 5,
 *     matches on id, clears the WHOLE dword and reaches record 15;
 *   - 0x10072270's read-before-clear order for the wake handle;
 *   - the timer probes ONCE and latches, checked by changing the platform's
 *     answer between two calls and asserting the second still takes the first
 *     answer's branch.
 *
 * WHY THE TIMER'S COUNTER BRANCH IS IN ITS OWN BINARY. `g_br86Probed` is a
 * latch in the original too, so within one process only one of the two
 * branches can ever run -- and a branch that never runs is not tested
 * (CONVENTIONS.md). This file drives the NO-COUNTER branch;
 * port/tests/test_slice8_86_perf.c drives the counter branch, and neither
 * would be reachable from the other.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice8_86.h"
#include "slice7_82.h"

/* 0x1002C2C0 loads the address of the 0x106806B0 frame-timer object as an
 * immediate and passes it in ecx.  The object itself lives in slice2_17.c,
 * which this test does not link, so it needs storage here. */
unsigned char g_br6806B0[0x24];

static int g_fail;

#define CHECK(cond, ...)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                      \
            printf(__VA_ARGS__);                                             \
            printf("\n");                                                    \
            g_fail++;                                                        \
        }                                                                    \
    } while (0)

/* C99 has no _Static_assert; the tree uses the negative-array-size trick.
 * The two vtables are 2 and 9 slots -- the extents pinned in the header. */
typedef char br86_assert_page_vtbl [(sizeof(BrUiPageVtbl)    == 2 * sizeof(void *)) ? 1 : -1];
typedef char br86_assert_phase_vtbl[(sizeof(BrPhaseFullVtbl) == 9 * sizeof(void *)) ? 1 : -1];

/* ==================================================================== */
/* Stand-ins                                                             */
/* ==================================================================== */

/* --- slice3_33.c's five builders ------------------------------------ */
static int          g_nBuild[5];
static BrUiBuildCtx *g_lastCtx;
static BrUiPhase    *g_lastPhase;

void BrExt_1004A580(BrUiBuildCtx *c, BrUiPhase *p)
{ g_nBuild[0]++; g_lastCtx = c; g_lastPhase = p; }
void BrExt_1004B430(BrUiBuildCtx *c, BrUiPhase *p)
{ g_nBuild[1]++; g_lastCtx = c; g_lastPhase = p; }
void BrExt_1004BDC0(BrUiBuildCtx *c, BrUiPhase *p)
{ g_nBuild[2]++; g_lastCtx = c; g_lastPhase = p; }
void BrExt_1004C4A0(BrUiBuildCtx *c, BrUiPhase *p)
{ g_nBuild[3]++; g_lastCtx = c; g_lastPhase = p; }
void BrExt_1004CAC0(BrUiBuildCtx *c, BrUiPhase *p)
{ g_nBuild[4]++; g_lastCtx = c; g_lastPhase = p; }

/* --- br_audio.c ------------------------------------------------------ */
static int g_nPlay, g_lastTrack;

int BrAudioPlayTrack(BrAudio *pAudio, int iTrack)
{ (void)pAudio; g_nPlay++; g_lastTrack = iTrack; return 0; }

/* --- slice7_82.c's GMEM_FIXED half ----------------------------------- */
static int   g_nUnlock, g_nFree;
static void *g_lastFreed;

void *BrGlobalHandle(void *pMem) { return pMem; }          /* identity */
int   BrGlobalUnlock(void *hMem) { (void)hMem; g_nUnlock++; return 0; }
void *BrGlobalFree(void *hMem)   { g_nFree++; g_lastFreed = hMem;
                                   free(hMem); return NULL; }

/* --- slice7_82.c's clocks -------------------------------------------- */
static int32_t g_perfAnswer;        /* what QueryPerfFreq is told to report */
static int     g_nFreqCalls;
static uint32_t g_fakeMs = 12345;

int32_t BrPlatQueryPerfFreq(int64_t *pFreq)
{ g_nFreqCalls++; *pFreq = 3000000; return g_perfAnswer; }
int32_t BrPlatQueryPerfCounter(int64_t *pCount)
{ *pCount = 777; return 1; }
uint32_t BrPlatTimeGetTime(void) { return g_fakeMs; }

/* --- slice3_32.c's vtable bodies ------------------------------------- */
static int g_nPageDel, g_nPageFrame, g_nPhaseDel, g_nPhaseFn;
static int g_nPhaseTick, g_nPhaseRun, g_nPhaseRel, g_nPhaseShut;
static BrScrGlobals *g_lastG;

void *BrUiPageDelete_100484C0(BrUiPage *p, int32_t n)
{ (void)n; g_nPageDel++; return p; }
int BrUiPageFrame_10048530(BrScrGlobals *g, BrUiPage *p)
{ (void)p; g_nPageFrame++; g_lastG = g; return 7; }
void *BrPhaseDelete_10048850(BrPhaseFull *p, int32_t n)
{ (void)n; g_nPhaseDel++; return p; }
int BrPhaseFn_100488B0(BrPhaseFull *p)
{ (void)p; g_nPhaseFn++; return 1; }
int BrPhaseTick_100488C0(BrScrGlobals *g, BrPhaseFull *p)
{ (void)p; g_nPhaseTick++; g_lastG = g; return 3; }
int BrPhaseRun_100489A0(BrScrGlobals *g, BrPhaseFull *p)
{ (void)p; g_nPhaseRun++; g_lastG = g; return 5; }
void BrPhaseReleasePages_10048AA0(BrScrGlobals *g, BrPhaseFull *p)
{ (void)p; g_nPhaseRel++; g_lastG = g; }
void BrPhaseShutdown_10048B20(BrScrGlobals *g, void *pArg)
{ (void)pArg; g_nPhaseShut++; g_lastG = g; }

/* --- the OS hooks ---------------------------------------------------- */
static int   g_nSet, g_nWait, g_nClose, g_nBegin, g_nEnd;
static void *g_aClosed[4];
static int   g_nLockPeer, g_nUnlockPeer;

static void os_set(void *h)   { (void)h; g_nSet++; }
static void os_wait(void *h)  { (void)h; g_nWait++; }
static void os_close(void *h) { if (g_nClose < 4) g_aClosed[g_nClose] = h;
                                g_nClose++; }
static void os_lock(uint32_t h)   { (void)h; g_nLockPeer++; }
static void os_unlock(uint32_t h) { (void)h; g_nUnlockPeer++; }
static void os_begin(uint32_t ms) { (void)ms; g_nBegin++; }
static void os_end(uint32_t ms)   { (void)ms; g_nEnd++; }

static const BrPlatOs86 g_os = {
    os_set, os_wait, os_close, os_lock, os_unlock, os_begin, os_end
};

/* ==================================================================== */
/* 1. The five enter hooks                                               */
/* ==================================================================== */

static void test_enter_hooks(void)
{
    BrUiBuildCtx ctx;
    BrUiPhase    phase;
    int i;

    memset(&ctx, 0, sizeof ctx);
    memset(&phase, 0, sizeof phase);

    /* Unbound: the retired stub's behaviour exactly -- nothing runs. */
    g_pBrUiBuildCtx86 = NULL;
    memset(g_nBuild, 0, sizeof g_nBuild);
    BrPhaseEnterPlaceholder_1004A580(&phase);
    BrPhaseEnterPlaceholder_1004B430(&phase);
    BrPhaseEnterPlaceholder_1004BDC0(&phase);
    BrPhaseEnterPlaceholder_1004C4A0(&phase);
    BrOptFn1004CAC0(&phase);
    for (i = 0; i < 5; ++i)
        CHECK(g_nBuild[i] == 0, "unbound hook %d ran", i);

    /* Bound: each hook reaches ITS OWN body, once, with both arguments. */
    g_pBrUiBuildCtx86 = &ctx;

    memset(g_nBuild, 0, sizeof g_nBuild);
    g_lastCtx = NULL; g_lastPhase = NULL;
    BrPhaseEnterPlaceholder_1004A580(&phase);
    CHECK(g_nBuild[0] == 1 && g_nBuild[1] + g_nBuild[2] + g_nBuild[3]
                              + g_nBuild[4] == 0,
          "0x1004A580 -> BrExt_1004A580 alone");
    CHECK(g_lastCtx == &ctx && g_lastPhase == &phase,
          "0x1004A580 forwards (ctx, phase)");

    memset(g_nBuild, 0, sizeof g_nBuild);
    BrPhaseEnterPlaceholder_1004B430(&phase);
    CHECK(g_nBuild[1] == 1 && g_nBuild[0] + g_nBuild[2] + g_nBuild[3]
                              + g_nBuild[4] == 0,
          "0x1004B430 -> BrExt_1004B430 alone");

    memset(g_nBuild, 0, sizeof g_nBuild);
    BrPhaseEnterPlaceholder_1004BDC0(&phase);
    CHECK(g_nBuild[2] == 1 && g_nBuild[0] + g_nBuild[1] + g_nBuild[3]
                              + g_nBuild[4] == 0,
          "0x1004BDC0 -> BrExt_1004BDC0 alone");

    memset(g_nBuild, 0, sizeof g_nBuild);
    BrPhaseEnterPlaceholder_1004C4A0(&phase);
    CHECK(g_nBuild[3] == 1 && g_nBuild[0] + g_nBuild[1] + g_nBuild[2]
                              + g_nBuild[4] == 0,
          "0x1004C4A0 -> BrExt_1004C4A0 alone");

    memset(g_nBuild, 0, sizeof g_nBuild);
    BrOptFn1004CAC0(&phase);
    CHECK(g_nBuild[4] == 1 && g_nBuild[0] + g_nBuild[1] + g_nBuild[2]
                              + g_nBuild[3] == 0,
          "0x1004CAC0 -> BrExt_1004CAC0 alone");

    g_pBrUiBuildCtx86 = NULL;
}

/* ==================================================================== */
/* 2. The two CD entry points                                            */
/* ==================================================================== */

static void test_cd(void)
{
    BrAudio audio;

    memset(&audio, 0, sizeof audio);

    g_pBrAudio86 = NULL;
    g_nPlay = 0;
    BrSub10002870(2);
    BrSub100027F0(2);
    CHECK(g_nPlay == 0, "unbound CD path played");

    g_pBrAudio86 = &audio;
    CHECK(g_iBrCdFirstTrack86 == BR86_CD_FIRST_MUSIC_TRACK,
          "the default base is the retail disc's first music track");

    /* CD numbering in, 0-based index out -- the seam br_audio.h names. */
    g_nPlay = 0;
    BrSub10002870(2);
    CHECK(g_nPlay == 1 && g_lastTrack == 0, "CD track 2 is index 0 (got %d)",
          g_lastTrack);
    BrSub100027F0(13);
    CHECK(g_nPlay == 2 && g_lastTrack == 11, "CD track 13 is index 11 (got %d)",
          g_lastTrack);

    /* The base is a variable so a soundtrack with no data track works. */
    g_iBrCdFirstTrack86 = 0;
    BrSub100027F0(3);
    CHECK(g_lastTrack == 3, "base 0 passes the track through (got %d)",
          g_lastTrack);
    g_iBrCdFirstTrack86 = BR86_CD_FIRST_MUSIC_TRACK;

    g_pBrAudio86 = NULL;
}

/* ==================================================================== */
/* 3. GlobalAlloc / GlobalLock                                           */
/* ==================================================================== */

static void test_global_alloc(void)
{
    unsigned char *p;
    int i, nz = 0;

    p = (unsigned char *)BrGlobalAlloc(
            BR86_GMEM_MOVEABLE | BR86_GMEM_ZEROINIT, 64);
    CHECK(p != NULL, "GlobalAlloc(64) returned NULL");
    if (p != NULL) {
        for (i = 0; i < 64; ++i)
            nz += (p[i] != 0);
        CHECK(nz == 0, "GMEM_ZEROINIT left %d non-zero bytes", nz);

        /* GMEM_FIXED model: lock is the identity and handle round-trips. */
        CHECK(BrGlobalLock(p) == p, "GlobalLock is not the identity");
        CHECK(BrGlobalHandle(BrGlobalLock(p)) == p, "handle round trip");

        g_nFree = 0;
        (void)BrGlobalFree(BrGlobalHandle(p));
        CHECK(g_nFree == 1, "GlobalFree not reached");
    }

    /* A zero-length request must not read as failure to the callers, all of
     * which test the pointer. */
    p = (unsigned char *)BrGlobalAlloc(BR86_GMEM_ZEROINIT, 0);
    CHECK(p != NULL, "GlobalAlloc(0) returned NULL");
    (void)BrGlobalFree(BrGlobalHandle(p));
}

/* ==================================================================== */
/* 4. 0x1003D0B0                                                         */
/* ==================================================================== */

/* A stand-in host object: a vtable of 31 reserved slots then f7C, matching
 * slice2_26.h's BrHostVtbl, with slot +0x58 == index 22 filled in. */
typedef int32_t (*Br86GetDescFn)(void *pThis, void *pvBuf, uint32_t *pcb);

typedef struct FakeHostVtbl { void *aSlots[32]; } FakeHostVtbl;
typedef struct FakeHost     { const FakeHostVtbl *pVtbl; } FakeHost;

static int      g_nDesc;
static uint32_t g_descSize;         /* what call 1 reports */
static int32_t  g_hr1, g_hr2;       /* what calls 1 and 2 return */
static void    *g_descBuf;          /* the buffer call 2 was handed */
static const FakeHostVtbl *g_swapTo;/* rewritten into the object mid-call */
static FakeHost *g_pFake;

static int32_t desc_a(void *pThis, void *pvBuf, uint32_t *pcb)
{
    (void)pThis;
    g_nDesc++;
    if (pvBuf == NULL) {
        *pcb = g_descSize;
        if (g_swapTo != NULL)       /* the "fetched once" probe */
            g_pFake->pVtbl = g_swapTo;
        return g_hr1;
    }
    g_descBuf = pvBuf;
    return g_hr2;
}

static int32_t desc_poison(void *pThis, void *pvBuf, uint32_t *pcb)
{
    (void)pThis; (void)pvBuf; (void)pcb;
    CHECK(0, "the second call went through a RE-READ vtable slot");
    return 0;
}

static FakeHostVtbl g_vtblA, g_vtblPoison;

static void fake_host_init(FakeHost *h)
{
    memset(&g_vtblA, 0, sizeof g_vtblA);
    memset(&g_vtblPoison, 0, sizeof g_vtblPoison);
    g_vtblA.aSlots[0x58 / 4]      = (void *)desc_a;
    g_vtblPoison.aSlots[0x58 / 4] = (void *)desc_poison;
    h->pVtbl = &g_vtblA;
    g_pFake  = h;
    g_swapTo = NULL;
}

static void test_1003D0B0(void)
{
    FakeHost h;
    void    *pOut;

    /* (a) the first call does not report BUFFERTOOSMALL: no allocation, no
     *     free, *ppOut untouched. */
    fake_host_init(&h);
    g_nDesc = 0; g_nFree = 0; g_descSize = 16; g_hr1 = 0; g_hr2 = 0;
    pOut = (void *)(uintptr_t)0xABCD;
    BrExt_1003D0B0(&h, &pOut);
    CHECK(g_nDesc == 1, "sized once (got %d)", g_nDesc);
    CHECK(pOut == (void *)(uintptr_t)0xABCD, "*ppOut written on the early out");
    CHECK(g_nFree == 0, "freed something it never allocated");

    /* (b) the happy path: *ppOut receives the block and it is NOT freed. */
    fake_host_init(&h);
    g_nDesc = 0; g_nFree = 0; g_nUnlock = 0;
    g_descSize = 40; g_hr1 = BR86_DPERR_BUFFERTOOSMALL; g_hr2 = 0;
    pOut = NULL; g_descBuf = NULL;
    BrExt_1003D0B0(&h, &pOut);
    CHECK(g_nDesc == 2, "two calls on the happy path (got %d)", g_nDesc);
    CHECK(pOut != NULL && pOut == g_descBuf,
          "*ppOut is the block the callee filled");
    CHECK(g_nFree == 0 && g_nUnlock == 0,
          "the block handed to *ppOut was freed anyway");
    free(pOut);                     /* the caller owns it now */

    /* (c) the second call fails: *ppOut untouched AND the block is released
     *     through the Unlock(Handle(p)) / Free(Handle(p)) pair. */
    fake_host_init(&h);
    g_nDesc = 0; g_nFree = 0; g_nUnlock = 0; g_lastFreed = NULL;
    g_descSize = 40; g_hr1 = BR86_DPERR_BUFFERTOOSMALL;
    g_hr2 = (int32_t)0x80004005;
    pOut = NULL; g_descBuf = NULL;
    BrExt_1003D0B0(&h, &pOut);
    CHECK(pOut == NULL, "*ppOut written on the failure path");
    CHECK(g_nUnlock == 1 && g_nFree == 1, "block not released on failure");
    CHECK(g_lastFreed == g_descBuf, "released a different block");

    /* (d) the slot is fetched ONCE: rewriting the vtable from inside the
     *     first call must not change where the second one goes. */
    fake_host_init(&h);
    g_nDesc = 0; g_nFree = 0;
    g_descSize = 8; g_hr1 = BR86_DPERR_BUFFERTOOSMALL; g_hr2 = 0;
    g_swapTo = &g_vtblPoison;
    pOut = NULL;
    BrExt_1003D0B0(&h, &pOut);
    g_swapTo = NULL;
    CHECK(h.pVtbl == &g_vtblPoison, "the probe did not actually swap");
    CHECK(g_nDesc == 2, "the second call did not happen");
    free(pOut);

    /* (e) NULL arguments are inert. */
    BrExt_1003D0B0(NULL, &pOut);
    BrExt_1003D0B0(&h, NULL);
}

/* ==================================================================== */
/* 5. 0x10071480                                                         */
/* ==================================================================== */

static void test_peer_clear(void)
{
    static BrPeer aPeer[BR_PEER_COUNT];
    int i;

    g_aBrPeer86 = NULL;
    BrSub10071480(9);               /* must not crash or touch anything */

    memset(aPeer, 0, sizeof aPeer);
    for (i = 0; i < BR_PEER_COUNT; ++i) {
        aPeer[i].hMutex = (uint32_t)(0x100 + i);
        aPeer[i].f04    = 9;        /* every record matches the id */
        aPeer[i].f2C    = 0x12340000u | (uint32_t)(i & 0x3F);
    }
    g_aBrPeer86   = aPeer;
    g_pBrPlatOs86 = &g_os;
    g_nLockPeer = g_nUnlockPeer = 0;

    BrSub10071480(9);

    /* The lock is taken and released once per record, unconditionally. */
    CHECK(g_nLockPeer == BR_PEER_COUNT && g_nUnlockPeer == BR_PEER_COUNT,
          "lock/unlock not once per record (%d/%d)", g_nLockPeer,
          g_nUnlockPeer);

    /* state < 5 clears; the boundary is 4 in and 5 out. */
    for (i = 0; i < BR_PEER_COUNT; ++i) {
        if ((i & 0x3F) < 5)
            CHECK(aPeer[i].f2C == 0, "record %d (state %d) not cleared", i,
                  i & 0x3F);
        else
            CHECK(aPeer[i].f2C == (0x12340000u | (uint32_t)(i & 0x3F)),
                  "record %d (state %d) cleared", i, i & 0x3F);
    }
    CHECK(aPeer[4].f2C == 0, "state 4 must clear");
    CHECK(aPeer[5].f2C != 0, "state 5 must NOT clear");

    /* The WHOLE dword goes, not just the masked bits. */
    aPeer[0].f2C = 0xDEADBE03u;
    BrSub10071480(9);
    CHECK(aPeer[0].f2C == 0, "only the low bits were cleared (0x%08X)",
          aPeer[0].f2C);

    /* A non-matching id is left entirely alone -- including record 15, which
     * is what proves the loop reaches the end of the table. */
    for (i = 0; i < BR_PEER_COUNT; ++i)
        aPeer[i].f2C = 0x00000003u;
    aPeer[BR_PEER_COUNT - 1].f04 = 77;
    BrSub10071480(77);
    CHECK(aPeer[BR_PEER_COUNT - 1].f2C == 0, "record 15 never visited");
    CHECK(aPeer[0].f2C == 0x00000003u, "a non-matching record was cleared");

    g_aBrPeer86 = NULL;
}

/* ==================================================================== */
/* 6. 0x10072270                                                         */
/* ==================================================================== */

static void test_thread_stop(void)
{
    void *hWake   = (void *)(uintptr_t)0x1111;
    void *hThread = (void *)(uintptr_t)0x2222;

    g_pBrPlatOs86 = &g_os;

    /* Not running: the whole body is skipped and the handles survive. */
    g_fBrSndThread86 = 0;
    g_hBrSndWake86   = hWake;
    g_hBrSndThread86 = hThread;
    g_nSet = g_nWait = g_nClose = 0;
    BrSub10072270();
    CHECK(g_nSet == 0 && g_nWait == 0 && g_nClose == 0,
          "not-running path did work");
    CHECK(g_hBrSndWake86 == hWake && g_hBrSndThread86 == hThread,
          "not-running path cleared the handles");

    /* Running: wake, join, close both, clear all three. */
    g_fBrSndThread86 = 1;
    g_nSet = g_nWait = g_nClose = 0;
    memset(g_aClosed, 0, sizeof g_aClosed);
    BrSub10072270();
    CHECK(g_nSet == 1 && g_nWait == 1 && g_nClose == 2,
          "set/wait/close = %d/%d/%d", g_nSet, g_nWait, g_nClose);
    /* The thread handle closes first; the wake handle is read BEFORE the
     * thread handle is cleared and closed after it -- the order the listing
     * has at 0x1006B214..0x1006B224. */
    CHECK(g_aClosed[0] == hThread, "thread handle not closed first");
    CHECK(g_aClosed[1] == hWake,
          "wake handle closed with the wrong value (read-before-clear)");
    CHECK(g_hBrSndWake86 == NULL && g_hBrSndThread86 == NULL
          && g_fBrSndThread86 == 0, "state not fully cleared");

    /* A second stop is a no-op, because the flag is now zero. */
    g_nSet = g_nWait = g_nClose = 0;
    BrSub10072270();
    CHECK(g_nSet == 0 && g_nClose == 0, "double stop did work");
}

/* ==================================================================== */
/* 7. The vtables, and 0x100484E0                                        */
/* ==================================================================== */

static void test_vtables(void)
{
    BrUiPage    page;
    BrPhaseFull phase;

    /* The slots that need no thunk are the real bodies, by identity. */
    CHECK(BrUiPageVtbl_1008F6F8.f00 == BrUiPageDelete_100484C0,
          "page +0x00 != 0x100484C0");
    CHECK(BrPhaseVtbl_1008F700.f00 == BrPhaseDelete_10048850,
          "phase +0x00 != 0x10048850");
    CHECK(BrPhaseVtbl_1008F700.f04 == BrPhaseFn_100488B0,
          "phase +0x04 != 0x100488B0");

    /* The three slots with no body anywhere in port/ are NULL, deliberately,
     * so a dispatch faults visibly instead of jumping into a stub's code. */
    CHECK(BrPhaseVtbl_1008F700.f10 == NULL, "+0x10 should be NULL");
    CHECK(BrPhaseVtbl_1008F700.f14 == NULL, "+0x14 should be NULL");
    CHECK(BrPhaseVtbl_1008F700.f20 == NULL, "+0x20 should be NULL");

    /* Unbound thunks answer 0 and run nothing -- the retired stub's answer. */
    g_pBrScrGlobals86 = NULL;
    g_nPageFrame = g_nPhaseTick = g_nPhaseRun = 0;
    g_nPhaseRel  = g_nPhaseShut = 0;
    memset(&page, 0, sizeof page);
    memset(&phase, 0, sizeof phase);
    CHECK(BrUiPageVtbl_1008F6F8.f04(&page) == 0, "unbound page +0x04 != 0");
    CHECK(BrPhaseVtbl_1008F700.f08(&phase) == 0, "unbound phase +0x08 != 0");
    CHECK(BrPhaseVtbl_1008F700.f0C(&phase) == 0, "unbound phase +0x0C != 0");
    BrPhaseVtbl_1008F700.f18(&phase, NULL);
    BrPhaseVtbl_1008F700.f1C(&phase);
    CHECK(g_nPageFrame + g_nPhaseTick + g_nPhaseRun + g_nPhaseRel
          + g_nPhaseShut == 0, "an unbound thunk called its body");

    /* Bound: each thunk reaches its own body and passes the globals through,
     * and the return value is the body's, not a constant. */
    {
        static BrScrGlobals g;
        memset(&g, 0, sizeof g);
        g_pBrScrGlobals86 = &g;

        g_lastG = NULL;
        CHECK(BrUiPageVtbl_1008F6F8.f04(&page) == 7, "page +0x04 result");
        CHECK(g_nPageFrame == 1 && g_lastG == &g, "page +0x04 -> 0x10048530");

        g_lastG = NULL;
        CHECK(BrPhaseVtbl_1008F700.f08(&phase) == 3, "phase +0x08 result");
        CHECK(g_nPhaseTick == 1 && g_lastG == &g, "phase +0x08 -> 0x100488C0");

        g_lastG = NULL;
        CHECK(BrPhaseVtbl_1008F700.f0C(&phase) == 5, "phase +0x0C result");
        CHECK(g_nPhaseRun == 1 && g_lastG == &g, "phase +0x0C -> 0x100489A0");

        g_lastG = NULL;
        BrPhaseVtbl_1008F700.f18(&phase, NULL);
        CHECK(g_nPhaseShut == 1 && g_lastG == &g, "phase +0x18 -> 0x10048B20");

        g_lastG = NULL;
        BrPhaseVtbl_1008F700.f1C(&phase);
        CHECK(g_nPhaseRel == 1 && g_lastG == &g, "phase +0x1C -> 0x10048AA0");

        g_pBrScrGlobals86 = NULL;
    }

    /* 0x100484E0 re-seats the page vtable, and the value it installs is the
     * real table -- which is the whole point of landing the two together. */
    page.pVtbl = NULL;
    BrSub100484E0(&page);
    CHECK(page.pVtbl == &BrUiPageVtbl_1008F6F8, "0x100484E0 stored the wrong vtable");
    CHECK(page.pVtbl->f00 == BrUiPageDelete_100484C0,
          "a dispatch through the stored vtable does not reach 0x100484C0");
    BrSub100484E0(NULL);            /* inert */
}

/* ==================================================================== */
/* 8. The timer -- NO-COUNTER branch, and the probe latch                */
/* ==================================================================== */

static void test_timer_no_counter(void)
{
    unsigned char obj[0x24];
    int32_t v;

    g_pBrPlatOs86 = &g_os;
    g_perfAnswer  = 0;              /* no high-resolution counter */
    g_nFreqCalls  = g_nBegin = g_nEnd = 0;

    memset(obj, 0xCC, sizeof obj);
    BrX100751D0(obj);

    CHECK(g_nFreqCalls == 1, "the frequency was probed %d times", g_nFreqCalls);
    CHECK(g_nBegin == 1, "timeBeginPeriod not called on the no-counter path");

    memcpy(&v, obj + BR86_TMR_PERIOD_MS, sizeof v);
    CHECK(v == 0x21, "period-ms is %d, not 0x21", v);
    memcpy(&v, obj + BR86_TMR_DUE_MS, sizeof v);
    CHECK(v == 0x21, "due-ms not seeded from period-ms (%d)", v);
    memcpy(&v, obj + BR86_TMR_NOW_MS, sizeof v);
    CHECK(v == (int32_t)g_fakeMs, "now-ms is %d, not the clock", v);

    /* THE LATCH: the platform now claims a counter exists, and the second
     * call must still take the first answer's branch, because the original
     * probes once and stores the result. */
    g_perfAnswer = 1;
    g_fakeMs     = 999;
    g_nBegin     = 0;
    BrX100751D0(obj);
    CHECK(g_nFreqCalls == 1, "the probe was repeated (%d)", g_nFreqCalls);
    CHECK(g_nBegin == 1, "the latched branch changed");
    memcpy(&v, obj + BR86_TMR_NOW_MS, sizeof v);
    CHECK(v == 999, "the restart did not re-read the clock (%d)", v);

    /* The teardown fires timeEndPeriod only when there is no counter. */
    g_nEnd = 0;
    BrX1002C2C0();
    CHECK(g_nEnd == 1, "timeEndPeriod not called on the no-counter path");

    BrX100751D0(NULL);              /* inert */

    /* With no OS hooks at all the state writes still happen -- that is what
     * makes a NULL binding safe rather than merely quiet. */
    g_pBrPlatOs86 = NULL;
    memset(obj, 0, sizeof obj);
    BrX100751D0(obj);
    memcpy(&v, obj + BR86_TMR_PERIOD_MS, sizeof v);
    CHECK(v == 0x21, "period-ms not written without OS hooks (%d)", v);
    BrX1002C2C0();
}

int main(void)
{
    test_enter_hooks();
    test_cd();
    test_global_alloc();
    test_1003D0B0();
    test_peer_clear();
    test_thread_stop();
    test_vtables();
    test_timer_no_counter();

    if (g_fail == 0)
        printf("test_slice8_86: all checks passed, 0 failures\n");
    else
        printf("test_slice8_86: %d failures\n", g_fail);
    return g_fail != 0;
}
