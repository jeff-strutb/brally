/* slice7_82.h -- packet 82: the stub sweep after the function maps were
 * rebuilt by flow analysis.
 *
 * ===========================================================================
 * WHAT THIS MODULE IS
 * ===========================================================================
 * Sixteen functions that port/host/br_stubs.c was standing in for, chosen by
 * static call-site demand (call sites in ported code, plus a sweep of .text
 * for call targets decoded ONLY inside flow-derived function extents, so no
 * 0xE8 operand byte is ever read as a call).
 *
 * They fall into three groups, and the group matters because the reason each
 * one was still a stub is different:
 *
 *   1. TEN PLATFORM / CRT LEAVES.  Not decompilation targets at all: they are
 *      KERNEL32 / WINMM / ADVAPI32 imports and two statically linked CRT
 *      routines (0x1008C000 `_itoa`, 0x1007E8B0 `atexit`).  CONVENTIONS.md
 *      forbids porting anything at or above 0x1007CC40, and every previous
 *      packet correctly declined them on that ground -- but declining to
 *      TRANSCRIBE them is not the same as declining to SUPPLY them, and
 *      leaving them stubbed left real defects in ported code (see below).
 *
 *   2. THREE TRANSCRIBED BODIES, read out of orig/BRGlide.dll and re-checked
 *      against orig/BRD3D.dll instruction for instruction.
 *
 *   3. THREE ADAPTERS onto bodies that ALREADY EXIST under another name.
 *      CONVENTIONS.md's "grep the ADDRESS, not the symbol" rule; a sweep of
 *      all 98 stub addresses against every banner in port/src found 15 more
 *      such collisions, listed at the foot of this header.
 *
 * ===========================================================================
 * WHY THE STUBBED PLATFORM LEAVES WERE NOT HARMLESS
 * ===========================================================================
 * br_stubs.c returns integer 0 from every stub.  For a function whose result
 * is ignored that is free.  For these ten it was not:
 *
 *   BrItoa         returned NULL *and never wrote the caller's buffer*.  Both
 *                  call sites (slice2_25.c:200, :514) then pass that
 *                  uninitialised stack buffer on as a string.  This is the
 *                  worst of the ten: it is a read of indeterminate memory,
 *                  not merely a wrong value.
 *   BrGlobalFree   returned 0 without releasing anything, so every
 *                  DirectPlay session descriptor the port asked for LEAKED --
 *                  five call sites, all of them on paths the menu re-enters.
 *   BrGlobalHandle returned NULL, so the `BrGlobalFree(BrGlobalHandle(p))`
 *                  idiom that slice2_25.c and slice5_61.c both use was
 *                  freeing NULL rather than the block.
 *   BrPlatTimeGetTime / QueryPerfCounter / QueryPerfFreq
 *                  all returned 0, so slice4_50's millisecond clock
 *                  (0x10075020) returned a constant.  A frozen clock reads at
 *                  runtime as "the port is hung", not as "one leaf is
 *                  missing", which is why this was worth doing early.
 *   BrXAtExit      returned 0 == success without registering anything.
 *
 * NOTE THE FLOAT RULE AND WHY IT DOES NOT BITE HERE.  A stub returns 0 in the
 * integer register and leaves xmm0 alone, so a FLOAT-returning stub hands its
 * caller garbage rather than zero.  None of the sixteen below returns float.
 * The one float-returning stub still outstanding is 0x1006F310
 * (BrProbe1006F310, 7 call sites) -- see the report at the foot of this file.
 *
 * ===========================================================================
 * THE HOOKS, AND WHY THEY ARE HOOKS
 * ===========================================================================
 * Three of these functions reach something this tree has no portable answer
 * for: an OLE class factory, and the nine "is anything active" globals whose
 * only instance is `static` inside port/host/brally.c.  Each is reached
 * through a function/object pointer that defaults to a safe value, following
 * the precedent slice6_70.h set with g_pfnBrPlatKillTimer.  A NULL hook is
 * never dereferenced; the fallback is documented at each declaration.
 */
#ifndef SLICE7_82_H
#define SLICE7_82_H

