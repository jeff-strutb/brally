/* slice5_61.h -- decompiled from BRD3D.dll, pass-61 packet (slice 5).
 *
 * This packet is a "close the link" packet: every entry is an address some
 * already-ported module calls through an `extern` it declared itself. The
 * names below are therefore NOT chosen here -- they are copied verbatim from
 * the declaring header so the link closes mechanically.
 *
 * ==========================================================================
 * WHAT IS IN THIS FILE                            (7 of the 19 wanted)
 * ==========================================================================
 *   0x10019290  BrSub_10019290       (slice2_15)
 *   0x10024260  BrGbiCall10024260    (slice2_16)
 *   0x1003CE80  BrSub1003CE80        (slice2_13, slice2_25, slice4_50)
 *   0x1003E510  BrSub1003E510        (slice4_50, slice3_31)
 *   0x10042410  BrExt_10042410       (slice3_31)
 *   0x10042AF0  BrGfx42AF0_1         (slice2_18)
 *   0x10060E90  BrTimeNow            (slice2_18)
 *
 * ==========================================================================
 * WHAT IS NOT, AND WHY                            (see the report)
 * ==========================================================================
 * ALREADY IMPLEMENTED ELSEWHERE -- these six are not gaps at all, they are
 * NAME collisions. Do not implement them again; wire the existing symbol.
 *   0x10031140  BrSub_10031140  == slice1_05.c  BrMat4Translate
 *   0x10035CE0  BrEnt35CE0      == slice2_19.c  BrPadTranslate
 *   0x1003C150  BrSub1003C150   == slice4_50.c  BrSub1003C150   (same name)
 *   0x1003DB00  BrExt_1003DB00  == slice2_22.c  BrDPlaySendTag7
 *   0x10043330  BrExt_10043330  == slice2_25.c  BrOptOpen2970
 *   0x100440D0  BrExt_100440D0  == slice2_25.c  BrOptOpen294C
 *
 * NOT TRACTABLE IN THIS PACKET
 *   0x1001BE90  BrSub_1001BE90   1934 bytes of Direct3D vertex-buffer /
 *                                vtable-slot work (slots +0x5C, +0x74,
 *                                +0x78 on 0x10277368) plus a second, wholly
 *                                different code path selected by a magic
 *                                number. Nothing here fixes the vertex
 *                                struct, so a port would be invented.
 *   0x10051D30  BrOptFn10051D30  }  C++ menu-screen constructors: SEH
 *   0x10057C10  BrOptFn10057C10  }  frames, `operator new` of 0x348 /
 *   0x10052030  BrExt_10052030   }  0x1E214-byte widget objects, and
 *   0x1005A6E0  BrExt_1005A6E0   }  eight-argument calls through vtable
 *   0x1004E830  BrExt_1004E830   }  slot +0x38 of classes that exist
 *                                   nowhere in the port yet. Their callers
 *                                   only ever take their ADDRESS, so a stub
 *                                   links exactly as well as a guess.
 */
#ifndef SLICE5_61_H
#define SLICE5_61_H

#include <stddef.h>
#include <stdint.h>

#include "slice1_05.h"   /* BrGfxWords -- the 8-byte display-list command */

/* ==========================================================================
 * Globals this packet needs that NO existing header exposes free-standing
 *
 * INTEGRATION: each of these already appears inside some other slice's state
 * struct. They are declared here as plain externs because that is how the
 * original addresses them; de-duplicate against the listed field.
 * ========================================================================== */

/* 0x100AB3F4 -- ALIASES slice2_23.h BrUiGlobals::g0AB3F4 and slice3_31.h
 * BrPhaseCtx31::n0AB3F4 ("set to -1 by every name reset"). Used here as an
 * index into the 0x10AA29D0 record array, so -1 is a live value and the
 * indexing is NOT range-checked by the original. */
extern int32_t g_br0AB3F4;

/* 0x10AA29D0 -- ALIASES slice3_32.h BrPhaseGlobals::nAA29D0 (modelled there
 * as an int32_t because the only thing slice3_32 does is store 0 into it).
 * It holds a POINTER to an array of records; see BR61_REC29D0_*. */
extern unsigned char *g_brPAA29D0;

/* 0x10A9D018 and 0x10A9D078 -- name scratch buffers. slice2_23.h models
 * 0x10A9D018 as `char *szA9D018`; in the original it is the buffer itself.
 * The two are 0x60 bytes apart, which is the only evidence there is for a
 * size, so both are left incomplete here and copied into with strcpy exactly
 * as the original does. */
