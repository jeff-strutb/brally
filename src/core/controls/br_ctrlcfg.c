/* br_ctrlcfg.c -- bringing the controller configuration up.
 *
 * RESPONSIBILITY: reading what the player is doing -- specifically the block
 * that records which key, button or axis each action is bound to.
 *
 * Moved here out of src/core/slice3_42.c (an address batch, not a module).
 */
#include "slice3_42.h"
#include "br_objlife.h"   /* BrAtexit_10069A70 -- 0x10069A70 */

#ifdef BR_MATCHING_BUILD

/* WHAT IT DOES: initialize the controller-config subsystem and register its atexit handler. */
/* @implements 0x10062AC0 glide BrCtrlCfgBoot */

int BrCtrlCfgBoot(void)

{
  BrCtrlCfgInitGlobal();
  BrAtexit_10069A70();
  return;
}

#endif /* BR_MATCHING_BUILD */
