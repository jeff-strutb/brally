/* slice8_84.h -- the control hooks the SLICE 6_71 and SLICE 6_72 builders
 * install, over br_ui.h's struct model.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS MODULE IS
 *
 * slice7_80.c filled the option-cycling slots of BrUi73Hooks / BrUi72Hooks and
 * slice7_81.c filled eleven screen-entry slots of BrUi73Hooks. Neither touched
 * BrS71Hooks at all, and BrUi72Hooks still had 51 of its 55 slots NULL. This
 * module is the same job for the ten screens packets 71 and 72 build:
 *
 *   port/src/slice6_71.c  0x10049F40  0x10051D30  0x1004F700  0x100575F0
 *   port/src/slice6_72.c  0x10056A10  0x10057C10  0x10052030  0x10059760
 *                         0x1005A6E0  0x1004E830
 *
 * EVERY pairing below was read out of one of those two transcriptions, and the
 * file:line of the store is quoted with each hook. Nothing is inferred from a
 * screen's name or from what a hook "looks like it should" belong to.
 *
 * ---------------------------------------------------------------------------
 * THE ONE FACT THAT DECIDES THE SHAPE OF THIS FILE
 *
 * A control hook slot is `BrUiCtlHookFn_`, i.e. `int32_t (*)(BrUiCtl_ *)`
 * (br_ui.h ADJ-8). Almost every one of these addresses ALREADY HAS A BODY in
 * the tree -- and almost none of those bodies can be given a `BrUiCtl_ *`:
 *
 *   slice2_23.c  models the argument as `BrUiObj`, a BYTE ARRAY, and reads it
 *                at literal 32-bit offsets (+0x1C, +0x2B5C, +0x1E20C ...).
 *   slice2_24.c  models it as `BrMenuItem`, a byte image of the same object
 *                (f1C +0x1C, text +0x2B5C, f1E20C +0x1E20C) -- the original's
 *                offsets, so it agrees with BrUiCtl_ on a 32-bit target and
 *                parts company with it under LP64.
 *   slice2_25.c  models it as `BrGameObj`, `pad0000[0x2AE8]; pSub; ...` -- a
 *                byte image with the original's offsets.
 *   slice3_31.c  reaches the same object through Br31Ld32(p, off) with the
 *                original's offsets, and through slice2_25.h's BrGameObj.
 *   slice2_26.c  takes `(BrPhaseCtx *, void *)` -- two arguments, and the
 *                first is a context this tree never instantiates.
 *
 * On a 32-bit host every one of those reads the same bytes as `BrUiCtl_`. On
 * LP64 they do not: `BrUiCtl_::pOwner` is at original +0x2AE8 but at a host
 * offset the widened struct has moved. slice7_81.h states this at length and
 * calls it load-bearing; this module found it again from the other end. So:
 *
 *   ADAPTER  is used only where the original PROVABLY does not read its stack
 *            argument, verified in the disassembly, not assumed from a
 *            parameter name. Six hooks qualify.
 *   TRANSCRIBE is used for the rest, exactly as slice7_81.c transcribes the
 *            same family for packet 73. Nineteen bodies, filling seventeen
 *            table slots -- three of them (0x10046DC0, 0x10046830, 0x10046870)
 *            are in no table at all and are poked into another control's
 *            +0x08 at run time, which is the original's own mechanism.
 *   NEITHER  is possible for the slice2_23 / slice2_24 caption-and-text
 *            families. They are listed under NOT DONE below rather than being
 *            given a cast that would link and read a wild pointer.
 *
 * ---------------------------------------------------------------------------
 * DIRECT INSTALL -- 0x10047360, and it is the highest-value slot here
 *
 * 0x10047360 is the +0x0C hook on EVERY selectable row of all ten screens
 * (26 stores across the two files). It already has a control-typed body:
 * br_sprfont.c's BrSprFontKindHook_10047360, which port/host/brally.c already
 * installs into BrUi73Hooks for the 73 screens. No adapter is needed or
 * wanted; BrUiHook84Install71 / ...72 store that function itself, so there is
 * one body and one name for the address across all three tables.
 *
 * ---------------------------------------------------------------------------
 * STORAGE OWNERSHIP, stated address by address
 *
 * OWNED ELSEWHERE -- reached, never duplicated:
 *   0x10AA2904 / 0x10AA2908 / 0x10AA2924 / 0x100AA010  br_uinav.h  BrUiNav
 *   0x10AA2918 0x10AA291C 0x10AA2928 0x10AA292C 0x10AA2940 0x10AA2974
 *   0x10AA28E4                                   slice7_81.h  g_brHook81
 *   0x10AA29B0  a CONTROL                        slice6_71.h  g_brS71
 *   0x10AA29C8 0x10AA29F4  CONTROLS              slice6_73.h  g_br73
 *   0x10AA29CC 0x10AA2518 0x10A9D618 0x10AA28A4  slice6_73.h  g_br73
 *   0x10AA29E8  a CONTROL                        slice6_72.h  g_pBr72Env
 *   0x1039B720                                   slice6_73.h  g_aBr39B720
 *   0x100AB3F4                                   slice5_61.h  g_br0AB3F4
 *
 * OWNED HERE (BrUiHook84Ctx below) -- nine words no other header defines
 * storage for. slice2_26.h's BrPhaseCtx and slice3_31.h's BrPhaseCtx31 both
 * DECLARE most of them; neither is instantiated anywhere in the tree, so
 * there is no aliasing today. If either is ever instantiated, exactly one of
 * the two must keep the storage and the other must alias into it:
 *   0x10AA2930  0x10AA2948  0x10AA295C  0x10AA2968  0x10AA2970
 *   0x10AA2988  0x10AA298C  0x10AA28E0  0x10AA26F5
 *
 * ---------------------------------------------------------------------------
 * CONFLICTS FOUND (reported, deliberately not silently "resolved")
 *
 * 1. RETURN TYPE, 0x10046FC0. slice3_31.h declares it
 *      void BrPhaseGoto_10046FC0(void)
 *    and slice3_31.c implements it as a one-statement void function. The
 *    original is thirteen bytes and the last three are `xor eax,eax / ret`:
 *    it returns 0, and the 0 is load-bearing -- 0x10048180 stops the frame on
 *    a 0 from a +0x08 hook and 0x10048530 then stops walking the page, which
 *    is how a row that just tore its own screen down does not go on being
 *    walked. Same finding slice7_81.h records as its CONFLICT 4 for the whole
 *    family. The `int32_t` reading is used here.
 *
 * 2. ARITY, 0x10046FC0 again. slice3_31.h declares it `(void)`. It IS `(void)`
 *    in the original -- the body never touches [esp+4] -- but the SLOT it is
 *    stored into is called with one argument, so the port needs the
 *    one-argument shape. Adapted, not re-declared.
 *
 * 3. 0x10046870 has TWO bodies in the tree already and they disagree about
 *    what it is. slice3_31.c has BrPhaseLeaveNamed_10046870 -> g_pExt->pAA2930,
 *    which matches the disassembly's final `mov ecx,[0x10AA2930] / mov
 *    [0x10AA2904],ecx`. The name-reset half is byte-for-byte slice7_81.c's
 *    Br81NameReset. So the two agree on behaviour and differ only in the model
 *    of the argument; this module transcribes it a third time only because
 *    neither existing body can take a BrUiCtl_ *.
 *
 * 4. PHASE VTABLE +0x18's ARGUMENT. br_phase.h types it
 *      void (*f18)(BrPhase_ *pThis, void *pArg)
 *    but 0x10043FA0 pushes the LITERAL 1 at it (`push 1` at 0x10043FA4), and
 *    slice2_25.c reads the same slot as `pfnSlot6(pSub, 1)` -- an int. The
 *    two callers in this file that reach +0x18 therefore have to cast an
 *    integer to `void *`. That cast is the conflict, not a solution to it:
 *    br_phase.h should type the argument `int32_t`.
 *
 * 5. tools/whereis.py REPORTS A FALSE IDENTITY for this family. It pairs
 *    0x10044C70, 0x10046710 and 0x10047060 all to Glide 0x1003E1C0 "matched by
 *    body", because its normalisation masks the global addresses -- and the
 *    global addresses are the ONLY thing that differs between these three
 *    56-byte functions:
 *      0x10044C70  next 0x10AA2908 (the ROOT phase), clears 0x10AA295C
 *      0x10046710  next 0x10AA2918,                  clears 0x10AA2988
 *      0x10047060  next 0x10AA292C,                  clears 0x10AA2930
 *    Only 0x10044C70 is really Glide 0x1003E1C0 (whose next is current+4, as
 *    0x10AA2908 is 0x10AA2904+4). Read the listings before trusting a body
 *    match on a function whose whole content is which global it moves.
 *
 * ---------------------------------------------------------------------------
 * NOT DONE, AND WHY -- the remaining slots these ten builders install
 *
 * A. THE slice2_23 FAMILY -- pfn04 / pfn10 / pfn14 "poll, code and caption"
 *    hooks. Bodies exist (port/src/slice2_23.c) but take
 *    `(BrUiObj *, BrUiGlobals *)`: two arguments, and the first is the byte
 *    array described above. Adapting one needs a marshal, not a cast.
 *      0x1003E7A0 0x1003EAE0 0x1003EC30 0x1003EF90 0x1003F020 0x1003F210
 *      0x1003F280 0x1003F5E0 0x1003F680 0x1003FA00 0x1003FCB0 0x1003FD30
 *      0x1003FDA0 0x1003FE10 0x1003FE80
 *
 * B. THE slice2_24 FAMILY -- pfn04 caption/text setters. Bodies exist
 *    (port/src/slice2_24.c) over `BrMenuItem`, a byte image at the original's
 *    own displacements -- which coincides with BrUiCtl_ only on 32-bit.
 *      0x10040730 0x100407E0 0x100408D0 0x10040950 0x10040990 0x100409B0
 *      0x100409D0 0x10040A50 0x10040AC0 0x10040B30 0x10041040 0x10041180
 *      0x10041300 0x100415A0
 *
 * C. NO BODY ANYWHERE. All five sit inside slice2_24's own declared range
 *    (0x10040450-0x10042740) and were skipped by that pass; they belong with
 *    it, not here, because porting them here would need a second copy of that
 *    module's state block.
 *      0x100413B0  text setter, 245 B, string ids 0xB3..0xB6 off 0x10AA28A0
 *                  -- NO LONGER TRUE: slice8_88.c has it, over BrUiCtl_.
 *      0x100414B0  text setter, 236 B, 0x10AA289C / 0x10AA28B8 / 0x10AA270E
 *                  -- NO LONGER TRUE: slice8_88.c has it, over BrUiCtl_.
 *                  Both needed no second copy of slice2_24's state block
 *                  after all: every global but three is already owned by
 *                  slice6_73.h's g_br73, and slice8_88.h names the three.
 *      0x10042170  BrTextList +0x04 callback (slice6_71.c:454)
 *      0x10042560  BrTextList +0x14 callback (slice6_72.c:1293)
 *      0x10042740  BrTextList +0x04 callback (slice6_72.c:1292)
 *
 * D. TOO BIG FOR AN ADAPTER-AND-TRANSCRIBE PASS, left as visible holes:
 *      0x10046260  284 B, slice3_31.c BrPhaseActivate_10046260(void). The
 *                  argument IS unread (its one [esp+4] is a local slot), so
 *                  this one is adaptable -- but its body writes fourteen
 *                  globals through slice3_31's context, which nothing wires.
 *      0x10044D00  221 B, an ACTIVATE with an SEH frame.
 *      0x10047250  slice3_31.c BrPhaseEdit_10047250(void *) -- reads the arg.
 *      0x100474B0  slice3_31.c BrPhaseTick_100474B0 -> BrSub10047360, which
 *                  byte-addresses the control. br_sprfont.c's control-typed
 *                  0x10047360 makes this a two-line adapter for a later pass.
 *      0x10042AC0 / 0x10042B00 / 0x100437D0 / 0x100444C0
 *
 * E. ALREADY INSTALLED BY slice7_80.c's BrUiOptInstall72 and deliberately NOT
 *    touched here, so the two installers cannot fight over a slot:
 *      0x10043590 0x100435F0 0x10043650 0x100436B0   (BrExt_1004E830's rows)
 *
 * ---------------------------------------------------------------------------
 * REFERENCE BINARY
 *
 * Every listing quoted below was read with tools/dumpasm.py. The addresses in
 * this family are D3D addresses, so BR_REF=orig/BRD3D.dll was used to read
 * them and orig/BRGlide.dll (the project reference) to cross-check the shape;
 * see CONFLICT 5 for what the cross-check found.
 * ---------------------------------------------------------------------------
 */
