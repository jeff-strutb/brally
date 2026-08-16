/* slice8_83.h -- Boss Rally, packet 83.
 *
 * WHAT THIS PACKET IS
 * ===================
 * Fourteen stubbed functions, chosen by STATIC CALL-SITE DEMAND rather than
 * by address range: every direct `call`/`jmp` target in both binaries' .text
 * was counted (recursive-descent disassembly over each function's extent in
 * config/functions_glide.csv and config/functions_d3d_flow.csv), and the
 * result was crossed with the call sites already present in port/.
 *
 * Two things fell out of that count that are worth recording, because both
 * are cheaper to believe than to re-derive:
 *
 *   1. TWO PAIRS OF STUBS ARE ONE ORIGINAL FUNCTION EACH.
 *        0x100607B0 is BOTH `BrCarRecordToState` (slice3_42.h:46) and
 *        `BrSub100607B0` (slice3_40.h:161).  Two names, two stub lines, one
 *        body -- so porting it retires two stubs.
 *        0x1003D210 is likewise both `BrFn1003D210` and `BrSub1003D210`.
 *        (0x1003D210 is not ported here: it is Win32 -- see the decline list.)
 *
 *   2. THE HIGHEST-DEMAND STUBS ARE PLATFORM CODE, NOT GAME CODE.
 *        0x1003D0B0 (8 sites) and 0x1003D210 (8 sites) are GlobalAlloc /
 *        GlobalLock / PostMessageA / DirectPlay-vtable bodies, and
 *        0x1005F5A0 (7 sites) is an IDirectDrawSurface::Blt retry loop
 *        (it tests DDERR 0x887601C2 and 0x8876021C by hand).  A demand rank
 *        that ignores that lands straight on the platform seam.
 *
 * REFERENCE: BRGlide.dll.  Every address named below is the D3D one, because
 * that is the numbering the rest of port/ uses; the Glide address actually
 * read is given alongside wherever the two differ.
 *
 * THE HOST-BINDING PATTERN, AND WHY IT IS USED HERE
 * ================================================
 * Nine of the fourteen are ADAPTERS: the original address already has a body
 * in this tree under a different name, found by grepping the ADDRESS (not the
 * name) across port/src.  What kept them stubbed was never the body -- it was
 * that each ported body takes an explicit `state` parameter that the original
 * read from a fixed global, and no module owns an instance of that state.
 *
 * This header does NOT invent that storage.  It exposes a POINTER per state
 * object, defaulting to NULL, exactly as slice7_82.h does with
 * `g_pBrActiveFlags82` and slice4_53.c does with `g_pBrSlice4PhaseCtx`.
 * While a pointer is NULL the adapter does nothing and returns zero -- which
 * is precisely what the generated stub did, so binding one can only improve
 * behaviour and never regress it.  Owning the storage here would have created
 * a second object for one original global, which is the aliased-storage bug
 * CONVENTIONS.md documents.
 *
 * The one exception is BrNetAnnounce (0x10003530): its three globals
 * (0x1021C9B0, 0x10226A38, 0x10226A54) are referenced NOWHERE else in port/,
 * so this module owns them outright and exports them.
 */
#ifndef SLICE8_83_H
#define SLICE8_83_H

#include <stdint.h>

#include "br_vec.h"      /* BrVec3                                          */
#include "br_mat.h"      /* BrMat4                                          */
#include "slice1_02.h"   /* BrCarState, BrNetState                          */
#include "slice1_05.h"   /* BrHooks                                         */
#include "slice2_16.h"   /* BrRcaFixup                                      */
#include "slice2_23.h"   /* BrStartupState                                  */
#include "slice2_26.h"   /* BrPhaseCtx (via slice4_53.h's instance pointer) */

/* =====================================================================
 * 0. Host bindings -- NULL means "behave exactly like the old stub"
 * ===================================================================== */

