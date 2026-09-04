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

/* 0x100747C0.
 * Written out longhand rather than with temporaries so that the write order
 * matches the original exactly: each output component is zeroed and fully
 * accumulated before the next one begins, and the translation row is added
 * to all three only afterwards. That ordering is observable when pOut
 * aliases pV. */
/* WHAT IT DOES: moves a point through a transform: rotates and scales it by
 * the matrix and then adds the matrix's translation. The awkward longhand
 * here is deliberate, because the original writes each result component out
 * before starting the next, which is visible if the caller passes the same
 * point as both input and output. */
/* @implements 0x1006DA20 glide BrMat4TransformPoint */
/* @implements 0x100747C0 d3d BrMat4TransformPoint */
void BrMat4TransformPoint(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV)
{
    /* Orig is two counted loops (ebp=3 outer, esi=3 inner), not unrolled
     * products: `mov [eax],0`; inner `fld [v]; fmul [m]; add m,0x10; add v,4;
     * dec esi; fadd [eax]; fstp [eax]`.  `sub edi,eax` is pM-pOut so the
     * column pointer is `lea r,[edi+eax]` as eax walks the output.
     *
     * INDEXED, NOT CURSORS.  Hand-rolled walking pointers (`col = m; v = pV;`
     * bumped by `col += 4; v++`) reproduce this exactly to 7 bytes and then
     * stop: VC5 binds the copy-from-register cursor to ecx and the lea-derived
     * one to edx, where the original has them the other way round, and the lea
     * comes out `[eax+edi]` instead of `[edi+eax]`.  Swapping the assignment
     * order, swapping the declaration order and block-scoping the pair inside
     * the outer loop all fail (9 / 7 / 5 diffs) -- a previous note here called
     * this a register-allocation wall and told the reader not to grind it, and
     * that was WRONG.  Letting the compiler build both induction variables
     * itself, from plain `pv[j]` and `pM->m[j][i]` subscripts, is byte-exact:
     * the two cursors then come into existence in the order VC5 wants them
     * and pick up ecx/edx accordingly.  Semantics are unchanged -- `pv[j]`
     * re-reads the live vector every outer pass, exactly as the reloaded
     * cursor did, which is what keeps the aliasing case above honest. */
    float       *o  = (float *)pOut;
    const float *pv = (const float *)pV;
    int i, j;

    for (i = 0; i < 3; i++) {
        o[i] = 0.0f;
        for (j = 0; j < 3; j++)
            o[i] += pv[j] * pM->m[j][i];
    }
    pOut->x += pM->m[3][0];
    pOut->y += pM->m[3][1];
    pOut->z += pM->m[3][2];
}

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

#ifdef BR_MATCHING_BUILD
/* 0x100739B0
 *
 * Fourteen constant arguments through the backend texture constructor at
 * 0x118AA0B0 -- the same cdecl as 0x10073980, last-arg-first: 0x40 x 0x40,
 * fmt 0, siz 4, source 0x100B94A8, result stored at 0x11829314. */
/* WHAT IT DOES: turns a baked-in 64-by-64 picture into a texture the rest of
 * the game can draw with, and remembers the handle the graphics backend
 * returns. */
/* @implements 0x100739B0 d3d BrSub100739B0 */
typedef void *(*BrSub100739B0Fn)(void *pSrc, int a2, int w, int h,
                                 int fmt, int siz, int b31, int b30,
                                 int b29, int b28, int a11, int a12,
                                 int a13, int a14);

extern BrSub100739B0Fn g_18AA0B0;     /* 0x118AA0B0 */
extern void           *g_1829314;     /* 0x11829314 */
extern unsigned char   g_0B94A8[];    /* 0x100B94A8 */

void BrSub100739B0(void)
{
    g_1829314 = g_18AA0B0(g_0B94A8, 0, 0x40, 0x40, 0, 4,
                          0, 0, 0, 0, 0, 0, 1, 0);
}
#endif

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

extern int DAT_117b3250;
extern int DAT_11849e60;
extern int DAT_1184c070;
extern int DAT_1184c074;
extern int g_aBr178FEF8;
extern int g_aBrPeer71;

/* WHAT IT DOES: the networking worker thread's wait loop: blocks until
 * either the quit event or a peer's mutex is signalled, exits the thread on
 * quit, and otherwise checks each peer's state and bails out of the scan as
 * soon as one is not ready. */
/* @implements 0x1006A650 glide FUN_1006a650 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_1006a650(void)
{
  DWORD wr;
  int *pPeer;
  int *pAlt;
  HANDLE h1[2];
  HANDLE h2[2];
  char skip;
  int st;
  int t;

  pPeer = &g_aBrPeer71;
  do {
    h1[0] = (HANDLE)DAT_11849e60;
    h1[1] = (HANDLE)*pPeer;
    wr = WaitForMultipleObjects(2, h1, 0, 0xffffffff);
    if (wr == 0) {
      ExitThread(0);
    }
    st = pPeer[0xb] & 0x3f;
    if (st < 2 || st == 3) {
      skip = 0;
    }
    else {
      skip = 1;
    }
    ReleaseMutex((HANDLE)*pPeer);
    if (skip) {
      return;
    }
    pPeer = pPeer + 0x25b;
  } while ((int)pPeer < 0x117b3248);

  pAlt = &g_aBr178FEF8;
  pPeer = &g_aBrPeer71;
  for (;;) {
    h1[0] = (HANDLE)DAT_11849e60;
    h1[1] = (HANDLE)*pPeer;
    wr = WaitForMultipleObjects(2, h1, 0, 0xffffffff);
    if (wr == 0) {
      ExitThread(0);
    }
    st = pPeer[0xb];
    skip = ((st & 0x3f) == 3);
    ReleaseMutex((HANDLE)*pPeer);
    if (skip) {
      h2[0] = (HANDLE)DAT_11849e60;
      h2[1] = (HANDLE)*pAlt;
      wr = WaitForMultipleObjects(2, h2, 0, 0xffffffff);
      if (wr == 0) {
        ExitThread(0);
      }
      st = pAlt[0xb];
      skip = ((st & 0x3f) != 3);
      ReleaseMutex((HANDLE)*pAlt);
      if (skip) {
        return;
      }
    }
    pPeer = pPeer + 0x25b;
    pAlt = pAlt + 0x280b;
    if ((int)pPeer >= 0x117b3248) {
      pPeer = &g_aBrPeer71;
      t = 4;
      do {
        h2[0] = (HANDLE)DAT_11849e60;
        h2[1] = (HANDLE)*pPeer;
        wr = WaitForMultipleObjects(2, h2, 0, 0xffffffff);
        if (wr == 0) {
          ExitThread(0);
        }
        if ((pPeer[0xb] & 0x3f) == 3) {
          pPeer[0xb] = t;
          DAT_117b3250 = 1;
          DAT_1184c074 = DAT_1184c070 + 3000;
        }
        ReleaseMutex((HANDLE)*pPeer);
        pPeer = pPeer + 0x25b;
      } while ((int)pPeer < 0x117b3248);
      return;
    }
  }
}

#endif /* BR_MATCHING_BUILD */
