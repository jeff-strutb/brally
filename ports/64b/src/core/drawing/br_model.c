/* br_model.c -- drawing: model records.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice1_05.h"   /* br_globals: its objects */
#include <stdint.h>
#include "slice3_41.h"
#include <string.h>


/* FUN_1006e1d0: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */                 /* entrant records, 0x2B68 each */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                   /* entrant count                */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrImgTintSetScale: prototype in br_funcs.h */
/* BrChkAlloc: prototype in br_funcs.h */

/* WHAT IT DOES: gives an enemy car its own colour: sets the tint scale from
 * the car's RGB, writes the colour (full and half intensity, RGBA5551,
 * byte-swapped) into the first two entries of each of the model's twelve
 * panel palettes, then takes private copies of every model texture into the
 * car's own texture table -- plus, when the livery palette exists and no mode
 * change is pending, up to four extra decal textures after re-seating the
 * livery quad's coordinates for each.  The three later extras copy from the
 * previous texture's data (the original never keeps their fetch result). */
/* @implements 0x1005EDC0 glide BrMakeEnemyCarColorPanels */
void BrMakeEnemyCarColorPanels(unsigned char *pCar)
{
    BrDriver *drv = (BrDriver *)pCar;   /* the driver slot, 0x80 B in the original */
    BrDriverCar *pSlot;
    char *penm;
    char *pEnt;
    unsigned short *pPal;
    void *pSrc;
    unsigned int size;
    unsigned int r, g, b;
    unsigned short w;
    int i, cptex;

    pSlot = &g_aBrRaceCar[drv->f74 + (*(int *)&g_brRaceNEntrant)];
    BrImgTintSetScale(drv->f5C, drv->f5D, drv->f5E);
    r = drv->f5C >> 3;
    g = drv->f5D >> 3;
    b = drv->f5E >> 3;
    for (i = 0; i < 12; i++) {
        penm = pSlot->pModel;
        pEnt = BR_AT32(char *, penm + 0x8014) + *(unsigned char *)(penm + 0x8110 + i) * 0x24;
        pPal = BR_AT32(unsigned short *, pEnt + 4);
        if (pPal != 0 && (*(unsigned int *)(pEnt + 0x20) & 0xf000000) == 0x1000000) {
            w = ((r << 5 | g) << 5 | b) << 1 | (pPal[i] & 1);
            pPal[0] = (unsigned short)((unsigned char)(w >> 8) | ((unsigned char)w << 8));
            w = ((r & 0x1e) << 5 | (g & 0x1e)) << 5 | (b & 0x1e) | (pPal[i] & 1);
            pPal[1] = (unsigned short)((unsigned char)(w >> 8) | ((unsigned char)w << 8));
        }
    }
    cptex = *(int *)(((char *)pSlot->pModel) + 0x7c);
    drv->cptex = cptex + 4;
    drv->aptex = (void **)BrChkAlloc((cptex + 4) * sizeof(void *), "MakeEnemyCarColorPanels: penm->aptex");
    for (i = 0; i < cptex; i++) {
        pSrc = (*(void * (**)(int, unsigned int *))&DAT_118ed1d4)(*(int *)(((char *)pSlot->pModel) + 4 + i * 4), &size);
        drv->aptex[i] = BrChkAlloc(size, "MakeEnemyCarColorPanels: penm->aptex[i]");
        memcpy(drv->aptex[i], pSrc, size);
    }
    pEnt = BR_AT32(char *, ((char *)pSlot->pModel) + 0x8014) +
           *(unsigned char *)(((char *)pSlot->pModel) + 0x811b) * 0x24;
    pPal = BR_AT32(unsigned short *, pEnt + 4);
    if (pPal != 0 && (*(int *)&g_AC300) == 0) {
        if (*(int *)(((char *)pSlot->pModel) + 0x84) != 0) {
            if ((*(int *)((char *)&g_aBrEntRecs + 0x7C)) == 0 && (*(int *)((char *)&g_aBrEntRecs + 0x84)) == 0) {
                pPal[0xf] = 400;
                pPal[0xa] = 0x1a0;
            } else {
                pPal[0xf] = 0x70;
                pPal[0xa] = 0x8290;
            }
            pPal[0xe] = 400;
            pPal[0xd] = pPal[0xf];
            pPal[0x9] = 0x1a0;
            pPal[0x8] = pPal[0xa];
            pPal[0xc] = 0x8179;
            pPal[0x7] = 0x4192;
            pPal[0xb] = 0x6bad;
            pPal[0x6] = 0x31c6;
            pSrc = (*(void * (**)(int, unsigned int *))&DAT_118ed1d4)(*(int *)(((char *)pSlot->pModel) + 0x84), &size);
            drv->aptex[cptex] = BrChkAlloc(size, "MakeEnemyCarColorPanels: penm->aptex[cptex+0]");
            memcpy(drv->aptex[cptex], pSrc, size);
        }
        if (*(int *)(((char *)pSlot->pModel) + 0x88) != 0) {
            pPal[0xb] = 0x6bad;
            pPal[0xe] = 0xc0;
            pPal[0xd] = 0xc0;
            pPal[0x6] = 0x31c6;
            pPal[0x9] = 0x4f9;
            pPal[0x8] = 0x4f9;
            (*(void * (**)(int, unsigned int *))&DAT_118ed1d4)(*(int *)(((char *)pSlot->pModel) + 0x88), &size);
            drv->aptex[cptex + 1] = BrChkAlloc(size, "MakeEnemyCarColorPanels: penm->aptex[cptex+1]");
            memcpy(drv->aptex[cptex + 1], pSrc, size);
        }
        if (*(int *)(((char *)pSlot->pModel) + 0x8c) != 0) {
            pPal[0xe] = 400;
            pPal[0xd] = pPal[0xf];
            pPal[0x9] = 0x1a0;
            pPal[0x8] = pPal[0xa];
            pPal[0xb] = 0x38e7;
            pPal[0x6] = 0xfeff;
            (*(void * (**)(int, unsigned int *))&DAT_118ed1d4)(*(int *)(((char *)pSlot->pModel) + 0x8c), &size);
            drv->aptex[cptex + 2] = BrChkAlloc(size, "MakeEnemyCarColorPanels: penm->aptex[cptex+2]");
            memcpy(drv->aptex[cptex + 2], pSrc, size);
        }
        if (*(int *)(((char *)pSlot->pModel) + 0x90) != 0) {
            pPal[0xb] = 0x38e7;
            pPal[0xe] = 0xc0;
            pPal[0xd] = 0xc0;
            pPal[0x6] = 0xfeff;
            pPal[0x9] = 0x4f9;
            pPal[0x8] = 0x4f9;
            (*(void * (**)(int, unsigned int *))&DAT_118ed1d4)(*(int *)(((char *)pSlot->pModel) + 0x90), &size);
            drv->aptex[cptex + 3] = BrChkAlloc(size, "MakeEnemyCarColorPanels: penm->aptex[cptex+3]");
            memcpy(drv->aptex[cptex + 3], pSrc, size);
        }
    }
}

