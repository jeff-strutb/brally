/* br_track.c -- startup.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_collrespsolve.h"   /* br_globals: its objects */
#include "br_coretypes.h"   /* br_globals: its objects */
#include <stdint.h>
#include <string.h>

#include "slice2_20.h"   /* g_BrLoad, BrTrackFixupRec54 */

/* XSLICE 0x10035BD1 */
/* BrSub10035BD1: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

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


/* BrSegPtrFixup: prototype in br_funcs.h */
/* BrTrackFixupSegRec: prototype in br_funcs.h */

/* WHAT IT DOES: walk the list of dword segment pointers at +0x78, byte-swap
 * and rebase each, then fix up the record it points at. */
/* @implements 0x10031910 glide BrTrackFixupSegList */

int BrTrackFixupSegList(int *param_1)

{
  char uVar1;
  int *puVar3;
  int iVar4;

  puVar3 = *(int **)(((int *)((char *)(param_1) + (0x78))));
  iVar4 = 0;
  if (0 < *(int *)(((int *)((char *)(param_1) + (0x7c))))) {
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
    } while (iVar4 < *(int *)(((int *)((char *)(param_1) + (0x7c)))));
  }
  return;
}



/* WHAT IT DOES: byte-swap a 0x28-byte record: three Vec3s then one dword. */
/* @implements 0x10031A40 glide BrTrackSwapRec28 */

int BrTrackSwapRec28(char *param_1)

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

int BrTrackSwapAllVec3(int *param_1)

{
  char *iVar1;
  int iVar2;
  
  iVar1 = *(int *)(((int *)((char *)(param_1) + (0x84))));
  iVar2 = 0;
  if (0 < *(int *)(((int *)((char *)(param_1) + (0x88))))) {
    do {
      BrSwapVec3(iVar1);
      iVar1 = iVar1 + 0xc;
      iVar2 = iVar2 + 1;
    } while (iVar2 < *(int *)(((int *)((char *)(param_1) + (0x88)))));
  }
  return;
}

/* WHAT IT DOES: byte-swap the track header's command directory: the count
 * at +0x224, then every 12-byte record from +0x164 by its kind byte (+8).
 * Kinds 0-2 swap both dwords; 3, 6, 7 swap the first only; kind 4 swaps and
 * rebases the first as a pointer, swaps the second as a vertex count and
 * byte-swaps that many Vec3s at the pointer; kind 5 does the same with the
 * LAST kind-4 count (it has none of its own). Anything above 7 is left alone.
 * The port body is BrTrackFixupCmds in slice2_20.c. */
/* @implements 0x10032190 glide BrGlTrackFixupCmds */

int BrGlTrackFixupCmds(int *param_1)

