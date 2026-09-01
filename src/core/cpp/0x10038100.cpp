/* @implements 0x10038100 glide BrUiHook85_1003EB10
 * @cpp_kind method
 * @cpp_symbol ?Hook@Ui85@@YAHPAVGameObj85@@@Z
 *
 * 74 B cdecl(pObj), returns 1. Embedded member object at +0x3838: slot-8
 * vcall with the cached value, clamp/reload, gated slot-9 vcall; the
 * member vtbl is CSE'd across both calls.
 */
class Emb3838 {
public:
    virtual void s0(); virtual void s1(); virtual void s2();
    virtual void s3(); virtual void s4(); virtual void s5();
    virtual void s6(); virtual void s7();
    virtual int  s8(int);       /* +0x20 */
    virtual void s9(int);       /* +0x24 */
};

class GameObj85 {
public:
    char pad[0x3838];
    Emb3838 m3838;              /* embedded at +0x3838 */
};

extern "C" {
extern int DAT_100aab94;
extern int DAT_10ac5c30;
}

int Hook(GameObj85 *pObj)
{
    int r = pObj->m3838.s8(DAT_100aab94);

    if (r >= 0)
        DAT_100aab94 = r;
    else
        r = DAT_100aab94;

    if (DAT_10ac5c30 != 0 && r >= 0)
        pObj->m3838.s9(r);
    return 1;
}
