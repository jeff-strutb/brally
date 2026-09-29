/* n64-cflags: -O1 */
/* pigetcmdq.c -- libultra's PI manager command queue (io/pigetcmdq.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: The PI manager's command queue, or 0 before it runs. */
/* @implements 0x8026B720 tgr osPiGetCmdQueue */
OSMesgQueue *osPiGetCmdQueue(void)
{
	if (!__osPiDevMgr.active)
		return NULL;
	return __osPiDevMgr.cmdQueue;
}
