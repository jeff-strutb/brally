/* br_objdl.c -- drawing: one scene object's display list.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * 0x1000CBA0 is the per-object half of the scene walk.  BrSceneDlBuild
 * (0x1000EAF0, br_scenedl.c) calls it once for every visible object with the
 * object's index, its surface-class bitmask and the scene block; this file
 * turns that one object into display-list commands.
 *
 * Two very different jobs share the entry point, chosen by the guard at the
 * top:
 *
 *   - the CHEAP path: three commands (pipe sync, end-of-DL marker, a branch
 *     to the object's own stored command list) and a bump of the three
 *     triangle/vertex counters.  This is what a plain opaque object costs.
 *
 *   - the EXPANDED path: the object's command list is WALKED here, command
 *     by command, and re-emitted into a second buffer with every vertex
 *     transformed into screen space and clip-coded on the way through.  That
 *     is the `while` over `pDL` further down: G_VTX (0x04) copies and
 *     transforms a vertex block, G_TRI2 (0xB1) and G_TRI1 (0xBF) drop the
 *     triangles whose three corners are all outside the same screen edge,
 *     and G_ENDDL (0xB8) stops the walk.
 *
 * The surface-class bitmask is consumed one bit per pass: the object is
 * re-emitted once per set bit, each pass reading a different 0x2b68-byte
 * slice of the scene block, so a car body and its shadow/lights sub-parts
 * come out of the same object record.
 *
 * br_scenedl.c's global naming is carried over verbatim (DAT_<rva>) so the
 * two files agree on what the display-list cursor and the counters are.  Its
 * EMIT / EMIT_SLOT macros are carried over too -- the "save the cursor, then
 * advance the global, then store through the saved copy" order is what VC5
 * emits for `*p++` on a global and is proven byte-exact there.  An include
 * set that looks redundant has already been shown elsewhere in this module
 * to move VC5's register allocation, so nothing here is trimmed on the
 * grounds that it looks unused.
 *
 * RESIDUE MAP / DEAD PROBES: see the bottom of this header, kept current as
 * the function is ground down.
 */
#ifdef BR_MATCHING_BUILD

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <string.h>

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef int            int32_t;

/* ------------------------------------------------------------------ */
/* Callees                                                            */
/* ------------------------------------------------------------------ */
int   BrPodNop();                                     /* 0x10008D60, varargs no-op logger */
void  BrRdpSetCombineLERP(uint32_t *pOut,             /* 0x1001CF90 */
                          int a0,  int b0,  int c0,  int d0,
                          int Aa0, int Ab0, int Ac0, int Ad0,
                          int a1,  int b1,  int c1,  int d1,
                          int Aa1, int Ab1, int Ac1, int Ad1);
void  BrSub_1003289F(int x, int y, int w, int h);     /* 0x1002BF50, scissor */
void  BrNodeChainReset_1000F460(void);                /* 0x1000C9C0 */
void  BrCopy8Words(void *pDst, const void *pSrc);     /* 0x100189A0 */
int   FUN_10018990(float v);                          /* 0x10018990, float -> level byte */
float FUN_10034840(const void *pv);                   /* 0x10034840, vector length */
void  FUN_100344d0(void *pv);                         /* 0x100344D0, br_dl_normalise */
void  FUN_10034af0(float *pOut, float *pA, float *pB);/* 0x10034AF0, matrix multiply */
void  FUN_1002f26b(const char *pMsg);                 /* 0x1002F26B, error logger */
void  FUN_1000dc00(char *pRec, float *pA, float *pB,  /* 0x1000DC00, triangle emit */
                   float *v0, float *v1, float *v2, float *pObjBase);

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */
extern uint32_t *DAT_106e7710;      /* display-list write cursor  */
extern uint32_t *DAT_1035f7d8;      /* expanded-batch cursor      */
extern float    *DAT_1035faec;      /* expanded-vertex cursor     */
extern int       DAT_1035fba4;      /* expanded-vertex arena base */
extern int       DAT_102e16b0;      /* expanded-batch arena base  */
extern int       DAT_106eed38;      /* object table base, 0x54 stride */
extern int       DAT_106e9d88;      /* screen/state block base    */
extern int       DAT_106ed520;      /* the active screen          */
extern int       DAT_10b71538;
extern int       DAT_106ed69c;      /* scissor/clip disable       */
extern int       DAT_106ed6a8;      /* view/split flags           */
extern int       DAT_106ed6ac;
extern int       DAT_106ed6b4;
extern int       DAT_106ec798;      /* viewport rect index        */
extern uint32_t  DAT_106e7718;      /* othermode_H                */
extern uint32_t  DAT_1035fb84;      /* othermode_L low word       */
extern uint32_t  DAT_1035fb88;      /* othermode_L high word      */
extern uint32_t  DAT_1184c470;      /* segment/texture id         */
extern int       DAT_100aa00c;
extern int       DAT_10396eb0;      /* software-clip enable       */
extern int       DAT_106e772c;      /* vertex counter             */
extern int       DAT_106e7734;      /* triangle counter           */
extern int       DAT_106e86a0;      /* command counter            */
extern uint8_t   DAT_106e72f0;      /* light colour, red          */
extern uint8_t   DAT_106e86a4;      /* light colour, green        */
extern uint8_t   DAT_106e7290;      /* light colour, blue         */
extern float     DAT_100771f0;
extern float     DAT_100771f4;
extern float     DAT_100771f8;      /* 0.0f */
extern float     DAT_100771fc;
extern float     DAT_10077200;
extern float     DAT_10077204;
extern float     DAT_1007720c;
extern char      DAT_100a5da8[];    /* "BAD VTX DL" */

