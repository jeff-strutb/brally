/* nodepass.c -- walking the scene tree
 */
#include "tgr/common.h"
#include "tgr/track.h"

/* -- declarations -- */
typedef struct BrNode {        /* a scene-tree node: a path segment (cartridge data, big-endian) */
  be32_t child;                 /* 0x00  BrNode */
  be32_t next;                  /* 0x04  next sibling */
  char pad08[9];
  unsigned char kind;           /* 0x11 */
  char pad12[4];
  be16_t flags;                 /* 0x16  bit 15: visited this pass, bit 0: skip */
} BrNode;
void BrNodeMarkPass(BrNode *n);
void BrNodeClearMarkPass(BrNode *n);
#define D_80025C70 BEPTR(BrNode *, D_80025C00.path)
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
    if (!(BE16(n->flags) & 0x8000) && !(BE16(n->flags) & 1)) {
      SET16(n->flags, BE16(n->flags) | 0x8000);
      if (n->kind == 2 && (D_8028B940 == 3 || D_8028B940 == 8)) {
        n->kind = 0;
      }
      BrNodeMarkPass(BEPTR(BrNode *, n->child));
    }
    n = BEPTR(BrNode *, n->next);
  }
}


/* WHAT IT DOES: Walk the scene tree taking off every visit stamp the
 * marking pass left, ready for the next walk. */
/* @implements 0x80255C50 tgr BrNodeClearMarkPass */
void BrNodeClearMarkPass(BrNode *n)
{
  while (n != 0) {
    if (BE16(n->flags) & 0x8000) {
      SET16(n->flags, BE16(n->flags) & 0x7fff);
      BrNodeClearMarkPass(BEPTR(BrNode *, n->child));
    }
    n = BEPTR(BrNode *, n->next);
  }
}

