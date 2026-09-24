/* Glide match for BrRacePathAdvance — 0x1005ECF0
 *
 * The port body lives in src/core/racing/br_racestep.c and reaches the AI
 * path through two safety helpers the original does not have: BrAiNodeAt()
 * (decode a node by FILE OFFSET into a local BrAiNode, bounds-checked) and
 * BrAiPoint_() (copy one point out, bounds-checked).  The original walks
 * RELOCATED POINTERS and reads the fields in place -- `test byte [esi+0x16]`,
 * `mov esi,[esi+4]`, `lea edx,[esi+eax*8+0x8c]` -- so every one of those
 * helper calls, its error branch and its `++g_aBrRaceStepHole[]` is a port
 * addition.  That is the whole 204 -> 443 byte gap; see the
 * port-safety/globals-parameter class in docs/VC5-IDIOMS.md.
 *
 * Layout, read off the original (a node is a relocated pointer, not an
 * offset):
 *      +0x00  next node          `mov esi,[esi]`
 *      +0x04  sibling            `mov esi,[esi+4]`
 *      +0x14  point count, u16   `xor ecx,ecx; mov cx,[esi+0x14]`
 *      +0x16  flag byte, bit 0 = skip this node
 *      +0x40  BrAiPoint[], stride 0x28, centre at +0x0C, arc at +0x24
 * so pts[i].centre is +0x4c+i*0x28 (`lea eax,[esi+ecx*8]; add eax,0x4c`) and
 * pts[i].arc is +0x64+i*0x28 (`lea edx,[esi+eax*8+0x8c]` addresses pts[i+1]'s
 * arc, with pts[i]'s read as `[edx-0x28]`).
 *
 * Shape notes:
 *  - BOTH lerps write straight into the 0x10B1CE98 global; the second one
 *    also READS it as its `b` operand.  The port's local `BrVec3 p` copied
 *    out at the end is an addition and costs the whole tail.
 *  - `avail` is a named float local: VC5 spills it (`fstp [esp+0x14]`, into
 *    the dead `index` home slot) and reloads it for both the compare and the
 *    divide.  Writing the product inline instead loses the spill.
 *  - the stop test is `!(dist > avail)`: `fcomp` + `test ah,0x41` + `jne`
 *    takes the found branch on less, on equal AND on unordered.
 *  - `ratio` is written back into its own parameter slot
 *    (`mov [esp+0x18],0x3f800000`), so it is a plain assignment to the
 *    parameter, not a separate local.
 */
#ifdef BR_MATCHING_BUILD

typedef struct RcVec3 {
    float x, y, z;
} RcVec3;

typedef struct RcPoint {            /* 0x28 */
    RcVec3 left;                    /* +0x00 */
    RcVec3 centre;                  /* +0x0C */
    RcVec3 right;                   /* +0x18 */
    float  arc;                     /* +0x24 */
} RcPoint;

typedef struct RcNode {
    struct RcNode *pNext;           /* +0x00 */
    struct RcNode *pSib;            /* +0x04 */
    char           pad08[0x0C];     /* +0x08 */
    unsigned short count;           /* +0x14 */
    unsigned short flags;           /* +0x16 */
    char           pad18[0x28];     /* +0x18 */
    RcPoint        pts[1];          /* +0x40 */
} RcNode;

typedef char chk_pts[sizeof(RcPoint) == 0x28 && sizeof(RcNode) == 0x68
                     ? 1 : -1];

extern RcVec3  g_brRacePathPos;     /* 0x10B1CE98 */
extern RcNode *g_brRacePathNode;    /* 0x10B1CBEC */
extern int     g_brRacePathIndex;   /* 0x10AF07F0 */

void BrVec3Lerp(RcVec3 *pOut, const RcVec3 *pA, const RcVec3 *pB, float t);

/* WHAT IT DOES: move a car's marker along the racing line to the node it is
 * nearest now, walking forward from where it was last frame. This is what
 * keeps track of a car's progress round the lap, and it feeds both the
 * position table and the AI. */
/* @implements 0x1005ECF0 glide BrRacePathAdvance */
void BrRacePathAdvance(RcNode *pNode, int index, float ratio, float dist)
{
    for (;;) {
        int count;

        /* 0x1005ECFD/0x1005ED05: hop SKIP nodes along the sibling link.
         * The null test at the top of the node loop IS this walk's guard --
         * VC5 threads its exit straight to the return -- so there is no
         * separate `if (pNode == 0) return` before it; with one, VC5 rotates
         * the node loop (+15 B, a second epilogue). */
        while (pNode != 0 && (pNode->flags & 1) != 0)
            pNode = pNode->pSib;
        if (pNode == 0)                 /* 0x1005ED11 */
            return;

        count = pNode->count;           /* 0x1005ED1B, u16 -> int */
        while (index < count) {         /* 0x1005ED1F signed, 0x1005ED57 */
            float avail = (pNode->pts[index].arc - pNode->pts[index + 1].arc)
                        * ratio;        /* 0x1005ED2D..0x1005ED3C */

            /* 0x1005ED40 `fcomp` + `test ah,0x41` + `jne`: C0 is set for
             * LESS and C3 for EQUAL, both for UNORDERED, so the walk stops
             * on less, on equal and on a NaN distance. */
            if (!(dist > avail)) {
                /* BrVec3Lerp is (a - b) * t + b, so t == 1 gives pts[i]. */
                BrVec3Lerp(&g_brRacePathPos,
                           &pNode->pts[index].centre,
                           &pNode->pts[index + 1].centre, ratio);
                BrVec3Lerp(&g_brRacePathPos,
                           &pNode->pts[index + 1].centre,
                           &g_brRacePathPos, dist / avail);
                g_brRacePathNode  = pNode;      /* 0x10B1CBEC */
                g_brRacePathIndex = index;      /* 0x10AF07F0 */
                return;
            }

            dist -= avail;              /* 0x1005ED4F */
            index++;                    /* 0x1005ED53 */
            ratio = 1.0f;               /* 0x1005ED59 */
        }

        pNode = pNode->pNext;           /* 0x1005ED67 */
        index = 0;
    }
}

#endif /* BR_MATCHING_BUILD */
