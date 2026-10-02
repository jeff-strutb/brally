/* WHAT IT DOES: set a menu item's label to a number from a table of 16-bit
 * values, indexed by the currently selected row, then re-lay and refresh it. */
/* @implements 0x100380B0 glide BrItemSetNumWord_100380B0
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetNumWord_100380B0@@YAHPAVObj380B0@@@Z
 *
 * cdecl, one arg, `ret`, 74 B. Print the selected word-valued setting as
 * decimal into the owner's +0x2B5C item label, then let the item relayout
 * (+0x08 vcall) and repaint (+0x2C vcall). Returns 1.
 *
 * Same 0x438 item record as 0x10041300 -- vtable at +0, label at +9. The
 * two vcalls share one cached vptr load, and the item's address is formed
 * once with `add esi, 0x2B5C` because the label lea already consumed the
 * owner pointer.
 *
 * The label pointer is a real local: the original keeps it in edi across
 * the _itoa call and TESTS it before the repaint vcall, so the source
 * holds `&item.szName[0]` in a variable and null-checks it (VC5 does not
 * fold the test away).
 *
 * Twin of 0x10037EF0, which differs only in the setting it reads (a byte
 * table indexed by the same selector instead of a word table).
 */
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>

class Item438B {
public:
    virtual void  s0();
    virtual void  s1();         /* +0x04 */
    virtual void  s2();         /* +0x08 relayout */
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6();
    virtual void  s7();
    virtual void  s8();
    virtual void  s9();
    virtual float s10();        /* +0x28 */
    virtual void  s11();        /* +0x2C repaint */

    int   f004;                 /* +0x004 */
    char  b008;                 /* +0x008 */
    char  szName[0x401];        /* +0x009 */
};



class Obj380B0 {
public:
    char    pad000[0x2B5C];
    Item438B m2B5C;              /* +0x2B5C */
};



extern "C" {
/* 64-bit core: g_brSel5C04 is defined once, in br_globals.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
}

int BrItemSetNumWord_100380B0(Obj380B0 *pObj)
{
    char *s = pObj->m2B5C.szName;

    _itoa(g_brVal40F8[g_brIdx5C04], s, 10);

    (BR_VFN(&(pObj->m2B5C), 2, void (*)(void *)))(&(pObj->m2B5C));
    if (s != 0)
        (BR_VFN(&(pObj->m2B5C), 11, void (*)(void *)))(&(pObj->m2B5C));

    return 1;
}
