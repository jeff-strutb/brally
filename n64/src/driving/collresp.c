/* collresp.c -- collision response: contact planes and the list they are kept on
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrCrNode {       /* a contact-list node */
  void *plane;
  struct BrCrNode *next;
} BrCrNode;
extern BrCrNode *D_802A4A20;          /* the contact list */
extern BrCrNode *D_802A4A24;          /* next free node */
void osSyncPrintf(char *fmt, ...);
/* -- end declarations -- */

/* WHAT IT DOES: Signed distance of a point from a plane: n . p + d. */
/* @implements 0x8025B704 tgr BrCrPlaneDist */
float BrCrPlaneDist(float n[3], float d, float p[3])
{
  return n[0] * p[0] + n[1] * p[1] + n[2] * p[2] + d;
}

/* WHAT IT DOES: Push a contact plane on the front of the contact list,
 * taking the next node from the pool.  The PC twin (br_collresp.c) is the
 * same source. */
/* @implements 0x8025C24C tgr BrCrListPush */
void BrCrListPush(void *plane)
{
  BrCrNode *n = D_802A4A24++;

  n->plane = plane;
  n->next = D_802A4A20;
  D_802A4A20 = n;
}

/* WHAT IT DOES: Debug print of how many contacts are on the list (the
 * print itself is compiled out in the retail osSyncPrintf). */
/* @implements 0x8025C27C tgr BrCrListPrintCount */
void BrCrListPrintCount(void)
{
  BrCrNode *n;
  short count;

  n = D_802A4A20;
  if (n != 0) {
    count = 0;
    for (; n != 0; n = n->next) {
      count++;
    }
    osSyncPrintf("----%d-------------------------------\n", count);
  }
}
