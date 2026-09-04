/* br_optrefresh.c -- menus: republish every derived setting after the player
 * has changed something in the options, nudging the track and car selections
 * past anything that is not available (0x1003E510).
 *
 * Filed out of slice5_61.c, whose preamble it keeps verbatim below so the
 * compiler's view of the body is unchanged.  The original banner follows.
 *
 * slice5_61.c -- decompiled from BRD3D.dll, pass-61 packet (slice 5).
 *
 * See slice5_61.h for the full inventory, including the six addresses in this
 * packet that turned out to be ALREADY IMPLEMENTED under a different name and
 * the six that are not tractable.
 *
 * FLOAT CONSTANTS -- read out of the images' .rdata with tools/pe.py, not
 * guessed:
 *     BRD3D    0x1008F3EC =  0.25f     (viewport scale/translate, 2.2 fixed)
 *     BRD3D    0x1008F3F0 = -0.25f     (the Y SCALE only)
 *     BRGlide  0x1007740C =  0.25f     (ALL FOUR -- Glide has no -0.25f here)
 *     BRGlide  0x100A7518 =  480       (an INT, `fild`: the screen height)
 * See BrGbiCall10024260, which now transcribes the Glide arithmetic.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice5_61.h"

#include <string.h>

#include "slice1_03.h"   /* BrTextState / BrTextGetState, BR_TEXT_ALIGN_* */
#include "slice5_63.h"   /* g_br4B035C -- raw text-align global */
#include "slice2_15.h"   /* BrRdpRegs / BrRdpGetRegs -- 0x104BBF08 etc.   */
#include "slice2_25.h"   /* the option globals, index tables and callees   */

/* The two index sweeps BrSub1003E510 shares with 0x1003CE80: copied here
 * verbatim, statics both, so each file keeps its own.  Their shape notes,
 * including the non-terminating wrap in the car sweep, are on the helpers
 * themselves. */
/* 0x1003CF3E..0x1003CF64 and 0x1003E543..0x1003E569, upper bound 0x1F. */
static void Br61AdvanceTrack(void)
{
    int32_t start = g_br0AC654;

    if (BrSub1003F320(start) != 0)
        return;                        /* the start index is tested FIRST */

    for (;;) {
        if (++g_br0AC654 > 0x1F)
            g_br0AC654 = 0;
        /* NOTE: unlike the car sweep below, the wrap FALLS INTO this test,
         * so a full circle really does end the search. */
        if (g_br0AC654 == start)       /* full circle: give up, no signal */
            return;
        if (BrSub1003F320(g_br0AC654) != 0)
            return;
    }
}

/* 0x1003E5D5..0x1003E60C. The bound is 14 when 0x10AA28FC is set, else 11. */
static void Br61AdvanceCar(void)
{
    int32_t start = g_br0AC648;
    int32_t limit;

    if (BrSub1003F2B0(start) != 0)
        return;

    for (;;) {
        /* `neg edx / sbb edx,edx / and edx,3 / add edx,0xB` */
        limit = (g_brAA28FC != 0 ? 3 : 0) + 0xB;

        if (++g_br0AC648 > limit) {
            g_br0AC648 = 0;
            /* GOTCHA (reproduced, and slice2_25.c records the same thing for
             * 0x10042EE0): the wrap path JUMPS PAST the full-circle test, so
             * index 0 is probed twice -- and if the sweep STARTED at 0 with
             * nothing selectable, the full-circle test is never reachable and
             * the loop does not terminate. That hang is in the original. */
        } else if (g_br0AC648 == start) {
            return;
        }

        if (BrSub1003F2B0(g_br0AC648) != 0)
            return;
    }
}

/* ==========================================================================
 * 0x1003E510  mode selection / derived-global refresh
 * ========================================================================== */

/* WHAT IT DOES: refreshes all the derived settings after the player has
 * changed something in the options: it looks up the chosen track, car and
 * half a dozen other choices in their respective tables and publishes the
 * values the rest of the game reads. It also nudges the track and car
 * selections forward to the next one that is actually available, so a locked
 * choice cannot be left selected. */
