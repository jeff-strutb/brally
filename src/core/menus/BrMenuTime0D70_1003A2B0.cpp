/* WHAT IT DOES: put the stage's stored lap time from a second table onto this
 * row (minutes:seconds.hundredths), or dashes when times are not available
 * yet or the time is not above zero. */
/* @t3 0x1003A2B0 2026-09-26 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 360/360 insns 120/120 rows 0+0 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue: register colouring of the stage-byte lookup only (bias, stage*3
 * and index rotate across eax/ecx/edx); dossier and dead list in the header
 * below.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1003A2B0 glide BrMenuTime0D70_1003A2B0
 * @cpp_kind free
 * @cpp_symbol ?BrMenuTime0D70_1003A2B0@@YAHPAVObj3A2B0@@@Z
 *
 * cdecl, one arg, `ret`, 360 B. Compiled as C++, like its neighbour
 * 0x1003A420 (BrItemSetSelTime_1003A420), whose float-chain lever it
 * reuses: one named float local per intermediate, and the constants read
 * from their .rdata cells.  The C spelling in br_menucb.c scores 249.
 * The flag test jumps straight into the dashes block through a label; that
 * gives the original's layout (dashes first, formatter after, with the
 * compared time left on the x87 stack and popped on the dashes path).
 *
 * RESIDUE (8 B): register colouring of the stage-byte lookup only -- the
 * original puts the bias in edx, stage*3 in ecx and the index in eax; ours
 * rotates them.  BrMenuCap07E0 (0x10039D20) carries the same rotation.
 * Probed and inert: an inline helper (4 shapes), named e/k locals, 2-D
 * array and struct views of the table, extern order (all 120), 50..4000
 * extra symbols, <windows.h>.
 * @t4-pass 0x1003A2B0 1 2026-09-26 probes 12 bytes 360 insns 120 regions 1 rows 0 census no  (inline-helper shapes, named e/k locals, 2-D/struct table views)
 * @t4-pass 0x1003A2B0 2 2026-09-26 probes 127 bytes 360 insns 120 regions 1 rows 0 census yes  (mechanism: all 120 extern orders, 50..4000 extra symbols, <windows.h>)
 */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include <string.h>

class Item438K {
public:
    virtual void  s0();
    virtual void  s1();
    virtual void  s2();
    virtual void  s3();
    virtual void  s4();
    int   f004;
    char  b008;
    char  szName[0x401];
};

class Obj3A2B0 {
public:
    char    pad000[0x2B5C];
    Item438K m2B5C;
};

extern "C" {
int           DAT_10ac5bf4;   /* 0x10AC5BF4 */
signed char   DAT_10ac5c10;        /* 0x10AC5C10 */
int           DAT_10ac5c04;    /* 0x10AC5C04 */
unsigned char DAT_100b3028[];  /* 0x100B3028 */
float DAT_10ac5af8[];               /* 0x10AC5AF8 */
float g_f077624;
float g_f077630;
float g_f077634;
float g_f077638;
float g_f07763C;
char  g_szBrDashes[];
char  g_szBrFmtTime[];
_CRTIMP char *_strupr(char *s);
}

int BrMenuTime0D70_1003A2B0(Obj3A2B0 *pObj)
{
    char  szTime[32];
    float t;
    char *pLabel;

    memset(szTime, 0, sizeof(szTime));

    if (DAT_10ac5bf4 == 0) goto dash;
    t = DAT_10ac5af8[DAT_100b3028[(DAT_10ac5c04 + 12 * DAT_10ac5c10) * 2]];
    if (t <= g_f077624) {
 dash:
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