#ifndef SLICE8_84_H
#define SLICE8_84_H

#include <stdint.h>

/* slice7_81.h pulls slice6_73.h, slice6_71.h and br_uinav.h in that order --
 * the order port/host/brally.c uses and the only one that works, because
 * br_uinav.h renames slice3_32.h's model. slice6_72.h is added after them;
 * slice6_73.h's CONFLICT 3 is what made that legal. */
#include "slice7_81.h"   /* g_brHook81, BrUiHook81Activate_*                 */
#include "slice6_72.h"   /* BrUi72Hooks, Br72Env, g_pBr72Env                 */
#include "br_sprfont.h"  /* BrSprFontKindHook_10047360 -- 0x10047360         */

/* ===========================================================================
 * The nine words this module owns. See STORAGE OWNERSHIP above.
 * ========================================================================== */
typedef struct BrUiHook84Ctx {
    BrPhase_ *pAA2930;   /* 0x10AA2930  destination of 0x10046830/0x10046870 */
    BrPhase_ *pAA2948;   /* 0x10AA2948  destination of 0x10043F50            */
    BrPhase_ *pAA295C;   /* 0x10AA295C  destination of 0x10044F00            */
    BrPhase_ *pAA2968;   /* 0x10AA2968  NOTIFIED, then cleared, by 0x10044F00 */
    BrPhase_ *pAA2970;   /* 0x10AA2970  notified and cleared by 0x100471B0   */
    BrPhase_ *pAA2988;   /* 0x10AA2988  cleared by 0x10046710                */
    BrPhase_ *pAA298C;   /* 0x10AA298C  cleared by 0x10043F50 and 0x10044B40 */
    int32_t   nAA28E0;   /* 0x10AA28E0  cleared by 0x10046E10               */
    uint8_t   bAA26F5;   /* 0x10AA26F5  cleared by 0x10047340 -- a BYTE      */
} BrUiHook84Ctx;

