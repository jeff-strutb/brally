/* WHAT IT DOES: the AI's corridor lookahead, eight path points deep.  Each
 * call handles one point and recurses to the next: it pulls both edges of
 * the point 0.2 of the way toward each other, keeps the point's centre, and
 * builds the two midpoints; then it checks that the straight line from the
 * car to the middle of the point stays clear of every corridor level already
 * visited.  A level that is not clear ends that branch.  The deepest clear
 * corridor found so far is remembered, with a left/right steering hint from
 * two more clearance tests, and its edges are copied out for the throttle
 * logic to read.  Returns non-zero once any corridor has been banked.  A
 * depth of zero clears the carried state; no node means start from the
 * path root.  Past the last point of a node it recurses into every child
 * node (or the root again when the node has none). */
/* @implements 0x1005D060 glide BrAiScanCorridor
 * @cpp_symbol ?Scan@Car5D060@@QAEIHHPAUNode5D060@@@Z
 *
 * C++: a recursive thiscall member (`this` = the car in ecx, three stack
 * arguments, `ret 0xc`) whose recursion pushes computed ints from registers,
 * which the C lane's __fastcall stand-in in br_ctlai.c cannot do.  Filed in
 * the driving module beside that C view; its TU (tu_046) is the same C++
 * object as 0x1005C6D0 and 0x1005C8B0.  Source facts that carry the shape:
 *   - the vector type has an inline member operator=: the centre, aim and
 *     look-ahead copies are then component loads and stores through a
 *     pointer with no address temporaries, as in the original;
 *   - `&centre[depth]` is taken once into a pointer and reused by both
 *     midpoints and the aim copy;
 *   - the clearance loop walks two result pointers (a counted index spends a
 *     register the original gives to the level counter);
 *   - the tail is one if/else with a single `return`, and `++mid` is tested
 *     in place, which keeps the index in eax across the null-node block;
 *   - the depth store (0x10B1CA1C) comes after the three edge copies: before
 *     them it keeps depth live in a register across the first copy and
 *     rotates eax/ecx/edx through the whole best-corridor block.
 *
 * BYTE-EXACT under /O2 /Gi (report_cpp "O2 Gi", 4/4), 2026-09-25.  /Gi is
 * this TU's compiler truth: without it the four reloads at the tail join
 * (+0x2D2: node, index, depth, result) come out in another order, and no
 * source spelling moves them.  Under /Gi that order follows the compiler's
 * heap layout, which depends on the lengths of the source and object path
 * strings and on what the shared idb already holds.  Measured the way the
 * image is built -- the "O2 Gi" rows compiled serially in (file, va) order
 * through one fresh idb, sweep-named objects -- this file's name length is
 * inside a matching window (12, 26-27 and 44 characters before `_1005D060`
 * match; the other lengths leave 4-8 bytes of reload order).  !! Renaming this
 * file, or adding an "O2 Gi" TU that sorts before it, can flip it: re-run
 * the serial chain.  The later "O2 Gi" rows (0x10054730, 0x10044860) still
 * match with this TU in the chain. */
#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include <string.h>

typedef struct BrVec3 Vec5D060;   /* x, y, z */

struct Pt5D060 {
    Vec5D060 left;                      /* +0x00 */
    Vec5D060 centre;                    /* +0x0C */
    Vec5D060 right;                     /* +0x18 */
    int      pad24;
};

struct Node5D060 {
    Node5D060     *pNext;               /* +0x00 first child */
    Node5D060     *pSib;                /* +0x04 next sibling */
    char           pad08[0x14 - 0x08];
    unsigned short count;               /* +0x14 points in this node */
    unsigned char  flags;               /* +0x16 bit 0: not scannable */
    char           pad17[0x40 - 0x17];
    Pt5D060        aPt[1];              /* +0x40, 0x28 each */
};

class Car5D060 {
public:
    char     pad00[0x30];
    Vec5D060 pos;                       /* +0x30 */

    unsigned Scan(int depth, int mid, Node5D060 *pNode);
};

