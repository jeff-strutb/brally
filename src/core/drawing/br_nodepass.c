/* br_nodepass.c -- drawing: the scene-tree mark pass.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice3_40.c, an address batch and not a module.  One walk
 * stamps a scratch bit on every node it reaches -- which is what stops a
 * loop in the tree running away -- a second walk clears it again, and the
 * third runs the pair back to back over the scene root.
 *
 * slice3_40.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
#include <string.h>

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is thiscall.  Rename the
 * prototype so the thiscall definition is not a C2373 redefinition. */
#define BrCarInitTables BrCarInitTables_cdecl_hdr
#define BrCarClear29C8  BrCarClear29C8_cdecl_hdr
#define BrZeroRegions   BrZeroRegions_cdecl_hdr
#endif
#include "slice3_40.h"
#ifdef BR_MATCHING_BUILD
#undef BrCarInitTables
#undef BrCarClear29C8
#undef BrZeroRegions
void BrZeroRegions(void);
#endif

#include "br_match.h"    /* BR_THISCALL1 */

/* 0x10061660 */
/* WHAT IT DOES: walks the whole scene tree and, for each node it has not
 * already visited, stamps a visit mark on it and then descends into it. The
 * visible effect is that one particular per-node byte is cleared, but only
 * in two specific game modes. The mark is set before descending, which is
 * what stops a loop in the tree from running away forever -- it is a scratch
 * bit, not a visibility flag. */
/* @implements 0x10061660 d3d BrNodeMarkPass */
/* @n64 0x80255BA0 located */
void BrNodeMarkPass(BrNode *pNode)
{
    while (pNode != NULL) {
        uint16_t flags = pNode->flags;

        if (!(flags & BR_NODE_FLAG_MARK) && !(flags & BR_NODE_FLAG_SKIP)) {
            uint8_t f11 = pNode->f11;

            /* set the mark BEFORE recursing -- this is the cycle guard */
            pNode->flags = (uint16_t)(pNode->flags | BR_NODE_FLAG_MARK);

            if (f11 == 2 && (BrG_0B380C == 3 || BrG_0B380C == 9)) {
                pNode->f11 = 0;
            }
            BrNodeMarkPass(pNode->f00);
        }
        pNode = pNode->f04;
    }
}

/* WHAT IT DOES: the exact reverse walk: it takes the visit mark off every
 * node the pass above stamped, leaving the tree ready to be walked again. */
/* @implements 0x100616C0 d3d BrNodeClearMarkPass */
/* @n64 0x80255C50 located */
void BrNodeClearMarkPass(BrNode *pNode)
{
    while (pNode != NULL) {
        if (pNode->flags & BR_NODE_FLAG_MARK) {
            BrNode *pChild = pNode->f00;
            pNode->flags = (uint16_t)(pNode->flags & 0x7FFFu);
            BrNodeClearMarkPass(pChild);
        }
        pNode = pNode->f04;
    }
}

/* WHAT IT DOES: runs the mark pass and then the unmark pass over the whole
 * scene tree, so that the only lasting effect is whatever the first pass
 * changed on the way through. */
/* @implements 0x10061700 d3d BrNodeRunMarkPass */
/* @n64 0x80255CA0 located */
void BrNodeRunMarkPass(void)
{
    /* the root is re-read from the global between the two calls */
    BrNodeMarkPass(BrG_6C7CB8);
    BrNodeClearMarkPass(BrG_6C7CB8);
}
