/* n64-cflags: -O1 */
/* thread.c -- libultra's thread queue removal (os/thread.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Unlink a thread from a priority queue, if it is on it. */
/* @implements 0x8026AC80 tgr __osDequeueThread */
void __osDequeueThread(OSThread **queue, OSThread *t)
{
	register OSThread *pred;
	register OSThread *succ;

	pred = (OSThread *)queue;
	succ = pred->next;
	while (succ != NULL) {
		if (succ == t) {
			pred->next = t->next;
			return;
		}
		pred = succ;
		succ = pred->next;
	}
}
