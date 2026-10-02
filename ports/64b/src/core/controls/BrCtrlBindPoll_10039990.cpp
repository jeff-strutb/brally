#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
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
 * The constant 1 lives in edi because every case BREAKS to the one
 * `return 1` at the bottom (VC5 then duplicates the return into each
 * case, as `mov eax,edi`); a `return 1` per case spells each 1 as an
 * immediate and nothing caches it.  The mode-3 axis scan walks absolute
 * addresses, compared as SIGNED ints (`jl`, not the pointer `jb`).
 * @t4-pass 2026-09-09 probes=7 result=diff289/missing-1-cache census no
 */
#define _CRTIMP __declspec(dllimport)

class Cfg39990 {
public:
    int  Query(int a, int b);                   /* 0x10062C30 */
    void Assign(int mode, int id, int v, int k);/* 0x10062B80 */
};

/* 64-bit core: declared once, in br_globals.h or its struct's header */                 /* 0x10B71290 */

extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrDikScan_10059040: prototype in br_funcs.h */
/* BrCtrlConflicts_10039870: prototype in br_funcs.h */
/* BrInputPollButton_100704E0: prototype in br_funcs.h */
/* BrInputPollPressed_100705F0: prototype in br_funcs.h */
}

extern "C" int BrCtrlBindPoll_10039990(void)
{
    int  v;
    char buf[256];
    int  r;
    int  i;

    if ((*(int *)&BrGlNavOff5B9C) != 0) {
        r = BrFn1005FFD0(buf);
        DAT_100abe40 = r;
        if (r == -1 && DAT_10ac5d90 != 0) {
            DAT_10ac6744 = 0;
            g_5C30 = 0;
            (*(int *)&BrGlNavOff5B9C) = 0;
            DAT_10ac5d90 = 0;
            return 1;
        }
        switch (g_brKind5D64) {
        case 0:
            if (r >= 0) {
                DAT_10ac5d90 = 1;
                v = (*(Cfg39990 *)&g_BrCtrlCfg).Query(0, (*(int (*)[])&g_brBindAAAD4)[g_brSel5B98 * 2]);
                (*(Cfg39990 *)&g_BrCtrlCfg).Assign(0, (*(int (*)[])&g_brBindAAAD4)[g_brSel5B98 * 2], v, DAT_100abe40);
            }
            DAT_10ac5ba8 = BrCfgFindConflicts(0);
            break;
        case 1:
            if (r == -1)
                r = BrJoyScanAny(&v);
            else
                v = 0;
            if (r >= 0) {
                DAT_10ac5d90 = 1;
                DAT_100abe40 = r;
                (*(Cfg39990 *)&g_BrCtrlCfg).Assign(1, (*(int (*)[])&g_brBindAAAD4)[g_brSel5B98 * 2], v, r);
            }
            DAT_10ac5ba8 = BrCfgFindConflicts(1);
            break;
        case 2:
            if (r == -1)
                r = BrJoyScanAny(&v);
            else
                v = 0;
            if (r >= 0) {
                DAT_10ac5d90 = 1;
                DAT_100abe40 = r;
                (*(Cfg39990 *)&g_BrCtrlCfg).Assign(2, (*(int (*)[])&g_brBindAAAD4)[g_brSel5B98 * 2], v, r);
            }
            DAT_10ac5ba8 = BrCfgFindConflicts(2);
            break;
        case 3:
            if (r == -1) {
                r = BrInputPollPressed();
                v = 0x300;
            } else {
                v = 0;
            }
            if (r >= 0) {
                DAT_100abe40 = r;
                (*(Cfg39990 *)&g_BrCtrlCfg).Assign(3, (*(int (*)[])&g_brBindAAAD4)[g_brSel5B98 * 2], v, r);
                DAT_10ac5d94 = 1;
            }
            DAT_10ac5ba8 = BrCfgFindConflicts(3);
            if (DAT_10ac5d94 != 0) {
                BrInputPollPressed();
                {
                    int *p;
                    for (p = &(*(int *)&BrGlNavEdge6720); (int)p < (int)(&(*(int *)&BrGlNavEdge6720) + 4); p++) {
                        if (*p != 0)
                            return 1;
                    }
                }
                DAT_10ac5d94 = 0;
                DAT_10ac5d90 = 1;
            }
            break;
        }
    }
    return 1;
}