extern char g_aBrA9D018[];
extern char g_aBrA9D078[];

/* 0x104BC198 -- the viewport's Y SCALE. slice2_15.h's BrRdpRegs gathers the
 * other three viewport globals (0x104BBF08, 0x104C0BB0, 0x104C0BB8) but not
 * this one, because nothing in that packet writes it. Defined by
 * slice5_61.c; if BrRdpRegs ever grows a field for it, drop this. */
extern float g_br4BC198;

/* 0x10AA26F4 / 0x10AA26F5 -- ALIASES slice3_31.h BrPhaseCtx31::bAA26F4 and
 * ::bAA26F5. 0x1003E510 reads the DWORD at 0x10AA26F4 and then uses only its
 * low two bytes; they are declared as two bytes here so the decode does not
 * depend on host endianness (see the DEVIATION in slice5_61.c).
 *
 * ALIAS RESOLVED. slice5_63.c already owns this dword as `g_aBrAA26F4[4]`
 * and reads the same two bytes out of it ([0] and [1]). Two host objects for
 * one original dword links cleanly and drifts apart on the first write, so
 * the two names are now two spellings of ONE array rather than two objects.
 * Both are used as plain values, so macros are enough and no call site
 * changes. */
extern uint8_t g_aBrAA26F4[4];        /* 0x10AA26F4, the whole dword */
#define g_brAA26F4 (g_aBrAA26F4[0])
#define g_brAA26F5 (g_aBrAA26F4[1])

/* 0x100B3820 -- two bytes per entry, indexed by (g_brAA26F5 + 12*g_brAA26F4).
 * Byte 0 of the pair goes to 0x100B380C, byte 1 to 0x1022B350. The DLL image
 * only holds four plausible pairs before the region turns into what looks
 * like unrelated 0xE0/0xE1/0xE2 records, so treat indices above 3 as
 * out of range; the original does not check. */
extern const uint8_t g_aBr0B3820[];

/* DEVIATION (portability). BrGbiCall10024260 dereferences pCmd->w1, which is
 * a 32-bit address in the original and stays 32 bits wide in the port, so on
 * a 64-bit host it cannot be reinterpreted as a host pointer. This hook turns
 * a display-list word into a host address, exactly as slice2_19.h's
 * g_BrModelDeref does for model slots. NULL (the default) means "reinterpret
 * it", which is what the original does and is exact on a 32-bit build. */
extern const void *(*g_brPfnDerefW1)(uint32_t w1);

/* 0x1003D0B0 -- SIGNATURE CONFLICT, reached through a pointer so that this
 * header contradicts neither of the two existing declarations:
 *
 *   slice4_51.h  int32_t BrSub1003D0B0(BrDPlay4Obj *, void **)   <- implemented
 *   slice2_25.h  void    BrSub1003D0B0(BrDPlay *, BrDPSessionDesc **)
 *
 * 0x1003CE80 TESTS the returned HRESULT (`test esi,esi / jge`), so the
 * int32_t form is the load-bearing one. The integration should point this at
 * slice4_51.c's BrSub1003D0B0. Until it does, BrSub1003CE80 treats a NULL
 * pointer here as "the call failed and wrote nothing".
 */
extern int32_t (*g_brPfn1003D0B0)(void *pObj, void **ppvOut);

/* ==========================================================================
 * Cross-slice callees the existing headers do not already declare
 * ========================================================================== */

/* XSLICE 0x1003E3A0 -- "options apply"; slice1_06.h lists it as NOT PORTED. */
extern void BrSub1003E3A0(void);

/* ==========================================================================
 * The packet
 * ========================================================================== */

/* 0x10019290 (8 bytes)  Text alignment mode := 1.
 *
 * The whole body is `mov byte [0x104B035C], 1`. 0x104B035C is slice1_03.h's
 * BrTextState::align, and slice1_03.h names the value 1 BR_TEXT_ALIGN_RIGHT
 * (0x10019270 writes 2 = CENTER, 0x10019280 writes 0 = LEFT). Implemented in
 * terms of BrTextGetState() so it drives the same byte the text renderer
 * reads, rather than a private copy. */
void BrSub_10019290(void);

