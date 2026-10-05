/* n64-cflags: -O1 */
/* settimer.c -- libultra's timer start (os/settimer.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Start a timer that sends msg to mq after countdown ticks
 * (the interval when 0), then every interval ticks; rearm the counter
 * interrupt when it is the next to fire. */
/* @implements 0x80268390 tgr osSetTimer */
int osSetTimer(OSTimer *t, OSTime countdown, OSTime interval, OSMesgQueue *mq, OSMesg msg)
{
	OSTime time;

	t->next = NULL;
	t->prev = NULL;
	t->interval = interval;
	if (countdown != 0)
		t->value = countdown;
	else
		t->value = interval;
	t->mq = mq;
	t->msg = msg;
	time = __osInsertTimer(t);
	if (__osTimerList->next == t) {
		__osSetTimerIntr(time);
	}
	return 0;
}
