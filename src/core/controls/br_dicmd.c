/* br_dicmd.c -- controls: pushing settings and effects at a DirectInput device.
 *
 * Responsibility: reading what the player is doing. Everything here is a thin
 * command wrapper -- it fills one Windows parameter block and hands it to the
 * device or effect through its vtable. Two set a device property (an axis
 * range, or a single-number setting like the dead zone); two send a
 * force-feedback effect its new parameters and start it.
 *
 * Moved out of src/core/slice3_45.c (an address batch) unchanged. The
 * preamble below is carried over verbatim from that file, including the
 * matching-build renames that have nothing to do with this code: they decide
 * the set of names the translation unit sees, and trimming them changes the
 * compiler's view of the code.
 *
 * The state these read -- g_brFfb, g_brDiEffSpring, g_brDiEffSquare,
 * g_brDiSpringCond, g_br18ABDBC, g_br0BD438 and the g_brB4E1xx flags -- all
 * has external linkage and its definitions stay in slice3_45.c, reached here
 * through the extern declarations in slice3_45.h. Nothing is copied.
 */

#include <math.h>
#include <string.h>

#include "br_match.h"
#ifdef BR_MATCHING_BUILD
/* Header is cdecl (this, x, y, z). Original is thiscall with ret 0xC. */
#define BrEntSetPos BrEntSetPos_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* The entity setters are thiscall with three stack floats; hide the
 * port's cdecl prototypes so the twins can carry the fastcall shape. */
#define BrEntSetMatrix      BrEntSetMatrix_port
#define BrEntSetVel         BrEntSetVel_port
#define BrEntSetAngVel      BrEntSetAngVel_port
#define BrEntSetOrientation BrEntSetOrientation_port
#define BrEntSetHeading     BrEntSetHeading_port
#include "slice3_45.h"
#undef BrEntSetMatrix
#undef BrEntSetVel
#undef BrEntSetAngVel
#undef BrEntSetOrientation
#undef BrEntSetHeading
#else
#include "slice3_45.h"
#endif
#ifdef BR_MATCHING_BUILD
#undef BrEntSetPos
#endif

#ifndef BR_MATCHING_BUILD
/* PRIVATE COPIES of slice3_45.c's file-static BrDiDev and BrDiEff, verbatim.
 *
 * Why copies are correct here, and why they are not a shared-state hazard:
 * each is a pure, stateless cast of a pointer to a wider vtable type, not a
 * static variable -- two copies cannot drift into two different values. The
 * tree already does exactly this with BrFtol, which slice1_02.c and
 * slice2_12.c each define privately for the same reason, and with
 * slice3_42.c's BrCtrlProfileIndex, copied into controls/br_ctrlquery.c.
 *
 * Why they are #ifndef BR_MATCHING_BUILD: only the PORT arm of the two macros
 * below calls them. The matching arm casts the vtable inline, because the
 * originals reach pfnSetProperty / pfnSetParameters through a __stdcall
 * function-pointer type that the port's cdecl vtable declarations do not
 * carry -- so in the matching build these must not be in scope at all.
 * Without the copies the port arm compiles to a C4013 implicit declaration
 * and leaves unresolved externals `_BrDiDev` / `_BrDiEff` in the object: a
 * link failure no sweep can see.
 *
 * slice3_45.c keeps its own definitions; its remaining users (the device
 * teardown paths, and BrFfbUpdateSpring's pfnStop/pfnStart) never leave that
 * file. */
static const BrDiDevVtbl *BrDiDev(BrDiObj *p)
{
    return (const BrDiDevVtbl *)(const void *)p->pVtbl;
}
static const BrDiEffVtbl *BrDiEff(BrDiObj *p)
{
    return (const BrDiEffVtbl *)(const void *)p->pVtbl;
}
#endif

#ifdef BR_MATCHING_BUILD
typedef long (__stdcall *BrDiSetParamsFn)(BrDiObj *, const BrDiEffect *, uint32_t);
typedef long (__stdcall *BrDiSetPropFn)(BrDiObj *, uint32_t, const void *);
#define BR_DI_SETPARAMS(p, eff, flags) \
    ((BrDiSetParamsFn)(((const BrDiEffVtbl *)(const void *)(p)->pVtbl)->pfnSetParameters))((p), (eff), (flags))