/* 0x10024260 (138 bytes)  G_MOVEMEM index 0x80 -- LOAD VIEWPORT.
 *
 * pCmd->w1 is the (already segment-resolved) address of an N64 `Vp`:
 *     s16 vscale[4]; s16 vtrans[4];      // 2.2 fixed point
 * Four of the eight shorts are used, sign-extended, converted to float and
 * scaled:
 *     0x104BBF08 = vscale.x * ( 0.25f)   (0x1008F3EC)
 *     0x104BC198 = vscale.y * (-0.25f)   (0x1008F3F0)  <-- NEGATED
 *     0x104C0BB0 = vtrans.x * ( 0.25f)
 *     0x104C0BB8 = vtrans.y * ( 0.25f)
 * vscale.z / vtrans.z / the two w slots are ignored.
 *
 * GOTCHA: only the Y SCALE is negated, and it is negated by using a separate
 * -0.25f constant rather than by negating the result -- easy to miss.
 *
 * This confirms slice2_15.h's guesses from the other end of the pipe: it
 * records 0x104BBF08 and 0x104C0BB0 as "(float)(cx/2)" and 0x104C0BB8 as
 * "(float)(cy/2)", which is exactly a viewport for a cx x cy screen.
 *
 * Returns pCmd + 1 (the original's `add eax, 8` -- BrGfxWords is 8 bytes).
 * GOTCHA: eax is set BEFORE any of the work, so the return value does not
 * depend on anything the body does; and the routine spills through its own
 * incoming argument slot, which is invisible from C but means the caller's
 * copy of pCmd is clobbered in the original. */
BrGfxWords *BrGbiCall10024260(BrGfxWords *pCmd);

/* 0x1003CE80 (311 bytes)  Adopt the host's session settings.
 *
 * Fetch DPSESSIONDESC2 through 0x1003D0B0, copy its four dwUser fields into
 * six globals, re-derive the mode tables (0x10044540), step the track index
 * to the next selectable one, copy the session NAME into 0x10A9D018, and
 * release the Global-allocated descriptor.
 *
 * GOTCHA: it returns 0x88770082 (a DirectPlay HRESULT) when 0x10277B40 is
 * NULL, the propagated HRESULT on failure, and 0 on success -- yet every one
 * of the three callers declares it `void` and ignores the value. The `void`
 * declaration is kept here because it is what all three use.
 *
 * GOTCHA: on the failure path the descriptor is freed only if 0x1003D0B0
 * actually wrote the out-parameter; 0x1003D0B0 leaves it untouched on every
 * failure, which is why the local is zeroed first.
 *
 * GOTCHA: each dwUser field lands in TWO unrelated globals:
 *     dwUser1 -> 0x100AC648 (car index) and 0x100B380C
 *     dwUser2 -> 0x10AA2A00             and 0x1022B350
 *     dwUser3 -> 0x10AA2A18
 *     dwUser4 -> 0x100BD3E0             and 0x100AC658
 */
void BrSub1003CE80(void);

/* 0x1003E510 (368 bytes)  Mode selection / derived-global refresh.
 *
 * slice1_06.h declined this one for want of "five index tables and two
 * further out-of-range calls"; slice2_25.h supplies all five tables
 * (0x100AC420, 0x100AC4A0, 0x100AC4B0, 0x100AC4C0, 0x100AC4D8, 0x100AC518)
 * and both calls (0x10044540, 0x1005FCF0), so it is portable now.
 *
 * Shape:
 *   0x1003E3A0();  0x10094350 = 0x100AC65C;
 *   if (0x100AA010 == 6) 0x10044540();
 *   advance 0x100AC654 to the next index 0x1003F320 accepts (0..0x1F);
 *   0x1022B34C = AC420[track];  0x1009435C = AC4A0[0x100AC64C];
 *   0x10094358 = AC4B0[0x100AC650];  0x10094354 = AC518[0x10AA2A08];
 *   if (0x100AA010 != 0) {
 *       advance 0x100AC648 to the next index 0x1003F2B0 accepts;
 *       0x100B380C = AC4D8[car];  0x100BD3E0 = 0x100AC658;
 *       0x1022B350 = AC4C0[0x10AA2A00];
 *   } else {
 *       (0x100B380C, 0x1022B350) = the byte pair at 0x100B3820;
 *   }
 *   0x1005FCF0();
 *
 * GOTCHA: the car index's upper bound is DATA-DEPENDENT -- it is 14 when
 * 0x10AA28FC is non-zero and 11 when it is zero (`neg / sbb / and 3 / add
 * 0xB`). slice2_25.h records the same thing for 0x10042EE0.
 *
 * GOTCHA: both search loops test the STARTING index first, so an already
 * selectable index is left alone; and if nothing is selectable the index is
 * left back where it started, with no failure signal.
 *
 * GOTCHA: the two sweeps differ in one branch and it matters. The TRACK
 * sweep falls from the wrap-to-0 straight into the "came back round" test,
 * so it always terminates. The CAR sweep JUMPS PAST that test on the wrap
 * (0x1003E5F4 -> 0x1003E601), so index 0 is probed twice -- and if the sweep
 * starts at 0 with nothing selectable it never terminates. slice2_25.c
 * records the same asymmetry between 0x10042B30 and 0x10042EE0, so it is the
 * original's, not a transcription error. Reproduced as-is.
 */
