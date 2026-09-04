/* WHAT IT DOES: scroll a slot list by one step, wrapping at the ends and
 * reporting both whether it wrapped and whether anything moved. */
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
 * PARKED at +3 bytes with an 8-instruction register-blind gap. Five of
 * those eight are the known cross-jumping wall (docs/VC5-IDIOMS.md,
 * "our cl merges identical error tails"): the original keeps three
 * separate `xor eax,eax` return-0 epilogues and one separate return-1,
 * ours tail-merges them, which flips two `je` to `jne`, one `jl` to
 * `jge`, and turns two `xor eax,eax` into `mov eax,1`. The other three:
 * the `/f1a9d0` is factored out of the ratio's two arms (the original
 * divides in both), the first clamp's subtraction comes out `fsub` where
 * the original has `fld`+`fsubr` -- and the original's OTHER arm uses the
 * plain `fsub` for the same construct, so that one is an emitter choice,
 * not a spelling -- and `i1a9b4 = 1` reuses the return-value register.
 * DO NOT RE-PROBE: negating the subtraction (+32 bytes), a ternary for
 * the ratio, moving the i1a9b4 store, and flags /O2 /Ob0, /O2 /Gy, /Ox
 * (all identical) and /O2 /Op (+192) change nothing.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

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

typedef char chk_i18[(unsigned)&((Ctl553B0 *)0)->i18 == 0x18 ? 1 : -1];
typedef char chk_cnt[(unsigned)&((Ctl553B0 *)0)->wCount == 0x1A92C ? 1 : -1];
typedef char chk_94c[(unsigned)&((Ctl553B0 *)0)->i1a94c == 0x1A94C ? 1 : -1];
typedef char chk_98c[(unsigned)&((Ctl553B0 *)0)->i1a98c == 0x1A98C ? 1 : -1];
typedef char chk_9ac[(unsigned)&((Ctl553B0 *)0)->f1a9ac == 0x1A9AC ? 1 : -1];
typedef char chk_9d0[(unsigned)&((Ctl553B0 *)0)->f1a9d0 == 0x1A9D0 ? 1 : -1];

extern "C" {
BrPad553B0  *g_pBrAC61E0;       /* 0x10AC61E0 */
BrTime553B0 *g_pBrAC5DD8;       /* 0x10AC5DD8 */
int   g_brAC5DB4;               /* 0x10AC5DB4 */
int   g_brAC5DB8;               /* 0x10AC5DB8 */
float g_brAC5DBC;               /* 0x10AC5DBC */
float g_brAC5DC0;               /* 0x10AC5DC0 */
int   g_brAC5BAC;               /* 0x10AC5BAC */
int   BrFn1006E280(void);
int   BrFn10037720(void);
void  BrFn10037710(void);
void  BrFn1006BA60(int a, int b);
}

