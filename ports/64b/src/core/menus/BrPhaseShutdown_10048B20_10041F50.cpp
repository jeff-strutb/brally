#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_ui.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
/* WHAT IT DOES: shut a phase down, waiting up to a mode-dependent deadline
 * for outstanding work to finish before releasing anything. */
/* @implements 0x10041F50 glide BrPhaseShutdown_10048B20
 * @cpp_kind method
 * @cpp_symbol ?BrPhaseShutdown_10048B20@@YGXH@Z
 *
 * 1668 B __stdcall(int) phase teardown. Timer spin-wait (deadline in
 * esi, Sleep import cached in edi), the full-shutdown arm zeroes the
 * mode/phase pair, frees the font-tex pointer table (do-while walk,
 * operator delete), then ~35 identical guarded blocks:
 * `if (g) { g->v7(); delete g; g = 0; <extras>; }`: delete's implicit
 * null test is the inner re-read guard (direct global rereads, EAX-form
 * Close vcall, EDX-form scalar-deleting push 1). Tail full-shutdown
 * arm: one more block, then the c58 pod (a REAL local: cached across
 * the non-virtual thiscall, plain operator delete), and the final
 * helper. Transcribed from the Ghidra draft block order.
 */
class Ph {
public:
    virtual void *ScalarDtor_(unsigned);   /* slot 0: the scalar deleting destructor */
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual void v4();
    virtual void v5();
    virtual void v6();
    virtual void v7();          /* +0x1C */
};

class PodObj {
public:
    void m();                   /* 0x10008D60 */
};

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
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrSub10075020: prototype in br_funcs.h */
/* BrFontTexFreeAll: prototype in br_funcs.h */
/* FUN_10058a30: prototype in br_funcs.h */

/* 64-bit core: Sleep is declared by the platform headers */
}

