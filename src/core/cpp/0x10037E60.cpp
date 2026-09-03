/* @implements 0x10037E60 glide BrItemSetLabelState_10037E60
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetLabelState_10037E60@@YAHPAVObj37E60@@@Z
 *
 * cdecl, one arg, `ret`, 100 B. Put one of two catalogue strings into the
 * owner\'s +0x2B5C item label -- the 0x51 string only when both mode flags
 * are clear, the 0x0C one otherwise -- then relayout through the +0x04
 * vcall. Returns 1.
 *
 * Same 0x438 item record as 0x10041300; the string comes from the same
 * index-to-string helper 0x10059350 uses for its error lines, and the
 * strcpy is the inline scan + rep movsd form because the helper\'s result
 * is an opaque char*.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

class Item438C {
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

typedef char chk_nameC[(unsigned)&((Item438C *)0)->szName == 9 ? 1 : -1];

class Obj37E60 {
public:
    char    pad000[0x2B5C];
    Item438C m2B5C;              /* +0x2B5C */
};

typedef char chk_itemC[(unsigned)&((Obj37E60 *)0)->m2B5C == 0x2B5C ? 1 : -1];

extern "C" {
int   g_brFlag0A9360;           /* 0x100A9360 */
int   g_brFlag21C650;           /* 0x1021C650 */
char *BrStrByIndex(int idx);    /* 0x1006D280 */
}

int BrItemSetLabelState_10037E60(Obj37E60 *pObj)
{
    char *pStr;

    if (g_brFlag0A9360 == 0 && g_brFlag21C650 == 0)
        pStr = BrStrByIndex(0x51);
    else
        pStr = BrStrByIndex(0x0C);

    strcpy(pObj->m2B5C.szName, pStr);

    pObj->m2B5C.s1();

    return 1;
}
