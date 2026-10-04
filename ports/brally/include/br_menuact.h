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
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif

#include <stdint.h>

/* 0x10044010  play-mode 0, then open the next screen. */
/* BrHook_10044010: prototype in br_funcs.h */
/* 0x10045780..0x100458C0  open + wire Back; differ only in which pair. */
/* BrHook_10045780: prototype in br_funcs.h */
/* BrHook_100457A0: prototype in br_funcs.h */
/* BrHook_10045800: prototype in br_funcs.h */
/* BrHook_10045820: prototype in br_funcs.h */
/* BrHook_10045840: prototype in br_funcs.h */
/* BrHook_10045860: prototype in br_funcs.h */
/* BrHook_100458C0: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif
#endif
