/* slice5_63.h -- decompiled from BRD3D.dll, agent-63 packet (slice 5).
 *
 * This packet is a "close the link" packet: every function below is already
 * declared `extern` by the module that calls it. The declarations here are
 * therefore DUPLICATES of the callers' declarations, repeated verbatim so the
 * coordinator can see them in one place. Where a caller's declaration and the
 * original disagree (the original returns a value that the caller declares
 * away, e.g. 0x1003C1E0 and 0x1007AC00), the CALLER's form wins and the
 * dropped value is noted.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS NOT HERE
 * ---------------------------------------------------------------------------
 * Ten of the twenty-seven addresses in the packet are deliberately absent; see
 * the report. In short:
 *   0x1003D210  (wanted twice, as BrFn1003D210 AND BrSub1003D210)  Win32
 *   0x10004C60  BrNetSendFull                                      Win32
 *   0x1003C5C0  BrSub1003C5C0                                      DirectPlay
 *   0x10038F30  BrExt_10038F30                    call sequence, see report
 *   0x1004CAC0 0x10049C20 0x1004F2B0 0x10052F50 0x10059BB0   C++ UI builders
 *
 * ---------------------------------------------------------------------------
 * HEADER COMPATIBILITY WARNING
 * ---------------------------------------------------------------------------
 * `slice1_06.h` and `slice2_25.h` CANNOT be included in one translation unit
 * (both define `BrDPlayVtbl`, differently). This header includes neither, so
 * it is safe to include next to either of them. slice5_63.c needs symbols from
 * both and works around the collision locally -- see the comment there.
 */
#ifndef SLICE5_63_H
#define SLICE5_63_H

#include <stdint.h>

#include "slice1_09.h"   /* BrVec4                                          */
#include "slice2_15.h"   /* BrHudView                                       */

/* ==========================================================================
 * 1. Stubs and thunks
 * ========================================================================== */

/* 0x10075330 -- a bare `ret` in the shipped DLL, exactly like 0x10008B80 and
 * 0x100378A0 (see CONTRACT). slice2_16 and slice3_44 both call it with a real
 * pointer; it does nothing with it. This is NOT a placeholder for missing
 * work: the one byte at 0x10075330 is `c3`. */
void BrGbiCall10075330(void *pv);

/* 0x100027C0 -- CD track dispatch. `g_brCdEnabled == 1` selects 0x10002870,
 * ANY other value (including 2, 3, ...) selects 0x100027F0. The test is
 * `cmp ...,1 / jne`, not a zero test. */
void BrCdTrackPlay(int track);

/* 0x1007AC00 -- if 0x1007A840 reports nothing, return; otherwise normalise
 * 0x1007A940's result to 0/1 (`neg/sbb/neg`).
 *
 * GOTCHA: slice2_26 declares this `void`, so BOTH return values are dropped.
 * The early-exit path returns 0x1007A840's value UNNORMALISED, the other path
 * returns 0 or 1. Kept as void to match the caller. */
void BrExt_1007AC00(void);

/* ==========================================================================
 * 2. Text / HUD state pokes (0x10019260 .. 0x100192F0)
 * ========================================================================== */

/* All four write one global and return. Three of them are slice1_03.h's
 * BrTextState fields and are forwarded through BrTextGetState() so the state
 * stays shared with BrTextDraw; the fourth writes 0x104B0358, which no other
 * slice models, so it is defined here. */

/* 0x10019260  g_br4B0358 = 0 */
void BrSub_10019260(void);
/* 0x10019270  BrTextGetState()->align = 2  (BR_TEXT_ALIGN_CENTER) */
void BrSub_10019270(void);
/* 0x10019280  BrTextGetState()->align = 0  (BR_TEXT_ALIGN_LEFT) */
void BrSub_10019280(void);
/* 0x100192F0  BrTextGetState()->scale = size */
void BrSub_100192F0(int size);

/* 0x104B0358 -- written by 0x10019260 only. One byte. Nothing in any packet
 * reads it, so its meaning is unknown and the name is positional. */
extern uint8_t g_br4B0358;

/* ==========================================================================
 * 3. 0x10017290 -- the two-line lap/split time readout
 * ========================================================================== */

