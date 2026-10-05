/* framepools.c -- per-frame pools of matrices, lights and viewports for the display list
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8028DFB0;
extern int D_8028DFB4;
extern int D_8028DFB8;
extern int D_8028A85C;
extern int *D_8028C75C;                 /* matrix pool: next and start */
extern int *D_8028C760;
extern short *D_8028C764;               /* vertex pool: next and start */
extern short *D_8028C768;
typedef struct BrMtx { be32_t m[16]; } BrMtx;           /* an Mtx */
typedef struct BrLight { int w[4]; } BrLight;         /* a Light */
typedef struct BrVp { be16_t v[16]; } BrVp;            /* 32 bytes */
extern BrMtx D_8036AE50[2][257];
extern BrLight D_80372ED0[2][21];
extern BrVp D_80373170[2][21];
extern char D_80353580[2][4000];
extern char D_803554C0[2][16000];
/* -- end declarations -- */

/* WHAT IT DOES: Empty the three per-frame pools at the start of a frame:
 * matrices (256 per frame), lights (20) and viewports (20), which the
 * display-list builders hand out one at a time. */
/* @implements 0x80255E64 tgr BrFramePoolsReset */
void BrFramePoolsReset(void)
{
  D_8028DFB0 = 0;
  D_8028DFB4 = 0;
  D_8028DFB8 = 0;
}

/* WHAT IT DOES: Point the vertex pools for this frame buffer back at their
 * start. */
/* @implements 0x80233FDC tgr BrVtxPoolsReset */
void BrVtxPoolsReset(void)
{
  D_8028C75C = (int *)D_80353580[D_8028A85C];
  D_8028C760 = D_8028C75C;
  D_8028C764 = (short *)D_803554C0[D_8028A85C];
  D_8028C768 = D_8028C764;
}


/* WHAT IT DOES: Hand out the next matrix slot of this frame's pool (256
 * slots); past the end it keeps returning the last one. */
/* @implements 0x80255CD0 tgr BrMtxAlloc */
BrMtx *BrMtxAlloc(void)
{
  if (D_8028DFB0 < 256) {
    return &D_8036AE50[D_8028A85C][D_8028DFB0++];
  }
  D_8028DFB0++;
  return &D_8036AE50[D_8028A85C][256];
}


/* WHAT IT DOES: Hand out the next light slot of this frame's pool (20
 * slots); past the end it keeps returning the last one. */
/* @implements 0x80255D4C tgr BrLightAlloc */
BrLight *BrLightAlloc(void)
{
  if (D_8028DFB4 < 20) {
    return &D_80372ED0[D_8028A85C][D_8028DFB4++];
  }
  D_8028DFB4++;
  return &D_80372ED0[D_8028A85C][20];
}


/* WHAT IT DOES: Hand out the next viewport slot of this frame's pool (20
 * slots); past the end it keeps returning the last one. */
/* @implements 0x80255DD8 tgr BrVpAlloc */
BrVp *BrVpAlloc(void)
{
  if (D_8028DFB8 < 20) {
    return &D_80373170[D_8028A85C][D_8028DFB8++];
  }
  D_8028DFB8++;
  return &D_80373170[D_8028A85C][20];
}