{
  char t;
  char u;
  int nVerts;
  int nCmds;
  int i;
  int k;
  char *pVtx;

  /* Every record field is an expression of the counter (the REC macros),
   * not a pointer local: with a `p = base + i*12` local VC5 proves the byte
   * accesses disjoint and hoists each pair's loads above the previous pair's
   * stores (and `pVtx = *p` above the count store); as counter expressions
   * it keeps program order, which is the original's (82 -> 4 diff bytes).
   * nVerts = 0 is written AFTER the count compose so the parameter takes
   * ebx and nVerts ebp with a lazy push (`mov ebp,0`, flags kept); the
   * count is stored then read back (`mov ebp,eax` after the store). */
  nCmds = (((*(unsigned char *)(param_1 + 0x225)
             | (unsigned int)*(unsigned char *)(param_1 + 0x224) << 8) << 8
            | *(unsigned char *)(param_1 + 0x226)) << 8)
          | *(unsigned char *)(param_1 + 0x227);
  *(int *)(param_1 + 0x224) = nCmds;
  nVerts = 0;
  i = 0;
  if (0 < nCmds) {
    do {
      switch (((*(char *)(param_1 + 0x164 + i * 0xc + ((8)))))) {
      case 0:
      case 1:
      case 2:
        t = ((*(char *)(param_1 + 0x164 + i * 0xc + ((0))))); u = ((*(char *)(param_1 + 0x164 + i * 0xc + ((3))))); ((*(char *)(param_1 + 0x164 + i * 0xc + ((3))))) = t; ((*(char *)(param_1 + 0x164 + i * 0xc + ((0))))) = u;
        t = ((*(char *)(param_1 + 0x164 + i * 0xc + ((1))))); u = ((*(char *)(param_1 + 0x164 + i * 0xc + ((2))))); ((*(char *)(param_1 + 0x164 + i * 0xc + ((2))))) = t; ((*(char *)(param_1 + 0x164 + i * 0xc + ((1))))) = u;
        t = ((*(char *)(param_1 + 0x164 + i * 0xc + ((4))))); u = ((*(char *)(param_1 + 0x164 + i * 0xc + ((7))))); ((*(char *)(param_1 + 0x164 + i * 0xc + ((7))))) = t; ((*(char *)(param_1 + 0x164 + i * 0xc + ((4))))) = u;
        t = ((*(char *)(param_1 + 0x164 + i * 0xc + ((5))))); u = ((*(char *)(param_1 + 0x164 + i * 0xc + ((6))))); ((*(char *)(param_1 + 0x164 + i * 0xc + ((6))))) = t; ((*(char *)(param_1 + 0x164 + i * 0xc + ((5))))) = u;
        break;
      case 4:
        t = ((*(char *)(param_1 + 0x164 + i * 0xc + ((0))))); u = ((*(char *)(param_1 + 0x164 + i * 0xc + ((3))))); ((*(char *)(param_1 + 0x164 + i * 0xc + ((3))))) = t; ((*(char *)(param_1 + 0x164 + i * 0xc + ((0))))) = u;
        t = ((*(char *)(param_1 + 0x164 + i * 0xc + ((1))))); u = ((*(char *)(param_1 + 0x164 + i * 0xc + ((2))))); ((*(char *)(param_1 + 0x164 + i * 0xc + ((2))))) = t; ((*(char *)(param_1 + 0x164 + i * 0xc + ((1))))) = u;
        BrSegPtrFixup((uint32_t *)&((*(int *)(param_1 + 0x164 + i * 0xc + ((0))))));
        ((*(int *)(param_1 + 0x164 + i * 0xc + ((4))))) = (((((*(unsigned char *)(param_1 + 0x164 + i * 0xc + ((5))))) | (unsigned int)((*(unsigned char *)(param_1 + 0x164 + i * 0xc + ((4))))) << 8) << 8 | ((*(unsigned char *)(param_1 + 0x164 + i * 0xc + ((6)))))) << 8) | ((*(unsigned char *)(param_1 + 0x164 + i * 0xc + ((7)))));
        nVerts = ((*(int *)(param_1 + 0x164 + i * 0xc + ((4)))));
        pVtx = ((*(int *)(param_1 + 0x164 + i * 0xc + ((0)))));
        if (0 < nVerts) {
          k = nVerts;
          do {
            BrSwapVec3(pVtx);
            pVtx = pVtx + 0xc;
            k = k - 1;
          } while (k != 0);
        }
        break;
      case 5:
        t = ((*(char *)(param_1 + 0x164 + i * 0xc + ((0))))); u = ((*(char *)(param_1 + 0x164 + i * 0xc + ((3))))); ((*(char *)(param_1 + 0x164 + i * 0xc + ((3))))) = t; ((*(char *)(param_1 + 0x164 + i * 0xc + ((0))))) = u;
        t = ((*(char *)(param_1 + 0x164 + i * 0xc + ((1))))); u = ((*(char *)(param_1 + 0x164 + i * 0xc + ((2))))); ((*(char *)(param_1 + 0x164 + i * 0xc + ((2))))) = t; ((*(char *)(param_1 + 0x164 + i * 0xc + ((1))))) = u;
        BrSegPtrFixup((uint32_t *)&((*(int *)(param_1 + 0x164 + i * 0xc + ((0))))));
        pVtx = ((*(int *)(param_1 + 0x164 + i * 0xc + ((0)))));
        if (0 < nVerts) {
          k = nVerts;
          do {
            BrSwapVec3(pVtx);
            pVtx = pVtx + 0xc;
            k = k - 1;
          } while (k != 0);
        }
        break;
      case 3:
      case 6:
      case 7:
        /* The leading pair is a HALFWORD compose here (`mov ah,[0]; mov
         * al,[1]` in that order); the plain Horner chain used by the other
         * two composes loads al first at this site (4 diff bytes). Dead at
         * this site: `|` operand order, a temp, a 4-statement accumulator,
         * `+`/`*256`, an explicit default arm. */
        ((*(int *)(param_1 + 0x164 + i * 0xc + ((0))))) = (((unsigned int)(unsigned short)(((*(unsigned char *)(param_1 + 0x164 + i * 0xc + ((1))))) | ((*(unsigned char *)(param_1 + 0x164 + i * 0xc + ((0))))) << 8) << 8 | ((*(unsigned char *)(param_1 + 0x164 + i * 0xc + ((2)))))) << 8)
                  | ((*(unsigned char *)(param_1 + 0x164 + i * 0xc + ((3)))));
        break;
      }
      i = i + 1;
    } while (i < *(int *)(param_1 + 0x224));
  }
#undef REC
#undef RECU
#undef RECD
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

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
    (*(int *)((char *)&g_brCrPlane + 0x18)) = (int)&(g_aBrCarPhysDrvT1A[0]);
    DAT_11778820 = (int)&(g_aBrCarPhysDrvT2A[0]);
    g_pBrCarPhysGrip = (int)&(g_aBrCarPhysGripA[0]);
    (*(int *)&DAT_100b5170) = 0x3f800000;
    return;
  case 5:
  case 0xb:
  case 0xd:
  case 0xe:
  default:
    (*(int *)((char *)&g_brCrPlane + 0x18)) = (int)&(g_aBrCarPhysDrvT1B[0]);
    DAT_11778820 = (int)&(g_aBrCarPhysDrvT2B[0]);
    g_pBrCarPhysGrip = (int)&(g_aBrCarPhysGripB[0]);
    (*(int *)&DAT_100b5170) = 0x3f666666;
    return;
  }
}


