/* rcp_ps1.c: the display lists drawn by the GTE and GPU (to come) */
#include <stdint.h>
#include "plat.h"

extern uint32_t tgr_rcp_frames;
void tgr_rcp_task(uint32_t dl) { (void)dl; tgr_rcp_frames++; }
void tgr_dump_state(const char *path, uint32_t dl) { (void)path; (void)dl; }
