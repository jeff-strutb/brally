/* br_obj.c -- see br_obj.h. */
#include "br_obj.h"

/* 0x10073B80 -- stores in the order +8, +0xC, +0, +4; order is irrelevant to
 * callers but the SET of fields is not: +0x10 is left untouched. */
/* WHAT IT DOES: clears the first four fields of an object header while
 * deliberately leaving the fifth alone. Which fields are cleared is the
 * whole content of the routine; the order they are cleared in is not. */
/* @implements 0x10073B80 d3d BrObjClear */
void BR_THISCALL1 BrObjClear(BrObjHeader *pObj)
{
    pObj->f08 = 0;
    pObj->f0C = 0;
    pObj->f00 = 0;
    pObj->f04 = 0;
}

int BrObjGetF10(const BrObjHeader *pObj)
{
    return pObj->f10;
}

/* @n64 0x802607C0 located */
void BrObjInitInline(BrObjInline *pObj)
{
    pObj->f08 = 0;
    pObj->f0C = 0;
    pObj->f00 = 0;
    pObj->f04 = 0;
    pObj->pBuf = pObj->inline_;      /* +0x10 = this + 0x14 */
}

/* 0x10073D20 -- reads the counter BEFORE clearing the flag, so the increment
 * happens exactly once per set flag. */
void BrObjConsumeFlag(BrObjFlagCount *pObj)
{
    if (pObj->flag != 0) {
        int n = pObj->count;
        pObj->flag = 0;
        pObj->count = n + 1;
    }
}