/* ==========================================================================
 * Filed out of slice2_20.c.  These four sit either side of the block above
 * in the original (0x10031660..0x10032680), so the compiler's view of them
 * is the batch's: <string.h> and slice2_20.h, added at the top.
 * ========================================================================== */

/* WHAT IT DOES: copies the actual texture pixels and colour palettes out of
 * the loaded track image and into the places the texture records point at,
 * for those records that ask for it. Records that fail any of half a dozen
 * checks are quietly skipped. */
/* The two globals the original reads here (g_BrLoad gathers them for the
 * port, see slice2_20.h): the texture/TLUT byte base and the parallel flag
 * array.  Named separately so each relocation resolves to its own variable
 * (config/globals_glide.csv). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x106B7C7C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x106EECF4 */
/* @implements 0x10038450 d3d BrTexCopyRecords */
void BrTexCopyRecords(void *pvTable, int cRecords)
{
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

        memcpy(pDst, g_brRcaBlob + *(uint32_t *)(void *)(pDesc + 0x0C),
               cb);

        pDst = *(uint8_t **)(void *)(pWalk - 4);
        if (pDst == 0)
            goto next;

        {
            char    *pFlags;
            uint32_t uSel;
            uint32_t cbPal;

            pDesc  = *(uint8_t **)(void *)pWalk;
            pFlags = (char *)g_brLoadTexFlags;
            pFlags += adj;
            uSel    = *(uint32_t *)(void *)(pFlags + (int32_t)(uint32_t)pWalk
                                            + 0x20);
            uSel   &= 0x0F000000u;
            cbPal   = (uSel == 0x01000000u) ? 0x20u : 0x200u;
            memcpy(pDst,
                   g_brRcaBlob + *(uint32_t *)(void *)(pDesc + 0x10),
                   cbPal);
        }
    next:
        pWalk += 0x24;
    } while (--n);
}

/* WHAT IT DOES: clears a block of state, plants an 8 in its first slot, and
 * calls on to further setup. What the block holds is not established here. */
/* @implements 0x10039000 d3d BrInit220B20 */
void BrInit220B20(void)
{
    memset((*(uint32_t (*)[70])&g_a220B20), 0, sizeof(uint32_t) * 0x46);
    (*(uint32_t (*)[70])&g_a220B20)[0] = 8;
    BrSessionReinitVideo();
}


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

void BrTrackSetF08FromMax(int *param_1)

