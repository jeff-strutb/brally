/* br_setupcommit.c -- settings: committing the setup-screen choices.
 *
 * BrExt_1005FBC0 copies the track / opponent / control choices made on the
 * setup screens into the settings the race reads, formats the two "N of M"
 * strings, and optionally totals four halfwords from a settings table.
 *
 * Filed out of the address batch slice5_63.c, whose preamble is carried
 * verbatim below.
 */
/* slice5_63.c -- decompiled from BRD3D.dll, pass-63 packet (slice 5).
 *
 * See slice5_63.h for what is here, what is not, and the gotchas.
 *
 * ---------------------------------------------------------------------------
 * WHY slice1_06.h IS NOT INCLUDED
 * ---------------------------------------------------------------------------
 * This file needs slice2_25.h (the option globals, BrOptObj, BrStrGet,
 * BrSub1003F2B0, the lookup tables) AND slice1_06.h (BrOptSave / BrOptAvailB,
 * which are 0x1003E310 and 0x1003F320 already decompiled). Those two headers
 * cannot coexist: both define `BrDPlayVtbl`, with different contents. That
 * collision predates this packet.
 *
 * The five slice1_06 declarations this file needs are therefore repeated
 * below, VERBATIM from slice1_06.h, behind the guard slice1_06.h itself uses,
 * so that if the collision is ever fixed and slice1_06.h ends up included
 * first this file picks up the real declarations instead. No second
 * implementation of either function is created: the definitions in
 * slice1_06.c are the ones that run.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_race.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
#include <stdio.h>
#include <string.h>

#define BrExt_1007AC00 BrExt_1007AC00_decl
#include "slice5_63.h"
#undef BrExt_1007AC00

#include "br_crt.h"      /* BrOperatorNew (0x1007DFE0)                       */
#include "slice1_03.h"   /* BrTextGetState, BrHudDrawTimeEntry               */
#include "slice2_25.h"   /* option globals, BrOptObj, BrStrGet, lookup tables */

/* ==========================================================================
 * Borrowed from slice1_06.h -- see the note at the top of the file.
 * ========================================================================== */
#ifndef SLICE1_06_H
#define BR_OPT_CFG_COUNT     6    /* 0x100AC648 + 4*i, contiguous */
#define BR_OPT_SEL_COUNT     7    /* 0x10AA2A00  + 4*i            */
#define BR_OPT_SCRATCH_COUNT 12   /* 0x10B4E710  + 4*i, packed    */

typedef struct BrOptState {
    int32_t aCfg[BR_OPT_CFG_COUNT];
    int32_t aSel[BR_OPT_SEL_COUNT];
} BrOptState;

typedef struct BrOptScratch {
    int32_t a[BR_OPT_SCRATCH_COUNT];
} BrOptScratch;

typedef struct BrOptCaps {
    int32_t  mode;           /* 0x100AA010 */
    int32_t  fForceAvailA;   /* 0x10AA28F8 */
    int32_t  fLowAlwaysB;    /* 0x10AA28FC */
    int32_t  fRebaseB;       /* 0x10AA28F4 */
    int32_t  fLowAlways;     /* 0x10AA28F0 */
    int32_t  fAlt;           /* 0x10AA289C */
    uint32_t maskPair;       /* 0x10AA27E0 (low 16) / 0x10AA27E2 (high 16) */
    uint32_t maskA;          /* 0x10A9D010 */
    uint32_t maskAMode;      /* 0x100AB3EC */
    uint32_t maskB;          /* 0x10AA2598 */
    uint32_t maskBMode6;     /* 0x100AB3E8 */
    int16_t  maskBDefault;   /* 0x100AB3E4 -- SIGN-extended when used */
    int32_t  nAlwaysB;       /* 0x10AD0984 */
} BrOptCaps;

/* BrOptSave: prototype in br_funcs.h */
int32_t BrOptAvailA(const BrOptCaps *pCaps, uint32_t n);
/* BrOptAvailB: prototype in br_funcs.h */
#endif /* SLICE1_06_H */

/* ==========================================================================
 * Cross-slice dependencies
 * ========================================================================== */

