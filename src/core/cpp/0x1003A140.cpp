/* WHAT IT DOES: format a stored split time into this item's label, showing
 * dashes instead when no time has been set yet. */
/* @implements 0x1003A140 glide BrItemSetSplitTime_1003A140
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetSplitTime_1003A140@@YAHPAVObj3A140@@@Z
 *
 * cdecl, one arg, `ret`, 360 B. The split-time variant of 0x1003A580:
 * same formatter, but the time comes from a float table indexed through a
 * byte map, and the whole thing is skipped (sentinel string) when the mode
 * flag is clear. VC5 merges the two sentinel arms.
 *
 * Same 0x438 item record as 0x10041300, same strlen gate,
 * _strupr-into-the-label tail and label-pointer null test.
 *
 * PARKED at 237 diffs. The formatter, the tail, the memset and the mode
 * test are all right; what is left is BLOCK PLACEMENT plus the register
 * rotation it drags along. The original puts the sentinel block first,
 * then the format block, then the common tail, so both branches are short
 * jumps. Ours puts the format block AFTER the tail, which turns the inner
 * `je` near (+4 bytes) and rotates the index computation's registers
 * (orig keeps the selector in eax and scales into ecx; ours does the
 * reverse and needs edx).
 * DO NOT RE-PROBE -- arm orders all measured: inner sentinel-then /
 * format-else with outer format-first is this 237; inner format-then is
 * 254 (outer je goes near too); outer sentinel-then duplicates the
 * sentinel instead of merging it (416 B, 241); and folding the mode test
 * into a short-circuit `||` moves the sentinel to the end (252).
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

class Item438I {
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

typedef char chk_nameI[(unsigned)&((Item438I *)0)->szName == 9 ? 1 : -1];

class Obj3A140 {
public:
    char    pad000[0x2B5C];
    Item438I m2B5C;              /* +0x2B5C */
};

typedef char chk_itemI[(unsigned)&((Obj3A140 *)0)->m2B5C == 0x2B5C ? 1 : -1];

extern "C" {
int           g_brMode5BF4;     /* 0x10AC5BF4 */
int           g_brIdx5C04;      /* 0x10AC5C04 */
char          g_brSel5C10;      /* 0x10AC5C10 */
unsigned char g_brMap3028[];    /* 0x100B3028 */
float         g_brFTbl5B54[];   /* 0x10AC5B54 */
float g_f077624;                /* 0x10077624 */
float g_f077630;                /* 0x10077630 */
float g_f077634;                /* 0x10077634 */
float g_f077638;                /* 0x10077638 */
float g_f07763C;                /* 0x1007763C */
char  g_szBrDashes[];           /* 0x100ACAE0 -- "--:--" */
char  g_szBrFmtTime[];          /* 0x1007B064 */
_CRTIMP char *_strupr(char *s);
}

int BrItemSetSplitTime_1003A140(Obj3A140 *pObj)
{
    char  szTime[32];
    float t;
    char *pLabel;

    memset(szTime, 0, sizeof(szTime));

    if (g_brMode5BF4 != 0) {
        t = g_brFTbl5B54[
                g_brMap3028[(g_brIdx5C04 + g_brSel5C10 * 12) * 2]];

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
    } else {
        strcpy(szTime, g_szBrDashes);
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