/* The scratch direction the sprite matrix is built from. */
typedef struct BrVec3 { float x, y, z; } BrVec3;
extern BrVec3 DAT_1035fb78;

/* The output matrix and the sprite matrix are adjacent in the image
 * (0x106E78F0 then 0x106E7930); the declarations keep that order so VC5's
 * x87 operand selection sees the same section offsets. */
extern float DAT_106e78f0[16];   /* the output matrix */
extern float DAT_106e7930[16];   /* the sprite matrix */
#define OUTM(k)  (DAT_106e78f0[k])
#define SPRM(k)  (DAT_106e7930[k])

/* The eight-byte display-list append through the GLOBAL cursor. */
#define EMIT(W0, W1) \
    { uint32_t *p_ = DAT_106e7710; DAT_106e7710 += 2; \
      p_[0] = (uint32_t)(W0); p_[1] = (uint32_t)(W1); }

/* An advanced slot handed to the combiner builder. */
#define EMIT_SLOT(S) \
    { (S) = DAT_106e7710; DAT_106e7710 += 2; }

/* Copy the command being walked through, both words, unchanged. */
#define COPY2() \
    { *pDL++ = *pCmd++; *pDL++ = *pCmd++; }

/* Hand one triangle word's three corners to the software clipper. */
#define CLIPTRI(W) \
    FUN_1000dc00(pRec, pObj + 0xb0, pObj + 0xab, \
                 pVtxBase + ((W) & 0x1f) * 8, \
                 pVtxBase + (((W) >> 8) & 0x1f) * 8, \
                 pVtxBase + (((W) >> 16) & 0x1f) * 8, pObjBase)

/* True when all three of a triangle word's corners are off the same edge. */
#define TRIOUT(W) \
    ((clip[(W) & 0x1f] & clip[((W) >> 8) & 0x1f] & clip[((W) >> 16) & 0x1f]) != 0)

/* WHAT IT DOES: turn one scene object into drawing commands.  Most objects
 * just get a three-command preamble and a jump into the model's own stored
 * command list.  The rest -- the ones the guard lets through -- are expanded
 * here instead: their command list is walked, every vertex is put through
 * the object's matrix and tagged with which screen edges it falls outside,
 * and triangles whose three corners are all outside the same edge are
 * dropped rather than handed on.  The surface-class bitmask says how many
 * times to repeat the whole thing, one pass per set bit, each pass reading
 * the next slice of the scene block. */
