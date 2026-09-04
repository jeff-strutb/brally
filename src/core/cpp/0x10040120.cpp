/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear. */
/* @implements 0x10040120 glide BrUiHook89_10046CD0
 * @cpp_kind method
 * @cpp_symbol ?Hook@Ui89@@YAHPAVGameObj89@@@Z
 *
 * 66 B cdecl(pObj), returns 0. Slot-7 vcall, delete of the current phase,
 * zero two flags, phase swap from the 5C88 slot.
 */
class Sub2AE8b {
public:
    virtual void s0(); virtual void s1(); virtual void s2();
    virtual void s3(); virtual void s4(); virtual void s5();
    virtual void s6(int);
    virtual void s7();          /* +0x1C */
};

class CurPhase89 {
public:
    virtual ~CurPhase89();
};

class GameObj89 {
public:
    char pad[0x2AE8];
    Sub2AE8b *p2AE8;
};

extern "C" {
extern CurPhase89 *DAT_10ac5c5c;
extern CurPhase89 *DAT_10ac5c88;
extern int         DAT_10ac5c6c;
extern int         DAT_10ac5d0c;
}

int Hook(GameObj89 *pObj)
{
    pObj->p2AE8->s7();

    if (DAT_10ac5c5c != 0)
        delete DAT_10ac5c5c;

    DAT_10ac5c6c = 0;
    DAT_10ac5d0c = 0;
    DAT_10ac5c5c = DAT_10ac5c88;
    return 0;
}