#define BR_DI_SETPROP(p, prop, pdiph) \
    ((BrDiSetPropFn)(((const BrDiDevVtbl *)(const void *)(p)->pVtbl)->pfnSetProperty))((p), (prop), (pdiph))
#else
#define BR_DI_SETPARAMS(p, eff, flags) \
    (BrDiEff(p)->pfnSetParameters((p), (eff), (flags)))
#define BR_DI_SETPROP(p, prop, pdiph) \
    (BrDiDev(p)->pfnSetProperty((p), (prop), (pdiph)))
#endif

/* 0x10078C30 */
/* WHAT IT DOES: tells Windows what range of numbers one axis of a controller
 * should report -- how far left and right count as full deflection. A small
 * convenience wrapper around the awkward Windows call; the game uses it to
 * scale the wheel and stick into the range it wants. */
/* @implements 0x10078C30 d3d BrDiSetPropRange */
long BrDiSetPropRange(BrDiObj *pDev, uint32_t prop, uint32_t dwObj,
                      uint32_t dwHow, int32_t lMin, int32_t lMax)
{
    BrDiPropRange r;

    r.dwSize       = 0x18u;
    r.dwHeaderSize = 0x10u;
    r.dwObj        = dwObj;
    r.dwHow        = dwHow;
    r.lMin         = lMin;
    r.lMax         = lMax;

    return BR_DI_SETPROP(pDev, prop, &r);
}

/* 0x10078C80 */
/* WHAT IT DOES: the same, for controller settings that are a single number
 * rather than a range -- the dead zone and the wheel's self-centring are the
 * two the game sets this way. */
/* @implements 0x10078C80 d3d BrDiSetPropDword */
long BrDiSetPropDword(BrDiObj *pDev, uint32_t prop, uint32_t dwObj,
                      uint32_t dwHow, uint32_t dwData)
{
    BrDiPropDword d;

    /* Assignment order = the original's store order: data, obj, how, then
     * the two header constants (which the scheduler sinks past the pushes). */
    d.dwObj  = dwObj;
    d.dwHow  = dwHow;
    d.dwData = dwData;
    d.dwSize       = 0x14u;
    d.dwHeaderSize = 0x10u;

    return BR_DI_SETPROP(pDev, prop, &d);
}

/* 0x10078ED0 */
/* WHAT IT DOES: actually delivers the shake -- it hands the wheel the length
 * and direction that were chosen above and tells it to start, which is what
 * the player feels on a bump or a collision. */
/* @implements 0x10078ED0 d3d BrFfbCommitDuration */
void BrFfbCommitDuration(void)
{
    BrDiObj *pEff;

    if (g_brB4E1D0 != 1 && g_brB4E1D0 != 2) {
        return;
    }
    if (g_brB4E1E0 == 0) {
        return;
    }
    if (g_br18ABDBC == 0) {
        return;
    }
    if (g_brFlag6909E0 != 0) {
        return;
    }
    g_brDiEffSquare.dwDuration = (uint32_t)g_br0BD438;

    pEff = g_brFfb.pEffectSquare;
    if (pEff == NULL) {
        return;
    }
    /* 0x20000041 = DIEP_DURATION | DIEP_DIRECTION | DIEP_START */
    BR_DI_SETPARAMS(pEff, &g_brDiEffSquare, 0x20000041u);
}

/* 0x100790B0 */
/* WHAT IT DOES: sets how hard a force-feedback wheel pulls back towards
 * centre -- the weight of the steering the player feels -- and sends the new
 * strength to the wheel at once. Unusually for this group it does not first
 * check that force feedback is enabled. */
/* @implements 0x100790B0 d3d BrFfbSetSpringCoeff */
void BrFfbSetSpringCoeff(int32_t coeff)
{
    BrDiObj *pEff;

    g_brDiSpringCond[0].lPositiveCoefficient = coeff;
    g_brDiSpringCond[0].lNegativeCoefficient = coeff;

    pEff = g_brFfb.pEffectSpring;
    if (pEff == NULL) {
        return;
    }
    /* 0x100 = DIEP_TYPESPECIFICPARAMS */
    BR_DI_SETPARAMS(pEff, &g_brDiEffSpring, 0x100u);
}
