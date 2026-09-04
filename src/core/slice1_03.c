/* slice1_03.c -- decompiled from BRD3D.dll, pass-03 packet.
 * See slice1_03.h for the map of what is in here and why.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_03.h"

#include <stdio.h>

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

static BrComHolder  *g_pComHolder;      /* 0x10A9D008 */
static BrComLockHooks g_comLock;

BrComHolder **BrComGetHolderSlot(void)
{
    return &g_pComHolder;
}

BrComLockHooks *BrComGetLockHooks(void)
{
    return &g_comLock;
}

/* 0x1000C4A0 */
/* WHAT IT DOES: hands a held object back to whatever owns it, through that
 * object's own release entry point, and then forgets the argument it was
 * holding. Nothing happens unless the holder, the object and the argument
 * are all present. Note the holder is looked up again after the call, so a
 * callback that swaps it out clears the new holder rather than the old one. */
/* port-only body; Glide match is src/core/generated/0x100099D0.c */
int BrComHolderRelease(void)
{
    BrComHolder *pHolder = g_pComHolder;
    BrComObj    *pObj;
    void        *pArg;
    int          rc;

    if (pHolder == NULL)
        return 0;
    pObj = pHolder->pObj;
    if (pObj == NULL)
        return 0;
    pArg = pHolder->pArg;
    if (pArg == NULL)
        return 0;

    rc = pObj->pVtbl->pfn24(pObj, pArg);

    /* the holder global is re-read here, exactly as the original does --
     * so a callback that swaps 0x10A9D008 clears the NEW holder's +0x08 */
    g_pComHolder->pArg = NULL;
    return rc;
}

/* 0x1000C4D0 */
int BrComCallLocked68(BrComObj *pThis, void *a2, void *a3, void *a4,
                      void *a5, void *a6)
{
    int rc;

    if (g_comLock.pfnEnter != NULL)
        g_comLock.pfnEnter(g_comLock.pCrit);

    /* DEVIATION: the original reads pThis->pVtbl with no NULL check. */
    rc = (pThis != NULL) ? pThis->pVtbl->pfn68(pThis, a2, a3, a4, a5, a6) : 0;

    if (g_comLock.pfnLeave != NULL)
        g_comLock.pfnLeave(g_comLock.pCrit);

    return rc;
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int *DAT_106e7710;
extern int DAT_106e72e8;
extern int DAT_106e7718;
extern int DAT_106e79b0;
extern int DAT_106ed6b0;
extern int DAT_1184c478;
extern int DAT_100ba2d0;
extern int DAT_10396efc;
int FUN_10011300(int, int, int, int, int);
int FUN_10010fb0(int);

/* WHAT IT DOES: emit the fixed run of display-list commands that puts the
 * renderer into the standard drawing mode for a pass -- pipeline sync,
 * render mode, blend colour and the colour-combiner setup, in that order. */
/* @implements 0x100119C0 glide FUN_100119c0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_100119c0(int param_1, int param_2)
{
  int *p_;

  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xe7000000; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba001402; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xbb000001; p_[1] = 0xffffffff; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba000c02; p_[1] = DAT_106e72e8; }
  p_ = DAT_106e7710;
  DAT_106e7710 = DAT_106e7710 + 2;
  BrRdpSetCombineLERP(p_, 0, 0, 0, 0x3eb, 0x3e9, 0, 0x3eb, 0, 0, 0, 0, 0x3eb, 0x3e9, 0, 0x3eb, 0);
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = DAT_1184c478 & 0xffffff | 0xdc000000; p_[1] = 1; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xfd900000; p_[1] = (int)&DAT_100ba2d0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xf5900000; p_[1] = 0x7018060; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xe6000000; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xf3000000; p_[1] = 0x77ff100; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xe7000000; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xf5881000; p_[1] = 0x18060; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xf2000000; p_[1] = 0xfc0fc; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba000e02; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba001301; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xb9000201; p_[1] = 4; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba000602; p_[1] = 0xc0; }
  if (DAT_106ed6b0 != 0) {
    { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba000402; p_[1] = 0x80; }
    { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xb900031d; p_[1] = 0x504b50; }
    FUN_10011300(param_1, DAT_10396efc & 0xffff, 0xe0, 0xe0, 0xff);
  }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xe7000000; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba001301; p_[1] = 0x80000; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xb9000201; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba000602; p_[1] = DAT_106e7718; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba000402; p_[1] = DAT_106e79b0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xb9000002; p_[1] = 1; }
  FUN_10010fb0(param_2);
}

#endif /* BR_MATCHING_BUILD */
