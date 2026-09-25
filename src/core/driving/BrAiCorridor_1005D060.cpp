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
 * match; the other lengths leave 4-8 bytes of reload order).  ‼ Renaming this
 * file, or adding an "O2 Gi" TU that sorts before it, can flip it: re-run
 * the serial chain.  The later "O2 Gi" rows (0x10054730, 0x10044860) still
 * match with this TU in the chain. */
#include <string.h>

struct Vec5D060 {
    float x, y, z;

    Vec5D060 &operator=(const Vec5D060 &o) { x = o.x; y = o.y; z = o.z; return *this; }
};

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
void BrVec3Lerp(Vec5D060 *pOut, const Vec5D060 *pA, const Vec5D060 *pB, float t);  /* 0x10034620 */
void BrVec3Midpoint(Vec5D060 *pOut, const Vec5D060 *pA, const Vec5D060 *pB);       /* 0x100346D0 */
int  BrSeg2Intersect(const Vec5D060 *pO, const Vec5D060 *pP,
                     Vec5D060 *pA, Vec5D060 *pB);                                  /* 0x100350F0 */

extern Node5D060 *DAT_106eed48;         /* path root */
extern float      DAT_100778cc;         /* 0.5f */
extern int        DAT_10ac680c;         /* deepest clear corridor so far */
extern int        DAT_10b1ca18;
extern int        DAT_10b1cf08;
extern int        DAT_10b1cf0c;         /* a level was blocked */
extern Node5D060 *DAT_10ac67d4;         /* node of the deepest corridor */
extern int        DAT_10b1c888;         /* its point index */
extern int        DAT_10b1cbe8;         /* steering hint, one way */
extern int        DAT_10b1cf04;         /* steering hint, the other way */
extern int        DAT_10b1ca1c;         /* depth of the copied edges */
extern Vec5D060   DAT_10b1ce88;         /* aim point */
extern Vec5D060   DAT_10af11f8;         /* half-depth look-ahead point */
extern Vec5D060   DAT_10b1caa0;         /* hint probe */
extern Vec5D060   DAT_10b1c8fc;
extern Vec5D060   DAT_10b1c89c;
extern Vec5D060   DAT_10b1cb3c[];       /* left edge pulled in, per depth */
extern Vec5D060   DAT_10b1ca7c[];       /* centre, per depth */
extern Vec5D060   DAT_10b1cadc[];       /* right edge pulled in, per depth */
extern Vec5D060   DAT_10b1c8e4[];       /* midpoint(right, centre) */
extern Vec5D060   DAT_10b1c884[];       /* midpoint(left, centre) */
extern Vec5D060   DAT_10b1caf4[];       /* clearance results A */
extern Vec5D060   DAT_10b1cb54[];       /* clearance results B */
extern Vec5D060   DAT_10b1c9b8[];       /* copied-out right edges */
extern Vec5D060   DAT_10b1ca28[];       /* copied-out centres */
extern Vec5D060   DAT_10b1c958[];       /* copied-out left edges */
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
        pNode = DAT_106eed48;
        mid = 0;
    }
    if (depth == 0) {
        DAT_10ac680c = 0;
        DAT_10b1ca18 = 0;
        DAT_10b1cf08 = 0;
        DAT_10b1cf0c = 0;
    } else {
        if (depth > 8 || (pNode->flags & 1))
            return 0;

        BrVec3Lerp(&DAT_10b1cb3c[depth], &pNode->aPt[mid].left, &pNode->aPt[mid].right, 0.2f);
        pC = &DAT_10b1ca7c[depth];
        *pC = pNode->aPt[mid].centre;
        BrVec3Lerp(&DAT_10b1cadc[depth], &pNode->aPt[mid].right, &pNode->aPt[mid].left, 0.2f);
        BrVec3Midpoint(&DAT_10b1c8e4[depth], &DAT_10b1cadc[depth], pC);
        BrVec3Midpoint(&DAT_10b1c884[depth], &DAT_10b1cb3c[depth], pC);
        midPt.x = (pNode->aPt[mid].left.x + pNode->aPt[mid].right.x) * DAT_100778cc;
        midPt.y = (pNode->aPt[mid].right.y + pNode->aPt[mid].left.y) * DAT_100778cc;

        pA = DAT_10b1caf4;
        pB = DAT_10b1cb54;
        for (level = 1; level < depth - 1; level++, pA++, pB++) {
            if (BrSeg2Intersect(&pos, &midPt, pA, pB) == 0) {
                DAT_10b1cf0c = 1;
                goto tail;
            }
        }

        if (depth > DAT_10ac680c) {
            DAT_10ac680c = depth;
            DAT_10ac67d4 = pNode;
            DAT_10b1c888 = mid;
            if (depth > 2) {
                if (BrSeg2Intersect(&pos, &DAT_10b1caa0, &DAT_10b1c8fc, &DAT_10b1caf4[0]) != 0) {
                    DAT_10b1cbe8 = 0;
                    DAT_10b1cf04 = 1;
                } else if (BrSeg2Intersect(&pos, &DAT_10b1caa0, &DAT_10b1c89c, &DAT_10b1cb54[0]) != 0) {
                    DAT_10b1cbe8 = 1;
                    DAT_10b1cf04 = 0;
                } else {
                    DAT_10b1cbe8 = 0;
                    DAT_10b1cf04 = 0;
                }
            } else {
                DAT_10b1cbe8 = 0;
                DAT_10b1cf04 = 0;
            }
            ret = 1;
            DAT_10b1ce88 = *pC;
            DAT_10af11f8 = DAT_10b1ca7c[1 + (depth >> 1)];
            memcpy(DAT_10b1c9b8, &DAT_10b1cadc[1], depth * sizeof(Vec5D060));
            memcpy(DAT_10b1ca28, &DAT_10b1ca7c[1], depth * sizeof(Vec5D060));
            memcpy(DAT_10b1c958, &DAT_10b1cb3c[1], depth * sizeof(Vec5D060));
            DAT_10b1ca1c = depth;
        }
    }

tail:
    if (++mid == pNode->count) {
        pNode = pNode->pNext;
        if (pNode == 0)
            pNode = DAT_106eed48;
        if (pNode != 0) {
            next = depth + 1;
            do {
                ret |= Scan(next, 0, pNode);
                pNode = pNode->pSib;
            } while (pNode != 0);
        }
    } else {
        ret |= Scan(depth + 1, mid, pNode);
    }
    return ret;
}
