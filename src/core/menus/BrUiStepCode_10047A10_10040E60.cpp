/* WHAT IT DOES: advance the cheat-code entry one character: picks up the
 * next symbol, moves to its slot, and ends the sequence when there are no
 * more. */
/* @implements 0x10040E60 glide BrUiStepCode_10047A10
 * @cpp_kind method
 * @cpp_symbol ?StepCode@Ui47A10@@QAEHXZ
 *
 * True thiscall receiver (this in ecx, no stack params): early-out slot-7
 * vcall when the mode word is clear; else a signed-short index selects a
 * code word (written back at +0x1E20C) and a 16-byte record whose pointer
 * feeds the slot-6 vcall. One shared `return 1` after if/else — VC5
 * tail-duplicates it per path with the epilogue pop BEFORE the constant
 * load (`pop esi; mov eax,1; ret`); explicit per-branch returns emit the
 * mov first and miss by 4 bytes. No EH.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Ui47A10 {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual void s6(char *);
    virtual void s7();
    char p0[0x124];
    short f128;
    char p1[0x2842];
    int f296C;
    char p2[0xD0];
    short aCode[0x100];
    char p3[0x1B5CC];
    short f1E20C;
    char p4[2];
    char *f1E210;

    int StepCode();
};

typedef char chk_128[(unsigned)&((Ui47A10 *)0)->f128 == 0x128 ? 1 : -1];
typedef char chk_296C[(unsigned)&((Ui47A10 *)0)->f296C == 0x296C ? 1 : -1];
typedef char chk_2A40[(unsigned)&((Ui47A10 *)0)->aCode == 0x2A40 ? 1 : -1];
typedef char chk_1E20C[(unsigned)&((Ui47A10 *)0)->f1E20C == 0x1E20C ? 1 : -1];
typedef char chk_1E210[(unsigned)&((Ui47A10 *)0)->f1E210 == 0x1E210 ? 1 : -1];

int Ui47A10::StepCode()
{
    int idx;
    char *p;

    if (f296C == 0) {
        s7();
    } else {
        idx = f128;
        p = f1E210 + idx * 16;
        f1E20C = aCode[idx];
        s6(p);
    }
    return 1;
}
