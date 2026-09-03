/* @implements 0x10055330 glide BrUiHitTest_10055330
 * @cpp_kind method
 * @cpp_symbol ?HitTest@Ui55330@@QAEHPAURect55330@@@Z
 *
 * Thiscall, one stack arg (`ret 4`), 117 B. Point-in-rect against the
 * cursor at 0x10AC5DD8; miss returns 0 without touching state. On a hit,
 * the widget's +0x18 flag word gets bit 1 set or cleared depending on
 * which "is something else grabbing input" test applies -- the 0x40000
 * flag picks between the two windows at 0x10AC61E0 (+0x2C / +0x30) and
 * the 0x10037720 helper -- and bit 5 is set unconditionally afterwards.
 *
 * The flag word is stored TWICE (`mov [esi+0x18],eax; or al,0x20; mov
 * [esi+0x18],eax`) because the arms each write it and the `| 0x20` is a
 * separate statement on the member; VC5 tail-merges the three arms onto
 * one store and keeps the value live for the second.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

struct Rect55330 {
    int x0;     /* +0x00 */
    int y0;     /* +0x04 */
    int x1;     /* +0x08 */
    int y1;     /* +0x0C */
};

struct Point55330 {
    int x;
    int y;
};

struct Grab55330 {
    char pad[0x2C];
    int  f2C;
    int  f30;
};

class Ui55330 {
public:
    char pad[0x18];
    int  f18;       /* +0x18 -- flag word */

    int HitTest(Rect55330 *pRect);
};

typedef char chk_f18[(unsigned)&((Ui55330 *)0)->f18 == 0x18 ? 1 : -1];

extern "C" {
Point55330 *g_AC5DD8;
Grab55330  *g_AC61E0;
int Fn10037720(void);
}

int Ui55330::HitTest(Rect55330 *pRect)
{
    if (pRect->x0 > g_AC5DD8->x || pRect->x1 < g_AC5DD8->x ||
        pRect->y0 > g_AC5DD8->y || pRect->y1 < g_AC5DD8->y)
        return 0;

    if (f18 & 0x40000) {
        if (g_AC61E0->f2C != 0 || g_AC61E0->f30 != 0)
            f18 |= 0x80002;
        else
            f18 &= ~2;
    } else if (Fn10037720() != 0) {
        f18 |= 2;
    } else {
        f18 &= ~2;
    }

    f18 |= 0x20;
    return 1;
}
