/* br_collray.c -- vertical raycast into the collision grid.
 *
 * TU 56, slot 5 (0x1006EC30).  Casts a ray straight down from the camera
 * through the collision grid, collecting near and far triangle hits.
 * Called once per frame from BrFrameDrive via FUN_1006ec30(0, 0, eye, ...). */

#ifdef BR_MATCHING_BUILD

#include <stdint.h>
#include "br_vec.h"
#include "slice1_06.h"  /* BrTriContainsPoint */
#include "slice1_08.h"  /* BrCollPlane */
#include "slice2_11.h"  /* BR_COLL_CELL_PLANES, DAT_11773698, DAT_11778800 */

extern float    DAT_106eed14;                               /* time now   */
extern float    DAT_106eed10;                               /* time prev  */
extern BrCollPlane DAT_11773698[4][BR_COLL_CELL_PLANES];
extern uint16_t    DAT_11778800[4];
extern const uint16_t *g_pBrCollTriIdx;                     /* 0x106EECE4 */
extern const uint16_t *DAT_106eed64;                        /* trk +0x8C  */
extern const uint16_t *DAT_106eed68;                        /* trk +0x90  */

extern short BrCollGridCellAcquire(float x, float y);

/* @t4-pass 0x1006EC30 1 2026-09-19 probes 10 bytes 1339 insns 355 regions 13 rows 24 census yes  (decl order, int/short types, inline dt, scope of t, split decls; residue is frameless vs EBP-frame + FP interleaving) */
/* @t4-pass 0x1006EC30 2 2026-09-19 probes 11 bytes 1339 insns 355 regions 13 rows 24 census yes  (moved origin assignments WORSE 1136/965, removed cell local, swap norm/hitPt/origin-dir order; residue stable) */
/* @t3 0x1006EC30 2026-09-19 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1339/1331 insns 355/355 rows 12+12 regions 13 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue: frameless (orig sub esp,0x68) vs EBP-frame (recomp sub esp,0x6c),
 * FP interleaving (fld+fxch vs mov pairs), uint16 masking (extra and R,0xffff).
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* WHAT IT DOES: vertical raycast from the eye through the collision grid.
 * Finds the nearest face directly below the camera and builds near/far
 * hit-index lists for the caller. */
