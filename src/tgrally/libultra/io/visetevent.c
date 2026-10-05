/* n64-cflags: -O1 */
/* visetevent.c -- libultra's retrace message (io/visetevent.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Have the VI manager send m to mq every retraceCount
 * retraces. */
/* @implements 0x80265600 tgr osViSetEvent */
void osViSetEvent(OSMesgQueue *mq, OSMesg m, u32 retraceCount)
{
	register u32 saveMask = __osDisableInt();

	__osViNext->msgq = mq;
	__osViNext->msg = m;
	__osViNext->retraceCount = retraceCount;
	__osRestoreInt(saveMask);
}
