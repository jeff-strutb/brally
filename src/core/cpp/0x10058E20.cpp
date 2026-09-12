/* WHAT IT DOES: fills the video-mode dropdown. Walks the list of display
 * modes the driver reported, prints each as "width x height x depth" (with
 * the refresh rate when one is known) and adds it to the selector; the mode
 * matching the CURRENT display settings becomes the selection, and failing
 * that, plain 640x480x16 does. Returns whether there were any modes at all. */
/* @implements 0x10058E20 glide BrVidModeListFill
 * @cpp_kind free
 * @cpp_symbol ?BrVidModeListFill@@YAHXZ
 *
 * The +0x3838 selector member and its vtable follow 0x100469B0.cpp's
 * Sel3838 model, extended to slot +0x28 (set one row's tag/payload).
 * wsprintfA through the import table; the two format strings live at
 * 0x100AD740 / 0x100AD730.
 *
 * PARKED at 5 regions, 352/353 B, 111/112 insns.  Control flow, both
 * wsprintf forms, the goto-take join, the selector vcalls and the loop
 * shape all line up.  Residue is allocation only: (1) the entry compare's
 * register roles are swapped (orig: count in eax, zero web in ecx, so the
 * early `return 0` re-materialises `xor eax,eax`; ours zeroes eax and
 * reuses it, -1 insn); (2) the frame slots rotate one dword (orig
 * idx+0 / found+4 / bpp-spill+8, ours idx+0 / spill+4 / found+8), which
 * cascades every [esp+S] byte.  Dead: decl orders x3 (pM/idx/found/sz
 * permutations), block-scope vs function-scope mode fields, the zd
 * rename (allocation is not alphabetical), compare operand swap.  The
 * slot rotation is the symbol-index bucket class
 * (declaration-order-tiebreak); no spelling reached it. */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Sel58E20 {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4(char *psz, int, int, void *, int);   /* +0x10 add item */
    virtual void s5();                                   /* +0x14 */
    virtual void s6();                                   /* +0x18 */
    virtual void s7();                                   /* +0x1C */
    virtual void s8();                                   /* +0x20 */
    virtual void s9();                                   /* +0x24 */
    virtual void s10(void *pTag, int, int);              /* +0x28 set row */
};

class Ctl58E20 {
public:
    char     pad[0x3838];
    Sel58E20 m3838;                  /* +0x3838 */
};

typedef char chk_sel58e20[(unsigned)&((Ctl58E20 *)0)->m3838 == 0x3838 ? 1 : -1];

struct VideoMode {
    int        w;                    /* +0x00 */
    int        h;                    /* +0x04 */
    int        bpp;                  /* +0x08 */
    int        hz;                   /* +0x0C */
    VideoMode *pNext;                /* +0x10 */
};

extern "C" {
__declspec(dllimport) int __cdecl wsprintfA(char *, const char *, ...);
extern int        DAT_10ac5dcc;      /* nonzero: the list was built     */
extern VideoMode *DAT_10ac5dc8;      /* head of the mode list           */
extern int        DAT_10b71a48;      /* current width                   */
extern int        DAT_10b71a4c;      /* current height                  */
extern int        DAT_10b71a50;      /* current depth                   */
extern int        DAT_10b71a54;      /* current refresh                 */
extern int        DAT_10ac5bbc;      /* the selected row, twice over    */
extern int        DAT_10ac5d88;
extern Ctl58E20  *DAT_10ac5d44;      /* the video-options control       */
extern char       DAT_100aacc8;
extern char       DAT_100ad740[];    /* "%d x %d x %d @ %d Hz" style    */
extern char       DAT_100ad730[];    /* the no-refresh form             */
void Ctl58D40(void);                 /* 0x10058D40                      */
}

int BrVidModeListFill(void)
{
    VideoMode *pM;
    int        idx;
    int        found;
    int        w, h, zd, hz;
    char       sz[80];

    Ctl58D40();
    if (DAT_10ac5dcc == 0) {
        return 0;
    }
    idx   = 0;
    found = 0;
    pM = DAT_10ac5dc8;
    while (pM != 0) {
        w   = pM->w;
        h   = pM->h;
        zd  = pM->bpp;
        hz  = pM->hz;

        if (hz != 0) {
            wsprintfA(sz, DAT_100ad740, w, h, zd, hz);
            if (DAT_10b71a48 == w && DAT_10b71a4c == h
                && DAT_10b71a50 == zd && DAT_10b71a54 == hz) {
                goto take;
            }
        } else {
            wsprintfA(sz, DAT_100ad730, w, h, zd);
            if (DAT_10b71a48 == w && DAT_10b71a4c == h
                && DAT_10b71a50 == zd) {
take:
                found = 1;
                DAT_10ac5bbc = idx;
                DAT_10ac5d88 = idx;
            }
        }

        if (found == 0 && w == 0x280 && h == 0x1E0 && zd == 0x10) {
            DAT_10ac5bbc = idx;
            DAT_10ac5d88 = idx;
        }

        if (DAT_10ac5d44 != 0) {
            DAT_10ac5d44->m3838.s4(sz, 0, 1, &DAT_100aacc8, 1);
            DAT_10ac5d44->m3838.s10(pM, 0x14, idx);
        }
        pM = pM->pNext;
        idx = idx + 1;
    }
    return 1;
}