/* 0x10221328.  Wanted by 0x100054A0, 0x10005DE0, 0x10005E70 and 0x10005FE0,
 * all four of which slice1_02/slice2_12 ported with the table lifted into a
 * BrNetState parameter. */
extern BrNetState *g_pBrNetState83;

/* The other three globals 0x100054A0 carries, which slice2_12.h lifted into
 * parameters alongside the state: 0x1022AF34, 0x10094294 and 0x10003460. */
extern void     *g_hBrNet1022AF34_83;
extern int32_t   g_brLocalSlot83;      /* 0x10094294 */
extern uint32_t  g_brNowTicks83;       /* 0x10003460 */

/* 0x106C0964 and friends -- the object slice1_05's BrHookSetC writes into.
 * 0x10034C66 is thiscall in the original, so the `this` is invisible at the
 * call site and has to come from somewhere. */
extern BrHooks *g_pBrHooks83;

/* The two objects slice2_23's BrUiFn1003DFC0 was given instead of its ten
 * globals.  0x1003DFC0 takes NO arguments in the original -- verified, see
 * BrExt_1003DFC0 below -- so both have to be bound. */
extern BrStartupState *g_pBrStartupState83;
extern void           *g_pBrB4DF3083;      /* 0x10B4DF30 */

/* The context slice2_16's BrRcaFixupArray takes.  Its `enable` field is the
 * original's 0x10675540, and zero there already means "skip the copying", so
 * a NULL binding here is the same shape of no-op the original supports. */
extern const BrRcaFixup *g_pBrRcaFixup83;

/* =====================================================================
 * 1. 0x100695A0 -- matrix -> BrCarState (quaternion + translation)
 * =====================================================================
 *
 * Glide 0x10062610, 41 bytes.  NOT a stub in br_stubs.c -- nothing in port/
 * called it yet -- but 0x100607B0 opens with it, so it is transcribed here
 * rather than hidden inside its caller.
 *
 *     BrSub100765E0(pMat, (BrVec4 *)pDst);      // 0x100765E0, slice5_62
 *     pDst->f10 = pMat->m[3][0];
 *     pDst->f14 = pMat->m[3][1];
 *     pDst->f18 = pMat->m[3][2];
 *
 * ARGUMENT ORDER IS NOT THE OBVIOUS ONE and was traced through the pushes:
 * the wrapper receives (pDst, pMat) but forwards them to 0x100765E0 as
 * (pMat, pDst) -- matrix first, which is what slice3_45.h/slice5_62.h
 * already declare.  Do not "harmonise" it with the dest-first convention.
 *
 * The first four floats of BrCarState are the quaternion SCALAR FIRST, so
 * BrVec4's f00 lands on BrCarState's f00 -- the two agree, which is why the
 * original can hand one buffer to a routine typed for the other. */
void BrCarStateFromMatrix83(BrCarState *pDst, const BrMat4 *pMat);

