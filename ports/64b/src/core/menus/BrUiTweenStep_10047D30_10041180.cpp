#include "br_ui.h"   /* BrUiCtl_, the canonical record */
/* WHAT IT DOES: advance an animated move by however much real time has
 * passed since the last frame, and report when it has arrived. Time-based
 * rather than per-frame, so the animation runs at the same speed regardless
 * of frame rate. */
/* @implements 0x10041180 glide BrUiTweenStep_10047D30
 * @cpp_kind method
 * @cpp_symbol ?Step@Tween41180@@QAEHXZ
 *
 * Thiscall, no args (`ret`), 373 B. Advance a two-axis tween by however
 * long has passed since the last call, moving each axis toward its limit
 * through the +0x28 easing vcall and clamping it there. When both axes
 * have arrived, the elapsed total and the "moving" flag are cleared.
 * Returns 1.
 *
 * The port body in slice3_32.c factors the per-axis work into a shared
 * helper; the original has it written out TWICE, once per axis, which is
 * why this TU spells both arms in full. Its accessor helpers also hide the
 * field widths -- the direction bytes are `char` and compare against -1,
 * the tick and elapsed fields are ints, and only the positions and limits
 * are floats.
 *
 * Each axis is a three-way test on its direction byte in this order:
 * -1 (down), 0 (already there), 1 (up); anything else does nothing at all,
 * not even marking the axis done. The clamp comparison differs per
 * direction -- `>=` going up, `<=` going down -- which is the `test ah,1`
 * versus `test ah,0x41` pair in the original.
 *
 * The eased value is stored to the position with `fst` (no pop) and then
 * compared, so it must be a NAMED float local: the store and the test are
 * two uses of one value.
 *
 * The three-way direction test is a SWITCH, not an if/else-if chain: the
 * original puts all three comparisons together at the top with the bodies
 * out of line after them, which is the switch layout. The chain form
 * inlines the first arm as the fall-through instead (worth 1 diff here but
 * the wrong shape, and it is the same lever that mattered on 0x1003A910).
 *
 * BYTE-EXACT 2026-09-28.  Two source facts closed the old 243-diff gap:
 *  - the elapsed update is `f382C += now - f3828;` BEFORE `f3828 = now;`.
 *    Written as `delta = now - f3828; f3828 = now; f382C = f382C + delta;`
 *    VC5 forwards the stored sum into the X axis's easing calls; the
 *    compound form makes it re-read the member there, as the original does;
 *  - each clamp arm sets its done flag BEFORE the clamp store.  With the
 *    flag last, VC5 merges every arm's `done = 1` into one tail shared with
 *    case 0 / the disabled axis; the original keeps each arm whole and
 *    shares only case 0 with the disabled path.
 */
#define _CRTIMP __declspec(dllimport)

class Tween41180 {
public:
    virtual void  s0();
    virtual void  s1();
    virtual void  s2();
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6();
    virtual void  s7();
    virtual void  s8();
    virtual void  s9();
    virtual float s10(int ms);      /* +0x28 -- easing curve */

    char  pad004[0x30 - 4];
    float f030;                     /* +0x030 start x */
    float f034;                     /* +0x034 start y */
    char  pad038[4];
    float f03C;                     /* +0x03C current x */
    float f040;                     /* +0x040 current y */
    char  pad044[0x3804 - 0x44];
    int   f3804;                    /* +0x3804 x enabled */
    int   f3808;                    /* +0x3808 y enabled */
    char  b380C;                    /* +0x380C x direction */
    char  b380D;                    /* +0x380D y direction */
    short pad380E;
    float f3810;                    /* +0x3810 x limit */
    float f3814;                    /* +0x3814 y limit */
    int   f3818;                    /* +0x3818 moving */
    char  pad381C[0x3828 - 0x381C];
    int   f3828;                    /* +0x3828 last tick */
    int   f382C;                    /* +0x382C elapsed */

