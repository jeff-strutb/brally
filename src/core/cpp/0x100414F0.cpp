/* @implements 0x100414F0 glide BrUiTickSteps_100480A0
 * @cpp_kind method
 * @cpp_symbol ?TickSteps@UiPage@@QAEHXZ
 *
 * 218 B thiscall step-timer tick. KEY SPELLING: the accumulate is
 * t-less — `f2974 += now - f2970; f2970 = now;` (scheduler emits the
 * f2970 store first). A temp form (`t = now - f2970; ... f2974 += t`)
 * makes VC5 FORWARD the stored value into the branch (mov ecx,edx)
 * where the original RE-READS f2974 — 147-diff cascade from that one
 * fork. Stepped branch is early-return style (jle/jg cross-jump to the
 * final return 1, the wrap path gets its own epilogue); both arms are
 * f1C |= then f3850 |= (the stepped arm's swapped stores are
 * scheduling). w128 is a SIGNED short here (movsx index).
 */
extern "C" {
int BrSub10075020();
}

class UiPage {
public:
    char pad00[0x1C];
    unsigned int f1C;               /* +0x1C */
    char pad20[0x108];              /* +0x20 */
    short w128;                     /* +0x128 */
    char pad12A[0x283E];            /* +0x12A */
    int f2968;                      /* +0x2968 */
    int f296C;                      /* +0x296C */
    int f2970;                      /* +0x2970 */
    int f2974;                      /* +0x2974 */
    int a2978[0x3B6];               /* +0x2978 */
    int f3850;                      /* +0x3850 */
    int TickSteps();
};

typedef char chk_3850[(unsigned)&((UiPage *)0)->f3850 == 0x3850 ? 1 : -1];

int UiPage::TickSteps()
{
    int now;

    if (f2968 == 0)
        return 1;
    now = BrSub10075020();
    f2974 += now - f2970;
    f2970 = now;
    if (f296C != 0) {
        if (f2974 <= a2978[w128])
            return 1;
        f2974 = 0;
        f1C |= 0x100;
        f3850 |= 0x100;
        ++w128;
        if (a2978[w128] > 0)
            return 1;
        w128 = 0;
        return 1;
    }
    if (f2974 > 0x3c) {
        f2974 = 0;
        f1C |= 0x100;
        f3850 |= 0x100;
    }
    return 1;
}
