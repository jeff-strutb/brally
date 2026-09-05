/* br_frametimer.c -- startup: the game's frame timer, both ends.
 *
 * Filed out of the address batch slice8_86.c (its section 8), whose preamble
 * this file carries verbatim.  RESPONSIBILITY: startup -- this is the clock
 * the frame loop is paced by, set up on the way in and handed back on the
 * way out.
 *
 * WHY THE WHOLE SECTION MOVED AT ONCE.  g_br86HasPerf is a CACHED FLAG --
 * real state, written once by BrX100751D0 behind the g_br86Probed guard and
 * only read afterwards.  A second translation unit's copy would never be
 * written, so br86_timer_end_period would read 0 on every machine and hand
 * back a timer resolution that was never asked for.  The sweep cannot see
 * that: the matching build emits identical bytes either way.  So the flag is
 * not duplicated -- its single writer, its readers and the three statics
 * travel together, and nothing left in slice8_86.c refers to any of them.
 *
 * NOT br_timer.c, which would otherwise be the obvious home: that file is an
 * aggregation of four batches and its header set collides with this one.
 * Adding this section to it fails to compile outright -- slice8_86.h's chain
 * redefines slice1_06.h's BrDPlayVtbl against what slice4_53.h already
 * brought in, and its windows.h declares timeEndPeriod with a different
 * return type than the dllimport below.  A separate file is what keeps each
 * body's view of the world the one its batch gave it.
 *
 * REFERENCE: orig/BRGlide.dll, cross-checked against orig/BRD3D.dll. Every
 * address named in a banner is the D3D one, because that is the numbering the
 * rest of port/ uses; the Glide address actually read is given beside it.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdlib.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD
/* slice8_86.h declares BrX100751D0 cdecl for the port; the Glide twin below
 * is thiscall (the caller 0x10019810 loads ecx), so the header's name is
 * diverted for this build only. */
#define BrX100751D0 BrX100751D0_port
#endif
#include "slice8_86.h"
#ifdef BR_MATCHING_BUILD
#undef BrX100751D0
#endif
#include "slice7_82.h"   /* BrGlobalHandle/Unlock/Free, BrPlat* clocks */
#include "br_match.h"    /* BR_THISCALL1 */

/* The frame-timer object itself, defined in slice2_17.c. 0x1002C2C0 plants
 * its ADDRESS in ecx as an immediate, so the object has to be named here --
 * routing through a pointer variable would cost a `mov eax, [mem]`. */
extern unsigned char g_br6806B0[0x24];

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
/* port-only body; Glide match is src/core/generated/0x1006E3F0.c */
#ifdef BR_MATCHING_BUILD
/* 0x1006E3F0 is matched in src/core/generated/0x1006E3F0.c; BrX100751D0's
 * `mov ecx,esi / call` needs that symbol, not a local copy. */
extern void BR_THISCALL1 br86_timer_restart(void *pThis);
#else
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
#endif

/* WHAT IT DOES: set the frame clock up for a 30 Hz tick. The first call
 * asks Windows whether a high-resolution counter exists and remembers the
 * answer; with one, the period is counter-frequency / 30 ticks, without one
 * it asks for 1 ms timer resolution and uses a 33 ms period. Either way the
 * clock is then started, and the timer object is handed back. */
/* @implements 0x1006E430 glide BrX100751D0 */
#ifdef BR_MATCHING_BUILD
__declspec(dllimport) int __stdcall QueryPerformanceFrequency(__int64 *pFreq);
__declspec(dllimport) unsigned int __stdcall timeBeginPeriod(unsigned int uPeriod);
void * BR_THISCALL1 BrX100751D0(void *pThis)
{
    if (g_br86Probed == 0) {
        g_br86HasPerf = QueryPerformanceFrequency(&g_br86PerfFreq);
        g_br86Probed  = 1;
    }
    if (g_br86HasPerf) {
        /* `push 0 / push 0x1e / call __alldiv` -- a 64-bit divide by the
         * LITERAL 30, i.e. a 30 Hz tick. */
        *(__int64 *)pThis = g_br86PerfFreq / 30;
    } else {
        timeBeginPeriod(1);
        *(int *)((char *)pThis + BR86_TMR_PERIOD_MS) = 0x21;   /* 33 ms */
    }
    br86_timer_restart(pThis);
    return pThis;
}
#else
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
#endif

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
/* @implements 0x10019830 glide BrX1002C2C0 */
void BrX1002C2C0(void)
{
    /* `mov ecx, 0x106806B0 / jmp 0x10075240`. The operand is an IMMEDIATE --
     * the ADDRESS of the frame-timer object -- so the object is named here and
     * its address taken; nothing is pushed. The callee never reads ecx, but
     * the thunk still loads it, so the callee wears BR_THISCALL1. */
    br86_timer_end_period(g_br6806B0);
}
