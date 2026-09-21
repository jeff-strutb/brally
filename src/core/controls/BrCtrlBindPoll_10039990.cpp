/* WHAT IT DOES: one polling step of the control-binding screen.  Reads the
 * pending key through the DIK scan (0x10059040); a -1 with the done-flag
 * already up tears the binding session down (four globals cleared).  Then
 * per bind mode (0 key, 1/2 buttons, 3 axis) it resolves the raw input --
 * mode 0 queries the current assignment first, modes 1/2 fall back to the
 * pressed-button poll, mode 3 to the axis poll with a 0x300 flags word --
 * and hands (mode, slot id, flags, input) to the global input-config
 * object's assign method, then refreshes the conflict list.  Mode 3 stays
 * armed until every axis in 0x10AC6720..2F reads zero.  Always returns 1. */
/* @implements 0x10039990 glide BrCtrlBindPoll_10039990
 * @cpp_kind function
 * @cpp_symbol _BrCtrlBindPoll_10039990
 *
 * Free cdecl function; the C++ part is the two thiscall calls on the
 * GLOBAL config object at 0x10B71290 (`mov ecx, imm`), which the C lane
 * cannot spell without an edx write.
 *
 * PARKED at 592 vs 540 B (289 positional diffs), structure complete: the
 * whole gap is that the original keeps BOTH constants in callee-saved
 * registers (esi=0, edi=1: flag stores, the mode-1 call args, and every
 * `return 1` are `mov eax,edi`) while ours caches only the zero (in edi)
 * and spells every 1 as an immediate.  DEAD: `int ok = 1` (declared first
 * or last, plain or `register`, used for returns only or for every 1-site
 * including the two mode-1 arguments) constant-folds away entirely; the
 * mode-3 axis scan as an explicit do/while pointer walk gets its first
 * iteration peeled, as an indexed for loop it compiles offset-based
 * (xor eax,eax / cmp eax,0x10) where the original walks absolute
 * addresses -- the for-pointer spelling keeps addresses and no peel and
 * is the closest.  The 0-cache reproduces on its own; no spelling found
 * that seats the second (1) cache -- same family as the BrSfxBankLoad
 * constant-cache levers but in the opposite direction.
 * @t4-pass 2026-09-09 probes=7 result=diff289/missing-1-cache census no
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Cfg39990 {
public:
    int  Query(int a, int b);                   /* 0x10062C30 */
    void Assign(int mode, int id, int v, int k);/* 0x10062B80 */
};

extern Cfg39990 g_Cfg_10B71290;                 /* 0x10B71290 */

extern "C" {
extern int DAT_10ac5b9c;        /* 0x10AC5B9C binding session active   */
extern int DAT_100abe40;        /* 0x100ABE40 last raw input           */
extern int DAT_10ac5d90;        /* 0x10AC5D90 done flag                */
extern int DAT_10ac6744;        /* 0x10AC6744 cleared on teardown      */
extern int DAT_10ac5c30;        /* 0x10AC5C30 cleared on teardown      */
extern int DAT_10ac5d64;        /* 0x10AC5D64 bind mode 0..3           */
extern int DAT_10ac5b98;        /* 0x10AC5B98 slot index               */
extern int DAT_100aaad4[];      /* 0x100AAAD4 slot table, stride 8     */
extern int DAT_10ac5ba8;        /* 0x10AC5BA8 conflict-list result     */
extern int DAT_10ac5d94;        /* 0x10AC5D94 mode-3 armed flag        */
extern int DAT_10ac6720;        /* 0x10AC6720 four axis accumulators   */
int BrDikScan_10059040(char *pBuf);             /* 0x10059040 */
int BrCtrlConflicts_10039870(int mode);         /* 0x10039870 */
int BrInputPollButton_100704E0(int *pFlags);    /* 0x100704E0 */
int BrInputPollPressed_100705F0(void);          /* 0x100705F0 */
}

extern "C" int BrCtrlBindPoll_10039990(void)
{
    int  v;
    char buf[256];
    int  r;
    int *p;

    if (DAT_10ac5b9c != 0) {
        r = BrDikScan_10059040(buf);
        DAT_100abe40 = r;
        if ((r == -1) && (DAT_10ac5d90 != 0)) {
            DAT_10ac6744 = 0;
            DAT_10ac5c30 = 0;
            DAT_10ac5b9c = 0;
            DAT_10ac5d90 = 0;
            return 1;
        }
        switch (DAT_10ac5d64) {
        case 0:
            if (r >= 0) {
                DAT_10ac5d90 = 1;
                v = g_Cfg_10B71290.Query(0, DAT_100aaad4[DAT_10ac5b98 * 2]);
                g_Cfg_10B71290.Assign(0, DAT_100aaad4[DAT_10ac5b98 * 2], v,
                                      DAT_100abe40);
            }
            DAT_10ac5ba8 = BrCtrlConflicts_10039870(0);
            return 1;
        case 1:
            if (r == -1) {
                r = BrInputPollButton_100704E0(&v);
            } else {
                v = 0;
            }
            if (r >= 0) {
                DAT_10ac5d90 = 1;
                DAT_100abe40 = r;
                g_Cfg_10B71290.Assign(1, DAT_100aaad4[DAT_10ac5b98 * 2], v, r);
            }
            DAT_10ac5ba8 = BrCtrlConflicts_10039870(1);
            return 1;
        case 2:
            if (r == -1) {
                r = BrInputPollButton_100704E0(&v);
            } else {
                v = 0;
            }
            if (r >= 0) {
                DAT_10ac5d90 = 1;
                DAT_100abe40 = r;
                g_Cfg_10B71290.Assign(2, DAT_100aaad4[DAT_10ac5b98 * 2], v, r);
            }
            DAT_10ac5ba8 = BrCtrlConflicts_10039870(2);
            return 1;
        case 3:
            if (r == -1) {
                r = BrInputPollPressed_100705F0();
                v = 0x300;
            } else {
                v = 0;
            }
            if (r >= 0) {
                DAT_100abe40 = r;
                g_Cfg_10B71290.Assign(3, DAT_100aaad4[DAT_10ac5b98 * 2], v, r);
                DAT_10ac5d94 = 1;
            }
            DAT_10ac5ba8 = BrCtrlConflicts_10039870(3);
            if (DAT_10ac5d94 != 0) {
                BrInputPollPressed_100705F0();
                for (p = &DAT_10ac6720; (int)p < 0x10ac6730; p++) {
                    if (*p != 0) {
                        return 1;
                    }
                }
                DAT_10ac5d94 = 0;
                DAT_10ac5d90 = 1;
            }
        }
    }
    return 1;
}
