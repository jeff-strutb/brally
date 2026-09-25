/* br_ffbdev.c -- controls: the DirectInput joystick / force-feedback device.
 *
 * Acquiring and releasing the input devices and driving the wheel's force
 * feedback: BrDiAcquire, BrDiKeyboardShutdown, BrFfbUpdateSpring (glide
 * 0x10072210, eases the centring spring toward its target), BrFfbSetup builds
 * the spring and shake effects, BrFfbEnumDevice (glide 0x100723D0) opens the
 * first usable controller DirectInput reports, and BrFfbInit (glide
 * 0x100724C0) finds the wheel/joystick, disables its autocentre and sets the
 * axis ranges.
 *
 * Filed out of the address batch slice3_45.c, with that file's preamble, the
 * GUIDs/strings/debug sink only these functions use, and copies of its
 * inline DirectInput helpers.
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

/* ====================================================================== */
/* Constants read out of orig/BRD3D.dll .rdata (do not re-derive)          */
/* ====================================================================== */

/* LAYOUT: BrDiv10000 sits ahead of the .rdata constants (in slice3_45.c it
 * followed them) so the compiler-generated $S suffixes of the static data
 * below come out as they did in the batch file; code bytes are unaffected. */
/* The compiler's magic-multiply for signed /10000 (0x68DB8BAD, sar 12, plus
 * the sign bit). Reproduced as a plain division: for every int32_t input the
 * two agree, because the original truncates toward zero exactly as C99 does.
 * The MULTIPLY that feeds it is done in uint32_t so its wrap matches. */
static __inline int32_t BrDiv10000(int32_t v)
{
    return v / 10000;
}

/* 0x100907D0 and 0x10090780, the two effect GUIDs handed to CreateEffect
 * (byte order as it sits in the file). slice1_10.h identified them as
 * GUID_Spring {13541C27-8E33-11D0-9AD0-00A0C9A06E35} and GUID_Square
 * {13541C22-...}; the bytes below confirm it. */
static const unsigned char kBrGuidSpring[16] = {
    0x27, 0x1c, 0x54, 0x13, 0x33, 0x8e, 0xd0, 0x11,
    0x9a, 0xd0, 0x00, 0xa0, 0xc9, 0xa0, 0x6e, 0x35
};
static const unsigned char kBrGuidSquare[16] = {
    0x22, 0x1c, 0x54, 0x13, 0x33, 0x8e, 0xd0, 0x11,
    0x9a, 0xd0, 0x00, 0xa0, 0xc9, 0xa0, 0x6e, 0x35
};

/* 0x10090650 = {5944E682-C92E-11CF-BFC7-444553540000} = IID_IDirectInputDevice2A. */
static const unsigned char kBrIidDevice2A[16] = {
    0x82, 0xe6, 0x44, 0x59, 0x2e, 0xc9, 0xcf, 0x11,
    0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00
};

/* 0x1007C7A0: DIDATAFORMAT { 0x18, 0x10, DIDF_ABSAXIS, 0x110, 164, rgodf }
 * -- c_dfDIJoystick2. Only its ADDRESS is used, so an opaque stand-in is
 * enough; the real table lives in the DLL's read-only data. */
static const uint32_t kBrDataFormatJoystick2[6] = {
    0x18u, 0x10u, 0x1u, 0x110u, 0xa4u, 0u
};

/* The literal strings the failure paths hand to OutputDebugStringA. */
static const char kBrErrCreateDevice[] = "Error: Failed to create device.\n";
static const char kBrErrDataFormat[]   = "Error: Failed to set game device data format.\n";
static const char kBrErrCoopLevel[]    = "Error: Failed to set cooperative level.\n";
static const char kBrErrInterface[]    = "Error: Failed to obtain interface.\n";
static const char kBrErrPropWord[]     = "Error: IDirectInputDevice::SetProperty(DIPH_WORD) FAILED\n";
static const char kBrErrPropRange[]    = "Error: IDirectInputDevice::SetProperty(DIPH_RANGE) FAILED\n";
static const char kBrErrProperty[]     = "Error: Failed to change device property.\n";

