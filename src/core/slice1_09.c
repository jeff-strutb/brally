/* slice1_09.c -- decompiled from BRD3D.dll, range 100734F0-10078CD0.
 * See slice1_09.h for the recovered layouts and the argument-order notes.
 *
 * Skipped functions and the reason for each are listed at the bottom of this
 * file so the information does not get lost.
 */
#ifdef BR_MATCHING_BUILD
/* slice1_09.h declares these cdecl; the originals are thiscall with stack
 * args.  Hide those prototypes so the matching bodies can use __fastcall
 * plus a struct-typed second argument (never register-eligible, so forced
 * onto the stack).  Same split as thiscall; do not redefine BR_THISCALL. */
#define BrBitStreamReadBits  BrBitStreamReadBits_cdecl
#define BrBitStreamInit      BrBitStreamInit_cdecl
#define BrBitStreamSkipBytes BrBitStreamSkipBytes_cdecl
#define BrBitStreamWriteU8   BrBitStreamWriteU8_cdecl
#define BrBitStreamWriteU24  BrBitStreamWriteU24_cdecl
#define BrBitStreamWriteU32  BrBitStreamWriteU32_cdecl
#define BrEntitySetIndex     BrEntitySetIndex_cdecl
#define BrEntityBindAux      BrEntityBindAux_cdecl
#endif
#include "slice1_09.h"
#ifdef BR_MATCHING_BUILD
#undef BrBitStreamReadBits
#undef BrBitStreamInit
#undef BrBitStreamSkipBytes
#undef BrBitStreamWriteU8
#undef BrBitStreamWriteU24
#undef BrBitStreamWriteU32
#undef BrEntitySetIndex
#undef BrEntityBindAux
#endif

#include <math.h>
#include <stddef.h>

/* The bit/byte stream (0x1006CDE0-0x1006D160, plus BrObjResetMsgHdr) moved
 * to src/core/gamedata/br_bitstream.c. */

/* ================================================================== */
/* Float math                                                          */
/* ================================================================== */

/* BrVec3Normalise / BrVec4Normalise live in br_vecnorm.c. */

/* 0x1006DA20 / 0x100747C0 BrMat4TransformPoint moved to
 * src/core/geometry/br_mat4.c. */

/* ================================================================== */
/* Entity array offsets                                                */
/* ================================================================== */

/* 0x10076AE0  __thiscall, ret 4. `cmp eax,0x10 / jl` -- signed. */
/* WHAT IT DOES: records which of two banks of sixteen an object belongs to.
 * Anything numbered sixteen or above is stored as the second bank with its
 * number reduced by sixteen; anything below it is the first bank. */
/* @implements 0x10076AE0 d3d BrEntitySetIndex */
#ifdef BR_MATCHING_BUILD
/* thiscall, one stack arg.  Size-exact (50) but encoding-walled:
 * original `sub eax, 0x10`, VC5 `add eax, -0x10`.  `i - 16`, `i -= 16`,
 * unsigned subtract, and inline-in-store all emit the add form under
 * every VC5 flag probed (/O1 /O2 /Os /Ox /G3 /G5, C and C++ front ends).
 * VC4.2 DOES emit `sub eax,0x10` -- but schedules it AFTER the bank
 * store for every probed source order, where the original subs first.
 * Neither compiler reproduces both; parked. */
typedef struct { int n; } BrEntityIndexArg;
/* RESIDUE 2 bytes, FIRSTDIV +0xa, and the whole of it is one instruction:
 * the original has `sub eax,0x10` where we emit `add eax,-0x10`.  That is NOT
 * a spelling choice -- MSVC5 canonicalises every straight-line constant
 * subtraction to add-negative.  Probed and DEAD, do not re-run: `i = i - 16`,
 * `i -= 0x10`, the subtraction inlined into the store, `i = index.n - 16`, a
 * const-propagated `base` local, an in-place bump on the parameter member,
 * and the compile variants /O2 /Op, /O2 /Oy-, /O1 and /Ox.  An isolated
 * one-line probe confirms the rule for int, long, unsigned, short and both
 * pointer spellings.  See the `sub reg, imm` entry in docs/VC5-IDIOMS.md: the
 * three MSVC5 constructs known to keep a real `sub` are a loop-carried
 * decrement, a 16-bit-typed subtraction whose result stays live narrow, and a
 * pointer difference feeding further arithmetic -- this function fits none of
 * them, so the answer is still open.  It is NOT the operator. */
