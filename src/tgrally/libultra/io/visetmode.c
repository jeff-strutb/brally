/* n64-cflags: -O1 */
/* visetmode.c -- libultra's video mode change (io/visetmode.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Switch the video mode at the next retrace. */
/* @implements 0x80264A60 tgr osViSetMode */
void osViSetMode(OSViMode *modep)
{
	register u32 saveMask = __osDisableInt();

	__osViNext->modep = modep;
	__osViNext->state = VI_STATE_MODE_UPDATED;
	__osViNext->control = __osViNext->modep->comRegs.ctrl;
	__osRestoreInt(saveMask);
}
