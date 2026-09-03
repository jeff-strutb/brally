/* @implements 0x10040DD0 glide BrUiDrawCodeRect_10047980
 * @cpp_kind method
 * @cpp_symbol ?DrawRect@Draw40DD0@@QAEHPAX@Z
 *
 * Thiscall, one stack arg (`ret 4`), 65 B. Draw the current code row with
 * a caller-supplied rectangle: the two float positions are truncated to
 * ints, the row number goes through as a short, and the row's trailing
 * field comes from the 24-byte table at 0x100AAD08. Returns 1.
 *
 * The port body in slice3_32.c takes a globals-struct pointer it does not
 * need; the original is a bare thiscall. Same split as the siblings.
 *
 * The row number reaches the callee via the SHORT PUSH -- `mov ax,[m]`
 * then `push eax` with the upper half left dirty -- because the parameter
 * is declared short. The two `(int)` casts are the usual `call __ftol`,
 * and the +0x40 one is evaluated before the +0x3C one because arguments go
 * right to left.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

struct BrRow40DD0 {
    short w00;                  /* +0x00 */
    short pad02;
    int   a04[4];               /* +0x04 */
    int   f14;                  /* +0x14 */
};                              /* 24 -- base 0x100AAD08 */

class Draw40DD0 {
public:
    char  pad000[0x3C];
    float f03C;                 /* +0x03C */
    float f040;                 /* +0x040 */
    char  pad044[0x1E20C - 0x44];
    short w1E20C;               /* +0x1E20C */

    int DrawRect(void *pRect);
};

typedef char chk_f03C_40DD0[(unsigned)&((Draw40DD0 *)0)->f03C == 0x3C ? 1 : -1];
typedef char chk_w_40DD0[(unsigned)&((Draw40DD0 *)0)->w1E20C == 0x1E20C ? 1 : -1];

extern "C" {
BrRow40DD0 g_brRowsAAD08[];   /* 0x100AAD08 */
void BrDraw10058380(int a, int b, short c, void *d, int e);   /* 0x10058380 */
}

int Draw40DD0::DrawRect(void *pRect)
{
    BrDraw10058380((int)f03C, (int)f040, w1E20C, pRect,
                   g_brRowsAAD08[w1E20C].f14);

    return 1;
}
