/* br_cardamage.c -- per-lap car damage staging (0x1001CA30).
 *
 * Fresh transcription from build/ghidra_decomp/0x1001ca30.c against the
 * original bytes, 2026-09-13.  Matching arm only.
 */
#ifdef BR_MATCHING_BUILD

/* One 0x18-byte damage-stage record at 0x100B301C, indexed by the car's
 * current stage byte (+4).  Only three fields are read here. */
typedef struct BrDmgStage {
    int            ticks;       /* +0x00  tick count that closes the stage */
    int            minSum;      /* +0x04  impact sum needed to advance     */
    unsigned char  pad08[2];
    unsigned short flag;        /* +0x0A  OR'd into the car's +0xF0 word   */
    unsigned char  pad0c[0x0C];
} BrDmgStage;

/* Two-byte records at 0x100B3028, indexed by tick + stage * 12. */
typedef struct BrDmgBit {
    unsigned char lo;           /* +0 -> bit for the +0xF2 word */
    unsigned char hi;           /* +1 -> bit for the +0xF4 word */
} BrDmgBit;

extern int        DAT_100a9360;
extern int        DAT_105ccb60;
extern int        DAT_100b3858;
extern int        DAT_100b3094;
extern int        DAT_10af2094;          /* first entrant's car pointer; 0x2B68 stride */
extern BrDmgStage DAT_100b301c[];
extern BrDmgBit   DAT_100b3028[];

/* WHAT IT DOES: advances every car's damage bookkeeping by one tick. Each car
 * keeps a tick counter and a damage stage; when the counter runs out the four
 * impact totals for that stage are summed and, if they reach the stage's
 * threshold, the stage's flag is set and the car moves to the next stage.
 * Reaching the last stage with nothing left to repair rolls the car's state
 * on, setting the matching status bits. Finally the current tick/stage pair
 * lights one bit in each of two status words, the first offset by six when
 * the car's state has its low bit set. */
/* T2 2026-09-13 (fresh transcription, 27 fn.py compiles): 446/450 B, 141/143
 * insns, register-blind residue 0+2.  What the bytes taught on the way:
 *   - the five state bits are ONE `or byte ptr [car+0xF1], K` each and stay
 *     separate arms only when spelled as a WORD OR with a high-byte mask
 *     (`*(unsigned short *)(car + 0xF0) |= K << 8`); a byte-typed
 *     `car[0xF1] |= K` (pointer, struct field, char, volatile, bitfield)
 *     splits into load/or/store and VC5 cross-jumps the five stores;
 *   - the repair scan keeps BOTH a pointer and a counter only when the
 *     pointer is initialised FIRST in the for-init (`pb = car + 0x1A, j = 0`);
 *     j-first, a do-while, or a while fold the pointer into [car+j+0x1A];
 *   - `test byte ptr [car], 1` with the +0xF2 word loaded INSIDE each arm,
 *     not before the test (a hoisted `w` load turns the test into mov/test).
 * RESIDUE (two unpaired `mov R,R`, one hoisted `mov edi,1`): the tail's
 * two-byte table index.  The original widens car[5] through ecx and COPIES
 * it to edx before widening car[4] in ecx again; we widen each byte in its
 * own zeroed register.  Dead (do not re-run): term order, int/uchar tick and
 * stage locals, a named idx (single and compound), a row pointer, the
 * six-term first in the shift count, declaration order.  Corpus MISS at
 * +0x14E len 6 and +0x151 len 4 -- the construct is not proven anywhere. */
