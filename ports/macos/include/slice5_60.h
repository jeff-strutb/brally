/* slice5_60.h -- Boss Rally (BRD3D.dll) decompilation, a later pass, slice 5.
 *
 * A "close the link" packet: fifteen individually-requested addresses that an
 * already-ported module calls through an `extern` it declared itself.  Every
 * name and signature below was grepped out of port/include/ BEFORE a line was
 * written; none of them is chosen here.
 *
 * ======================================================================
 * IN THIS FILE (6 of the 15 wanted)
 * ======================================================================
 *   0x100243D0  BrGbiCall100243D0   slice2_16.h:98    BrGfxWords *(BrGfxWords *)
 *   0x10020FA0  BrGbiCall10020FA0   slice2_16.h:102   void (uint32_t)
 *   0x1002BF80  BrSub1002BF80       slice2_19.h:682   void (uint32_t)
 *                 + BrDlRegister    slice2_20.h:47    void (void *)   -- SAME
 *                   address under a second, pre-existing name; see CONFLICTS.
 *   0x100341B3  BrSub100341B3       slice2_19.h:661   int (uint32_t *, const void *)
 *   0x100603A0  BrSub100603A0       slice4_52.h:138   void (void *, void *)
 *   0x10071130  BrSub10071130       slice2_25.h:460   void (int, int)
 *
 * ======================================================================
 * NOT IN THIS FILE, AND WHY   (the full argument is in slice5_60.c's tail)
 * ======================================================================
 *   0x10044970  BrOptFn10044970  -- ALREADY IMPLEMENTED.  slice2_26.c has this
 *                exact body as `BrPhaseLeave_10044970(BrPhaseCtx *, void *)`.
 *                The contract forbids duplicating it.  What is missing is an
 *                adapter, not a decompilation.
 *   0x1003C020  BrSub1003C020 / BrExt_1003C020 -- Win32 + DirectPlay
 *                (KillTimer, SetTimer, CreateEventA, IDirectPlay4 slot +0x98).
 *                Not portable; and ONE ADDRESS, TWO WANTED NAMES -- see
 *                CONFLICTS.
 *   0x10004E50  BrNetSendDelta -- WaitForMultipleObjects / ReleaseMutex over a
 *                per-player 0x978-stride ring, plus a message-builder class
 *                (ctor 0x10073B40, append 0x10073D60, encode 0x10006830, send
 *                0x10004DD0) that nothing in the port models.
 *   0x10056FF0  BrOptFn10056FF0   } the five menu-screen constructors.  See
 *   0x10049F40  BrExt_10049F40    } "THE FIVE SCREEN BUILDERS" below: the
 *   0x1004F700  BrExt_1004F700    } blocker is the DECLARED PARAMETER TYPE,
 *   0x10053CF0  BrExt_10053CF0    } not the bodies.
 *   0x1004D640  BrExt_1004D640    }
 *
 * ======================================================================
 * THE FIVE SCREEN BUILDERS -- a structural blocker integration must fix
 * ======================================================================
 * All five have the shape slice3_33.h already decompiled five times over:
 *
 *     p = operator new(0x1E214); p = p ? BrUiCtlCtor(p) : NULL;
 *     screen->apCtl[screen->cCtl] = p;
 *     p->pVtbl->f38(p, phase, x, y, flags, 2, 5, a6, a7);
 *     p->pVtbl->f34(p, BrStrGet(id), a2, a3, style);
 *     screen->cCtl++;
 *
 * The object layouts are NOT the problem -- slice3_33.h (BrUiPhase/BrUiScreen/
 * BrUiCtl) and slice3_32.h (BrUiPage/BrUiObj) both model them.  The problem is
 * that the WANTED signature is
 *
 *     void BrExt_10049F40(BrPhase *pSelf);          (slice3_31.h:244)
 *     void BrExt_1004D640(BrPhase *pSelf);          (slice2_26.h:289)
 *     void BrOptFn10056FF0(BrOptObj *pThis);        (slice2_25.h:469)
 *
 * and NONE of `BrPhase` (slice2_26.h) or `BrOptObj` (slice2_25.h) has the
 * fields these bodies touch.  Both model the same 0xC8-byte object as
 * {pVtbl, pfn04, pfn08, f0C, f68}; the builders need +0x10 (uint16 screen
 * count), +0x14 (screen-pointer array) and +0x6C (a parallel int array),
 * which is exactly slice3_33.h's BrUiPhase and is a DIFFERENT layout.
 * slice3_33.h says so itself ("CONFLICT TO RESOLVE AT INTEGRATION") and
 * declares its own five as `void BrExt_1004A580(BrUiBuildCtx *, BrUiPhase *)`,
 * i.e. it did not close these links either.  slice3_32.c, slice4_51.c and
 * slice5_61.h all declined the same family independently.
 *
 * Writing `void BrExt_10049F40(BrPhase *p)` and casting p to BrUiPhase * would
 * compile and link and be wrong at every field access, silently.  That is the
 * "wrong-but-plausible" outcome the contract rules out, so these five are left
 * out.  ONE merged phase layout unblocks all five at once.
 *
 * ======================================================================
 * SIGNATURE / NAME CONFLICTS FOUND (reported, deliberately not "resolved")
 * ======================================================================
 * 1. 0x1003C020 carries TWO wanted names in this one packet:
 *      BrSub1003C020 (slice2_25.h:425, slice4_50, slice4_53) and
 *      BrExt_1003C020 (slice2_26.h:252).  Same `void (void)` shape.
 *    Both blocks in work/slice5/agent60.asm are byte-identical, so this is a
 *    naming duplicate, not a mispairing.
 *
 * 2. 0x1002BF80 carries TWO pre-existing names with INCOMPATIBLE shapes:
 *      slice2_19.h:682  void BrSub1002BF80(uint32_t v)      <- the wanted one
 *      slice2_20.h:47   void BrDlRegister(void *pv)
 *    The body dereferences the argument, so only the `void *` form can be
 *    implemented directly.  Both are provided here: BrDlRegister is the body,
 *    BrSub1002BF80 is the u32-address entry that resolves through
 *    slice2_19.h's own g_BrModelDeref hook (which is what its one caller,
 *    slice2_19.c:894, has just used on the very same word).
 *
 * 3. 0x104C16A0 and 0x106C6618 are modelled as fields of slice5_62.h's
 *    `BrRasterSel`, written this same round.  They are plain externs here.
 *    One of the two must win; they must not both own storage.
 *
 * 4. 0x10277370 / 0x10277378.. / 0x102773F8.. (the deferred render-state
 *    block) now has THREE models: slice4_51.h's BrGbiRectState (used here,
 *    because it is the one already integrated), slice5_62.h's BrRasterSel,
 *    and slice2_16.h's BrGbiState which deliberately excludes it.
 *
 * ======================================================================
 * GLOBALS THIS FILE DEFINES that another header already models as a FIELD
 * ======================================================================
 * These have no standalone owner today, so storage is defined in slice5_60.c.
 * Where some other header models the same address inside a struct, it is
 * named here so integration can alias rather than duplicate:
 *
 *   0x100AB3DC  g_Br0AB3DC   = slice3_32.h  BrScrCtx.w0AB3DC
 *   0x10AA286C  g_BrAA286C   = slice3_32.h  BrScrCtx.wAA286C
 *   0x10AA2844  g_BrAA2844   = slice2_23.h/slice2_24.h  gAA2844
 *   0x10AA2DAC  g_BrAA2DAC   = br_state.h   BrActiveFlags.a7
 *   0x10AA2DB4  g_BrAA2DB4   = br_state.h   BrActiveFlags.a8
 *   0x104C16A0  g_Br4C16A0   = slice5_62.h  BrRasterSel.f4C16A0
 *   0x106C6618  g_Br6C6618   = slice5_62.h  BrRasterSel.f6C6618
 *
 * 0x106C661C, 0x106C6624 and 0x106C666C are NOT redefined -- slice2_19.c owns
 * them and slice2_19.h is included below.
 */
