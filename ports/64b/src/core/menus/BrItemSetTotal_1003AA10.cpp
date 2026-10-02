/* WHAT IT DOES: work out the total for the selected entry and put it in this
 * item's label; shows a fixed placeholder string when the mode has no total
 * to show. */
/* @t3 0x1003AA10 2026-09-13 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 238/238 insns 92/92 rows 0+0 regions 4 oracle UNCLASSIFIED
 * @t3-effort passes 3 zero-movement 2 3
 * Residue: register plan -- `mov ecx,4` scheduled after the table lea and the buffer lea/push pair in eax/ecx instead of ecx/edx (rows 0+0 after regnorm, 4 masked regions). Dead list = the three ledger passes (31 cpp probes: loop shape, declaration order, slot-census spellings).
 * Do not reopen before the end-grind. */
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
 *
 * @t4-pass 0x1003AA10 1 2026-09-13 probes 10 bytes 238 insns 92 regions 4 rows 0 census no  (cpp harness: sum/pointer walk: *p++, sum after memset, src local, pLabel init, table cast, i > 0, literal 32, buffer after sum, p at function scope, &w[0])
 * @t4-pass 0x1003AA10 2 2026-09-13 probes 11 bytes 238 insns 92 regions 4 rows 0 census yes  (cpp harness: slot census (buffer lea x5): /Op /Oy- /Os /Ot, !strlen, strupr split, field null test, arm swap, unsigned sum, ret local, int selector)
 * @t4-pass 0x1003AA10 3 2026-09-13 probes 10 bytes 238 insns 92 regions 4 rows 0 census no  (cpp harness: loop shape + declaration order: i before p, index form, row struct, do-while, --n, count-up pointer, both at function scope (two orders), init order, end-pointer while)
 */
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>
#include <string.h>

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



class Obj3AA10 {
public:
    char    pad000[0x2B5C];
    Item438E m2B5C;              /* +0x2B5C */
};



extern "C" {
struct BrRow8 { unsigned short w[4]; };

/* 64-bit core: g_brMode5BF4 is defined once, in br_globals.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: g_brSel5C10 is defined once, in br_globals.c */
_CRTIMP char *_strupr(char *s);
}

int BrItemSetTotal_1003AA10(Obj3AA10 *pObj)
{
    char  szNum[32];
    int   sum = 0;
    char *pLabel;

    memset(szNum, 0, sizeof(szNum));

    if (g_5BF4 == 0) {
        strcpy(szNum, g_szBr0ACA50);
    } else {
        unsigned short *p = (*(BrRow8 (*)[])&g_aBrAA270E)[(*(char *)&DAT_10ac5c10)].w;
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

    (BR_VFN(&(pObj->m2B5C), 2, void (*)(void *)))(&(pObj->m2B5C));
    if (pLabel != 0)
        (BR_VFN(&(pObj->m2B5C), 11, void (*)(void *)))(&(pObj->m2B5C));

    return 1;
}
