/* WHAT IT DOES: lay the heads-up display out for a given rectangle -- stores
 * the corners and works out the offsets every HUD element is positioned
 * from. */
/* @implements 0x10054730 glide BrHudLayoutInit_10054730
 * @cpp_kind method
 * @cpp_symbol ?Layout@Hud54730@@QAEHHPAHFFF@Z
 *
 * Thiscall, five stack args (`ret 0x14`), 618 B. Seed the readout block
 * that starts just past the slot count at +0x1A92C: the caller's rect in
 * both int and float form, the three separator characters ('0', '.',
 * ':'), then one of two layout arms chosen by which of the two mode
 * words at +0x1A9B8 / +0x1A9BC is set. Both arms end by rounding the two
 * anchor floats back to ints with `_ftol` and offsetting them by the
 * clamped viewport extent. Returns 1.
 *
 * The function is frameless: every `fild` scratch and every spill lands
 * in the incoming argument slots, which VC5 reuses once the parameters
 * are dead. That only happens while the body declares no locals of its
 * own, so the arm bodies are written as straight member assignments --
 * naming an intermediate forces `sub esp,N` and shifts every stack
 * displacement.
 *
 * Where the original RELOADS a field it just stored (`mov ebp,
 * [esi+0x1A980]`) the source reads the member back; where it keeps the
 * register the source still reads the member and VC5 forwards the store.
 * The two spellings are not interchangeable in the other direction: an
 * expression rewritten from the members produces a fresh `mov eax,
 * [eax+0xC]` reload of the caller's rect, because `int *` may alias
 * `this`.
 *
 * The bias is `- (-1.0f)`, not `+ 1.0f`: the literal pool holds -1.0 and
 * the instruction is `fsub`, so the minus sign is the operator's.
 *
 * The tail was worth 13 diffs on its own: the two extents are ACCUMULATED
 * (`dx += i1a98c; i1a994 = dx;`), not read as `dx + i1a98c`. Written as a
 * sum, the extent register frees early, the second `_ftol` result is
 * stored before the adds, and both adds come out reversed.
 *
 * PARKED at 31 diffs / instruction parity (recomp is 618 B of body plus
 * six alignment nops). Two sites, both allocator, register-blind gap 2:
 *   +0x109  `lea edx,[edi+eax]` vs `[eax+edi]` -- 1 byte, the SIB
 *           base/index choice for i1a984.
 *   +0x1C2..+0x200  the else-arm's `mov ecx,[esi+0x1A960]` is hoisted to
 *           just after i1a960's store, which pins the i1a964 add into edx
 *           (`add edx,ecx`); ours issues the load 27 bytes later and the
 *           add lands the other way (`add ecx,edx`). Same instructions,
 *           same count, different order.
 * DO NOT RE-PROBE. Unchanged by: both operand orders on i1a954/i1a958/
 * i1a964/i1a984, read-modify-write spellings of i1a964 and i1a984 (rmw on
 * i1a964 is much worse, 153), moving i1a968 before i1a964 (69), i1a960
 * last (158), routing i1a964 through i1a95c (187), f1a9ac through i1a95c
 * (189), f1a9d0 through f1a9cc (189), `1 + i1a958` for f1a9c8, tail
 * interleaving (71/73), and flags /O2 /Op (395), /O2 /Op /Oy- (542),
 * /Ox, /O2 /Ot, /O2 /Gy, /O2 /Ob0 (all 44 pre-tail-fix), /Od (567),
 * /O2 /Oy- (536).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Hud54730 {
public:
    char  pad000[0x18];
    int   i18;                              /* +0x18 */
    float f1c;                              /* +0x1C */
    float f20;                              /* +0x20 */
    char  pad024[0x1A92E - 0x24];
    short w1a92e;
    short w1a930;
    short w1a932;
    short w1a934;
    short w1a936;
    short w1a938;
    char  pad1a93a[0x1A93C - 0x1A93A];
    int   i1a93c;
    int   i1a940;
    int   i1a944;
    int   i1a948;
    int   i1a94c;
    int   i1a950;
    int   i1a954;
    int   i1a958;
    int   i1a95c;
    int   i1a960;
    int   i1a964;
    int   i1a968;
    int   i1a96c;
    int   i1a970;
    int   i1a974;
    int   i1a978;
    int   i1a97c;
    int   i1a980;
    int   i1a984;
    int   i1a988;
    int   i1a98c;
    int   i1a990;
    int   i1a994;
    int   i1a998;
    char  pad1a99c[0x1A9AC - 0x1A99C];
    float f1a9ac;
    float f1a9b0;
    char  pad1a9b4[0x1A9B8 - 0x1A9B4];
    int   i1a9b8;
    int   i1a9bc;
    float f1a9c0;
    float f1a9c4;
    float f1a9c8;
    float f1a9cc;
    float f1a9d0;

    int Layout(int a1, int *r, short a3, short a4, short a5);
};