/* Draws one or two BrHudDrawTimeEntry lines at the right-hand edge.
 *
 * GOTCHA 1: the vertical gap between the two lines is 0x1E in a ONE-view
 * layout and 0 otherwise, so in split screen the second line lands exactly on
 * top of the first. That is what the original computes
 * (`dec/neg/sbb/and 0xFFFFFFE2/add 0x1E`).
 *
 * GOTCHA 2: the first line is drawn ONLY in the one-view layout, but the
 * second is always drawn -- and it is the second that gets the offset.
 *
 * GOTCHA 3: 0x100AA010 selects through a 7-entry jump table in which modes
 * 0, 1, 2 and 6 share one arm, mode 3 has its own, and modes 4 and 5 draw
 * NOTHING. Anything above 6 also draws nothing (`ja`, unsigned). */
void BrSub_10017290(BrHudView *aViews);

/* 0x100BD3EC -- the gate 0x10017290 returns on. No other slice models it. */
extern int32_t g_br0BD3EC;

/* ==========================================================================
 * 4. 0x10031688 -- filled-rectangle helper
 * ========================================================================== */

/* Emits SEVEN 8-byte commands into the 0x106C0680 write cursor:
 *
 *   E7000000 00000000   pipe sync
 *   B900031D 0F0A4000   set other mode L
 *   BA001402 00300000   set other mode H  (cycle type -> fill)
 *   F7000000 CCCCCCCC   set fill colour, the 16-bit value doubled
 *   E1xxxxxx xxxxxxxx   fill rectangle, INTEGER corners (see CONTRACT)
 *   E7000000 00000000   pipe sync
 *   BA001402 00000000   set other mode H  (cycle type -> 0)
 *
 * The colour is packed RGBA5551: `(c0<<8 & 0xF800) | (c1<<3 & 0x7C0) |
 * (c2>>2 & 0x3E) | 1`, and the `>>2` on c2 is an ARITHMETIC shift.
 *
 * GOTCHA -- the hi-res scale is applied TWICE, asymmetrically. When
 * BrG_6C65E4 is non-zero all four arguments are doubled up front; the
 * lower-right corner is then shifted LEFT AGAIN by BrG_6C65E4 while the
 * upper-left corner is not shifted at all. With BrG_6C65E4 == 1 that makes
 * the rectangle's lower-right 4*(x+w)-1 against an upper-left of 2*x. The
 * asymmetry is in the original; do not "fix" it. */
void BrSub_10031688(int32_t x, int32_t y, int32_t w, int32_t h,
                    int32_t c0, int32_t c1, int32_t c2);

/* ==========================================================================
 * 5. 0x10074090 -- quaternion product
 * ========================================================================== */

/* ESTABLISHED HERE (slice3_45.h says the order was not established from that
 * packet alone): this is the Hamilton product `*pDst = (*pA) * (*pB)` with
 * element f00 as the SCALAR part and f04/f08/f0C as the vector part:
 *
 *   f00 = a0*b0 - a1*b1 - a2*b2 - a3*b3
 *   f04 = a1*b0 + a0*b1 - a3*b2 + a2*b3
 *   f08 = a2*b0 + a3*b1 + a0*b2 - a1*b3
 *   f0C = a3*b0 - a2*b1 + a1*b2 + a0*b3
 *
 * (The x87 listing has no fmul/fsub pairing that survives casually; the order
 * above comes from tracing all 24 fxch/faddp/fsubp operands. The signs are
 * self-consistent: it is a real Hamilton product, not the reversed one.)
 *
 * ALIASING: every component of both inputs is loaded before the first store,
 * so pDst may alias pA and/or pB. slice3_45 relies on this -- it calls
 * f(&q, &q, &local). */
void BrSub10074090(BrVec4 *pDst, const BrVec4 *pA, const BrVec4 *pB);

/* ==========================================================================
 * 6. The options block (0x1003E310, 0x1003F320, 0x1003E510)
 * ========================================================================== */

