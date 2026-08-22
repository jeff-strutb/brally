/* slice8_86.c -- packet 86. See port/include/slice8_86.h for the demand
 * ranking, the arity adjudication, the signature conflicts and the decline
 * list. This file is the transcription and nothing else.
 *
 * REFERENCE: orig/BRGlide.dll, cross-checked against orig/BRD3D.dll. Every
 * address named in a banner is the D3D one, because that is the numbering the
 * rest of port/ uses; the Glide address actually read is given beside it.
 */
#include <stdlib.h>
#include <string.h>

#include "slice8_86.h"
#include "slice7_82.h"   /* BrGlobalHandle/Unlock/Free, BrPlat* clocks */
#include "br_match.h"    /* BR_THISCALL1 */

/* The frame-timer object itself, defined in slice2_17.c. 0x1002C2C0 plants
 * its ADDRESS in ecx as an immediate, so the object has to be named here --
 * routing through a pointer variable would cost a `mov eax, [mem]`. */
extern unsigned char g_br6806B0[0x24];

/* slice3_33.c's five bodies. Declared by slice3_33.h, which slice8_86.h
 * already pulls in. */

/* ==========================================================================
 * 0. Host bindings -- NULL means "behave exactly like the retired stub"
 * ========================================================================== */

BrUiBuildCtx  *g_pBrUiBuildCtx86  = NULL;
BrAudio       *g_pBrAudio86       = NULL;
BrPeer        *g_aBrPeer86        = NULL;
BrScrGlobals  *g_pBrScrGlobals86  = NULL;

const BrPlatOs86 *g_pBrPlatOs86 = NULL;

int g_iBrCdFirstTrack86 = BR86_CD_FIRST_MUSIC_TRACK;

/* ==========================================================================
 * 1. 0x1004A580 / 0x1004B430 / 0x1004BDC0 / 0x1004C4A0 / 0x1004CAC0
 *    Glide 0x100439B0 / 0x10044860 / 0x100451F0 / 0x100458D0 / 0x10045EF0
 *
 *    ADAPTERS. One argument in the original -- the phase -- and it is
 *    slice3_33.h's BrUiPhase; slice3_33.c's leading BrUiBuildCtx * is an
 *    injected host context, not an original argument. The adjudication is
 *    written out in the header.
 * ========================================================================== */

/* WHAT IT DOES: builds one of the game's menu screens when the player enters
 * it -- laying out its rows of labels and selectable controls. This is only
 * the glue that hands the screen off to the builder that does the work; a
 * host that has not supplied the surrounding menu state gets nothing built. */
/* @implements 0x1004A580 d3d BrPhaseEnterPlaceholder_1004A580 */
void BrPhaseEnterPlaceholder_1004A580(void *pSelf)
{
    if (g_pBrUiBuildCtx86 == NULL)
        return;
    BrExt_1004A580(g_pBrUiBuildCtx86, (BrUiPhase *)pSelf);
}

/* WHAT IT DOES: the same glue for a second menu screen. */
/* @implements 0x1004B430 d3d BrPhaseEnterPlaceholder_1004B430 */
void BrPhaseEnterPlaceholder_1004B430(void *pSelf)
{
    if (g_pBrUiBuildCtx86 == NULL)
        return;
    BrExt_1004B430(g_pBrUiBuildCtx86, (BrUiPhase *)pSelf);
}

/* WHAT IT DOES: the same glue for a third menu screen. */
/* @implements 0x1004BDC0 d3d BrPhaseEnterPlaceholder_1004BDC0 */
void BrPhaseEnterPlaceholder_1004BDC0(void *pSelf)
{
    if (g_pBrUiBuildCtx86 == NULL)
        return;
    BrExt_1004BDC0(g_pBrUiBuildCtx86, (BrUiPhase *)pSelf);
}