void __fastcall BrEntitySetIndex(void *pEntity, BrEntityIndexArg index)
{
    int i = index.n;
    if (i >= 16) {
        i -= 16;
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_BANK)  = 1;
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_INDEX) = i;
    } else {
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_BANK)  = 0;
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_INDEX) = i;
    }
}
#else
void BrEntitySetIndex(void *pEntity, int index)
{
    unsigned char *p = (unsigned char *)pEntity;
    int *pIndex = (int *)(void *)(p + BR_ENTITY_OFF_INDEX);
    int *pBank  = (int *)(void *)(p + BR_ENTITY_OFF_BANK);

    if (index >= 16) {
        *pBank  = 1;
        *pIndex = index - 16;
    } else {
        *pBank  = 0;
        *pIndex = index;
    }
}
#endif

/* ================================================================== */
/* Misc                                                                */
/* ================================================================== */

/* 0x10073A10 (PARTIAL).
 *
 * The full original does three things: two calls through the import pointer
 * at 0x118AA0B0 with fourteen constant arguments (an unidentified 0x40x0x40
 * surface/texture creation), then this table build, then a call to
 * sub_100098A0(dst=0x11829118, src=0x11829330, size=0x40, format=2) whose
 * byte return is divided by 16. Only the table build is portable and
 * identifiable, so only it is ported; the rest is reported as skipped.
 *
 * The loop bound in the original is `cmp eax, 0x11829371 / jl` against a
 * cursor that starts at base+1 and steps 4, giving exactly 16 iterations
 * over a 0x40-byte table at 0x11829330. */
/* WHAT IT DOES: fills a sixteen-entry colour table: every entry pure white,
 * with the transparency stepping evenly from fully see-through to fully
 * solid. Only this table build is transcribed; the rest of the original
 * routine creates two textures through an unidentified backend call and is
 * not ported. */
/* 0x10073A10 CARRIES NO @implements LINE.  The banner above already called
 * this PARTIAL and it is: the original is four statements and this is one of
 * them, 37 of its 167 bytes.  The two surface creations write 0x11829100 and
 * 0x11829104 and the sub_100098A0 call writes 0x11829318 -- three globals a
 * caller of 0x10073A10 gets and a caller of this does not.  The manifest form
 * is whole-function only, so "@implements 0x10073A10" asserted all four, the
 * address counted as ported, and nobody was going to come back for the other
 * three.  It is better read as unported until the backend call at 0x118AA0B0
 * is identified.  The table build itself stands and is used; only the CLAIM
 * was wrong. */
void BrAlphaRampBuild(unsigned char *pOut)
{
    int i;
    for (i = 0; i < 16; ++i) {
        pOut[i * 4 + 0] = 0xFF;
        pOut[i * 4 + 1] = 0xFF;
        pOut[i * 4 + 2] = 0xFF;
        pOut[i * 4 + 3] = (unsigned char)((i << 4) | i);
    }
}

/* 0x10074F70.
 *
 * DEVIATION: the original brackets the whole body with
 * WaitForSingleObject(g_18AA0A0, INFINITE) / ReleaseMutex(g_18AA0A0). The
 * mutex is dropped here -- callers must serialise. The ring itself is
 * otherwise verbatim, including the fact that the write index is stored
 * back before the bounds test and then overwritten with 0 when it reached
 * 0x100. There is no read cursor and no fullness check anywhere in the
 * original: entry 0 is simply overwritten on the 257th push. */
void BrPairRingPush(BrPairRing *pRing, int a, int b)
{
    int i = pRing->write;

    pRing->aItems[i].a = a;
    pRing->aItems[i].b = b;

    i++;
    pRing->write = i;
    if (i >= BR_PAIR_RING_SLOTS)
        pRing->write = 0;
}

/* 0x10075100.
 *
 * The original calls the platform timer at 0x10075020 for `ms` and then
 * does the arithmetic below with `div esi` (esi = 100) and two unsigned
 * magic multiplies: 0x51EB851F >> 37 is ms/100 and 0x3E0F83E1 >> 35 is
 * (ms % 100) / 33.
 *
 * DEVIATION: `ms` is a parameter instead of a call into
 * QueryPerformanceCounter / timeGetTime (0x10075020, skipped -- see below).
 * Everything else is verbatim, including the order of the three stores. */