/* 0x106C0680, the display-list write cursor, is reached through slice2_15's
 * accessor rather than slice2_18's BrG_6C0680 -- slice2_15 is the module that
 * calls 0x10031688 and it must see the same cursor. */

/* XSLICE 0x106C65E4 -- slice2_18.h's name for the hi-res scale. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* XSLICE 0x106C2CF8 -- slice2_18.h's name. Deliberately the RAW pointer and
 * not slice2_15.h's BrRace *, because BrRace is documented there as "logical,
 * not byte-exact" and the three fields 0x10017290 needs (+0xFB0, +0xFE4,
 * +0xFEC) are not in it. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* XSLICE 0x10019290 -- slice2_15.h's name. */
/* BrSub_10019290: prototype in br_funcs.h */

/* XSLICE 0x100940A4 -- slice2_11.h's name. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* XSLICE 0x10A9BFDC / the SetTimer hook -- slice4_53.h's names. Declared by
 * hand (not via slice4_53.h) so the BrPlatSetTimerFn typedef is not
 * re-declared; the types are compatible with slice4_53.h's. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
extern uint32_t (*g_pfnBrPlatSetTimer)(void *hWnd, uint32_t idEvent,
                                       uint32_t uElapseMs, void *pfnProc);

/* Callees with no name anywhere in the port yet. Positional names, in the
 * BrSub<ADDR> form slice2_25.h uses. */

/* XSLICE 0x10002870 -- CD play, path A (g_brCdEnabled == 1). */
/* BrSub10002870: prototype in br_funcs.h */
/* XSLICE 0x100027F0 -- CD play, path B. */
/* BrSub100027F0: prototype in br_funcs.h */
/* XSLICE 0x1003E3A0 -- the inverse of BrOptSave; restores the twelve pairs. */
/* BrSub1003E3A0: prototype in br_funcs.h */
/* XSLICE 0x1003CC70 -- called with 0x10277B40. */
extern void BrSub1003CC70(void *p);
/* 0x1007A840 is the D3D-only gate in front of 0x1007AC00's one call;
 * BRGlide.dll has no counterpart and the Glide twin 0x10058F90 calls the
 * body unconditionally.  Declared only for the matching build, which diffs
 * against BRD3D.dll. */
extern int BrSub1007A840(void);
/* XSLICE 0x1007A940 (Glide 0x10058E20 -- byte-identical, shared.csv `body`) */
/* BrSub1007A940: prototype in br_funcs.h */

/* ==========================================================================
 * 7. 0x1005FBC0
 * ========================================================================== */

/* The four halfwords summed at the end start 0x1E bytes into the 0x10AA26F0
 * block (0x10AA270E) and step by 8 per index. */
#define BR63_AA270E_OFF     0x1E
#define BR63_AA270E_STRIDE  8
#define BR63_AA270E_TERMS   4

/* WHAT IT DOES: commits the choices the player has been making on the setup
 * screens -- track, opponents, control layout -- into the settings the race
 * itself will read, and builds the two "N of M" strings the screen displays
 * (both counted from one rather than from zero). Its argument only controls
 * whether it also totals up four numbers from a settings table at the end;
 * everything else happens either way. */
