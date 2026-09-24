/* br_model.c -- drawing: model records.
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

#ifdef BR_MATCHING_BUILD

int FUN_1006e1d0();
extern int g_AC300;

extern char DAT_10af1208[];                 /* entrant records, 0x2B68 each */
extern int  DAT_100b3858;                   /* entrant count                */
extern int  DAT_106ed6ac;
extern int  DAT_106ed6b4;
extern void *(*DAT_118ed1d4)(int hTex, unsigned int *pSize);
void  BrImgTintSetScale(int r, int g, int b);           /* 0x1005A4E0 */
void *BrChkAlloc(unsigned int size, const char *pWhat); /* 0x100036F0 */

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
    char *pSlot;
    char *penm;
    char *pEnt;
    unsigned short *pPal;
    void *pSrc;
    unsigned int size;
    unsigned int r, g, b;
    unsigned short w;
    int i, cptex;

    pSlot = DAT_10af1208 + (*(int *)(pCar + 0x74) + DAT_100b3858) * 0x2b68;
    BrImgTintSetScale(pCar[0x5c], pCar[0x5d], pCar[0x5e]);
    r = pCar[0x5c] >> 3;
    g = pCar[0x5d] >> 3;
    b = pCar[0x5e] >> 3;
    for (i = 0; i < 12; i++) {
        penm = *(char **)(pSlot + 0x29c4);
        pEnt = *(char **)(penm + 0x8014) + *(unsigned char *)(penm + 0x8110 + i) * 0x24;
        pPal = *(unsigned short **)(pEnt + 4);
        if (pPal != 0 && (*(unsigned int *)(pEnt + 0x20) & 0xf000000) == 0x1000000) {
            w = ((r << 5 | g) << 5 | b) << 1 | (pPal[i] & 1);
            pPal[0] = (unsigned short)((unsigned char)(w >> 8) | ((unsigned char)w << 8));
            w = ((r & 0x1e) << 5 | (g & 0x1e)) << 5 | (b & 0x1e) | (pPal[i] & 1);
            pPal[1] = (unsigned short)((unsigned char)(w >> 8) | ((unsigned char)w << 8));
        }
    }
    cptex = *(int *)(*(int *)(pSlot + 0x29c4) + 0x7c);
    *(int *)(pCar + 0x7c) = cptex + 4;
    *(void ***)(pCar + 0x78) = (void **)BrChkAlloc((cptex + 4) * 4, "MakeEnemyCarColorPanels: penm->aptex");
    for (i = 0; i < cptex; i++) {
        pSrc = DAT_118ed1d4(*(int *)(*(int *)(pSlot + 0x29c4) + 4 + i * 4), &size);
        (*(void ***)(pCar + 0x78))[i] = BrChkAlloc(size, "MakeEnemyCarColorPanels: penm->aptex[i]");
        memcpy((*(void ***)(pCar + 0x78))[i], pSrc, size);
    }
    pEnt = *(char **)(*(int *)(pSlot + 0x29c4) + 0x8014) +
           *(unsigned char *)(*(int *)(pSlot + 0x29c4) + 0x811b) * 0x24;
    pPal = *(unsigned short **)(pEnt + 4);
    if (pPal != 0 && g_AC300 == 0) {
        if (*(int *)(*(int *)(pSlot + 0x29c4) + 0x84) != 0) {
            if (DAT_106ed6ac == 0 && DAT_106ed6b4 == 0) {
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
            pSrc = DAT_118ed1d4(*(int *)(*(int *)(pSlot + 0x29c4) + 0x84), &size);
            (*(void ***)(pCar + 0x78))[cptex] = BrChkAlloc(size, "MakeEnemyCarColorPanels: penm->aptex[cptex+0]");
            memcpy((*(void ***)(pCar + 0x78))[cptex], pSrc, size);
        }
        if (*(int *)(*(int *)(pSlot + 0x29c4) + 0x88) != 0) {
            pPal[0xb] = 0x6bad;
            pPal[0xe] = 0xc0;
            pPal[0xd] = 0xc0;
            pPal[0x6] = 0x31c6;
            pPal[0x9] = 0x4f9;
            pPal[0x8] = 0x4f9;
            DAT_118ed1d4(*(int *)(*(int *)(pSlot + 0x29c4) + 0x88), &size);
            (*(void ***)(pCar + 0x78))[cptex + 1] = BrChkAlloc(size, "MakeEnemyCarColorPanels: penm->aptex[cptex+1]");
            memcpy((*(void ***)(pCar + 0x78))[cptex + 1], pSrc, size);
        }
        if (*(int *)(*(int *)(pSlot + 0x29c4) + 0x8c) != 0) {
            pPal[0xe] = 400;
            pPal[0xd] = pPal[0xf];
            pPal[0x9] = 0x1a0;
            pPal[0x8] = pPal[0xa];
            pPal[0xb] = 0x38e7;
            pPal[0x6] = 0xfeff;
            DAT_118ed1d4(*(int *)(*(int *)(pSlot + 0x29c4) + 0x8c), &size);
            (*(void ***)(pCar + 0x78))[cptex + 2] = BrChkAlloc(size, "MakeEnemyCarColorPanels: penm->aptex[cptex+2]");
            memcpy((*(void ***)(pCar + 0x78))[cptex + 2], pSrc, size);
        }
        if (*(int *)(*(int *)(pSlot + 0x29c4) + 0x90) != 0) {
            pPal[0xb] = 0x38e7;
            pPal[0xe] = 0xc0;
            pPal[0xd] = 0xc0;
            pPal[0x6] = 0xfeff;
            pPal[0x9] = 0x4f9;
            pPal[0x8] = 0x4f9;
            DAT_118ed1d4(*(int *)(*(int *)(pSlot + 0x29c4) + 0x90), &size);
            (*(void ***)(pCar + 0x78))[cptex + 3] = BrChkAlloc(size, "MakeEnemyCarColorPanels: penm->aptex[cptex+3]");
            memcpy((*(void ***)(pCar + 0x78))[cptex + 3], pSrc, size);
        }
    }
}