/* WHAT IT DOES: the same glue for a fourth menu screen. */
/* @implements 0x1004C4A0 d3d BrPhaseEnterPlaceholder_1004C4A0 */
void BrPhaseEnterPlaceholder_1004C4A0(void *pSelf)
{
    if (g_pBrUiBuildCtx86 == NULL)
        return;
    BrExt_1004C4A0(g_pBrUiBuildCtx86, (BrUiPhase *)pSelf);
}

/* WHAT IT DOES: the same glue for a fifth menu screen -- the one whose rows
 * come out of the game's table of on-screen text. */
/* @implements 0x1004CAC0 d3d BrOptFn1004CAC0 */
void BrOptFn1004CAC0(void *pSelf)
{
    if (g_pBrUiBuildCtx86 == NULL)
        return;
    BrExt_1004CAC0(g_pBrUiBuildCtx86, (BrUiPhase *)pSelf);
}

/* ==========================================================================
 * 2. 0x10002870 and 0x100027F0 -- the two CD entry points
 *    Glide 0x10002BA0 and 0x10002B20
 *
 *    ADAPTERS onto br_audio.c, which is the whole module in one place. The
 *    CD-number-to-index conversion is here because this is the seam; see
 *    the header.
 * ========================================================================== */

/* 0x10002870 -- the Redbook path. It records the track and posts
 * MM_MCINOTIFY (0x3B9) so the notify handler re-issues MCI_PLAY of the SAME
 * track; that "repeat forever" behaviour is br_audio.h's
 * BR_AUDIO_REPEAT_TRACK and is a property of the BrAudio, not of this call.
 *
 * PRESERVED: this path does NOT clamp. The clamp is in the sibling only, and
 * BrAudioPlayTrack applies it on both -- a DEVIATION whose only effect is on
 * an out-of-range track, which the original would have handed to MCI raw. */
void BrSub10002870(int track)
{
    if (g_pBrAudio86 == NULL)
        return;
    (void)BrAudioPlayTrack(g_pBrAudio86, track - g_iBrCdFirstTrack86);
}

/* 0x100027F0 -- the middleware path. Clamps into [first, count] and stores
 * the result BEFORE testing whether the module is running, which is why
 * br_audio.c records the selection even when disabled. */
/* WHAT IT DOES: starts a piece of music playing, counting from the first
 * music track on the disc rather than from track one -- the audio CD's first
 * track is the game's data. This is the route used when the music is going
 * through the game's own audio module rather than straight to Windows. */
void BrSub100027F0(int track)
{
    if (g_pBrAudio86 == NULL)
        return;
    (void)BrAudioPlayTrack(g_pBrAudio86, track - g_iBrCdFirstTrack86);
}

/* ==========================================================================
 * 3. GlobalAlloc / GlobalLock, on slice7_82.c's GMEM_FIXED model
 * ========================================================================== */

void *BrGlobalAlloc(uint32_t uFlags, uint32_t cb)
{
    /* GMEM_ZEROINIT is honoured for real; see the DEVIATION in the header for
     * the case that is not asked for anywhere in this tree. */
    (void)uFlags;

    if (cb == 0) {
        /* GlobalAlloc(.., 0) returns a valid, zero-length handle. calloc(0,1)
         * may return NULL, which every caller here reads as failure, so one
         * byte is allocated instead. */
        cb = 1;
    }
    return calloc((size_t)cb, 1);
}

void *BrGlobalLock(void *hMem)
{
    /* GMEM_FIXED: the locked pointer IS the handle, exactly as
     * slice7_82.c's BrGlobalHandle is the identity in the other direction. */
    return hMem;
}

/* ==========================================================================
 * 4. 0x1003D0B0 -- size it, allocate it, fill it       [8 static call sites]
 *    Glide 0x10036740, 127 bytes
 * ========================================================================== */

/* Host vtable slot +0x58. slice2_26.h's BrHostVtbl models +0x00..+0x78 as
 * `void *aReserved[31]`, so +0x58 is aReserved[22]; reached through a local
 * function type rather than by editing that header. */
typedef int32_t (*Br86HostGetDescFn)(void *pThis, void *pvBuf, uint32_t *pcb);