#ifndef SLICE5_60_H
#define SLICE5_60_H

#include <stddef.h>
#include <stdint.h>

#include "br_seg.h"      /* BrSegMap, BrSegFixup (0x1002B970)               */
#include "slice1_05.h"   /* BrGfxWords, BrPtrList, BrF3D* (0x1002C150..)    */
#include "slice2_16.h"   /* BrGbiCall100243D0 / BrGbiCall10020FA0 (wanted)  */
#include "slice2_19.h"   /* BrSub100341B3 / BrSub1002BF80 (wanted),
                          * g_BrSegMap, g_BrModelDeref, g_Br6C661C,
                          * g_Br6C6624, g_Br6C666C                          */
#include "slice3_39.h"   /* g_BrDikState, g_BrBtnRaw, g_BrAA3398,
                          * g_BrAA33B4/B8, BrMenuSub1005FFF0 (0x1005FFF0)   */
#include "slice4_51.h"   /* BrGbiRectState, BrGbiRectGetState               */
#include "slice4_52.h"   /* BrSub100603A0 (wanted)                          */

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * 1. 0x10020FA0 -- the deferred-render-state selector
 * ====================================================================== */

/* Five globals 0x10020FA0 touches that live outside BrGbiRectState. */

/* 0x100A79D8.  READ ONLY here: it is the pending value index 3 receives on
 * the `w1 == 4` path, where every other path uses the literal 6.  Whatever
 * writes it is outside this packet. */