/* The single instance, zero-initialised. NULL is the original's state too:
 * these are .bss until an ACTIVATE fills one. */
extern BrUiHook84Ctx g_brHook84;

/* Put every singleton back WITHOUT releasing anything -- for tests and for a
 * host that re-boots the menu in one process. It frees nothing, because the
 * original frees nothing: the LEAVE routines drop the reference and that is
 * all. Same contract as BrUiHook81Reset. */
void BrUiHook84Reset(void);

/* ===========================================================================
 * ADAPTERS -- the six bodies whose stack argument the original never reads.
 *
 * Verified per body in the disassembly, NOT taken from a parameter name:
 *   0x10042EE0 0x100430B0 0x10043180 0x10044600   no [esp+4] reference at all
 *   0x100443E0 0x100446D0             two [esp+4] reads, BOTH the SEH frame
 *                                     unlink (`mov fs:[0],ecx`), not the arg
 * Every body lives in port/src/slice2_25.c and is NOT re-transcribed. This is
 * the same move slice7_80.c makes for the same module, for the same reason.
 * ========================================================================== */

/* 0x10042EE0  slice2_25.c BrOptCycleTrack. Step 0x100AC648 over the vehicle
 * range, skipping unavailable entries; publishes string id BR_OPT_STR_CAR.
 * PAIRING: BrOptFn10057C10's first conditional row, caption string 0x1B --
 * port/src/slice6_72.c:799. That row exists only when 0x1022AF18 == 2. */
