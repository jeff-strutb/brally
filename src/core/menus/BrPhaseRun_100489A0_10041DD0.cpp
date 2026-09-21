/* WHAT IT DOES: run the phase to completion: on the first pass it saves the
 * control configuration and restarts, and afterwards it steps normally. This
 * is what makes the settings page persist a change before acting on it. */
/* @implements 0x10041DD0 glide BrPhaseRun_100489A0
 * @cpp_kind method
 * @cpp_symbol ?Run@BrPhase41@@QAEHXZ
 *
 * Phase step driver: thiscall on the phase object (`mov esi,ecx`), returns
 * int, no stack args.  Head and tail both take the "phase finished" exit:
 * FUN_10037920, the CtrlCfg save (thiscall on the 0x10B71290 object with
 * the pushed 0x10B72F48 path), idx=0, then the +0x18 vcall with a pushed 0
 * (EAX-pattern head, tail reuses the vtbl CSEd into edi across the +8
 * call).  The middle swaps g_cur to g_next around the 0x100592D0 thiscall
 * on *(0x10AC5C58), latches g_AC5BC0 = (cur==next), and walks the item
 * table: items[] at +0x14, flags[] at +0x6C, cur at +0x64, count/idx are
 * 16-bit at +0x10/+0x12; the per-item vcall is slot +4 on a re-read of
 * this->cur (EDX pattern).  No EH (no new).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class BrPhaseItem41 {
public:
    virtual int i0();
    virtual int i1();          /* slot +4, the per-item poll */
};

class BrCtrlCfg41 {
public:
    void Save(void *pPath);    /* 0x100634B0 BrGlCfgSave */
};

class BrPhase41 {
public:
    virtual int s0();
    virtual int s1();          /* +0x04 */
    virtual int s2();          /* +0x08 */
    virtual int s3();
    virtual int s4();
    virtual int s5();
    virtual int f18(void *);   /* +0x18, callee pops the one arg */

    char            pad04[0x10 - 4];
    unsigned short  count;     /* +0x10 */
    unsigned short  idx;       /* +0x12 */
    BrPhaseItem41  *items[20]; /* +0x14 */
    BrPhaseItem41  *cur;       /* +0x64 */
    int             done;      /* +0x68 */
    int             flags[1];  /* +0x6C.. */

    int  Run();
    void Hand();               /* 0x100592D0, non-virtual thiscall */
};

extern "C" {
extern BrPhase41 *g_brPhaseAA2904;   /* 0x10AC5C5C, the current phase */
extern BrPhase41 *DAT_10ac5c60;      /* the next phase */
extern BrPhase41 *DAT_10ac5c58;
extern int        DAT_10ac5bc0;
extern int        DAT_10b72f48;      /* the pushed path/record */

int FUN_10037920();
int BrDikPollAndEdge();              /* 0x10059020 */
}

extern BrCtrlCfg41 g_BrCtrlCfg;      /* 0x10B71290 */

int BrPhase41::Run()
{
    int i;

    if (this->done == 0) {
        FUN_10037920();
        g_BrCtrlCfg.Save(&DAT_10b72f48);
        this->idx = 0;
        this->f18(0);
        return 0;
    }

    this->s1();

    {
        BrPhase41 *pSave = g_brPhaseAA2904;
        g_brPhaseAA2904 = DAT_10ac5c60;
        DAT_10ac5c58->Hand();
        g_brPhaseAA2904 = pSave;
    }

    BrDikPollAndEdge();
    DAT_10ac5bc0 = (g_brPhaseAA2904 == DAT_10ac5c60);

    this->idx = 0;
    for (i = 0; i < this->count; i++) {
        this->cur = this->items[i];
        if (this->cur == 0) {
            return 0;
        }
        this->idx = (unsigned short)i;
        if (this->flags[i] != 0) {
            if (this->cur->i1() == 0) {
                return 0;
            }
        }
    }

    this->s2();

    if (this->done == 0) {
        FUN_10037920();
        g_BrCtrlCfg.Save(&DAT_10b72f48);
        this->idx = 0;
        this->f18(0);
        return 0;
    }
    return 1;
}
