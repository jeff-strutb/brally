/* br_wire75.c -- stubs that were never missing code.
 *
 * WHAT THIS FILE IS FOR
 *
 * port/host/br_stubs.c lists 135 functions as unported. Auditing that list
 * against the ported tree shows a distinct category inside it: entries where
 * the ORIGINAL ADDRESS is already decompiled, tested and linked into the same
 * binary -- under a different NAME. The stub exists because the caller's
 * packet coined its own name for the address and nothing ever joined the two
 * up. Those are not decompilation work. They are one line each, and the body
 * they reach is already covered by that module's own test suite.
 *
 * Only exact matches are wired here. Where the two sides disagree about
 * arity, argument type, or an implicit `this`/state pointer, the entry is
 * NOT wired -- it is listed at the bottom with the specific mismatch, because
 * closing one of those needs an adjudication (or new storage for a global),
 * and guessing would produce exactly the wrong-but-plausible code
 * CONVENTIONS.md warns about.
 *
 * Each wiring below states the address, the two names, and why the signatures
 * are the same function rather than merely compatible.
 */

#include <stdint.h>

#include "br_bits.h"       /* BrHandleLookup      (0x10074030) */
#include "slice2_18.h"     /* BrScissorSet        (0x1003289F) */
#include "slice4_52.h"     /* g_apBrStrTable      (0x11829370) */

/* --------------------------------------------------------------------------
 * 0x1003289F  BrSub1003289F  ==  BrScissorSet
 *
 * slice2_14.c declares `void BrSub1003289F(int, int, int, int)` and calls it
 * with a four-element clear rect. slice2_18.c defines
 * `void BrScissorSet(int32_t x, int32_t y, int32_t w, int32_t h)` at exactly
 * 0x1003289F, and slice2_18.h records the (x, y, w, h) order explicitly
 * because it is NOT (x0, y0, x1, y1). Same arity, same types, same order.
 *
 * This address is also one the function map flagged as suspect and the
 * README later cleared as genuine, so the identification is settled.
 * -------------------------------------------------------------------------- */
void BrSub1003289F(int a, int b, int c, int d);
void BrSub1003289F(int a, int b, int c, int d)
{
    BrScissorSet((int32_t)a, (int32_t)b, (int32_t)c, (int32_t)d);
}

/* --------------------------------------------------------------------------
 * 0x10076AE0  BrX10076AE0  ==  BrEntitySetIndex
 *
 * slice2_17.c declares `void BrX10076AE0(void *pThis, int a0)`; slice1_09.c
 * defines `void BrEntitySetIndex(void *pEntity, int index)` at 0x10076AE0.
 * Identical signature, and slice1_09.c carries the behaviour that matters
 * (>= 16 selects the second bank and subtracts 16; the compare is SIGNED).
 * -------------------------------------------------------------------------- */
void BrEntitySetIndex(void *pEntity, int index);
void BrX10076AE0(void *pThis, int a0);
void BrX10076AE0(void *pThis, int a0)
{
    BrEntitySetIndex(pThis, a0);
}

/* --------------------------------------------------------------------------
 * 0x10035BBA  BrX10035BBA  ==  BrLogSet
 *
 * slice2_17.c declares `void BrX10035BBA(const char *psz)` and calls it with
 * a formatted buffer; slice2_19.c defines `void BrLogSet(void *p)` at
 * 0x10035BBA, which stores the pointer and emits. The original takes one
 * pointer argument; `const char *` and `void *` are the same argument.
 *
 * NOTE for the naming registry: the README records 0x10035BBA as carrying
 * THREE names (BrFatal, BrLogEmit, BrLogSet). This wiring adds no fourth --
 * it points the fourth spelling that already existed at the one definition.
 * The const cast is deliberate: BrLogSet stores the pointer and the emit path
 * only reads it, so nothing writes through the discarded qualifier.
 * -------------------------------------------------------------------------- */
