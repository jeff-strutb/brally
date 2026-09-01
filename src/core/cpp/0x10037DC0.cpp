/* @implements 0x10037DC0 glide BrUiHook85_1003E7A0
 * @cpp_kind method
 * @cpp_symbol ?BrUiHook85_1003E7A0@@YAHPAVGameObj@@@Z
 *
 * Tile-row emitter: two `(int)float` casts through CRT __ftol (value in
 * ST0, no stack arg), signed /16 (cdq/and 0xF/sar), then three slot-5
 * vcalls — VC5 CSEs the virtual function pointer into a spill slot and
 * strength-reduces `x + i*16` to a running register in the loop. No EH.
 *
 * PARKED 16-diff T3a residue: shape is instruction-identical and
 * frameless (unsigned loop count is REQUIRED — signed emits jle AND an
 * ebp frame), but y/n sit in swapped registers (ebp/ebx) with swapped
 * spill slots, and orig spells y's -12 as `add ebp,-0xc` where every
 * probed source (- 12, + -12, -3-9, split -=, pointer -3, decl orders)
 * canonicalizes to `sub`. Register-blind gap 0.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Ui3E7A0 {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5(int, int, int);
    char p0[0x2F62];
    short w2F66;
    char p1[4];
    float f2F6C;
    float f2F70;
};

typedef char chk_w[(unsigned)&((Ui3E7A0 *)0)->w2F66 == 0x2F66 ? 1 : -1];
typedef char chk_f6C[(unsigned)&((Ui3E7A0 *)0)->f2F6C == 0x2F6C ? 1 : -1];
typedef char chk_f70[(unsigned)&((Ui3E7A0 *)0)->f2F70 == 0x2F70 ? 1 : -1];

int BrUiHook85_1003E7A0(Ui3E7A0 *pGame)
{
    int x;
    int y;
    unsigned int n;
    unsigned int i;

    x = (int)pGame->f2F6C - 3;
    y = (int)pGame->f2F70 - 12;
    n = pGame->w2F66 / 16 + 1;
    pGame->s5(0x3D, x - 8, y);
    for (i = 0; i < n; i++)
        pGame->s5(0x3B, x + i * 16, y);
    pGame->s5(0x3C, x + i * 16, y);
    return 1;
}