/* DEVIATION: the original calls KERNEL32!OutputDebugStringA. That is not
 * portable, so the sink is a function pointer defaulting to a no-op. Every
 * call site, and the order of the calls relative to the COM calls around
 * them, is preserved exactly. */
/* g_pBrDbgPrint itself stays defined in slice3_45.c (declared in slice3_45.h). */

static void BrDbgPrint(const char *pMsg)
{
    if (g_pBrDbgPrint != NULL) {
        g_pBrDbgPrint(pMsg);
    }
}

/* BrFfbInit's original caches &KERNEL32!OutputDebugStringA in a register and
 * calls through it (`mov edi,[__imp__]; call edi`, stdcall). The matching arm
 * spells that pointer; the port routes the same local through the safe sink
 * above. */
#ifdef BR_MATCHING_BUILD
__declspec(dllimport) void __stdcall OutputDebugStringA(const char *pMsg);
typedef void (__stdcall *BrDbgSink)(const char *pMsg);
#define BR_DBG_SINK OutputDebugStringA
#else
typedef void (*BrDbgSink)(const char *pMsg);
#define BR_DBG_SINK BrDbgPrint
#endif

/* BrFfbEnumDevice's original calls the import DIRECTLY per site
 * (`call dword ptr [__imp__OutputDebugStringA]`), unlike BrFfbInit's cached
 * register. The port routes the same sites through the safe sink. */
#ifdef BR_MATCHING_BUILD
#define BR_DBG_OUT(msg) OutputDebugStringA(msg)
#else
#define BR_DBG_OUT(msg) BrDbgPrint(msg)
#endif

/* ====================================================================== */
/* Small helpers                                                           */
/* ====================================================================== */


static __inline int32_t BrMulWrap(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

static __inline int32_t BrAddWrap(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a + (uint32_t)b);
}

/* Cast helpers for the three COM interfaces. slice1_10.h's BrDiObj carries a
 * `const BrDiVtbl *`; these reinterpret it as the wider vtables declared in
 * slice3_45.h. See the note there -- BrDiVtbl is deliberately not redefined. */
static __inline const BrDiRootVtbl *BrDiRoot(BrDiObj *p)
{
    return (const BrDiRootVtbl *)(const void *)p->pVtbl;
}
static __inline const BrDiDevVtbl *BrDiDev(BrDiObj *p)
{
    return (const BrDiDevVtbl *)(const void *)p->pVtbl;
}
static __inline const BrDiEffVtbl *BrDiEff(BrDiObj *p)
{
    return (const BrDiEffVtbl *)(const void *)p->pVtbl;
}

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

/* ====================================================================== */
/* 4. DirectInput devices                                                  */
/* ====================================================================== */

/* 0x100773D0 */
/* WHAT IT DOES: takes hold of the joystick or wheel so the game can read it,
 * which Windows requires again every time the game comes back to the
 * foreground. It reports whether it succeeded, and says "no" harmlessly if
 * there is no such device. */
/* port-only body; Glide match is src/core/generated/0x100706B0.c */
int32_t BrDiAcquire(void)
{
    BrDiObj *pDev = g_brFfb.pDevice;

    if (pDev == NULL) {
        return 0;
    }
    return (BrDiDev(pDev)->pfnAcquire(pDev) >= 0) ? 1 : 0;
}

/* 0x10078BC0 */
/* WHAT IT DOES: lets go of the keyboard, but only once as many parts of the
 * game have finished with it as asked for it in the first place -- it counts
 * users rather than shutting down on the first call. An extra call after the
 * count has already reached zero does nothing at all. */
