/* slice6_74.h -- BRD3D.dll, packet 74 (slice 6).
 *
 * HOW THESE TARGETS WERE CHOSEN
 * =============================
 * By measurement, and the measurement produced a negative result worth
 * recording: at the time of writing NO stub is reached by any boot path.
 * `./build/brally` reports "stubs: none reached", and `-all` -- which runs each
 * of the sixteen ported screen builders in a forked child -- reaches a stub in
 * none of the sixteen either.  The eight builders that crash there crash on
 * NULL harness HOOKS inside Br72ScreenNew / slice6_71, not on stubs, so the
 * hit-count list the harness exists to produce is empty.
 *
 * With no runtime demand to rank by, the ranking used instead is STATIC
 * demand: how many call sites in the already-ported tree name each stub.  That
 * is the same list one frame earlier -- these are the functions that go hot the
 * moment their callers run.  The counts quoted per function below are those
 * call-site counts, and for 0x10073E70 the count was confirmed against the
 * image itself (51 call/jmp sites in .text).
 *
 * The second selection rule was CONVENTIONS' "do not re-decompile a function
 * that already exists under another name".  Cross-referencing every stub
 * address against the ported tree found six stubs whose bodies were already
 * present under a different name; those become thin adapters here and are
 * marked ADAPTER, with the owning module named.  Writing a second
 * transcription of any of them would have been the expensive mistake.
 *
 * SCOPE
 * =====
 *   0x10073E70  BrBitStreamWriteBits   transcribed here (the only substantial
 *                                      body in this packet)
 *   0x10074DC0  BrSub10074DC0          transcribed here
 *   0x10008B80  the six 8B80 names     a bare `ret` in this build; real, empty
 *   0x10073C90  BrBitReaderRead        ADAPTER -> slice1_09 BrBitStreamReadBits
 *   0x1003B170  BrVec3Len              ADAPTER -> br_vec   BrVec3Length
 *   0x1002B920  BrFtolArg              ADAPTER -> br_crt   BrFtolTrunc
 *   0x10072AF0  BrExt_10072AF0         ADAPTER -> slice1_08 BrSndPlaySimple
 *   0x1003CDA0  BrSub1003CDA0          ADAPTER -> slice6_72 BrExt_1003CDA0
 *   0x100695D0  BrSub100695D0          ADAPTER -> slice3_42 BrMat4FromCarState
 *   --          BrNetMutexLock/Unlock  the documented single-threaded hooks
 *
 * Every name above already has a declaration in the module that calls it; this
 * header does not re-declare them, so that the one declaration each caller
 * compiles against stays the single source of truth.  What this header
 * exports is the storage this packet owns plus the two names that had no
 * declaration anywhere.
 *
 * SIGNATURE CONFLICTS FOUND (reported, not resolved here)
 * ======================================================
 *   0x10072AF0  slice3_31.h declares `void BrExt_10072AF0(int32_t, uint32_t)`,
 *               but the address is slice1_08's BrSndPlaySimple, which returns
 *               int32_t.  The adapter honours slice3_31.h's void and DISCARDS
 *               the result.  Both callers of BrExt_10072AF0 ignore it anyway,
 *               so nothing observable changes -- but the declaration is wrong
 *               about the original and should be corrected at the source.
 *
 *   0x1002BD50  slice2_19.h declares `BrModelVtxResolve(uint32_t *, int)` --
 *               two arguments -- and slice1_05.h declares the SAME address as
 *               `BrVtxCacheResolve(BrVtxCache *, void **, int)` -- three.
 *               Disassembly settles it in slice2_19.h's favour: 0x1002BD50 is
 *               cdecl with args at [esp+8] and [esp+0x14] after three pushes,
 *               i.e. TWO arguments, and its cache lives in the globals
 *               0x1067B54C / 0x1067554C rather than behind a `this`.
 *               slice1_05.h's leading BrVtxCache * is a modelling choice that
 *               lifted those globals into an object.  NOT adapted here: an
 *               adapter would have to invent which BrVtxCache instance is the
 *               one at 0x1067554C, which is the aliased-storage bug
 *               CONVENTIONS warns about.  See the report.
 *
 *   __ftol      br_crt.h's comment says out-of-range input "yields the x87
 *               'indefinite' value 0x80000000".  That contradicts its own
 *               preceding sentence ("stores the LOW DWORD of a 64-bit fistp"
 *               -- the 64-bit indefinite is 0x8000000000000000, whose low
 *               dword is 0), contradicts CONVENTIONS, and contradicts
 *               br_crt.c, which correctly returns 0.  The CODE is right and
 *               the header COMMENT is wrong.  Nothing here depends on it;
 *               reported so it gets fixed at the source.
 */
#ifndef SLICE6_74_H
#define SLICE6_74_H

#include <stdint.h>

/* ==========================================================================
 * Storage this packet owns
 * ========================================================================== */

/* 0x118AA088.  Written by 0x10074DC0 and by nothing else in the image; read
 * from three sites (0x100259B7, 0x10027EB5, 0x1002819D), all of them inside
 * the still-unported CD/MCI region.  So from the port's point of view it is
 * write-only today, which is what makes it safe to own here.
 *
 * OWNERSHIP NOTE (CONVENTIONS, "aliased storage"): the original's neighbouring
 * dword 0x118AA084 is already modelled, as `g_pfn18AA084` in the provisional
 * block in port/host/br_stubs.c.  These two are adjacent in the original's
 * .bss.  If a later pass models that region as one struct, THIS symbol must be
 * aliased into it rather than left as a second view of the same dword. */
extern int32_t g_br18AA088;

/* ==========================================================================
 * Functions with no prior declaration anywhere
 * ========================================================================== */

/* 0x10073B90  reset the READ cursor only: readBit = 0, readByte = 0.
 *
 * The whole body is two stores and a ret.  Contrast 0x10073B80
 * (br_obj.h's BrObjClear), which zeroes +0x00..+0x0C and so resets BOTH
 * cursors: these two are one instruction pair apart in the image and are not
 * interchangeable.  One caller in the image.
 *
 * Declared as void * rather than as slice1_09.h's BrBitStream so that this
 * header stays includable next to headers that model the same object
 * differently; the .c casts.  (0x10073D80, the write-u16 twin that completes
 * slice1_09.h's accessor family, is deliberately absent -- see the .c.) */
void BrBitStreamResetRead(void *pBs);

#endif /* SLICE6_74_H */
