/* WHAT IT DOES: wrap the page's selection index back into range, at both
 * ends, so moving past the last entry lands on the first. */
/* @implements 0x10041940 glide BrUiPageSelect_100484F0
 * @cpp_kind method
 * @cpp_symbol ?Adv@Phase32F@@QAEHXZ
 *
 * 64 B frameless thiscall — the Adv() the 0x10041980 Frame twin calls.
 * The limit word is read into a register var FIRST (dx, zero-extended
 * with and 0xffff at the compare); the paced counter is DIRECT REREADS
 * of the global (a short local spills and grows a frame — the
 * direct-global-reread idiom), CSEd into ax across the branches;
 * wrap-to-0 vs clamp-to-lim-1 share the cross-jumped word store, then
 * w346 latches the forwarded value.
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

    lim = w344;
    if ((short)DAT_10ac5bc4 >= lim) {
        DAT_10ac5bc4 = 0;
    } else if ((short)DAT_10ac5bc4 < 0) {
        DAT_10ac5bc4 = (unsigned short)(lim - 1);
    }
    w346 = (short)DAT_10ac5bc4;
    return 1;
}