int32_t BrUiHook84Opt_10042EE0(BrUiCtl_ *pCtl);

/* 0x10043180  slice2_25.c BrOptCycleAA2A00. Cycler over 0x10AA2A00, 0..4.
 * PAIRING: BrOptFn10057C10, caption string 0x1C -- slice6_72.c:810. */
int32_t BrUiHook84Opt_10043180(BrUiCtl_ *pCtl);

/* 0x100430B0  slice2_25.c BrOptCycleBD3E0. The 1-BASED cycler, 1..12.
 * PAIRING: BrOptFn10057C10, caption string 0x1D -- slice6_72.c:821. */
int32_t BrUiHook84Opt_100430B0(BrUiCtl_ *pCtl);

/* 0x10044600  slice2_25.c BrOptCycleAA2A18. Cycler over 0x10AA2A18, 0..4.
 * PAIRING: BrOptFn10057C10, caption string 0x65 -- slice6_72.c:832. */
int32_t BrUiHook84Opt_10044600(BrUiCtl_ *pCtl);

/* 0x100443E0  slice2_25.c BrOptOpen2950B. Open 0x10AA2950 in networked form.
 * PAIRING: BrOptFn100575F0, caption string 0x1E -- port/src/slice6_71.c:692.
 * That is the same control the builder then records into 0x10AA29BC. */