extern "C" {
/* BrVec3Lerp: prototype in br_funcs.h */
/* BrVec3Midpoint: prototype in br_funcs.h */
/* BrSeg2Intersect: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
}

unsigned Car5D060::Scan(int depth, int mid, Node5D060 *pNode)
{
    unsigned ret = 0;
    Vec5D060 midPt;
    int      level, k;
    int      next;
    int      limit;
    Vec5D060 *pA, *pB;
    Vec5D060 *pC;

    if (pNode == 0) {
        pNode = (Node5D060 *)(BR_PTR32(Node5D060 *, g_brTrkHdr.aPathRoot));
        mid = 0;
    }
    if (depth == 0) {
        DAT_10ac680c = 0;
        (*(int *)&g_brAiScanFlag18) = 0;
        DAT_10b1cf08 = 0;
        DAT_10b1cf0c = 0;
    } else {
        if (depth > 8 || ((*(unsigned char *)&((BrAiPathNode *)(pNode))->flags) & 1))
            return 0;

        BrVec3Lerp((struct BrVec3 *)(&(*(Vec5D060 (*)[])&g_aScanInsetA)[depth]), (const struct BrVec3 *)(&(*(Pt5D060 *)&((BrAiPathNode *)(pNode))->aPt[mid]).left), (const struct BrVec3 *)(&(*(Pt5D060 *)&((BrAiPathNode *)(pNode))->aPt[mid]).right), 0.2f);
        pC = &(*(Vec5D060 (*)[])&g_aScanCentre)[depth];
        *pC = (*(Pt5D060 *)&((BrAiPathNode *)(pNode))->aPt[mid]).centre;
        BrVec3Lerp((struct BrVec3 *)(&(*(Vec5D060 (*)[])&g_aScanInsetB)[depth]), (const struct BrVec3 *)(&(*(Pt5D060 *)&((BrAiPathNode *)(pNode))->aPt[mid]).right), (const struct BrVec3 *)(&(*(Pt5D060 *)&((BrAiPathNode *)(pNode))->aPt[mid]).left), 0.2f);
        BrVec3Midpoint((struct BrVec3 *)(&(*(Vec5D060 (*)[])&g_aScanMidB)[depth]), (const struct BrVec3 *)(&(*(Vec5D060 (*)[])&g_aScanInsetB)[depth]), (const struct BrVec3 *)(pC));
        BrVec3Midpoint((struct BrVec3 *)(&(*(Vec5D060 (*)[])&g_aScanMidA)[depth]), (const struct BrVec3 *)(&(*(Vec5D060 (*)[])&g_aScanInsetA)[depth]), (const struct BrVec3 *)(pC));
        midPt.x = ((*(Pt5D060 *)&((BrAiPathNode *)(pNode))->aPt[mid]).left.x + (*(Pt5D060 *)&((BrAiPathNode *)(pNode))->aPt[mid]).right.x) * DAT_100778cc;
        midPt.y = ((*(Pt5D060 *)&((BrAiPathNode *)(pNode))->aPt[mid]).right.y + (*(Pt5D060 *)&((BrAiPathNode *)(pNode))->aPt[mid]).left.y) * DAT_100778cc;

        pA = (*(Vec5D060 (*)[])&g_aScanHitA);
        pB = (*(Vec5D060 (*)[])&g_aScanHitB);
        for (level = 1; level < depth - 1; level++, pA++, pB++) {
            if (BrSeg2Intersect((const struct BrVec2 *)(&pos), (const struct BrVec2 *)(&midPt), (const struct BrVec2 *)(pA), (const struct BrVec2 *)(pB)) == 0) {
                DAT_10b1cf0c = 1;
                goto tail;
            }
        }

        if (depth > DAT_10ac680c) {
            DAT_10ac680c = depth;
            (*(Node5D060 * *)&g_pBrAiScanBestNode) = (Node5D060 *)((BrAiPathNode *)(pNode));
            (*(int *)&g_brAiScanBestPt) = mid;
            if (depth > 2) {
                if (BrSeg2Intersect((const struct BrVec2 *)(&pos), (const struct BrVec2 *)(&(*(Vec5D060 *)&g_brAiScanProbe)), (const struct BrVec2 *)(&(*(Vec5D060 *)&g_brAiScanEndA)), (const struct BrVec2 *)(&(*(Vec5D060 (*)[])&g_aScanHitA)[0])) != 0) {
                    (*(int *)&g_brAiBiasPos) = 0;
                    DAT_10b1cf04 = 1;
                } else if (BrSeg2Intersect((const struct BrVec2 *)(&pos), (const struct BrVec2 *)(&(*(Vec5D060 *)&g_brAiScanProbe)), (const struct BrVec2 *)(&DAT_10b1c89c), (const struct BrVec2 *)(&(*(Vec5D060 (*)[])&g_aScanHitB)[0])) != 0) {
                    (*(int *)&g_brAiBiasPos) = 1;
                    DAT_10b1cf04 = 0;
                } else {
                    (*(int *)&g_brAiBiasPos) = 0;
                    DAT_10b1cf04 = 0;
                }
            } else {
                (*(int *)&g_brAiBiasPos) = 0;
                DAT_10b1cf04 = 0;
            }
            ret = 1;
            DAT_10b1ce88 = *pC;
            DAT_10af11f8 = (*(Vec5D060 (*)[])&g_aScanCentre)[1 + (depth >> 1)];
            memcpy((*(Vec5D060 (*)[])&g_aBrAiScanA), &(*(Vec5D060 (*)[])&g_aScanInsetB)[1], depth * sizeof(Vec5D060));
            memcpy((*(Vec5D060 (*)[])&g_aBrAiScanB), &(*(Vec5D060 (*)[])&g_aScanCentre)[1], depth * sizeof(Vec5D060));
            memcpy((*(Vec5D060 (*)[])&g_aScanOut3), &(*(Vec5D060 (*)[])&g_aScanInsetA)[1], depth * sizeof(Vec5D060));
            DAT_10b1ca1c = depth;
        }
    }

tail:
    if (++mid == (*(unsigned short *)&((BrAiPathNode *)(pNode))->count)) {
        pNode = BR_PTR32(Node5D060 *, ((BrAiPathNode *)(pNode))->aNext);
        if (pNode == 0)
            pNode = (Node5D060 *)(BR_PTR32(Node5D060 *, g_brTrkHdr.aPathRoot));
        if (pNode != 0) {
            next = depth + 1;
            do {
                ret |= Scan(next, 0, pNode);
                pNode = BR_PTR32(Node5D060 *, ((BrAiPathNode *)(pNode))->aSib);
            } while (pNode != 0);
        }
    } else {
        ret |= Scan(depth + 1, mid, pNode);
    }
    return ret;
}

/* C entry point */
extern "C" unsigned BrAiScanCorridor(void *self, int depth, int mid, void *pNode)
{
    return ((Car5D060 *)self)->Scan(depth, mid, (Node5D060 *)pNode);
}