{
  int iMax;
  int nEntry;
  int i;

  iMax = 0;
  nEntry = *(unsigned short *)(*(int *)(((int *)((char *)(param_1) + (0x24)))) + 0x2000);
  for (i = 1; i < nEntry; i = i + 1) {
    if ((*(unsigned short **)(((int *)((char *)(param_1) + (0x20)))))[i] > iMax) {
      iMax = (*(unsigned short **)(((int *)((char *)(param_1) + (0x20)))))[i];
    }
  }
  *(int *)(((int *)((char *)(param_1) + (8)))) = iMax + 1;
  return;
}

/* WHAT IT DOES: walk an array of 0x54-byte track records and fixup each one. */
/* @implements 0x100316A0 glide BrTrackFixupAllRec54 */

int BrTrackFixupAllRec54(int *param_1)

{
  char *iVar1;
  int iVar2;
  
  iVar1 = *(int *)(((int *)((char *)(param_1) + (0x60))));
  iVar2 = 0;
  if (0 < *(int *)(((int *)((char *)(param_1) + (100))))) {
    do {
      BrTrackFixupRec54(iVar1);
      iVar1 = iVar1 + 0x54;
      iVar2 = iVar2 + 1;
    } while (iVar2 < *(int *)(((int *)((char *)(param_1) + (100)))));
  }
  return;
}

/* BrSwapVec3Array: prototype in br_funcs.h */
/* BrRcaFixupArray: prototype in br_funcs.h */
/* BrSwapU16Array: prototype in br_funcs.h */
/* BrSwapU16x4Array: prototype in br_funcs.h */
/* BrF3DListFixup: prototype in br_funcs.h */
/* BrFontSetRenderDst: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: the whole endian fixup pass over a freshly loaded track
 * blob. Byte-swaps the Vec3 array, the texture records, both u16 index
 * tables (each 0x1001 entries of header, then as many entries as the u16 at
 * +0x2000 of its blob says), sizes the lookup tables by scanning for the
 * largest index actually used, swaps the string table up to its terminating
 * zero, then runs the display-list fixup, the render-destination switches,
 * the optional texture-pixel copy (video mode 3 only), the two installed
 * fixup hooks, and the per-record, segment-list and Vec3 fixups. */
/* @implements 0x100314D0 glide BrGlTrackFixupAll */

void BrGlTrackFixupAll(int *param_1)

{
  short sVar1;
  char *iVar2;
  short *psVar3;
  unsigned short *puVar4;
  int iMax2;
  int iMax1;
  char *i;

  BrSwapVec3Array(*(int *)(param_1 + 0x14),*(int *)(param_1 + 0x10));
  BrRcaFixupArray(*(int *)(param_1 + 0x1c),*(int *)(param_1 + 0x18));
  BrSwapU16Array(*(int *)(param_1 + 0x24),0x1001);
  BrSwapU16Array(*(int *)(param_1 + 0x20),
                 *(unsigned short *)(*(int *)(param_1 + 0x24) + 0x2000));
  iMax1 = 0;
  iVar2 = *(unsigned short *)(*(int *)(param_1 + 0x24) + 0x2000) - 1;
  if (1 <= iVar2) {
    puVar4 = (unsigned short *)(*(int *)(param_1 + 0x20) + iVar2 * 2);
    i = iVar2;
    do {
      if (*puVar4 > iMax1) {
        iMax1 = *puVar4;
      }
      puVar4 = puVar4 + -1;
      i = i + -1;
    } while (i != 0);
  }
  BrSwapU16Array(*(int *)(param_1 + 0x90),iMax1 + 1);
  iMax2 = 0;
  if (1 <= iMax1) {
    puVar4 = (unsigned short *)(*(int *)(param_1 + 0x90) + iMax1 * 2);
    do {
      if (*puVar4 > iMax2) {
        iMax2 = *puVar4;
      }
      puVar4 = puVar4 + -1;
      iMax1 = iMax1 - 1;
    } while (iMax1 != 0);
  }
  iVar2 = *(int *)(param_1 + 0x8c);
  psVar3 = (short *)(iVar2 + iMax2 * 2);
  sVar1 = *(short *)(iVar2 + iMax2 * 2);
  while (sVar1 != 0) {
    psVar3 = psVar3 + 1;
    iMax2 = iMax2 + 1;
    sVar1 = *psVar3;
  }
  BrSwapU16Array(iVar2,iMax2);
  BrTrackSetF08FromMax(param_1);
  BrSwapU16x4Array(*(int *)(param_1 + 0xc),*(int *)(param_1 + 8));
  BrF3DListFixup(*(int *)(param_1 + 0x50));
  BrFontSetRenderDst(4);
  if (DAT_104b15e8 == 3) {
    BrTexCopyRecords(*(void **)(param_1 + 0x1c),*(int *)(param_1 + 0x18));
  }
  (*(*(int (**)())&g_pfn18AA0C4))(*(int *)(param_1 + 0x50));
  BrTrackFixupAllRec54(param_1);
  BrFontSetRenderDst(1);
  (*(*(int (**)())&DAT_118ed1e4))(*(int *)(param_1 + 0x1c),*(int *)(param_1 + 0x18));
  BrSwapU16Array(*(int *)(param_1 + 0x6c),0x1001);
  BrSwapU16Array(*(int *)(param_1 + 0x68),
                 *(unsigned short *)(*(int *)(param_1 + 0x6c) + 0x2000));
  BrTrackFixupSegList(param_1);
  BrTrackSwapAllVec3(param_1);
  return;
}



