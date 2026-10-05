/* n64-cflags: -O1 */
/* ai.c -- libultra's audio interface busy test (io/ai.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Whether the AI's buffer FIFO is full. */
/* @implements 0x8026CE20 tgr __osAiDeviceBusy */
int __osAiDeviceBusy(void)
{
	register s32 status = IO_READ(AI_STATUS_REG);

	if (status & AI_STATUS_FIFO_FULL)
		return TRUE;
	else
		return FALSE;
}
