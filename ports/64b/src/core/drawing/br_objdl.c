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

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_mat.h"   /* br_globals: its objects */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include <string.h>

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef int            int32_t;

/* ------------------------------------------------------------------ */
/* Callees                                                            */
/* ------------------------------------------------------------------ */
/* BrPodNop: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */
/* BrSub_1003289F: prototype in br_funcs.h */
/* BrNodeChainReset_1000F460: prototype in br_funcs.h */
/* BrCopy8Words: prototype in br_funcs.h */
/* FUN_10018990: prototype in br_funcs.h */
/* FUN_10034840: prototype in br_funcs.h */
/* FUN_100344d0: prototype in br_funcs.h */
/* FUN_10034af0: prototype in br_funcs.h */
/* FUN_1002f26b: prototype in br_funcs.h */
/* FUN_1000dc00: prototype in br_funcs.h */

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* display-list write cursor  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* expanded-batch cursor      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* expanded-vertex cursor     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* expanded-vertex arena base */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* expanded-batch arena base  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* object table base, 0x54 stride */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* screen/state block base    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* the active screen          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* scissor/clip disable       */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* view/split flags           */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* viewport rect index        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* othermode_H                */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* othermode_L low word       */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* othermode_L high word      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* segment/texture id         */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* software-clip enable       */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* vertex counter             */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* triangle counter           */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* command counter            */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* light colour, red          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* light colour, green        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* light colour, blue         */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0.0f */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* "BAD VTX DL" */

/* The scratch direction the sprite matrix is built from. */
/* BrVec3: br_vec.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* The output matrix and the sprite matrix are adjacent in the image
 * (0x106E78F0 then 0x106E7930); the declarations keep that order so VC5's
 * x87 operand selection sees the same section offsets. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* the output matrix */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* the sprite matrix */
#define OUTM(k)  ((*(float (*)[16])&g_BrDrawCombined)[k])
#define SPRM(k)  ((*(float (*)[16])&g_BrDrawScale)[k])

/* The eight-byte display-list append through the GLOBAL cursor. */
#define EMIT(W0, W1) \
    { uint32_t *p_ = g_BrGfxPtr; g_BrGfxPtr += 2; \
      p_[0] = (uint32_t)(W0); p_[1] = (uint32_t)(W1); }

/* An advanced slot handed to the combiner builder. */
#define EMIT_SLOT(S) \
    { (S) = g_BrGfxPtr; g_BrGfxPtr += 2; }

/* Copy the command being walked through, both words, unchanged. */
#define COPY2() \
    { *pDL++ = *pCmd++; *pDL++ = *pCmd++; }

