/* WHAT IT DOES: begin an animated move: works out the per-step increment
 * from the distance and the number of steps, and remembers where the item
 * started. */
/* @implements 0x10041100 glide BrUiTweenBegin_10047CB0
 * @cpp_kind method
 * @cpp_symbol ?Begin@Tween41100@@QAEHH@Z
 *
 * Thiscall, one stack arg (`ret 4`), 48 B. Work out the per-step delta for
 * a tween -- the span divided by the step count -- and move the target
 * triple back into the current one. Returns 1.
 *
 * The divisor is an INT argument divided into a float, which VC5 spells as
 * a single `fidiv dword ptr [arg]` against the integer in memory; casting
 * it to float first would emit a fild instead.
 *
 * The port body in slice3_32.c takes a globals-struct pointer it does not
 * need; the original is a bare thiscall. Same split as the siblings.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Tween41100 {
public:
    char  pad000[0x30];
    int   f030;                 /* +0x030 */
    int   f034;
    int   f038;
    int   f03C;
    int   f040;
    int   f044;
    char  pad048[0x3818 - 0x48];
    int   f3818;                /* +0x3818 */
    float f381C;                /* +0x381C */
    float f3820;
    float f3824;

    int Begin(int nSteps);
};

typedef char chk_f030_41100[(unsigned)&((Tween41100 *)0)->f030 == 0x30 ? 1 : -1];
typedef char chk_f3818_41100[(unsigned)&((Tween41100 *)0)->f3818 == 0x3818 ? 1 : -1];
typedef char chk_f3824_41100[(unsigned)&((Tween41100 *)0)->f3824 == 0x3824 ? 1 : -1];

int Tween41100::Begin(int nSteps)
{
    f3824 = (f3820 - f381C) / nSteps;

    f030 = f03C;
    f034 = f040;
    f038 = f044;

    return 1;
}
