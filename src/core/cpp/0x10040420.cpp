/* @implements 0x10040420 glide BrPhaseLeave_10046FD0
 * @cpp_kind free
 * @cpp_symbol ?BrPhaseLeave_10046FD0@@YAHPAVCtl40420@@@Z
 *
 * cdecl, one arg, `ret`, 119 B. Leave a phase that owns three optional
 * side panels: tear each of them down through the same +0x1C vcall and
 * null it, then the owner's own teardown. After that the usual tail --
 * run the owner's +0x1C teardown vcall, delete whatever phase is current
 * afterwards, clear the pending marker and make the root phase current.
 * Returns 0, which is what stops the caller's frame from continuing.
 *
 * The port body in br_uinav.c reaches its globals through a
 * BrUiNav * globals pointer; the original addresses them directly. Same
 * split as the slice2_23.c / slice3_32.c siblings.
 *
 * The original loads the ROOT phase BEFORE clearing the pending marker and
 * only then stores it as current -- but do NOT introduce a temp to force
 * that. VC5 hoists the load above the unrelated store on its own; naming
 * the value costs 10 diffs by moving it out of eax (the accumulator forms
 * `a1`/`a3`) into ecx. The port body's temp, added to "preserve" the
 * observed order, is exactly what breaks the match here.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Sub2AE8_40420 {
public:
    virtual void s0(); virtual void s1(); virtual void s2();
    virtual void s3(); virtual void s4(); virtual void s5();
    virtual void s6();
    virtual void s7();          /* +0x1C teardown */
};

class Phase40420 {
public:
    virtual ~Phase40420();
};

class Ctl40420 {
public:
    char            pad000[0x2AE8];
    Sub2AE8_40420  *p2AE8;      /* +0x2AE8 */
};

typedef char chk_p2AE8[(unsigned)&((Ctl40420 *)0)->p2AE8 == 0x2AE8 ? 1 : -1];

extern "C" {
Phase40420      *g_brPhase5C5C;     /* 0x10AC5C5C -- current phase */
Phase40420      *g_brRoot5C60;      /* 0x10AC5C60 -- root phase */
int              g_brPending5CCC;   /* 0x10AC5CCC */
Sub2AE8_40420   *g_brPanel5C8C;     /* 0x10AC5C8C */
Sub2AE8_40420   *g_brPanel5C90;     /* 0x10AC5C90 */
Sub2AE8_40420   *g_brPanel5C94;     /* 0x10AC5C94 */
}

int BrPhaseLeave_10046FD0(Ctl40420 *pCtl)
{
    if (g_brPanel5C8C != 0) {
        g_brPanel5C8C->s7();
        g_brPanel5C8C = 0;
    }
    if (g_brPanel5C90 != 0) {
        g_brPanel5C90->s7();
        g_brPanel5C90 = 0;
    }
    if (g_brPanel5C94 != 0) {
        g_brPanel5C94->s7();
        g_brPanel5C94 = 0;
    }

    pCtl->p2AE8->s7();

    if (g_brPhase5C5C != 0)
        delete g_brPhase5C5C;

    g_brPending5CCC = 0;
    g_brPhase5C5C = g_brRoot5C60;

    return 0;
}
