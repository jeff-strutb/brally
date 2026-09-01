/* @implements 0x100414B0 glide BrUiCheckOther_10048060
 * @cpp_kind method
 * @cpp_symbol ?CheckOther@UiPage@@QAEHXZ
 *
 * 52 B thiscall. Tests the global current-page pointer against this;
 * the 1 is materialized once (mov eax,1) and serves the f70 compare,
 * the flag store, and the return value. this==current skips both flag
 * stores straight to return 0.
 */
class UiCont {
public:
    char pad[0x70];
    int f70;                        /* +0x70 */
};

class UiPage {
public:
    char pad[0x2AE8];
    UiCont *f2AE8;                  /* +0x2AE8 */
    int CheckOther();
};

extern "C" {
extern UiPage *DAT_10ac5d18;
extern int DAT_10ac5bb0;
}

int UiPage::CheckOther()
{
    UiPage *p;

    p = DAT_10ac5d18;
    if (p != 0 && p->f2AE8->f70 == 1) {
        if (this != p) {
            DAT_10ac5bb0 = 1;
            return 1;
        }
    } else {
        DAT_10ac5bb0 = 0;
    }
    return 0;
}
