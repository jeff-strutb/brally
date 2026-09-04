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

/* 0x1002B280 */
/* WHAT IT DOES: points both halves of a pair of cursors at the same place,
 * which is how a buffer gets rewound to its start. What the buffer holds is not
 * established here. */
/* @implements 0x1002B280 d3d BrCursorPairSet */
#ifdef BR_MATCHING_BUILD
void *g_brCursor575510;   /* 0x10575510 */
void *g_brCursor575518;   /* 0x10575518 */

void BrCursorPairSet(void *pv)
{
    /* CLOSE, NOT MATCHING -- 15 bytes / 4 instructions against the original's
     * 18 / 5.  The one argument and the two absolute stores are right; the
     * whole residue is that the original keeps a second live copy:
     *
     *     8b c8            mov ecx, eax
     *     a3 <g1>          mov [g1], eax      (accumulator form, 5 bytes)
     *     89 0d <g2>       mov [g2], ecx      (6 bytes)
     *
     * against our `a3 <g1>` / `a3 <g2>`.  Note the original's encoding is
     * strictly WORSE -- one more instruction and one more byte -- so VC5 is
     * not choosing it for size; it is holding two live ranges where we have
     * one.  See the accumulator-encoding entry in docs/VC5-IDIOMS.md.
     *
     * PROBED AND DEAD, do not re-run -- ALL of these compile to the identical
     * 15 bytes: chained assignment (a = b = pv); either store order; a `void
     * *p = pv` local feeding one or both stores; and reading the first global
     * BACK for the second store (`g2 = g1;`), in both orders -- VC5 forwards
     * the store and folds the load away.  Register-allocation class; the
     * `VARIABLE IDENTITY IS INERT` entry says VC5 splits live ranges itself,
     * which is the same statement from the other side. */
    g_brCursor575510 = pv;
    g_brCursor575518 = pv;
}
#else
void BrCursorPairSet(BrCursorPair *pPair, void *pv)
{
    pPair->f10 = pv;
    pPair->f18 = pv;
}
#endif

/* 0x1002F460 */
/* WHAT IT DOES: purpose unclear. Observably it reads a pair of numbers out of
 * a table using two selector bytes as a row and column, and if one flag bit is
 * set it rotates the first of the pair by half a turn of twelve -- six becomes
 * zero, zero becomes six -- which has the shape of a mirroring or opposite-
 * direction rule. The second number is looked up fresh so the rotation cannot
 * affect it. What the table describes is not established here. */
/* @implements 0x1002F460 d3d BrSelLookup */
#ifdef BR_MATCHING_BUILD
/* Original: no parameters. The input record comes through a pointer
 * global, the table is two interleaved pinned byte columns (0x100B3028 /
 * 0x100B3029), and the results are globals. A shared unsigned-char temp
 * carries first the table byte (edx, copied into the int a) and then the
 * flag byte (dl reloaded without re-zeroing). */
typedef struct BrSelInM {
    unsigned char f00;
    unsigned char pad[3];
    unsigned char f04;
    unsigned char f05;
} BrSelInM;

extern BrSelInM     *DAT_10af2094;
extern unsigned char DAT_100b3028[];
extern unsigned char DAT_100b3029[];
extern int           DAT_100b3014;
extern int           DAT_104b15e8;

void BrSelLookup(void)
{
    BrSelInM *p = DAT_10af2094;
    int idx = p->f04 * 12 + p->f05;
    unsigned char t;
    int a;

    t = DAT_100b3028[idx * 2];
    a = t;
    DAT_100b3014 = a;

    t = p->f00;
    if (t & 1) {
        if (a < 6)
            a += 6;
        else
            a -= 6;
        DAT_100b3014 = a;
    }

    /* recomputed, so the fold above cannot leak into the second lookup */
    idx = p->f04 * 12 + p->f05;
    DAT_104b15e8 = DAT_100b3029[idx * 2];
}
#else
void BrSelLookup(const BrSelInput *pIn, const unsigned char (*aTable)[2],
                 int *pOutA, int *pOutB)
{
    int idx = (int)pIn->f04 * 12 + (int)pIn->f05;
    int a   = (int)aTable[idx][0];

    *pOutA = a;

    if (pIn->f00 & 1) {
        a = (a >= 6) ? (a - 6) : (a + 6);
        *pOutA = a;
    }

    /* Recomputed from f04/f05 in the original, so the fold above cannot
     * leak into the second lookup. */
    idx = (int)pIn->f04 * 12 + (int)pIn->f05;
    *pOutB = (int)aTable[idx][1];
}
#endif

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

/* ================================================================== */
/* 6. Peer table                                                      */
/* ================================================================== */

/* 0x10036030 */
/* WHAT IT DOES: finds which slot in the network player table belongs to a
 * given player. If that player is not there yet it gives back the first free
 * slot instead, so the same call both looks up and allocates; if the table is
 * full it reports failure. The local player is always slot zero. */
BrPeer g_aBrPeers[BR_PEER_COUNT];   /* Glide 0x117A9B88; loop 1 starts at [1] */

#ifdef BR_MATCHING_BUILD
/* The original probes every record under that record's own Win32 mutex:
 * WaitForSingleObject(h, INFINITE), read f04/f2C, ReleaseMutex(h) -- through
 * the import table (the Wait import is CSEd into ebp, Release stays a
 * memory call).  Each probe's verdict is computed between the reads and the
 * release, then tested after it. */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

/* WHAT IT DOES: find which peer slot a network id belongs to. Id 1 is always
 * slot 0 -- the local player -- and everything else is a linear scan. */
/* @implements 0x10036030 d3d BrPeerFind */
int BrPeerFind(uint32_t id)
{
    int i;

    if (id == 1)
        return 0;

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        const BrPeer *p = &g_aBrPeers[i];
        uint32_t idv, st;

        WaitForSingleObject((void *)(uintptr_t)p->hMutex, 0xFFFFFFFFu);
        idv = p->f04;
        st  = p->f2C;
        ReleaseMutex((void *)(uintptr_t)p->hMutex);

        if ((st & BR_PEER_STATE_MASK) >= 1u && idv == id)
            return i;
    }

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        const BrPeer *p = &g_aBrPeers[i];
        uint32_t st;

        WaitForSingleObject((void *)(uintptr_t)p->hMutex, 0xFFFFFFFFu);
        /* Dword load, byte-width AND (`and bl,0x3f`), then the neg/sbb/inc
         * boolean OVERWRITES st in the same register (dword sbb), crossing
         * the Release call; the byte cast in the if gives the original's
         * `test bl,bl`.
         * RESIDUE (1+0 regnorm, +1 insn): the original births the load in
         * ebx and computes in place; ours computes in eax and copies to
         * ebx before the call.  Probed and failed: uint8_t st (byte load),
         * split byte local (extra byte move), separate int bFree (same). */
        st = p->f2C;
        st = (uint32_t)(((uint8_t)st & BR_PEER_STATE_MASK) == 0u);
        ReleaseMutex((void *)(uintptr_t)p->hMutex);

        if ((uint8_t)st)
            return i;
    }

    return -1;
}
#else
int BrPeerFind(uint32_t id)
{
    int i;

    if (id == 1)
        return 0;

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        if ((g_aBrPeers[i].f2C & BR_PEER_STATE_MASK) != 0u &&
            g_aBrPeers[i].f04 == id)
            return i;
    }

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        if ((g_aBrPeers[i].f2C & BR_PEER_STATE_MASK) == 0u)
            return i;
    }

    return -1;
}
#endif