/* port-only body; Glide match is src/core/generated/0x10071EB0.c */
void BrDiKeyboardShutdown(void)
{
    BrDiObj *pDev;

    g_br18ABDD8 -= 1;
    if (g_br18ABDD8 < 0) {
        g_br18ABDD8 = 0;
        return;                       /* clamp, and NO teardown */
    }
    if (g_br18ABDD8 != 0) {
        return;
    }
    pDev = g_pBr18ABDD0;
    if (pDev == NULL) {
        return;
    }
    BrDiDev(pDev)->pfnUnacquire(pDev);
    /* Re-read, deliberately without a second NULL test -- as the original. */
    pDev = g_pBr18ABDD0;
    BrDiDev(pDev)->pfnRelease(pDev);
    g_pBr18ABDD0 = NULL;
}

/* 0x10078C30 BrDiSetPropRange, 0x10078C80 BrDiSetPropDword,
 * 0x10078ED0 BrFfbCommitDuration and 0x100790B0 BrFfbSetSpringCoeff moved to
 * src/core/controls/br_dicmd.c. They were the only users of BR_DI_SETPROP and
 * BR_DI_SETPARAMS above; the macros are left here so this file's preamble is
 * unchanged. The globals they read stay defined here, and slice3_45.h
 * declares all four for the callers that remain (BrFfbUpdateSpring below
 * calls BrFfbSetSpringCoeff). */

/* @t4-pass 0x10072210 1 2026-09-12 probes 10 bytes 391 insns 127 regions 5 rows 0 census no  (hand, fn.py variants: named k local for the g_br0BD424 web -- fixes the whole esi/ecx rotation AND the [esp+0x14] schedule but breaks the up-arm bound into an imul-from-memory fold, size -2; own-statement / rate-temp / scaled-temp loads of g_br0BD428 all add a mov r,r; operand swap, direct expression and plain signed spelling inert; cur-first declaration order inert; end-of-TU position inert; before-loads-first collapses the *1000 lea chain the original keeps. Residue = one named-local-vs-CSE-web priority swap: orig cur->esi / g_br0BD424-web->ecx, ours reversed. Byte-exact additionally gated on the 5 unmapped d3d-global reloc regions, same bootstrap as 0x10079390.) */
/* @t4-pass 0x10072210 2 2026-09-12 probes 10 bytes 391 insns 127 regions 5 rows 0 census yes  (hand, fn.py variants: guard OR-chain, arm swap, chained scaled/cur/clamp stores, block-scope pEff, mul operand swap, split scaled statements, reversed compares, up copied to a local -- all inert or worse; slotcensus orig vs recomp identical slot-for-slot, 3 arg reads, no locals slots) */
/* @t3 0x10072210 2026-09-12 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 391/391 insns 127/127 rows 0+0 regions 5 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is one allocation fork: the original gives cur the callee-saved
 * esi and the g_br0BD424 CSE web ecx, ours the reverse, dragging the
 * [esp+0x14] enable read two slots later (identical multiset, 0+0). A named
 * k local flips both but breaks the up-arm bound into an imul-from-memory
 * fold -- full dead list in the two ledger lines. Byte-exact additionally
 * gated on the unmapped d3d-global reloc regions.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* 0x10078F20 */
/* WHAT IT DOES: eases the weight of the steering up or down a step at a time
 * rather than jumping to it, so the wheel's resistance changes smoothly as
 * the car speeds up or slows down, stopping at a floor and a ceiling that
 * depend on how much resistance the caller has asked for. Asking for it to be
 * off stops the effect entirely, and asking for it again afterwards restarts
 * it. The wheel is only told when the value has actually moved. */
