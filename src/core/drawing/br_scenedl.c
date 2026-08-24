/* br_scenedl.c -- 0x1000EAF0, the frame's scene display-list builder.
 *
 * 9,354 bytes -- the second-largest function in BRGlide.dll after the race
 * step.  Called by the frame driver 0x10011FA0 (see include/br_drawcar.h);
 * emits the frame-global DL preamble (fog, combiner, othermode, texture
 * windows), walks the sorted object table at 0x106EED38 (0x54-byte records:
 * a 4x4 matrix, a DL pointer and flag words), transforms and range-checks
 * each object's matrix, then on the mirror/second pass batches the wheel
 * trail quads out of the per-wheel 500-entry rings at 0x10273690.
 *
 * Matching build only -- transcribed from build/ghidra_decomp/0x1000EAF0.c
 * against the disassembly of build/match/orig/0x1000EAF0.bin.
 *
 * STATE (2026-08-24, second pass): 9,344/9,354 bytes, 2,333/2,328
 * instructions, 33 divergence regions -- 17 with stack-slot numbers
 * masked.  Every branch, loop, call, emit and constant is verified
 * against the original bytes.  The out/view matrices are DEFINED in-TU
 * (see VC5-IDIOMS.md: fld-side depends on declaration form), which
 * reproduces the scale block; the four row transforms remain mirrored
 * under every declaration form and spelling probed, and the slot-layout
 * cascade (frame 0xd4 vs 0xdc) sits downstream of that one block.
 * The @implements tag is LIVE while iterating (match_sweep skips
 * untagged files entirely); the row in report.csv is a diff row, and
 * rule 2 forbids calling this matched in prose until it diffs clean.
 * Resume: tools/match_sweep.py src/core/drawing/br_scenedl.c, and
 * rebuild the divergence comparator from match_diff.parse_coff_obj +
 * capstone (reloc-masked byte equality per instruction, resync window,
 * optional slot-displacement masking).
 */
#ifdef BR_MATCHING_BUILD

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <math.h>

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef int            int32_t;

/* ------------------------------------------------------------------ */
/* Callees                                                            */
/* ------------------------------------------------------------------ */
void  FUN_1000e320(int a, int b, int c);              /* 0x1000E320 */
float BrFloat12MaxAbs(const float *pv);               /* 0x1002A957 */
int   BrPodNop();                                     /* 0x10008D60, varargs no-op logger */
void  BrRdpSetCombineLERP(uint32_t *pOut,             /* 0x1001CF90 */
                          int a0,  int b0,  int c0,  int d0,
                          int Aa0, int Ab0, int Ac0, int Ad0,
                          int a1,  int b1,  int c1,  int d1,
                          int Aa1, int Ab1, int Ac1, int Ad1);
void  BrDlRectCmdFlush(void);                         /* 0x1002C4A3 */
void  FUN_1002af17(void);                             /* 0x1002AF17 */
void  FUN_10008f90(void);                             /* 0x10008F90 */
float *BrMtxPoolAlloc(void);                          /* 0x10062500 (BrSub_10069490) */
void  BrGuMtxStore(const float *pSrc, float *pDst);   /* 0x10029E50 */
void  FUN_1000cba0(int a, int idx, int cls, int b, int c); /* 0x1000CBA0 */
void  BrCopy8Words(void *pDst, const void *pSrc);     /* 0x100189A0 */
float BrGroundProbeZ(const float *pPoint);            /* 0x100682C0 */

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */
extern int       DAT_106ed6a8;      /* view/split flags */
extern int       DAT_106ed6ac;
extern int       DAT_106ed6b0;
extern int       DAT_106ed6b4;
extern int       DAT_100b2f04;      /* car count */
extern int       DAT_1035fb8c;      /* object count (total) */
extern int       DAT_1035fb9c;
extern int       DAT_1035fb74;
extern uint32_t  DAT_1035fb84;      /* othermode_L low word  */
extern uint32_t  DAT_1035fb88;      /* othermode_L high word */
extern float     DAT_1035f7d0;
extern uint32_t  DAT_1035f7e0;      /* object cull mask */
extern int       DAT_100a9360;
extern int       DAT_100b3014;
extern int       DAT_102e0c9c;
extern int       DAT_102e16bc;
extern int       DAT_102e0ca0;
extern uint32_t *DAT_106e7710;      /* display-list write cursor */
extern uint32_t  DAT_106ea360;
extern uint16_t  DAT_106e770c;
extern uint32_t  DAT_100a9ec0[];    /* identity/default matrix */
extern int       DAT_1035fba0;      /* light-table pointer (value, not array) */
extern uint8_t   DAT_100a5ca8[];
extern uint8_t   DAT_100a5cb0[];
extern int       DAT_1035fb70;
extern uint32_t  DAT_106ecb40;      /* fog colour */
extern uint32_t  DAT_106e9a78;
extern uint32_t  DAT_106e7718;
extern uint32_t  DAT_106e79b0;
extern int       DAT_100aa00c;
extern uint32_t  DAT_106e72e8;
extern int       DAT_106ea3f4;
extern int       DAT_106e8204;
extern int       DAT_100aa010;
extern int       DAT_100aa044;
extern int       DAT_1035f7d4;
extern int       DAT_102e170c;
extern int       DAT_102e16a8;
extern uint16_t  DAT_1035e710[];    /* sorted object index list */
extern int       DAT_10396eb0;      /* trail-quad enable */
extern int       DAT_106eed38;      /* object table base, 0x54 stride */
extern uint16_t  DAT_10396eb4;
extern int       DAT_10396eac;
extern int       DAT_10396ea8;
extern uint32_t  DAT_106e79e0[];    /* fog colour table [4] */
extern int       DAT_106e772c;
extern int       DAT_106e7734;
extern int       DAT_106e86a0;
extern int       DAT_106eed34;      /* object name table */
extern char      DAT_100a5ea0[];    /* fallback name */
extern char      DAT_100a5db4[];    /* "Bad Final Matrix..." */
extern float     DAT_100771f8;      /* clip range constants */
extern double    DAT_10077238;
extern double    DAT_10077240;
extern float     DAT_10077248;
extern float     DAT_1007724c;
extern uint32_t *DAT_1035f7d8;      /* trail-batch patch cursor */
extern uint32_t  DAT_1184c470;      /* trail texture id */
extern uint32_t  DAT_118ec98c;      /* light texture id */
extern float    *DAT_1035faec;      /* trail vertex build cursor */
extern float     DAT_10077218;
extern int       DAT_10b71b00;
extern int       DAT_10226e80;
extern int       DAT_100a5d98[];    /* wheel index remap [4] */
extern float     DAT_10077250;
extern float     DAT_10077254;
extern float     DAT_10077258;
extern int       DAT_1035faf0[];    /* per-wheel ring heads [cars*4] */
extern int       DAT_1035f750[];    /* per-wheel ring tails [cars*4] */
extern uint8_t   DAT_10386ca8[];    /* surface class table */
extern int       DAT_102e16ac;      /* trail vertex arena base */
extern uint32_t *DAT_1035f7dc;      /* trail vertex write cursor */