#include <stdint.h>
#include <stddef.h>

struct BrDPlay;

/* =========================================================================
 * 1. CRT leaves
 * ========================================================================= */

/* 0x1008C000 -- the CRT's `_itoa`.  22 call sites in .text, 2 in port/src.
 *
 * MSVC semantics, which are NOT the same as a printf: for radix 10 a
 * negative `value` is emitted with a leading '-'; for every other radix the
 * value is formatted UNSIGNED, so _itoa(-1, b, 16) is "ffffffff" and not
 * "-1".  Returns pszBuf.
 *
 * The buffer is assumed large enough by the original and by both call sites
 * (33 bytes covers every radix >= 2 for a 32-bit value, plus sign and NUL). */
char *BrItoa(int value, char *pszBuf, int radix);

/* 0x1007E8B0 -- the CRT's `atexit` (0x1007E820 wrapped, returning 0 or -1).
 * 13 call sites in .text.  Forwarded to the host CRT's atexit, which has the
 * same contract: 0 on success, non-zero on failure. */
int BrXAtExit(void (*pfn)(void));

/* =========================================================================
 * 2. Platform leaves
 * ========================================================================= */

/* KERNEL32 GlobalHandle / GlobalUnlock / GlobalFree, used verbatim by
 * 0x10043810 and 0x10043A00 to dispose of the session descriptor DirectPlay
 * hands back.  Signatures are slice2_25.h:548-550's, unchanged.
 *
 * GMEM_FIXED semantics are modelled exactly, because that is what DirectPlay
 * allocates with and what the two-step `Unlock(Handle(p)); Free(Handle(p))`
 * idiom in the original depends on:
 *
 *   - the handle of a fixed block IS its pointer, so BrGlobalHandle is the
 *     identity (and NULL for NULL, as the original returns NULL for an
 *     address it cannot resolve);
 *   - a fixed block has a lock count of zero, so GlobalUnlock returns FALSE.
 *     BrGlobalUnlock therefore returns 0 ALWAYS.  That is the original's
 *     behaviour, not a stub: both call sites ignore the result;
 *   - GlobalFree returns NULL on success and the handle on failure. */
void *BrGlobalHandle(void *pMem);
int   BrGlobalUnlock(void *hMem);
void *BrGlobalFree(void *hMem);

/* How BrGlobalFree actually releases the block.  Defaults to the host's
 * free().
 *
 * WHY THIS IS A HOOK AND NOT A BARE free().  The blocks are allocated by
 * DirectPlay, which in this port is behind slice5_61/slice6_70's function
 * pointers.  Whoever supplies those pointers also supplies the allocator, and
 * handing free() a pointer it did not produce is undefined behaviour, not a
 * leak.  free() is the right default because the only stand-ins in the tree
 * today are malloc-backed; a real DirectPlay shim must re-point this. */
extern void (*g_pfnBrGlobalRelease)(void *pMem);

/* KERNEL32 QueryPerformanceFrequency / QueryPerformanceCounter and WINMM
 * timeGetTime.  Signatures are slice4_50.h:385-387's, unchanged.
 *
 * The counter here is a monotonic MICROSECOND clock, so the frequency is
 * 1000000.  Microseconds rather than nanoseconds is deliberate: slice4_50's
 * 0x10075020 computes `(counter * 1000 + 500) / freq` in 64-bit signed
 * arithmetic, and a nanosecond counter overflows that multiply after about
 * 107 days of uptime.  At 1 MHz it cannot overflow within any plausible
 * uptime, and the resolution is still 1000x finer than the millisecond the
 * caller wants.
 *
 * BrPlatQueryPerfFreq returns non-zero (a high-resolution counter exists),
 * which is the branch 0x10075020 latches ONCE at calibration -- see the
 * GOTCHA in slice4_50.h.  Both write through their out-parameter only on
 * success, as the originals do. */
int32_t  BrPlatQueryPerfFreq(int64_t *pFreq);
int32_t  BrPlatQueryPerfCounter(int64_t *pCount);
uint32_t BrPlatTimeGetTime(void);

