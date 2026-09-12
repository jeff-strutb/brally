/* WHAT IT DOES: draw a menu item's frame by tiling three sprites: a left
 * cap, as many middle pieces as the item is wide, and a right cap. */
/* @implements 0x10037DC0 glide BrUiHook85_1003E7A0
 * @cpp_kind method
 * @cpp_symbol ?BrUiHook85_1003E7A0@@YAHPAVGameObj@@@Z
 *
 * Tile-row emitter: two `(int)float` casts through CRT __ftol (value in
 * ST0, no stack arg), signed /16 (cdq/and 0xF/sar), then three slot-5
 * vcalls — VC5 CSEs the virtual function pointer into a spill slot and
 * strength-reduces `x + i*16` to a running register in the loop. No EH.
 *
 * BYTE-EXACT 2026-09-12 (was parked 16 diffs: y/n in swapped registers
 * ebp/ebx with swapped spill slots, and `add ebp,-0xc` for the -12).  All
 * three were ONE spelling: the baseline is `y = (int)f - 12` written into
 * each call.  Instead `y` is the bare truncation and `y - 12` is spelled in
 * ALL THREE call arguments -- VC5 CSEs the expression into a temp, folds
 * the temp into y's dying register as `add ebp,-0xc` (a CSE'd `v - K`
 * is emitted as an add of the negative immediate, never `sub`), and the
 * temp's web is created after n's, which is what gives n ebx / slot 0x10
 * and y ebp, with x's spill at slot 0x14.  The unsigned loop count is still
 * required (signed emits jle AND an ebp frame).
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
    y = (int)pGame->f2F70;
    n = pGame->w2F66 / 16 + 1;
    pGame->s5(0x3D, x - 8, y - 12);
    for (i = 0; i < n; i++)
        pGame->s5(0x3B, x + i * 16, y - 12);
    pGame->s5(0x3C, x + i * 16, y - 12);
    return 1;
}