void __stdcall BrPhaseShutdown_10048B20(int bPartial)
{
    unsigned int deadline;
    int *p;
    PodObj *pPod;

    deadline = 0;
    if (g_track == 2)
        deadline = 0x11da;
    else if (g_track == 3)
        deadline = 0x604;
    deadline += BrSub10075020();
    while (BrSub10075020() < deadline)
        Sleep(0);

    if (bPartial == 0) {
        (*(int *)&g_AC300) = 0;
        g_brPAA29B8 = 0;
        BrFontTexFreeAll();
        p = &(*(int *)&g_img[0].path);
        do {
            if (*(void **)p != 0)
                operator delete(*(void **)p);
            *p = 0;
            p += 2;
        } while ((uintptr_t)p < (uintptr_t)&(*(int *)&g_aBrAA2518[4]));
    }

    if ((*(Ph * *)&g_5C98) != 0) {
        (*(Ph * *)&g_5C98)->v7();
        br_vdelete((*(Ph * *)&g_5C98));
        (*(Ph * *)&g_5C98) = 0;
        g_guardA = 0;
    }

    if ((*(Ph * *)&g_5C64) != 0) {
        (*(Ph * *)&g_5C64)->v7();
        br_vdelete((*(Ph * *)&g_5C64));
        (*(Ph * *)&g_5C64) = 0;
        g_hookObj = 0;
    }

    if ((*(Ph * *)&g_5C68) != 0) {
        (*(Ph * *)&g_5C68)->v7();
        br_vdelete((*(Ph * *)&g_5C68));
        (*(Ph * *)&g_5C68) = 0;
    }

    if ((*(Ph * *)&g_5C6C) != 0) {
        (*(Ph * *)&g_5C6C)->v7();
        br_vdelete((*(Ph * *)&g_5C6C));
        (*(Ph * *)&g_5C6C) = 0;
        DAT_10ac5d0c = 0;
    }

    if ((*(Ph * *)&g_5C70) != 0) {
        (*(Ph * *)&g_5C70)->v7();
        br_vdelete((*(Ph * *)&g_5C70));
        (*(Ph * *)&g_5C70) = 0;
    }

    if ((*(Ph * *)&g_5C74) != 0) {
        (*(Ph * *)&g_5C74)->v7();
        br_vdelete((*(Ph * *)&g_5C74));
        (*(Ph * *)&g_5C74) = 0;
    }

    if (DAT_10ac5c78 != 0) {
        DAT_10ac5c78->v7();
        br_vdelete(DAT_10ac5c78);
        DAT_10ac5c78 = 0;
        DAT_10ac5d00 = 0;
    }

    if ((*(Ph * *)&g_5C7C) != 0) {
        (*(Ph * *)&g_5C7C)->v7();
        br_vdelete((*(Ph * *)&g_5C7C));
        (*(Ph * *)&g_5C7C) = 0;
    }

    if ((*(Ph * *)&g_5C80) != 0) {
        (*(Ph * *)&g_5C80)->v7();
        br_vdelete((*(Ph * *)&g_5C80));
        (*(Ph * *)&g_5C80) = 0;
        DAT_10ac5d18 = 0;
        g_5D24 = 0;
        g_brUipAA29F4 = 0;
    }

    if (g_brPhaseAA292C != 0) {
        g_brPhaseAA292C->v7();
        br_vdelete(g_brPhaseAA292C);
        g_brPhaseAA292C = 0;
        DAT_10ac5d08 = 0;
    }

    if ((*(Ph * *)&g_5C88) != 0) {
        (*(Ph * *)&g_5C88)->v7();
        br_vdelete((*(Ph * *)&g_5C88));
        (*(Ph * *)&g_5C88) = 0;
    }

    if ((*(Ph * *)&g_5C8C) != 0) {
        (*(Ph * *)&g_5C8C)->v7();
        br_vdelete((*(Ph * *)&g_5C8C));
        (*(Ph * *)&g_5C8C) = 0;
    }

    if ((*(Ph * *)&g_5C90) != 0) {
        (*(Ph * *)&g_5C90)->v7();
        br_vdelete((*(Ph * *)&g_5C90));
        (*(Ph * *)&g_5C90) = 0;
    }

    if ((*(Ph * *)&g_5C94) != 0) {
        (*(Ph * *)&g_5C94)->v7();
        br_vdelete((*(Ph * *)&g_5C94));
        (*(Ph * *)&g_5C94) = 0;
    }

    if ((*(Ph * *)&g_5C98) != 0) {
        (*(Ph * *)&g_5C98)->v7();
        br_vdelete((*(Ph * *)&g_5C98));
        (*(Ph * *)&g_5C98) = 0;
    }

    if (DAT_10ac5c9c != 0) {
        DAT_10ac5c9c->v7();
        br_vdelete(DAT_10ac5c9c);
        DAT_10ac5c9c = 0;
    }

    if ((*(Ph * *)&g_2948) != 0) {
        (*(Ph * *)&g_2948)->v7();
        br_vdelete((*(Ph * *)&g_2948));
        (*(Ph * *)&g_2948) = 0;
        g_29B8 = 0;
        g_brPAA29D8 = 0;
        g_brPAA29D4 = 0;
        g_5BD8 = 0;
    }

    if ((*(Ph * *)&g_294C) != 0) {
        (*(Ph * *)&g_294C)->v7();
        br_vdelete((*(Ph * *)&g_294C));
        (*(Ph * *)&g_294C) = 0;
        g_29B8 = 0;
    }

    if ((*(Ph * *)&g_brPAA2950) != 0) {
        (*(Ph * *)&g_brPAA2950)->v7();
        br_vdelete((*(Ph * *)&g_brPAA2950));
        (*(Ph * *)&g_brPAA2950) = 0;
    }

    if (g_5CAC != 0) {
        ((Ph *)g_5CAC)->v7();
        br_vdelete(g_5CAC);
        g_5CAC = 0;
        g_brPAA29E4 = 0;
        g_pGame = 0;
    }

    if ((*(Ph * *)&g_5CB0) != 0) {
        (*(Ph * *)&g_5CB0)->v7();
        br_vdelete((*(Ph * *)&g_5CB0));
        (*(Ph * *)&g_5CB0) = 0;
        DAT_10ac5d00 = 0;
    }

    if ((*(Ph * *)&g_298C) != 0) {
        (*(Ph * *)&g_298C)->v7();
        br_vdelete((*(Ph * *)&g_298C));
        (*(Ph * *)&g_298C) = 0;
        DAT_10ac5d40 = 0;
    }

    if ((*(Ph * *)&g_5CB4) != 0) {
        (*(Ph * *)&g_5CB4)->v7();
        br_vdelete((*(Ph * *)&g_5CB4));
        (*(Ph * *)&g_5CB4) = 0;
    }

    if (DAT_10ac5cb8 != 0) {
        DAT_10ac5cb8->v7();
        br_vdelete(DAT_10ac5cb8);
        DAT_10ac5cb8 = 0;
    }

    if ((*(Ph * *)&DAT_10ac5cbc) != 0) {
        (*(Ph * *)&DAT_10ac5cbc)->v7();
        br_vdelete((*(Ph * *)&DAT_10ac5cbc));
        (*(Ph * *)&DAT_10ac5cbc) = 0;
    }

    if ((*(Ph * *)&g_5CC0) != 0) {
        (*(Ph * *)&g_5CC0)->v7();
        br_vdelete((*(Ph * *)&g_5CC0));
        (*(Ph * *)&g_5CC0) = 0;
        DAT_10ac5d1c = 0;
        g_brPAA29D0 = 0;
    }

    if (DAT_10ac5cc4 != 0) {
        DAT_10ac5cc4->v7();
        br_vdelete(DAT_10ac5cc4);
        DAT_10ac5cc4 = 0;
    }

    if ((*(Ph * *)&DAT_10ac5cc8) != 0) {
        (*(Ph * *)&DAT_10ac5cc8)->v7();
        br_vdelete((*(Ph * *)&DAT_10ac5cc8));
        (*(Ph * *)&DAT_10ac5cc8) = 0;
    }

    if (g_brPhaseAA2974 != 0) {
        g_brPhaseAA2974->v7();
        br_vdelete(g_brPhaseAA2974);
        g_brPhaseAA2974 = 0;
    }

    if ((*(Ph * *)&g_5CD4) != 0) {
        (*(Ph * *)&g_5CD4)->v7();
        br_vdelete((*(Ph * *)&g_5CD4));
        (*(Ph * *)&g_5CD4) = 0;
    }

    if ((*(Ph * *)&g_5CD8) != 0) {
        (*(Ph * *)&g_5CD8)->v7();
        br_vdelete((*(Ph * *)&g_5CD8));
        (*(Ph * *)&g_5CD8) = 0;
    }

    if ((*(Ph * *)&g_5CDC) != 0) {
        (*(Ph * *)&g_5CDC)->v7();
        br_vdelete((*(Ph * *)&g_5CDC));
        (*(Ph * *)&g_5CDC) = 0;
    }

    if ((*(Ph * *)&g_5CE0) != 0) {
        (*(Ph * *)&g_5CE0)->v7();
        br_vdelete((*(Ph * *)&g_5CE0));
        (*(Ph * *)&g_5CE0) = 0;
    }

    if (DAT_10ac5ce8 != 0) {
        DAT_10ac5ce8->v7();
        br_vdelete(DAT_10ac5ce8);
        DAT_10ac5ce8 = 0;
        DAT_10ac5d48 = 0;
    }

    if ((*(Ph * *)&g_5CEC) != 0) {
        (*(Ph * *)&g_5CEC)->v7();
        br_vdelete((*(Ph * *)&g_5CEC));
        (*(Ph * *)&g_5CEC) = 0;
        DAT_10ac5d44 = 0;
    }

    if ((*(Ph * *)&g_5CF0) != 0) {
        (*(Ph * *)&g_5CF0)->v7();
        br_vdelete((*(Ph * *)&g_5CF0));
        (*(Ph * *)&g_5CF0) = 0;
    }

    if (bPartial == 0) {
        if ((*(Ph * *)&g_2908) != 0) {
            (*(Ph * *)&g_2908)->v7();
            br_vdelete((*(Ph * *)&g_2908));
            (*(Ph * *)&g_2908) = 0;
        }
        pPod = (PodObj *)((*(PodObj * *)&g_obj400));
        if (pPod != 0) {
            pPod->m();
            operator delete(pPod);
            (*(PodObj * *)&g_obj400) = 0;
        }
        BrRaceSettingsCommit();
    }
}

/* Methods of the local classes above that other files define: each is
 * the function at its original address, reached through its C entry. */
/* 0x10008D60: the original calls BrPodNop by address */
void PodObj::m()
{
    BrPodNop();
}
