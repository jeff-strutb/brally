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

class BrDpList {
public:
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual long AddRow(long name, long kind, long live, const void *pCol,
                        long one);                      /* +0x10 */
    virtual void v5();
    virtual void v6();
    virtual void v7();
    virtual void v8();
    virtual void v9();
    virtual long SetRowData(void *pData, long cb, long who);   /* +0x28 */
};

extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_100361a0: prototype in br_funcs.h */
/* FUN_10036130: prototype in br_funcs.h */
/* 64-bit core: GlobalAlloc is declared by the platform headers */
/* 64-bit core: GlobalLock is declared by the platform headers */
}

struct BrGuid16 { long a, b, c, d; };

extern "C"
int __stdcall BrNetEnumSessionCb(void *pDesc, void *pUnused, unsigned flags,
                                 void *pCtx)
{
    void *pMem;
    long kind;
    unsigned char *pBase;

    (void)pUnused;
    pBase = (unsigned char *)(uintptr_t)(unsigned)g_brPAA29D4;
    if (pBase == 0)
        return 0;
    if ((flags & 1) != 0)
        return 0;

    if (DAT_10ac5bf0 != 0) {
        if (*(unsigned *)((char *)pDesc + 0x2c) < 8u
            && (*(unsigned char *)((char *)pDesc + 4) & 0x20) == 0) {
            kind = 1;
            *(unsigned char *)&flags = (unsigned char)kind;
        } else {
            kind = 0x11;
            *(unsigned char *)&flags = 0;
        }

        ((BrDpList *)(pBase + 0x3838))->AddRow(
            *(long *)((char *)pDesc + 0x30), kind, (long)flags,
            &(*(int *)&g_hot0), 1);

        pMem = GlobalLock(GlobalAlloc(0x42u, 0x10u));
        if (pMem == 0)
            return 1;
        *(BrGuid16 *)pMem = *(const BrGuid16 *)((const char *)pDesc + 8);

        ((BrDpList *)((unsigned char *)(uintptr_t)(unsigned)g_brPAA29D4
                      + 0x3838))->SetRowData(pMem, 0x10, -1);
    } else {
        /* The original materialises this only on the skip path. */
        pMem = (void *)(uintptr_t)flags;
    }
    FUN_100361a0(pMem, (void *)&BrWmHook36130, pCtx, 0);
    return 1;
}
