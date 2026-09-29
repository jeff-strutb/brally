/* n64-cflags: -O1 */
/* viswapbuf.c -- libultra's frame buffer swap (io/viswapbuf.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Show a new frame buffer from the next retrace. */
/* @implements 0x80264AD0 tgr osViSwapBuffer */
void osViSwapBuffer(void *frameBufPtr)
{
	u32 saveMask = __osDisableInt();

	__osViNext->framep = frameBufPtr;
	__osViNext->state |= VI_STATE_BUFFER_UPDATED;
	__osRestoreInt(saveMask);
}
