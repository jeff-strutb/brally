/* slice1_10.c -- another module's packet, 0x10079550-0x10086A10.
 *
 * ONE function of the fourteen is game code (0x10079550). The other thirteen
 * are statically linked MSVC CRT and are deliberately NOT ported. They are
 * listed below with the evidence, because "it looked CRT-shaped" is not a
 * reason anyone can check later.
 *
 *   0x1007CC40  _cexit
 *       `_doexit(0, 0, 1)`. 0x1007CC50 is _doexit: it takes (code, quick,
 *       retcaller), calls _lockexit, walks the atexit table backwards, and
 *       ends in ExitProcess/TerminateProcess.
 *   0x1007CD10  _lockexit    -> `_lock(13)`, 13 == _EXIT_LOCK1
 *   0x1007CD20  _unlockexit  -> `_unlock(13)`
 *       0x10081490/0x10081510 are _lock/_unlock: free(0x1007D2E0) brackets
 *       its HeapFree with the same pair using 9 == _HEAP_LOCK.
 *   0x1007CE90  fopen
 *       `_fsopen(file, mode, 0x40)`; 0x40 == _SH_DENYNO. 0x1007CE50 is
 *       _fsopen: _getstream() / _openfile(4 args) / _unlock_str().
 *   0x1007CF00  getc   -> forwards its single argument to fgetc.
 *       0x1007CEB0 is fgetc: _lock_str, `if (--stream->_cnt < 0) _filbuf()`
 *       else `return *stream->_ptr++`, _unlock_str.
 *   0x1007D350  malloc
 *       `_nh_malloc(size, __newmode)` where __newmode is the global at
 *       0x118AC344. 0x1007D370 is _nh_malloc: clamps size 0 to 1, rejects
 *       size > 0xFFFFFFE0, retries via _callnewh.
 *   0x1007DE40  operator delete  -> `free(p)`, free being 0x1007D2E0
 *       (_lock(9) / __sbh_find_block / HeapFree). 511 call sites.
 *   0x1007DFE0  operator new     -> `_nh_malloc(size, 1)`. 84 call sites.
 *   0x1007E170  _chkstk / _alloca_probe
 *       The canonical stack-probe loop: touch one dword per 0x1000, move
 *       esp, then `push` the saved return address and `ret` to it.
 *   0x1007ECB0  _aulldiv   (unsigned 64/64, stdcall, `ret 0x10`)
 *   0x1007ED20  _allmul    (64x64 -> 64)
 *   0x1007FD10  _alldiv    (signed 64/64; sign-folds, then _aulldiv's body)
 *   0x10086A10  __initmbctable -> `_setmbcp(-3)`, -3 == _MB_CP_ANSI.
 *       0x10086710 is _setmbcp: _lock(25), GetCPInfo, rebuilds the 257-byte
 *       _mbctype table at 0x118AC488.
 *
 * Nothing above is Boss Rally code; porting it would mean porting a 1997 CRT.
 *
 * The real function follows. See slice1_10.h for how the subsystem was
 * identified and for the full list of gotchas.
 */
#include <stddef.h>

#include "slice1_10.h"

/* 0x10079550 */
void BrFfbShutdown(BrFfb *pFfb)
{
    BrDiObj *pObj;
    int count;

    /* 10079550-1007955B: dec, store, and branch on the SIGN of the result.
     * `jns` means "count >= 0 continues"; the negative path below is the
     * underflow clamp. */
    count = pFfb->initCount - 1;
    pFfb->initCount = count;
    if (count < 0) {
        /* 1007955D: clamp to 0 and RETURN. No teardown on this path -- see
         * gotcha 1 in the header. */
        pFfb->initCount = 0;
        return;
    }

    /* 10079568: the original re-loads the global rather than reusing the
     * register it just stored. Kept as a re-read for fidelity; with a single
     * thread it is the same test as `count != 0`. */
    if (pFfb->initCount != 0)
        return;

    /* 10079571: square-wave effect (0x118ABDFC) goes first. */
    pObj = pFfb->pEffectSquare;
    if (pObj != NULL) {
        pObj->pVtbl->pfnRelease(pObj);      /* call [ecx+8] */
        pFfb->pEffectSquare = NULL;
    }

    /* 1007958A: then the spring effect (0x118ABDEC). */
    pObj = pFfb->pEffectSpring;
    if (pObj != NULL) {
        pObj->pVtbl->pfnRelease(pObj);      /* call [edx+8] */
        pFfb->pEffectSpring = NULL;
    }

    /* 100795A3: finally the device: Unacquire, then Release. */
    pObj = pFfb->pDevice;
    if (pObj != NULL) {
        pObj->pVtbl->pfnUnacquire(pObj);    /* call [ecx+0x20] */

        /* 100795B2: the original RE-READS the global here and does not
         * re-test it against NULL. Reproduced verbatim -- adding a guard
         * would be a behaviour change, not a portability fix. */
        pObj = pFfb->pDevice;
        pObj->pVtbl->pfnRelease(pObj);      /* call [edx+8] */

        pFfb->pDevice = NULL;
    }
}
