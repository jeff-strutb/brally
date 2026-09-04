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

#ifdef BR_MATCHING_BUILD

int FUN_1006e1d0();
extern int g_AC300;

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
