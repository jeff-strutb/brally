/* n64-cflags: -O1 */
/* vigetcurrcontext.c -- libultra's current VI state
 * (io/vigetcurrcontext.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: The VI state being shown now. */
/* @implements 0x8026C060 tgr __osViGetCurrentContext */
__OSViContext *__osViGetCurrentContext(void)
{
	return __osViCurr;
}
