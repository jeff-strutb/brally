/* br_comgetalloc.c -- COM blob fetch: BrComGetAlloc asks a DirectPlay object for a variable-size
 * blob (size query, global-heap allocation, fill) and hands it back.
 *
 * Filed out of the address batch slice1_06.c; its preamble is carried verbatim.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
#ifdef BR_MATCHING_BUILD
/* The original BrNameListInit is a thiscall ctor with no stack args (vtbl
 * and fill string are fixed); hide the port's 3-arg prototype. */
#define BrNameListInit BrNameListInit_port
#include "slice1_06.h"
#undef BrNameListInit
#else
#include "slice1_06.h"
#endif
#undef BrOptSave
#undef BrOptAvailB
#else
#include "slice1_06.h"
#endif

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
#ifdef BR_MATCHING_BUILD
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

/* WHAT IT DOES: fetch a blob whose size is not known in advance -- ask
 * once with no buffer, allocate that much, ask again, hand the block
 * back.  On failure the block is released and the error is reported. */
/* DEAD 2026-09-09 (all at 142 B RAW 1+1): NULL-compare orders and !pv; a
 * handle local at the alloc (12+12) or in cleanup (12+12); the GMEM
 * constant spelled as an OR; empty-statement padding; 0 <= hr; while(0!=0);
 * break as goto cleanup; pv = 0; a comment line between the declarations;
 * register hr; (void *)0; every slot in the TU (7 of 15 compile).  Corpus:
 * the byte-exact BrDPlayPump spells the same alloc with a guarded
 * assignment and NO jump -- a different control shape, not transplantable
 * over this function's goto-cleanup arm.
 * @t4-pass 0x10036810 1 2026-09-09 probes 11 bytes 142 insns 64 regions 2 rows 2 census yes  (hand, fn.py variants + corpus)
 * @t4-pass 0x10036810 2 2026-09-09 probes 10 bytes 142 insns 64 regions 2 rows 2 census yes  (position sweep + 3 spelling variants) */
/* @t3 0x10036810 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 142/142 insns 64/64 rows 1+1 regions 2 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * residue is allocation/scheduling: 1+1 classified rows, 2 masked regions;
 * every row pairs under t3.py's canonical classes.  Effort: 2 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 1 and 2);
 * crank candidates and scores in build/match/crank.log, dead probes in the
 * comment block above.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1003D180 d3d BrComGetAlloc */
int32_t BrComGetAlloc(BrDPlayObj *pObj, void *pParam, void **ppvOut)
{
    BrComGetFnStd pfn = (BrComGetFnStd)pObj->pVtbl->pfnGet;
    uint32_t      cb;
    void         *pv = NULL;
    int32_t       hr;

    /* cb is uninitialised: the original reuses pObj's stack slot as the size
     * out-param and never stores 0 into it. */
    /* Layout matches the source order -- [call1][alloc][OOM, jmp cleanup]
     * [call2, jge success][cleanup][success].  The OOM arm ends in an
     * explicit `goto cleanup` and the second call is the FALLTHROUGH
     * continuation, not an else arm (an if/else makes VC5 move the call2
     * block out-of-line past the cleanup); the success block lives after
     * the cleanup via its own label. */
    /* do-while(0) with break, NOT structured if-nesting: the loop body
     * stays contiguous, which is what recovers the original's size and its
     * OOM-arm `jmp` over the second call.
     * RESIDUE (1+1 regnorm, 53 masked B, T3a-layout): the original lays the
     * OOM arm INLINE (test;jne over it, `mov esi,OOM; jmp cleanup`) with
     * the second call after; every probed spelling (nested if/else, three
     * goto flattenings, else-arm, mixed break/goto) makes VC5 thread the
     * jump and move one arm out-of-line -- one je-vs-jne with the OOM arm
     * at the end.
     * 2026-09-03, and this narrows it to a genuine either/or: writing the
     * body as a PLAIN if/else (OOM the then-arm, the second call the else
     * arm, no do-while) DOES fix the polarity -- VC5 then emits the
     * original's `test edi,edi / jne <call2>` with the OOM arm falling
     * through.  But it will not also emit the original's `jmp cleanup`: it
     * places the join (cleanup) immediately after the OOM arm to save those
     * two bytes and sinks the call2 block past it, so the success test
     * flips from the original's `jge <success>` to `jl <cleanup>`.  Result
     * 140/142 B, regnorm 1+2 -- strictly worse than the do-while's
     * size-exact 1+1, so the do-while spelling stands.  The two residues are
     * mutually exclusive under VC5's join placement; DEAD, do not re-run:
     * plain if/else with an early `return 0`, the same with `goto success`,
     * if/else + `goto cleanup` in the OOM arm, and the literal transcription
     * `if (pv != NULL) goto call2;` (which VC5 inverts back to `je`, 2+3). */
    do {
        hr = pfn(pObj, pParam, NULL, &cb);
        if (hr != BR_COM_E_BUFFERTOOSMALL)
            break;
        /* GMEM_MOVEABLE|GMEM_ZEROINIT == 0x42. */
        pv = GlobalLock(GlobalAlloc(0x42u, cb));
        if (pv == NULL) {
            hr = BR_COM_E_OUTOFMEMORY;
            goto cleanup;
        }
        hr = pfn(pObj, pParam, pv, &cb);
        if (hr >= 0)
            goto success;
    } while (0);

cleanup:

    if (pv != NULL) {
        GlobalUnlock(GlobalHandle(pv));
        GlobalFree(GlobalHandle(pv));
    }
    return hr;

success:
    *ppvOut = pv;
    return 0;       /* the original discards hr here */
}
#else
/* WHAT IT DOES: the same two-step fetch, using calloc instead of a
 * Windows global heap block. */
/* port-only variant of BrComGetAlloc (matching build uses the #ifdef branch above) */
int32_t BrComGetAlloc(BrDPlayObj *pObj, void *pParam, void **ppvOut)
{
    BrComGetFn pfn = pObj->pVtbl->pfnGet;
    uint32_t   cb  = 0;
    void      *pv  = NULL;
    int32_t    hr;

    hr = pfn(pObj, pParam, NULL, &cb);
    if (hr == BR_COM_E_BUFFERTOOSMALL) {
        /* DEVIATION: GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT, cb) followed by
         * GlobalLock -> calloc; GlobalUnlock(GlobalHandle(p)) +
         * GlobalFree(GlobalHandle(p)) -> free. The original's caller
         * therefore receives a locked global handle's base pointer, not a CRT
         * allocation; anything that later passes it back to GlobalFree has to
         * be adjusted alongside this.
         *
         * Corner case, not reproduced: GlobalAlloc(GMEM_MOVEABLE, 0) yields a
         * handle to a discarded object and the following GlobalLock returns
         * NULL, so the original turns a zero-size result into
         * E_OUTOFMEMORY. calloc has no such rule; the substitution below
         * succeeds instead. No caller in this range asks for zero. */
        pv = calloc(cb ? cb : 1u, 1u);
        if (pv == NULL) {
            hr = BR_COM_E_OUTOFMEMORY;
        } else {
            hr = pfn(pObj, pParam, pv, &cb);
            if (hr >= 0) {
                *ppvOut = pv;
                return 0;   /* the original discards hr here */
            }
        }
    }

    if (pv != NULL) {
        free(pv);
    }
    return hr;
}
#endif