/* @implements 0x10078F20 d3d BrFfbUpdateSpring */
void BrFfbUpdateSpring(int32_t up, int32_t enable, int32_t decay)
{
    int32_t scaled;
    int32_t rate;
    int32_t cur;
    int32_t before;
    int32_t bound;
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

    /* ((decay + 8) * g_br0BD424) * 1000 / 10000, all in 32 bits. */
    scaled = BrMulWrap(BrMulWrap(BrAddWrap(decay, 8), g_br0BD424), 1000);
    scaled = BrDiv10000(scaled);
    g_br0BD42C = scaled;

    if (enable == 0) {
        if (g_br18ABDF8 != 0) {
            return;                    /* already stopped */
        }
        pEff = g_brFfb.pEffectSpring;
        if (pEff != NULL) {
            BrDiEff(pEff)->pfnStop(pEff);
        }
        g_br18ABDF8 = 1;
        return;
    }

    if (g_br18ABDF8 != 0) {
        pEff = g_brFfb.pEffectSpring;
        if (pEff != NULL) {
            BrDiEff(pEff)->pfnStart(pEff, 1u, 0u);
        }
        BrFfbSetDurationLong();
        /* Both are re-read from memory here -- BrFfbSetDurationLong could in
         * principle have changed them. */
        scaled = g_br0BD42C;
        g_br18ABDF8 = 0;
    }

    cur    = g_br18ABD78;
    before = cur;

    if (up != 0) {
        /* The original forms -(g_br0BD424) * 1000 and divides THAT, so the
         * negation happens before the truncation. Kept in that order. */
        rate = BrDiv10000(BrMulWrap(g_br0BD424, -1000));
        cur  = BrAddWrap(before, rate);
        g_br18ABD78 = cur;
        bound = BrDiv10000(BrMulWrap(g_br0BD428, g_br0BD424));
        if (cur < bound) {                 /* lower clamp */
            cur = bound;
            g_br18ABD78 = cur;
        }
    } else {
        rate = BrDiv10000(BrMulWrap(g_br0BD424, 1000));
        cur  = BrAddWrap(before, rate);
        g_br18ABD78 = cur;
        bound = BrDiv10000(BrMulWrap(scaled, g_br0BD424));
        if (cur > bound) {                 /* upper clamp */
            cur = bound;
            g_br18ABD78 = cur;
        }
    }

    if (cur != before) {
        BrFfbSetSpringCoeff(cur);
    }
}

/* In the original both DIEFFECTs' rgdwAxes point at a two-dword STACK local of
 * 0x10079390 -- the `sub esp,8` frame and the two `lea` writes into the axis
 * pointers are that buffer.  It dangles the moment the function returns, and
 * BrFfbSetSpringCoeff / BrFfbCommitDuration hand those descriptors back to
 * SetParameters afterwards, so reproducing it in a build that actually RUNS
 * would be undefined behaviour.  The matching build spells it as the original
 * does; the port build gives the buffer static storage so it stays valid. */
#ifndef BR_MATCHING_BUILD
static uint32_t g_brFfbAxes[2];
#endif

/* 0x10079390 */
/* WHAT IT DOES: builds the two force-feedback effects the game uses -- the
 * constant centring pull that gives the steering its weight, and the shake
 * used for bumps and impacts -- and hands both to the wheel ready to be
 * started. The centring effect is created but deliberately left stopped. */
/*
 * SIZE-EXACT (80/80 insns, 440/440 B), REGNORM 6+6, from four source facts:
 *   1. the axis buffer is a STACK local, not a file-scope global -- that is
 *      the `sub esp,8` frame and the two `lea` writes (matching arm only;
 *      the port keeps it static because it dangles after return);
 *   2. g_brFfb.pDevice is re-read at each CreateEffect site, not hoisted into
 *      a local -- hoisting pins it in a callee-saved reg and costs a push ebp;
 *   3. BrDiRoot/BrDiDev/BrDiEff must be __inline or VC5 emits an out-of-line
 *      call for each vtable cast (an extra call + `add esp,4` per use);
 *   4. the COM vtable pointers are BR_STDCALL -- callee-cleaned, so no
 *      `add esp,0x14` after each CreateEffect.
 * The residue is SIX unmapped d3d globals: fn.py cannot substitute their
 * addresses, so it reads the reloc slots as immediates.  Every slot is a real
 * relocation (coff_relocs.py), so this is byte-exact once the globals carry
 * their d3d addresses -- g_brDiSpringDir 0x118EEF08, g_brDiSpringCond
 * 0x118EEE20, g_brDiSquarePeriod 0x118EEBD8, kBrGuidSpring 0x100787A8,
 * kBrGuidSquare 0x10078758 (g_br0BD430 0x100BCC38 already mapped).  They
 * cannot be learned from this function (reloc_learn only trusts a
 * masked-match, which needs them mapped) -- seed them from a matched sibling
 * or hand-map, then this row and the other three BrFfb diffs close together. */
