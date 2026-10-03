/* WHAT IT DOES: builds the two force-feedback effects the game uses -- the
 * constant centring pull that gives the steering its weight, and the shake
 * used for bumps and impacts -- and hands both to the wheel ready to be
 * started. The centring effect is created but deliberately left stopped. */
/* @implements 0x10072680 glide BrFfbSetup
 * @cpp_kind free
 * @cpp_symbol _BrFfbSetup
 *
 * cdecl, two arguments, 440 B (D3D twin 0x10079390).  Lives in the C++ lane
 * because the original TU was C++ against the C++ DirectInput interfaces:
 * with CreateEffect a virtual method, C1XX hoists each call's argument
 * pushes and the device load above the effect-structure fills, which the C
 * transcription (br_ffbdev.c, whose notes record the C spellings tried)
 * could not reproduce -- it pushed them just before the call.  The body is
 * the C transcription unchanged, compiled as C++ inside extern "C"; the
 * preamble is br_ffbdev.c's, with the three COM interfaces declared as
 * classes of __stdcall virtual methods in vtable-slot order.
 *
 * The axis buffer is a STACK local, as in the original: both DIEFFECTs'
 * rgdwAxes point at it and it dangles once this returns (the port gives it
 * static storage).
 */
#include <math.h>
#include <string.h>

extern "C" {
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
struct IBrDiRoot {
    virtual long __stdcall s00(void) = 0;
    virtual long __stdcall s01(void) = 0;
    virtual long __stdcall Release(void) = 0;
    virtual long __stdcall CreateDevice(const void *rguid, BrDiObj **ppDev, void *pUnkOuter) = 0;
    virtual long __stdcall EnumDevices(uint32_t devType, BrDiEnumDevicesCb cb, void *pvRef, uint32_t flags) = 0;
};
struct IBrDiDev {
    virtual long __stdcall QueryInterface(const void *iid, void **ppOut) = 0;
    virtual long __stdcall s01(void) = 0;
    virtual long __stdcall Release(void) = 0;
    virtual long __stdcall s03(void) = 0;
    virtual long __stdcall s04(void) = 0;
    virtual long __stdcall s05(void) = 0;
    virtual long __stdcall SetProperty(uint32_t prop, const void *pdiph) = 0;
    virtual long __stdcall Acquire(void) = 0;
    virtual long __stdcall Unacquire(void) = 0;
    virtual long __stdcall s09(void) = 0;
    virtual long __stdcall s10(void) = 0;
    virtual long __stdcall SetDataFormat(const void *pdf) = 0;
    virtual long __stdcall s12(void) = 0;
    virtual long __stdcall SetCooperativeLevel(void *hwnd, uint32_t flags) = 0;
    virtual long __stdcall s14(void) = 0;
    virtual long __stdcall s15(void) = 0;
    virtual long __stdcall s16(void) = 0;
    virtual long __stdcall s17(void) = 0;
    virtual long __stdcall CreateEffect(const void *rguid, const BrDiEffect *pEff, BrDiObj **ppEff, void *pUnkOuter) = 0;
};
struct IBrDiEff {
    virtual long __stdcall s00(void) = 0;
    virtual long __stdcall s01(void) = 0;
    virtual long __stdcall Release(void) = 0;
    virtual long __stdcall s03(void) = 0;
    virtual long __stdcall s04(void) = 0;
    virtual long __stdcall s05(void) = 0;
    virtual long __stdcall SetParameters(const BrDiEffect *pEff, uint32_t flags) = 0;
    virtual long __stdcall Start(uint32_t iterations, uint32_t flags) = 0;
    virtual long __stdcall Stop(void) = 0;
};
static __inline IBrDiRoot *BrDiRoot(void *p)
{
    return (IBrDiRoot *)p;
}
static __inline IBrDiDev *BrDiDev(void *p)
{
    return (IBrDiDev *)p;
}
static __inline IBrDiEff *BrDiEff(void *p)
{
    return (IBrDiEff *)p;
}

typedef long (__stdcall *BrDiSetParamsFn)(BrDiObj *, const BrDiEffect *, uint32_t);
typedef long (__stdcall *BrDiSetPropFn)(BrDiObj *, uint32_t, const void *);
#define BR_DI_SETPARAMS(p, eff, flags) \
    ((BrDiSetParamsFn)(((const BrDiEffVtbl *)(const void *)(p)->pVtbl)->pfnSetParameters))((p), (eff), (flags))
#define BR_DI_SETPROP(p, prop, pdiph) \
    ((BrDiSetPropFn)(((const BrDiDevVtbl *)(const void *)(p)->pVtbl)->pfnSetProperty))((p), (prop), (pdiph))

/* The FFB state is four separate globals in the original, not one record:
 * the device at 0x118EEEEC, the two effects at 0x118EEF04 / 0x118EEF14 and
 * the init count at 0x118EEF18 (the C lane's BrFfb struct gathers them for
 * the port, at offsets the original does not have). */
extern BrDiObj *g_brFfbDevice;          /* 0x118EEEEC */
extern BrDiObj *g_brFfbEffectSpring;    /* 0x118EEF04 */
extern BrDiObj *g_brFfbEffectSquare;    /* 0x118EEF14 */
extern int      g_brFfbInitCount;       /* 0x118EEF18 */

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

/* In the original both DIEFFECTs' rgdwAxes point at a two-dword STACK local of
 * 0x10079390 -- the `sub esp,8` frame and the two `lea` writes into the axis
 * pointers are that buffer.  It dangles the moment the function returns, and
 * BrFfbSetSpringCoeff / BrFfbCommitDuration hand those descriptors back to
 * SetParameters afterwards, so reproducing it in a build that actually RUNS
 * would be undefined behaviour.  The matching build spells it as the original
 * does; the port build gives the buffer static storage so it stays valid. */

void BrFfbSetup(int32_t springCoeff, int32_t springCoeff2)
{
    long hr;
    uint32_t rgAxes[2];            /* original: the axis buffer is a stack local */

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

    hr = BrDiDev(g_brFfbDevice)->CreateEffect(kBrGuidSpring, &g_brDiEffSpring,
                                        &g_brFfbEffectSpring, NULL);
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

    (void)BrDiDev(g_brFfbDevice)->CreateEffect(kBrGuidSquare, &g_brDiEffSquare,
                                         &g_brFfbEffectSquare, NULL);
}

}