int32_t BrUiHook84Opt_100443E0(BrUiCtl_ *pCtl);

/* 0x100446D0  slice2_25.c BrOptOpen2954.
 * PAIRING: BrOptFn10057C10's unconditional row, caption string 0x66 when
 * 0x10AA2884 is set and 0x1E when it is not -- slice6_72.c:846. */
int32_t BrUiHook84Opt_100446D0(BrUiCtl_ *pCtl);

/* ===========================================================================
 * LEAVE -- control +0x08. Every one returns 0 (slice7_81.h CONFLICT 4).
 *
 * Eleven of the thirteen share this prologue, in this order:
 *     pCtl->pOwner->pVtbl->f1C(pCtl->pOwner);      // 0x10048AA0, release all
 *     if (current != NULL) current->pVtbl->f00(current, 1);
 * The owner is dereferenced unguarded, exactly as the original does; only the
 * current phase is NULL-tested and that test IS the original's.
 * ========================================================================== */

/* 0x10046F60 (90 B). The odd one: it clears the CURRENT phase to NULL before
 * notifying the saved one, so the notify runs with 0x10AA2904 == NULL.
 *     prologue; p = 0x10AA292C; 0x10AA2904 = NULL; 0x10AA2974 = NULL;
 *     if (p) { p->vtbl+0x00(p,1); 0x10AA292C = NULL; }
 *     0x10AA2904 = 0x10AA2908;
 * PAIRING: 0x10049F40's third control, caption string 0x11 -- the one the
 * builder records into 0x10AA29B0 -- port/src/slice6_71.c:286, :289. */
int32_t BrUiHook84_10046F60(BrUiCtl_ *pCtl);

/* 0x10046FC0 (13 B). NO prologue at all, and no argument: it is three
 * instructions, `0x10AA2904 = 0x10AA292C; return 0`. See CONFLICTS 1 and 2.
 * PAIRING: 0x10049F40's fourth control, caption string 0x12 --
 * port/src/slice6_71.c:298. */
int32_t BrUiHook84_10046FC0(BrUiCtl_ *pCtl);

/* 0x10046E10 (146 B). prologue; clears 0x10AA2924 and 0x10AA28E0, sets
 * 0x100AB3F4 to -1, copies 0x1039B720 into BOTH 0x10AA2518 and 0x10A9D618,
 * then -> 0x10AA291C.
 * GOTCHA: it does NOT clear 0x10AA2928 / 0x10AA29C0 / 0x10AA29CC / 0x10AA28E4,
 * which the fuller reset in 0x10046870 does. The two are not the same routine.
 * DEVIATION: the two copies are unbounded inline `rep movs` in the original;
 * here they are bounded by g_br73.cbScratch and NUL-terminated, as in
 * slice7_81.c and slice3_31.c.
 * PAIRING: 0x1004F700's "quit" row, caption string 0x0C -- slice6_71.c:518. */
int32_t BrUiHook84_10046E10(BrUiCtl_ *pCtl);

/* 0x10046DC0 (76 B). prologue; clears 0x10AA292C, 0x10AA29B0 and 0x10AA2974
 * -- i.e. both objects 0x10045C90 built and the control it wired -- then
 * -> 0x10AA2924.
 * PAIRING: not installed by any builder. 0x10045090 and 0x100450C0 poke it
 * into 0x10AA29B0's +0x08 at run time, which is the original's own mechanism
 * (the same one slice7_81.c documents for 0x10046D70). */
