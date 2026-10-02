/* WHAT IT DOES: scroll a slot list by one step, wrapping at the ends and
 * reporting both whether it wrapped and whether anything moved. */
/* @t3 0x100553B0 2026-09-21 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1447/1453 insns 408/408 rows 4+4 regions 5 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is three VC5 emitter choices, none of them missing or wrong code:
 * the two ratio arms' identical `fdiv [1a9d0]` tail-merged into one divide;
 * the first clamp's width comes out `fsub` where the original picked `fsubr`
 * (its own other arm uses `fsub` for the same construct); and `i1a9b4 = 1`
 * reuses the return register. The A5 oracle proves same-in/same-out. Dossier,
 * the wall-break (final-if-sense flip), and the dead list are in the header
 * comment below and the @t4-pass ledger. Do not reopen before the end-grind.
 */
/* @implements 0x100553B0 glide BrSlotScrollStep_100553B0
 * @cpp_kind method
 * @cpp_symbol ?Step@Ctl553B0@@QAEHPAH@Z
 *
 * Thiscall, one stack arg (`ret 4`), 1453 B. The per-frame step of the
 * same slot control as 0x10054A30 / 0x10054E20 / 0x10054730: run the
 * repeat timer, ask the input hook (vtable slot 6) for scroll / up / down
 * in turn, move the highlight and re-derive the two scroll anchors, and
 * otherwise walk the visible rows once looking for one to activate.
 *
 * The record array is based at `this` itself -- record n at
 * `this + n*0x438` with its own object at +0x2C -- so rows are addressed
 * off a `char *`, as in the sibling TUs.
 *
 * Four shapes worth keeping, each worth tens of bytes:
 *  - the 0x80000 test is written TWICE, once in each arm of the outer
 *    if/else, which is why the bytes test it again after the first jump;
 *  - the ratio computed at the top stays on the x87 stack across the whole
 *    clamp, so it is one float local multiplied in at the end;
 *  - the three input probes are FLAT guarded blocks, not an if/else-if
 *    chain: when a probe succeeds but 0x200000 is set the original still
 *    falls into the NEXT probe, which `else if` cannot express;
 *  - inside each probe the long arm is the `if` and the one-line arm
 *    follows it with its own return -- an `if/else` with a shared return
 *    after it duplicates the epilogue and costs 130 bytes.
 *  - the two row-rejects in the scan loop are ONE `||`; writing them as
 *    two `if`s emits the `p[0] = 1` block twice.
 *
 * A fifth shape breaks what was logged as a "cross-jumping wall": the
 * final tail is `if (bAny == 0) { clear bits; return 0; } return 1;`,
 * NOT `if (bAny) return 1; ...; return 0;`. The original's last physical
 * block is the shared return-1 at 0x1005594e (the `<60` timer path and
 * the end-of-scan path both jump forward to it); the return-0 sites stay
 * inline. Writing `return 0` last made VC5 pick return-0 as the shared
 * tail and duplicate the return-1s -- the mirror image. Flipping the
 * sense so `return 1` is physically last flips every one of those
 * merge/duplicate decisions to match: the prologue and all three early
 * return sites then go byte-exact (first diff was +0x0, now +0x82), and
 * the register-blind instruction gap drops to 0.
 *
 * CERTIFIED T3 (rule 12): the A5 oracle proves same-in/same-out and
 * gates 0+A pass. The residue is three VC5 emitter choices, not missing
 * or wrong code, all confirmed a floor under 12 spelling probes and 12
 * flag probes:
 *  - the two ratio arms both end in `fdiv [esi+1a9d0]`; VC5 tail-merges
 *    them into one divide after the reconverge and slides the i1a9b8 load
 *    into the gap, where the original divides in each arm. A per-arm
 *    `float dv = f1a9d0;` local does force two divides (raw bytes
 *    1072->683) BUT trades the merge for an `fld dv; fdivr 1.0` operand
 *    role in the reciprocal arm, pushing the register-blind rows to 11
 *    (> the Gate-A limit) -- worse for certification, so not taken;
 *  - the first clamp's width `(f1a9ac - f1a9c0)` comes out `fld ac;
 *    fsub c0` where the original has `fld c0; fsubr ac`. The original's
 *    OTHER arm uses the plain `fsub` for the same construct, so this is
 *    an emitter choice, not a spelling (mul-first, a temp, and a negated
 *    reverse all leave it or make it worse);
 *  - `i1a9b4 = 1` reuses the return-value register (`mov eax,1; mov
 *    [..],eax`) where the original stores the immediate then loads eax=1.
 *    Storing before the pfn10 call would cut it but reorders the store
 *    across the callback -- the original stores AFTER the call, so the
 *    faithful order keeps the reuse.
 * DEAD (unmoved): a ternary for the ratio, mul-first / temp / negated
 * spellings of the width, a result-var for the store, and flags /O2 /Ob0,
 * /Ox, /O2 /Gy, /O2 /Ot, /Ob1, /Og /Oy (all identical or worse); /O2 /Op
 * is +192, /O1 /Os /Oxs shrink and diverge.
 *
 * @t4-pass 0x100553B0 1 2026-09-21 probes 12 bytes 1447 insns 408 regions 5 rows 8 census no  (spelling sweep on the three residue causes: fdiv-merge via compound-assign / per-arm-divisor-local / numerator-hoist, fsubr via mul-first / temp / negated-reverse, and the i1a9b4=1 store via a result-var. The per-arm-divisor-local cut raw bytes 1072->683 but broke Gate A2, and store-before-call reorders across the pfn10 callback; neither is a faithful floor. The faithful residue is unmoved.)
 * @t4-pass 0x100553B0 2 2026-09-21 probes 12 bytes 1447 insns 408 regions 5 rows 8 census yes  (flag mechanism sweep: /O2 /Ob0, /Ox, /O2 /Op, /Og /Oy, /O1, /O2 /Os, /O2 /Ot, /Oxs, /O2 /Gy, /O2 /Ob1, /O2 /Oy /Ob2, /Oy /Ot /Og /Oi -- every one leaves the register-blind residue at 408 insns / 5 regions / 8 rows or diverges further. The A5 oracle proves same-in/same-out; the residue is register allocation plus the two x87 operand-role picks above. Numbers unmoved from pass 1.)
 */
