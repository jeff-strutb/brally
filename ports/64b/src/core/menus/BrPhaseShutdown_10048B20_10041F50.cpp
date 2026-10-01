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
    virtual ~Ph();
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
    if (g_brAA2854 == 2)
        deadline = 0x11da;
    else if (g_brAA2854 == 3)
        deadline = 0x604;
    deadline += BrSub10075020();
    while (BrSub10075020() < deadline)
        Sleep(0);

    if (bPartial == 0) {
        g_AC300 = 0;
        g_brPhaseAA2904 = 0;
        BrFontTexFreeAll();
        p = &DAT_10ac53ec;
        do {
            if (*(void **)p != 0)
                operator delete(*(void **)p);
            *p = 0;
            p += 2;
        } while ((uintptr_t)p < (uintptr_t)&DAT_10ac5874);
    }

    if (DAT_10ac5c98 != 0) {
        DAT_10ac5c98->v7();
        delete DAT_10ac5c98;
        DAT_10ac5c98 = 0;
        DAT_10ac408c = 0;
    }

    if (DAT_10ac5c64 != 0) {
        DAT_10ac5c64->v7();
        delete DAT_10ac5c64;
        DAT_10ac5c64 = 0;
        DAT_10ac5d04 = 0;
    }

    if (DAT_10ac5c68 != 0) {
        DAT_10ac5c68->v7();
        delete DAT_10ac5c68;
        DAT_10ac5c68 = 0;
    }

    if (DAT_10ac5c6c != 0) {
        DAT_10ac5c6c->v7();
        delete DAT_10ac5c6c;
        DAT_10ac5c6c = 0;
        DAT_10ac5d0c = 0;
    }

    if (DAT_10ac5c70 != 0) {
        DAT_10ac5c70->v7();
        delete DAT_10ac5c70;
        DAT_10ac5c70 = 0;
    }

    if (DAT_10ac5c74 != 0) {
        DAT_10ac5c74->v7();
        delete DAT_10ac5c74;
        DAT_10ac5c74 = 0;
    }

    if (DAT_10ac5c78 != 0) {
        DAT_10ac5c78->v7();
        delete DAT_10ac5c78;
        DAT_10ac5c78 = 0;
        DAT_10ac5d00 = 0;
    }

    if (DAT_10ac5c7c != 0) {
        DAT_10ac5c7c->v7();
        delete DAT_10ac5c7c;
        DAT_10ac5c7c = 0;
    }

    if (DAT_10ac5c80 != 0) {
        DAT_10ac5c80->v7();
        delete DAT_10ac5c80;
        DAT_10ac5c80 = 0;
        DAT_10ac5d18 = 0;
        DAT_10ac5d24 = 0;
        g_AA29F4 = 0;
    }

    if (g_brPhaseAA292C != 0) {
        g_brPhaseAA292C->v7();
        delete g_brPhaseAA292C;
        g_brPhaseAA292C = 0;
        g_brAA29B0 = 0;
    }

    if (DAT_10ac5c88 != 0) {
        DAT_10ac5c88->v7();
        delete DAT_10ac5c88;
        DAT_10ac5c88 = 0;
    }

    if (DAT_10ac5c8c != 0) {
        DAT_10ac5c8c->v7();
        delete DAT_10ac5c8c;
        DAT_10ac5c8c = 0;
    }

    if (DAT_10ac5c90 != 0) {
        DAT_10ac5c90->v7();
        delete DAT_10ac5c90;
        DAT_10ac5c90 = 0;
    }

    if (g_brPhaseAA293C != 0) {
        g_brPhaseAA293C->v7();
        delete g_brPhaseAA293C;
        g_brPhaseAA293C = 0;
    }

    if (DAT_10ac5c98 != 0) {
        DAT_10ac5c98->v7();
        delete DAT_10ac5c98;
        DAT_10ac5c98 = 0;
    }

    if (DAT_10ac5c9c != 0) {
        DAT_10ac5c9c->v7();
        delete DAT_10ac5c9c;
        DAT_10ac5c9c = 0;
    }

    if (DAT_10ac5ca0 != 0) {
        DAT_10ac5ca0->v7();
        delete DAT_10ac5ca0;
        DAT_10ac5ca0 = 0;
        DAT_10ac5d10 = 0;
        DAT_10ac5d30 = 0;
        g_brPAA29D4 = 0;
        g_iAA2880 = 0;
    }

    if (DAT_10ac5ca4 != 0) {
        DAT_10ac5ca4->v7();
        delete DAT_10ac5ca4;
        DAT_10ac5ca4 = 0;
        DAT_10ac5d10 = 0;
    }

    if (DAT_10ac5ca8 != 0) {
        DAT_10ac5ca8->v7();
        delete DAT_10ac5ca8;
        DAT_10ac5ca8 = 0;
    }

    if (DAT_10ac5cac != 0) {
        DAT_10ac5cac->v7();
        delete DAT_10ac5cac;
        DAT_10ac5cac = 0;
        g_brPAA29E4 = 0;
        DAT_10ac5d38 = 0;
    }

    if (DAT_10ac5cb0 != 0) {
        DAT_10ac5cb0->v7();
        delete DAT_10ac5cb0;
        DAT_10ac5cb0 = 0;
        DAT_10ac5d00 = 0;
    }

    if (DAT_10ac5ce4 != 0) {
        DAT_10ac5ce4->v7();
        delete DAT_10ac5ce4;
        DAT_10ac5ce4 = 0;
        DAT_10ac5d40 = 0;
    }

    if (DAT_10ac5cb4 != 0) {
        DAT_10ac5cb4->v7();
        delete DAT_10ac5cb4;
        DAT_10ac5cb4 = 0;
    }

    if (DAT_10ac5cb8 != 0) {
        DAT_10ac5cb8->v7();
        delete DAT_10ac5cb8;
        DAT_10ac5cb8 = 0;
    }

    if (DAT_10ac5cbc != 0) {
        DAT_10ac5cbc->v7();
        delete DAT_10ac5cbc;
        DAT_10ac5cbc = 0;
    }

    if (DAT_10ac5cc0 != 0) {
        DAT_10ac5cc0->v7();
        delete DAT_10ac5cc0;
        DAT_10ac5cc0 = 0;
        DAT_10ac5d1c = 0;
        DAT_10ac5d28 = 0;
    }

    if (DAT_10ac5cc4 != 0) {
        DAT_10ac5cc4->v7();
        delete DAT_10ac5cc4;
        DAT_10ac5cc4 = 0;
    }

    if (g_brAA2970 != 0) {
        g_brAA2970->v7();
        delete g_brAA2970;
        g_brAA2970 = 0;
    }

    if (g_brPhaseAA2974 != 0) {
        g_brPhaseAA2974->v7();
        delete g_brPhaseAA2974;
        g_brPhaseAA2974 = 0;
    }

    if (DAT_10ac5cd4 != 0) {
        DAT_10ac5cd4->v7();
        delete DAT_10ac5cd4;
        DAT_10ac5cd4 = 0;
    }

    if (DAT_10ac5cd8 != 0) {
        DAT_10ac5cd8->v7();
        delete DAT_10ac5cd8;
        DAT_10ac5cd8 = 0;
    }

    if (DAT_10ac5cdc != 0) {
        DAT_10ac5cdc->v7();
        delete DAT_10ac5cdc;
        DAT_10ac5cdc = 0;
    }

    if (DAT_10ac5ce0 != 0) {
        DAT_10ac5ce0->v7();
        delete DAT_10ac5ce0;
        DAT_10ac5ce0 = 0;
    }

    if (DAT_10ac5ce8 != 0) {
        DAT_10ac5ce8->v7();
        delete DAT_10ac5ce8;
        DAT_10ac5ce8 = 0;
        DAT_10ac5d48 = 0;
    }

    if (DAT_10ac5cec != 0) {
        DAT_10ac5cec->v7();
        delete DAT_10ac5cec;
        DAT_10ac5cec = 0;
        DAT_10ac5d44 = 0;
    }

    if (DAT_10ac5cf0 != 0) {
        DAT_10ac5cf0->v7();
        delete DAT_10ac5cf0;
        DAT_10ac5cf0 = 0;
    }

    if (bPartial == 0) {
        if (DAT_10ac5c60 != 0) {
            DAT_10ac5c60->v7();
            delete DAT_10ac5c60;
            DAT_10ac5c60 = 0;
        }
        pPod = (PodObj *)(DAT_10ac5c58);
        if (pPod != 0) {
            pPod->m();
            operator delete(pPod);
            DAT_10ac5c58 = 0;
        }
        FUN_10058a30();
    }
}