/* @implements 0x1003E510 d3d BrSub1003E510 */
#ifdef BR_MATCHING_BUILD
/* The two sweeps are INLINE in the original -- 99 instructions against 47
 * with them factored out, which is the whole 137-byte gap. They stay as
 * static helpers for their other caller (0x1003E4?? above); MSVC declines to
 * inline a static with two call sites, so this arm spells both out. Their
 * shape notes, including the non-terminating wrap in the car sweep, are on
 * the helpers themselves. */
void BrSub1003E510(void)
{
    int32_t start;

    BrSub1003E3A0();
    g_br094350 = g_br0AC65C;

    if (g_br0AA010 == 6)
        BrSub10044540();

    /* ---- Br61AdvanceTrack, inline ----
     * The probe takes the GLOBAL, not the saved start: the original loads it
     * once into eax, pushes that, and copies eax to esi for the start. Passing
     * the local instead loses the copy. */
    start = g_br0AC654;
    if (BrSub1003F320(g_br0AC654) == 0) {
        for (;;) {
            if (++g_br0AC654 > 0x1F)
                g_br0AC654 = 0;
            if (g_br0AC654 == start)
                break;
            if (BrSub1003F320(g_br0AC654) != 0)
                break;
        }
    }

    g_br22B34C = g_aBrAC420[g_br0AC654];
    g_br09435C = g_aBrAC4A0[g_br0AC64C];
    g_br094358 = g_aBrAC4B0[g_br0AC650];
    g_br094354 = g_aBrAC518[g_brAA2A08];

    if (g_br0AA010 != 0) {
        /* ---- Br61AdvanceCar, inline ---- */
        start = g_br0AC648;
        if (BrSub1003F2B0(g_br0AC648) == 0) {
            for (;;) {
                int32_t limit = (g_brAA28FC != 0 ? 3 : 0) + 0xB;

                if (++g_br0AC648 > limit) {
                    g_br0AC648 = 0;
                } else if (g_br0AC648 == start) {
                    break;
                }
                if (BrSub1003F2B0(g_br0AC648) != 0)
                    break;
            }
        }

        g_br0B380C = g_aBrAC4D8[g_br0AC648];
        g_br0BD3E0 = g_br0AC658;
        g_br22B350 = g_aBrAC4C0[g_brAA2A00];
    } else {
        /* The original loads the DWORD at 0x10AA26F4 and uses byte 0 and
         * byte 1 of it:  index = byte1 + 12 * byte0.
         * DEVIATION: decoded from the two bytes directly instead of from a
         * dword, so the result does not depend on host endianness. */
        size_t idx = (size_t)g_brAA26F5 + 12u * (size_t)g_brAA26F4;

        /* The DOUBLED index in its own local: written `[idx*2]` and
         * `[idx*2+1]` inline, VC5 folds the x2 into the SIB scale
         * (`[eax*2 + base]`); the original materialises it with one
         * `shl eax,1` and then indexes `[eax+base]` / `[eax+base+1]`. */
        size_t k = idx * 2u;

        g_br0B380C = g_aBr0B3820[k];       /* zero-extended, `mov cl` */
        g_br22B350 = g_aBr0B3820[k + 1u];  /* zero-extended, `mov dl` */
    }

    BrSub1005FCF0();
}
#else
void BrSub1003E510(void)
{
    BrSub1003E3A0();
    g_br094350 = g_br0AC65C;

    if (g_br0AA010 == 6)
        BrSub10044540();

    Br61AdvanceTrack();

    g_br22B34C = g_aBrAC420[g_br0AC654];
    g_br09435C = g_aBrAC4A0[g_br0AC64C];
    g_br094358 = g_aBrAC4B0[g_br0AC650];
    g_br094354 = g_aBrAC518[g_brAA2A08];

    if (g_br0AA010 != 0) {
        Br61AdvanceCar();

        g_br0B380C = g_aBrAC4D8[g_br0AC648];
        g_br0BD3E0 = g_br0AC658;
        g_br22B350 = g_aBrAC4C0[g_brAA2A00];
    } else {
        size_t idx = (size_t)g_brAA26F5 + 12u * (size_t)g_brAA26F4;

        g_br0B380C = g_aBr0B3820[idx * 2u + 0u];
        g_br22B350 = g_aBr0B3820[idx * 2u + 1u];
    }

    BrSub1005FCF0();
}
#endif
