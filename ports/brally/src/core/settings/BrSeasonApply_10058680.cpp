/* WHAT IT DOES: brings the season save block into the live race settings
 * before a race: copies the whole block into its working mirror the first
 * time through, copies the chosen track, car, class and lap options into
 * the settings globals, builds the two "N" strings the setup screens show,
 * totals the four score words of the current entrant, and then decides
 * whether the season can go on. If it can, both cars' per-stage damage
 * records for the new stage are wiped and 1 is returned; otherwise the race
 * mode is set to the season-over value, the end-of-season handler runs, the
 * front end is told to leave, and 0 comes back. */
/* @t3 0x10058680 2026-09-16 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 624/629 insns 167/167 rows 1+1 regions 5 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * 2026-09-19: the two constant-folded exits (`return ret`/`return z`) were
 * merged into a single `done:` exit so the result is not a compile-time
 * constant at the return -- VC5 now holds it in a register as the original
 * does (`mov eax,edi` tail, was a folded `mov eax,1`): 630 -> 624 B, and the
 * body FITS its 629 B image slot.  Re-proven A5 EQUIVALENT (64 seeds) after
 * the respell.
 * Insn gap 0: every instruction is present, and divergence (key 8) reports 0
 * unpaired rows -- every diff pairs as a register rename. Residue is which
 * global lands in which register in the head (the two byte temps swap dl/cl,
 * the +0x1E word loads into dx not cx), the `mov edx,5` placement, and the
 * tail vcall's vtable pointer in eax not edx. That is register allocation,
 * undirectable from C; see the @t4-pass ledger below. Behaviourally proven: the A5 oracle
 * returns EQUIVALENT on 64 valid-state seeds (return + globals + side effects
 * agree), and byte-shape independently shows insn gap 0 / colouring only. Do
 * not reopen before the end-grind. */
/* @implements 0x10058680 glide BrSeasonApply
 * @cpp_kind free
 * @cpp_symbol _BrSeasonApply
 *
 * cdecl, no args, `ret`, 629 B.  Lives in the C++ lane because the tail's
 * front-end call is a thiscall vcall with ONE pushed argument
 * (`push ebx; mov ecx,[g]; mov edx,[ecx]; call [edx+0x18]`), which the C
 * lane cannot spell.
 *
 * T2 2026-09-13 (fresh, 13 cpp probes): 629/629 B, every instruction
 * present, register-blind residue about 8 rows.  What the bytes taught:
 *   - the entrant block at 0x10AC5A4C is ONE object (stage word, the +2
 *     bytes, the +0x1A words, the +0x4C dwords): spelled as separate
 *     globals VC5 CSEs the `& 0xff` index across the three stores, the
 *     original re-reads and re-masks it every time;
 *   - the car stores are Ghidra-literal int arithmetic
 *     (`i + car[4]*4 + 6 + car`), which keeps `lea [i + stage*4]` before
 *     the base is added; an `unsigned char *car` folds the base first;
 *   - the score sum is a local stored ONCE after the loop; a named zero
 *     `z` (compared and stored) is what makes the byte tests `cmp cl,bl`
 *     and the stage-word test `cmp ch,bl` (through a union view of the
 *     dword); the two entrant tests are UNSIGNED `>` (`ja`/`jbe`).
 * RESIDUE: the head's two byte temps swap dl/cl and the +0x1E word load
 * lands in dx not cx; `mov edx,5` sits before the mode load instead of
 * after; the tail vcall's vtable pointer is eax not edx; and the "leave
 * alone" return is a folded `mov eax,1` where the original keeps the 1 in
 * edi from the first test (`mov edi,1` between `cmp ch,bl` and its jne).
 * Dead: ret assigned at the top, `hi`/`next` temps, byte compares via
 * shifts/masks (sar/test or test ch,0xff), separate car locals.
 *
 * @t4-pass 0x10058680 1 2026-09-13 probes 13 bytes 630 insns 167 regions 5 rows 2 census no  (first transcription, 13 cpp probes: entrant block as one object vs separate globals; Ghidra-literal car arithmetic vs unsigned char* base; score sum as a once-stored local; named zero z driving the byte/stage compares; unsigned > entrant tests. Every instruction present; residue is register colouring.)
 * @t4-pass 0x10058680 2 2026-09-16 probes 12 bytes 630 insns 167 regions 5 rows 2 census yes  (confirming sweep -- #pragma intrinsic, #pragma optimize speed, declaration/rename levers -- none moves the 5 regions / 2 rows; divergence at key 8 shows 0 unpaired rows: every diff pairs as a register rename (dl/cl swap, the +0x1E word into dx not cx, the vtable pointer in eax not edx, the folded mov eax,1), so the residue is which-global-into-which-register allocation, not missing or wrong code. Numbers unmoved.)
 */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include "br_race.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include <stdio.h>

