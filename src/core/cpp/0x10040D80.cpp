/* @implements 0x10040D80 glide BrUiDrawCode_10047930
 * @cpp_kind method
 * @cpp_symbol ?Draw@Draw40D80@@QAEHXZ
 *
 * Thiscall, no args (`ret`), 79 B. Draw the current code row, taking the
 * label, the rectangle and the trailing field all from the row's entry in
 * the 24-byte table at 0x100AAD08. A negative row number draws nothing.
 * Returns 1 either way.
 *
 * The port body in slice3_32.c takes a globals-struct pointer it does not
 * need; the original is a bare thiscall. Same split as the siblings.
 *
 * The row's own short reaches the callee via the SHORT PUSH; the two
 * `(int)` casts are `call __ftol`.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

struct BrRow40D80 {
    short w00;                  /* +0x00 */
    short pad02;
    int   a04[4];               /* +0x04 */
    int   f14;                  /* +0x14 */
};                              /* 24 -- base 0x100AAD08 */

class Draw40D80 {
public:
    char  pad000[0x3C];
    float f03C;                 /* +0x03C */
    float f040;                 /* +0x040 */
    char  pad044[0x1E20C - 0x44];
    short w1E20C;               /* +0x1E20C */

    int Draw();
};

typedef char chk_f03C_40D80[(unsigned)&((Draw40D80 *)0)->f03C == 0x3C ? 1 : -1];
typedef char chk_w_40D80[(unsigned)&((Draw40D80 *)0)->w1E20C == 0x1E20C ? 1 : -1];

extern "C" {
BrRow40D80 g_brRowsAAD08[];   /* 0x100AAD08 */
void BrDraw10058380(int a, int b, short c, void *d, int e);   /* 0x10058380 */
}

int Draw40D80::Draw()
{
    if (w1E20C >= 0) {
        BrDraw10058380((int)f03C, (int)f040, g_brRowsAAD08[w1E20C].w00,
                       g_brRowsAAD08[w1E20C].a04,
                       g_brRowsAAD08[w1E20C].f14);
    }

    return 1;
}