void BrSub1003E510(void);

/* 0x10042410 (179 bytes)  Commit the edited name into record 0x100AB3F4.
 *
 * Zeroes +0x70 of the game object's +0x2AE8 sub-object, replaces
 * record[n].f44C with the flag "f44C WAS zero", reads that flag back out and
 * publishes it to 0x10AA28D8, and -- only when it is set, i.e. only when the
 * old f44C was zero -- swaps the record's name at +0x35 out to 0x10A9D078
 * and the edited name at 0x1039B720 in.
 *
 * GOTCHA: the flag slot is written and then RE-READ from memory, so what
 * lands in 0x10AA28D8 is always 0 or 1, never the field's previous value.
 *
 * GOTCHA: the two record fields are reached from the SAME base with the SAME
 * index scale (0x438 bytes per record, computed as n*3*5*9*8), but +0x44C is
 * 0x14 bytes PAST the end of record n. Either the flag genuinely lives in
 * the next record or the real stride is larger than the index math implies.
 * Reproduced exactly rather than "corrected"; see BR61_REC29D0_OFF_FLAG.
 *
 * GOTCHA: the name swap is skipped entirely when the old flag was non-zero,
 * but 0x10AA28D8 is written on both paths.
 *
 * Always returns 1. slice3_31.c discards the value.
 */
int32_t BrExt_10042410(void *pArg);

/* The record geometry 0x10042410 uses on *g_brPAA29D0. */
#define BR61_REC29D0_STRIDE     0x438u
#define BR61_REC29D0_OFF_NAME   0x035u   /* char[] */
#define BR61_REC29D0_OFF_FLAG   0x44Cu   /* int32_t -- NOTE: > STRIDE */

/* 0x10042AF0 (6 bytes)  `mov eax, 1 / ret`.
 *
 * SIGNATURE CONFLICT (reported): the original returns 1 in eax and 0x1003C260
 * tests it, but slice2_18.h -- the only header that DECLARES this address --
 * declares `void BrGfx42AF0_1(void *)`, and slice4_50.h deliberately routes
 * around that with a function POINTER (`g_brPfn42AF0_1`). The majority (and
 * only) declaration is followed here. The value it would have returned is a
 * constant 1, so integration can point g_brPfn42AF0_1 at any `return 1`.
 *
 * The argument is never read; 0x10042AF0 is also called with THREE arguments
 * (slice2_18.h's BrGfx42AF0_3), which is consistent -- it reads none of them.
 */
void BrGfx42AF0_1(void *p0);

/* 0x10060E90 (5 bytes)  `jmp 0x10078C10`.
 *
 * 0x10078C10 is NOT a clock. It is a 64-bit accumulator at 0x118ABDE0
 * (uninitialised .bss, so it starts at 0) which each call advances by a
 * literal 0x0017D784 = 1,562,500 and then returns the LOW dword of, as a
 * signed 32-bit value:
 *     mov eax,[118ABDE0] / mov edx,[118ABDE4] / add eax,17D784 / adc edx,0
 *     mov [118ABDE0],eax / mov [118ABDE4],edx / ret
 * So it is a deterministic fake timer: the n-th call returns n*1562500 mod
 * 2^32. slice2_18.c uses it for frame timing (0x106C0208/0x106C020C), and
 * slice2_17.c reaches the same address under the name BrX10060E90.
 *
 * GOTCHA: the high dword is maintained but never returned, so the result
 * wraps: 2^32 / 1562500 is 2748.8, and the sign bit is set for roughly half
 * of every such cycle. Anything that subtracts two of these (slice2_18.c
 * does: `BrG_6C020C = BrTimeNow() - BrG_6C020C`) is relying on the wrap.
 */
int32_t BrTimeNow(void);

/* Test/reset hook for the counter above. Not present in the original. */
uint64_t *BrTimeNowCounter(void);

#endif /* SLICE5_61_H */