#define BR_SLOT 0x438
#define BR_REC(n) ((char *)this + (n) * BR_SLOT)

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

    if ((i18 & 0x18) != 0)
        return 0;

    if ((i18 & 0x80000) != 0 && g_pBrAC61E0->f2C == 0 && g_pBrAC61E0->f30 == 0) {
        i1a9b4 = 0;
        i18 = i18 & 0xFFF7FFFD;
    } else if ((i18 & 0x80000) != 0
               && (g_pBrAC61E0->f2C != 0 || g_pBrAC61E0->f30 != 0)) {
        int now;

        i18 |= 0x22;
        now = BrFn1006E280();
        g_brAC5DB4 = g_brAC5DB4 + (now - g_brAC5DB8);
        g_brAC5DB8 = now;
        if (g_brAC5DB4 < 60)
            return 1;
        g_brAC5DB4 = 0;
        bWrapped = 1;
    }

    if ((s6(&i1a98c) != 0 || i1a9b4 != 0) && (i18 & 0x200000) == 0) {
        if ((i18 & 2) == 0)
            return 1;

            if (wCount <= 1)
                ratio = 1.0f / f1a9d0;
            else
                ratio = (float)(wCount - 1) / f1a9d0;

            if (i1a9b8 != 0) {
                float v = (float)(g_pBrAC5DD8->f00 - i1a98c);

                if (i1a9b4 == 0)
                    g_brAC5DBC = v;
                f1a9ac = v + (float)g_pBrAC5DD8->f00 - g_brAC5DBC;
                if (f1a9ac < f1a9c0)
                    f1a9ac = f1a9c0;
                else if (f1a9ac > f1a9c4)
                    f1a9ac = f1a9c4;
                i1a98c = (int)f1a9ac;
                i1a994 = (int)f1a9ac + 0x10;
                if ((int)wCount - 1 > 0)
                    w1a92e = (short)(int)((f1a9ac - f1a9c0) * ratio);
            } else if (i1a9bc != 0) {
                float w = (float)g_pBrAC5DD8->f04 - (float)g_pBrAC5DD8->f10;

                if (i1a9b4 == 0)
                    g_brAC5DC0 = (float)g_pBrAC5DD8->f04 - (float)i1a990;
                f1a9b0 = w + (float)g_pBrAC5DD8->f04 - g_brAC5DC0;
                if (f1a9b0 < f1a9c8)
                    f1a9b0 = f1a9c8;
                else if (f1a9b0 > f1a9cc)
                    f1a9b0 = f1a9cc;
                i1a990 = (int)f1a9b0;
                i1a998 = (int)f1a9b0 + 0x10;
                if ((int)wCount - 1 > 0)
                    w1a92e = (short)(int)((f1a9b0 - f1a9c8) * ratio);
            }

        if (pfn10 != 0)
            pfn10();
        i1a9b4 = 1;
        return 1;
    }

    if (s6(&i1a94c) != 0 && (i18 & 0x200000) == 0) {
        if ((i18 & 2) != 0) {
            i1a99c = 1;
            w1a92e--;
            if (w1a92e < 0)
                w1a92e = 0;
            if (pfn08 != 0)
                pfn08();

            d = (unsigned short)(wCount - 1);
            if (d <= 0)
                d = 1;
            f1a9b0 = f1a9b0 - f1a9d0 / (float)d;
            if (f1a9b0 < f1a9c8)
                f1a9b0 = f1a9c8;
            else if (f1a9b0 > f1a9cc)
                f1a9b0 = f1a9cc;
            i1a990 = (int)f1a9b0;
            i1a998 = (int)f1a9b0 + 0x10;
            return 1;
        }
        i1a99c = 0;
        return 1;
    }

    if (s6(&i1a95c) != 0 && (i18 & 0x200000) == 0) {
        if ((i18 & 2) != 0) {
            w1a92e++;
            i1a9a0 = 1;
            if ((int)w1a92e >= (int)wCount)
                w1a92e = (short)(wCount - 1);
            if (pfn0C != 0)
                pfn0C();

            d = (unsigned short)(wCount - 1);
            if (d <= 0)
                d = 1;
            f1a9b0 = f1a9d0 / (float)d + f1a9b0;
            if (f1a9b0 < f1a9c8)
                f1a9b0 = f1a9c8;
            else if (f1a9b0 > f1a9cc)
                f1a9b0 = f1a9cc;
            i1a990 = (int)f1a9b0;
            i1a998 = (int)f1a9b0 + 0x10;
            return 1;
        }
        i1a9a0 = 0;
        return 1;
    }

    bAny = 0;
    if (bWrapped != 0)
        return 0;

    iEnd = (int)w1a930 + (int)w1a92e;
    if (iEnd > 100)
        iEnd = 100;

    for (i = w1a92e; i < iEnd; i++) {
        p = BR_REC(i) + 0x34;

        if ((p[-4] & 0x10) != 0) {
            p[0] = 0;
            continue;
        }
        if (s6((int *)(p + 1 + 0x41B)) == 0 || strlen(p + 1) == 0) {
            p[0] = 1;
            continue;
        }

        bAny = 1;
        if ((i18 & 0x1000000) != 0)
            continue;

        if ((i18 & 0x100) != 0) {
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
            i18 = i18 & 0xFFFFFEFF;
        }

        if (BrFn10037720() == 0)
            continue;
        BrFn10037710();
        if ((p[-4] & 0x10) != 0)
            continue;

        *pArg = i;
        BrFn1006BA60(1, 0x200020);
        g_brAC5BAC = 1;
        if (pfn04 != 0)
            pfn04(this, pArg);
    }

    if (bAny != 0)
        return 1;

    i18 = i18 & 0xFFFFFFDD;
    return 0;
}
