/* br_track.c -- startup.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>
#include <string.h>

#include "slice2_20.h"   /* g_BrLoad, BrTrackFixupRec54 */

/* XSLICE 0x10035BD1 */
extern void BrSub10035BD1(void);
/* XSLICE 0x10220B20 */ extern uint32_t g_a220B20[0x46];

/* Private copies of three stateless readers from slice2_20.c, which keeps its
 * own -- they are `static` there, so the definitions could not travel, and
 * without them the port arms below call them implicitly (C4013) and leave
 * undefined externals: a link failure match_sweep.py cannot see, because it
 * only compiles the matching configuration. Found by tools/portcheck.py.
 * Duplicating these is safe for the reason BrFtol is duplicated in
 * slice1_02.c and slice2_12.c: they hold no state, so two copies cannot
 * drift. */
static uint32_t BrRd32(const void *pv)
{
    uint32_t v;
    memcpy(&v, pv, sizeof v);
    return v;
}

static uint16_t BrRd16(const void *pv)
{
    uint16_t v;
    memcpy(&v, pv, sizeof v);
    return v;
}

/* The dword at pv, already rebased, as a host pointer. */
static void *BrPtrAt(const void *pv)
{
    return BrLoadResolve(BrRd32(pv));
}

#ifdef BR_MATCHING_BUILD

extern void BrSegPtrFixup(uint32_t *p);
int BrTrackFixupSegRec();

/* WHAT IT DOES: walk the list of dword segment pointers at +0x78, byte-swap
 * and rebase each, then fix up the record it points at. */
/* @implements 0x10031910 glide BrTrackFixupSegList */

int BrTrackFixupSegList(int param_1)

{
  char uVar1;
  int *puVar3;
  int iVar4;

  puVar3 = *(int **)(param_1 + 0x78);
  iVar4 = 0;
  if (0 < *(int *)(param_1 + 0x7c)) {
    do {
      uVar1 = *(char *)((int)puVar3 + 3);
      *(char *)((int)puVar3 + 3) = *(char *)puVar3;
      *(char *)puVar3 = uVar1;
      uVar1 = *(char *)((int)puVar3 + 2);
      *(char *)((int)puVar3 + 2) = *(char *)((int)puVar3 + 1);
      *(char *)((int)puVar3 + 1) = uVar1;
      BrSegPtrFixup(puVar3);
      BrTrackFixupSegRec(*puVar3);
      puVar3 = puVar3 + 1;
      iVar4 = iVar4 + 1;
    } while (iVar4 < *(int *)(param_1 + 0x7c));
  }
  return;
}



/* WHAT IT DOES: byte-swap a 0x28-byte record: three Vec3s then one dword. */
/* @implements 0x10031A40 glide BrTrackSwapRec28 */

int BrTrackSwapRec28(int param_1)

{
  char uVar1;

  BrSwapVec3(param_1);
  BrSwapVec3(param_1 + 0xc);
  BrSwapVec3(param_1 + 0x18);
  uVar1 = *(char *)(param_1 + 0x27);
  *(char *)(param_1 + 0x27) = *(char *)(param_1 + 0x24);
  *(char *)(param_1 + 0x24) = uVar1;
  uVar1 = *(char *)(param_1 + 0x26);
  *(char *)(param_1 + 0x26) = *(char *)(param_1 + 0x25);
  *(char *)(param_1 + 0x25) = uVar1;
  return;
}

/* WHAT IT DOES: byte-swaps and rebases one segment record in place: its
 * four leading pointers (swap, then rebase through BrSegPtrFixup), its two
 * big-endian 16-bit counts, the 0x28-byte record at +0x18, and then the
 * array of those records from +0x40 -- one more of them than the first
 * count says. */
/* @implements 0x10031960 glide BrTrackFixupSegRec */

int BrTrackFixupSegRec(int param_1)