    int Step();
};





extern "C" {
/* BrSub1006E280: prototype in br_funcs.h */
}

int Tween41180::Step()
{
    int doneX = 0;
    int doneY = 0;
    int now;

    if ((*(int *)&((BrUiCtl_ *)(this))->twActive) == 0)
        return 1;

    now = BrSub10075020();

    if ((*(int *)&((BrUiCtl_ *)(this))->twTick) <= 0)
        (*(int *)&((BrUiCtl_ *)(this))->twTick) = now;

    (*(int *)&((BrUiCtl_ *)(this))->twMs) += now - (*(int *)&((BrUiCtl_ *)(this))->twTick);
    (*(int *)&((BrUiCtl_ *)(this))->twTick) = now;

    if ((*(int *)&((BrUiCtl_ *)(this))->twXOn) != 0) {
        switch ((*(char *)&((BrUiCtl_ *)(this))->twXDir)) {
        case -1: {
            float v = (*(float *)&((BrUiCtl_ *)(this))->fTweenX) - s10((*(int *)&((BrUiCtl_ *)(this))->twMs));

            (*(float *)&((BrUiCtl_ *)(this))->x) = v;
            if (v <= (*(float *)&((BrUiCtl_ *)(this))->twXEnd)) {
                doneX = 1;
                (*(float *)&((BrUiCtl_ *)(this))->x) = (*(float *)&((BrUiCtl_ *)(this))->twXEnd);
            }
            break;
        }
        case 0:
            doneX = 1;
            break;
        case 1: {
            float v = s10((*(int *)&((BrUiCtl_ *)(this))->twMs)) + (*(float *)&((BrUiCtl_ *)(this))->fTweenX);

            (*(float *)&((BrUiCtl_ *)(this))->x) = v;
            if (v >= (*(float *)&((BrUiCtl_ *)(this))->twXEnd)) {
                doneX = 1;
                (*(float *)&((BrUiCtl_ *)(this))->x) = (*(float *)&((BrUiCtl_ *)(this))->twXEnd);
            }
            break;
        }
        }
    } else {
        doneX = 1;
    }

    if ((*(int *)&((BrUiCtl_ *)(this))->twYOn) != 0) {
        switch ((*(char *)&((BrUiCtl_ *)(this))->twYDir)) {
        case -1: {
            float v = (*(float *)&((BrUiCtl_ *)(this))->fTweenY) - s10((*(int *)&((BrUiCtl_ *)(this))->twMs));

            (*(float *)&((BrUiCtl_ *)(this))->y) = v;
            if (v <= (*(float *)&((BrUiCtl_ *)(this))->twYEnd)) {
                doneY = 1;
                (*(float *)&((BrUiCtl_ *)(this))->y) = (*(float *)&((BrUiCtl_ *)(this))->twYEnd);
            }
            break;
        }
        case 0:
            doneY = 1;
            break;
        case 1: {
            float v = s10((*(int *)&((BrUiCtl_ *)(this))->twMs)) + (*(float *)&((BrUiCtl_ *)(this))->fTweenY);

            (*(float *)&((BrUiCtl_ *)(this))->y) = v;
            if (v >= (*(float *)&((BrUiCtl_ *)(this))->twYEnd)) {
                doneY = 1;
                (*(float *)&((BrUiCtl_ *)(this))->y) = (*(float *)&((BrUiCtl_ *)(this))->twYEnd);
            }
            break;
        }
        }
    } else {
        doneY = 1;
    }

    if (doneX && doneY) {
        (*(int *)&((BrUiCtl_ *)(this))->twMs) = 0;
        (*(int *)&((BrUiCtl_ *)(this))->twActive) = 0;
    }

    return 1;
}

/* C entry points (generated by ports/64b/tools/methodfwd.py) */
extern "C" int BrUiTweenStep_10047D30(void *self)
{
    return ((class Tween41180 *)self)->Step();
}
/* end of C entry points */