extern uint32_t g_Br0A79D8;

/* 0x100AA720.  A gate: when it is ZERO the (w1 & 0x1800) && (w1 & 0x10000)
 * path RETURNS WITHOUT WRITING `dirty` -- see the GOTCHAs. */
extern int32_t g_Br0AA720;

/* 0x104BBE28.  Shadows aPending[7] (D3DRENDERSTATE_ALPHAFUNC); written with
 * the same value on every path that writes index 7, never read here. */
extern uint32_t g_Br4BBE28;

/* 0x104C16A0.  Shadows aPending[5] (D3DRENDERSTATE_ZFUNC), same deal.
 * ALSO MODELLED as slice5_62.h's BrRasterSel.f4C16A0 -- see CONFLICTS. */
extern uint32_t g_Br4C16A0;

/* 0x104C5184.  Selects the short form of three of the eight arms (it skips
 * indices 2 and 3), and is WRITTEN by two arms.  Not the same address as
 * slice4_51.h's cVertices (0x104C5188) or cIndices (0x104C518C). */
extern int32_t g_Br4C5184;

/* ======================================================================
 * 2. 0x100341B3 -- the display-list rendermode substituter
 * ====================================================================== */

/* 0x106C6618.  Selects WHICH of a record's four (w0,w1) pairs is substituted
 * in: the column index is `g_Br6C6618 + 1` or `+ 2`.  With the shipped table
 * (six 32-byte records = four pairs each) only 0 and 1 are legal values.
 * ALSO MODELLED as slice5_62.h's BrRasterSel.f6C6618 -- see CONFLICTS. */
extern int32_t g_Br6C6618;

/* 0x106C6620.  Arms both colour-substitution arms (G_SETPRIMCOLOR /
 * G_SETENVCOLOR); tested a SECOND time inside each of them. */
extern int32_t g_Br6C6620;

/* The two .rdata tables 0x100341B3 reaches by absolute address.  Values read
 * out of orig/BRD3D.dll with tools/pe.py, not assumed. */

/* 0x100AA8B8: ONE 16-byte record {matchW0, matchW1, newW0, newW1}.  The loop
 * bound really is 1 in the original. */
#define BR_DLSUB_AA8B8_COUNT 1
typedef struct BrDlSubst4 {
    uint32_t matchW0, matchW1, newW0, newW1;
} BrDlSubst4;
extern const BrDlSubst4 g_aBrAA8B8[BR_DLSUB_AA8B8_COUNT];

/* 0x100AA8C8: TWO 8-byte records {w0, w1}, matched but never substituted --
 * a hit only sets the local flag the two colour arms read. */