/* KERNEL32!Sleep, imported at 0x118AE548 (slice3_32.h:547).  The single call
 * site passes 0, which on Windows means "yield the rest of this timeslice";
 * that case is mapped to a yield rather than to a zero-length sleep. */
void BrScrSleep(uint32_t ms);

/* ADVAPI32 GetUserNameA (slice4_50.h:325).  Non-zero on success, and *pcb is
 * updated to the length INCLUDING the terminating NUL -- that is the Win32
 * contract, not the strlen the name suggests.  On entry *pcb is the capacity.
 * The original ignores the result either way. */
int32_t BrPlatGetUserName(char *pszBuf, uint32_t *pcb);

/* =========================================================================
 * 3. Transcribed bodies
 * ========================================================================= */

/* 0x1002BF40, 60 bytes.  Non-zero if pv is NULL or is already in the
 * registered display-list table at 0x1067B548/0x1067B550.  Callers use
 * `== 0` to mean "not seen yet" (slice2_20.c:293, :318).
 *
 * GOTCHA -- A NULL LIST IS NOT AN EMPTY LIST FOR THE NULL ARGUMENT.  The
 * pv == NULL test comes FIRST and returns 1 before the table is looked at,
 * so BrDlIsRegistered(NULL) is 1 even with no table at all.  The stub
 * returned 0 for that case, which is the opposite answer.
 *
 * DEVIATION: the original's table is a fixed global; here it is reached
 * through slice5_60's `BrPtrList *g_pBrDlPtrList`, which is NULL until
 * something owns the storage.  A NULL list is treated as an EMPTY list --
 * the same answer the original gives when its count is <= 0 -- rather than
 * being dereferenced.  This is slice5_60.h's own DEVIATION, applied to the
 * reader as it was to the writer. */
int BrDlIsRegistered(const void *pv);

/* 0x10058700, 72 bytes.  Toggle the second field of the slot whose id
 * matches the UI object's +0x08, and return its new value.
 *
 * Returns 0 when there is no UI object or no matching slot -- and 0 is also
 * a legitimate "toggled to off" result, which the original does not
 * distinguish either.
 *
 * GOTCHA: the eight-record bound is arithmetic in the original (`cmp eax,
 * 0x10AA2598` after `add eax, 12` from 0x10AA2538), which is br_slots.h's
 * BR_SLOT_COUNT == 8 exactly.  The slot's FIRST field is the id and -1 means
 * free, so a UI object whose +0x08 is -1 matches a FREE slot.  Preserved. */
int BrSub10058700(void);

/* 0x1003C520, 46 bytes.  CoCreateInstance(CLSID_DirectPlay, NULL,
 * CLSCTX_INPROC_SERVER, IID_IDirectPlay4A, &out), then publish `out`.
 *
 * The two GUIDs were read out of the binary rather than assumed:
 *   0x10090880  D1EB6D20-8923-11D0-9D97-00A0C90A43CB  CLSID_DirectPlay
 *   0x10090850  133EFE41-32DC-11D0-9CFB-00A0C90A43CB  IID_IDirectPlay4A
 *
 * SIGNATURE NOTE -- THE RETURN VALUE IS REAL BUT INCIDENTAL.  The function
 * has no explicit return: eax simply still holds CoCreateInstance's HRESULT
 * when it rets, because the three instructions in between touch only ecx and
 * edx.  slice6_70.h:359 already declares it `int32_t (struct BrDPlay **)`
 * and slice6_70.c:118 relies on the HRESULT, so that reading is confirmed by
 * two independent sightings and is preserved here.
 *
 * GOTCHA: the out-parameter is written on EVERY path, including failure --
 * the local was zeroed before the call and is stored back afterwards
 * unconditionally.  slice6_70.c:126 depends on exactly that (it tests the
 * published pointer for NULL after a >= 0 HRESULT). */
int32_t BrSub1003C520(struct BrDPlay **ppDPlay);