#define BR86_HOST_SLOT_58  (0x58 / 4)

void BrExt_1003D0B0(void *pHost, void **ppOut)
{
    void *const *const  *ppVtbl;
    Br86HostGetDescFn    pfn;
    unsigned char       *pBlock;
    uint32_t             cb;
    int32_t              hr;

    /* DEVIATION: the original checks neither argument nor the slot. All
     * three are guarded here because the only caller in port/ (slice2_26.c
     * :171) already null-checks pHost before calling, and because the slot
     * lands in a `void *` reserved array that no ported writer fills -- so
     * an unbound host would dereference NULL rather than reach the
     * DirectPlay method the original had. No reachable behaviour changes:
     * every one of these is a case the original could not be handed. */
    if (pHost == NULL || ppOut == NULL) {
        return;
    }

    /* `mov eax,[ebx] / mov ebp,[eax+0x58]` -- the slot is fetched ONCE and
     * both calls go through the same ebp. Preserved: a vtable rewritten
     * between the two calls would not be seen by the second. */
    ppVtbl = (void *const *const *)pHost;
    pfn = (Br86HostGetDescFn)(*ppVtbl)[BR86_HOST_SLOT_58];
    if (pfn == NULL) {
        return;
    }

    /* The original passes the address of its own (dead) first-argument slot
     * as this out-parameter. It is uninitialised there; the callee is
     * expected to write it on the DPERR_BUFFERTOOSMALL path and does. Zeroed
     * here rather than left indeterminate -- a DEVIATION that can only matter
     * on a callee that returns BUFFERTOOSMALL without writing a size, in
     * which case the original allocates a garbage-sized block. */
    cb = 0;
    hr = pfn(pHost, NULL, &cb);
    if (hr != BR86_DPERR_BUFFERTOOSMALL) {
        return;                     /* result discarded -- see the header */
    }

    pBlock = (unsigned char *)BrGlobalLock(
                 BrGlobalAlloc(BR86_GMEM_MOVEABLE | BR86_GMEM_ZEROINIT, cb));
    if (pBlock == NULL) {
        /* hr = E_OUTOFMEMORY, and NOTHING is freed -- esi is still zero at
         * the cleanup test on this arm. */
        return;
    }

    hr = pfn(pHost, pBlock, &cb);
    if (hr >= 0) {
        *ppOut = pBlock;
        pBlock = NULL;              /* `xor esi,esi`: ownership has moved */
    }

    if (pBlock != NULL) {
        (void)BrGlobalUnlock(BrGlobalHandle(pBlock));
        (void)BrGlobalFree(BrGlobalHandle(pBlock));
    }
}

/* ==========================================================================
 * 5. 0x10071480 -- silence one peer's voices
 *    Glide 0x1006A3F0, 79 bytes
 * ========================================================================== */

void BrSub10071480(uint32_t idPlayer)
{
    int i;

    if (g_aBrPeer86 == NULL) {
        return;
    }

    for (i = 0; i < BR_PEER_COUNT; ++i) {
        BrPeer *p = &g_aBrPeer86[i];

        if (g_pBrPlatOs86 != NULL && g_pBrPlatOs86->pfnLockPeer != NULL) {
            g_pBrPlatOs86->pfnLockPeer(p->hMutex);
        }

        /* The mask is 0x3F so the original's signed byte compare against 5
         * is simply `< 5`; the clear writes the WHOLE dword. */
        if (p->f04 == idPlayer && (p->f2C & BR_PEER_STATE_MASK) < 5u) {
            p->f2C = 0;
        }

        /* The unlock re-reads hMutex from the record rather than reusing the
         * value it locked with (`mov edx,[esi-0x2c]` at 0x1006A426).
         * Preserved: a record whose handle changed inside the guarded block
         * would be unlocked through the NEW handle. */
        if (g_pBrPlatOs86 != NULL && g_pBrPlatOs86->pfnUnlockPeer != NULL) {
            g_pBrPlatOs86->pfnUnlockPeer(p->hMutex);
        }
    }
}