#define BR_DLSUB_AA8C8_COUNT 2
extern const BrGfxWords g_aBrAA8C8[BR_DLSUB_AA8C8_COUNT];

/* The two literals the colour arms force into w1. */
#define BR_DL_PRIMCOLOR_FORCED 0x60789000u   /* G_SETPRIMCOLOR (0xFA) */
#define BR_DL_ENVCOLOR_FORCED  0x8C9CA800u   /* G_SETENVCOLOR  (0xFB) */

/* ======================================================================
 * 3. 0x1002BF80 -- byte-swap and register a display list
 * ====================================================================== */

/* 0x1067B548 / 0x1067B550 -- the flat pointer list 0x1002C1F0 appends to.
 * slice1_05.h owns the TYPE and the append routine but no instance, and
 * slice2_17.h reaches the same storage through a state struct.  A pointer, so
 * that whichever module ends up owning the storage can be aliased in without
 * a second copy.
 *
 * DEVIATION (memory safety): when it is NULL the append is SKIPPED rather
 * than crashing.  The original has no such guard -- the list is a fixed
 * global there. */
extern BrPtrList *g_pBrDlPtrList;

/* 0x1002BF80's other, pre-existing name (slice2_20.h).  This is the real
 * body; see CONFLICTS #2.  Walks 8-byte commands from *pv, byte-reversing
 * BOTH words of every command in place, until G_ENDDL. */
void BrDlRegister(void *pv);

/* ======================================================================
 * 4. 0x100603A0 -- poll the mouse and fold it into the menu cursor
 * ====================================================================== */

/* The DirectInput device at mouse +0x50.  Slot +0x1C is called with `this`
 * alone and slot +0x24 with (this, cbData, pvData) where cbData is 0x10; the
 * error it retries on is 0x8007001E.  That triple is IDirectInputDeviceA's
 * Acquire / GetDeviceState and DIERR_INPUTLOST, and matches no other DirectX
 * interface -- which is what fixes the identification.
 *
 * Named BrDInputDev*, never a generic COM name (contract). */
typedef struct BrDInputDev BrDInputDev;

typedef struct BrDInputDevVtbl {
    void *aSlots00[7];                                   /* +0x00..+0x18 */
    int32_t (*Acquire)(BrDInputDev *pThis);              /* +0x1C */
    void *aSlots20[1];                                   /* +0x20 Unacquire */
    int32_t (*GetDeviceState)(BrDInputDev *pThis,
                              uint32_t cbData,
                              void *pvData);             /* +0x24 */
} BrDInputDevVtbl;

struct BrDInputDev {
    const BrDInputDevVtbl *pVtbl;   /* +0x00 */
};

#define BR_DIERR_INPUTLOST 0x8007001Eu

/* The 0x10-byte block GetDeviceState fills: three relative axes and four
 * button bytes, i.e. DIMOUSESTATE. */
typedef struct BrMouseSample {
    int32_t dx;          /* +0x00 */
    int32_t dy;          /* +0x04 */
    int32_t dz;          /* +0x08 */
    uint8_t aBtn[4];     /* +0x0C..+0x0F, bit 7 = down */
} BrMouseSample;

#define BR_MOUSE_BTN_COUNT 4
#define BR_MOUSE_SAMPLE_SIZE 0x10u

/* The object 0x100603A0 is a __thiscall method of.
 *
 * SIGNATURE NOTE: slice4_52.c reaches it as `BrSub100603A0((void *)
 * g_pBrAA2E80, g_brP680584)` and slice3_39.h types 0x10AA2E80 as
 * `BrPointI *` -- only {x, y}.  That is an UNDER-model of the same object:
 * this function reads and writes it out to +0x50.  BrPointI's two ints are
 * this struct's first two, so the prefix agrees; nothing else does.
 *
 * Every field before pDev is a 32-bit scalar, so the original byte offsets
 * hold on a 64-bit host too (0x50 is already 8-aligned).  Checked below. */
