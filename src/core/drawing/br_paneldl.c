/* br_paneldl.c -- the car body-panel texture display list (0x10010FB0).
 *
 * Fresh transcription from build/ghidra_decomp/0x10010fb0.c against the
 * original bytes, 2026-09-13.  Matching arm only.
 */
#ifdef BR_MATCHING_BUILD

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
/* T2 2026-09-13 (fresh, 14 fn.py compiles): 839/843 B, 210/212 insns,
 * register-blind residue 6+8, first divergence in the prologue frame size
 * (0x14 vs 0x18).  The allocation idiom is `p = cursor; cursor += 2` (a
 * `cursor = p + 2` spelling gives lea instead of the copy-then-add).
 * RESIDUE: the panel loop's induction plan.  The original keeps the panel
 * offset (esi, += 4) and the strip word (edi, += 0x40000) in registers and
 * RELOADS the panel pointer and the 0x400 counter from their frame slots
 * each iteration; VC5 rebases the offset family onto `offset + 5` (an IV
 * in ecx, the plain offset derived as ecx-5), keeps the panel pointer in a
 * register biased to the fourth word, and spills the 0x400 counter into the
 * spent parameter slot -- one slot fewer in the frame.  Dead: the +5 term
 * as a named temp, signed / first-declared offset, word-store order, const
 * pointer, explicit-deref tests, init order, and a single panel index with
 * every counter derived from it (856 B). */
/* @t4-pass 0x10010FB0 1 2026-09-13 probes 14 bytes 839 insns 210 regions 1 rows 14 census no  (hand, fn.py variants: alloc idiom, +5 temp, declaration/init/store orders, const pointer, deref tests, single-index loop) */
/* @implements 0x10010FB0 glide BrPanelDlBuild */
void BrPanelDlBuild(short *param_1)
{
    unsigned int *p;
    unsigned int *q;
    short        *psVar4;
    int           iVar1;
    int          *local_14;
    unsigned int  local_18;
    unsigned int  local_c;
    unsigned int  uVar5;
    unsigned int  uVar6;
    int           local_8;
    int           local_4;

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
        local_4 = 0;
        if (DAT_100b2f00 > 0) {
            local_14 = (int *)((int)param_1 + 0x60);
            do {
                iVar1 = *local_14;
                if (iVar1 != 0) {
                    local_8 = 4;
                    p = BrPanelDlAlloc(); p[0] = 0x1060040; p[1] = iVar1 + 0x26d4;
                    local_18 = iVar1 + 0x11a0;
                    param_1 = (short *)(iVar1 + 0x2340);
                    do {
                        p = BrPanelDlAlloc(); p[0] = 0x40083ff; p[1] = local_18;
                        uVar5 = 0;
                        local_c = 0x400;
                        uVar6 = 0x10000;
                        psVar4 = param_1;
                        do {
                            if (psVar4[-1] != 0 || psVar4[0] != 0 || psVar4[1] != 0
                                || psVar4[0xb] != 0 || psVar4[0xc] != 0 || psVar4[0xd] != 0) {
                                q = BrPanelDlAlloc();
                                q[0] = ((uVar5 & 0xff) | 0xffffb100) << 0x10
                                     | ((uVar5 + 1) & 0xff) | (local_c & 0xff00);
                                q[1] = (uVar6 & 0xff0000) | ((uVar5 + 5) & 0xff) | (local_c & 0xff00);
                            }
                            uVar6 += 0x40000;
                            psVar4 += 0xc;
                            uVar5 += 4;
                            local_c += 0x400;
                        } while ((int)uVar6 < 0x1d0000);
                        param_1 += 0x6c;
                        local_18 += 0x480;
                        local_8--;
                    } while (local_8 != 0);
                    p = BrPanelDlAlloc(); p[0] = 0xbd000000; p[1] = 0;
                }
                local_4++;
                local_14 += 0x20;
            } while (local_4 < DAT_100b2f00);
        }
        p = BrPanelDlAlloc(); p[0] = 0xb7000000; p[1] = 0x2000;
    }
}

#endif /* BR_MATCHING_BUILD */