/* @implements 0x1000CBA0 glide BrObjDlBuild */
void BrObjDlBuild(int pRects, int idx, uint32_t cls, int bLit, int pScene)
{
    uint32_t *pCmd;
    uint32_t *pDL;
    uint32_t *pDLMark;
    uint32_t *pSave;
    uint32_t *pS;
    char     *pRec;
    float    *pVtx;
    float    *pVtxBase;
    float    *pObj;
    float    *pObjBase;
    int      *pRect;
    int       pTex;
    int       i;
    int       n;
    uint32_t  c;
    uint32_t  c2;
    uint8_t  *pFlag;
    uint8_t   fl;
    float     sx, sy, len2, k;
    float     vx, vz;
    float     m00, m01, m10, m11, m20, m21, m30, m31;
    uint8_t   clip[32];

    pVtx = DAT_1035faec;
    pDL  = DAT_1035f7d8;
    pRec = (char *)(DAT_106eed38 + idx * 0x54);
    pCmd = *(uint32_t **)(pRec + 0x44);

    if (cls == 0 || DAT_106ed520 == DAT_106e9d88 + 0x2890 ||
        (pRec[0x4d] & 2) != 0 || DAT_10b71538 == 0) {
        EMIT(0xbb001001, 0xffffffff);
        EMIT(0xe8000000, 0);
        EMIT(0x06000000, pCmd);
        DAT_106e772c += *(uint16_t *)(pRec + 0x50);
        DAT_106e7734 += *(uint16_t *)(pRec + 0x4e);
        DAT_106e86a0 += *(uint16_t *)(pRec + 0x52);
    } else {
        BrPodNop(0, 0x50, 0xfa, 0x50, 0xff);
        EMIT(0xbb001001, 0xffffffff);
        EMIT(0xe8000000, 0);
        EMIT(0xfa001700, 0xff0000ff);
        EMIT(0x06000000, pCmd);
        EMIT((DAT_1184c470 & 0xffffff) | 0xdc000000, 1);
        EMIT(0xba001001, 0);
        EMIT(0xfa001700, 0xff0000ff);
        if (bLit != 0 && DAT_106ed6ac == 0 && DAT_106ed6b4 == 0) {
            EMIT(0xb900031d, 0x504b50);
        } else {
            EMIT(0xb900031d, 0x504f50);
        }
        EMIT(0xb9000002, 1);
        EMIT(0xf9000000, 8);
        EMIT_SLOT(pS);
        BrRdpSetCombineLERP(pS, 0x3ed, 0, 0x3f4, 0, 0, 0, 0, 0x3e9,
                                0x3ed, 0, 0x3f4, 0, 0, 0, 0, 0x3e9);
        EMIT(0xb6000000, 0x70004);
        EMIT(0xba000602, 0xc0);
        EMIT(0x06000000, pDL);
        EMIT(0xe7000000, 0);
        if (DAT_106ed69c == 0) {
            pRect = (int *)(pRects + DAT_106ec798 * 0x58);
            BrSub_1003289F(pRect[0], pRect[1], pRect[2], pRect[3]);
        }
        EMIT(0xba000602, DAT_106e7718);
        EMIT(0xf9000000, 0);
        if (DAT_106ed6a8 != 0) {
            EMIT(0xb7000000, 0x30004);
        } else {
            EMIT(0xb7000000, 0x20004);
        }
        EMIT(0xba001001, DAT_100aa00c != 0 ? 0x10000 : 0);
        EMIT(0xfa001700, 0xff0000ff);
        EMIT(0xb900031d, 0);
        EMIT(0xba001402, 0x100000);
        EMIT(0xb900031d, DAT_1035fb88 | DAT_1035fb84);
        EMIT_SLOT(pS);
        BrRdpSetCombineLERP(pS, 0x3ea, 0x3e9, 0x3f5, 0x3e9, 0x3ea, 0x3e9, 0x3f5, 0x3e9,
                                0x3e8, 0, 0x3ec, 0, 0, 0, 0, 0x3e8);
        DAT_106e772c += *(uint16_t *)(pRec + 0x50) * 3;
        DAT_106e7734 += *(uint16_t *)(pRec + 0x4e) * 3;
        DAT_106e86a0 += *(uint16_t *)(pRec + 0x52) * 3;

        pVtxBase = pVtx;
        pObj     = (float *)(pScene + 0x2730);
        pDLMark  = 0;
        i        = 0;
        do {
            if ((cls & 1) != 0 &&
                ((DAT_106ed520 != DAT_106e9d88 + 0x273c &&
                  DAT_106ed520 != DAT_106e9d88 + 0x27c4) ||
                 *(int *)(DAT_106e9d88 + 0x140) != i)) {
                if (DAT_10396eb0 != 0) {
                    BrNodeChainReset_1000F460();
                }
                pDL[0] = 0xe7000000;
                pDL[1] = 0;
                pDL += 2;
                pDL[0] = 0xfb000000;
                pDL[1] = (FUN_10018990(pObj[0] * DAT_100771f4) & 0xff) |
                         ((((DAT_106e72f0 << 8) | DAT_106e86a4) << 8 |
                           DAT_106e7290) << 8);
                pObjBase = pObj - 0x9cc;
                pDL += 2;

                DAT_1035fb78.x = *pObjBase;
                DAT_1035fb78.y = pObj[-0x9cb];
                DAT_1035fb78.z = pObj[-0x9ca];
                if (DAT_1035fb78.x == DAT_100771f8 && DAT_1035fb78.y == DAT_100771f8) {
                    DAT_1035fb78.x = pObj[-0x9c4];
                    DAT_1035fb78.y = pObj[-0x9c3];
                }
                DAT_1035fb78.z = DAT_100771f8;
                sx = FUN_10034840(&DAT_1035fb78);
                if (sx < DAT_100771fc) {
                    sx = 0.5f;
                }
                len2 = FUN_10034840(&pObj[-0x9c8]);
                if (len2 < DAT_100771fc) {
                    len2 = DAT_100771fc;
                }
                pTex = *(int *)((char *)pObj + 0x294);
                sx = (DAT_10077200 / *(float *)(pTex + 0x80e0)) / sx;
                sy = (DAT_10077204 / *(float *)(pTex + 0x80e4)) / len2;
                FUN_100344d0(&DAT_1035fb78);

                memcpy(DAT_106e78f0, pRec, 0x40);

                OUTM(12) = OUTM(12) - pObj[-0x9c0];
                SPRM(0)  = DAT_1035fb78.x * sx;
                SPRM(4)  = -(-DAT_1035fb78.y * sx);
                SPRM(8)  = 0.0f;
                SPRM(12) = 512.0f;
                OUTM(13) = OUTM(13) - pObj[-0x9bf];
                SPRM(1)  = -DAT_1035fb78.y * sy;
                SPRM(5)  = DAT_1035fb78.x * sy;
                SPRM(9)  = 0.0f;
                SPRM(13) = 512.0f;
                SPRM(2)  = 0.0f;
                SPRM(6)  = 0.0f;
                SPRM(10) = 1.0f;
                SPRM(14) = 0.0f;
                SPRM(3)  = 0.0f;
                SPRM(7)  = 0.0f;
                SPRM(11) = 0.0f;
                SPRM(15) = 1.0f;
                FUN_10034af0(DAT_106e78f0, DAT_106e78f0, DAT_106e7930);

                k = OUTM(3) + OUTM(7) + OUTM(11) + OUTM(15);
                if (k == DAT_100771f8) {
                    k = 1.0f;
                } else {
                    k = DAT_100771f0 / k;
                }
                OUTM(0)  = k * OUTM(0);
                OUTM(4)  = k * OUTM(4);
                OUTM(8)  = k * OUTM(8);
                OUTM(12) = k * OUTM(12);
                OUTM(1)  = k * OUTM(1);
                OUTM(5)  = k * OUTM(5);
                OUTM(9)  = k * OUTM(9);
                OUTM(13) = k * OUTM(13);

                if (DAT_106ed69c == 0) {
                    pSave = DAT_106e7710;
                    DAT_106e7710 = pDL;
                    BrSub_1003289F(*(short *)((char *)pObj + 0x26c),
                                   *(short *)((char *)pObj + 0x272),
                                   *(short *)((char *)pObj + 0x270) -
                                       *(short *)((char *)pObj + 0x26c),
                                   *(short *)((char *)pObj + 0x26e) -
                                       *(short *)((char *)pObj + 0x272));
                    pDL = DAT_106e7710;
                    DAT_106e7710 = pSave;
                }

                while ((((int)(char *)pDL - DAT_102e16b0) & ~3) <= 0x13800) {
                    c = pCmd[0];
                    switch (c >> 24) {
                    case 0x04:
                        m00 = OUTM(0);
                        m20 = OUTM(8);
                        m10 = OUTM(4);
                        m01 = OUTM(1);
                        m21 = OUTM(9);
                        m11 = OUTM(5);
                        m30 = OUTM(12);
                        m31 = OUTM(13);
                        if (DAT_10396eb0 == 0 && pDL == pDLMark) {
                            pDL -= 2;
                            pVtx = pVtxBase;
                        }
                        *pDL++ = c;
                        n = (c >> 10) & 0x3f;
                        pCmd++;
                        if ((int)(((int)(char *)pVtx + n * 0x20 - DAT_1035fba4) & ~0x1f) >
                            32000) {
                            pDL--;
                            goto nextObj;
                        }
                        if (n > 32) {
                            FUN_1002f26b(DAT_100a5da8);
                        }
                        c2 = *pCmd++;
                        *pDL++ = (uint32_t)pVtx;
                        pDLMark  = pDL;
                        pVtxBase = pVtx;
                        pFlag    = clip;
                        while (n != 0) {
                            BrCopy8Words(pVtx, (const void *)c2);
                            vx = pVtx[0];
                            vz = pVtx[2];
                            pVtx[3] = m00 * vx + m10 * pVtx[1] + m20 * vz + m30;
                            pVtx[4] = m01 * vx + m11 * pVtx[1] + m21 * vz + m31;
                            if (pVtx[3] < DAT_100771f8) {
                                fl = 1;
                            } else if (pVtx[3] < DAT_1007720c) {
                                fl = 0;
                            } else {
                                fl = 2;
                            }
                            if (pVtx[4] < DAT_100771f8) {
                                fl |= 4;
                            } else if (DAT_1007720c <= pVtx[4]) {
                                fl |= 8;
                            }
                            *pFlag++ = fl;
                            pVtx += 8;
                            c2 += 0x20;
                            n--;
                        }
                        break;
                    case 0xb1:
                        if (DAT_106ed69c != 0) {
                            COPY2();
                            break;
                        }
                        if (TRIOUT(c)) {
                            c2 = pCmd[1];
                            if (TRIOUT(c2)) {
                                pCmd += 2;
                                break;
                            }
                            if (DAT_10396eb0 != 0) {
                                CLIPTRI(c2);
                            } else {
                                *pDL++ = 0xbf000000;
                                *pDL++ = pCmd[1] & 0xffffff;
                            }
                            pCmd += 2;
                            break;
                        }
                        c2 = pCmd[1];
                        if (TRIOUT(c2)) {
                            if (DAT_10396eb0 != 0) {
                                CLIPTRI(c);
                            } else {
                                *pDL++ = 0xbf000000;
                                *pDL++ = pCmd[0] & 0xffffff;
                            }
                            pCmd += 2;
                            break;
                        }
                        if (DAT_10396eb0 == 0) {
                            COPY2();
                            break;
                        }
                        CLIPTRI(c);
                        c2 = pCmd[1];
                        CLIPTRI(c2);
                        pCmd += 2;
                        break;
                    case 0xb8:
                        goto walkDone;
                    case 0xbf:
                        if (DAT_106ed69c != 0) {
                            COPY2();
                            break;
                        }
                        c2 = pCmd[1];
                        if (TRIOUT(c2)) {
                            pCmd += 2;
                            break;
                        }
                        if (DAT_10396eb0 == 0) {
                            COPY2();
                            break;
                        }
                        c2 = pCmd[1];
                        CLIPTRI(c2);
                        pCmd += 2;
                        break;
                    default:
                        pCmd += 2;
                        break;
                    }
                }
            walkDone:
                if (DAT_10396eb0 == 0 && pDL == pDLMark) {
                    pDL -= 2;
                    pVtx = pVtxBase;
                }
            }
        nextObj:
            i++;
            pObj += 0xada;
            cls = (uint32_t)((int)cls >> 1);
        } while (cls != 0);

        pDL[0] = 0xb8000000;
        pDL[1] = 0;
        pDL += 2;
        BrPodNop(0, 0xff, 0x80, 0x80, 0xff);
    }
    DAT_1035f7d8 = pDL;
    DAT_1035faec = pVtx;
}

#endif /* BR_MATCHING_BUILD */