{
  char uVar1;
  int iVar4;

  /* The first swap is spelled exactly as BrTrackFixupSegList's (save p[3],
   * store p[0] over it, restore): VC5 schedules the p[0] store FIRST either
   * way, but the decompiler's reading (save p[0], store p[3] over it) puts
   * the two byte temps in the other registers -- 6 diff bytes, every
   * declaration-order / naming permutation inert. */
  uVar1 = *(char *)(param_1 + 3);
  *(char *)(param_1 + 3) = *(char *)param_1;
  *(char *)param_1 = uVar1;
  uVar1 = *(char *)(param_1 + 2);
  *(char *)(param_1 + 2) = *(char *)(param_1 + 1);
  *(char *)(param_1 + 1) = uVar1;
  BrSegPtrFixup((uint32_t *)param_1);
  uVar1 = *(char *)(param_1 + 7);
  *(char *)(param_1 + 7) = *(char *)(param_1 + 4);
  *(char *)(param_1 + 4) = uVar1;
  uVar1 = *(char *)(param_1 + 6);
  *(char *)(param_1 + 6) = *(char *)(param_1 + 5);
  *(char *)(param_1 + 5) = uVar1;
  BrSegPtrFixup((uint32_t *)(param_1 + 4));
  uVar1 = *(char *)(param_1 + 0xb);
  *(char *)(param_1 + 0xb) = *(char *)(param_1 + 8);
  *(char *)(param_1 + 8) = uVar1;
  uVar1 = *(char *)(param_1 + 10);
  *(char *)(param_1 + 10) = *(char *)(param_1 + 9);
  *(char *)(param_1 + 9) = uVar1;
  BrSegPtrFixup((uint32_t *)(param_1 + 8));
  uVar1 = *(char *)(param_1 + 0xf);
  *(char *)(param_1 + 0xf) = *(char *)(param_1 + 0xc);
  *(char *)(param_1 + 0xc) = uVar1;
  uVar1 = *(char *)(param_1 + 0xe);
  *(char *)(param_1 + 0xe) = *(char *)(param_1 + 0xd);
  *(char *)(param_1 + 0xd) = uVar1;
  BrSegPtrFixup((uint32_t *)(param_1 + 0xc));
  *(unsigned short *)(param_1 + 0x14) =
      (unsigned short)(((unsigned int)*(unsigned char *)(param_1 + 0x14) << 8)
                       | *(unsigned char *)(param_1 + 0x15));
  *(unsigned short *)(param_1 + 0x16) =
      (unsigned short)(((unsigned int)*(unsigned char *)(param_1 + 0x16) << 8)
                       | *(unsigned char *)(param_1 + 0x17));
  BrTrackSwapRec28(param_1 + 0x18);
  /* The record address is an expression of the counter, not a pointer
   * local: VC5 strength-reduces it into an induction temp that is set up
   * INSIDE the loop guard (`push ebx; lea ebx,[esi+0x40]` after the `jb`)
   * and pushed lazily; a named pointer is initialised before the test and
   * takes the counter's register (4 shape diffs). */
  for (iVar4 = 0; iVar4 <= *(unsigned short *)(param_1 + 0x14); iVar4++) {
    BrTrackSwapRec28(param_1 + 0x40 + iVar4 * 0x28);
  }
  return;
}


/* WHAT IT DOES: walk an array of Vec3s in a track struct and byte-swap each one. */
/* @implements 0x10031A80 glide BrTrackSwapAllVec3 */

int BrTrackSwapAllVec3(int param_1)

{
  int iVar1;
  int iVar2;
  
  iVar1 = *(int *)(param_1 + 0x84);
  iVar2 = 0;
  if (0 < *(int *)(param_1 + 0x88)) {
    do {
      BrSwapVec3(iVar1);
      iVar1 = iVar1 + 0xc;
      iVar2 = iVar2 + 1;
    } while (iVar2 < *(int *)(param_1 + 0x88));
  }
  return;
}

extern int DAT_11778808;
extern int DAT_11778820;
extern int DAT_11773690;
extern int DAT_100b5170;
extern int DAT_100b4c30;
extern int DAT_100b4e70;
extern int DAT_100b4f30;
extern int DAT_100b4d50;
extern int DAT_100b4ed0;
extern int DAT_100b5050;

/* WHAT IT DOES: configure track surface grip tables by surface type index. */
/* @implements 0x10069530 glide BrTrackSurfaceSet */

void BrTrackSurfaceSet(int param_1)