typedef struct BrMouseState {
    int32_t  x;                          /* +0x00  clamped to g_BrAA33B8   */
    int32_t  y;                          /* +0x04  clamped to g_BrAA33B4   */
    int32_t  z;                          /* +0x08  NOT clamped             */
    int32_t  xPrev;                      /* +0x0C                          */
    int32_t  yPrev;                      /* +0x10                          */
    uint8_t  pad14[0x24 - 0x14];         /* +0x14..+0x23 untouched here    */
    uint8_t  aBtn[BR_MOUSE_BTN_COUNT];   /* +0x24..+0x27  raw & 0x80       */
    uint8_t  aBtnPrev[BR_MOUSE_BTN_COUNT]; /* +0x28..+0x2B                 */
    int32_t  aDown[BR_MOUSE_BTN_COUNT];  /* +0x2C,+0x30,+0x34,+0x38        */
    int32_t  aRelease[BR_MOUSE_BTN_COUNT]; /* +0x3C,+0x40,+0x44,+0x48      */
    int32_t  f4C;                        /* +0x4C  "a button is held"      */
    BrDInputDev *pDev;                   /* +0x50                          */
} BrMouseState;

typedef char BrMouseStateOffsetCheck[
    (offsetof(BrMouseState, aBtn)     == 0x24 &&
     offsetof(BrMouseState, aBtnPrev) == 0x28 &&
     offsetof(BrMouseState, aDown)    == 0x2C &&
     offsetof(BrMouseState, aRelease) == 0x3C &&
     offsetof(BrMouseState, f4C)      == 0x4C &&
     offsetof(BrMouseState, pDev)     == 0x50) ? 1 : -1];

/* Indices into slice3_39.h's g_BrAA3398[7] (0x10AA3398..0x10AA33B0).  Named
 * for what this function does with them; the array itself stays slice3_39's. */
#define BR_CURSOR_UP_LATCH    0   /* 0x10AA3398 */
#define BR_CURSOR_DOWN_LATCH  1   /* 0x10AA339C */
#define BR_CURSOR_UP_REQ      2   /* 0x10AA33A0 */
#define BR_CURSOR_DOWN_REQ    3   /* 0x10AA33A4 */
#define BR_CURSOR_ARMED       4   /* 0x10AA33A8 */
#define BR_CURSOR_LAST_MS     5   /* 0x10AA33AC */
#define BR_CURSOR_HELD_MS     6   /* 0x10AA33B0 */

/* The dwell the held time must EXCEED (`jle`, so 120 itself does not arm). */
#define BR_CURSOR_ARM_MS 0x78

/* DIK scancodes, as offsets into slice3_39.h's g_BrDikState[256]. */
#define BR_DIK_UP   0xC8
#define BR_DIK_DOWN 0xD0

/* 0x10AA2844 -- master gate: non-zero and 0x100603A0 does nothing at all. */
extern int32_t g_BrAA2844;
/* 0x10AA2BDC / 0x10AA2BE0 -- when set, step the CD track down / up. */
extern int32_t g_BrAA2BDC;
extern int32_t g_BrAA2BE0;
/* 0x10AA2DAC / 0x10AA2DB4 -- "menu up" / "menu down" requests from elsewhere.
 * br_state.h models the same two as BrActiveFlags.a7 / .a8. */
extern int32_t g_BrAA2DAC;
extern int32_t g_BrAA2DB4;
/* 0x100AB3DC -- a 16-BIT step, written -1 or +1.  slice3_32.h: w0AB3DC. */
extern uint16_t g_Br0AB3DC;
/* 0x10AA286C -- a 16-BIT selection cursor, dec'd / inc'd.  slice3_32.h:
 * wAA286C. */
extern uint16_t g_BrAA286C;
/* 0x10AA33E8 -- the timestamp of the last change to x/y or the buttons. */
extern int32_t g_BrAA33E8;
/* 0x10AA2A78 -- set to `this` immediately before every 0x1005FFF0 call. */
extern void *g_pBrAA2A78;

/* ======================================================================
 * 5. 0x10071130 -- the config-file reader
 * ====================================================================== */

