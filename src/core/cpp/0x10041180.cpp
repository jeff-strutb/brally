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
 * PARKED at 243 diffs / 37 bytes SHORT. The remaining cause is one thing:
 * the original RE-READS `f382C` from memory as the argument of each easing
 * vcall, where ours forwards the value it just stored there (`mov ecx,edx`
 * and no reload). Four reloads at 6 bytes each is most of the gap. This is
 * the "do not cache what the original re-reads" idiom in its hard
 * direction -- here the caching is VC5's store-to-load forwarding, not a
 * source local, so writing the member access out again does not undo it.
 * DO NOT RE-PROBE the arm forms; the switch above is already the right
 * shape. A fresh idea is needed for the reload.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

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

typedef char chk_f030[(unsigned)&((Tween41180 *)0)->f030 == 0x30 ? 1 : -1];
typedef char chk_b380C[(unsigned)&((Tween41180 *)0)->b380C == 0x380C ? 1 : -1];
typedef char chk_f3828[(unsigned)&((Tween41180 *)0)->f3828 == 0x3828 ? 1 : -1];

extern "C" {
int BrSub1006E280(void);        /* 0x1006E280 -- millisecond clock */
}

int Tween41180::Step()
{
    int doneX = 0;
    int doneY = 0;
    int now;
    int delta;

    if (f3818 == 0)
        return 1;

    now = BrSub1006E280();

    if (f3828 <= 0)
        f3828 = now;

    delta = now - f3828;
    f3828 = now;
    f382C = f382C + delta;

    if (f3804 != 0) {
        switch (b380C) {
        case -1: {
            float v = f030 - s10(f382C);

            f03C = v;
            if (v <= f3810) {
                f03C = f3810;
                doneX = 1;
            }
            break;
        }
        case 0:
            doneX = 1;
            break;
        case 1: {
            float v = s10(f382C) + f030;

            f03C = v;
            if (v >= f3810) {
                f03C = f3810;
                doneX = 1;
            }
            break;
        }
        }
    } else {
        doneX = 1;
    }

    if (f3808 != 0) {
        switch (b380D) {
        case -1: {
            float v = f034 - s10(f382C);

            f040 = v;
            if (v <= f3814) {
                f040 = f3814;
                doneY = 1;
            }
            break;
        }
        case 0:
            doneY = 1;
            break;
        case 1: {
            float v = s10(f382C) + f034;

            f040 = v;
            if (v >= f3814) {
                f040 = f3814;
                doneY = 1;
            }
            break;
        }
        }
    } else {
        doneY = 1;
    }

    if (doneX && doneY) {
        f382C = 0;
        f3818 = 0;
    }

    return 1;
}