/* The class factory BrSub1003C520 goes through.  Writes the new interface
 * pointer through its argument and returns an HRESULT.  NULL (the default)
 * makes BrSub1003C520 report BR82_HR_CLASSNOTREG and publish NULL, which is
 * the same shape of failure the original reports when DirectPlay is absent. */
extern int32_t (*g_pfnBrCoCreateDPlay)(void **ppOut);

/* REGDB_E_CLASSNOTREG -- what an unhooked BrSub1003C520 reports. */
#define BR82_HR_CLASSNOTREG  ((int32_t)0x80040154)

/* =========================================================================
 * 4. Adapters onto bodies that already exist
 * ========================================================================= */

/* 0x1003445A -- slice2_20.c:56's name for the address slice2_19.c:268
 * already implements as BrDlOwnerFixup(BrDlOwner *).
 *
 * This one is worth spelling out because the address was on the "intractable"
 * list for the wrong reason: the OLD sweep map gave 0x1003445A 549 bytes.
 * The rebuilt map gives it 125, and the disassembly at 125 bytes is exactly
 * the five-line function slice2_19.h:270 already documents.  The 424 bytes
 * the old map added belong to 0x100344D7, a separate function. */
void BrSub1003445A(void *pv);

/* 0x1003E0E0 -- slice3_31.h:288's name for the address slice2_23.c:320
 * already implements as BrUiFn1003E0E0(const BrActiveFlags *).  25 bytes;
 * both maps agree about the extent.  4 call sites in port/src, the most of
 * any addressed stub still outstanding.
 *
 * The original reads nine globals directly; slice2_23 lifted them into a
 * BrActiveFlags, and the only instance of that struct in the tree is
 * `static BrActiveFlags g_active` inside port/host/brally.c -- unreachable
 * from a module.  Rather than create a SECOND instance (which would be the
 * aliased-storage bug CONVENTIONS.md opens with: one original object, two
 * host objects, silently drifting), the flags are reached through a pointer
 * the host can aim at its own instance.
 *
 * DEVIATION while that pointer is NULL: an all-zero flag block is used, so
 * the "is anything active" half answers 0 and the result is decided entirely
 * by the input-edge half.  That is a real behavioural gap, and it is
 * reported rather than hidden -- but it is strictly closer to the original
 * than the stub's unconditional 0, which answered "no" even when a key was
 * down. */
int32_t BrExt_1003E0E0(void);

/* Aim this at the host's BrActiveFlags to close the gap above.  Typed void*
 * so that this header does not have to pull in br_state.h, and so that a
 * host holding its own model of the same nine globals can bind without a
 * cast war.  It is passed straight through to BrUiFn1003E0E0. */
extern void *g_pBrActiveFlags82;

/* 0x10043CD0 -- slice2_26.h:299's name for the address slice2_25.c:819
 * already implements as BrOptOpen2940(BrGameObj *).  The argument is unread
 * in the original, exactly as slice6_76.c:223 records for its sibling
 * 0x10043BF0, and the int return is discarded at this call shape. */
void BrExt_10043CD0(int32_t a);