/* 0x11782E28.  The scratch buffer every read lands in.  0x80 bytes is what
 * mode 2 reads (twice) and 0x100 what mode 3 reads; 0x100 is the size used.
 *
 * DEVIATION (memory safety): the original's default arm reads `size` bytes
 * into this buffer with `size` taken from the caller and never bounded.  The
 * port clamps to the buffer size and reports the clamp -- see the .c. */
#define BR_CFG_BUF_SIZE 0x100
extern uint8_t g_aBr1782E28[BR_CFG_BUF_SIZE];

/* 0x100ADF58 / 0x100ADF5C / 0x100ADF60 -- the first three dwords of mode 2's
 * FIRST record, latched before the second record is read. */
extern uint32_t g_Br0ADF58;
extern uint32_t g_Br0ADF5C;
extern uint32_t g_Br0ADF60;

/* 0x100B5DD0.  Hard-coded, absolute, and with a drive letter. */
#define BR_CFG_PATH "c:\\RallyConfig.dat"
/* 0x10094110 -- the fopen mode, binary. */
#define BR_CFG_MODE "rb"
/* The fixed record sizes: mode 2 reads 0x80 twice, mode 3 reads 0x100 once. */
#define BR_CFG_REC_SIZE  0x80
#define BR_CFG_FULL_SIZE 0x100

/* 0x10690A18 -- the index into the 0x2B68-stride record array.  slice2_25.c
 * owns the storage; declared, not defined, here (same name and type). */
extern int32_t g_br690A18;

/* DEVIATION: mode 2 finishes with
 *
 *     pDst = *(void **)(0x10ACED34 + 0x2B68 * g_br690A18);
 *     *(uint32_t *)(pDst + 0xF8 + 4*k) = rec[k];   k = 0..4
 *
 * i.e. it follows the FIRST dword of entity record `g_br690A18` (stride
 * 0x2B68 -- see CONTRACT) and writes five dwords deep inside whatever that
 * points at.  Neither the array nor the target is modelled anywhere in the
 * port, and 0x10ACED34 is described three different ways in three headers
 * (slice2_24.h, slice4_52.h, slice3_31.h).  Rather than invent a fourth, the
 * port asks for the destination through this hook.  When it is NULL the five
 * stores are skipped and everything else still happens. */
extern void *(*g_BrCarEquipTarget)(int32_t index);

/* Byte offset of the first of the five dwords inside that object. */
#define BR_CAR_EQUIP_OFF   0xF8
#define BR_CAR_EQUIP_COUNT 5

/* ======================================================================
 * Cross-slice callees
 * ====================================================================== */

/* XSLICE 0x10002930 / 0x10002970 -- CD track down / up.  Declarations copied
 * verbatim from slice2_11.h (implemented in slice2_11.c). */
extern int BrCdTrackPrev(void);
extern int BrCdTrackNext(void);

/* XSLICE 0x10075020 -- the millisecond clock.  Declaration copied verbatim
 * from slice3_32.h / slice4_50.h (implemented in slice4_50.c). */
extern int32_t BrSub10075020(void);

/* XSLICE 0x10008B80 -- a bare `ret` in this build (see CONTRACT).  Name and
 * shape copied verbatim from slice2_18.h so the two headers can coexist. */
extern void BrStub8B80_1p(const void *p0);

/* XSLICE 0x10070610 -- 0x10071130's modes 0 and 4 are pure forwards to it,
 * passing the mode through as its FIRST argument.  Its return value becomes
 * 0x10071130's, which the wanted `void` signature discards. */
extern int32_t BrSub10070610(int32_t mode, int32_t arg);

/* XSLICE 0x10070E60 -- mode 1's forward.  Same note about the return. */
extern int32_t BrSub10070E60(int32_t arg);

/* ======================================================================
 * The packet
 * ====================================================================== */

/* 0x100243D0
 *
 * Eight bytes: `mov eax,[esp+4] / add eax,8 / ret`.  Reached from
 * 0x10020F50 when the command's selector byte is 0, and its job is only to
 * skip the command -- so it is the identity handler.  `+ 8` is one 8-byte
 * command, and slice2_16.c's other handlers all `return pCmd + 1`. */
