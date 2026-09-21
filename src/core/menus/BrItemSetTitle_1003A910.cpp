/* WHAT IT DOES: build the title line for the current stage -- its number,
 * then the stage's name from the string table, chosen by a switch over which
 * stage it is. */
/* @implements 0x1003A910 glide BrItemSetTitle_1003A910
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetTitle_1003A910@@YAHPAVObj3A910@@@Z
 *
 * cdecl, one arg, `ret`, 245 B. Build the phase title into the owner\'s
 * +0x2B5C item label: first render the 1-based phase number into the
 * shared text buffer, then pick the catalogue string for that phase and
 * concatenate the two with "%s%s", then copy the result into the label and
 * relayout (+0x04) / repaint (+0x10).
 *
 * Same 0x438 item record as 0x10041300, and the same label-pointer-in-a-
 * local null test before the repaint vcall.
 *
 * The catalogue index is a SWITCH -- the original tests it with a
 * `dec eax / je` chain, which an if/else-if chain of `cmp` does not
 * produce -- and the whole concatenating sprintf is written out in every
 * arm. VC5 then tail-merges: cases 2 and 3 converge on the sprintf call,
 * and the default converges all the way back onto case 1\'s
 * catalogue-lookup call, each arm keeping only its own `push imm`.
 * Hoisting the index into a variable and calling once instead collapses
 * that structure.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include <string.h>
#endif

class Item438F {
public:
    virtual void  s0();
    virtual void  s1();         /* +0x04 relayout */
    virtual void  s2();         /* +0x08 */
    virtual void  s3();
    virtual void  s4();         /* +0x10 repaint */
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

typedef char chk_nameF[(unsigned)&((Item438F *)0)->szName == 9 ? 1 : -1];

class Obj3A910 {
public:
    char    pad000[0x2B5C];
    Item438F m2B5C;              /* +0x2B5C */
};

typedef char chk_itemF[(unsigned)&((Obj3A910 *)0)->m2B5C == 0x2B5C ? 1 : -1];

extern "C" {
int  g_brPhase5BF8;             /* 0x10AC5BF8 */
char g_szBrText5870[];          /* 0x10AC5870 -- shared text buffer */
char g_szBrFmt6B84[];           /* 0x100A6B84 */
char g_szBrFmtSS[];             /* 0x100AA340 -- "%s%s" */
char *BrStrByIndex(int idx);    /* 0x1006D280 */
}

int BrItemSetTitle_1003A910(Obj3A910 *pObj)
{
    char  szTmp[128];
    char *pLabel;

    sprintf(g_szBrText5870, g_szBrFmt6B84, g_brPhase5BF8 + 1);

    switch (g_brPhase5BF8 + 1) {
    case 1:
        sprintf(szTmp, g_szBrFmtSS, g_szBrText5870, BrStrByIndex(0xB3));
        break;
    case 2:
        sprintf(szTmp, g_szBrFmtSS, g_szBrText5870, BrStrByIndex(0xB4));
        break;
    case 3:
        sprintf(szTmp, g_szBrFmtSS, g_szBrText5870, BrStrByIndex(0xB5));
        break;
    default:
        sprintf(szTmp, g_szBrFmtSS, g_szBrText5870, BrStrByIndex(0xB6));
        break;
    }

    pLabel = pObj->m2B5C.szName;
    strcpy(pLabel, szTmp);

    pObj->m2B5C.s1();
    if (pLabel != 0)
        pObj->m2B5C.s4();

    return 1;
}