{
  switch(param_1) {
  case 0:
  case 1:
  case 2:
  case 3:
  case 4:
  case 6:
  case 7:
  case 8:
  case 9:
  case 10:
  case 0xc:
    DAT_11778808 = (int)&DAT_100b4c30;
    DAT_11778820 = (int)&DAT_100b4e70;
    DAT_11773690 = (int)&DAT_100b4f30;
    DAT_100b5170 = 0x3f800000;
    return;
  case 5:
  case 0xb:
  case 0xd:
  case 0xe:
  default:
    DAT_11778808 = (int)&DAT_100b4d50;
    DAT_11778820 = (int)&DAT_100b4ed0;
    DAT_11773690 = (int)&DAT_100b5050;
    DAT_100b5170 = 0x3f666666;
    return;
  }
}

#endif /* BR_MATCHING_BUILD */

/* ==========================================================================
 * Filed out of slice2_20.c.  These four sit either side of the block above
 * in the original (0x10031660..0x10032680), so the compiler's view of them
 * is the batch's: <string.h> and slice2_20.h, added at the top.
 * ========================================================================== */

/* WHAT IT DOES: copies the actual texture pixels and colour palettes out of
 * the loaded track image and into the places the texture records point at,
 * for those records that ask for it. Records that fail any of half a dozen
 * checks are quietly skipped. */
/* @implements 0x10038450 d3d BrTexCopyRecords */
void BrTexCopyRecords(void *pvTable, int cRecords)
{
#ifdef BR_MATCHING_BUILD
    /* orig: ebx=-8; lea eax,[table+8]; sub ebx,table; ebp=n; then
     * [eax+0x18] flags and [pTexFlags+ebx+eax+0x20] for the parallel
     * array. ebx stays loop-invariant. */
    uint8_t *pTable;
    uint8_t *pWalk;
    int32_t  adj;
    int      n;

    if (cRecords <= 0)
        return;

    pTable = (uint8_t *)pvTable;
    adj    = -8;
    pWalk  = pTable + 8;
    adj   -= (int32_t)(uint32_t)pTable;
    n      = cRecords;
    do {
        uint8_t  *pDst = *(uint8_t **)(void *)(pWalk - 8);
        uint32_t  uFlags;
        uint8_t  *pDesc;
        uint32_t  cb;

        if (pDst == 0)
            goto next;

        uFlags = *(uint32_t *)(void *)(pWalk + 0x18);
        if ((uFlags & 0x00100000u) == 0)
            goto next;

        pDesc = *(uint8_t **)(void *)pWalk;
        if (*(uint16_t *)(void *)(pDesc + 2) != 2)
            goto next;
        if (*(int32_t *)(void *)(pDesc + 8) != -1)
            goto next;

        cb = uFlags & 0x0003FFFFu;
        if (cb == 0)
            goto next;

        memcpy(pDst, g_BrLoad.pTexBase + *(uint32_t *)(void *)(pDesc + 0x0C),
               cb);

        pDst = *(uint8_t **)(void *)(pWalk - 4);
        if (pDst == 0)
            goto next;

        {
            char    *pFlags;
            uint32_t uSel;
            uint32_t cbPal;

            pDesc  = *(uint8_t **)(void *)pWalk;
            pFlags = (char *)g_BrLoad.pTexFlags;
            pFlags += adj;
            uSel    = *(uint32_t *)(void *)(pFlags + (int32_t)(uint32_t)pWalk
                                            + 0x20);
            uSel   &= 0x0F000000u;
            cbPal   = (uSel == 0x01000000u) ? 0x20u : 0x200u;
            memcpy(pDst,
                   g_BrLoad.pTexBase + *(uint32_t *)(void *)(pDesc + 0x10),
                   cbPal);
        }
    next:
        pWalk += 0x24;
    } while (--n);
#else
    uint8_t *pTable = (uint8_t *)pvTable;
    int i;

    if (cRecords <= 0)
        return;

    for (i = 0; i < cRecords; ++i) {
        uint8_t *pRec = pTable + (size_t)i * 0x24;
        uint8_t *pDst;
        uint8_t *pDesc;
        uint8_t *pSrc;
        uint32_t uFlags;
        uint32_t cb;

        pDst = (uint8_t *)BrPtrAt(pRec + 0x00);
        if (pDst == NULL)
            continue;

        uFlags = BrRd32(pRec + 0x20);
        if ((uFlags & 0x00100000u) == 0)
            continue;

        pDesc = (uint8_t *)BrPtrAt(pRec + 0x08);
        if (pDesc == NULL)
            continue;
        if (BrRd16(pDesc + 0x02) != 2)
            continue;
        if ((int32_t)BrRd32(pDesc + 0x08) != -1)
            continue;

        cb = uFlags & 0x0003FFFFu;
        if (cb == 0)
            continue;

        /* DEVIATION: the sources are byte offsets into the texture image and
         * the original adds them to a raw global base with no check.  The
         * port refuses a copy that would run past the declared image. */
        pSrc = g_BrLoad.pTexBase;
        if (pSrc == NULL)
            continue;
        {
            uint32_t off = BrRd32(pDesc + 0x0C);
            if ((size_t)off > g_BrLoad.cbTexBase
             || (size_t)cb  > g_BrLoad.cbTexBase - off)
                continue;
            memcpy(pDst, pSrc + off, cb);
        }

        pDst = (uint8_t *)BrPtrAt(pRec + 0x04);
        if (pDst == NULL)
            continue;

        {
            uint32_t uSel;
            uint32_t cbPal;
            uint32_t off = BrRd32(pDesc + 0x10);

            if (g_BrLoad.pTexFlags == NULL)
                continue;
            /* Parallel array, same index and stride, field +0x20. */
            uSel = BrRd32(g_BrLoad.pTexFlags + (size_t)i * 0x24 + 0x20);
            uSel &= 0x0F000000u;
            cbPal = (uSel == 0x01000000u) ? 0x20u : 0x200u;

            if ((size_t)off > g_BrLoad.cbTexBase
             || (size_t)cbPal > g_BrLoad.cbTexBase - off)
                continue;
            memcpy(pDst, g_BrLoad.pTexBase + off, cbPal);
        }
    }
#endif
}