/* @implements 0x10079390 d3d BrFfbSetup */
void BrFfbSetup(int32_t springCoeff, int32_t springCoeff2)
{
    long hr;
#ifdef BR_MATCHING_BUILD
    uint32_t rgAxes[2];            /* original: the axis buffer is a stack local */
#else
#define rgAxes g_brFfbAxes
#endif

    rgAxes[0] = 0u;   /* lX */
    rgAxes[1] = 4u;   /* lY */

    g_brDiSpringCond[1].lPositiveCoefficient = springCoeff2;
    g_brDiSpringCond[1].lNegativeCoefficient = springCoeff2;

    g_brDiSpringCond[0].lOffset              = 0;
    g_brDiSpringCond[0].lPositiveCoefficient = springCoeff;
    g_brDiSpringCond[0].lNegativeCoefficient = springCoeff;
    g_brDiSpringCond[0].dwPositiveSaturation = 10000u;
    g_brDiSpringCond[0].dwNegativeSaturation = 10000u;
    g_brDiSpringCond[0].lDeadBand            = 0;
    g_brDiSpringCond[1].lOffset              = 0;
    g_brDiSpringCond[1].dwPositiveSaturation = 10000u;
    g_brDiSpringCond[1].dwNegativeSaturation = 10000u;
    g_brDiSpringCond[1].lDeadBand            = 0;

    g_brDiEffSpring.dwSize                 = 0x34u;   /* sizeof on x86 */
    g_brDiEffSpring.dwFlags                = 0x12u;   /* OBJECTOFFSETS|CARTESIAN */
    g_brDiEffSpring.dwDuration             = 0xFFFFFFFFu;  /* INFINITE */
    g_brDiEffSpring.dwSamplePeriod         = 0u;
    g_brDiEffSpring.dwGain                 = 10000u;
    g_brDiEffSpring.dwTriggerButton        = 0xFFFFFFFFu;  /* DIEB_NOTRIGGER */
    g_brDiEffSpring.dwTriggerRepeatInterval = 0u;
    g_brDiEffSpring.cAxes                  = 2u;
    g_brDiEffSpring.rgdwAxes               = rgAxes;
    g_brDiEffSpring.rglDirection           = g_brDiSpringDir;
    g_brDiEffSpring.lpEnvelope             = NULL;
    g_brDiEffSpring.cbTypeSpecificParams   = 0x30u;   /* two DICONDITIONs */
    g_brDiEffSpring.lpvTypeSpecificParams  = g_brDiSpringCond;

    hr = BrDiDev(g_brFfb.pDevice)->pfnCreateEffect(g_brFfb.pDevice, kBrGuidSpring, &g_brDiEffSpring,
                                        &g_brFfb.pEffectSpring, NULL);
    if (hr == 0) {
        g_br18ABD78 = springCoeff;
        g_br18ABDF8 = 1;               /* created, but not started */
    }

    g_brDiSquarePeriod.dwMagnitude = 10000u;
    g_brDiSquarePeriod.lOffset     = 0;
    g_brDiSquarePeriod.dwPhase     = 0u;
    g_brDiSquarePeriod.dwPeriod    = 0x3D090u;   /* 250000 us */

    g_brDiEffSquare.dwSize                  = 0x34u;
    g_brDiEffSquare.dwFlags                 = 0x12u;
    g_brDiEffSquare.dwDuration              = (uint32_t)g_br0BD438;
    g_brDiEffSquare.dwSamplePeriod          = 0u;
    g_brDiEffSquare.dwGain                  = 10000u;
    g_brDiEffSquare.dwTriggerButton         = 0xFFFFFFFFu;
    g_brDiEffSquare.dwTriggerRepeatInterval = 0u;
    g_brDiEffSquare.cAxes                   = 2u;
    g_brDiEffSquare.rgdwAxes                = rgAxes;
    g_brDiEffSquare.rglDirection            = g_br0BD430;
    g_brDiEffSquare.lpEnvelope              = NULL;
    g_brDiEffSquare.cbTypeSpecificParams    = 0x10u;  /* one DIPERIODIC */
    g_brDiEffSquare.lpvTypeSpecificParams   = &g_brDiSquarePeriod;

    (void)BrDiDev(g_brFfb.pDevice)->pfnCreateEffect(g_brFfb.pDevice, kBrGuidSquare, &g_brDiEffSquare,
                                         &g_brFfb.pEffectSquare, NULL);
}
#ifndef BR_MATCHING_BUILD
#undef rgAxes
#endif

