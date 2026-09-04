/* WHAT IT DOES: choose this item's caption from a table when a mode flag is
 * set, and a fixed fallback string when it is not. */
/* @implements 0x100391F0 glide BrUiText1003FCB0
 * @cpp_kind free
 * @cpp_symbol ?BrUiText1003FCB0@@YAHPAVObj391F0@@@Z
 *
 * cdecl, one arg, `ret`, 100 B. Put the catalogue string the +0x5D80
 * selector points at into the owner's +0x2B5C item label, relayout through
 * the +0x04 vcall and apply. Returns 1.
 *
 * Same 0x438 item record as 0x10041300. One of three identical siblings
 * (0x100391F0 / 0x10039350 / 0x10039510) differing only in the selector
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

class Item391F0 {
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

typedef char chk_name391F0[(unsigned)&((Item391F0 *)0)->szName == 9 ? 1 : -1];

class Obj391F0 {
public:
    char       pad000[0x2B5C];
    Item391F0 m2B5C;          /* +0x2B5C */
};

typedef char chk_item391F0[(unsigned)&((Obj391F0 *)0)->m2B5C == 0x2B5C ? 1 : -1];

extern "C" {
int  g_brFlag8EEED4;            /* 0x118EEED4 */
int  g_brSel5D74;               /* 0x10AC5D74 */
int  g_brTblABBA0[];            /* 0x100ABBA0 */

char *BrStrByIndex(int idx);                    /* 0x1006D280 */
void  BrItemApply_10038380(void *pObj, int a);  /* 0x10038380 */
}

int BrUiText1003FCB0(Obj391F0 *pObj)
{
    char *s;

    if (g_brFlag8EEED4 != 0)
        s = BrStrByIndex(g_brTblABBA0[g_brSel5D74]);
    else
        s = BrStrByIndex(0x74);

    strcpy(pObj->m2B5C.szName, s);

    pObj->m2B5C.s1();
    BrItemApply_10038380(pObj, 0);

    return 1;
}
