/* br_menuact.c -- menus.  See br_menuact.h. */
#include "br_menuact.h"

#include <stdint.h>

#ifdef BR_MATCHING_BUILD
extern uint32_t g_AA287C;
extern uint32_t *g_AA29F4;
extern uint32_t *g_AA29C8;
void BrExt_10043E70(void *);
void BrExt_10045BC0(void *);
void BrExt_100451E0(void *);
void BrExt_10046790(void);
void BrExt_10046750(void);
void BrExt_10046910(void);
void BrExt_10046950(void);
void BrExt_100469F0(void);
void BrExt_10046A30(void);
void BrExt_10046BB0(void);
void BrExt_10043E70(void *p) { (void)p; }
void BrExt_10045BC0(void *p) { (void)p; }
void BrExt_100451E0(void *p) { (void)p; }
void BrExt_10046790(void) {}
void BrExt_10046750(void) {}
void BrExt_10046910(void) {}
void BrExt_10046950(void) {}
void BrExt_100469F0(void) {}
void BrExt_10046A30(void) {}
void BrExt_10046BB0(void) {}
#else
uint32_t  g_AA287C;
uint32_t *g_AA29F4;
uint32_t *g_AA29C8;
void BrExt_10043E70(void *p);
void BrExt_10045BC0(void *p);
void BrExt_100451E0(void *p);
void BrExt_10046790(void);
void BrExt_10046750(void);
void BrExt_10046910(void);
void BrExt_10046950(void);
void BrExt_100469F0(void);
void BrExt_10046A30(void);
void BrExt_10046BB0(void);
#endif

/* WHAT IT DOES: the first play-mode button.  Records "mode 0" and opens
 * the next screen.  Always reports success. */
/* @implements 0x10044010 d3d BrHook_10044010 */
int BrHook_10044010(void *p)
{
    g_AA287C = 0;
    BrExt_10043E70(p);
    return 1;
}

/* WHAT IT DOES: a menu button was pressed.  Open the matching screen,
 * then point that screen's Back row at a leave routine so backing out
 * returns here.  Always reports success. */
/* @implements 0x100457A0 d3d BrHook_100457A0 */
int BrHook_100457A0(void *p)
{
    BrExt_10045BC0(p);
    g_AA29F4[2] = (uint32_t)(uintptr_t)&BrExt_10046790;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045780 d3d BrHook_10045780 */
int BrHook_10045780(void *p)
{
    BrExt_100451E0(p);
    g_AA29C8[2] = (uint32_t)(uintptr_t)&BrExt_10046750;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045800 d3d BrHook_10045800 */
int BrHook_10045800(void *p)
{
    BrExt_100451E0(p);
    g_AA29C8[2] = (uint32_t)(uintptr_t)&BrExt_10046910;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045820 d3d BrHook_10045820 */
int BrHook_10045820(void *p)
{
    BrExt_10045BC0(p);
    g_AA29F4[2] = (uint32_t)(uintptr_t)&BrExt_10046950;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045840 d3d BrHook_10045840 */
int BrHook_10045840(void *p)
{
    BrExt_100451E0(p);
    g_AA29C8[2] = (uint32_t)(uintptr_t)&BrExt_100469F0;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045860 d3d BrHook_10045860 */
int BrHook_10045860(void *p)
{
    BrExt_10045BC0(p);
    g_AA29F4[2] = (uint32_t)(uintptr_t)&BrExt_10046A30;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x100458C0 d3d BrHook_100458C0 */
int BrHook_100458C0(void *p)
{
    BrExt_100451E0(p);
    g_AA29C8[2] = (uint32_t)(uintptr_t)&BrExt_10046BB0;
    return 1;
}