BrGfxWords *BrGbiCall100243D0(BrGfxWords *pCmd);

/* 0x10020FA0
 *
 * Translate one F3D rendermode word into the deferred Direct3D render states
 * slice4_51.h's BrGbiCall10021560 later flushes.  Every write goes through
 * one primitive, applied 0 to 6 times depending on which of eight arms `w1`
 * selects:
 *
 *     aPending[i] = v;
 *     if (aShadow[i] == v) dirty &= ~(1u << i); else dirty |= (1u << i);
 *
 * The index-to-render-state map is slice4_51.h's BrGbiRectRenderState[], and
 * the values it produces are recognisable D3D ones -- index 2 (SRCBLEND) gets
 * 5 = D3DBLEND_SRCALPHA and index 3 (DESTBLEND) gets 6 = D3DBLEND_INVSRCALPHA
 * on the ordinary arms, index 7 (ALPHAFUNC) gets 7 = D3DCMP_GREATEREQUAL --
 * which is independent confirmation of that ordering.
 *
 * GOTCHA: the dirty bit is CLEARED when the new pending value already equals
 * the shadow.  It is not a sticky "something changed" flag.
 *
 * GOTCHA: aPending[0] is set to 1 by the PROLOGUE, before `w1` is even
 * looked at, and four of the arms then set it back to 0.
 *
 * GOTCHA (two silent early exits that leave `dirty` STALE):
 *   - `w1 == 3` sets 0x104C5184 = 0 and returns.  aPending[0] has already
 *     been written to 1 and the recomputed dirty word is DISCARDED.
 *   - the (w1 & 0x1800) && (w1 & 0x10000) arm returns the same way when
 *     0x100AA720 is zero, after the same prologue write.
 *   Everything else assigns `dirty` on the way out.
 *
 * GOTCHA: indices 4 (CULLMODE), 8 (TEXTUREMAG) and 9 (TEXTUREMIN) are never
 * touched by this function on any path.
 *
 * GOTCHA: the two `0x1800`-family arms are NOT a plain else-branch of the
 * literal comparisons -- they are reached only after eight exact-value tests
 * fail, so e.g. w1 == 0x00504240 never reaches the bit tests even though it
 * has bit 0x0800 set. */
void BrGbiCall10020FA0(uint32_t w1);

/* 0x100341B3
 *
 * Walk an 8-byte-command display list and substitute rendermode / colour
 * commands, returning 1 if any "high" substitution was made.
 *
 * Its one caller is slice2_19.c's BrDlOwnerFixup (0x1003445A), which passes
 * the six-record table at 0x100AA8D8 and ORs 8 into the owner's flags on a
 * non-zero return.
 *
 * slice2_19.c:977 recorded this function as skipped because its packet began
 * 47 bytes in and "the prologue would tell us how [ebp-0x18] ... and the
 * return slot [ebp-0x14] are initialised".  This packet supplied exactly that
 * prologue.  The answers are:
 *
 *     ret  = 0
 *     sel  = (g_Br6C661C == 0 && g_Br6C6624 == 0) ? 1 : 0
 *     col  = g_Br6C6618 + (sel ? 1 : 2)
 *
 * CORRECTION to that note: the loop does NOT stop at "a null command
 * pointer".  pDl is tested for NULL exactly once, before the loop; the only
 * way out afterwards is opcode 0xB8 (G_ENDDL).  A list without one runs off
 * the end.
 *
 * Dispatch (index = op - 0xB8, valid 0..0x44; the byte table at 0x10034415
 * and the six-entry jump table at 0x100343FD were read out of the DLL):
 *
 *   0xB8 G_ENDDL        -> stop
 *   0xB9 G_SETOTHERMODE -> match (w0,w1) against pTable's six 32-byte
 *                          records; on a hit replace the command with the
 *                          record's pair number `col`, and if the record
 *                          index is >= 3 set the return value to 1
 *   0xFC G_SETCOMBINE   -> when `sel`, substitute from g_aBrAA8B8; then, when
 *                          g_Br6C6620 && g_Br6C666C, set a local flag iff the
 *                          command is one of g_aBrAA8C8's two
 *   0xFA G_SETPRIMCOLOR -> if that flag && g_Br6C6620, force w1
 *   0xFB G_SETENVCOLOR  -> if that flag && g_Br6C6620, force w1
 *   anything else       -> skip
 *
 * GOTCHA: g_Br6C666C is cleared on EVERY exit, including the pDl == NULL one.
 * GOTCHA: `col` indexes 8-byte pairs inside a 32-byte record, so only
 *   g_Br6C6618 in {0, 1} is in bounds.  There is no check.
 * GOTCHA: the flag the two colour arms read is set by the G_SETCOMBINE arm,
 *   so ordering inside the list decides the result; and it is RESET to 0 by
 *   a later G_SETCOMBINE that misses. */