/* =====================================================================
 * 2. 0x100607B0 -- car record -> BrCarState        [RETIRES TWO STUBS]
 * =====================================================================
 *
 * Glide 0x10059820, 554 bytes.  Declared twice in port/include, identically
 * except for the second parameter's type:
 *     slice3_42.h:46   void BrCarRecordToState(BrCarState *, void *)
 *     slice3_40.h:161  void BrSub100607B0     (BrCarState *, BrCar *)
 * Both are defined here; the second forwards to the first.
 *
 * ASYMMETRIES WITH ITS INVERSE (0x10060A10), ALL PRESERVED:
 *
 *   - The four bytes at +0x510, +0x928, +0x71C and +0xB34 are read
 *     SIGN-extended (`movsx`), while +0x36D and the eight at +0x362.. are
 *     read ZERO-extended (`xor eax,eax` then `mov al,`).  Reading all nine
 *     the same way is wrong for whichever group you pick.
 *   - f70 tests TWO flag bits (0xC0000) but 0x10060A10 only ever writes ONE
 *     of them (0x40000).  The round trip is therefore not the identity when
 *     0x80000 is set on the way in.
 *   - +0x73C is copied to f38; the inverse writes f38 back to BOTH +0x73C
 *     and +0xB54.
 *
 * f78 IS A LAP FIELD, NOT A POINTER.  The original compares the dword at
 * car+0xFA8 against the global 0x100BCBE8 (Glide) / 0x100BD3E0 (D3D) and
 * substitutes 4188888.0f when they match.  That global is an int32 -- it is
 * slice2_25.c's `g_br0BD3E0`, the 1-based lap cycler, and br_race.h's
 * `BrRaceRules::nLaps` under the OTHER build's address.  So the rule is
 * "on the final lap, report the sentinel instead of the best time".
 *
 * CONSTANTS were read out of BOTH images and agree: 1.0f, 0.0f, 4188888.0f
 * and -1000.0f, at Glide 0x1007776C/70/74/78 and D3D 0x1008F7A4/A8/AC/B0. */
void BrCarRecordToState(BrCarState *pDst, void *pCar);
void BrSub100607B0(BrCarState *pDst, void *pCar);

/* =====================================================================
 * 3. 0x10060A10 -- BrCarState -> car record
 * =====================================================================
 *
 * Glide 0x10059A80, 673 bytes.  The inverse of the above, and the function
 * that fills the car's BrMat4 at +0x220 (via slice3_42's BrMat4FromCarState,
 * 0x100695D0) and its BrRbState at +0x1DC (via slice3_44's
 * BrRbQuatDerivative, 0x100742D0), then mirrors that 0x44-byte state into
 * +0x278 and +0x2BC.
 *
 * THE +0x0FF4 UPDATE IS CONDITIONAL AND THE CONDITION IS TWO COMPARISONS:
 *
 *     if (!(car.f0FF4 > 0.0f) || !((car.f0FF4 - (-1000.0f)) > pSrc->f78))
 *         car.f0FF4 = pSrc->f78;
 *
 * i.e. "take the new time if we have no time yet, or if ours plus a thousand
 * is not still better".  Both are spelled as negated `>` because `fcom` +
 * `test ah,0x41` takes the assign side for unordered, so a NaN anywhere in
 * that expression assigns.
 *
 * +0xE68 is written as a SIGN (-1.0f / +1.0f) from the boolean f74, which is
 * the inverse of 0x100607B0 reading it back as `f74 = (E68 < 0)`. */
void BrCarRecordFromState(void *pCar, const BrCarState *pSrc);

/* =====================================================================
 * 4. 0x10003530 -- BrNetAnnounce
 * =====================================================================
 *
 * Glide 0x100038A0, 77 bytes.  Mutex, strcpy into a fixed global buffer,
 * set a "there is a message" flag, release.  slice1_02.c:567 already calls
 * it with the string 0x10005FE0 formats.
 *
 * This module OWNS the three globals because nothing else in port/ mentions
 * them.  The buffer's true length is not recoverable (the original's copy is
 * an unbounded inlined `repne scasb` + `rep movsd`), so it is generous and
 * the copy is bounded -- a DEVIATION, and the only one. */
#define BR83_ANNOUNCE_MAX 0x400

extern char     g_aBrAnnounce83[BR83_ANNOUNCE_MAX];  /* 0x1021C9B0 */
extern int32_t  g_brAnnouncePending83;               /* 0x10226A38 */
extern void    *g_hBrAnnounce83;                     /* 0x10226A54 */

void BrNetAnnounce(const char *pszText);

/* =====================================================================
 * 5. Adapters
 * ===================================================================== */

/* 0x100054A0.  slice2_12.c ports this as BrNetSlotPredict with the four
 * globals 0x10221328 / 0x1022AF34 / 0x10094294 / 0x10003460 lifted into
 * parameters; the original takes only (pDst, slot).  slice3_40.h asked for
 * exactly this binding by name. */
