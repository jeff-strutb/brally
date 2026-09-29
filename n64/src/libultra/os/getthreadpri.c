/* n64-cflags: -O1 */
/* getthreadpri.c -- libultra's thread priority read (os/getthreadpri.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: A thread's priority (0: the running thread's). */
/* @implements 0x8026C040 tgr osGetThreadPri */
OSPri osGetThreadPri(OSThread *t)
{
	if (t == NULL)
		t = __osRunningThread;
	return t->priority;
}