/* 0x100790E0 */
/* WHAT IT DOES: called by Windows once for each controller it finds; this is
 * the game deciding to use that one. It opens the device, claims it, and
 * describes what sort of data it wants back, stopping the search on the first
 * one that works. Every way it can fail writes a message to the debugger and
 * gives the device back. */
/* @t4-pass 0x100723D0 1 2026-09-12 probes 10 bytes 239 insns 78 regions 2 rows 0 census no  (hand: CreateDevice out pointer through the spent pDevInst arg slot, no pTmp local, frame 0x10; direct per-site OutputDebugStringA import calls; create-failure as the ELSE of nesting the rest in the success arm, landing at the tail -- a goto spelling gets pulled inline. fn.py variants on the last residue: dataformat pVtbl local, hr temp, decl order, arg cast, EOF position, void-cast release all inert. Residue = one vtable temp ecx-vs-edx at SetDataFormat, 2 instructions / 4 B, plus the unmapped d3d-global reloc regions.) */
/* @t4-pass 0x100723D0 2 2026-09-12 probes 10 bytes 239 insns 78 regions 2 rows 0 census yes  (hand, fn.py variants: cast spellings, decl orders, !(hr>=0), pDev re-read drop, coop pVtbl local, else-arm inversion, EOF position, void-cast release, hr for CreateDevice, IID via local -- all inert or worse; slotcensus orig vs recomp byte-identical including the reused arg slot's 0-write/2-read shape) */
/* @t3 0x100723D0 2026-09-12 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 239/239 insns 78/78 rows 0+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is one vtable-temp register at the SetDataFormat call (ours ecx,
 * original edx; 2 instructions, 4 bytes) -- the same creation-order class as
 * BrFfbInit's. Dead lists in the two ledger lines. Byte-exact additionally
 * gated on the unmapped d3d-global reloc regions.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x100790E0 d3d BrFfbEnumDevice */