class Ui5C5C {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual void s6(int);       /* +0x18 leave the front end */
    char pad[0x64];
    int  f68;                   /* +0x68 */
};

extern "C" {
/* 64-bit core: DAT_1021c650 is defined once, in br_globals.c */
/* 64-bit core: DAT_1021c654 is defined once, in br_globals.c */
/* 64-bit core: DAT_1021c658 is defined once, in br_globals.c */
/* 64-bit core: DAT_1021c65c is defined once, in br_globals.c */
/* 64-bit core: DAT_1021c660 is defined once, in br_globals.c */
/* 64-bit core: DAT_1021c664 is defined once, in br_globals.c */
/* 64-bit core: DAT_1021c668 is defined once, in br_globals.c */
/* 64-bit core: DAT_1021c66c is defined once, in br_globals.c */
/* 64-bit core: DAT_1021c670 is defined once, in br_globals.c */
/* 64-bit core: DAT_1021c674 is defined once, in br_globals.c */
/* 64-bit core: DAT_1021c678 is defined once, in br_globals.c */
/* 64-bit core: DAT_10ac5928 is defined once, in br_globals.c */
/* 64-bit core: DAT_10ac58f8 is defined once, in br_globals.c */
/* 64-bit core: DAT_10226e80 is defined once, in br_globals.c */
/* 64-bit core: DAT_10ac5c10 is defined once, in br_globals.c */
/* 64-bit core: DAT_10ac5bf8 is defined once, in br_globals.c */
/* 64-bit core: DAT_100b3014 is defined once, in br_globals.c */
/* 64-bit core: DAT_10ac5c18 is defined once, in br_globals.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: DAT_10ac5bfc is defined once, in br_globals.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: DAT_10ac5c20 is defined once, in br_globals.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: DAT_10ac5c1c is defined once, in br_globals.c */
/* 64-bit core: DAT_100a9360 is defined once, in br_globals.c */
/* 64-bit core: DAT_10ac4c64 is defined once, in br_globals.c */
/* 64-bit core: DAT_10ac5c0c is defined once, in br_globals.c */
/* 64-bit core: DAT_10ac5c08 is defined once, in br_globals.c */
/* 64-bit core: DAT_10af2094 is defined once, in br_globals.c */
/* 64-bit core: DAT_10af4bfc is defined once, in br_globals.c */
/* 64-bit core: DAT_10ac5c5c is defined once, in br_globals.c */
/* BrExt_1005FBC0: prototype in br_funcs.h */

int BrSeasonApply(void)
{
    int  *src;
    int  *dst;
    int   n;
    int   stage;
    int   i;
    int   sum;
    int   ret;
    int   z;
    unsigned short *w;
    unsigned char   cur;
    union { int i; unsigned char b[4]; } st;

    z = 0;
    ret = 1;
    if ((*(int *)&g_a220B20) == z) {
        src = (int *)(&(*(int *)&g_a220B20));
        dst = &DAT_10ac5928;
        for (n = 0x46; n != 0; n--) {
            *dst++ = *src++;
        }
    }
    g_226e80 = (*(int *)((char *)&g_a220B20 + 0x14));
    (*(char *)&DAT_10ac5c10) = (char)(*(int *)((char *)&g_a220B20 + 0x4));
    g_brPhase5BF8 = (*(int *)((char *)&g_a220B20 + 0xC));
    (*(int *)&g_Br0B380C) = (*(int *)((char *)&g_a220B20 + 0x10));
    DAT_10ac5c18 = (*(int *)((char *)&g_a220B20 + 0x18));
    g_brVal5A40[(*(int *)((char *)&g_a220B20 + 0x8))] = (char)(*(int *)((char *)&g_a220B20 + 0x1C)) + 1;
    g_brIdx5BFC = (*(int *)((char *)&g_a220B20 + 0x8));
    (*(unsigned short (*)[])&g_brVal40F8)[g_brIdx5BFC] = (*(unsigned short *)((char *)&g_a220B20 + 0x20));
    (*(int *)&g_brTime5C20) = (*(int *)((char *)&g_a220B20 + 0x24));
    sprintf(g_aBrAA2518, g_szBrFmt6B84, (*(int *)((char *)&g_a220B20 + 0xC)) + 1);
    sprintf(DAT_10ac46a0, g_szBrFmt6B84, g_brIdx5BFC + 1);

    stage = (*(char *)&DAT_10ac5c10);
    src = &(*(int *)((char *)&g_a220B20 + 0x28));
    dst = (int *)(&(g_brFTbl58F8[0]));
    for (n = 0xc; n != 0; n--) {
        *dst++ = *src++;
    }

    w = (unsigned short *)((*(unsigned char (*)[])&g_aBrAA26F4) + 0x1a) + stage * 4;
    sum = 0;
    n = 4;
    do {
        sum += *w++;
        n--;
    } while (n != 0);
    (*(int *)&DAT_10ac5c1c) = sum;

    st.i = *(int *)(*(unsigned char (*)[])&g_aBrAA26F4);
    if (st.b[1] == z && (*(int *)&g_brRaceRules.mode) != 5 && (*(int *)&g_a220B20) == z) {
        cur = st.b[0];
        if (cur <= (*(unsigned char *)&g_aBrA9DBD8[1]) && (cur != z || !((*(unsigned char *)&g_aBrA9DBD8[1]) > (unsigned char)z))) {
            (*(int *)&g_brRaceRules.mode) = z;
            DAT_10ac5c0c = ret;
            for (i = 0; i < 4; i++) {
                (*(unsigned char (*)[])&g_aBrAA26F4)[i + 2 + (*(int *)(*(unsigned char (*)[])&g_aBrAA26F4) & 0xff) * 4] = z;
                *(unsigned short *)((*(unsigned char (*)[])&g_aBrAA26F4) + 0x1a + (i + (*(int *)(*(unsigned char (*)[])&g_aBrAA26F4) & 0xff) * 4) * 2) = z;
                *(int *)((*(unsigned char (*)[])&g_aBrAA26F4) + 0x4c + (i + (*(int *)(*(unsigned char (*)[])&g_aBrAA26F4) & 0xff) * 4) * 4) = z;
                *(unsigned char *)(i + *(unsigned char *)((((intptr_t)(g_aBrRaceCar[0].pEquip))) + 4) * 4 + 6 + (((intptr_t)(g_aBrRaceCar[0].pEquip)))) = z;
                *(unsigned char *)(i + *(unsigned char *)((((intptr_t)(g_aBrRaceCar[1].pEquip))) + 4) * 4 + 6 + (((intptr_t)(g_aBrRaceCar[1].pEquip)))) = z;
                *(unsigned short *)((((intptr_t)(g_aBrRaceCar[0].pEquip))) + 0x1e + (i + *(unsigned char *)((((intptr_t)(g_aBrRaceCar[0].pEquip))) + 4) * 4) * 2) = z;
                *(unsigned short *)((((intptr_t)(g_aBrRaceCar[1].pEquip))) + 0x1e + (i + *(unsigned char *)((((intptr_t)(g_aBrRaceCar[1].pEquip))) + 4) * 4) * 2) = z;
                *(int *)((((intptr_t)(g_aBrRaceCar[0].pEquip))) + (i + 0x14 + *(unsigned char *)((((intptr_t)(g_aBrRaceCar[0].pEquip))) + 4) * 4) * 4) = z;
                *(int *)((((intptr_t)(g_aBrRaceCar[1].pEquip))) + (i + 0x14 + *(unsigned char *)((((intptr_t)(g_aBrRaceCar[1].pEquip))) + 4) * 4) * 4) = z;
            }
            goto done;
        }
        (*(int *)&g_brRaceRules.mode) = 5;
        if (cur == z && (*(unsigned char *)&g_aBrA9DBD8[1]) > (unsigned char)z) {
            if ((*(int *)((char *)&g_a220B20 + 0xC)) < 5)
                DAT_10ac5c08 = ret;
            BrExt_1005FBC0(z);
        } else {
            BrExt_1005FBC0(ret);
        }
        if ((*(int *)((char *)&g_a220B20 + 0x4)) < 4 && (*(int *)((char *)&g_a220B20 + 0xC)) < ret)
            DAT_10ac5c08 = ret;
        (*(int *)&((BrPhase_ *)((*(Ui5C5C * *)&g_brPAA29B8)))->f68) = z;
        (*(Ui5C5C * *)&g_brPAA29B8)->s6(z);
        ret = z;
    }
done:
    return ret;
}
}