/* ==========================================================================
 * 6. 0x10072270 -- stop the streaming thread
 *    Glide 0x1006B1E0, 92 bytes
 * ========================================================================== */

int32_t g_fBrSndThread86;
void   *g_hBrSndWake86;
void   *g_hBrSndThread86;

void BrSub10072270(void)
{
    void *hWake;

    if (g_fBrSndThread86 == 0) {
        return;
    }

    if (g_pBrPlatOs86 != NULL && g_pBrPlatOs86->pfnSetEvent != NULL) {
        g_pBrPlatOs86->pfnSetEvent(g_hBrSndWake86);
    }
    if (g_pBrPlatOs86 != NULL && g_pBrPlatOs86->pfnWaitSingle != NULL) {
        g_pBrPlatOs86->pfnWaitSingle(g_hBrSndThread86);
    }
    if (g_pBrPlatOs86 != NULL && g_pBrPlatOs86->pfnCloseHandle != NULL) {
        g_pBrPlatOs86->pfnCloseHandle(g_hBrSndThread86);
    }

    /* ORDER PRESERVED: the wake handle is READ (0x1006B214) before the thread
     * handle is cleared (0x1006B219), and only closed afterwards. */
    hWake = g_hBrSndWake86;
    g_hBrSndThread86 = NULL;

    if (g_pBrPlatOs86 != NULL && g_pBrPlatOs86->pfnCloseHandle != NULL) {
        g_pBrPlatOs86->pfnCloseHandle(hWake);
    }
    g_hBrSndWake86  = NULL;
    g_fBrSndThread86 = 0;
}

/* ==========================================================================
 * 7. 0x100484E0 -- re-seat the page vtable
 * ========================================================================== */

void BrSub100484E0(BrUiPage *pThis)
{
    if (pThis == NULL) {
        return;
    }
    /* `mov dword ptr [ecx], 0x1008F6F8` -- the vtable pointer is the object's
     * first member in slice3_32.h's BrUiPage, so this is that store. */
    pThis->pVtbl = &BrUiPageVtbl_1008F6F8;
}

/* ==========================================================================
 * 8. 0x100751D0 / 0x1002C2C0 -- the frame timer, both ends
 *    Glide 0x1006E430 / 0x10019810; helpers 0x10075190 and 0x10075240
 * ========================================================================== */

/* 0x118AB138 (int64), 0x118AB140, 0x118AB144. Owned here: nothing else in
 * port/ mentions any of the four addresses. */
static int64_t g_br86PerfFreq;      /* 0x118AB138 / 0x118AB13C */
static int32_t g_br86Probed;        /* 0x118AB140 */
static int32_t g_br86HasPerf;       /* 0x118AB144 */

/* The timer object is a foreign 0x24-byte byte image (0x106806B0); every
 * field it uses is an integer, so it survives LP64 as bytes. memcpy access
 * only -- no alignment or aliasing assumption, the same technique
 * slice3_32.c's BrScrLd32 family uses. */
static int64_t br86_ld64(const void *p, size_t off)
{
    int64_t v;
    memcpy(&v, (const unsigned char *)p + off, sizeof v);
    return v;
}

static void br86_st64(void *p, size_t off, int64_t v)
{
    memcpy((unsigned char *)p + off, &v, sizeof v);
}

static int32_t br86_ld32(const void *p, size_t off)
{
    int32_t v;
    memcpy(&v, (const unsigned char *)p + off, sizeof v);
    return v;
}

static void br86_st32(void *p, size_t off, int32_t v)
{
    memcpy((unsigned char *)p + off, &v, sizeof v);
}

/* 0x10075190 -- (re)start the clock. Private to this pair: its only two
 * callers are 0x100751D0's two arms. */
/* WHAT IT DOES: starts the frame clock ticking from now, noting the current
 * time and when the next frame is due. It uses the machine's precise timer
 * where there is one and the ordinary Windows clock otherwise. */
