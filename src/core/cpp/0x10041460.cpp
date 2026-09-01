/* @implements 0x10041460 glide BrUiEnter_10048010
 * @cpp_kind method
 * @cpp_symbol ?Enter@Ui48010@@QAEHXZ
 *
 * True thiscall receiver: flag-gated dispatch — bit 0x100000 routes to a
 * slot-4 vcall on the embedded item at +0x2B5C (guarded by a `lea`/test
 * on the member text array, which MSVC5 does not fold), bit 0x200000
 * suppresses, else a slot-4 vcall on self whose zero result is returned
 * as-is. Same +0x2B5C/+0x2B65 item/text layout as 0x10038D30. No EH.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Item {
public:
    virtual void i0();
    virtual void i1();
    virtual void i2();
    virtual void i3();
    virtual void i4();
};

class Ui48010 {
public:
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual int v4();
    char p0[0x18];
    int f1C;
    char p1[8];
    int f28;
    char p2[0x2B30];
    Item item;
    char p3[5];
    char text[256];

    int Enter();
};

typedef char chk_1C[(unsigned)&((Ui48010 *)0)->f1C == 0x1C ? 1 : -1];
typedef char chk_28[(unsigned)&((Ui48010 *)0)->f28 == 0x28 ? 1 : -1];
typedef char chk_item[(unsigned)&((Ui48010 *)0)->item == 0x2B5C ? 1 : -1];
typedef char chk_text[(unsigned)&((Ui48010 *)0)->text == 0x2B65 ? 1 : -1];

int Ui48010::Enter()
{
    int fl;
    int r;

    if (f28 & 1) {
        fl = f1C;
        if (fl & 0x100000) {
            if (text != 0) {
                item.i4();
                return 1;
            }
        } else if (!(fl & 0x200000)) {
            r = v4();
            if (r == 0)
                return r;
        }
    }
    return 1;
}
