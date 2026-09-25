/* slice1_05.c -- Boss Rally (BRD3D.dll), a later pass, 0x1002B280..0x100360F0.
 * See slice1_05.h for the per-function notes and gotchas. */

#ifdef BR_MATCHING_BUILD
/* The originals of the vtx-cache cluster take no BrVtxCache parameter --
 * state is loose globals -- and BrVtxExpand/Insert/Resolve have different
 * arities. Hide the header's port prototypes behind renames so the
 * matching twins can define the real symbols with the original
 * signatures; other TUs keep calling with the port signatures (cdecl, so
 * the extra leading argument is harmless at run time). */
#define BrVtxExpand       BrVtxExpand_hdr
#define BrVtxCacheInsert  BrVtxCacheInsert_hdr
#define BrVtxCacheResolve BrVtxCacheResolve_hdr
#define BrSelLookup       BrSelLookup_hdr
#define BrPtrListAdd      BrPtrListAdd_hdr
#define BrF3DVtxFixup     BrF3DVtxFixup_hdr
#include "slice1_05.h"
#include "br_gamestep.h"
#undef BrVtxExpand
#undef BrVtxCacheInsert
#undef BrVtxCacheResolve
#undef BrSelLookup
#undef BrPtrListAdd
#undef BrF3DVtxFixup
#else
#include "slice1_05.h"
#include "br_gamestep.h"   /* 0x10034C66/0x10034C73 == BRGlide 0x1002E317/0x1002E324 */
#endif

#include <stddef.h>

/* ================================================================== */
/* 4. 4x4 matrix helpers                                              */
/* ================================================================== */

/* 0x100306C0 */
/* WHAT IT DOES: multiplies two 4x4 transforms together, which is how the game
 * combines a rotation with a position, or an object's placing with the camera.
 * If the answer is being written back over one of the inputs it works through a
 * scratch copy -- and, oddly, adds the four products up in a different order on
 * that path, so the two routes can disagree in the last bit or two. */
/* @implements 0x100306C0 d3d BrMat4Mul */
void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut)
{
    BrMat4 tmp;
    int i, j;

    if (pA == NULL || pB == NULL)
        return;
    /* pOut is deliberately NOT checked -- see the header. */

    /* TWO separate loop nests, not one nest with a flag: the original
     * branches once (both compares jump into the scratch path, the direct
     * path is the fallthrough) and each path carries its own rolled 4x4
     * loops with a different summation order. */
    /* Each element is ONE expression (a named `s` accumulator costs
     * fadd-without-pop + a discard at the loop tail).  The written pair
     * order is REVERSED from the evaluated one, and the two nests' pair
     * spellings are COUPLED through the optimizer -- all four combinations
     * measured; this one is the minimum.
     * RESIDUE (2+2 regnorm, 24 masked B, T3a): hoisted-operand-load order
     * inside the aliased nest (which b-row load is hoisted first) --
     * identical op counts, operand-source only; the playbook's documented
     * scheduling wall class. */
    if (pA != pOut && pB != pOut) {
        for (i = 0; i < 4; ++i) {
            for (j = 0; j < 4; ++j) {
                /* 0x100306FD evaluates ((a2*b2 + a3*b3) + a0*b0) + a1*b1 */
                pOut->m[i][j] = (pA->m[i][3] * pB->m[3][j]
                                 + pA->m[i][2] * pB->m[2][j]
                                 + pA->m[i][0] * pB->m[0][j])
                                + pA->m[i][1] * pB->m[1][j];
            }
        }
        return;
    }

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            /* 0x10030753 evaluates ((a3*b3 + a1*b1) + a0*b0) + a2*b2 */
            /* DEAD 2026-09-13: `a2b2 + (a1b1 + a3b3 + a0b0)` and
             * `(a0b0 + (a1b1 + a3b3)) + a2b2` (both value-identical) --
             * 24 and 33 diffs, the aliased nest keeps its row-3 anchor. */
            tmp.m[i][j] = (pA->m[i][1] * pB->m[1][j]
                           + pA->m[i][3] * pB->m[3][j]
                           + pA->m[i][0] * pB->m[0][j])
                          + pA->m[i][2] * pB->m[2][j];
        }
    }
    *pOut = tmp;                /* `rep movsd` of 16 dwords in the original */
}

/* 0x10031140 */
void BrMat4Translate(BrMat4 *pM, float tx, float ty, float tz)
{
    pM->m[0][0] = 1.0f; pM->m[0][1] = 0.0f; pM->m[0][2] = 0.0f; pM->m[0][3] = 0.0f;
    pM->m[1][0] = 0.0f; pM->m[1][1] = 1.0f; pM->m[1][2] = 0.0f; pM->m[1][3] = 0.0f;
    pM->m[2][0] = 0.0f; pM->m[2][1] = 0.0f; pM->m[2][2] = 1.0f; pM->m[2][3] = 0.0f;
    pM->m[3][0] = tx;   pM->m[3][1] = ty;   pM->m[3][2] = tz;   pM->m[3][3] = 1.0f;
}

/* ================================================================== */
/* 5. Assorted setters, lists and lookups                             */
/* ================================================================== */

/* 0x10034C32 */
void BrHookNopA(void)
{
}

/* 0x10034C37 */
void BrHookSetA(BrHooks *pH, void *pv)
{
    pH->pfA = pv;
}

/* 0x10034C44 */
void BrHookSetB(BrHooks *pH, void *pv)
{
    pH->pfB = pv;
}

/* 0x10034C66 -- ONE BODY, br_gamestep.c's (BrGameStepSet), which carries
 * BRGlide's 0x1002E317 for it.
 *
 * `pH` IS NOT USED, AND WAS NEVER USED BY THE ORIGINAL.  0x10034C66 is
 *     push ebp / mov ebp,esp / mov eax,[ebp+8] / mov [0x106C0964],eax / ret
 * -- one cdecl argument written to a fixed global.  There is no `this`: the
 * BrHooks struct is a port-side gathering of six unrelated globals, and a
 * note elsewhere in the tree explaining this pair as __thiscall with the
 * `this` dropped was reading a struct that does not exist in the game.  The
 * parameter is kept only so the existing call sites need no change. */
void BrHookSetC(BrHooks *pH, void (*pfn)(void))
{
    (void)pH;
    BrGameStepSet(pfn);
}

/* 0x10034C73 -- ONE BODY, br_gamestep.c's (BrGameStepInvoke), which carries
 * BRGlide's 0x1002E324 for it.  `pH` is unused for the same reason as the
 * setter above: the original is `call dword ptr [0x106C0964]` and takes no
 * argument at all.
 *
 * DEVIATION, inherited from the surviving body and stated here because this
 * declaration used to promise the opposite: br_gamestep.c tests the slot for
 * NULL and returns 0, where the original calls through it and faults.  The
 * host harness needs to be able to report "nothing installed" rather than
 * die; the fault is the only behaviour lost. */
/* WHAT IT DOES: runs one frame of whatever the game is currently doing --
 * the race, or the front end -- by calling the routine installed in the
 * single slot that names the current activity. */
/* @n64 0x8021C718 located */
void BrHookCallC(const BrHooks *pH)
{
    (void)pH;
    (void)BrGameStepInvoke();
}

/* 0x10034C83 */
void BrHookNopB(void)
{
}

