/* n64-cflags: -O1 */
/* seteventmesg.c -- libultra's system event registration
 * (os/seteventmesg.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Have the system send msg to mq whenever the given event
 * happens. */
/* @implements 0x80265440 tgr osSetEventMesg */
void osSetEventMesg(OSEvent event, OSMesgQueue *mq, OSMesg msg)
{
	register u32 saveMask = __osDisableInt();
	__OSEventState *es;

	es = &__osEventStateTab[event];
	es->messageQueue = mq;
	es->message = msg;
	__osRestoreInt(saveMask);
}