/* 0x100311C0 BrTrackLoad and what it needs.  _CRTIMP is already the
 * dllimport form above. */
#include <stdlib.h>
#include <stdio.h>

/* Forward declarations for unknown functions/globals */
/* FUN_10031140: prototype in br_funcs.h */
/* FUN_10018a10: prototype in br_funcs.h */
/* FUN_10018a40: prototype in br_funcs.h */
/* FUN_10003320: prototype in br_funcs.h */
/* FUN_100032d0: prototype in br_funcs.h */
/* FUN_10031b80: prototype in br_funcs.h */
/* FUN_100034c0: prototype in br_funcs.h */
/* FUN_100035e0: prototype in br_funcs.h */
/* FUN_10032190: prototype in br_funcs.h */
/* FUN_10030f50: prototype in br_funcs.h */
/* FUN_1006c910: prototype in br_funcs.h */
/* FUN_1006c950: prototype in br_funcs.h */
/* FUN_100314d0: prototype in br_funcs.h */
/* FUN_10034a70: prototype in br_funcs.h */
/* FUN_100347f0: prototype in br_funcs.h */
/* FUN_1005a780: prototype in br_funcs.h */
/* FUN_10069530: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* The instance records are a typed 0x54-byte array indexed by the loop
 * counter in a plain for loop: that is what schedules the unit-vector
 * stores below the argument pushes, forms arg3 with `add edx,esi`, and
 * gives `fld st(0); fmul [rec]` on the first scale test (a byte-offset
 * do/while gives the lea form and loads the field first).  The flag word
 * at +0x4C is a 16-bit bitfield: setting bit 13 emits `or byte
 * [..+0x4D],0x20` with the field's `lea [..+0x4C]` left behind. */
typedef struct BrTrkInst {
    float m[16];                  /* +0x00 */
    float fInvScale;              /* +0x40 */
    int   f44, f48;
    unsigned short lo : 13;       /* +0x4C */
    unsigned short f20 : 1;
    unsigned short hi : 2;
    unsigned short w4e;
    int   f50;
} BrTrkInst;
/* WHAT IT DOES: load one track: reset the handling data, set the segment
 * bases for the track heap, build "tracks/<name>.trk", read the 0x230-byte
 * header and the rest of the blob (capped at 4,000,000 bytes), run the
 * command fixup, read the four sky-texture files for the track, compute the
 * heap window globals, run the full endian/pointer fixup, then for every
 * 0x54-byte instance record derive the inverse scale of its matrix (marking
 * pure uniform scales with flag 0x20), and finish with the node mark pass
 * and the per-track surface table. Aborts with printf+exit(1) on a too-big
 * file, too many instances, or a header-size mismatch. */
/* @implements 0x100311C0 glide BrTrackLoad */
void BrTrackLoad(int param_1)

