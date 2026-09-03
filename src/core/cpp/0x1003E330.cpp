/* @implements 0x1003E330 glide BrPhaseLeave_10044DE0
 * @cpp_kind free
 * @cpp_symbol ?BrPhaseLeave_10044DE0@@YAHPAVCtl3E330@@@Z
 *
 * cdecl, one arg, `ret`, 56 B. Twin of 0x100400E0 on a different
 * next phase and pending marker. The "leave this phase" hook:
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

class Sub2AE8_3E330 {
public:
    virtual void s0(); virtual void s1(); virtual void s2();
    virtual void s3(); virtual void s4(); virtual void s5();
    virtual void s6();
    virtual void s7();          /* +0x1C teardown */
};

class Phase3E330 {
public:
    virtual ~Phase3E330();
};

class Ctl3E330 {
public:
    char            pad000[0x2AE8];
    Sub2AE8_3E330  *p2AE8;      /* +0x2AE8 */
};

typedef char chk_p2AE8[(unsigned)&((Ctl3E330 *)0)->p2AE8 == 0x2AE8 ? 1 : -1];

extern "C" {
Phase3E330 *g_brPhase5C5C;      /* 0x10AC5C5C -- current phase */
Phase3E330 *g_brNext5CB4;       /* 0x10AC5C60 -- root phase */
int         g_brPending5CBC;    /* 0x10AC5CBC */
}

int BrPhaseLeave_10044DE0(Ctl3E330 *pCtl)
{
    pCtl->p2AE8->s7();

    if (g_brPhase5C5C != 0)
        delete g_brPhase5C5C;

    g_brPending5CBC = 0;
    g_brPhase5C5C = g_brNext5CB4;

    return 0;
}