int BrNetSlotPredictOrig(BrCarState *pDst, int32_t slot);

/* 0x10005DE0 and 0x10005E70.
 *
 * SIGNATURE CONFLICT, ADJUDICATED AGAINST THE DISASSEMBLY.  slice2_17.c:86
 * and :91 declare the single argument `void *pOwner` and pass an owner
 * pointer; slice1_02.c:513 and slice2_12.c:528 declare it `int32_t slot`.
 * The original scales that argument by 0x978 and uses it as a table index
 * (`lea ecx,[eax+eax*4]` twice, `lea eax,[eax+ecx*4]`, `lea esi,[eax+eax*2]`,
 * `shl esi,3`), so it is an INDEX.  Confirmed a second way at the only call
 * site, Glide 0x1001C6A0 (D3D 0x1002F130), which loads a plain int global and
 * pushes it -- there is no object there to take the address of.
 *
 * These adapters therefore read the argument as an index, and BOUND it: a
 * value outside 0..BR_NET_SLOTS-1 (which is what a real host pointer would
 * look like) is refused and the old stub's answer is returned, rather than
 * indexing 0x978 bytes off the end of the slot table. */
int         BrX10005DE0(void *pOwner, unsigned char *pb0,
                        unsigned char *pb1, unsigned char *pb2);
const char *BrX10005E70(void *pOwner);

/* 0x10005FE0 = slice1_02.c:535 BrNetDropMatching.  NO index hazard here: the
 * argument is matched against each slot's +0x004 (a player id), not used to
 * subscript anything, so slice2_13.c's `uint32_t idPlayer` is right. */
void BrSub10005FE0(uint32_t idPlayer);

/* 0x10034C66 = slice1_05.c:332 BrHookSetC.  Thiscall in the original. */
void BrX10034C66(void (*pfn)(void));

/* 0x1002BA80 = slice2_16.c:1345 BrRcaFixupArray. */
void BrSwapRec24Array(void *pv, int n);

/* 0x1003DFC0 = slice2_23.c:287 BrUiFn1003DFC0.
 *
 * ARITY: slice2_26.h:311 declares `void (void)` and IS RIGHT -- the original
 * (Glide 0x10037660, 66 bytes) is ten stores to fixed globals and reads
 * nothing off the stack.  slice2_23 lifted those globals into
 * BrStartupState + the 0x10B4DF30 object, so the adapter supplies both. */
void BrExt_1003DFC0(void);

/* 0x10043260 and 0x10043330 = slice2_25.c:543/548 BrOptOpen296C /
 * BrOptOpen2970.  Packet 82 called this pairing "probable" because
 * slice2_25.c banners three addresses over four functions.  It is now PROVEN
 * by the globals: 0x10043260 tests and writes 0x10AA296C and installs the
 * method 0x10051990, 0x10043330 tests and writes 0x10AA2970 and installs
 * 0x10051D30, and slice2_25.c's two bodies do exactly that, in that pairing.
 *
 * The int return is discarded, matching slice3_31.h's `void (void *)`. */
void BrExt_10043260(void *pArg);
void BrExt_10043330(void *pArg);

/* 0x10044970 = slice2_26.c BrPhaseLeave_10044970.
 *
 * ARITY: the original takes ONE argument (`push esi; mov esi,[esp+8]`), the
 * entity, which is what slice2_25.h declares.  slice2_26 lifted the phase
 * globals into a leading BrPhaseCtx *.  This adapter reuses slice4_53.c's
 * ALREADY EXISTING `g_pBrSlice4PhaseCtx` rather than coining a second
 * pointer at the same object -- slice4_53.c:248 binds its sibling
 * BrPhaseLeave_10044A30 through that very pointer. */
void BrOptFn10044970(void *pEntity);

#endif /* SLICE8_83_H */