int BrSub100341B3(uint32_t *pDl, const void *pTable);

/* 0x1002BF80
 *
 * The u32-address entry point (slice2_19.h's shape).  Resolves `v` through
 * slice2_19.h's g_BrModelDeref -- the same hook its only caller has just
 * applied to the same word -- and hands the result to BrDlRegister. */
void BrSub1002BF80(uint32_t v);

/* 0x100603A0
 *
 * __thiscall(this, pArg), `ret 4`.  pArg is accepted and NEVER READ; the
 * original loads nothing from it.  slice4_52.c passes g_brP680584.
 *
 * Poll the mouse, accumulate it into the cursor, derive per-button
 * down/release edges, and fold the keyboard's up/down arrows and two
 * externally-set request flags into the menu selection.  Ends by publishing `this` at
 * 0x10AA2A78 and running 0x1005FFF0 (the four-entry button edge pass).
 *
 * GOTCHA: `z` is accumulated but, unlike x and y, never clamped.
 * GOTCHA: the clamp is asymmetric -- `< 0` becomes 0, but the upper test is
 *   `>= limit` becomes limit, so `limit` itself is reachable.
 * GOTCHA: the DIERR_INPUTLOST retry calls Acquire TWICE per attempt (once to
 *   test the result, once more before re-reading) and loops forever while the
 *   error persists and Acquire keeps succeeding.
 * GOTCHA: on the button-press edge (aBtn[i] set, aDown[i] was 0) aDown[i] is
 *   set and aRelease[i] is LEFT ALONE -- so a release flag survives one extra
 *   frame.  Every other path writes aRelease[i].
 * GOTCHA: f4C is set to 1 when any button is down and cleared by the release
 *   edges and by the up/down requests -- three writers, no reader here.
 * GOTCHA: 0x100AB3DC is 16-bit and receives -1 (0xFFFF) for "up".
 * GOTCHA: 0x10AA286C is 16-bit and is dec'd/inc'd with no bound. */
void BrSub100603A0(void *pThis, void *pArg);

/* 0x10071130
 *
 * A five-way config-file switch.  Modes 0 and 4 forward to 0x10070610 with
 * the mode as its first argument, mode 1 forwards to 0x10070E60, modes 2 and
 * 3 read BR_CFG_PATH, and anything else uses the SECOND argument as both the
 * path and the byte count (see the DEVIATION in the .c -- that arm cannot be
 * expressed with the declared `int` parameter and is almost certainly a bug).
 *
 * GOTCHA: on fopen failure the original returns `(arg & 0xFF) != 0` -- the
 * LOW BYTE of the second argument, not the whole word.  The wanted signature
 * is `void`, so every return value here is discarded; they are computed and
 * commented anyway so the information is not lost.
 * GOTCHA: mode 3 opens the same file as mode 2 but reads 0x100 bytes in one
 * go and does nothing with them.
 * GOTCHA: the two 0x10008B80 calls are the log messages "Loading car
 * equipment se..." (0x100B5DAC) and "Done!\n" (0x100B5DA4); 0x10008B80 is a
 * bare `ret` in this build, so they do nothing. */
void BrSub10071130(int a, int b);

#ifdef __cplusplus
}
#endif

#endif /* SLICE5_60_H */
