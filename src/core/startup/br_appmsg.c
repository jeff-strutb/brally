/* br_appmsg.c -- startup: the application message dispatcher.
 *
 * Filed out of the address batch slice1_03.c; the preamble is that file's.
 * The hooks table and its accessor come along because they are the port
 * arm's only users of the file-static below.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_03.h"

/* =====================================================================
 * 3. Glue
 * ===================================================================== */

static BrAppMsgHooks g_appMsg;

BrAppMsgHooks *BrAppMsgGetHooks(void)
{
    return &g_appMsg;
}

/* 0x1000BEA0
 *
 * The original is two dispatchers stacked, and both collapse:
 *
 *  - ids <= 0x21 run a compare chain (`cmp 0x21 / je`, `sub 3 / je`,
 *    `sub 2 / jne`) whose only non-returning outcome is id == 5.
 *
 *  - ids in [0x31, 0x107] index a 0xD7-byte selector table at 0x1000BF20
 *    into a 6-entry jump table at 0x1000BF08. Read out of the DLL, five of
 *    the six jump targets are the same bare `ret` at 0x1000BF04, and the
 *    selector byte 4 -- the only one reaching real code at 0x1000BEE6 --
 *    appears at exactly ONE index, 0xD6, i.e. id 0x107.
 *
 * So the whole table apparatus is equivalent to a single `if`. */
/* WHAT IT DOES: routes an application message to whoever handles it. The
 * original looks like a wide switch with a jump table, but the tables decode
 * to almost nothing: only two message numbers do anything at all, and
 * everything else simply returns. Two of the five arguments are passed by
 * every caller and never read. */
/* @implements 0x1000BEA0 d3d BrAppMsgDispatch */
#ifdef BR_MATCHING_BUILD
/* The original really is the wide switch: empty DPSYS_* labels (same `ret`
 * as default, `return` not `break` so each keeps its own jump-table group)
 * preserve the compare chain for ids <= 0x21 and the two-level table for
 * 0x31..0x107 -- the proven 0x10009530 sibling shape. The gate is the real
 * global (glide 0x100ABAA0) read directly, and both live cases call their
 * handlers unconditionally -- no hook-pointer guards, no NULL guard. */
extern int32_t DAT_100abaa0;
extern void BrSub10003580(void *pv1, int32_t f0C, int32_t f10,
                          int32_t f08, void *pv5);      /* glide 0x100038F0 */
extern void BrSub10005FE0(uint32_t idPlayer);           /* glide 0x10006350 */

void BrAppMsgDispatch(void *pv1, const BrAppMsg *pMsg, void *pv3, void *pv4,
                      void *pv5)
{
    uint32_t id = (uint32_t)pMsg->id;    /* `ja`: the switch is unsigned */

    (void)pv3;    /* pushed by every caller, never read by the original */
    (void)pv4;

    switch (id) {
    case 3u:
        return;
    case 5u:
        if (DAT_100abaa0 == 0)
            BrSub10005FE0((uint32_t)pMsg->f08);
        return;
    case 0x21:
        return;
    case 0x31:
        return;
    case 0x101:
        return;
    case 0x102:
        return;
    case 0x103:
        return;
    case 0x107:
        BrSub10003580(pv1, pMsg->f0C, pMsg->f10, pMsg->f08, pv5);
        return;
    }
}
#else
void BrAppMsgDispatch(void *pv1, const BrAppMsg *pMsg, void *pv3, void *pv4,
                      void *pv5)
{
    int32_t id;

    (void)pv3;    /* pushed by every caller, never read by the original */
    (void)pv4;

    if (pMsg == NULL)
        return;   /* DEVIATION: the original dereferences unconditionally */

    id = pMsg->id;

    if (id > 0x21) {
        if ((uint32_t)(id - 0x31) > 0xD6u)
            return;
        if (id != 0x107)
            return;
        if (g_appMsg.pfnMsg107 != NULL)
            g_appMsg.pfnMsg107(pv1, pMsg->f0C, pMsg->f10, pMsg->f08, pv5);
        return;
    }

    if (id != 5)
        return;
    if (g_appMsg.f0AC300 != 0)
        return;
    if (g_appMsg.pfnMsg5 != NULL)
        g_appMsg.pfnMsg5(pMsg->f08);
}
#endif