/* @implements 0x10075190 d3d br86_timer_restart */
static void br86_timer_restart(void *pThis)
{
    if (g_br86HasPerf) {
        int64_t now = 0;

        (void)BrPlatQueryPerfCounter(&now);
        br86_st64(pThis, BR86_TMR_NOW, now);
        br86_st64(pThis, BR86_TMR_DUE, br86_ld64(pThis, BR86_TMR_PERIOD));
    } else {
        br86_st32(pThis, BR86_TMR_NOW_MS, (int32_t)BrPlatTimeGetTime());
        br86_st32(pThis, BR86_TMR_DUE_MS, br86_ld32(pThis, BR86_TMR_PERIOD_MS));
    }
}

void BrX100751D0(void *pThis)
{
    if (pThis == NULL) {
        return;
    }

    if (g_br86Probed == 0) {
        g_br86HasPerf = BrPlatQueryPerfFreq(&g_br86PerfFreq);
        g_br86Probed  = 1;
    }

    if (g_br86HasPerf) {
        /* `push 0 / push 0x1e / call __alldiv` -- a 64-bit divide by the
         * LITERAL 30, i.e. a 30 Hz tick. */
        br86_st64(pThis, BR86_TMR_PERIOD, g_br86PerfFreq / 30);
    } else {
        if (g_pBrPlatOs86 != NULL && g_pBrPlatOs86->pfnTimeBeginPeriod != NULL) {
            g_pBrPlatOs86->pfnTimeBeginPeriod(1);
        }
        br86_st32(pThis, BR86_TMR_PERIOD_MS, 0x21);   /* 33 ms */
    }

    br86_timer_restart(pThis);
    /* The original returns `this`; slice2_17.c's declaration is void and the
     * one call site discards it. */
}

/* 0x10075240 -- the teardown 0x1002C2C0 tail-calls into. */
/* WHAT IT DOES: gives back the finer timer resolution the game asked Windows
 * for, and only on machines that needed it -- where the precise timer was
 * available nothing was asked for and nothing is returned. */
/* @implements 0x1006E4A0 glide br86_timer_end_period */
#ifdef BR_MATCHING_BUILD
/* Glide 0x1006E4A0: 18 B -- MOV EAX,[g_br86HasPerf] / TEST / JNZ+8 /
 * PUSH 1 / CALL [IAT:timeEndPeriod] / RET.  No struct-pointer guard.
 * Direct dllimport call produces FF 15 [IAT] = one reloc at offset 13. */
__declspec(dllimport) void __stdcall timeEndPeriod(unsigned int uPeriod);
static void BR_THISCALL1 br86_timer_end_period(void *pThis)
{
    (void)pThis;
    if (g_br86HasPerf == 0)
        timeEndPeriod(1);
}
#else
static void BR_THISCALL1 br86_timer_end_period(void *pThis)
{
    /* `this` is passed but never read -- the body only looks at globals. It
     * is declared so that 0x1002C2C0's thunk can load it into ecx. */
    (void)pThis;

    if (g_br86HasPerf == 0) {
        if (g_pBrPlatOs86 != NULL && g_pBrPlatOs86->pfnTimeEndPeriod != NULL) {
            g_pBrPlatOs86->pfnTimeEndPeriod(1);
        }
    }
}
#endif

/* WHAT IT DOES: shuts the frame clock down on the way out of the game, by
 * handing back the timer resolution above. */
/* @implements 0x1002C2C0 d3d BrX1002C2C0 */
void BrX1002C2C0(void)
{
    /* `mov ecx, 0x106806B0 / jmp 0x10075240`. The operand is an IMMEDIATE --
     * the ADDRESS of the frame-timer object -- so the object is named here and
     * its address taken; nothing is pushed. The callee never reads ecx, but
     * the thunk still loads it, so the callee wears BR_THISCALL1. */
    br86_timer_end_period(g_br6806B0);
}