typedef char chk_f1c[(unsigned)&((Hud54730 *)0)->f1c == 0x1C ? 1 : -1];
typedef char chk_w92e[(unsigned)&((Hud54730 *)0)->w1a92e == 0x1A92E ? 1 : -1];
typedef char chk_i93c[(unsigned)&((Hud54730 *)0)->i1a93c == 0x1A93C ? 1 : -1];
typedef char chk_f9ac[(unsigned)&((Hud54730 *)0)->f1a9ac == 0x1A9AC ? 1 : -1];
typedef char chk_i9b8[(unsigned)&((Hud54730 *)0)->i1a9b8 == 0x1A9B8 ? 1 : -1];
typedef char chk_f9d0[(unsigned)&((Hud54730 *)0)->f1a9d0 == 0x1A9D0 ? 1 : -1];

extern "C" {
int g_brVp0AB164;               /* 0x100AB164 */
int g_brVp0AB168;               /* 0x100AB168 */
int g_brVp0AB18C;               /* 0x100AB18C */
int g_brVp0AB190;               /* 0x100AB190 */
int g_brVp0AB194;               /* 0x100AB194 */
int g_brVp0AB198;               /* 0x100AB198 */
}

int Hud54730::Layout(int a1, int *r, short a3, short a4, short a5)
{
    int dx;
    int dy;

    f1c = (float)r[0];
    f20 = (float)r[1];

    i1a93c = r[0];
    i1a940 = r[1];
    i1a944 = r[2];
    i1a948 = r[3];

    i18 = a1;

    w1a930 = a3;
    w1a92e = a4;
    w1a936 = a5;

    w1a932 = '0';
    w1a934 = '.';
    w1a938 = ':';

    dx = g_brVp0AB194 - g_brVp0AB18C;
    dy = g_brVp0AB198 - g_brVp0AB190;
    if (dx < 0)
        dx = 0;
    if (dy < 0)
        dy = 0;

    if (i1a9b8 != 0) {
        i1a96c = r[0];
        i1a970 = r[3] + 3;
        i1a974 = i1a96c + dx;
        i1a978 = i1a970 + dy;
        i1a97c = r[2] - dx;
        i1a980 = r[3] + 3;
        i1a984 = dx + i1a97c;
        i1a988 = i1a980 + dy;

        f1a9ac = (float)i1a974 - (-1.0f);
        f1a9b0 = (float)i1a970;
        f1a9d0 = (float)(i1a97c - dx) - f1a9ac;
        f1a9c0 = (float)(i1a974 + 1);
        f1a9c4 = (float)(i1a97c - dx);
    } else if (i1a9bc != 0) {
        i1a94c = r[2] + 3;
        i1a950 = r[1];
        i1a954 = g_brVp0AB194 + i1a94c;
        i1a958 = g_brVp0AB198 + i1a950;
        i1a95c = r[2] + 3;
        i1a960 = r[3] - g_brVp0AB168;
        i1a964 = g_brVp0AB164 + i1a94c;
        i1a968 = r[3];

        f1a9ac = (float)i1a94c;
        f1a9b0 = (float)i1a958 - (-1.0f);
        f1a9d0 = (float)(i1a960 - dy) - f1a9b0;
        f1a9c8 = (float)(i1a958 + 1);
        f1a9cc = (float)(i1a960 - dy);
    }

    i1a98c = (int)f1a9ac;
    i1a990 = (int)f1a9b0;
    dx += i1a98c;
    dy += i1a990;
    i1a994 = dx;
    i1a998 = dy;

    return 1;
}