#define _CRTIMP __declspec(dllimport)
#include "slice3_39.h"   /* br_globals: its objects */
#include <string.h>

struct BrPad553B0 {
    char pad00[0x2C];
    int  f2C;
    int  f30;
};

struct BrTime553B0 {
    int f00;
    int f04;
    char pad08[0x10 - 0x08];
    int f10;
};

class Ctl553B0 {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual int  s6(int *pOut);     /* +0x18 */

    void  (*pfn04)(void *pThis, int *pOut);   /* +0x04 */
    void  (*pfn08)(void);                     /* +0x08 */
    void  (*pfn0C)(void);                     /* +0x0C */
    void  (*pfn10)(void);                     /* +0x10 */
    char           pad014[0x18 - 0x14];
    int            i18;                       /* +0x18 */
    char           pad01C[0x1A92C - 0x1C];
    unsigned short wCount;                    /* +0x1A92C */
    short          w1a92e;
    unsigned short w1a930;
    char           pad1a932[0x1A94C - 0x1A932];
    int            i1a94c;
    char           pad1a950[0x1A95C - 0x1A950];
    int            i1a95c;
    char           pad1a960[0x1A98C - 0x1A960];
    int            i1a98c;
    int            i1a990;
    int            i1a994;
    int            i1a998;
    int            i1a99c;
    int            i1a9a0;
    char           pad1a9a4[0x1A9AC - 0x1A9A4];
    float          f1a9ac;
    float          f1a9b0;
    int            i1a9b4;
    int            i1a9b8;
    int            i1a9bc;
    float          f1a9c0;
    float          f1a9c4;
    float          f1a9c8;
    float          f1a9cc;
    float          f1a9d0;

    int Step(int *pArg);
};








extern "C" {
/* 64-bit core: g_pBrAC61E0 is defined once, in br_globals.c */
/* 64-bit core: g_pBrAC5DD8 is defined once, in br_globals.c */
/* 64-bit core: g_brAC5DB4 is defined once, in br_globals.c */
/* 64-bit core: g_brAC5DB8 is defined once, in br_globals.c */
/* 64-bit core: g_brAC5DBC is defined once, in br_globals.c */
/* 64-bit core: g_brAC5DC0 is defined once, in br_globals.c */
/* 64-bit core: g_brAC5BAC is defined once, in br_globals.c */
/* BrFn1006E280: prototype in br_funcs.h */
/* BrFn10037720: prototype in br_funcs.h */
/* BrFn10037710: prototype in br_funcs.h */
/* BrFn1006BA60: prototype in br_funcs.h */
}

#define BR_SLOT 0x438

