/* @implements 0x10041160 glide BrUiTweenReset_10047D10
 * @cpp_kind method
 * @cpp_symbol ?Reset@Tween41160@@QAEHXZ
 *
 * Thiscall, no args (`ret`), 30 B. Snapshot the tween's current triple into
 * the target triple and arm the "moving" flag. Returns 1.
 *
 * The port body in slice3_32.c takes a globals-struct pointer it does not
 * need; the original is a bare thiscall. Same split as the siblings.
 *
 * The flag store comes FIRST in the source even though it is emitted
 * third-from-last: VC5 materialises the constant 1 once (it is also the
 * return value) and sinks the store into the middle of the three copies,
 * which keeps the copies as a strict load/store chain through one
 * register. Writing the flag last, or between two copies, batches the
 * loads into two registers instead (18-26 diffs).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Tween41160 {
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

    int Reset();
};

typedef char chk_f030_41160[(unsigned)&((Tween41160 *)0)->f030 == 0x30 ? 1 : -1];
typedef char chk_f3818_41160[(unsigned)&((Tween41160 *)0)->f3818 == 0x3818 ? 1 : -1];
typedef char chk_f3824_41160[(unsigned)&((Tween41160 *)0)->f3824 == 0x3824 ? 1 : -1];

int Tween41160::Reset()
{
    f3818 = 1;
    f03C = f030;
    f040 = f034;
    f044 = f038;

    return 1;
}