/* ==========================================================================
 * 9. The two vtable objects
 *
 *    Slots read out of BRD3D.dll .rdata. The five thunks put back the
 *    leading BrScrGlobals * that slice3_32.c added and the original does not
 *    have; a NULL binding makes each one a no-op returning 0, which is the
 *    retired stub's answer.
 * ========================================================================== */

static int32_t br86_page_f04(BrUiPage *pThis)          /* 0x10048530 */
{
    if (g_pBrScrGlobals86 == NULL) {
        return 0;
    }
    return (int32_t)BrUiPageFrame_10048530(g_pBrScrGlobals86, pThis);
}

static int32_t br86_phase_f08(BrPhaseFull *pThis)      /* 0x100488C0 */
{
    if (g_pBrScrGlobals86 == NULL) {
        return 0;
    }
    return (int32_t)BrPhaseTick_100488C0(g_pBrScrGlobals86, pThis);
}

static int32_t br86_phase_f0C(BrPhaseFull *pThis)      /* 0x100489A0 */
{
    if (g_pBrScrGlobals86 == NULL) {
        return 0;
    }
    return (int32_t)BrPhaseRun_100489A0(g_pBrScrGlobals86, pThis);
}

static void br86_phase_f18(BrPhaseFull *pThis, void *pArg)  /* 0x10048B20 */
{
    /* GOTCHA recorded by slice3_32.h: 0x10048B20 IGNORES `this` -- it is pure
     * global work -- which is why the thunk drops it rather than passing it
     * somewhere. */
    (void)pThis;
    if (g_pBrScrGlobals86 == NULL) {
        return;
    }
    BrPhaseShutdown_10048B20(g_pBrScrGlobals86, pArg);
}

static void br86_phase_f1C(BrPhaseFull *pThis)         /* 0x10048AA0 */
{
    if (g_pBrScrGlobals86 == NULL) {
        return;
    }
    BrPhaseReleasePages_10048AA0(g_pBrScrGlobals86, pThis);
}

const BrUiPageVtbl BrUiPageVtbl_1008F6F8 = {
    BrUiPageDelete_100484C0,        /* +0x00  0x100484C0 */
    br86_page_f04                   /* +0x04  0x10048530 */
};

const BrPhaseFullVtbl BrPhaseVtbl_1008F700 = {
    BrPhaseDelete_10048850,         /* +0x00  0x10048850 */
    BrPhaseFn_100488B0,             /* +0x04  0x100488B0 */
    br86_phase_f08,                 /* +0x08  0x100488C0 */
    br86_phase_f0C,                 /* +0x0C  0x100489A0 */
    NULL,                           /* +0x10  0x1005AE70 -- no body in port/ */
    NULL,                           /* +0x14  0x10048960 -- no body in port/ */
    br86_phase_f18,                 /* +0x18  0x10048B20 */
    br86_phase_f1C,                 /* +0x1C  0x10048AA0 */
    NULL                            /* +0x20  0x1005AFA0 -- no body in port/ */
};

/* ==========================================================================
 * 10. 0x10075150 -- 30 Hz tick stepper
 *    Glide 0x1006E3B0
 * ========================================================================== */

#ifdef BR_MATCHING_BUILD
static int32_t g_br18AB12C;                    /* 0x118AB12C */
static int32_t g_br0BBAC8[3] = { 33, 33, 34 }; /* 0x100BBAC8 */
static int32_t g_br18AB118;                    /* 0x118AB118 */
static int32_t g_br18AB134;                    /* 0x118AB134 */

/* WHAT IT DOES: walks the game's thirty-tick-a-second clock forward by one
 * tick. A three-step counter picks 33, 33 or 34 milliseconds in turn from a
 * small table, that amount is added to the millisecond clock, and the tick
 * count goes up by one -- three calls add exactly one tenth of a second. */
/* @implements 0x10075150 d3d BrSub10075150 */
void BrSub10075150(void)
{
    if (++g_br18AB12C > 2)              /* `cmp eax,2 / jle` -- signed */
        g_br18AB12C = 0;
    g_br18AB118 += g_br0BBAC8[g_br18AB12C];
    g_br18AB134++;
}
#endif
