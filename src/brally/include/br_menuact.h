/* br_menuact.h -- menus: button callbacks that open a screen and wire Back.
 *
 * Responsibility: the front end.  A button was pressed; open the next
 * screen and point that screen's Back row at a leave routine so backing
 * out returns here.  Always report success.
 *
 * slice2_24.c holds the caption-column and lap-time pickers that share
 * the same menu object; they stay there because that file owns g_menu.
 */
#ifndef BR_MENUACT_H
#define BR_MENUACT_H

#include <stdint.h>

/* 0x10044010  play-mode 0, then open the next screen. */
int BrHook_10044010(void *pCtl);
/* 0x10045780..0x100458C0  open + wire Back; differ only in which pair. */
int BrHook_10045780(void *pCtl);
int BrHook_100457A0(void *pCtl);
int BrHook_10045800(void *pCtl);
int BrHook_10045820(void *pCtl);
int BrHook_10045840(void *pCtl);
int BrHook_10045860(void *pCtl);
int BrHook_100458C0(void *pCtl);

extern uint32_t *g_AA29F4;
extern uint32_t *g_AA29C8;
extern uint32_t  g_AA287C;

#endif
