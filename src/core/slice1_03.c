/* slice1_03.c -- decompiled from BRD3D.dll, pass-03 packet.
 * See slice1_03.h for the map of what is in here and why.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_03.h"

#include <stdio.h>

/* 0x1000BEA0 BrAppMsgDispatch, with the hooks table and accessor it shares
 * a file-static with, now lives in src/core/startup/br_appmsg.c. */

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
