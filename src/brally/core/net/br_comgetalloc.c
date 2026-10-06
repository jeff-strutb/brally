/* br_comgetalloc.c -- COM blob fetch: BrComGetAlloc asks a DirectPlay object for a variable-size
 * blob (size query, global-heap allocation, fill) and hands it back.
 *
 * Filed out of the address batch slice1_06.c; its preamble is carried verbatim.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
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

/* WHAT IT DOES: fetches a piece of information from a system object whose size
 * is not known in advance -- it asks once with no buffer to be told how big the
 * answer is, allocates that much, then asks again to have it filled in, and
 * hands the block to the caller. On any failure it releases the block and
 * reports the error instead. */
/* COM methods are stdcall; the header's BrComGetFn is cdecl. A local
 * stdcall typedef is what removes the `add esp, 10h` the cdecl form emits
 * after each call. __declspec(dllimport) is what emits `call dword ptr
 * [IAT]` rather than a direct `call` thunk. */
typedef int32_t (__stdcall *BrComGetFnStd)(void *pThis, void *pParam,
                                           void *pvBuf, uint32_t *pcb);

__declspec(dllimport) void *__stdcall GlobalAlloc(unsigned uFlags,
                                                  unsigned dwBytes);
__declspec(dllimport) void *__stdcall GlobalLock(void *hMem);
__declspec(dllimport) void *__stdcall GlobalHandle(void *pMem);
__declspec(dllimport) int   __stdcall GlobalUnlock(void *hMem);
__declspec(dllimport) void *__stdcall GlobalFree(void *hMem);

/* 0x10036810 BrComGetAlloc is byte-exact in the C++ lane,
 * src/brally/core/net/BrComGetAlloc_10036810.cpp: the DirectPlay SDK samples'
 * size-query helper built against the C++ interface.  The C transcription's
 * dead list, kept for the record: NULL-compare orders and !pv; a handle
 * local at the alloc or in cleanup; the GMEM constant as an OR; plain
 * if/else (fixes the OOM-arm polarity but sinks the second call past the
 * join), three goto flattenings, do-while(0) with break (size-exact, the
 * OOM arm out of line).  Under C the second call reloads the vtable slot;
 * only a stdcall pointer local cached it, and that local is what pinned
 * the arm layout. */
