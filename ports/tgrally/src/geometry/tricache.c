/* tricache.c -- the collision triangle cache: four slots, each holding the
 * triangle planes of one 32x32 grid cell, recycled least-recently used.
 */
#include "tgr/common.h"

/* -- declarations -- */
extern unsigned short D_8037EA80[4];   /* the cell each slot holds (0 = none) */
extern int D_8037EA90[4];              /* each slot's last-use stamp */
extern short D_8037EA88[4];            /* each slot's triangle count */
extern int D_8037EAA0;                 /* the use clock */
/* A cached triangle (0x20 bytes): its plane and its three vertices. */
typedef struct BrTri {
    float plane[4];             /* 0x00  normal and distance */
    TgrAddr v[3];                /* float * -- 0x10 */
    short id;                   /* 0x1C */
    unsigned char kind;         /* 0x1E */
    char pad1f;
} BrTri;
extern BrTri D_80379F80[4][150];       /* each slot's triangles */
extern int D_80379940[50][4][2];      /* 50 records of four pairs */
extern void *D_802A4A20;
extern void *D_802A4A24;
/* -- end declarations -- */

/* WHAT IT DOES: Drop the cache slot holding the grid cell under (x, y), if
 * any: its cell and use stamp go back to zero.  Returns 1 when a slot was
 * dropped, else 0. */
/* @implements 0x8025F0A4 tgr BrTriCacheDrop */
int BrTriCacheDrop(float x, float y)
{
  short cell;
  int i;

  cell = ((int)y / 32) * 64 + (int)x / 32;
  for (i = 0; i < 4; i++) {
    if (cell == D_8037EA80[i]) {
      D_8037EA90[i] = 0;
      D_8037EA80[i] = 0;
      return 1;
    }
  }
  return 0;
}

/* WHAT IT DOES: Empty the collision triangle cache: every slot's cell, use
 * stamp and count, every cached triangle's plane and vertices, the 50
 * records at 0x80379940, two counters and the use clock. */
/* @implements 0x80259350 tgr BrTriCacheReset */
void BrTriCacheReset(void)
{
  int i;
  int j;

  for (i = 0; i < 4; i++) {
    D_8037EA80[i] = 0;
    D_8037EA90[i] = 0;
    D_8037EA88[i] = 0;
    for (j = 0; j < 150; j++) {
      D_80379F80[i][j].plane[0] = 0.0f;
      D_80379F80[i][j].plane[1] = 0.0f;
      D_80379F80[i][j].plane[2] = 0.0f;
      D_80379F80[i][j].plane[3] = 0.0f;
      D_80379F80[i][j].v[0] = 0;
      D_80379F80[i][j].v[1] = 0;
      D_80379F80[i][j].v[2] = 0;
    }
  }
  for (i = 0; i < 50; i++) {
    for (j = 0; j < 4; j++) {
      D_80379940[i][j][0] = 0;
      D_80379940[i][j][1] = 0;
    }
  }
  D_802A4A20 = 0;
  D_802A4A24 = 0;
  D_8037EAA0 = 0;
}
