/* @implements 0x1003A420 glide BrItemSetSelTime_1003A420
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetSelTime_1003A420@@YAHPAVObj3A420@@@Z
 *
 * cdecl, one arg, `ret`, 341 B. The selected-slot variant of 0x1003A580:
 * the time is the live one when the selector is 3 and a table entry
 * otherwise. Format a time into the owner\'s
 * +0x2B5C item label: a sentinel string when the time is not above the
 * floor, otherwise minutes / seconds / hundredths pulled apart with the
 * usual scale-and-truncate chain and printed. Upper-case the result into
 * the label and relayout (+0x04) / repaint (+0x10).
 *
 * Same 0x438 item record as 0x10041300, the same strlen gate and
 * _strupr-into-the-label tail as 0x1003AA10, and the same
 * label-pointer-in-a-local null test.
 *
 * Every `(int)` of a float is a `call __ftol` and every int fed back into
 * the float chain is `mov [temp],eax / fild [temp]`, so the conversions
 * are the shape of the whole function.
 *
 * THE LEVER: one NAMED float local per intermediate. Writing the chain as
 * five int locals with the conversions inline costs 205 diffs; naming the
 * two int-to-float conversions (fa, fb) takes it to 155; naming the
 * PRODUCT fb * K30 as well takes it to 0. Each named local is what makes
 * VC5 treat the value as one definition with several uses, which is what
 * produces the original's stack discipline -- `fld st(0)` to duplicate fa,
 * `fst` (no pop) to home fb because three later statements read it, and
 * `fsubr st(1)` against the copy still on the stack. Unnamed, VC5
 * re-associates and starts spilling to slots the original never uses.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include <string.h>
#endif

class Item438J {
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

typedef char chk_nameJ[(unsigned)&((Item438J *)0)->szName == 9 ? 1 : -1];

class Obj3A420 {
public:
    char    pad000[0x2B5C];
    Item438J m2B5C;              /* +0x2B5C */
};

typedef char chk_itemJ[(unsigned)&((Obj3A420 *)0)->m2B5C == 0x2B5C ? 1 : -1];

extern "C" {
int   g_brSel5C28;              /* 0x10AC5C28 */
float g_brTime5C20;             /* 0x10AC5C20 */
float g_brFTbl58F8[];           /* 0x10AC58F8 */
float g_f077624;                /* 0x10077624 */
float g_f077630;                /* 0x10077630 */
float g_f077634;                /* 0x10077634 */
float g_f077638;                /* 0x10077638 */
float g_f07763C;                /* 0x1007763C */
char  g_szBrDashes[];           /* 0x100ACAE0 -- "--:--" */
char  g_szBrFmtTime[];          /* 0x1007B064 */
_CRTIMP char *_strupr(char *s);
}

int BrItemSetSelTime_1003A420(Obj3A420 *pObj)
{
    char  szTime[32];
    float t;
    char *pLabel;

    memset(szTime, 0, sizeof(szTime));

    if (g_brSel5C28 == 3)
        t = g_brTime5C20;
    else
        t = g_brFTbl58F8[g_brSel5C28];

    if (t <= g_f077624) {
        strcpy(szTime, g_szBrDashes);
    } else {
        int   a  = (int)(t * g_f077630);
        float fa = (float)a;
        int   b  = (int)(fa * g_f077634);
        float fb = (float)b;
        float fc = fb * g_f077630;
        int   c  = (int)(fa - fc);
        int   d  = (int)(fb * g_f077638);
        int   e  = (int)(fb - d * g_f07763C);

        sprintf(szTime, g_szBrFmtTime, d, e, c);
    }

    if (strlen(szTime) == 0)
        return 0;

    pLabel = pObj->m2B5C.szName;
    strcpy(pLabel, _strupr(szTime));

    pObj->m2B5C.s1();
    if (pLabel != 0)
        pObj->m2B5C.s4();

    return 1;
}
