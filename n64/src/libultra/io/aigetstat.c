/* n64-cflags: -O1 */
/* aigetstat.c -- libultra's audio interface status (io/aigetstat.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: The AI status register. */
/* @implements 0x80268520 tgr osAiGetStatus */
u32 osAiGetStatus(void)
{
	return IO_READ(AI_STATUS_REG);
}
