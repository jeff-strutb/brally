/* br_paneldl.c -- the car body-panel texture display list (0x10010FB0).
 *
 * Fresh transcription from build/ghidra_decomp/0x10010fb0.c against the
 * original bytes, 2026-09-13.  Matching arm only.
 */

extern unsigned int *DAT_106e7710;      /* display-list cursor */
extern int           DAT_106ed6b0;
extern int           DAT_100b3014;
extern int           DAT_100b2f00;      /* car count */
extern unsigned int  DAT_106ea360;
extern unsigned int  DAT_1184c468;

void FUN_1001cf90(unsigned int *p, int, int, int, int, int, int, int, int,
                  int, int, int, int, int, int, int, int);

/* The original's allocation idiom: read the cursor, bump it by 8, write. */
static __inline unsigned int *BrPanelDlAlloc(void)
{
    unsigned int *p = DAT_106e7710;
    DAT_106e7710 += 2;
    return p;
}

/* WHAT IT DOES: emits the drawing commands that paint the damage panels
 * onto every car's body texture: a fixed set-up (pipe sync, blend mode,
 * combiner, tile), then for each car with a body texture four rows of
 * twelve panel tiles, loading each row's strip and drawing only the panels
 * whose damage words are non-zero, and a final flush. Does nothing unless
 * the panel effect is enabled or the game is in one of the two modes that
 * always show it. */
/* The loop is the N64 build's skid-mark pass (BrSkidDraw in src/tgrally/drawing/
 * particles.c): per car, four strips, and per strip seven quads drawn as one
 * G_TRI2 (v, v+4, v+1 / v+1, v+4, v+5) when any of the quad's six words is
 * set.  Three spellings are load-bearing: the strip address is a second loop
 * variable stepped in the for header, the w1 high vertex is formed before the
 * test (which orders VC5's induction variables as the original's), and the
 * w0 opcode half keeps the sign-extended G_TRI2 byte unfolded. */
/* @t4-pass 0x10010FB0 1 2026-09-13 probes 14 bytes 839 insns 210 regions 1 rows 14 census no  (hand, fn.py variants: alloc idiom, +5 temp, declaration/init/store orders, const pointer, deref tests, single-index loop) */
/* @t4-pass 0x10010FB0 2 2026-09-13 probes 88 bytes 839 insns 210 regions 5 rows 28 census yes  (tools/crank.py) */
/* @t4-pass 0x10010FB0 3 2026-09-20 probes 12 bytes 839 insns 210 regions 5 rows 28 census yes  (regrouping (uVar5+5)&0xff to defeat the +5 induction variable moved nothing) */
/* @t4-pass 0x10010FB0 4 2026-09-20 probes 10 bytes 839 insns 210 regions 5 rows 28 census no   (baseline reconfirm; the panel-loop IV plan + one-fewer frame slot are allocation, per header) */
/* @implements 0x10010FB0 glide BrPanelDlBuild */
void BrPanelDlBuild(short *param_1)
{
    unsigned int *p;
    unsigned int *q;
    short        *pt;
    int           car;
    int           i, g;
    unsigned int  hi;
    int           strip;
    int           v;

    if (DAT_106ed6b0 == 0 || DAT_100b3014 == 2 || DAT_100b3014 == 8) {
        p = BrPanelDlAlloc(); p[0] = 0xe7000000; p[1] = 0;
        p = BrPanelDlAlloc(); p[0] = 0xba001402; p[1] = 0x100000;
        p = BrPanelDlAlloc();
        FUN_1001cf90(p, 0, 0, 0, 0x3ec, 0x3e9, 0, 0x3ec, 0, 0, 0, 0, 1000, 0, 0, 0, 1000);
        p = BrPanelDlAlloc(); p[0] = 0xb900031d; p[1] = 0xc184a50;
        p = BrPanelDlAlloc(); p[0] = 0xb7000000; p[1] = 4;
        p = BrPanelDlAlloc(); p[0] = 0xb6000000; p[1] = 0x23000;
        p = BrPanelDlAlloc(); p[0] = 0x1030040;  p[1] = DAT_106ea360;
        p = BrPanelDlAlloc(); p[0] = (DAT_1184c468 & 0xffffff) | 0xdc000000; p[1] = 1;
        p = BrPanelDlAlloc(); p[0] = 0xf2002002; p[1] = 0x7e0fe;
        p = BrPanelDlAlloc(); p[0] = 0xbb000001; p[1] = 0xffffffff;
        for (i = 0; i < DAT_100b2f00; i++) {
            car = *(int *)((char *)param_1 + 0x60 + i * 0x80);
            if (car == 0)
                continue;
            p = BrPanelDlAlloc(); p[0] = 0x1060040; p[1] = car + 0x26d4;
            for (g = 0, strip = car + 0x11a0; g < 4; g++, strip += 0x480) {
                p = BrPanelDlAlloc(); p[0] = 0x40083ff; p[1] = strip;
                pt = (short *)(car + 0x2340 + g * 0xd8);
                for (v = 0; v < 28; v += 4) {
                    hi = (v + 1) << 16;
                    if (pt[-1] != 0 || pt[0] != 0 || pt[1] != 0
                        || pt[0xb] != 0 || pt[0xc] != 0 || pt[0xd] != 0) {
                        q = BrPanelDlAlloc();
                        q[0] = ((v & 0xff) | 0xffffb100) << 0x10
                             | (((v + 4) << 8) & 0xff00) | ((v + 1) & 0xff);
                        q[1] = (hi & 0xff0000) | (((v + 4) << 8) & 0xff00) | ((v + 5) & 0xff);
                    }
                    pt += 0xc;
                }
            }
            p = BrPanelDlAlloc(); p[0] = 0xbd000000; p[1] = 0;
        }
        p = BrPanelDlAlloc(); p[0] = 0xb7000000; p[1] = 0x2000;
    }
}