{
  FILE **uVar3;
  int iVar6;
  float fVar11;
  struct { float x; float y; float z; } local_40c;
  char local_400 [1024];

  BrTrackLoadHandling(param_1);
  (*(int *)&g_brKeyBias) = 0x80025c00 - (int)DAT_106eff08;
  BrSegSetBases(0x80025c00, (unsigned int)DAT_106eff08);
  BrSegSetFlag(1);
  strcpy(local_400, s_tracks__100b74c0);
  strcat(local_400, (&PTR_s_desert_trk_100b78c0)[param_1]);
  uVar3 = BrChkFReadOpen(local_400);
  iVar6 = BrChkFileSize(uVar3);
  BrGlTrackHdrRead(&DAT_106eecd8, uVar3);
  if (4000000 < iVar6) {
    printf(DAT_100aa3a4, param_1, iVar6, 4000000);
    exit(1);
  }
  BrChkFRead(&DAT_106f0138, 1, iVar6 + -0x230, uVar3);
  BrChkFClose(uVar3);
  BrGlTrackFixupCmds(&DAT_106eecd8);
  BrFileReadInto(&DAT_118ed1f0, (&PTR_s_cargfx_skytexdesert_lut4_100bb30c)[param_1 * 0x5f], 0x20);
  BrFileReadInto(&DAT_118eda10, (&PTR_s_cargfx_skytexdesert_lut4_100bb30c)[param_1 * 0x5f] + 0x20,
               -1);
  BrFileReadInto(&DAT_118ee210, (&PTR_s_cargfx_skytexdesertn_lut4_100bb314)[param_1 * 0x5f], 0x20);
  BrFileReadInto(&DAT_118ed210, (&PTR_s_cargfx_skytexdesertn_lut4_100bb314)[param_1 * 0x5f] + 0x20,
               -1);
  BrSub10073AC0();
  BrSub10073B00();
  _DAT_106ec77c = (int)DAT_106eff08 - DAT_106eecdc;
  (*(unsigned char * *)&g_brRcaBlob) = DAT_106eff08 + DAT_106eecd8;
  BrGlTrackFixupAll(&DAT_106eecd8);
  _DAT_100aa02c = -1;
  DAT_100aa030 = -1;
  for (iVar6 = 0; iVar6 < g_BrSpanCount; iVar6++) {
      local_40c.x = 1.0f;
      local_40c.y = 0.0f;
      local_40c.z = 0.0f;
      BrMtxXfmDir3((int *)&local_40c, (int *)&local_40c, (int)&((BrTrkInst *)g_BrDrawTrackFlags)[iVar6]);
      fVar11 = BrVec3Length((int *)&local_40c);
      if (fVar11 != _DAT_10077528) {
        fVar11 = _DAT_10077524 / fVar11;
        if (fVar11 * ((BrTrkInst *)g_BrDrawTrackFlags)[iVar6].m[0] == _DAT_10077524 && ((BrTrkInst *)g_BrDrawTrackFlags)[iVar6].m[5] * fVar11 == _DAT_10077524 && ((BrTrkInst *)g_BrDrawTrackFlags)[iVar6].m[10] * fVar11 == _DAT_10077524)
          ((BrTrkInst *)g_BrDrawTrackFlags)[iVar6].f20 = 1;
        ((BrTrkInst *)g_BrDrawTrackFlags)[iVar6].fInvScale = fVar11;
      }
  }
  if (0x800 < g_BrSpanCount) {
    printf(DAT_100aa378, g_BrSpanCount, 0x800);
    exit(1);
  }
  if (DAT_106eecdc != 0x230) {
    printf(DAT_100aa348, DAT_106eecdc, 0x230);
    exit(1);
  }
  BrNodeRunMarkPass();
  BrTrackSurfaceSet(param_1);
}

/* FUN_10031030: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* ".hnt" */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* "%s%s" */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* "tracks/" */

/* WHAT IT DOES: load one track's texture-detail hint file: builds
 * "tracks/<track file>" from the track table, swaps the extension for
 * ".hnt" and hands the path to the threshold loader (0x10031030), which
 * picks the texture detail level from it.  (The name is historical: this
 * is not handling data.) */
/* @implements 0x10031140 glide BrTrackLoadHandling */
void BrTrackLoadHandling(int iTrack)
{
  char szPath[1024];

  sprintf(szPath, g_szBrFmtSS, s_tracks__100b74c0,
          (&PTR_s_desert_trk_100b78c0)[iTrack]);
  strcpy(strrchr(szPath, '.'), DAT_100aa338);
  FUN_10031030(szPath);
}