/* WHAT IT DOES: apply texture slots from one model record to another, including optional extra slots when enabled. */
/* @implements 0x1005F220 glide BrModelSlotApply */

void BrModelSlotApply(BrDriverCar *param_1, BrDriver *param_2)

{
  const uint8_t *m = (const uint8_t *)param_1->pModel;
  void **aptex = param_2->aptex;
  int iVar1;
  int iVar2;

  iVar2 = 0;
  iVar1 = *(const int32_t *)(m + 0x7c);
  if (0 < iVar1) {
    do {
      BrTexQueuePush(*(const int32_t *)(m + 4 + iVar2 * 4), aptex[iVar2]);
      iVar2 = iVar2 + 1;
    } while (iVar2 < iVar1);
  }
  if ((*(const uint32_t *)(BR_AT32(const uint8_t *, m + 0x8014) + 4 + (unsigned int)m[0x811b] * 0x24) != 0) &&
     ((*(int *)&g_AC300) == 0)) {
    if (*(const int32_t *)(m + 0x84) != 0) {
      BrTexQueuePush(*(const int32_t *)(m + 0x84), aptex[iVar1]);
    }
    iVar2 = *(const int32_t *)(m + 0x88);
    if (iVar2 != 0) {
      BrTexQueuePush(iVar2, aptex[iVar1 + 1]);
    }
    iVar2 = *(const int32_t *)(m + 0x8c);
    if (iVar2 != 0) {
      BrTexQueuePush(iVar2, aptex[iVar1 + 2]);
    }
    iVar2 = *(const int32_t *)(m + 0x90);
    if (iVar2 != 0) {
      BrTexQueuePush(iVar2, aptex[iVar1 + 3]);
    }
  }
}


/* Header prototype is cdecl (this, r, g, b).  Original is thiscall with
 * ret 0xC; hide that prototype so the definition can take the struct-arg
 * __fastcall shape that reproduces it. */
#define BrRgbSinkSet BrRgbSinkSet_hdr
/* slice2_19.h / br_seg.h declare these cdecl with a leading state pointer the
 * originals do not have.  Hide those prototypes so BrModelLoad can call them
 * with the shapes the bytes show. */
#define BrSub100088B0 BrSub100088B0_cdecl
#define BrSegSetBases BrSegSetBases_cdecl
#include "slice2_19.h"
#undef BrSub100088B0
#undef BrSegSetBases
typedef struct { void *p; } BrModelLoadArg;
/* 64-bit core: declared once, in br_globals.h or its struct's header */                        /* 0x10AC0810 */
/* BrSub100088B0: prototype in br_funcs.h */
/* BrSegSetBases: prototype in br_funcs.h */
#undef BrRgbSinkSet

#include <string.h>

/* WHAT IT DOES: loads a model from disk and makes it ready to draw -- reads
 * the file in, tells the address fixer where it landed, and runs the
 * byte-order and address correction over it. */
/* @implements 0x10036BD0 d3d BrModelLoad */
/* TWO arguments, not three, and the first callee is a thiscall.  The original
 * reads [esp+4] and [esp+8] only; the `pMgr` parameter is really the constant
 * 0x10AC0810 loaded into ecx (`mov ecx, 0x10ac0810`), so the loader is a
 * thiscall member on a fixed object.  Its two stack arguments are spelled as
 * one-pointer STRUCTS so neither can claim edx -- the convention slice1_09.c
 * already uses -- which is what makes a multi-argument thiscall reachable
 * from a CALL site at all.
 *
 * BrSegSetBases likewise takes two arguments here, not three: the original
 * pushes 0 and the loaded block and nothing else.  br_seg.c's matching body
 * already records that its third parameter is the port's own pMap slot, so
 * this call site simply declares the two-argument shape. */
void *BrModelLoad(void *pBuf, const char *pszName)
{
    void *p;

    /* 0x10008A90 on the archive object: read the named entry into pBuf. */
    p = M8A90(&g_brModelMgr, pszName, pBuf);

    BrSegSetBases(0, p);
    BrModelSwap(p);
    return p;
}
