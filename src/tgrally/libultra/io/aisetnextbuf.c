/* n64-cflags: -O1 */
/* aisetnextbuf.c -- libultra's next audio buffer (io/aisetnextbuf.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Queue the next audio buffer (-1 while the AI is busy),
 * working round the AI's DMA bug for buffers ending on an 8K boundary
 * inside a 16K page. */
/* @implements 0x80268470 tgr osAiSetNextBuffer */
s32 osAiSetNextBuffer(void *bufPtr, u32 size)
{
	static u8 hdwrBugFlag = 0;
	char *bptr = bufPtr;

	if (hdwrBugFlag != 0)
		bptr -= 0x2000;

	if ((((u32)bufPtr + size) & 0x3fff) == 0x2000)
		hdwrBugFlag = 1;
	else
		hdwrBugFlag = 0;

	if (__osAiDeviceBusy())
		return -1;

	IO_WRITE(AI_DRAM_ADDR_REG, osVirtualToPhysical(bptr));
	IO_WRITE(AI_LEN_REG, size);
	return 0;
}