/* WHAT IT DOES: converts a time in milliseconds into the game's own clock,
 * which counts thirty ticks a second -- the N64's frame rate, kept in the PC
 * build. It also clears one other field of the timing record. */
/* port-only body; the Glide twin is src/core/generated/0x1006E360.c -- the
 * original takes NO arguments: it calls 0x1006E280 for the millisecond count
 * and writes three absolute globals, so neither parameter here exists. */
void BrTimeUpdate(BrTimeState *pState, unsigned int ms)
{
    pState->f12C   = 0;
    pState->ms     = ms;
    pState->tick30 = (ms % 100u) / 33u + 3u * (ms / 100u);
}

/* ==================================================================
 * SKIPPED, with reasons
 * ==================================================================
 *
 * Already implemented elsewhere (not re-done):
 *   0x10073B40 0x10073B80 0x10073D20 0x10073F40 0x10073F50  br_obj.h
 *   0x10074030  BrHandleLookup (br_bits.h)
 *   0x10074720 0x10074770                                   br_mat.h
 *
 * Windows / COM / platform-only, nothing portable inside:
 *   0x100734F0  tears down the g_1828F48 object and clears a 0x48-stride
 *               table plus a 60-byte block; pure global bookkeeping around
 *               two calls into an unrecovered class.
 *   0x10073560  DirectSound-family init: GlobalAlloc/GlobalLock a 0x12-byte
 *               descriptor, fill it (0x5622, 0x15888, 4, 0x10, 0),
 *               CoCreateInstance, then five vtable calls with the usual
 *               release-on-failure ladder. Nothing to port.
 *   0x10073950  one call through the import at 0x118AA0B0 with 14 constant
 *               arguments (0x40 x 0x40, format 4). Callee unidentified.
 *   0x100739E0  the same call with a different source and all-zero flags.
 *   0x100770F0  COM/DirectSound init behind a +1 refcount guard; three
 *               vtable calls, each with its own bail-out.
 *   0x10078CD0  SEH frame, MessageBoxA on failure, two more vtable calls.
 *   0x10075020  QueryPerformanceFrequency / QueryPerformanceCounter with a
 *               64-bit multiply/divide pair (0x1007ED20, 0x1007FD10) and a
 *               timeGetTime fallback. The portable core is
 *               ms = (ticks * 1000 + 500) / freq  -- note the +500, i.e.
 *               round-to-nearest, not truncation -- minus a base captured on
 *               the first call. Not ported because it is entirely a wrapper
 *               over two Win32 clocks.
 *   0x10076CE0 0x10076E90 0x10076ED0 0x10076FA0  RIFF/WAVE loading built
 *               entirely on WINMM's mmio* API. Error codes, for whoever
 *               reimplements them: 0xE000 out of memory, 0xE100 open
 *               failed, 0xE101 malformed/short RIFF, 0xE102 short read of
 *               the fmt chunk, 0xE103 mmio buffer exhausted mid-copy.
 *               0x10076CE0 additionally hardcodes the assumption that a
 *               PCM (wFormatTag == 1) header has no cbSize field and reads
 *               only 16 bytes for it.
 *
 * Layout not established well enough to port:
 *   0x100770C0  zeroes 14 dwords at 0x118ABD38, zeroes 0x118ABAD4 and sets
 *               0x118ABD80 to 1. The three globals are 0x2A4 apart and
 *               0x48 apart respectively with nothing to tie them into one
 *               structure, so any struct here would be invented.
 */

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
typedef int (*funcptr)();
#include <windows.h>
extern int DAT_100b84a8;
extern int DAT_104af5c8;
extern int DAT_104b05c8;
extern int DAT_1184c480;
extern int _DAT_1184c460;
extern int _DAT_1184c464;
extern int g_br18AB118_S_S1499;
extern funcptr g_pfn18AA0B0;



/* 0x1006E350 BrGetTimerState now lives in src/core/startup/br_timer.c. */

#endif /* BR_MATCHING_BUILD */