/* Hand one triangle word's three corners to the software clipper. */
#define CLIPTRI(W) \
    BrPolyClipTri(pRec, pObj + 0xb0, pObj + 0xab, \
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
/* @t4-pass 0x1000CBA0 1 2026-09-07 probes 150 bytes 4164 insns 1144 regions 23 rows 184 census yes  (tools/crank.py) */
/* T3 RESOLVED (2026-09-19): the code is a faithful, complete transcription;
 * the byte residue the 2026-09-15 verdict called "semantic" (orig `shr R,8`
 * progressive vs our `shr R,0x10` third-index, indexed byte-RMW clip tests,
 * frame 0x68 vs 0x60) is COMPILER SCHEDULING, proven behaviourally
 * irrelevant by the A5 oracle.  The dominant byte gap is VC5 tail-SHARING our
 * five CLIPTRI call sites down to two (the orig keeps five); MSVC5 /O2 does
 * not cross-jump identical blocks in the orig, so the share is driven by the
 * upstream walk-loop register allocation -- an immovable colouring wall, not
 * missing/wrong code.  A5 verdict = EQUIVALENT with real teeth: the oracle
 * profile (tools/oracle_profiles.py) was seeding ZERO vertices (a masking bug
 * served only byte 0 of each coord float), so every transform coefficient was
 * multiplied by zero and the geometry/clip half had no teeth -- a false
 * EQUIVALENT.  Fixed to seed full coordinate words; negative-controlled: a
 * matrix-coefficient swap (m00<->m20), a corner-index shift bug, a clip-flag
 * bug, a counter bug, and a switch-dispatch mislabel are ALL now caught as
 * DIFF, and the correct code stays EQUIVALENT.  So T3 is a real cert here. */
/* @t4-pass 0x1000CBA0 2 2026-09-07 probes 150 bytes 4164 insns 1144 regions 24 rows 182 census yes  (tools/crank.py) */
/* @t4-pass 0x1000CBA0 3 2026-09-19 probes 12 bytes 3980 insns 1081 regions 21 rows 256 census no  (register-alloc grind on the frame-correct O2 variant: hoisting pObj+0xb0/+0xab into locals flips the frame 0x60->0x68 to match the orig and pushes FIRSTDIV +0x2->+0x1e, but VC5 still tail-shares the five CLIPTRI sites to two -- the singleton sites jmp a common push+call tail -- driven by the walk-loop entry register allocation, which no source spelling reproduces; the progressive-shift and byte-RMW residue is the same scheduling. Numbers held.) */
/* @t4-pass 0x1000CBA0 4 2026-09-19 probes 11 bytes 3980 insns 1081 regions 21 rows 256 census yes  (write-slot census (tools/slotcensus.py) + O2 variant sweep confirm every slot's writes/reads are consistent -- the residue is register allocation/scheduling, not missing/wrong code; the teeth-fixed A5 oracle proves same-in/same-out across the cheap and expanded paths incl. the vertex transform and the clip drop/keep decision; numbers unmoved.) */
/* @t3 0x1000CBA0 2026-09-19 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 3980/4180 insns 1081/1031 rows 103+153 regions 21 oracle EQUIVALENT
 * @t3-effort passes 4 zero-movement 3 4
 * Residue by wall: (1) VC5 tail-shares the five CLIPTRI call sites to two,
 * driven by the walk-loop entry register allocation (MSVC5 /O2 does not
 * cross-jump identical blocks -- the orig keeps five); (2) progressive
 * `shr8;shr8` third-index vs our `shr0x10`, byte-RMW clip tests, frame 0x68
 * vs 0x60 -- all x87/integer scheduling.  A5 EQUIVALENT with teeth (see the
 * T3-RESOLVED note above and the @t4-pass ledger).  Colouring wall; do not
 * reopen before the end-grind. */
/* @implements 0x1000CBA0 glide BrObjDlBuild */
void BrObjDlBuild(struct BrViewRect * pRects, int idx, uint32_t cls, int bLit, unsigned char * pScene)
{
    uint32_t *pCmd;
    uint32_t *pCmdStart;  /* each object walks the list from its start */
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
    pDL  = (*(uint32_t * *)&DAT_1035f7d8);
    pRec = (char *)(BR_PTR32(void *, g_brTrkHdr.aInstances) + idx * 0x54);
    pCmd = *(uint32_t **)(pRec + 0x44);

    if (cls == 0 || g_BrCamera == g_pBr63Race + 0x2890 ||
        (pRec[0x4d] & 2) != 0 || DAT_10b71538 == 0) {
        EMIT(0xbb001001, 0xffffffff);
        EMIT(0xe8000000, 0);
        EMIT(0x06000000, pCmd);
        DAT_106e772c += *(uint16_t *)(pRec + 0x50);
        DAT_106e7734 += *(uint16_t *)(pRec + 0x4e);
        DAT_106e86a0 += *(uint16_t *)(pRec + 0x52);
    } else {
        BrPodNop();
        EMIT(0xbb001001, 0xffffffff);
        EMIT(0xe8000000, 0);
        EMIT(0xfa001700, 0xff0000ff);
        EMIT(0x06000000, pCmd);
        EMIT(((*(uint32_t *)&DAT_1184c470) & 0xffffff) | 0xdc000000, 1);
        EMIT(0xba001001, 0);
        EMIT(0xfa001700, 0xff0000ff);
        if (bLit != 0 && (*(int *)((char *)&g_aBrEntRecs + 0x7C)) == 0 && (*(int *)((char *)&g_aBrEntRecs + 0x84)) == 0) {
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
        if ((*(int *)((char *)&g_aBrEntRecs + 0x6C)) == 0) {
            pRect = (int *)((char *)pRects + g_BrEnvSection * 0x58);   /* 0x58-byte view rects, no pointers */
            BrSub_1003289F(pRect[0], pRect[1], pRect[2], pRect[3]);
        }
        EMIT(0xba000602, BrG_6C0688);
        EMIT(0xf9000000, 0);
        if ((*(int *)((char *)&g_aBrEntRecs + 0x78)) != 0) {
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
        pCmdStart = pCmd;
        do {
            /* The original reloads the command pointer from its saved start
             * at the top of every object iteration (1000D06C); without it
             * the second object began at the first one's end marker and
             * drew nothing (live oracle, benchmark flythrough). */
            pCmd = pCmdStart;
            if ((cls & 1) != 0 &&
                ((g_BrCamera != g_pBr63Race + 0x273c &&
                  g_BrCamera != g_pBr63Race + 0x27c4) ||
                 *(int *)(g_pBr63Race + 0x140) != i)) {
                if (DAT_10396eb0 != 0) {
                    BrNodeChainReset_1000F460();
                }
                pDL[0] = 0xe7000000;
                pDL[1] = 0;
                pDL += 2;
                pDL[0] = 0xfb000000;
                pDL[1] = (BrFtolArg(pObj[0] * DAT_100771f4) & 0xff) |
                         ((((BrG_6C0260 << 8) | BrG_6C1614) << 8 |
                           BrG_6C0200) << 8);
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
                sx = BrVec3LenXY(&DAT_1035fb78);
                /* The original's clamp stores its 0.5 with the length call's
                 * argument still pushed, so it lands four bytes off -- in
                 * k's slot, which is recomputed below -- and the length stays
                 * unclamped.  That is the behaviour; the benchmark flythrough
                 * reaches it (live oracle, short sprite direction vectors). */
                if (sx < DAT_100771fc) {
                    k = 0.5f;
                }
                len2 = BrVec3LenXY(&pObj[-0x9c8]);
                if (len2 < DAT_100771fc) {
                    len2 = DAT_100771fc;
                }
                pTex = *(int *)((char *)pObj + 0x294);
                sx = (DAT_10077200 / *(float *)(pTex + 0x80e0)) / sx;
                sy = (DAT_10077204 / *(float *)(pTex + 0x80e4)) / len2;
                br_dl_normalise(&DAT_1035fb78);

                memcpy((*(float (*)[16])&g_BrDrawCombined), pRec, 0x40);

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
                BrMtxMul((*(float (*)[16])&g_BrDrawCombined), (*(float (*)[16])&g_BrDrawCombined), (*(float (*)[16])&g_BrDrawScale));

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

                if ((*(int *)((char *)&g_aBrEntRecs + 0x6C)) == 0) {
                    pSave = g_BrGfxPtr;
                    g_BrGfxPtr = pDL;
                    BrSub_1003289F(*(short *)((char *)pObj + 0x26c),
                                   *(short *)((char *)pObj + 0x272),
                                   *(short *)((char *)pObj + 0x270) -
                                       *(short *)((char *)pObj + 0x26c),
                                   *(short *)((char *)pObj + 0x26e) -
                                       *(short *)((char *)pObj + 0x272));
                    pDL = g_BrGfxPtr;
                    g_BrGfxPtr = pSave;
                }

                while ((((char *)pDL - (char *)DAT_102e16b0) & ~3) <= 0x13800) {
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
                        if ((int)(((char *)pVtx + n * 0x20 - (char *)DAT_1035fba4) & ~0x1f) >
                            32000) {
                            pDL--;
                            goto nextObj;
                        }
                        if (n > 32) {
                            BrLogSet(DAT_100a5da8);
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
                        if ((*(int *)((char *)&g_aBrEntRecs + 0x6C)) != 0) {
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
                        if ((*(int *)((char *)&g_aBrEntRecs + 0x6C)) != 0) {
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
        BrPodNop();
    }
    (*(uint32_t * *)&DAT_1035f7d8) = pDL;
    DAT_1035faec = pVtx;
}