int32_t BrUiHook84_10046DC0(BrUiCtl_ *pCtl);

/* 0x10046710 (56 B). prologue; clears 0x10AA2988; -> 0x10AA2918.
 * PAIRING: 0x1004E830's "back" row, caption string 0x0C -- the control that
 * builder then records into 0x10AA29C8 -- slice6_72.c:1495, :1498. */
int32_t BrUiHook84_10046710(BrUiCtl_ *pCtl);

/* 0x10047060 (56 B). prologue; clears 0x10AA2930; -> 0x10AA292C.
 * PAIRING: 0x10052030's "back" row, caption string 0x0C -- slice6_72.c:990. */
int32_t BrUiHook84_10047060(BrUiCtl_ *pCtl);

/* 0x10046830 (56 B). prologue; clears 0x10AA2918; -> 0x10AA2930.
 * PAIRING: poked into 0x10AA29C8's +0x08 by 0x100457C0. */
int32_t BrUiHook84_10046830(BrUiCtl_ *pCtl);

/* 0x10046870 (158 B). The full name reset -- clears 0x10AA2928, 0x10AA29C0,
 * 0x10AA29CC and 0x10AA28E4, sets 0x100AB3F4 to -1, copies 0x1039B720 into
 * both scratch buffers -- then -> 0x10AA2930.
 * GOTCHA: the destination is read AFTER the clears and the copies, where the
 * plain leaves read theirs first. No routine clears the word it is about to
 * read, so the two orders agree; reproduced anyway.
 * PAIRING: poked into 0x10AA29F4's +0x08 by 0x100457E0. */
int32_t BrUiHook84_10046870(BrUiCtl_ *pCtl);

/* 0x10043FA0 (30 B). The only hook here that drives phase vtable +0x18 rather
 * than +0x1C, and it does not notify the current phase at all:
 *     pCtl->pOwner->pVtbl->f18(pOwner, 1);   // see CONFLICT 4
 *     0x10AA2904 = 0x10AA2908;
 * PAIRING: 0x10052030's SECOND rectangle control, place flags 0x402001 and
 * step id 0x52/0x53 -- port/src/slice6_72.c:1023. */
int32_t BrUiHook84_10043FA0(BrUiCtl_ *pCtl);

/* 0x10043F50 (66 B). Sets 0x10AA287C to 2 BEFORE the prologue, then clears
 * 0x10AA298C and -> 0x10AA2948.
 * PAIRING: 0x10056A10's row with caption string 0x1E -- the control that
 * builder records into 0x10AA29E8 -- slice6_72.c:737, :740. */
int32_t BrUiHook84_10043F50(BrUiCtl_ *pCtl);

/* 0x10044B40 (66 B). prologue; clears 0x10AA298C and 0x10AA29E8 (a CONTROL);
 * -> 0x10AA2940.
 * PAIRING: 0x10056A10's "back" row, caption string 0x0C -- slice6_72.c:749. */
int32_t BrUiHook84_10044B40(BrUiCtl_ *pCtl);

/* 0x10044C70 (56 B). prologue; clears 0x10AA295C; -> 0x10AA2908, the ROOT
 * phase, which is what makes this one a "back to the top" rather than a step.
 * PAIRING: 0x10059760's "back" row, caption string 0x0C -- slice6_72.c:1226. */
int32_t BrUiHook84_10044C70(BrUiCtl_ *pCtl);

/* 0x10044F00 (66 B). GOTCHA, and it is the only one in the family: the object
 * it notifies through +0x00 is 0x10AA2968, NOT the current phase. It then
 * clears 0x10AA2968, points 0x10AA2904 at 0x10AA295C and sets 0x100AA010 to 2.
 * PAIRING: 0x1005A6E0's row with caption string 0x1E -- slice6_72.c:1323. */
int32_t BrUiHook84_10044F00(BrUiCtl_ *pCtl);

/* ===========================================================================
 * INSTALLERS -- control +0x08. Run an ACTIVATE, then poke a LEAVE routine
 * into some OTHER control's +0x08. All return 1 except 0x100471B0.
 * ========================================================================== */

