/* WHAT IT DOES: put the stage's stored lap time from a second table onto this
 * row (minutes:seconds.hundredths), or dashes when times are not available
 * yet or the time is not above zero. */
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
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include <string.h>
#endif

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
int           g_brTimesAvail5BF4;   /* 0x10AC5BF4 */
signed char   g_brStage5C10;        /* 0x10AC5C10 */
int           g_brStageBias5C04;    /* 0x10AC5C04 */
unsigned char g_brStageByte3028[];  /* 0x100B3028 */
float g_brFTbl5AF8[];               /* 0x10AC5AF8 */
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

    if (g_brTimesAvail5BF4 == 0) goto dash;
    t = g_brFTbl5AF8[g_brStageByte3028[(g_brStageBias5C04 + 12 * g_brStage5C10) * 2]];
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
