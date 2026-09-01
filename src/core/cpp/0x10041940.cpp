/* @implements 0x10041940 glide BrUiPageSelect_100484F0
 * @cpp_kind method
 * @cpp_symbol ?Adv@Phase32F@@QAEHXZ
 *
 * 64 B frameless thiscall — the Adv() the 0x10041980 Frame twin calls.
 * The limit word is read into a register var FIRST (dx, zero-extended
 * with and 0xffff at the compare), the paced counter global second;
 * wrap-to-0 vs clamp-to-lim-1 both store the global through the shared
 * word store (cross-jumped), then w346 latches the result.
 */
class Phase32F {
public:
    char pad[0x344];
    unsigned short w344;            /* +0x344 */
    short w346;                     /* +0x346 */
    int Adv();
};

extern "C" {
extern unsigned short DAT_10ac5bc4;
}

int Phase32F::Adv()
{
    unsigned short lim;
    short v;

    lim = w344;
    v = (short)DAT_10ac5bc4;
    if (v >= lim) {
        v = 0;
        DAT_10ac5bc4 = v;
    } else if (v < 0) {
        v = (short)(lim - 1);
        DAT_10ac5bc4 = v;
    }
    w346 = v;
    return 1;
}