/* WHAT IT DOES: apply texture slots from one model record to another, including optional extra slots when enabled. */
/* @implements 0x1005F220 glide BrModelSlotApply */

int BrModelSlotApply(int param_1,int param_2)

{
  int iVar1;
  int iVar2;
  
  iVar2 = 0;
  iVar1 = *(int *)(*(int *)(param_1 + 0x29c4) + 0x7c);
  if (0 < iVar1) {
    do {
      FUN_1006e1d0(*(int *)(*(int *)(param_1 + 0x29c4) + 4 + iVar2 * 4),
                   *(int *)(*(int *)(param_2 + 0x78) + iVar2 * 4));
      iVar2 = iVar2 + 1;
    } while (iVar2 < iVar1);
  }
  iVar2 = *(int *)(param_1 + 0x29c4);
  if ((*(int *)(*(int *)(iVar2 + 0x8014) + 4 + (unsigned int)*(unsigned char *)(iVar2 + 0x811b) * 0x24) != 0) &&
     (g_AC300 == 0)) {
    if (*(int *)(iVar2 + 0x84) != 0) {
      FUN_1006e1d0(*(int *)(iVar2 + 0x84),*(int *)(*(int *)(param_2 + 0x78) + iVar1 * 4));
    }
    iVar2 = *(int *)(*(int *)(param_1 + 0x29c4) + 0x88);
    if (iVar2 != 0) {
      FUN_1006e1d0(iVar2,*(int *)(*(int *)(param_2 + 0x78) + 4 + iVar1 * 4));
    }
    iVar2 = *(int *)(*(int *)(param_1 + 0x29c4) + 0x8c);
    if (iVar2 != 0) {
      FUN_1006e1d0(iVar2,*(int *)(*(int *)(param_2 + 0x78) + 8 + iVar1 * 4));
    }
    iVar2 = *(int *)(*(int *)(param_1 + 0x29c4) + 0x90);
    if (iVar2 != 0) {
      FUN_1006e1d0(iVar2,*(int *)(*(int *)(param_2 + 0x78) + 0xc + iVar1 * 4));
    }
  }
  return;
}

#endif /* BR_MATCHING_BUILD */

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl (this, r, g, b).  Original is thiscall with
 * ret 0xC; hide that prototype so the definition can take the struct-arg
 * __fastcall shape that reproduces it. */
#define BrRgbSinkSet BrRgbSinkSet_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* slice2_19.h / br_seg.h declare these cdecl with a leading state pointer the
 * originals do not have.  Hide those prototypes so BrModelLoad can call them
 * with the shapes the bytes show. */
#define BrSub100088B0 BrSub100088B0_cdecl
#define BrSegSetBases BrSegSetBases_cdecl
#endif
#include "slice2_19.h"
#ifdef BR_MATCHING_BUILD
#undef BrSub100088B0
#undef BrSegSetBases
typedef struct { void *p; } BrModelLoadArg;
extern int g_brModelMgr;                        /* 0x10AC0810 */
void * __fastcall BrSub100088B0(void *pThis, BrModelLoadArg a,
                                BrModelLoadArg b);
void BrSegSetBases(uint32_t n64Base, uint32_t hostBase);
#endif
#ifdef BR_MATCHING_BUILD
#undef BrRgbSinkSet
#endif

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
#ifdef BR_MATCHING_BUILD
void *BrModelLoad(void *a1, void *a2)
{
    BrModelLoadArg x, y;
    void *p;

    x.p = a2;
    y.p = a1;
    p = BrSub100088B0(&g_brModelMgr, x, y);

    BrSegSetBases(0, (uint32_t)(uintptr_t)p);
    BrModelSwap(p);
    return p;
}
#else
void *BrModelLoad(void *pMgr, void *a1, void *a2)
{
    void *p;

    /* GOTCHA: a2 is pushed last, so it is the callee's FIRST argument. */
    p = BrSub100088B0(pMgr, a2, a1);

    BrSegSetBases(g_BrSegMap, 0, (uint32_t)(uintptr_t)p);
    BrModelSwap(p);
    return p;
}
#endif