/* @t4-pass 0x10058900 1 2026-09-13 probes 75 bytes 288 insns 71 regions 3 rows 0 census yes  (tools/crank.py) */
/* @implements 0x1005FBC0 d3d BrExt_1005FBC0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AA26F4[0], [1] */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* high half of 0x10AA27E0 */
void BrExt_1005FBC0(int32_t a)
{
    int32_t v;

    (*(int8_t *)&DAT_10ac5c10) = (int8_t)(g_aBrAA26F4[0]);
    (*(int32_t *)&g_brPhase5BF8) = (*(int32_t (*)[83])&(DAT_10ac5a48[0]))[0];
    (*(int32_t *)&g_brIdx5BFC) = (int32_t)(g_aBrAA26F4[1]);     /* movzx: byte 1, zero-extended */
    (*(int32_t *)&g_7b324) = (*(int32_t *)&DAT_10ac5b44);
    (*(int32_t *)&g_7b32c) = (*(int32_t *)&DAT_10ac5b48);
    (*(int32_t *)&g_7b328) = (*(int32_t *)&DAT_10ac5b4c);
    (*(int32_t *)&g_BrCtrlCfg.active) = (*(int32_t *)&DAT_10ac5b50);

    /* `dec/je` three times: 1, 2, 3 select records 1, 2, 3 and EVERYTHING
     * else -- including 0 -- selects record 0. A SWITCH, not an if-else-if
     * chain: the chain compares (`cmp eax,1`) and puts the default last,
     * the switch decrements and puts the default FIRST, which is the
     * original's layout.
     *
     * SOLVED, and the old note here said how: the two bytes ARE separate
     * globals.  Spelled as one array VC5 merged them into a single dword load
     * and took byte 1 out of `ah`; as DAT_10ac5a4c / DAT_10ac5a4d it emits the
     * original's two independent byte loads.  Same story twice more below --
     * the 0x10AA27E0 high half is its own 16-bit global, not a shift, and the
     * halfword base is a POINTER so VC5 folds the array address into the
     * lea's displacement instead of adding it separately.  Together: 71/71
     * instructions and a register-blind gap of 0+0, from 5+4.
     *
     * WHAT IS LEFT is 288 bytes against 289 with RAW 15+15 and REGNORM 0+0 --
     * the instruction multiset is IDENTICAL and only the schedule and the
     * register naming differ, so the one-byte gap is an encoding-length
     * difference from that naming (an accumulator form where the original
     * uses a general register or the reverse).  The visible symptom is at the
     * very top: the original stores the byte third (`al` load, block-base
     * load, byte store) where we hoist a third dword load ahead of the store.
     * T3a -- do not grind.
     * DEAD 2026-09-12 (fn.py, do not re-run): the seven head assignments in
     * five other statement orders (the block-base store second, first,
     * last, the byte store second, the block-base store fifth) -- every one
     * 288 B at register-blind 1+1; VC5 reschedules the seven stores to the
     * same 1,6,4,5,2,3,7 order whatever the source order. */
    v = (*(int32_t *)&DAT_10ac5b50);
    switch (v) {
    case 1:
        (*(void * *)&g_BrPadModeBytes) = &(*(unsigned char (*)[4][168])&g_BrCtrlCfg)[1];
        break;
    case 2:
        (*(void * *)&g_BrPadModeBytes) = &(*(unsigned char (*)[4][168])&g_BrCtrlCfg)[2];
        break;
    case 3:
        (*(void * *)&g_BrPadModeBytes) = &(*(unsigned char (*)[4][168])&g_BrCtrlCfg)[3];
        break;
    default:
        (*(void * *)&g_BrPadModeBytes) = &(*(unsigned char (*)[4][168])&g_BrCtrlCfg)[0];
        break;
    }

    /* Both counters are printed PLUS ONE. */
    /* THIS site calls MSVCRT's imported sprintf (0x118F0570), not the
     * in-DLL BrSprintf at 0x1007C830 -- two calls, so VC5 caches the import
     * pointer in esi and issues `call esi` twice. And the format is the
     * literal, not the `g_pszBr0A73C4` pointer variable: the original pushes
     * the string's address as an immediate, which a pointer read cannot be. */
    sprintf(g_aBrAA2518, "%d", (*(int32_t *)&g_brPhase5BF8) + 1);
    sprintf(DAT_10ac46a0, "%d", (*(int32_t *)&g_brIdx5BFC) + 1);

    (*(int32_t *)&g_brIdx5C04) = (*(int32_t *)&g_brIdx5BFC);

    if (a != 0) {
        /* movsx: the index byte is SIGNED, so a byte >= 0x80 indexes
         * BACKWARDS off the front of the block. Faithfully reproduced. */
        /* A POINTER, not an int offset: the original folds the block's
         * address into the lea displacement (`lea eax,[eax*8+0x10ac5a66]`),
         * which it can only do if the base is part of the address
         * expression.  An int `base` added to the array afterwards costs a
         * second lea. */
        const unsigned char *pHw = (const unsigned char *)(*(int32_t (*)[83])&(DAT_10ac5a48[0]))
                                 + (int32_t)(*(int8_t *)&DAT_10ac5c10) * BR63_AA270E_STRIDE
                                 + BR63_AA270E_OFF;
        uint32_t sum  = 0;
        int      i;

        /* DEVIATION: the halfwords are read with memcpy from a byte view of
         * the 0x10AA26F0 block because 0x10AA270E is not 4-byte aligned, so a
         * uint16_t * into an int32_t array would be misaligned. */
        for (i = 0; i < BR63_AA270E_TERMS; ++i) {
            uint16_t hw;
            memcpy(&hw, pHw + i * (int)sizeof hw, sizeof hw);
            sum += hw;
        }
        DAT_10ac5c1c = (int32_t)sum;
    }

    /* GOTCHA: 0x10AA2A10 takes the LOW half of the 0x10AA27E0 dword and
     * 0x10AA2A14 the HIGH half; both are OR-ed IN, not assigned.  The HIGH
     * half is read as its OWN 16-bit global (`mov dx, word ptr [0x10ac5b3a]`,
     * i.e. the dword's address + 2), not as a shift of the dword -- a `>> 16`
     * costs a shr and a register copy and misses the `xor`. */
    (*(int32_t *)&DAT_10ac5d68) |= (int32_t)((*(uint32_t *)&DAT_10ac5b38) & 0xFFFFu);
    (*(int32_t *)&DAT_10ac5d6c) |= (int32_t)(*(uint16_t *)((char *)&DAT_10ac5b38 + 0x2));
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* the menu's picks (g_br094354..5C)   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* the live race settings              */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* g_brAA28A4                          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* g_brB4E1D0                          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* g_brAA26F0                          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* g_aBrAA26F4[0]                      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* g_aBrAA26F4[1]                      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* g_br0AA010, the game mode           */
/* 64-bit core: declared once, in br_globals.h or its struct's header *//* g_aBr0B3820: [level][pick][2]  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* g_br0B380C                          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* g_br22B350                          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* the low mask word                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* the high mask word                  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* g_brAA2A10                          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* g_brAA2A14                          */

/* WHAT IT DOES: commit the menu's choices into the live race settings --
 * copies the three picked values and the two-byte difficulty pair across,
 * looks the pair up in the 12-wide byte table to set the two derived
 * globals (single-player mode only), and merges the two selection mask
 * words into the accumulated masks. The Glide twin of the tail of
 * slice5_63.c's menu function. Fields are written in ADDRESS order
 * (0x10ac5a48 group, then 0x10ac5b44 group) and the pair table is a
 * [level][pick][2] byte array -- both decide the register schedule. */
/* @implements 0x10058A30 glide BrRaceSettingsCommit */
void BrRaceSettingsCommit(void)
{
  (DAT_10ac5a48[0]) = g_brPhase5BF8;
  (g_aBrAA26F4[0]) = (unsigned char)DAT_10ac5c10;
  (g_aBrAA26F4[1]) = (unsigned char)g_brIdx5BFC;
  DAT_10ac5b44 = g_7b324;
  DAT_10ac5b48 = g_7b32c;
  DAT_10ac5b4c = g_7b328;
  DAT_10ac5b50 = (*(int *)&g_BrCtrlCfg.active);
  if ((*(int *)&g_brRaceRules.mode) == 0) {
    (*(int *)&g_Br0B380C) = (*(unsigned char (*)[][12][2])&g_aBr0B3820)[(g_aBrAA26F4[0])][(g_aBrAA26F4[1])][0];
    g_226e80 = (*(unsigned char (*)[][12][2])&g_aBr0B3820)[(g_aBrAA26F4[0])][(g_aBrAA26F4[1])][1];
  }
  DAT_10ac5d68 |= DAT_10ac5b38 & 0xffff;
  DAT_10ac5d6c |= (*(uint16_t *)((char *)&DAT_10ac5b38 + 0x2));
}