void BrLogSet(void *p);
void BrX10035BBA(const char *psz);
void BrX10035BBA(const char *psz)
{
    BrLogSet((void *)(uintptr_t)(const void *)psz);
}

/* --------------------------------------------------------------------------
 * 0x10074030  BrExt_10074030  ==  BrHandleLookup, against 0x11829370
 *
 * slice3_31.h declares `void *BrExt_10074030(int32_t nId)` -- one argument,
 * which is the ORIGINAL's shape: br_bits.h records that the table address
 * 0x11829370 is baked into the instruction (`mov eax,[eax*4+0x11829370]`) and
 * that its own `apTable` parameter is a port DEVIATION added only to avoid a
 * hardcoded absolute address.
 *
 * So the wiring supplies the table the original hardcodes. It does NOT
 * allocate one: slice4_52.c already owns 0x11829370 as `g_apBrStrTable`, so
 * this reaches the same storage the rest of the port uses rather than adding
 * a second object for one address.
 *
 * Handle validity (1..0x12E, with 0 reserved as null) is enforced inside
 * BrHandleLookup and is not re-implemented here.
 * -------------------------------------------------------------------------- */
void *BrExt_10074030(int32_t nId);
void *BrExt_10074030(int32_t nId)
{
    return BrHandleLookup(g_apBrStrTable, (uint32_t)nId);
}

/* ==========================================================================
 * AUDITED AND DELIBERATELY NOT WIRED
 *
 * Each of these is also an address that IS ported, so each looks like a free
 * win; each has a concrete reason it is not one. Recorded so the next pass
 * does not have to re-derive the mismatch -- and does not wire one blind.
 *
 *   0x100586A0  BrSub100586A0(void)  vs  BrSlotsReset(BrSlotTable *)
 *       The original takes no argument; the port's parameter needs a
 *       BrSlotTable instance. README's erratum says that struct is a FICTION
 *       -- the slots (0x10AA2538) and the count (0x10AA288C) are 756 bytes
 *       apart -- and 0x10AA288C is separately the DirectPlay send gate.
 *       Manufacturing an instance here would create a second object for a
 *       global that already has a live dual role.
 *
 *   0x10069490  BrX10069490(void)  vs  BrPoolAlloc(BrPool *)
 *       Same shape of problem: the original hardcodes 0x10AF9BC0 /
 *       0x106C65EC / 0x10B01C40, and slice3_41.c already models the frame
 *       bank at 0x10069530. A local BrPool would be a third view.
 *
 *   0x1003563A  BrX1003563A(int a0)  vs  BrAnimUpdate(BrAnimSet *)
 *       The caller passes slice2_17's `f680944`, modelled as int32. Turning a
 *       32-bit value into a host pointer is precisely the LP64 hazard
 *       CONVENTIONS.md forbids. Needs slice2_17's field retyped first.
 *
 *   0x10005DE0  BrX10005DE0(void *, uchar *, uchar *, uchar *)   [4 args]
 *               vs BrNetSlotGetF030(BrNetState *, int32_t slot, uint8_t *,
 *                                  uint8_t *, uint8_t *)         [5 args]
 *       The caller has no slot argument. Which slot it means is a real
 *       question about the call site, not a cast.
 *
 *   0x10005E70  BrX10005E70(void *)  vs  BrNetSlotName(BrNetState *, int32_t)
 *       Same missing slot argument.
 *
 *   0x10034C66  BrX10034C66(void (*)(void))  vs  BrHookSetC(BrHooks *,
 *                                                           void (*)(void))
 *       __thiscall: the caller's declaration drops the implicit `this`, so
 *       wiring it needs a BrHooks global that nothing yet owns.
 *
 *   0x10002910  BrCdTrackGet(void)
 *       br_audio.c is a re-architected portable module keyed on a BrAudio
 *       instance, not a 1:1 transcription of this address. Connecting them is
 *       an architecture decision about who owns the BrAudio object.
 * ========================================================================== */