/* WHAT IT DOES: clears a block of state, plants an 8 in its first slot, and
 * calls on to further setup. What the block holds is not established here. */
/* @implements 0x10039000 d3d BrInit220B20 */
void BrInit220B20(void)
{
    memset(g_a220B20, 0, sizeof(uint32_t) * 0x46);
    g_a220B20[0] = 8;
    BrSub10035BD1();
}

#ifdef BR_MATCHING_BUILD

/* WHAT IT DOES: work out how many slots the track header's lookup table
 * actually needs. It scans the u16 index array at +0x20 for the largest index
 * anyone uses and stores one past it at +0x08 -- the table's used length.
 * Entry 0 is deliberately skipped and the running maximum starts at 0, so the
 * answer is at least 1 even when every index is zero. The entry count is the
 * u16 sitting 0x2000 bytes into the blob at +0x24. See slice2_20.h. */
/* The port twin of this is BrTrackF08FromMax higher up in this file; it reads
 * through BrRd16/BrPtrAt so it works on a big-endian image on any host, which
 * is exactly why it cannot reproduce these bytes. Same split as
 * BrTrackFixupList60 / BrTrackFixupAllRec54. */
/* @implements 0x10031660 glide BrTrackSetF08FromMax */

void BrTrackSetF08FromMax(int param_1)

{
  int iMax;
  int nEntry;
  int i;

  iMax = 0;
  nEntry = *(unsigned short *)(*(int *)(param_1 + 0x24) + 0x2000);
  for (i = 1; i < nEntry; i = i + 1) {
    if ((*(unsigned short **)(param_1 + 0x20))[i] > iMax) {
      iMax = (*(unsigned short **)(param_1 + 0x20))[i];
    }
  }
  *(int *)(param_1 + 8) = iMax + 1;
  return;
}

/* WHAT IT DOES: walk an array of 0x54-byte track records and fixup each one. */
/* @implements 0x100316A0 glide BrTrackFixupAllRec54 */

int BrTrackFixupAllRec54(int param_1)

{
  int iVar1;
  int iVar2;
  
  iVar1 = *(int *)(param_1 + 0x60);
  iVar2 = 0;
  if (0 < *(int *)(param_1 + 100)) {
    do {
      BrTrackFixupRec54(iVar1);
      iVar1 = iVar1 + 0x54;
      iVar2 = iVar2 + 1;
    } while (iVar2 < *(int *)(param_1 + 100));
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
