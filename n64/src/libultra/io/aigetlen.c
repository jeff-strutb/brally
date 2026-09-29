/* n64-cflags: -O1 */
/* aigetlen.c -- libultra's audio buffer bytes left (io/aigetlen.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: The bytes left in the audio buffer playing now. */
/* @implements 0x80268530 tgr osAiGetLength */
u32 osAiGetLength(void)
{
	return IO_READ(AI_LEN_REG);
}