/* @implements 0x1006EC30 glide FUN_1006ec30 */
int FUN_1006ec30(BrVec3 *pPosOut, BrVec3 *pNormOut, const BrVec3 *pEye, uint16_t *pNearIds, int *pGotHit, uint16_t *pFarIds, int *pFarCount, float *pDistOut, int *pFaceOut)
{
    float dt;
    int nearCount, hitCount, farCount;
    float bestNearDist, bestFarDist;
    float bestNearHitZ, bestFarHitZ;
    short farFaceIdx;
    int farFaceVal;
    BrVec3 bestNearNorm, bestFarNorm;
    BrVec3 origin, dir;
    BrVec3 hitPt, tmpV;
    float dist;
    BrCollPlane *pP, *pEnd;
    int fIdx;
    uint16_t fVal;
    short cell;

    dt = DAT_106eed14 - DAT_106eed10;
    bestNearDist = dt * dt - (-1.0f);

    bestFarNorm.x = 0.0f;
    bestFarNorm.y = 0.0f;
    bestFarNorm.z = 1.0f;
    bestNearNorm.x = 0.0f;
    bestNearNorm.y = 0.0f;
    origin.y = pEye->y;
    origin.x = pEye->x;
    bestFarDist = bestNearDist;
    bestNearHitZ = pEye->z;
    bestFarHitZ = pEye->z;
    bestNearNorm.z = 1.0f;
    dir.x = 0.0f;
    dir.y = 0.0f;
    dir.z = 1.0f;
    origin.z = 1.0f;
    nearCount = 0;
    farCount = 0;
    farFaceIdx = 0;
    farFaceVal = 0;
    hitCount = 0;
    *pFaceOut = 0;

    cell = BrCollGridCellAcquire(pEye->x, pEye->y);
    pP = DAT_11773698[cell];
    pEnd = pP + DAT_11778800[cell];

    for (; pP != pEnd; pP++) {
        float t;

        if (pP->nz < 0.0f)
            continue;

        dist = BrVec3Dot(&dir, (const BrVec3 *)pP);
        if (dist == 0.0f)
            continue;

        BrVec3Sub(&tmpV, pP->pV0, &origin);
        t = BrVec3Dot(&tmpV, (const BrVec3 *)pP) / dist;
        BrVec3MulAdd(&hitPt, &origin, &dir, t);

        if (!BrTriContainsPoint(&hitPt, pP->pV0, pP->pV1, pP->pV2,
                                (const BrVec3 *)pP))
            continue;

        dist = pEye->z - (-1.5f) - hitPt.z;

        if (dist >= 0.0f) {
            hitCount++;

            if (dist < bestNearDist) {
                bestNearDist = dist;
                bestNearHitZ = hitPt.z;
                *pFaceOut = (int)(short)pP->tri;

                if (pP->nz < 0.0f) {
                    BrVec3Negate(&bestNearNorm, (const BrVec3 *)pP);
                } else {
                    bestNearNorm.x = pP->nx;
                    bestNearNorm.y = pP->ny;
                    bestNearNorm.z = pP->nz;
                }

                if (nearCount < 32) {
                    pNearIds[nearCount] = pNearIds[0];
                    nearCount++;
                }

                fIdx = (int)(short)pP->tri;
                fVal = g_pBrCollTriIdx[fIdx * 4 + 3] + 1;
                pNearIds[0] = fVal;

                if (dist < 5.0f) {
                    unsigned int ci = (unsigned int)DAT_106eed68[(int)(short)pP->tri];
                    if (DAT_106eed64[ci] != 0) {
                        do {
                            if (farCount < 32) {
                                pFarIds[farCount] = pFarIds[0];
                                farCount++;
                            }
                            pFarIds[0] = DAT_106eed64[ci];
                            ci++;
                        } while (DAT_106eed64[ci] != 0);
                    }
                }
            } else {
                if (nearCount < 32) {
                    fIdx = (int)(short)pP->tri;
                    fVal = g_pBrCollTriIdx[fIdx * 4 + 3] + 1;
                    pNearIds[nearCount] = fVal;
                    nearCount++;

                    if (dist < 5.0f) {
                        unsigned int ci = (unsigned int)DAT_106eed68[(int)(short)pP->tri];
                        if (DAT_106eed64[ci] != 0) {
                            uint16_t *pDst = &pFarIds[farCount];
                            do {
                                if (farCount < 32) {
                                    *pDst = DAT_106eed64[ci];
                                    farCount++;
                                    pDst++;
                                }
                                ci++;
                            } while (DAT_106eed64[ci] != 0);
                        }
                    }
                }
            }
        }

        dist -= 1.5f;
        if (dist <= 0.0f) {
            if (dist < bestFarDist) {
                short tri = pP->tri;
                fIdx = (int)tri;
                fVal = g_pBrCollTriIdx[fIdx * 4 + 3] + 1;
                farFaceVal = (int)fVal;

                if (dist > -1.0f) {
                    bestFarDist = dist;
                    bestFarHitZ = hitPt.z;
                    farFaceIdx = tri;

                    if (pP->nz < 0.0f) {
                        BrVec3Negate(&bestFarNorm, (const BrVec3 *)pP);
                    } else {
                        bestFarNorm.x = pP->nx;
                        bestFarNorm.y = pP->ny;
                        bestFarNorm.z = pP->nz;
                    }
                }
            }
        }
    }

    if (hitCount != 0) {
        if (pPosOut != NULL) {
            pPosOut->z = bestNearHitZ;
            *pDistOut = bestNearDist - 1.5f;
        }
        if (pNormOut != NULL) {
            pNormOut->x = bestNearNorm.x;
            pNormOut->y = bestNearNorm.y;
            pNormOut->z = bestNearNorm.z;
        }
    } else {
        unsigned int ci = (unsigned short)farFaceIdx;
        ci = (unsigned int)DAT_106eed68[ci];
        if (DAT_106eed64[ci] != 0) {
            uint16_t *pDst = &pFarIds[farCount];
            while (1) {
                if (farCount >= 32) {
                    pFarIds[31] = DAT_106eed64[ci];
                    break;
                }
                *pDst = DAT_106eed64[ci];
                farCount++;
                pDst++;
                ci++;
                if (DAT_106eed64[ci] == 0)
                    break;
            }
        }

        if (nearCount < 32) {
            pNearIds[nearCount] = (uint16_t)farFaceVal;
        } else {
            pNearIds[31] = (uint16_t)farFaceVal;
        }

        if (pPosOut != NULL) {
            pPosOut->z = bestFarHitZ;
        }
        if (pNormOut != NULL) {
            pNormOut->x = bestFarNorm.x;
            pNormOut->y = bestFarNorm.y;
            pNormOut->z = bestFarNorm.z;
        }
    }

    *pGotHit = 1;
    *pFarCount = farCount;
    return hitCount;
}

#else
/* Port stub -- the port uses BrPhysProbeCell via br_collgrid.h instead. */
int FUN_1006ec30(void *a, void *b, const float *pEye,
                 void *p4, int *p5, void *p6, int *p7, float *p8, int *p9)
{
    *p9 = 0;
    *p5 = 0;
    *p7 = 0;
    return 0;
}
#endif