/* The output and view matrices are DEFINED here, initialized, in address
 * order: VC5's x87 operand selection ranks memory operands by their known
 * section offsets, which zero-valued extern relocs cannot reproduce.
 * (0x106e78f0 precedes 0x106e9a38 in the image; the definitions keep that
 * order.) */
float DAT_106e78f0_def[16] = {1.0f};
float DAT_106e9a38_def[16] = {1.0f};
#define OUTM(k)  (DAT_106e78f0_def[k])
#define VIEW(k)  (DAT_106e9a38_def[k])

/* The trail ring: 500 segments per wheel, 4 wheels per car. */
typedef struct BrTrailSeg {
    float    x1;                    /* 0x10273690 */
    float    y1;                    /* 0x10273694 */
    float    z1;                    /* 0x10273698 */
    float    x2;                    /* 0x1027369C */
    float    y2;                    /* 0x102736A0 */
    float    z2;                    /* 0x102736A4 */
    uint32_t flags;                 /* 0x102736A8 */
} BrTrailSeg;
extern BrTrailSeg DAT_10273690[];

/* The eight-byte display-list append, inlined at every site. */
#define EMIT(W0, W1) \
    { uint32_t *p_ = DAT_106e7710; DAT_106e7710 += 2; \
      p_[0] = (uint32_t)(W0); p_[1] = (uint32_t)(W1); }

/* An advanced-slot handed to the combiner builder. */
#define EMIT_SLOT(S) \
    { (S) = DAT_106e7710; DAT_106e7710 += 2; }

/* The same append through the trail-batch's own cursor. */
#define TEMIT(W0, W1) \
    { uint32_t *q_ = pT; pT += 2; \
      q_[0] = (uint32_t)(W0); q_[1] = (uint32_t)(W1); }

/* WHAT IT DOES: build the frame's scene display list -- global state
 * preamble, every scene object's matrix + DL, then the trail quads. */
