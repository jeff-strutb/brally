/* n64-cflags: -O1 */
/* spsetstat.c -- libultra's RSP status write (io/spsetstat.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Write the RSP status register. */
/* @implements 0x8026B850 tgr __osSpSetStatus */
void __osSpSetStatus(u32 data)
{
	IO_WRITE(SP_STATUS_REG, data);
}