/* 0x1003E310 and 0x1003F320 are ALREADY IMPLEMENTED, as slice1_06.h's
 * BrOptSave and BrOptAvailB. The two entry points below are thin adapters
 * that gather the loose globals into the structs those take and call them --
 * there is no second copy of either body. */

/* 0x10B4E710 -- the 12-dword scratch block BrOptSave fills. slice1_06.h names
 * the layout (BR_OPT_SCRATCH_COUNT) but nothing declared the storage.
 *
 * COLLISION: slice2_25.h declares `g_brB4E728` (0x10B4E728) as a standalone
 * int32_t. That address is element 6 of this array. They are the same memory
 * in the original; the coordinator has to pick one. */
#define BR63_SCRATCH_COUNT 12
extern int32_t g_aBrB4E710[BR63_SCRATCH_COUNT];

/* 0x1003E310 */
void BrSub1003E310(void);

/* 0x1003F320 -- non-zero => index is selectable. Returns the MASKED BIT, not
 * a normalised 0/1 (BrOptAvailB's contract). */
int BrSub1003F320(int index);

/* 0x1003E510 -- recompute every derived option value.
 *
 * GOTCHA: the two "advance to the next selectable index" loops give up
 * DIFFERENTLY. Each remembers the index it started from and stops when the
 * search wraps back onto it -- but it then uses THAT index anyway, even
 * though the predicate just rejected it. Both loops also use the value the
 * search left in the global, so a full-circle failure still moves the option.
 *
 * GOTCHA: 0x100AC648's upper bound is recomputed on every step (14 when
 * 0x10AA28FC is set, 11 otherwise), inside the loop, not before it. */
void BrExt_1003E510(void);

/* --- the loose globals BrOptAvailA / BrOptAvailB read ---------------------
 * These have no declaration anywhere else in the port (slice1_06.h gathers
 * them into BrOptCaps, slice2_23/24/31 into per-packet context structs), so
 * this packet defines the storage. Addresses are authoritative. */
extern int32_t  g_brAA28F8;   /* 0x10AA28F8 */
extern int32_t  g_brAA28F4;   /* 0x10AA28F4 */
extern int32_t  g_brAA28F0;   /* 0x10AA28F0 */
extern uint32_t g_brAA27E0;   /* 0x10AA27E0 -- ONE dword, TWO 16-bit masks   */
extern uint32_t g_brA9D010;   /* 0x10A9D010 */
extern uint32_t g_br0AB3EC;   /* 0x100AB3EC */
extern uint32_t g_brAA2598;   /* 0x10AA2598 */
extern uint32_t g_br0AB3E8;   /* 0x100AB3E8 */
extern int16_t  g_br0AB3E4;   /* 0x100AB3E4 -- SIGN-extended when used       */

/* 0x10AA2A10 / 0x10AA2A14 -- selector slots 4 and 5. slice2_25.h declares
 * 0x10AA2A00/08/0C/18/1C/20/24/28 but skips these two. */
extern int32_t g_brAA2A10;    /* 0x10AA2A10 */
extern int32_t g_brAA2A14;    /* 0x10AA2A14 */

/* 0x10AA26F4 -- read as a dword, but only bytes 0 and 1 are ever used, and
 * 0x1005FBC0 addresses byte 1 directly as 0x10AA26F5.
 *
 * DEVIATION: modelled as a 4-byte array rather than a uint32_t so the byte
 * extraction is endianness-independent (see CONTRACT). Byte 0 is the low
 * byte of the original dword. */
extern uint8_t g_aBrAA26F4[4];

/* 0x100B3820 -- table of 2-byte records indexed by
 * `g_aBrAA26F4[1] + 12 * g_aBrAA26F4[0]`; byte 0 goes to 0x100B380C and byte
 * 1 to 0x1022B350. slice1_05.h and slice2_23.h describe the same table. The
 * length is not recoverable; declared incomplete on purpose. */
extern const uint8_t g_aBr0B3820[];

/* ==========================================================================
 * 7. 0x1005FBC0 -- publish the staged settings
 * ========================================================================== */

