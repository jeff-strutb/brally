/* WHAT IT DOES: format the current lap time into this item's label, showing
 * dashes when no lap has been completed. */
/* @implements 0x1003A580 glide BrItemSetLapTime_1003A580
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetLapTime_1003A580@@YAHPAVObj3A580@@@Z
 *
 * cdecl, one arg, `ret`, 322 B. Format a lap time into the owner\'s
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
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
#include <string.h>

class Item438G {
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



class Obj3A580 {
public:
    char    pad000[0x2B5C];
    Item438G m2B5C;              /* +0x2B5C */
};



extern "C" {
/* 64-bit core: g_brTime5C20 is defined once, in br_globals.c */
/* 64-bit core: g_f077624 is defined once, in br_globals.c */
/* 64-bit core: g_f077630 is defined once, in br_globals.c */
/* 64-bit core: g_f077634 is defined once, in br_globals.c */
/* 64-bit core: g_f077638 is defined once, in br_globals.c */
/* 64-bit core: g_f07763C is defined once, in br_globals.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
_CRTIMP char *_strupr(char *s);
}

int BrItemSetLapTime_1003A580(Obj3A580 *pObj)
{
    char  szTime[32];
    float t = g_brTime5C20;
    char *pLabel;

    memset(szTime, 0, sizeof(szTime));

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

    pLabel = (*(class Item438G *)&((BrUiCtl_ *)(pObj))->aText[0]).szName;
    strcpy(pLabel, _strupr(szTime));

    (BR_VFN(&((*(class Item438G *)&((BrUiCtl_ *)(pObj))->aText[0])), 1, void (*)(void *)))(&((*(class Item438G *)&((BrUiCtl_ *)(pObj))->aText[0])));
    if (pLabel != 0)
        (BR_VFN(&((*(class Item438G *)&((BrUiCtl_ *)(pObj))->aText[0])), 4, void (*)(void *)))(&((*(class Item438G *)&((BrUiCtl_ *)(pObj))->aText[0])));

    return 1;
}
