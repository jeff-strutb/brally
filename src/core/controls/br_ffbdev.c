/* br_ffbdev.c -- controls: the DirectInput joystick / force-feedback device.
 *
 * Acquiring and releasing the input devices and driving the wheel's force
 * feedback: BrDiAcquire, BrDiKeyboardShutdown, BrFfbUpdateSpring (glide
 * 0x10072210, eases the centring spring toward its target) and
 * BrFfbEnumDevice (glide 0x100723D0, opens the first usable controller
 * DirectInput reports).  BrFfbSetup (0x10072680, builds the spring and shake
 * effects) and BrFfbInit (0x100724C0, finds the wheel/joystick, disables its
 * autocentre and sets the axis ranges) are byte-exact in the C++ lane,
 * src/core/controls/BrFfbSetup_10072680.cpp and BrFfbInit_100724C0.cpp: the
 * original TU was C++ against the C++ DirectInput interfaces.
 *
 * Filed out of the address batch slice3_45.c, with that file's preamble, the
 * GUIDs/strings/debug sink only these functions use, and copies of its
 * inline DirectInput helpers.
 */

#include <math.h>
#include <string.h>

#include "br_match.h"
/* Header is cdecl (this, x, y, z). Original is thiscall with ret 0xC. */
#define BrEntSetPos BrEntSetPos_hdr
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
#undef BrEntSetPos

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
__declspec(dllimport) void __stdcall OutputDebugStringA(const char *pMsg);
typedef void (__stdcall *BrDbgSink)(const char *pMsg);
#define BR_DBG_SINK OutputDebugStringA

/* BrFfbEnumDevice's original calls the import DIRECTLY per site
 * (`call dword ptr [__imp__OutputDebugStringA]`), unlike BrFfbInit's cached
 * register. The port routes the same sites through the safe sink. */
#define BR_DBG_OUT(msg) OutputDebugStringA(msg)

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

typedef long (__stdcall *BrDiSetParamsFn)(BrDiObj *, const BrDiEffect *, uint32_t);
typedef long (__stdcall *BrDiSetPropFn)(BrDiObj *, uint32_t, const void *);
#define BR_DI_SETPARAMS(p, eff, flags) \
    ((BrDiSetParamsFn)(((const BrDiEffVtbl *)(const void *)(p)->pVtbl)->pfnSetParameters))((p), (eff), (flags))
#define BR_DI_SETPROP(p, prop, pdiph) \
    ((BrDiSetPropFn)(((const BrDiDevVtbl *)(const void *)(p)->pVtbl)->pfnSetProperty))((p), (prop), (pdiph))

/* ====================================================================== */
/* 4. DirectInput devices                                                  */
/* ====================================================================== */

/* 0x100773D0 */
/* WHAT IT DOES: takes hold of the joystick or wheel so the game can read it,
 * which Windows requires again every time the game comes back to the
 * foreground. It reports whether it succeeded, and says "no" harmlessly if
 * there is no such device. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/generated/0x100706B0.c */
int32_t BrDiAcquire(void);

/* 0x10078BC0 */
/* WHAT IT DOES: lets go of the keyboard, but only once as many parts of the
 * game have finished with it as asked for it in the first place -- it counts
 * users rather than shutting down on the first call. An extra call after the
 * count has already reached zero does nothing at all. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/generated/0x10071EB0.c */
void BrDiKeyboardShutdown(void);

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
 * Do not reopen before the end-grind. */
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

/* 0x100790E0 */
/* WHAT IT DOES: called by Windows once for each controller it finds; this is
 * the game deciding to use that one. It opens the device, claims it, and
 * describes what sort of data it wants back, stopping the search on the first
 * one that works. Every way it can fail writes a message to the debugger and
 * gives the device back. */
/* @t4-pass 0x100723D0 1 2026-09-12 probes 10 bytes 239 insns 78 regions 2 rows 0 census no  (hand: CreateDevice out pointer through the spent pDevInst arg slot, no pTmp local, frame 0x10; direct per-site OutputDebugStringA import calls; create-failure as the ELSE of nesting the rest in the success arm, landing at the tail -- a goto spelling gets pulled inline. fn.py variants on the last residue: dataformat pVtbl local, hr temp, decl order, arg cast, EOF position, void-cast release all inert. Residue = one vtable temp ecx-vs-edx at SetDataFormat, 2 instructions / 4 B, plus the unmapped d3d-global reloc regions.) */
/* @t4-pass 0x100723D0 2 2026-09-12 probes 10 bytes 239 insns 78 regions 2 rows 0 census yes  (hand, fn.py variants: cast spellings, decl orders, !(hr>=0), pDev re-read drop, coop pVtbl local, else-arm inversion, EOF position, void-cast release, hr for CreateDevice, IID via local -- all inert or worse; slotcensus orig vs recomp byte-identical including the reused arg slot's 0-write/2-read shape) */
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

        /* Each failure arm releases the device itself; VC5 cross-jumps
         * the two identical tails into one block. Written once after a
         * join, the release's vtable temp is created after SetDataFormat's
         * and the two swap ecx/edx. */
        pDev = g_brFfb.pDevice;
        /* pvRef is an integer cooperative level, not a pointer. */
        if (BrDiDev(pDev)->pfnSetCooperativeLevel(pDev, g_brP680584,
                (uint32_t)(uintptr_t)pvRef) < 0) {
            BR_DBG_OUT(kBrErrCoopLevel);
            pDev = g_brFfb.pDevice;
            BrDiDev(pDev)->pfnRelease(pDev);
            g_brFfb.pDevice = NULL;
            return 0;
        }
        pDev = g_brFfb.pDevice;
        if (BrDiDev(pDev)->pfnSetDataFormat(pDev, kBrDataFormatJoystick2) < 0) {
            BR_DBG_OUT(kBrErrDataFormat);
            pDev = g_brFfb.pDevice;
            BrDiDev(pDev)->pfnRelease(pDev);
            g_brFfb.pDevice = NULL;
            return 0;
        }
        return 0;                           /* success -- DIENUM_STOP */
    }

    BR_DBG_OUT(kBrErrCreateDevice);
    return 0;
}
