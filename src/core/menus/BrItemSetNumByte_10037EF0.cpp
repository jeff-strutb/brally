/* WHAT IT DOES: set a menu item's label to a number taken from the currently
 * selected row, then re-lay and refresh it. */
/* @implements 0x10037EF0 glide BrItemSetNumByte_10037EF0
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetNumByte_10037EF0@@YAHPAVObj37EF0@@@Z
 *
 * cdecl, one arg, `ret`, 73 B. Print the selected byte-valued setting as
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
 * Twin of 0x100380B0, which differs only in the setting it reads (a word
 * table indexed by the same selector instead of a byte table).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>
#endif

class Item438 {
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

typedef char chk_name[(unsigned)&((Item438 *)0)->szName == 9 ? 1 : -1];

class Obj37EF0 {
public:
    char    pad000[0x2B5C];
    Item438 m2B5C;              /* +0x2B5C */
};

typedef char chk_item[(unsigned)&((Obj37EF0 *)0)->m2B5C == 0x2B5C ? 1 : -1];

extern "C" {
int  g_brSel5C04;               /* 0x10AC5C04 */
char g_brVal5A40[];             /* 0x10AC5A40 */
}

int BrItemSetNumByte_10037EF0(Obj37EF0 *pObj)
{
    char *s = pObj->m2B5C.szName;

    _itoa(g_brVal5A40[g_brSel5C04], s, 10);

    pObj->m2B5C.s2();
    if (s != 0)
        pObj->m2B5C.s11();

    return 1;
}
