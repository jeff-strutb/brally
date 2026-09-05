/* Matching TU for 0x10013E80 -- one-time reset of the race-begin bookkeeping.
 *
 * RESIDUE (T3a, 4 bytes): size-exact 128/128, 30/30 instructions, REGNORM 0+0
 * -- the two streams are the SAME instruction multiset and differ only in
 * WHERE ONE STORE SITS.  The original puts the memset's fifth store AFTER the
 * loop's pointer init:
 *      59  mov [0x104AB4F0], ecx
 *      5f  mov eax, 0x10396F48        <- loop IV init
 *      64  mov [0x10396F20], esi      <- memset store #5, filling the AGI slot
 *      6a  mov [eax], ecx             <- loop body
 * and every spelling tried emits store #5 at 0x4d instead, leaving `mov eax`
 * back-to-back with `mov [eax]`.
 *
 * ‼ WHAT DID MOVE THE NEEDLE (keep it): `g_brRaceBeginLimitOn = 1;` must be
 * the FIRST statement in the block.  That is what forces `mov edx,1` into the
 * prologue, which in turn denies edx to the memset's zero temp and buys the
 * `push esi` / `pop esi` pair the original has.  Without it the function is
 * 121-126 bytes and REGNORM 0+2 (missing push/pop).
 *
 * DEAD PROBES for the remaining 4 bytes -- do not re-run (all plateau at 4,
 * or regress):
 *   - memset at every one of the six source positions among the five -1
 *     stores, crossed with limitOn first / second / last (18 builds): only
 *     limitOn-first survives, and all survivors read 4.
 *   - zero chain as three separate statements; chain written 6F24-last;
 *     chain sourced from g_aBrRbPerCar[0]; chain after the loop.
 *   - loop spellings: do/while on the index, do/while on a pointer, while on
 *     a pointer, `for (p = ...; p != end; p++)`, hoisted `p =` before the
 *     chain, separate counter + pointer, `(float)0` vs `0.0f`, int field
 *     instead of float.  Pointer forms cost a REGNORM row; the rest read 4.
 *   - array split 4+1 with 0x10396F20 as its own global, stored before the
 *     chain, after the chain, or via a second one-word memset: the store
 *     lands in the right REGION but takes ecx, not the memset's esi (8).
 *   - memset(...,20) vs sizeof; six-element array absorbing 0x10396F24.
 *   - guard as `!g_brRbInited`; guard against a const 0.
 *   - COMPILER FLAGS ARE NOT THE LEVER: /G5, /O1, /Ox, /Oxs, /O2 /Op,
 *     /O2 /Oy-, /O2 /Gr and /Og /Oi /Ot /Oy /Ob1 /Gs all emit store #5 at
 *     0x4d (the two /O1-family builds also lose the whole shape).
 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

extern int   g_brRbInited;          /* 0x104AB504 */
extern int   g_aBrRbPerCar[5];      /* 0x10396F10 .. 0x10396F20 -- one per car */
extern int   g_brRb6F24;            /* 0x10396F24 */
extern int   g_brRb6F44;            /* 0x10396F44 */
extern int   g_brRbB4E8;            /* 0x104AB4E8 */
extern int   g_brRbB4EC;            /* 0x104AB4EC */
extern int   g_brRbB4F0;            /* 0x104AB4F0 */
extern int   g_brRbB4F4;            /* 0x104AB4F4 */
extern int   g_brRbB4FC;            /* 0x104AB4FC */
extern int   g_brRbB500;            /* 0x104AB500 */
extern int   g_brRaceBeginLimitOn;  /* 0x100A5EA8 */

/* Five car records of 0x2E0F0 bytes starting at 0x10396F48; the loop
 * bound 0x1047D3F8 is the end of the fifth. */
typedef struct {
    float f0;
    char  pad[0x2E0F0 - 4];
} BrRbCar;
extern BrRbCar g_aBrRbCar[5];       /* 0x10396F48 */

/* WHAT IT DOES: the first-time reset of the race-begin state -- the per-car
 * int array is cleared (an inlined memset: five stores from one zeroed
 * register), the position/lap slots go to -1, the limit flag comes on, three
 * float accumulators go to 0 and each car's first field is cleared.  Guarded
 * by a done flag so it only ever runs once. */
/* @implements 0x10013E80 glide BrRaceBeginResetOnce */
void BrRaceBeginResetOnce(void)
{
    int i;

    if (g_brRbInited == 0) {
        g_brRaceBeginLimitOn = 1;
        g_brRbB4FC = -1;
        memset(g_aBrRbPerCar, 0, sizeof(g_aBrRbPerCar));
        g_brRb6F44 = -1;
        g_brRbB4E8 = -1;
        g_brRbB4EC = -1;
        g_brRbB500 = -1;
        g_brRbB4F0 = g_brRbB4F4 = g_brRb6F24 = 0;
        for (i = 0; i < 5; i++)
            g_aBrRbCar[i].f0 = 0.0f;
        g_brRbInited = 1;
    }
}

#endif /* BR_MATCHING_BUILD */
