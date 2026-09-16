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
 * @t3-measure bytes 630/629 insns 167/167 rows 1+1 regions 5 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Insn gap 0: every instruction is present, and divergence (key 8) reports 0
 * unpaired rows -- every diff pairs as a register rename. Residue is which
 * global lands in which register in the head (the two byte temps swap dl/cl,
 * the +0x1E word loads into dx not cx), the `mov edx,5` placement, the tail
 * vcall's vtable pointer in eax not edx, and the "leave alone" return folded to
 * `mov eax,1` instead of held in edi. That is register allocation, undirectable
 * from C; see the @t4-pass ledger below. Behaviourally proven: the A5 oracle
 * returns EQUIVALENT on 64 valid-state seeds (return + globals + side effects
 * agree), and byte-shape independently shows insn gap 0 / colouring only. Do
 * not reopen before the end-grind (CLAUDE.md rule 12). */
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
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#endif

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
int           DAT_1021c650;     /* season block: 0x46 dwords */
int           DAT_1021c654;
int           DAT_1021c658;
int           DAT_1021c65c;
int           DAT_1021c660;
int           DAT_1021c664;
int           DAT_1021c668;
int           DAT_1021c66c;
unsigned short DAT_1021c670;
int           DAT_1021c674;
int           DAT_1021c678;     /* 0xc dwords */
int           DAT_10ac5928;     /* mirror of the block */
int           DAT_10ac58f8;     /* mirror of the 0xc dwords */
int           DAT_10226e80;
char          DAT_10ac5c10;
int           DAT_10ac5bf8;
int           DAT_100b3014;
int           DAT_10ac5c18;
char          DAT_10ac5a40[];
int           DAT_10ac5bfc;
unsigned short DAT_10ac40f8[];
int           DAT_10ac5c20;
char          DAT_10ac5870[];
char          DAT_10ac46a0[];
char          DAT_100a6b84[];   /* "%d" */
unsigned char DAT_10ac5a4c[];  /* entrant block: +0 stage word, +2 bytes, +0x1a words, +0x4c dwords */
int           DAT_10ac5c1c;
int           DAT_100a9360;
unsigned char DAT_10ac4c64;
int           DAT_10ac5c0c;
int           DAT_10ac5c08;
int           DAT_10af2094;    /* entrant 0's car */
int           DAT_10af4bfc;    /* entrant 1's car */
Ui5C5C       *DAT_10ac5c5c;
void BrExt_1005FBC0(int a);     /* 0x10058900 */

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
    if (DAT_1021c650 == z) {
        src = &DAT_1021c650;
        dst = &DAT_10ac5928;
        for (n = 0x46; n != 0; n--) {
            *dst++ = *src++;
        }
    }
    DAT_10226e80 = DAT_1021c664;
    DAT_10ac5c10 = (char)DAT_1021c654;
    DAT_10ac5bf8 = DAT_1021c65c;
    DAT_100b3014 = DAT_1021c660;
    DAT_10ac5c18 = DAT_1021c668;
    DAT_10ac5a40[DAT_1021c658] = (char)DAT_1021c66c + 1;
    DAT_10ac5bfc = DAT_1021c658;
    DAT_10ac40f8[DAT_10ac5bfc] = DAT_1021c670;
    DAT_10ac5c20 = DAT_1021c674;
    sprintf(DAT_10ac5870, DAT_100a6b84, DAT_1021c65c + 1);
    sprintf(DAT_10ac46a0, DAT_100a6b84, DAT_10ac5bfc + 1);

    stage = DAT_10ac5c10;
    src = &DAT_1021c678;
    dst = &DAT_10ac58f8;
    for (n = 0xc; n != 0; n--) {
        *dst++ = *src++;
    }

    w = (unsigned short *)(DAT_10ac5a4c + 0x1a) + stage * 4;
    sum = 0;
    n = 4;
    do {
        sum += *w++;
        n--;
    } while (n != 0);
    DAT_10ac5c1c = sum;

    st.i = *(int *)DAT_10ac5a4c;
    if (st.b[1] == z && DAT_100a9360 != 5 && DAT_1021c650 == z) {
        cur = st.b[0];
        if (cur <= DAT_10ac4c64 && (cur != z || !(DAT_10ac4c64 > (unsigned char)z))) {
            DAT_100a9360 = z;
            DAT_10ac5c0c = ret;
            for (i = 0; i < 4; i++) {
                DAT_10ac5a4c[i + 2 + (*(int *)DAT_10ac5a4c & 0xff) * 4] = z;
                *(unsigned short *)(DAT_10ac5a4c + 0x1a + (i + (*(int *)DAT_10ac5a4c & 0xff) * 4) * 2) = z;
                *(int *)(DAT_10ac5a4c + 0x4c + (i + (*(int *)DAT_10ac5a4c & 0xff) * 4) * 4) = z;
                *(unsigned char *)(i + *(unsigned char *)(DAT_10af2094 + 4) * 4 + 6 + DAT_10af2094) = z;
                *(unsigned char *)(i + *(unsigned char *)(DAT_10af4bfc + 4) * 4 + 6 + DAT_10af4bfc) = z;
                *(unsigned short *)(DAT_10af2094 + 0x1e + (i + *(unsigned char *)(DAT_10af2094 + 4) * 4) * 2) = z;
                *(unsigned short *)(DAT_10af4bfc + 0x1e + (i + *(unsigned char *)(DAT_10af4bfc + 4) * 4) * 2) = z;
                *(int *)(DAT_10af2094 + (i + 0x14 + *(unsigned char *)(DAT_10af2094 + 4) * 4) * 4) = z;
                *(int *)(DAT_10af4bfc + (i + 0x14 + *(unsigned char *)(DAT_10af4bfc + 4) * 4) * 4) = z;
            }
            return ret;
        }
        DAT_100a9360 = 5;
        if (cur == z && DAT_10ac4c64 > (unsigned char)z) {
            if (DAT_1021c65c < 5)
                DAT_10ac5c08 = ret;
            BrExt_1005FBC0(z);
        } else {
            BrExt_1005FBC0(ret);
        }
        if (DAT_1021c654 < 4 && DAT_1021c65c < ret)
            DAT_10ac5c08 = ret;
        DAT_10ac5c5c->f68 = z;
        DAT_10ac5c5c->s6(z);
        return z;
    }
    return ret;
}
}
