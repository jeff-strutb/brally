/* @implements 0x1003AA10 glide BrItemSetTotal_1003AA10
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetTotal_1003AA10@@YAHPAVObj3AA10@@@Z
 *
 * cdecl, one arg, `ret`, 238 B. Put a total into the owner\'s +0x2B5C item
 * label: in the simple mode a fixed catalogue string, otherwise the sum of
 * the four words of the selected 8-byte table row printed as decimal. If
 * the result is not the empty string, upper-case it into the label and
 * relayout (+0x08) / repaint (+0x2C). Returns 1, or 0 on the empty string
 * (the original falls out of the strlen with eax already zero).
 *
 * Sibling of 0x1003AB00 -- same buffer, same strlen gate, same
 * _strupr-into-the-label tail, same label-pointer-in-a-local null test.
 *
 * The word loads are `xor esi,esi / mov si,[eax]` INSIDE the loop, the
 * byte-slot widening idiom for an unsigned short read; the accumulator\'s
 * zero hoists all the way above the register saves next to the memset
 * constants.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>
#include <string.h>
#endif

class Item438E {
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

typedef char chk_nameE[(unsigned)&((Item438E *)0)->szName == 9 ? 1 : -1];

class Obj3AA10 {
public:
    char    pad000[0x2B5C];
    Item438E m2B5C;              /* +0x2B5C */
};

typedef char chk_itemE[(unsigned)&((Obj3AA10 *)0)->m2B5C == 0x2B5C ? 1 : -1];

extern "C" {
struct BrRow8 { unsigned short w[4]; };

int     g_brMode5BF4;           /* 0x10AC5BF4 */
char    g_szBr0ACA50[];         /* 0x100ACA50 */
BrRow8  g_brTbl5A66[];          /* 0x10AC5A66 */
char    g_brSel5C10;            /* 0x10AC5C10 */
_CRTIMP char *_strupr(char *s);
}

int BrItemSetTotal_1003AA10(Obj3AA10 *pObj)
{
    char  szNum[32];
    int   sum = 0;
    char *pLabel;

    memset(szNum, 0, sizeof(szNum));

    if (g_brMode5BF4 == 0) {
        strcpy(szNum, g_szBr0ACA50);
    } else {
        unsigned short *p = g_brTbl5A66[g_brSel5C10].w;
        int             i;

        for (i = 4; i != 0; i--) {
            sum += *p;
            p++;
        }

        _itoa(sum, szNum, 10);
    }

    if (strlen(szNum) == 0)
        return 0;

    pLabel = pObj->m2B5C.szName;
    strcpy(pLabel, _strupr(szNum));

    pObj->m2B5C.s2();
    if (pLabel != 0)
        pObj->m2B5C.s11();

    return 1;
}
