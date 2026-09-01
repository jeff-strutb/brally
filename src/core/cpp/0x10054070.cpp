/* @implements 0x10054070 glide BrUiTick_10054070
 * @cpp_kind method
 * @cpp_symbol ?Tick@Ui54070@@QAEXHH@Z
 *
 * Thiscall receiver, two stack args (`ret 8`): accumulate elapsed time
 * from the timer helper into a global, and every 120 ms fire the slot-6
 * vcall on self with (word global, arg a, arg b, char member) — the word
 * global pushed via the `mov dx,[g]; push edx` short-push idiom. No EH.
 *
 * PARKED 4-diff T3a residue: the delta computation's scratch pair is
 * rotated (orig loads g_5DB0/g_5DAC through ecx with delta in edx;
 * recomp the reverse). Add-operand order, split delta statement, and an
 * explicit old-value temp all leave the rotation. Register-blind gap 0.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Ui54070 {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual void s6(short, int, int, int);
    char pad[4];
    char f08;

    void Tick(int a, int b);
};

typedef char chk_f08[(unsigned)&((Ui54070 *)0)->f08 == 8 ? 1 : -1];

extern "C" {
int g_5DB0;
int g_5DAC;
short g_C17C;
int FnE280(void);
}

void Ui54070::Tick(int a, int b)
{
    int now;
    int old;
    int delta;
    int acc;

    now = FnE280();
    old = g_5DB0;
    delta = now - old;
    acc = g_5DAC + delta;
    g_5DB0 = now;
    g_5DAC = acc;
    if (acc >= 0x78) {
        g_5DAC = 0;
        s6(g_C17C, a, b, f08);
    }
}