/* @t4-pass 0x1001CA30 1 2026-09-13 probes 27 bytes 446 insns 141 regions 1 rows 2 census no  (hand, fn.py variants: loop init/increment orders, flag-byte OR spellings, bit-test placement, tail index spellings) */
/* @t4-pass 0x1001CA30 2 2026-09-13 probes 92 bytes 446 insns 141 regions 3 rows 2 census yes  (tools/crank.py) */
/* @t4-pass 0x1001CA30 3 2026-09-13 probes 17 bytes 446 insns 141 regions 3 rows 2 census yes  (hand, fn.py variants: 2-D bit table, unsigned index, 1u shift, car[5]-first and 12*car[4] term orders, six-term first; for-loop sum, += 1 forms, byte-pointer stride, pb bound before the for, explicit != 0 flag tests, merged head guard, q offset order, minSum operand order, hex flag literal -- 446/141/0+2 every time, the two orders that move (e02, e09) move the wrong way) */
/* @t3 0x1001CA30 2026-09-13 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 446/450 insns 141/143 rows 2+0 regions 3 oracle EQUIVALENT
 * @t3-effort passes 3 zero-movement 2 3
 * Residue: the tail's two-byte table index -- the original widens car[5]
 * through ecx and copies it to edx before widening car[4], and hoists the
 * second statement's `mov edi,1`; ours widens each byte in its own zeroed
 * register.  The dossier, the dead list and the three ledger passes are in
 * the comment block above.  Do not reopen before the end-grind (CLAUDE.md
 * rule 12). */
/* @implements 0x1001CA30 glide BrCarDamageTick */
void BrCarDamageTick(void)
{
    int  i;
    int *p;
    int  n;
    int  j;
    unsigned char *pb;

    if (DAT_100a9360 != 0)
        return;
    if (DAT_105ccb60 == 0)
        return;
    i = 0;
    if (DAT_100b3858 <= 0)
        return;

    p = &DAT_10af2094;
    do {
        unsigned char  *car = (unsigned char *)*p;
        int             sum = 0;
        int             k   = 4;
        unsigned short *q   = (unsigned short *)(car + 0x1e + car[4] * 8);

        do {
            sum += *q++;
        } while (--k != 0);

        car[5]++;
        car = (unsigned char *)*p;
        if (car[5] == DAT_100b301c[car[4]].ticks) {
            car[5] = 0;
            car = (unsigned char *)*p;
            if (sum >= DAT_100b301c[car[4]].minSum) {
                *(unsigned short *)(car + 0xf0) |= DAT_100b301c[car[4]].flag;
                car = (unsigned char *)*p;
                car[4]++;
                car = (unsigned char *)*p;
                if (car[4] == 6) {
                    unsigned short w;

                    n = DAT_100b3094;

                    for (pb = car + 0x1a, j = 0; j < n; j++, pb++) {
                        if (*pb != 0)
                            goto state;
                    }
                    if (*(int *)car & 1) {
                        w = *(unsigned short *)(car + 0xf2);
                        if (w & 0x200)
                            goto state;
                        w |= 0x200;
                    } else {
                        w = *(unsigned short *)(car + 0xf2);
                        if (w & 8)
                            goto state;
                        w |= 8;
                    }
                    *(unsigned short *)(car + 0xf2) = w;
state:
                    car = (unsigned char *)*p;
                    if (*(int *)car == 0)
                        *(unsigned short *)(car + 0xf0) |= 4 << 8;
                    else if (*(int *)car == 1)
                        *(unsigned short *)(car + 0xf0) |= 0x22 << 8;
                    else if (*(int *)car == 2)
                        *(unsigned short *)(car + 0xf0) |= 8 << 8;
                    else if (*(int *)car == 3)
                        *(unsigned short *)(car + 0xf0) |= 0x50 << 8;
                    else if (*(int *)car == 4)
                        *(unsigned short *)(car + 0xf0) |= 0x80 << 8;
                    car = (unsigned char *)*p;
                    car[4] = 0;
                    car = (unsigned char *)*p;
                    (*(int *)car)++;
                }
            }
        }

        car = (unsigned char *)*p;
        *(unsigned short *)(car + 0xf2) |=
            (unsigned short)(1 << (DAT_100b3028[car[4] * 12 + car[5]].lo
                                   + ((*(int *)car & 1) ? 6 : 0)));
        car = (unsigned char *)*p;
        *(unsigned short *)(car + 0xf4) |=
            (unsigned short)(1 << DAT_100b3028[car[4] * 12 + car[5]].hi);

        i++;
        p += 0xada;
    } while (i < DAT_100b3858);
}

#endif /* BR_MATCHING_BUILD */