/* 2026-09-21: insn gap 0, rows 4+4, oracle EQUIVALENT. The 2026-09-13
 * "cross-jumping wall" was a layout artefact of writing `return 0` last:
 * the final `if (bAny == 0) { ...; return 0; } return 1;` below puts the
 * shared return-1 physically last, matching the original, and every early
 * return then goes byte-exact (prologue through +0x82). What remains is
 * register allocation plus the fdiv tail-merge, the fsub/fsubr operand
 * role, and the i1a9b4=1 register reuse -- see the file-header ledger. */
int Ctl553B0::Step(int *pArg)
{
    int            bWrapped;
    int            bAny;
    float          ratio;
    unsigned short d;
    int            i;
    int            iEnd;
    char          *p;

    bWrapped = 0;

    if (((*(int *)&((BrTextList *)(this))->f18) & 0x18) != 0)
        return 0;

    if (((*(int *)&((BrTextList *)(this))->f18) & 0x80000) != 0 && (*(BrPad553B0 * *)&g_pBrAA2E80)->f2C == 0 && (*(BrPad553B0 * *)&g_pBrAA2E80)->f30 == 0) {
        (*(int *)&((BrTextList *)(this))->f1A99C[6]) = 0;
        (*(int *)&((BrTextList *)(this))->f18) = (*(int *)&((BrTextList *)(this))->f18) & 0xFFF7FFFD;
    } else if (((*(int *)&((BrTextList *)(this))->f18) & 0x80000) != 0
               && ((*(BrPad553B0 * *)&g_pBrAA2E80)->f2C != 0 || (*(BrPad553B0 * *)&g_pBrAA2E80)->f30 != 0)) {
        int now;

        (*(int *)&((BrTextList *)(this))->f18) |= 0x22;
        now = BrSub10075020();
        g_brAC5DB4 = g_brAC5DB4 + (now - g_brAC5DB8);
        g_brAC5DB8 = now;
        if (g_brAC5DB4 < 60)
            return 1;
        g_brAC5DB4 = 0;
        bWrapped = 1;
    }

    if ((s6(&(*(int *)&((BrTextList *)(this))->f1A98C)) != 0 || (*(int *)&((BrTextList *)(this))->f1A99C[6]) != 0) && ((*(int *)&((BrTextList *)(this))->f18) & 0x200000) == 0) {
        if (((*(int *)&((BrTextList *)(this))->f18) & 2) == 0)
            return 1;

            if ((*(unsigned short *)&((BrTextList *)(this))->count) <= 1)
                ratio = 1.0f / (*(float *)&((BrTextList *)(this))->f1A99C[13]);
            else
                ratio = (float)((*(unsigned short *)&((BrTextList *)(this))->count) - 1) / (*(float *)&((BrTextList *)(this))->f1A99C[13]);

            if ((*(int *)&((BrTextList *)(this))->f1A99C[7]) != 0) {
                float v = (float)((*(BrTime553B0 * *)&BrGlNavThis5DD8)->f00 - (*(int *)&((BrTextList *)(this))->f1A98C));

                if ((*(int *)&((BrTextList *)(this))->f1A99C[6]) == 0)
                    g_brAC5DBC = v;
                (*(float *)&((BrTextList *)(this))->f1A99C[4]) = v + (float)(*(BrTime553B0 * *)&BrGlNavThis5DD8)->f00 - g_brAC5DBC;
                if ((*(float *)&((BrTextList *)(this))->f1A99C[4]) < (*(float *)&((BrTextList *)(this))->f1A99C[9]))
                    (*(float *)&((BrTextList *)(this))->f1A99C[4]) = (*(float *)&((BrTextList *)(this))->f1A99C[9]);
                else if ((*(float *)&((BrTextList *)(this))->f1A99C[4]) > (*(float *)&((BrTextList *)(this))->f1A99C[10]))
                    (*(float *)&((BrTextList *)(this))->f1A99C[4]) = (*(float *)&((BrTextList *)(this))->f1A99C[10]);
                (*(int *)&((BrTextList *)(this))->f1A98C) = (int)(*(float *)&((BrTextList *)(this))->f1A99C[4]);
                (*(int *)&((BrTextList *)(this))->f1A994) = (int)(*(float *)&((BrTextList *)(this))->f1A99C[4]) + 0x10;
                if ((int)(*(unsigned short *)&((BrTextList *)(this))->count) - 1 > 0)
                    (*(short *)&((BrTextList *)(this))->f1A92E) = (short)(int)(((*(float *)&((BrTextList *)(this))->f1A99C[4]) - (*(float *)&((BrTextList *)(this))->f1A99C[9])) * ratio);
            } else if ((*(int *)&((BrTextList *)(this))->f1A99C[8]) != 0) {
                float w = (float)(*(BrTime553B0 * *)&BrGlNavThis5DD8)->f04 - (float)(*(BrTime553B0 * *)&BrGlNavThis5DD8)->f10;

                if ((*(int *)&((BrTextList *)(this))->f1A99C[6]) == 0)
                    g_brAC5DC0 = (float)(*(BrTime553B0 * *)&BrGlNavThis5DD8)->f04 - (float)(*(int *)&((BrTextList *)(this))->f1A990);
                (*(float *)&((BrTextList *)(this))->f1A99C[5]) = w + (float)(*(BrTime553B0 * *)&BrGlNavThis5DD8)->f04 - g_brAC5DC0;
                if ((*(float *)&((BrTextList *)(this))->f1A99C[5]) < (*(float *)&((BrTextList *)(this))->f1A99C[11]))
                    (*(float *)&((BrTextList *)(this))->f1A99C[5]) = (*(float *)&((BrTextList *)(this))->f1A99C[11]);
                else if ((*(float *)&((BrTextList *)(this))->f1A99C[5]) > (*(float *)&((BrTextList *)(this))->f1A99C[12]))
                    (*(float *)&((BrTextList *)(this))->f1A99C[5]) = (*(float *)&((BrTextList *)(this))->f1A99C[12]);
                (*(int *)&((BrTextList *)(this))->f1A990) = (int)(*(float *)&((BrTextList *)(this))->f1A99C[5]);
                (*(int *)&((BrTextList *)(this))->f1A998) = (int)(*(float *)&((BrTextList *)(this))->f1A99C[5]) + 0x10;
                if ((int)(*(unsigned short *)&((BrTextList *)(this))->count) - 1 > 0)
                    (*(short *)&((BrTextList *)(this))->f1A92E) = (short)(int)(((*(float *)&((BrTextList *)(this))->f1A99C[5]) - (*(float *)&((BrTextList *)(this))->f1A99C[11])) * ratio);
            }

        if ((*(void (**)(void))&((BrTextList *)(this))->f10) != 0)
            (*(void (**)(void))&((BrTextList *)(this))->f10)();
        (*(int *)&((BrTextList *)(this))->f1A99C[6]) = 1;
        return 1;
    }

    if (s6(&(*(int *)&((BrTextList *)(this))->f1A94C)) != 0 && ((*(int *)&((BrTextList *)(this))->f18) & 0x200000) == 0) {
        if (((*(int *)&((BrTextList *)(this))->f18) & 2) != 0) {
            (*(int *)&((BrTextList *)(this))->f1A99C[0]) = 1;
            (*(short *)&((BrTextList *)(this))->f1A92E)--;
            if ((*(short *)&((BrTextList *)(this))->f1A92E) < 0)
                (*(short *)&((BrTextList *)(this))->f1A92E) = 0;
            if ((*(void (**)(void))&((BrTextList *)(this))->f08) != 0)
                (*(void (**)(void))&((BrTextList *)(this))->f08)();

            d = (unsigned short)((*(unsigned short *)&((BrTextList *)(this))->count) - 1);
            if (d <= 0)
                d = 1;
            (*(float *)&((BrTextList *)(this))->f1A99C[5]) = (*(float *)&((BrTextList *)(this))->f1A99C[5]) - (*(float *)&((BrTextList *)(this))->f1A99C[13]) / (float)d;
            if ((*(float *)&((BrTextList *)(this))->f1A99C[5]) < (*(float *)&((BrTextList *)(this))->f1A99C[11]))
                (*(float *)&((BrTextList *)(this))->f1A99C[5]) = (*(float *)&((BrTextList *)(this))->f1A99C[11]);
            else if ((*(float *)&((BrTextList *)(this))->f1A99C[5]) > (*(float *)&((BrTextList *)(this))->f1A99C[12]))
                (*(float *)&((BrTextList *)(this))->f1A99C[5]) = (*(float *)&((BrTextList *)(this))->f1A99C[12]);
            (*(int *)&((BrTextList *)(this))->f1A990) = (int)(*(float *)&((BrTextList *)(this))->f1A99C[5]);
            (*(int *)&((BrTextList *)(this))->f1A998) = (int)(*(float *)&((BrTextList *)(this))->f1A99C[5]) + 0x10;
            return 1;
        }
        (*(int *)&((BrTextList *)(this))->f1A99C[0]) = 0;
        return 1;
    }

    if (s6(&(*(int *)&((BrTextList *)(this))->f1A95C)) != 0 && ((*(int *)&((BrTextList *)(this))->f18) & 0x200000) == 0) {
        if (((*(int *)&((BrTextList *)(this))->f18) & 2) != 0) {
            (*(short *)&((BrTextList *)(this))->f1A92E)++;
            (*(int *)&((BrTextList *)(this))->f1A99C[1]) = 1;
            if ((int)(*(short *)&((BrTextList *)(this))->f1A92E) >= (int)(*(unsigned short *)&((BrTextList *)(this))->count))
                (*(short *)&((BrTextList *)(this))->f1A92E) = (short)((*(unsigned short *)&((BrTextList *)(this))->count) - 1);
            if ((*(void (**)(void))&((BrTextList *)(this))->f0C) != 0)
                (*(void (**)(void))&((BrTextList *)(this))->f0C)();

            d = (unsigned short)((*(unsigned short *)&((BrTextList *)(this))->count) - 1);
            if (d <= 0)
                d = 1;
            (*(float *)&((BrTextList *)(this))->f1A99C[5]) = (*(float *)&((BrTextList *)(this))->f1A99C[13]) / (float)d + (*(float *)&((BrTextList *)(this))->f1A99C[5]);
            if ((*(float *)&((BrTextList *)(this))->f1A99C[5]) < (*(float *)&((BrTextList *)(this))->f1A99C[11]))
                (*(float *)&((BrTextList *)(this))->f1A99C[5]) = (*(float *)&((BrTextList *)(this))->f1A99C[11]);
            else if ((*(float *)&((BrTextList *)(this))->f1A99C[5]) > (*(float *)&((BrTextList *)(this))->f1A99C[12]))
                (*(float *)&((BrTextList *)(this))->f1A99C[5]) = (*(float *)&((BrTextList *)(this))->f1A99C[12]);
            (*(int *)&((BrTextList *)(this))->f1A990) = (int)(*(float *)&((BrTextList *)(this))->f1A99C[5]);
            (*(int *)&((BrTextList *)(this))->f1A998) = (int)(*(float *)&((BrTextList *)(this))->f1A99C[5]) + 0x10;
            return 1;
        }
        (*(int *)&((BrTextList *)(this))->f1A99C[1]) = 0;
        return 1;
    }

    bAny = 0;
    if (bWrapped != 0)
        return 0;

    iEnd = (int)(*(unsigned short *)&((BrTextList *)(this))->f1A930) + (int)(*(short *)&((BrTextList *)(this))->f1A92E);
    if (iEnd > 100)
        iEnd = 100;

    for (i = (*(short *)&((BrTextList *)(this))->f1A92E); i < iEnd; i++) {
        p = (((char *)this + ((i)) * BR_SLOT)) + 0x34;

        if ((p[-4] & 0x10) != 0) {
            p[0] = 0;
            continue;
        }
        if (s6((int *)(p + 1 + 0x41B)) == 0 || strlen(p + 1) == 0) {
            p[0] = 1;
            continue;
        }

        bAny = 1;
        if (((*(int *)&((BrTextList *)(this))->f18) & 0x1000000) != 0)
            continue;

        if (((*(int *)&((BrTextList *)(this))->f18) & 0x100) != 0) {
            switch (p[0]) {
            case 0:
                p[0] = 1;
                break;
            case 1:
                p[0] = 2;
                break;
            case 2:
                p[0] = 1;
                break;
            default:
                p[0] = 0;
                break;
            }
            (*(int *)&((BrTextList *)(this))->f18) = (*(int *)&((BrTextList *)(this))->f18) & 0xFFFFFEFF;
        }

        if (BrInputAnyActive() == 0)
            continue;
        BrFn1003E070();
        if ((p[-4] & 0x10) != 0)
            continue;

        *pArg = i;
        BrSub10072AF0(1, 0x200020);
        g_track = 1;
        if ((*(void (**)(void *, int *))&((BrTextList *)(this))->f04) != 0)
            (*(void (**)(void *, int *))&((BrTextList *)(this))->f04)(this, pArg);
    }

    if (bAny == 0) {
        (*(int *)&((BrTextList *)(this))->f18) = (*(int *)&((BrTextList *)(this))->f18) & 0xFFFFFFDD;
        return 0;
    }
    return 1;
}

/* C entry points (generated by ports/64b/tools/methodfwd.py) */
extern "C" int BrSlotScrollStep_100553B0(void *self, int * pArg)
{
    return ((class Ctl553B0 *)self)->Step(pArg);
}
/* end of C entry points */