/* GOTCHA: the argument gates ONLY the four-halfword checksum at the end. The
 * whole publish half runs regardless. slice3_31 calls it with 1.
 *
 * GOTCHA: 0x10B4E1D4 is pointed at record (v == 1 ? 1 : v == 2 ? 2 :
 * v == 3 ? 3 : 0) of the 0x10B4DF30 array -- note that 0 AND everything above
 * 3 both select record 0, because the original tests `dec/je` three times. */
void BrExt_1005FBC0(int32_t a);

extern int32_t g_brAA27EC;    /* 0x10AA27EC */
extern int32_t g_brAA27F0;    /* 0x10AA27F0 */
extern int32_t g_brAA27F4;    /* 0x10AA27F4 */
extern int32_t g_brAA27F8;    /* 0x10AA27F8  selects the 0x10B4DF30 record   */
extern int32_t g_brAA28A0;    /* 0x10AA28A0 */
extern int32_t g_brAA28A4;    /* 0x10AA28A4 */
extern int32_t g_brAA28AC;    /* 0x10AA28AC */
extern int8_t  g_brAA28B8;    /* 0x10AA28B8 -- read with movsx, SIGNED       */
extern int32_t g_brAA28C4;    /* 0x10AA28C4 */

/* 0x10AA2518 and 0x10A9D618 -- the two BrSprintf destinations. Neither size
 * is recoverable; BR_OPT_TEXT_MAX (0x104) is used, matching slice2_25.h. */
#define BR63_TEXT_MAX 0x104
extern char g_aBrAA2518[BR63_TEXT_MAX];   /* 0x10AA2518 */
extern char g_aBrA9D618[BR63_TEXT_MAX];   /* 0x10A9D618 */

/* 0x100A73C4 -- the BrSprintf format both calls use. */
extern const char *g_pszBr0A73C4;

/* ==========================================================================
 * 8. 0x10043E70 -- create (once) and enter the options screen
 * ========================================================================== */

/* GOTCHA: the int argument is pushed by the caller and NEVER read.
 *
 * GOTCHA: on the already-created path the object's pfn04 is NOT re-installed
 * and NOT called, and +0x0C / +0x68 are NOT re-set; only 0x10AA2904 is
 * re-pointed. The two paths are not equivalent.
 *
 * GOTCHA: the trailing 0x1003C020 call needs 0x10A9CFFC == 0 AND
 * 0x10A9D000 == 0 AND 0x10AA287C in {0, 1}. An allocation failure returns
 * before reaching it. */
void BrExt_10043E70(int32_t a);

/* ==========================================================================
 * 9. 0x1003C1E0 / 0x1003D130 -- session start helpers
 * ========================================================================== */

/* 0x1003C1E0 -- 0x1003C020, then SetTimer(0x10680584, 1, 1000, NULL), then
 * 0x10A9CFFC = 1, then 0x1003CC70(0x10277B40) if 0x10AA29D4 is set.
 *
 * This is 0x1003C230 (slice4_53's BrTimerStart1003C230) PLUS the trailing
 * 0x1003CC70 call, so it reuses slice4_53's SetTimer hook and 0x10A9BFDC
 * rather than introducing a second one. The original returns 1; slice2_25
 * declares it void.
 *
 * GOTCHA: 0x10AA29D4 is loaded BEFORE 0x10A9CFFC is written, so a 0x1003C020
 * that clears 0x10AA29D4 still lets the 0x1003CC70 call happen. Preserved. */
void BrSub1003C1E0(void);

/* 0x1003D130 -- copy the global name string at 0x10A9D018 into pDesc and zero
 * pDesc[0xC8].
 *
 * GOTCHA: the copy is skipped when strlen(0x10A9D018) <= 1, i.e. for BOTH the
 * empty string and any one-character string -- a one-character name is NOT
 * copied. The +0xC8 store happens either way. */
void BrSub1003D130(void *pDesc);

/* 0x10A9D018 -- the source string. slice2_23.h models it as a pointer field
 * (`szA9D018`); in the DLL it is the buffer itself. Size not recoverable. */
extern char g_aBrA9D018[BR63_TEXT_MAX];

/* 0x1003D130 writes a dword zero at this byte offset into its argument. */
#define BR63_DESC_ZERO_OFF 0xC8

#endif /* SLICE5_63_H */
