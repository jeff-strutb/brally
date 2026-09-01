/* @implements 0x10041F50 glide BrPhaseShutdown_10048B20
 * @cpp_kind method
 * @cpp_symbol ?BrPhaseShutdown_10048B20@@YGXH@Z
 *
 * 1668 B __stdcall(int) phase teardown. Timer spin-wait (deadline in
 * esi, Sleep import cached in edi), the full-shutdown arm zeroes the
 * mode/phase pair, frees the font-tex pointer table (do-while walk,
 * operator delete), then ~35 identical guarded blocks:
 * `if (g) { g->v7(); delete g; g = 0; <extras>; }` — delete's implicit
 * null test is the inner re-read guard (direct global rereads, EAX-form
 * Close vcall, EDX-form scalar-deleting push 1). Tail full-shutdown
 * arm: one more block, then the c58 pod (a REAL local — cached across
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
extern int g_brAA2854;
extern int g_AC300;
extern int g_brPhaseAA2904;
extern int DAT_10ac53ec;
extern int DAT_10ac5874;
extern PodObj *DAT_10ac5c58;

extern Ph *DAT_10ac5c98;
extern Ph *DAT_10ac5c64;
extern Ph *DAT_10ac5c68;
extern Ph *DAT_10ac5c6c;
extern Ph *DAT_10ac5c70;
extern Ph *DAT_10ac5c74;
extern Ph *DAT_10ac5c78;
extern Ph *DAT_10ac5c7c;
extern Ph *DAT_10ac5c80;
extern Ph *g_brPhaseAA292C;
extern Ph *DAT_10ac5c88;
extern Ph *DAT_10ac5c8c;
extern Ph *DAT_10ac5c90;
extern Ph *g_brPhaseAA293C;
extern Ph *DAT_10ac5c9c;
extern Ph *DAT_10ac5ca0;
extern Ph *DAT_10ac5ca4;
extern Ph *DAT_10ac5ca8;
extern Ph *DAT_10ac5cac;
extern Ph *DAT_10ac5cb0;
extern Ph *DAT_10ac5ce4;
extern Ph *DAT_10ac5cb4;
extern Ph *DAT_10ac5cb8;
extern Ph *DAT_10ac5cbc;
extern Ph *DAT_10ac5cc0;
extern Ph *DAT_10ac5cc4;
extern Ph *g_brAA2970;
extern Ph *g_brPhaseAA2974;
extern Ph *DAT_10ac5cd4;
extern Ph *DAT_10ac5cd8;
extern Ph *DAT_10ac5cdc;
extern Ph *DAT_10ac5ce0;
extern Ph *DAT_10ac5ce8;
extern Ph *DAT_10ac5cec;
extern Ph *DAT_10ac5cf0;
extern Ph *DAT_10ac5c60;
extern int DAT_10ac408c;
extern int DAT_10ac5d04;
extern int DAT_10ac5d0c;
extern int DAT_10ac5d00;
extern int DAT_10ac5d18;
extern int DAT_10ac5d24;
extern int g_AA29F4;
extern int g_brAA29B0;
extern int DAT_10ac5d10;
extern int DAT_10ac5d30;
extern int g_brPAA29D4;
extern int g_iAA2880;
extern int g_brPAA29E4;
extern int DAT_10ac5d38;
extern int DAT_10ac5d40;
extern int DAT_10ac5d1c;
extern int DAT_10ac5d28;
extern int DAT_10ac5d48;
extern int DAT_10ac5d44;

unsigned int BrSub10075020(void);
void BrFontTexFreeAll(void);
void FUN_10058a30(void);

__declspec(dllimport) void __stdcall Sleep(unsigned long);
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
        } while ((int)p < (int)&DAT_10ac5874);
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
        pPod = DAT_10ac5c58;
        if (pPod != 0) {
            pPod->m();
            operator delete(pPod);
            DAT_10ac5c58 = 0;
        }
        FUN_10058a30();
    }
}