int32_t BR_STDCALL BrFfbEnumDevice(void *pDevInst, void *pvRef)
{
    unsigned char guid[16];
    BrDiObj *pDev;
    long hr;

    memcpy(guid, (const unsigned char *)pDevInst + 4, sizeof guid);

    /* The original has NO local for the created device: CreateDevice's out
     * pointer is written into the spent pDevInst ARGUMENT SLOT (the lea
     * points at [esp+arg0]), and the parameter is read back as the device.
     * That is why the frame is 0x10 -- the guid alone -- and why there is no
     * NULL initialisation anywhere. */
    /* The create-failure message is the ELSE of nesting everything in the
     * success arm -- that is what lands it at the function's tail (jl to the
     * end), where its return-0 shares the epilogue the SetDataFormat success
     * jumps to. A goto spelling gets pulled back inline. */
    if (BrDiRoot(g_pBr18ABD70)->pfnCreateDevice(g_pBr18ABD70, guid,
                                                (BrDiObj **)&pDevInst,
                                                NULL) >= 0) {

        hr = BrDiDev(pDevInst)->pfnQueryInterface(pDevInst, kBrIidDevice2A,
                                                  (void **)(void *)&g_brFfb.pDevice);
        BrDiDev(pDevInst)->pfnRelease(pDevInst);
        if (hr < 0) {
            /* Note: g_brFfb.pDevice is NOT cleared on this path. */
            BR_DBG_OUT(kBrErrInterface);
            return 0;
        }

        pDev = g_brFfb.pDevice;
        /* pvRef is an integer cooperative level, not a pointer. */
        if (BrDiDev(pDev)->pfnSetCooperativeLevel(pDev, g_brP680584,
                (uint32_t)(uintptr_t)pvRef) < 0) {
            BR_DBG_OUT(kBrErrCoopLevel);
        } else {
            pDev = g_brFfb.pDevice;
            if (BrDiDev(pDev)->pfnSetDataFormat(pDev, kBrDataFormatJoystick2) >= 0) {
                return 0;                   /* success -- DIENUM_STOP */
            }
            BR_DBG_OUT(kBrErrDataFormat);
        }

        pDev = g_brFfb.pDevice;
        BrDiDev(pDev)->pfnRelease(pDev);
        g_brFfb.pDevice = NULL;
        return 0;
    }

    BR_DBG_OUT(kBrErrCreateDevice);
    return 0;
}

/* 0x100791D0 */
/* WHAT IT DOES: finds the player's wheel or joystick and gets it ready. It
 * looks first for one that can do force feedback, and if it finds one it
 * turns off the wheel's own self-centring (the game supplies its own) and
 * builds the effects; failing that it settles for any controller at all. Then
 * it sets both axes to the range the game expects with no dead zone. It only
 * does the work once no matter how many times it is called. */
/* @t4-pass 0x100724C0 1 2026-09-12 probes 10 bytes 434 insns 147 regions 5 rows 0 census no  (hand: nested-guard restructure with shared return-ret tail and per-kind inline teardown blocks; stdcall OutputDebugStringA pointer cached in a local; hr local dropped for direct call compares; ret local with end-of-body reassignment. fn.py variants on the last residue: pVtbl hoist wins the store sink, pHdr hoist folds away, SETPROP macro / call-embedded assignment / decl orders all inert. Residue = ecx/edx creation-order swap on the SetProperty vtable/&d temps, 3 instructions, plus the unmapped d3d-global reloc regions.) */
/* @t4-pass 0x100724C0 2 2026-09-12 probes 10 bytes 434 insns 147 regions 5 rows 0 census yes  (hand, fn.py variants: ++count and fused RMW-compare, pfnDbg hoist, d store orders, void-cast drop, !pDev, teardown re-read drop, decl orders, EOF position, pRoot local -- all inert or worse; slotcensus orig vs recomp identical, sole delta the known lea register) */
/* @t3 0x100724C0 2026-09-12 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 434/434 insns 147/147 rows 0+0 regions 5 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is one temp-creation-order fork at the SetProperty call: the
 * original creates the &d lea before the vtable load (ecx/edx swapped on
 * three instructions). pVtbl hoist wins the store sink but not the pair;
 * pHdr folds away; macro and call-embedded spellings lose the sink. Dead
 * lists in the two ledger lines. Byte-exact additionally gated on the
 * unmapped d3d-global reloc regions.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x100791D0 d3d BrFfbInit */