/* 0x10045090 (42 B).
 *     0x10045C90(pCtl);                       // argument pushed, never read
 *     0x10AA29B0->pfn08 = 0x10046DC0;
 *     0x100AA010 = 0;
 *     return 1;
 * GOTCHA: 0x10AA29B0 is dereferenced with no NULL test even though the
 * activate can fail. DEVIATION: guarded here, as slice7_81.c guards its twin.
 * PAIRING: 0x1004F700's caption-0x1E row -- port/src/slice6_71.c:475. */
int32_t BrUiHook84_10045090(BrUiCtl_ *pCtl);

/* 0x100450C0 (47 B). 0x10045090 with a leading bare call to 0x10041BD0.
 * 0x10041BD0 is NOT PORTED; the host harness satisfies it from
 * port/host/br_stubs.c, so the call is kept and does nothing today.
 * PAIRING: 0x1004F700's autosave row, caption string 0x35, whose place flags
 * are 0x102001 or 0x102011 depending on whether AutoSave.brf opens --
 * port/src/slice6_71.c:502. */
int32_t BrUiHook84_100450C0(BrUiCtl_ *pCtl);

/* 0x100457C0 (32 B). 0x100451E0(pCtl); 0x10AA29C8->pfn08 = 0x10046830.
 * The twin of slice7_81.c's BrUiHook81_10045880, which installs 0x10046AD0
 * into the same slot from the same activate.
 * PAIRING: 0x10052030's THIRD rectangle control, place flags 0x402001 and
 * step id 0x54/0x55 -- port/src/slice6_72.c:1041. */
int32_t BrUiHook84_100457C0(BrUiCtl_ *pCtl);

/* 0x100457E0 (32 B). 0x10045BC0(pCtl); 0x10AA29F4->pfn08 = 0x10046870.
 * The twin of slice7_81.c's BrUiHook81_100458A0, and slice7_81.h already
 * names this address as that hook's neighbour.
 * PAIRING: 0x10052030's FIRST rectangle control, place flags 0x402001 and
 * step id 0x78/0x79 -- port/src/slice6_72.c:1004. */
int32_t BrUiHook84_100457E0(BrUiCtl_ *pCtl);

/* 0x100471B0 (55 B). The order is the reverse of every other installer: the
 * ACTIVATE runs FIRST and the owner release second.
 *     0x10045C90(pCtl);
 *     pCtl->pOwner->pVtbl->f1C(pOwner);
 *     if (0x10AA2970) 0x10AA2970->vtbl+0x00(it, 1);
 *     0x10AA2970 = NULL;
 *     return 0;                               // 0, unlike its neighbours
 * PAIRING: 0x10051D30's one selectable control -- the step-table one with no
 * f34 call at all -- port/src/slice6_71.c:361. */
int32_t BrUiHook84_100471B0(BrUiCtl_ *pCtl);

/* ===========================================================================
 * MISC
 * ========================================================================== */

/* 0x10047340 (32 B). `rep stosd` of EIGHT dwords -- 32 bytes, and of ZERO,
 * not -1 -- at 0x10A9D618, then 0x10AA28A4 = 0 and the BYTE 0x10AA26F5 = 0.
 * Returns 1.
 * DEVIATION: the clear is bounded by g_br73.cbScratch. The original's 32 is a
 * literal and the buffer's true size is not established here; slice3_31.c
 * made the same choice and its banner says so.
 * PAIRING: 0x10052030's first selectable row, caption string 0x45 --
 * port/src/slice6_72.c:965. */
int32_t BrUiHook84_10047340(BrUiCtl_ *pCtl);

/* ===========================================================================
 * INSTALLATION
 *
 * Each fills only the slots listed above and TOUCHES NOTHING ELSE -- an
 * unwired slot must stay a visible hole, so every remaining entry is left
 * exactly as the caller had it. In particular Install72 does not touch
 * p10043590 / p100435F0 / p10043650 / p100436B0, which are slice7_80.c's
 * (BrUiOptInstall72), so the two installers can be called in either order.
 * ========================================================================== */
void BrUiHook84Install71(BrS71Hooks  *pHooks);
void BrUiHook84Install72(BrUi72Hooks *pHooks);

#endif /* SLICE8_84_H */
