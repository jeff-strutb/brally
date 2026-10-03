/* WHAT IT DOES: DirectPlay EnumSessions callback. On a live session (not a
 * timeout) it tells the lobby list widget about the game, copies the 16-byte
 * session id, and hands that id to the join helper. Returns 1 to keep
 * enumerating, 0 if there is no lobby object or DirectPlay timed out. */
/* @implements 0x10036220 glide BrNetEnumSessionCb
 * @cpp_symbol _BrNetEnumSessionCb@16
 *
 * The C transcription (br_dplayenum.c) is PARKED T2 on exactly the wall its
 * own header names: the lobby-list object's two calls are VIRTUAL THISCALL
 * with stack arguments and callee cleanup (`mov edx,[ecx+0x3838]; ...;
 * call [edx+0x10]`, this in ecx, five pushes) -- the fastcall fake needs
 * five wrapper structs, which cost an ebp frame and misplace the flag-byte
 * store.  This file is the calling TU's real shape: a class with declared
 * virtuals at slots 4 (+0x10) and 10 (+0x28), the session base kept in a
 * local across the first call (the original holds it in ecx from the guard),
 * and a `long` kind local so the flag byte stores from AL. */
#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
#include <stdint.h>

#include "br_ui.h"   /* BrUiCtl_, BrTextList */
#include "dplay.h"   /* DPSESSIONDESC2 */

/* 64-bit core: the session list is the control's canonical BrTextList (+0x3838
 * on i386), called through its own vtable: slot 4 appends a row
 * (BrSlotAdd_10054A30), slot 10 attaches a blob to it (BrTextListSetBlob). */
typedef int32_t (*BrListSetBlobFn)(BrTextList *pThis, const void *pData,
                                   int32_t cb, int32_t index);

extern "C"
int __stdcall BrNetEnumSessionCb(void *pv, void *pUnused, unsigned flags,
                                 void *pCtx)
{
    const DPSESSIONDESC2 *pDesc = (const DPSESSIONDESC2 *)pv;
    void *pMem;
    int32_t kind;
    BrUiCtl_ *pCtl;
    BrTextList *pList;

    (void)pUnused;
    pCtl = (BrUiCtl_ *)g_brPAA29D4;
    if (pCtl == 0)
        return 0;
    if ((flags & 1) != 0)
        return 0;

    if (DAT_10ac5bf0 != 0) {
        if (pDesc->dwCurrentPlayers < 8u
            && (pDesc->dwFlags & 0x20) == 0) {
            kind = 1;
            *(unsigned char *)&flags = (unsigned char)kind;
        } else {
            kind = 0x11;
            *(unsigned char *)&flags = 0;
        }

        pList = &pCtl->list;
        pList->pVtbl->f10(pList, pDesc->lpszSessionNameA, kind, (int32_t)flags,
                          &(*(int *)&g_hot0), 1);

        pMem = GlobalLock(GlobalAlloc(0x42u, 0x10u));
        if (pMem == 0)
            return 1;
        memcpy(pMem, &pDesc->guidInstance, 0x10);

        pList = &((BrUiCtl_ *)g_brPAA29D4)->list;
        ((BrListSetBlobFn)pList->pVtbl->f28)(pList, pMem, 0x10, -1);
    } else {
        /* The original materialises this only on the skip path. */
        pMem = (void *)(uintptr_t)flags;
    }
    FUN_100361a0(pMem, (void *)&BrWmHook36130, pCtx, 0);
    return 1;
}