int32_t BrFfbInit(void)
{
    BrDiObj *pDev;
    BrDbgSink pfnDbg;
    int32_t ret;

    ret = g_brB4E1D0;
    if (ret != 0) {
        g_brFfb.initCount += 1;
        if (g_brFfb.initCount == 1) {
            pfnDbg = BR_DBG_SINK;

            if (g_brB4E1E0 != 0 &&
                BrDiRoot(g_pBr18ABD70)->pfnEnumDevices(g_pBr18ABD70, 4u,
                    BrFfbEnumDevice, (void *)(uintptr_t)5u, 0x101u) == 0 &&
                g_brFfb.pDevice != NULL) {

                /* pvRef 5 = DISCL_EXCLUSIVE|DISCL_FOREGROUND,
                 * flags 0x101 = DIEDFL_ATTACHEDONLY|DIEDFL_FORCEFEEDBACK. */
                BrDiPropDword d;
                const BrDiDevVtbl *pVtbl;

                /* The vtable pointer is a NAMED local loaded before the
                 * struct fill -- that hoist is what makes VC5 push the three
                 * arguments first and sink every store past them, as the
                 * original does. Residue: the original creates the &d temp
                 * before this load (ecx/edx swapped on three instructions);
                 * pHdr hoists, call-embedded assignment, decl order all
                 * probed dead 2026-09-12. */
                pDev = g_brFfb.pDevice;
                pVtbl = BrDiDev(pDev);

                g_br18ABDBC = 1;

                d.dwSize       = 0x14u;
                d.dwHeaderSize = 0x10u;
                d.dwObj        = 0u;
                d.dwHow        = 0u;    /* DIPH_DEVICE */
                d.dwData       = 0u;    /* autocentre OFF */

                /* property 9 = DIPROP_AUTOCENTER */
                if (pVtbl->pfnSetProperty(pDev, 9u, &d) < 0) {
                    pfnDbg(kBrErrProperty);
                }
                (void)BrDiAcquire();
                BrFfbSetup(0x3E8, 0x1F40);
            } else {
                /* pvRef 6 = DISCL_NONEXCLUSIVE|DISCL_FOREGROUND, flags 1. */
                (void)BrDiRoot(g_pBr18ABD70)->pfnEnumDevices(g_pBr18ABD70, 4u,
                         BrFfbEnumDevice, (void *)(uintptr_t)6u, 1u);
                (void)BrDiAcquire();
                g_br18ABDBC = 0;
            }

            pDev = g_brFfb.pDevice;
            if (pDev == NULL) {
                return 0;
            }

            /* Axis 0 (lX): range +-0x80, no dead zone. Then axis 4 (lY), the
             * same. dwHow 1 = DIPH_BYOFFSET; prop 4 = DIPROP_RANGE,
             * 5 = DIPROP_DEADZONE.
             *
             * The failure blocks sit INLINE after the third and fourth calls
             * (each ends in its own return, which is why the original carries
             * TWO copies of the teardown); the first two calls goto INTO
             * them. That is the original's layout -- do not re-share. */
            if (BrDiSetPropRange(pDev, 4u, 0u, 1u, -0x80, 0x80) < 0) {
                goto failRange;
            }
            pDev = g_brFfb.pDevice;
            if (BrDiSetPropDword(pDev, 5u, 0u, 1u, 0u) < 0) {
                goto failDword;
            }
            pDev = g_brFfb.pDevice;
            if (BrDiSetPropRange(pDev, 4u, 4u, 1u, -0x80, 0x80) < 0) {
failRange:
                pfnDbg(kBrErrPropRange);
                pDev = g_brFfb.pDevice;
                BrDiDev(pDev)->pfnUnacquire(pDev);
                pDev = g_brFfb.pDevice;
                BrDiDev(pDev)->pfnRelease(pDev);
                g_brFfb.pDevice = NULL;
                /* NOTE: initCount stays raised. See slice1_10.h. */
                return 0;
            }
            pDev = g_brFfb.pDevice;
            if (BrDiSetPropDword(pDev, 5u, 4u, 1u, 0u) < 0) {
failDword:
                pfnDbg(kBrErrPropWord);
                pDev = g_brFfb.pDevice;
                BrDiDev(pDev)->pfnUnacquire(pDev);
                pDev = g_brFfb.pDevice;
                BrDiDev(pDev)->pfnRelease(pDev);
                g_brFfb.pDevice = NULL;
                /* NOTE: initCount stays raised. See slice1_10.h. */
                return 0;
            }
            ret = g_brB4E1D0;
        }
    }
    return ret;
}
