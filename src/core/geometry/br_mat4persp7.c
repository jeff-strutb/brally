/* br_mat4persp7.c -- geometry: camera projection.
 *
 * BrMat4Perspective7 builds a perspective projection matrix from a field of
 * view, aspect ratio and near/far planes (the seven-argument form; the scale
 * argument is passed through to BrMat4Frustum and unused).
 *
 * Filed out of the address batch slice4_50.c, whose preamble is carried
 * verbatim below.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "slice4_50.h"
#include "br_gamestep.h"   /* 0x10034C51 == BRGlide 0x1002E302 -- one slot, one owner */

#ifdef BR_MATCHING_BUILD
/* Orig inlines KERNEL32 IAT WaitForSingleObject / ReleaseMutex (FF 15). */
__declspec(dllimport) int __stdcall WaitForSingleObject(void *, unsigned int);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);
#endif

/* ==========================================================================
 * Cross-slice callees. Each is already declared, with this exact signature,
 * by the header named beside it; repeated here rather than including those
 * headers so that this translation unit stays free of their type models.
 * ========================================================================== */

/* XSLICE 0x10035BBA -- slice2_19.h */
extern void     BrLogSet(void *p);
/* XSLICE 0x10072820 -- slice1_08.h */
extern int32_t  BrSndPlayGroup(int32_t group, uint32_t packed, int32_t loop);
/* XSLICE 0x1007DFE0 -- slice2_26.h / slice3_33.h. operator new; does NOT
 * zero the block (contract). */
extern void    *BrOperatorNew(uint32_t cb);
/* XSLICE 0x100419D0 -- slice2_26.h */
extern void     BrExt_100419D0(void *p);
/* XSLICE 0x10005470 -- slice2_12.h. The original reads its two operands from
 * 0x10ACEDB0 and 0x100B36FC; that port takes them as parameters. */
#ifdef BR_MATCHING_BUILD
/* Orig reads 0x10ACEDB0 / 0x100B36FC from inside the callee -- no args. */
extern uint32_t BrEntityCountActive(void);
#else
extern uint32_t BrEntityCountActive(const void *pvRecords, int32_t cRecords);
#endif
/* XSLICE 0x1000C670 -- slice2_13.h. 0xFFFF is its failure sentinel. */
extern uint32_t BrDPlayGetCurrentPlayers(void);
/* DEVIATION -- slice1_02.h. The original inlines KERNEL32
 * WaitForSingleObject(h, INFINITE) / ReleaseMutex(h); that header already
 * routes the identical pattern through these two hooks. */
extern void     BrNetMutexLock(void *hMutex);
extern void     BrNetMutexUnlock(void *hMutex);

/* ==========================================================================
 * 4. Projection
 * ========================================================================== */

/* 0x10030930 */
/* WHAT IT DOES: sets up the camera lens -- how wide a view the player sees
 * and how near and far things can be before they are cut off. One of the
 * seven values it is handed, a scale, is passed along and then never looked
 * at by anything. */
/* @implements 0x10030930 d3d BrMat4Perspective7 */
/* @n64 0x80260E30 located */
int BrMat4Perspective7(BrMat4 *pM, uint16_t *pPerspNorm,
                       float fovyDegrees, float aspect,
                       float n, float f, float scale)
{
#ifdef BR_MATCHING_BUILD
    /* pi/360 as a double so the half-angle multiply is `fmul qword`. fptan,
     * two fchs, eight-arg call (scale is pushed and unused by Frustum).
     * Return is the perspNorm pointer, not Frustum's status.
     *
     * `nh` is a NAMED local (2026-09-04): with `-h` inline in the call the
     * function is 8 B and four `fxch` short -- VC5 pops fptan's 1.0 before
     * the multiply and homes h in fovy's dead slot, where the original
     * multiplies first, pops after, and homes h in n's slot.  Naming -h
     * gives size- and instruction-exact output; naming -w as well is
     * inert, naming -w alone is worse (15).
     * The last region (w's store/load ordered before `push eax`) fell to
     * the explicit `(float)` cast on the w product, 2026-09-09: the cast
     * pins the rounding store next to the multiply, so w's bits are in ecx
     * before h is pushed.  (Dead first: every order of the nh/nw statements
     * and declarations, `aspect * h`, inline spellings, a double ty,
     * copied scalars, per-part casts on h/ty, 0.0f-h, arg tweaks, every
     * slot in the TU.) */
    float ty, h, w, nh;
    ty = (float)tan((double)fovyDegrees * 0.0087266462599716477);
    h = n * ty;
    w = (float)(h * aspect);
    nh = -h;
    ((int (*)(BrMat4 *, float, float, float, float, float, float, float))BrMat4Frustum)
        (pM, -w, w, nh, h, n, f, scale);
    *pPerspNorm = 1;
    return (int)pPerspNorm;
#else
    (void)scale;
    return BrMat4Perspective(pM, (unsigned short *)pPerspNorm,
                             fovyDegrees, aspect, n, f);
#endif
}