/* @implements 0x1000EAF0 glide BrSceneDlBuild */
void BrSceneDlBuild(int param_1, int param_2, int param_3, int param_4)
{
    int      i;
    int      bSolo;
    int      bTexLoaded;
    uint32_t *pS;
    int      cHead;
    int      base;
    int      nTotal;
    int      firstVis;
    int      idx;
    float    *pObj;
    uint16_t *pDst;
    int      iCar;
    int      iWheel;
    int      ring;
    int      head;
    int      slot;
    uint32_t *pT;
    int      *pCar;
    uint8_t  active[32];
    int      cursor[32];

    bTexLoaded = 0;
    if (DAT_106ed6ac == 0) {
        bSolo = 1;
        if (DAT_106ed6b4 != 0) {
            bSolo = 0;
        }
    } else {
        bSolo = 0;
    }
    if (param_2 == 0) {
        i = 0;
        if (0 < DAT_100b2f04) {
            uint32_t *p = (uint32_t *)(param_4 + 0x2a00);
            do {
                p[-1] = 0x44800000;
                p[0] = 0x44800000;
                p[7] = 0;
                p[8] = 0x44800000;
                p[0xf] = 0x44800000;
                p[0x10] = 0;
                p[0x17] = 0;
                p[0x18] = 0;
                p[-6] = 0;
                p[-7] = 0;
                p[-8] = 0;
                p[-9] = 0;
                i = i + 1;
                p = p + 0xada;
            } while (i < DAT_100b2f04);
        }
        FUN_1000e320(param_1, param_3, param_4);
        DAT_1035fb9c = DAT_1035fb8c;
        base = DAT_1035fb8c;
        DAT_1035fb74 = -1;
        cHead = -1;
        DAT_1035f7d0 = BrFloat12MaxAbs(DAT_106e9a38_def);
        DAT_1035f7e0 = 0;
        if (bSolo) {
            DAT_1035f7e0 = 0x800;
        }
        if (DAT_100a9360 != 1 && DAT_100a9360 != 6 &&
            (DAT_100a9360 != 5 || *(char *)(*(int *)(param_4 + 0xe8c) + 4) != '\0')) {
            DAT_1035f7e0 = DAT_1035f7e0 | 0x4000;
        }
        if (DAT_106ed6a8 != 0 &&
            (DAT_106ed6b0 != 0 || DAT_106ed6b4 != 0 || DAT_106ed6ac != 0) &&
            DAT_100b3014 != 2 && DAT_100b3014 != 8) {
            DAT_1035f7e0 = DAT_1035f7e0 | 0x20;
        }
        DAT_102e0c9c = 0x1000;
        DAT_102e16bc = 0x1000;
        DAT_102e0ca0 = 0x1000;
    }
    BrPodNop(0, 0xff, 0x80, 0x80, 0xff);
    DAT_1035fb84 = 0xc8000000;
    if (DAT_106ed6a8 == 0) {
        DAT_1035fb84 = 0xc080000;
    }
    DAT_1035fb88 = 0x112038;
    EMIT(0x1030040, DAT_106ea360);
    EMIT(0x1060040, DAT_100a9ec0);
    EMIT(0xbc00000e, DAT_106e770c);
    EMIT(0x3840010, DAT_1035fba0);
    EMIT(0x3820010, DAT_1035fba0 + 0x10);
    EMIT(0xbc000002, 0x80000040);
    EMIT(0x3860010, DAT_100a5cb0 + DAT_1035fb70 * 0x18);
    EMIT(0x3880010, DAT_100a5ca8 + DAT_1035fb70 * 0x18);
    EMIT(0xbc00000a, DAT_106ecb40);
    EMIT(0xbc00040a, DAT_106ecb40);
    EMIT(0xbc00200a, DAT_106e9a78);
    EMIT(0xbc00240a, DAT_106e9a78);
    EMIT(0xe7000000, 0);
    EMIT(0xba001301, 0x80000);
    EMIT(0xba000903, 0xc00);
    EMIT(0xba000801, 0);
    EMIT(0xb9000002, 1);
    EMIT(0xba000602, DAT_106e7718);
    EMIT(0xba000402, DAT_106e79b0);
    EMIT(0xba001402, 0);
    EMIT(0xf9000000, 0);
    EMIT(0xba001402, 0x100000);
    EMIT(0xb900031d, DAT_1035fb88 | DAT_1035fb84);
    EMIT_SLOT(pS);
    BrRdpSetCombineLERP(pS, 0x3ea, 0x3e9, 0x3f5, 0x3e9,
                            0x3ea, 0x3e9, 0x3f5, 0x3e9,
                            0x3e8, 0, 0x3ec, 0,
                            0, 0, 0, 0x3e8);
    EMIT(0xba001102, 0);
    EMIT(0xba001001, DAT_100aa00c != 0 ? 0x10000 : 0);
    EMIT(0xba000e02, 0);
    EMIT(0xba000c02, DAT_106e72e8);
    EMIT(0xbc000006, 0);
    EMIT(0xb6000000, 0x853200);
    EMIT(0xb7000000, ((DAT_106ea3f4 ^ DAT_106e8204) ? 0x1000 : 0x2000) |
                     (DAT_100aa010 != 0 ? 0x200 : 0) |
                     (DAT_106ed6a8 != 0 ? 0x10000 : 0) | 0xa0005);
    BrDlRectCmdFlush();
    EMIT(0x1030040, DAT_106ea360);
    EMIT(0xbc00000e, DAT_106e770c);
    EMIT(0x1060040, DAT_100a9ec0);
    EMIT(0xbc000002, 0x80000040);
    EMIT(0x3860010, DAT_100a5cb0 + DAT_1035fb70 * 0x18);
    EMIT(0x3880010, DAT_100a5ca8 + DAT_1035fb70 * 0x18);
    EMIT(0xbc00000a, DAT_106ecb40);
    EMIT(0xbc00040a, DAT_106ecb40);
    EMIT(0xbc00200a, DAT_106e9a78);
    EMIT(0xbc00240a, DAT_106e9a78);
    FUN_1002af17();
    FUN_1002af17();
    EMIT(0xf9000000, 0);
    EMIT(0xb6000000, 0x53200);
    EMIT(0xb7000000, ((DAT_106ea3f4 ^ DAT_106e8204) ? 0x1000 : 0x2000) |
                     (DAT_100aa010 != 0 ? 0x200 : 0) |
                     (DAT_106ed6a8 != 0 ? 0x10000 : 0) | 0xa0005);
    EMIT(0xe7000000, 0);
    EMIT(0xba001402, 0x100000);
    EMIT(0xb900031d, DAT_1035fb88 | DAT_1035fb84);
    EMIT_SLOT(pS);
    BrRdpSetCombineLERP(pS, 0x3ea, 0x3e9, 0x3f5, 0x3e9,
                            0x3ea, 0x3e9, 0x3f5, 0x3e9,
                            0x3e8, 0, 0x3ec, 0,
                            0, 0, 0, 0x3e8);
    EMIT(0xfa001700, 0xff0000ff);
    EMIT(0xba001102, 0);
    EMIT(0xba001001, DAT_100aa00c != 0 ? 0x10000 : 0);
    EMIT(0xba000e02, 0);
    EMIT(0xba000c02, DAT_106e72e8);
    if (DAT_100aa044 == 1) {
        EMIT(0xbc000404, 1);
        EMIT(0xbc000c04, 1);
        EMIT(0xbc001404, 0xffff);
        EMIT(0xbc001c04, 0xffff);
    } else {
        EMIT(0xbc000404, 6);
        EMIT(0xbc000c04, 6);
        EMIT(0xbc001404, 0xfffa);
        EMIT(0xbc001c04, 0xfffa);
    }
    EMIT(0xbb001001, 0xffffffff);
    EMIT(0xb6000000, 0xc0000);
    EMIT(0xe8000000, 0);
    EMIT(0xf5100000, 0x7000000);
    EMIT(0xf50001f0, 0x6000000);
    EMIT(0xf5000100, 0x5000000);
    if (param_2 != 0) {
        cHead = DAT_1035fb74;
        base = DAT_1035fb9c;
        DAT_1035fb8c = DAT_1035fb74 + 1 + DAT_1035fb9c;
        i = DAT_1035fb9c;
    } else {
        FUN_10008f90();
        i = 0;
    }
    if (i < DAT_1035fb8c) {
        nTotal = cHead + base;
        firstVis = 1 - base;
        pDst = DAT_1035e710 + cHead;
        do {
            if (i == DAT_1035f7d4 || i == DAT_102e170c) {
                if (DAT_100aa044 == 1) {
                    EMIT(0xbc000404, 1);
                    EMIT(0xbc000c04, 1);
                    EMIT(0xbc001404, 0xffff);
                    EMIT(0xbc001c04, 0xffff);
                } else {
                    EMIT(0xbc000404, 6);
                    EMIT(0xbc000c04, 6);
                    EMIT(0xbc001404, 0xfffa);
                    EMIT(0xbc001c04, 0xfffa);
                }
                if (i == DAT_102e170c) {
                    EMIT(0xb7000000, 0x800000);
                }
            }
            if (i < base) {
                idx = DAT_1035e710[i];
                pObj = (float *)(DAT_106eed38 + idx * 0x54);
                if ((*(uint16_t *)(pObj + 0x12) & DAT_10396eb4) == 0 &&
                    (DAT_10396eac == 0 || idx != (uint32_t)DAT_10396ea8) &&
                    (DAT_1035f7e0 & *(uint16_t *)(pObj + 0x13)) == 0) {
                    if ((*(uint16_t *)(pObj + 0x13) & 8) == 0) goto draw;
                    if (DAT_102e0ca0 == 0x1000 && i > DAT_102e16a8) {
                        DAT_102e0ca0 = firstVis + nTotal;
                    }
                    cHead = cHead + 1;
                    pDst = pDst + 1;
                    nTotal = nTotal + 1;
                    *pDst = DAT_1035e710[i];
                }
            } else {
                idx = DAT_1035e710[nTotal - i];
                pObj = (float *)(DAT_106eed38 + idx * 0x54);
draw:
                if ((*((uint8_t *)pObj + 0x4d) & 0x20) != 0) {
                    float fMax, fMin, scale;
                    if (!bTexLoaded) {
                        bTexLoaded = 1;
                        EMIT(0x1020040, DAT_100a9ec0);
                    }
                    OUTM(12) = VIEW(4) * pObj[0xd] +
                                   VIEW(8) * pObj[0xe] +
                                   VIEW(0) * pObj[0xc] + VIEW(12) * pObj[0xf];
                    OUTM(13) = VIEW(1) * pObj[0xc] +
                                   VIEW(13) * pObj[0xf] +
                                   VIEW(9) * pObj[0xe] + VIEW(5) * pObj[0xd];
                    OUTM(14) = VIEW(2) * pObj[0xc] +
                                   VIEW(14) * pObj[0xf] +
                                   VIEW(10) * pObj[0xe] + VIEW(6) * pObj[0xd];
                    OUTM(15) = VIEW(3) * pObj[0xc] +
                                   VIEW(15) * pObj[0xf] +
                                   VIEW(11) * pObj[0xe] + VIEW(7) * pObj[0xd];
                    scale = *pObj;
                    OUTM(0) = scale * VIEW(0);
                    OUTM(1) = scale * VIEW(1);
                    OUTM(2) = scale * VIEW(2);
                    OUTM(3) = scale * VIEW(3);
                    OUTM(4) = scale * VIEW(4);
                    OUTM(5) = scale * VIEW(5);
                    OUTM(6) = scale * VIEW(6);
                    OUTM(7) = scale * VIEW(7);
                    OUTM(8) = scale * VIEW(8);
                    OUTM(9) = scale * VIEW(9);
                    OUTM(10) = scale * VIEW(10);
                    OUTM(11) = scale * VIEW(11);
                    {
                    float *pM = BrMtxPoolAlloc();
                    fMax = 0.0f;
                    fMin = 0.0f;
                    if (OUTM(12) >= DAT_100771f8) {
                        fMax = OUTM(12);
                    }
                    if (OUTM(12) <= DAT_100771f8) {
                        fMin = OUTM(12);
                    }
                    if (OUTM(13) >= fMax) {
                        fMax = OUTM(13);
                    }
                    if (OUTM(13) <= fMin) {
                        fMin = OUTM(13);
                    }
                    if (OUTM(14) >= fMax) {
                        fMax = OUTM(14);
                    }
                    if (OUTM(14) <= fMin) {
                        fMin = OUTM(14);
                    }
                    if (OUTM(15) >= fMax) {
                        fMax = OUTM(15);
                    }
                    if (OUTM(15) <= fMin) {
                        fMin = OUTM(15);
                    }
                    if (fMax > DAT_10077238 || fMin < DAT_10077240) {
                        if (i == 1) {
                            char *pName;
                            if (DAT_106eed34 != 0) {
                                pName = *(char **)(DAT_106eed34 + idx * 4);
                            } else {
                                pName = DAT_100a5ea0;
                            }
                            BrPodNop(DAT_100a5db4, idx, pName, (double)scale,
                                     (double)pObj[0], (double)pObj[1], (double)pObj[2],
                                     (double)pObj[3], (double)pObj[4], (double)pObj[5],
                                     (double)pObj[6], (double)pObj[7], (double)pObj[8],
                                     (double)pObj[9], (double)pObj[10], (double)pObj[0xb],
                                     (double)pObj[0xc], (double)pObj[0xd], (double)pObj[0xe],
                                     (double)pObj[0xf],
                                     (double)VIEW(0), (double)VIEW(1),
                                     (double)VIEW(2), (double)VIEW(3),
                                     (double)VIEW(4), (double)VIEW(5),
                                     (double)VIEW(6), (double)VIEW(7),
                                     (double)VIEW(8), (double)VIEW(9),
                                     (double)VIEW(10), (double)VIEW(11),
                                     (double)VIEW(12), (double)VIEW(13),
                                     (double)VIEW(14), (double)VIEW(15),
                                     (double)OUTM(0), (double)OUTM(1),
                                     (double)OUTM(2), (double)OUTM(3),
                                     (double)OUTM(4), (double)OUTM(5),
                                     (double)OUTM(6), (double)OUTM(7),
                                     (double)OUTM(8), (double)OUTM(9),
                                     (double)OUTM(10), (double)OUTM(11),
                                     (double)OUTM(12), (double)OUTM(13),
                                     (double)OUTM(14), (double)OUTM(15));
                        }
                        if (fMax > -fMin) {
                            fMin = DAT_10077248 / fMax;
                        } else {
                            fMin = DAT_1007724c / fMin;
                        }
                        OUTM(0) = fMin * OUTM(0);
                        OUTM(1) = fMin * OUTM(1);
                        OUTM(2) = fMin * OUTM(2);
                        OUTM(3) = fMin * OUTM(3);
                        OUTM(4) = fMin * OUTM(4);
                        OUTM(5) = fMin * OUTM(5);
                        OUTM(6) = fMin * OUTM(6);
                        OUTM(7) = fMin * OUTM(7);
                        OUTM(8) = fMin * OUTM(8);
                        OUTM(9) = fMin * OUTM(9);
                        OUTM(10) = fMin * OUTM(10);
                        OUTM(11) = fMin * OUTM(11);
                        OUTM(12) = fMin * OUTM(12);
                        OUTM(13) = fMin * OUTM(13);
                        OUTM(14) = fMin * OUTM(14);
                        OUTM(15) = fMin * OUTM(15);
                    }
                    BrGuMtxStore(DAT_106e78f0_def, pM);
                    EMIT(0x39e0010, pM);
                    EMIT(0x3980010, pM + 4);
                    EMIT(0x39a0010, pM + 8);
                    EMIT(0x39c0010, pM + 0xc);
                    }
                } else {
                    float *pM = BrMtxPoolAlloc();
                    BrGuMtxStore(pObj, pM);
                    EMIT(0x1020040, pM);
                    bTexLoaded = 0;
                }
                if ((*(uint16_t *)(pObj + 0x13) & 0x4a4) != 0) {
                    if ((*(uint16_t *)(pObj + 0x13) & 0x400) != 0) {
                        if (DAT_106ed6ac == 0 || (*(uint16_t *)(pObj + 0x13) & 0x100) == 0) {
                            EMIT(0xbc00000a, 0);
                            EMIT(0xbc00040a, 0);
                        } else {
                            EMIT(0xbc00000a, DAT_106ecb40 >> 1 & 0x7f7f7f00);
                            EMIT(0xbc00040a, DAT_106ecb40 >> 1 & 0x7f7f7f00);
                        }
                        EMIT(0xbc00200a, DAT_106e79e0[*((uint8_t *)pObj + 0x4c) & 3]);
                        EMIT(0xbc00240a, DAT_106e79e0[*((uint8_t *)pObj + 0x4c) & 3]);
                    }
                    if ((*(uint16_t *)(pObj + 0x13) & 4) != 0) {
                        EMIT(0xb6000000, 0x3000);
                    }
                    if ((*(uint16_t *)(pObj + 0x13) & 0x20) != 0 && DAT_106ed6a8 != 0 &&
                        DAT_106ed6ac == 0 && DAT_106ed6b0 == 0 && DAT_106ed6b4 == 0) {
                        EMIT(0xb6000000, 0x10000);
                    }
                    if ((*(uint16_t *)(pObj + 0x13) & 0x80) != 0 && DAT_100aa010 != 0) {
                        EMIT(0xb6000000, 0x200);
                    }
                }
                if (i > DAT_102e16a8 && i < DAT_102e0ca0) {
                    EMIT(0xbb001001, 0xffffffff);
                    EMIT(0xe8000000, 0);
                    EMIT(0x6000000, *(uint32_t *)(pObj + 0x11));
                    DAT_106e772c = DAT_106e772c + *(uint16_t *)(pObj + 0x14);
                    DAT_106e7734 = DAT_106e7734 + *(uint16_t *)((char *)pObj + 0x4e);
                    DAT_106e86a0 = DAT_106e86a0 + *(uint16_t *)((char *)pObj + 0x52);
                } else {
                    FUN_1000cba0(param_1, idx, DAT_10386ca8[idx], param_2, param_4);
                }
                if ((*(uint16_t *)(pObj + 0x13) & 0x4a4) != 0) {
                    if ((*(uint16_t *)(pObj + 0x13) & 0x80) != 0 && DAT_100aa010 != 0) {
                        EMIT(0xb7000000, 0x200);
                    }
                    if ((*((uint8_t *)pObj + 0x4d) & 4) != 0) {
                        EMIT(0xbc00000a, DAT_106ecb40);
                        EMIT(0xbc00040a, DAT_106ecb40);
                        EMIT(0xbc00200a, DAT_106e9a78);
                        EMIT(0xbc00240a, DAT_106e9a78);
                    }
                    if ((*(uint16_t *)(pObj + 0x13) & 4) != 0) {
                        EMIT(0xb7000000, ((DAT_106ea3f4 ^ DAT_106e8204) ? 0x1000 : 0x2000));
                    }
                    if (DAT_106ed6a8 != 0 && DAT_106ed6ac == 0 && DAT_106ed6b0 == 0 &&
                        DAT_106ed6b4 == 0 && (*(uint16_t *)(pObj + 0x13) & 0x20) != 0) {
                        EMIT(0xb7000000, 0x10000);
                    }
                }
            }
            i = i + 1;
        } while (i < DAT_1035fb8c);
    }
    pT = DAT_1035f7d8;
    if (param_2 != 0) {
        EMIT(0x6000000, pT);
        TEMIT(0x1060040, DAT_100a9ec0);
        TEMIT((DAT_1184c470 & 0xffffff) | 0xdc000000, 1);
        if (DAT_106ed6ac == 0 && DAT_106ed6b4 == 0) {
            TEMIT(0xb900031d, 0x504b50);
        } else {
            TEMIT(0xb900031d, 0x504f50);
        }
        TEMIT(0xb900031d, 1);
        TEMIT(0xb9000002, 1);
        TEMIT(0xf9000000, 8);
        {
        uint32_t *q_ = pT; pT += 2;
        BrRdpSetCombineLERP(q_, 0x3ed, 0, 0x3f4, 0,
                                0, 0, 0, 0x3e9,
                                0x3ed, 0, 0x3f4, 0,
                                0, 0, 0, 0x3e9);
        }
        iCar = 0;
        if (0 < DAT_100b2f04) {
            int negCar0;
            pCar = (int *)(param_4 + 0x29e0);
            negCar0 = -(param_4 + 0x29e0);
            do {
                if (DAT_10396eb0 != 0 && pCar[-1] != 0 && pCar[0] != 0 &&
                    pCar[1] != 0 && pCar[2] != 0) {
                    TEMIT((DAT_1184c470 & 0xffffff) | 0xdc000000, 1);
                    TEMIT(0x400107f, DAT_1035faec);
                    {
                    int *pW = pCar + 4;
                    pDst = (uint16_t *)4;
                    do {
                        BrCopy8Words(DAT_1035faec, pW);
                        pW = pW + 8;
                        DAT_1035faec[2] = DAT_1035faec[2] - DAT_10077218;
                        DAT_1035faec = DAT_1035faec + 8;
                        pDst = (uint16_t *)((int)pDst - 1);
                    } while (pDst != (uint16_t *)0);
                    }
                    TEMIT(0xb900031d, 1);
                    TEMIT(0xb1000103, 0x302);
                }
                if (DAT_10b71b00 != 0) {
                    iWheel = 0;
                    do {
                        int cls;
                        if (DAT_10226e80 == 2 || DAT_10226e80 == 3) {
                            if ((iWheel != 0 ||
                                 (pCar[-0xa1e] == 0 || *(int *)(pCar[-0xa1e] + 0x1b4) == 0)) &&
                                (iWheel != 1 ||
                                 (pCar[-0xa1c] == 0 || *(int *)(pCar[-0xa1c] + 0x1b4) == 0)) &&
                                (iWheel != 2 ||
                                 (pCar[-0xa1d] == 0 || *(int *)(pCar[-0xa1d] + 0x1b4) == 0)) &&
                                (iWheel != 3 ||
                                 (pCar[-0xa1b] == 0 || *(int *)(pCar[-0xa1b] + 0x1b4) == 0)))
                                goto no_mark;
                            cls = 1;
                        } else {
                            int e;
                            if ((iWheel == 0 && (e = pCar[-0xa1e]) != 0 &&
                                 *(char *)(e + 0x1a0) == '\x03' && *(int *)(e + 0x1b4) != 0) ||
                                (iWheel == 1 && (e = pCar[-0xa1c]) != 0 &&
                                 *(char *)(e + 0x1a0) == '\x03' && *(int *)(e + 0x1b4) != 0) ||
                                (iWheel == 2 && (e = pCar[-0xa1d]) != 0 &&
                                 *(char *)(e + 0x1a0) == '\x03' && *(int *)(e + 0x1b4) != 0) ||
                                (iWheel == 3 && (e = pCar[-0xa1b]) != 0 &&
                                 *(char *)(e + 0x1a0) == '\x03' && *(int *)(e + 0x1b4) != 0)) {
                                cls = *((uint8_t *)pCar + -0x2673);
                            } else {
no_mark:
                                cls = 0;
                            }
                        }
                        if (cls != 0) {
                            float dx, dy, len, ox, oy, x1, x2, y1, y2, z;
                            int iw = DAT_100a5d98[iWheel];
                            int wb = iw * 0x40 + param_4 + negCar0;
                            float *pW;
                            dx = *(float *)(wb + (int)pCar + 0x50);
                            dy = *(float *)(wb + (int)pCar + 0x54);
                            pW = (float *)(wb + (int)pCar);
                            len = (float)sqrt(dy * dy + dx * dx);
                            ox = (dx / len) * DAT_10077250;
                            oy = (dy / len) * DAT_10077250;
                            x1 = pW[0x1c] - ox;
                            x2 = ox + pW[0x1c];
                            y1 = pW[0x1d] - oy;
                            y2 = oy + pW[0x1d];
                            z = (pW[0x1e] - BrGroundProbeZ(pW + 0x1c)) -
                                DAT_10077254;
                            ring = iWheel + iCar * 4;
                            if (DAT_1035faf0[ring] != DAT_1035f750[ring]) {
                                int h1 = DAT_1035faf0[ring] - 1;
                                pDst = (uint16_t *)h1;
                                if (h1 < 0) {
                                    pDst = (uint16_t *)0x1f3;
                                }
                                if (pDst != (uint16_t *)DAT_1035f750[ring] &&
                                    (DAT_10273690[(int)pDst + ring * 500].flags & 0x8000000) ==
                                    0x8000000) {
                                    slot = (int)pDst - 1;
                                    if (slot < 0) {
                                        slot = 499;
                                    }
                                    if (slot != DAT_1035f750[ring] &&
                                        (DAT_10273690[slot + ring * 500].flags & 0x8000000) ==
                                        0x8000000) {
                                        float ex1 = DAT_10273690[slot + ring * 500].x1 - x1;
                                        float ey1 = DAT_10273690[slot + ring * 500].y1 - y1;
                                        float ex2 = DAT_10273690[slot + ring * 500].x2 - x2;
                                        float ey2 = DAT_10273690[slot + ring * 500].y2 - y2;
                                        if (ey1 * ey1 + ex1 * ex1 < DAT_10077258 &&
                                            ey2 * ey2 + ex2 * ex2 < DAT_10077258) {
                                            if (h1 < 0) {
                                                h1 = 499;
                                            }
                                            DAT_1035faf0[ring] = h1;
                                        }
                                    }
                                }
                            }
                            head = DAT_1035faf0[ring];
                            DAT_10273690[head + ring * 500].x1 = x1;
                            DAT_10273690[head + ring * 500].x2 = x2;
                            DAT_10273690[head + ring * 500].y1 = y1;
                            DAT_10273690[head + ring * 500].y2 = y2;
                            DAT_10273690[head + ring * 500].z1 = z;
                            DAT_10273690[head + ring * 500].z2 = z;
                            DAT_10273690[head + ring * 500].flags =
                                *(uint16_t *)(pCar + -0x24) | 0x8000000;
                            head = head + 1;
                            if (499 < head) {
                                head = 0;
                            }
                            DAT_1035faf0[ring] = head;
                            if (head == DAT_1035f750[ring]) {
                                head = DAT_1035f750[ring] + 1;
                                if (499 < head) {
                                    head = 0;
                                }
                                DAT_1035f750[ring] = head;
                            }
                        } else {
                            ring = iWheel + iCar * 4;
                            head = DAT_1035faf0[ring];
                            if (DAT_1035f750[ring] != head) {
                                slot = head - 1;
                                if (slot < 0) {
                                    slot = 499;
                                }
                                if ((DAT_10273690[slot + ring * 500].flags & 0x8000000) ==
                                    0x8000000) {
                                    DAT_10273690[ring * 500 + head].flags =
                                        DAT_10273690[ring * 500 + head].flags & 0xf7ffffff;
                                    head = head + 1;
                                    if (499 < head) {
                                        head = 0;
                                    }
                                    DAT_1035faf0[ring] = head;
                                    if (head == DAT_1035f750[ring]) {
                                        head = DAT_1035f750[ring] + 1;
                                        if (499 < head) {
                                            head = 0;
                                        }
                                        DAT_1035f750[ring] = head;
                                    }
                                }
                            }
                        }
                        iWheel = iWheel + 1;
                    } while (iWheel < 4);
                }
                iCar = iCar + 1;
                pCar = pCar + 0xada;
            } while (iCar < DAT_100b2f04);
        }
        if (DAT_10b71b00 != 0) {
            int again;
            TEMIT(0xb6000000, 0x3000);
            TEMIT((DAT_118ec98c & 0xffffff) | 0xdc000000, 1);
            iCar = 0;
            if (0 < DAT_100b2f04) {
                uint8_t *pA = active;
                do {
                    iWheel = 0;
                    ring = iCar * 4;
                    do {
                        head = DAT_1035faf0[ring];
                        slot = head - 1;
                        if (slot < 0) {
                            slot = 499;
                        }
                        cursor[ring] = slot;
                        pA[iWheel] = head != DAT_1035f750[ring];
                        iWheel = iWheel + 1;
                        ring = ring + 1;
                    } while (iWheel < 4);
                    iCar = iCar + 1;
                    pA = pA + 4;
                } while (iCar < DAT_100b2f04);
            }
            do {
                int dCar, dw, dring;
                again = 0;
                dCar = 0;
                if (0 < DAT_100b2f04) {
                    int rowBase = 0;
                    uint8_t *pA = active;
                    do {
                        int row = rowBase;
                        dring = dCar * 4;
                        dw = 0;
                        do {
                            if (pA[dw] != '\0') {
                                int dh, ds;
                                dh = cursor[dring];
                                if (dh == DAT_1035f750[dring]) goto dead;
                                {
                                    ds = dh - 1;
                                    if (ds < 0) {
                                        ds = 0x1f3;
                                    }
                                    {
                                    uint32_t fl = DAT_10273690[dh + row].flags & 0x8000000;
                                    if (fl == 0x8000000) {
                                        if (ds == DAT_1035f750[dring]) goto dead;
                                        if ((DAT_10273690[ds + row].flags & 0x8000000) ==
                                            0x8000000 &&
                                            ((DAT_10386ca8[DAT_10273690[dh + row].flags &
                                                           0xf7ffffff] & 0x80) == 0 ||
                                             (DAT_10386ca8[DAT_10273690[ds + row].flags &
                                                           0xf7ffffff] & 0x80) == 0)) {
                                            if (DAT_1035f7dc + 0x20 >=
                                                (uint32_t *)(DAT_102e16ac + 0x3e800))
                                                goto full;
                                            TEMIT(0x400107f, DAT_1035f7dc);
                                            DAT_1035f7dc[0] =
                                                *(uint32_t *)&DAT_10273690[ds + row].x2;
                                            DAT_1035f7dc[1] =
                                                *(uint32_t *)&DAT_10273690[ds + row].y2;
                                            DAT_1035f7dc[2] =
                                                *(uint32_t *)&DAT_10273690[ds + row].z2;
                                            DAT_1035f7dc[3] = 0;
                                            DAT_1035f7dc[4] = 0;
                                            DAT_1035f7dc = DAT_1035f7dc + 8;
                                            DAT_1035f7dc[0] =
                                                *(uint32_t *)&DAT_10273690[dh + row].x2;
                                            DAT_1035f7dc[1] =
                                                *(uint32_t *)&DAT_10273690[dh + row].y2;
                                            DAT_1035f7dc[2] =
                                                *(uint32_t *)&DAT_10273690[dh + row].z2;
                                            DAT_1035f7dc[3] = 0x44800000;
                                            DAT_1035f7dc[4] = 0;
                                            DAT_1035f7dc = DAT_1035f7dc + 8;
                                            DAT_1035f7dc[0] =
                                                *(uint32_t *)&DAT_10273690[ds + row].x1;
                                            DAT_1035f7dc[1] =
                                                *(uint32_t *)&DAT_10273690[ds + row].y1;
                                            DAT_1035f7dc[2] =
                                                *(uint32_t *)&DAT_10273690[ds + row].z1;
                                            DAT_1035f7dc[3] = 0;
                                            DAT_1035f7dc[4] = 0x44800000;
                                            DAT_1035f7dc = DAT_1035f7dc + 8;
                                            DAT_1035f7dc[0] =
                                                *(uint32_t *)&DAT_10273690[dh + row].x1;
                                            DAT_1035f7dc[1] =
                                                *(uint32_t *)&DAT_10273690[dh + row].y1;
                                            DAT_1035f7dc[2] =
                                                *(uint32_t *)&DAT_10273690[dh + row].z1;
                                            DAT_1035f7dc[3] = 0x44800000;
                                            DAT_1035f7dc[4] = 0x44800000;
                                            DAT_1035f7dc = DAT_1035f7dc + 8;
                                            TEMIT(0xb1000103, 0x302);
                                        }
                                        again = 1;
                                        cursor[dring] = ds;
                                    } else if (fl == 0) {
                                        again = 1;
                                        cursor[dring] = ds;
                                    }
                                    }
                                }
                                goto next;
dead:
                                pA[dw] = 0;
next:;
                            }
                            dw = dw + 1;
                            row = row + 500;
                            dring = dring + 1;
                        } while (dw < 4);
                        dCar = dCar + 1;
                        rowBase = rowBase + 2000;
                        pA = pA + 4;
                    } while (dCar < DAT_100b2f04);
                }
            } while (again);
full:
            TEMIT(0xb7000000,
                  ((DAT_106ea3f4 ^ DAT_106e8204) ? 0x1000 : 0x2000));
        }
        TEMIT(0xf9000000, 0);
        TEMIT(0xb900031d, 0);
        TEMIT(0xba001402, 0x100000);
        TEMIT(0xb900031d, DAT_1035fb88 | DAT_1035fb84);
        {
        uint32_t *q_ = pT; pT += 2;
        BrRdpSetCombineLERP(q_, 0x3ea, 0x3e9, 0x3f5, 0x3e9,
                                0x3ea, 0x3e9, 0x3f5, 0x3e9,
                                0x3e8, 0, 0x3ec, 0,
                                0, 0, 0, 0x3e8);
        }
        TEMIT(0xba000602, DAT_106e7718);
        TEMIT(0xbd000000, 0);
        pT[0] = 0xb8000000;
        pT[1] = 0;
        pT = pT + 4;
        DAT_1035f7d8 = pT;
    }
    if (param_2 == 0) {
        if (DAT_102e0ca0 == 0x1000) {
            DAT_102e0ca0 = cHead;
        }
        DAT_102e0ca0 = (cHead + base) - DAT_102e0ca0;
    }
    EMIT(0x1020040, DAT_100a9ec0);
    EMIT(0xb6000000, 0x10000);
    EMIT(0xe7000000, 0);
    BrPodNop(0, 0, 0xb4, 0, 0xff);
    DAT_1035fb74 = cHead;
}

#endif /* BR_MATCHING_BUILD */