/* =========================================================================
 * 5. WHAT THIS PACKET FOUND AND DID NOT LAND
 * =========================================================================
 *
 * FIFTEEN MORE STUBS ARE DUPLICATE NAMES FOR ADDRESSES THAT ALREADY HAVE A
 * BODY.  Found by grepping every stub ADDRESS against every banner comment in
 * port/src, per CONVENTIONS.md.  Each is a few lines of adapter once the
 * question at the end of its row is answered; none is a decompilation job:
 *
 *   0x10035CE0  BrEnt35CE0        = slice2_19.c:646 BrPadTranslate(BrPad*)
 *                 BLOCKED, AND THE BLOCKER IS AN LP64 LAYOUT BUG -- see below.
 *   0x1003DFC0  BrExt_1003DFC0    = slice2_23.c:287 BrUiFn1003DFC0
 *                 needs a BrStartupState instance; none exists.
 *   0x1004A580  BrPhaseEnterPlaceholder_1004A580 = slice3_33.c:103 BrExt_1004A580
 *   0x1004B430  ...1004B430       = slice3_33.c:346 BrExt_1004B430
 *   0x1004BDC0  ...1004BDC0       = slice3_33.c:497 BrExt_1004BDC0
 *   0x1004C4A0  ...1004C4A0       = slice3_33.c:601 BrExt_1004C4A0
 *   0x1004CAC0  BrOptFn1004CAC0   = slice3_33.c:694 BrExt_1004CAC0
 *                 all five: ARITY CONFLICT.  slice2_26.h declares one
 *                 argument (BrPhase *pSelf, __thiscall); slice3_33.h declares
 *                 two (BrUiBuildCtx *, BrUiPhase *).  One of the two is
 *                 wrong about the calling convention and it must be
 *                 adjudicated against the disassembly, not cast away.
 *   0x10043260  BrExt_10043260    = slice2_25.c:543 BrOptOpen296C
 *   0x10043330  BrExt_10043330    = slice2_25.c:548 BrOptOpen2970 (probable)
 *                 slice2_25.c banners THREE addresses over FOUR functions, so
 *                 the pairing is ambiguous by inspection.  Resolve by which
 *                 global each body writes before adapting.
 *   0x10005DE0  BrX10005DE0       = slice2_12.c:528 BrNetSlotGetF030
 *   0x10005E70  BrX10005E70       = slice1_02.c:513 BrNetSlotName
 *   0x10005FE0  BrSub10005FE0     = slice1_02.c:535 BrNetDropMatching
 *                 all three: need the BrNetState instance those modules
 *                 lifted the 0x978-stride table into.  SEE THE SIGNATURE
 *                 CONFLICT NOTE BELOW -- slice2_17 is passing a POINTER where
 *                 the original takes an INDEX.
 *   0x10034C66  BrX10034C66       = slice1_05.c:332 BrHookSetC
 *                 needs a BrHooks instance for the 0x106C0964 global.
 *   0x1003E1D0  BrSub1003E1D0     = slice1_06.c:181 BrPairBufReset
 *                 BLOCKED BY AN ALIAS -- see below.
 *   0x1003563A  BrX1003563A       = slice2_19.c:474 BrAnimUpdate
 *                 TYPE CONFLICT: slice2_17.c:74 declares `void (int a0)` and
 *                 calls it with an int; slice2_19 declares `void (BrAnimSet*)`.
 *
 * TWO ALIASED-STORAGE INSTANCES, both new, both matching the pattern
 * CONVENTIONS.md predicts (one packet names a global positionally, another
 * semantically; neither can find the other by grepping its own name):
 *
 *   0x10ACED34  `uint8_t *g_pBrMenuACED34` (br_data.c:415, "the record
 *               BrMenuAutoSaveName clears")  vs  `BrPairBuf::pA`
 *               (slice1_06.h:178).  This is what blocks 0x1003E1D0: the
 *               function lazily points that global at a 0x53-dword static
 *               block and zeroes it, so the two views must be one object
 *               before it can be adapted.
 *   0x10A9D008  `static BrComHolder *g_pComHolder` (slice1_03.c:379)  vs
 *               `BrOptUi *g_brPA9D008` (slice2_25.h:369).  slice1_03's is
 *               `static`, so the link is clean and there are genuinely two
 *               objects for one original pointer.  BrSub10058700 above reads
 *               it through slice2_25's name, following slice2_25.c:743 which
 *               does the identical lookup.
 *
 * ONE LP64 LAYOUT HAZARD, which is why 0x10035CE0 is not adapted here.
 * slice2_18.c:783 walks records of stride 0x15C and hands each to
 * BrEnt35CE0.  slice2_19.h:470's BrPad has `BrPadRaw *pRaw` at +0x158, so on
 * a 64-bit host sizeof(BrPad) is at least 0x160 -- FOUR BYTES PAST THE END OF
 * THE RECORD.  Overlaying BrPad on that array would have every element after
 * the first read its neighbour's bytes, and would write past the last one.
 * This is CONVENTIONS.md's "byte offsets are 32-bit-only" rule producing a
 * concrete out-of-bounds, and it must be fixed in BrPad (index, not pointer)
 * before the adapter is safe.
 *
 * ONE EXTENT CORRECTION THAT UNBLOCKS A FUNCTION THIS PACKET CANNOT OWN.
 * 0x1006F4A0 was 152 bytes in the sweep map and is 134 in the rebuilt one;
 * the missing 18 are the four-entry jump table at 0x1006F528 plus padding,
 * i.e. DATA that the old extent swallowed.  The body is fully resolved:
 *
 *     for (i = 0; i < 4; ++i) {
 *         sub = pCar->apSub[i];               // 0x168, 0x16C, 0x170, 0x174
 *         v   = -f_1006F0C0(pCar164, sub);    // `fchs` on the return
 *         sub->f1D8 = v;
 *         w = (v > 0.0f) ? 0.0f : v;          // clamp above; NaN keeps v
 *         sub->f80 = !(w >= -0.4f) ? -0.4f : w;   // clamp below; NaN -> -0.4
 *     }
 *
 * Both constants were read out of the image, not assumed: 0x1008FAB0 is
 * 0.0f and 0x1008FC00 is -0.4f, and the -0.4f is ALSO materialised as the
 * immediate 0xBECCCCCD in ebp -- the same value twice, which is the second
 * sighting that confirms it.  Both comparisons are written negated because
 * `fcom` + `test ah` takes the true side for unordered.
 *
 * It is NOT landed here, and the reason is a signature problem, not a
 * transcription one: the four sub-objects live at pCar+0x168..0x174 and
 * slice3_40.c already models exactly those four through BR_CAR_SUBPTR,
 * precisely because raw 32-bit offsets into the car record do not survive
 * LP64.  The declared signature `void BrSub1006F4A0(void *pCar164)` cannot
 * get back to pCar.  It belongs in slice3_40.c as
 * `void BrSub1006F4A0(BrCar *pCar)`; reproducing it here from a `void *`
 * would mean overlaying a struct on a foreign buffer.
 *
 * SIGNATURE CONFLICTS FOUND (reported, never silently resolved)
 * ============================================================
 *   0x10005DE0 / 0x10005E70.  Both take ONE argument in the original and use
 *   it as an INDEX, scaled by 0x978 (`lea ecx,[eax+eax*4]` twice, then
 *   `lea eax,[eax+ecx*4]`, `lea esi,[eax+eax*2]`, `shl esi,3`).  slice2_17.c
 *   :86 and :91 declare that argument `void *pOwner` and slice2_17.c:615
 *   passes an owner POINTER.  slice1_02.c:513 and slice2_12.c:528 declare it
 *   `int32_t slot`.  The two readings cannot both be right; the disassembly
 *   says index.
 *
 *   0x1003C520.  slice6_70.h:359 says `int32_t (struct BrDPlay **)`; the
 *   br_stubs declaration is `long (void)`.  The header is right -- confirmed
 *   above -- and this is one of the cases where the header was NOT wrong.
 *
 *   0x1006F4A0.  `void (void *pCar164)` (slice3_40.h:187) cannot express the
 *   function; see above.
 *
 *   0x1003563A.  `void (int)` vs `void (BrAnimSet *)`; see above.
 *
 *   0x1004A580 / 0x1004B430 / 0x1004BDC0 / 0x1004C4A0 / 0x1004CAC0.  One
 *   argument vs two; see above.
 *
 * STILL OUTSTANDING AND STILL WORTH THE MOST
 * ==========================================
 *   0x1006F310  BrProbe1006F310.  7 call sites and it RETURNS FLOAT, so its
 *   stub is ACTIVELY WRONG today -- callers read xmm0, which the stub never
 *   writes, so they get whatever the last float computation left there, not
 *   zero.  Declined again for the reason packets 76 and 78 gave, unchanged:
 *   it walks the collision grid at 0x11750338/0x117554A0, the one aliased-
 *   storage instance CONVENTIONS.md records as unresolvable on this host.
 *   It is the single highest-value remaining stub and it is blocked on
 *   making BrCollPlane store vertex INDICES.
 */

#endif /* SLICE7_82_H */
