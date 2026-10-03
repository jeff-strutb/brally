/* WHAT IT DOES: fetches a piece of information from a system object whose size
 * is not known in advance -- it asks once with no buffer to be told how big the
 * answer is, allocates that much, then asks again to have it filled in, and
 * hands the block to the caller. On any failure it releases the block and
 * reports the error instead. */
/* @implements 0x10036810 glide BrComGetAlloc
 * @cpp_kind free
 * @cpp_symbol _BrComGetAlloc
 *
 * cdecl, three arguments, 142 B (D3D twin 0x1003D180).  The DirectPlay SDK
 * samples' size-query helper, compiled as C++ against the C++ interface:
 * the method is a virtual call, so C1XX loads the vtable slot ONCE into ebp
 * for both calls, and the sample's own goto shape (FAILURE label last,
 * GlobalAllocPtr / GlobalFreePtr expanded) lays the out-of-memory arm
 * inline as the original does.  The C transcription in br_comgetalloc.c
 * could reach one or the other, not both (its notes keep the dead list).
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
extern "C" {
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
/* The original BrNameListInit is a thiscall ctor with no stack args (vtbl
 * and fill string are fixed); hide the port's 3-arg prototype. */
#define BrNameListInit BrNameListInit_port
#include "slice1_06.h"
#undef BrNameListInit
#undef BrOptSave
#undef BrOptAvailB

#include <stdlib.h>
#include <string.h>

/* Layout facts the original's arithmetic depends on. */
typedef char br06_assert_pendlist[
    (offsetof(BrPendList, count) == BR_PENDLIST_MAX * sizeof(void *)) ? 1 : -1];
typedef char br06_assert_namelist[
    (BR_NAMELIST_COUNT * BR_NAMELIST_STRIDE == 0x1964 * 4) ? 1 : -1];

/* ==========================================================================
 * 0x1003D180
 * ========================================================================== */

__declspec(dllimport) void *__stdcall GlobalAlloc(unsigned uFlags,
                                                  unsigned dwBytes);
__declspec(dllimport) void *__stdcall GlobalLock(void *hMem);
__declspec(dllimport) void *__stdcall GlobalHandle(void *pMem);
__declspec(dllimport) int   __stdcall GlobalUnlock(void *hMem);
__declspec(dllimport) void *__stdcall GlobalFree(void *hMem);

/* IDirectPlay2A as the C++ interface the original was built against: the
 * fetch is slot 21 (+0x54), GetPlayerName(idPlayer, lpData, lpdwDataSize). */
struct IBrDP {
    virtual long __stdcall s00(void) = 0;
    virtual long __stdcall s01(void) = 0;
    virtual long __stdcall s02(void) = 0;
    virtual long __stdcall s03(void) = 0;
    virtual long __stdcall s04(void) = 0;
    virtual long __stdcall s05(void) = 0;
    virtual long __stdcall s06(void) = 0;
    virtual long __stdcall s07(void) = 0;
    virtual long __stdcall s08(void) = 0;
    virtual long __stdcall s09(void) = 0;
    virtual long __stdcall s10(void) = 0;
    virtual long __stdcall s11(void) = 0;
    virtual long __stdcall s12(void) = 0;
    virtual long __stdcall s13(void) = 0;
    virtual long __stdcall s14(void) = 0;
    virtual long __stdcall s15(void) = 0;
    virtual long __stdcall s16(void) = 0;
    virtual long __stdcall s17(void) = 0;
    virtual long __stdcall s18(void) = 0;
    virtual long __stdcall s19(void) = 0;
    virtual long __stdcall s20(void) = 0;
    virtual long __stdcall GetPlayerName(void *id, void *pv, uint32_t *pcb) = 0;
};
int32_t BrComGetAlloc(BrDPlayObj *pObj, void *pParam, void **ppvOut)
{
    IBrDP *p = (IBrDP *)pObj;
    void *pv = NULL;
    uint32_t cb;
    int32_t hr;

    hr = p->GetPlayerName(pParam, NULL, &cb);
    if (hr != BR_COM_E_BUFFERTOOSMALL)
        goto fail;
    pv = GlobalLock(GlobalAlloc(0x42u, cb));
    if (pv == NULL) {
        hr = BR_COM_E_OUTOFMEMORY;
        goto fail;
    }
    hr = p->GetPlayerName(pParam, pv, &cb);
    if (hr < 0)
        goto fail;
    *ppvOut = pv;
    return 0;

fail:
    if (pv != NULL) {
        GlobalUnlock(GlobalHandle(pv));
        GlobalFree(GlobalHandle(pv));
    }
    return hr;
}

}
