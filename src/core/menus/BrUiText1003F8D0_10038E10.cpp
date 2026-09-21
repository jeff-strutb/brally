/* WHAT IT DOES: set this item's caption and its highlight state together: a
 * flagged entry gets a different string and a different colour, so an
 * unavailable option reads as unavailable. */
/* @implements 0x10038E10 glide BrUiText1003F8D0
 * @cpp_kind free
 * @cpp_symbol ?BrUiText1003F8D0@@YAHPAVObj38E10@@@Z
 *
 * cdecl, one arg, `ret`, 100 B. Put the catalogue string the +0x5D80
 * selector points at into the owner's +0x2B5C item label, relayout through
 * the +0x04 vcall and apply. Returns 1.
 *
 * Same 0x438 item record as 0x10041300. One of three identical siblings
 * (0x10038E10 / 0x10039350 / 0x10039510) differing only in the selector
 * global and the index table.
 *
 * The port body in slice2_23.c reaches its globals through a BrUiGlobals*
 * second parameter; the original is cdecl with ONE argument and direct
 * global addresses. Same split as the 0x10038650 sibling.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

class Item38E10 {
public:
    virtual void  s0();
    virtual void  s1();         /* +0x04 relayout */
    virtual void  s2();
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6();
    virtual void  s7();
    virtual void  s8();
    virtual void  s9();
    virtual float s10();
    virtual void  s11();

    int   f004;
    char  b008;
    char  szName[0x401];        /* +0x009 */
};

typedef char chk_name38E10[(unsigned)&((Item38E10 *)0)->szName == 9 ? 1 : -1];

class Obj38E10 {
public:
    char       pad000[0x2B5C];
    Item38E10 m2B5C;          /* +0x2B5C */
};

typedef char chk_item38E10[(unsigned)&((Obj38E10 *)0)->m2B5C == 0x2B5C ? 1 : -1];

extern "C" {
int  g_brFlag5BA8;              /* 0x10AC5BA8 */
int  g_brSel5B98;               /* 0x10AC5B98 */
int  g_brTbl4648[];             /* 0x10AC4648 */
char g_szBr0ACAD8[];            /* 0x100ACAD8 */

char *BrStrByIndex(int idx);                    /* 0x1006D280 */
void  BrItemApply_10038380(void *pObj, int a);  /* 0x10038380 */
}

int BrUiText1003F8D0(Obj38E10 *pObj)
{
    if (g_brFlag5BA8 != 0) {
        strcpy(pObj->m2B5C.szName, BrStrByIndex(0xAF));

        if (g_brTbl4648[g_brSel5B98] != 0)
            pObj->m2B5C.b008 = 4;
        else
            pObj->m2B5C.b008 = 1;
    } else {
        strcpy(pObj->m2B5C.szName, g_szBr0ACAD8);
    }

    pObj->m2B5C.s1();
    BrItemApply_10038380(pObj, 0);

    return 1;
}
