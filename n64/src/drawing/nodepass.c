/* nodepass.c -- walking the scene tree
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrNode {        /* a scene-tree node */
  struct BrNode *child;         /* 0x00 */
  struct BrNode *next;          /* 0x04  next sibling */
  char pad08[9];
  unsigned char kind;           /* 0x11 */
  char pad12[4];
  unsigned short flags;         /* 0x16  bit 15: visited this pass, bit 0: skip */
} BrNode;
void BrNodeMarkPass(BrNode *n);
void BrNodeClearMarkPass(BrNode *n);
extern BrNode *D_80025C70;
extern int D_8028B940;
/* -- end declarations -- */

/* WHAT IT DOES: Walk the scene tree once, stamping each node and applying
 * the per-mode fix-up, then walk it again taking the stamps off so the tree
 * is ready for the next pass. */
/* @implements 0x80255CA0 tgr BrScenePassRun */
void BrScenePassRun(void)
{
  BrNodeMarkPass(D_80025C70);
  BrNodeClearMarkPass(D_80025C70);
}

/* WHAT IT DOES: Walk the scene tree, stamping each node not yet visited
 * (and not marked skip) before descending into its children; on tracks 3
 * and 8 nodes of kind 2 are turned into kind 0 on the way. */
/* @implements 0x80255BA0 tgr BrNodeMarkPass */
void BrNodeMarkPass(BrNode *n)
{
  while (n != 0) {
    if (!(n->flags & 0x8000) && !(n->flags & 1)) {
      n->flags |= 0x8000;
      if (n->kind == 2 && (D_8028B940 == 3 || D_8028B940 == 8)) {
        n->kind = 0;
      }
      BrNodeMarkPass(n->child);
    }
    n = n->next;
  }
}


/* WHAT IT DOES: Walk the scene tree taking off every visit stamp the
 * marking pass left, ready for the next walk. */
/* @implements 0x80255C50 tgr BrNodeClearMarkPass */
void BrNodeClearMarkPass(BrNode *n)
{
  while (n != 0) {
    if (n->flags & 0x8000) {
      n->flags &= 0x7fff;
      BrNodeClearMarkPass(n->child);
    }
    n = n->next;
  }
}

