/* WHAT IT DOES: finds the player's wheel or joystick and gets it ready. It
 * looks first for one that can do force feedback, and if it finds one it
 * turns off the wheel's own self-centring (the game supplies its own) and
 * builds the effects; failing that it settles for any controller at all. Then
 * it sets both axes to the range the game expects with no dead zone. It only
 * does the work once no matter how many times it is called. */
/* @implements 0x100724C0 glide BrFfbInit
 * @cpp_kind free
 * @cpp_symbol _BrFfbInit
 *
 * cdecl, no arguments, 434 B (D3D twin 0x100791D0).  Lives in the C++ lane
 * because the original TU was C++ against the C++ DirectInput interfaces:
 * with SetProperty a virtual method, C1XX forms the &d argument before the
 * vtable load (the ecx/edx order the C transcription in br_ffbdev.c could
 * not reach with any vtable-pointer local).  The body is the C transcription
 * with the device calls as method calls, compiled as C++ inside extern "C";
 * the preamble is br_ffbdev.c's.
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
/* Constants read out of reference/brally/orig/BRD3D.dll .rdata (do not re-derive)          */
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
/* declared only (the Mac port keeps its own body in ports/brally-wasm/patch/); Glide match is src/brally/core/generated/0x100706B0.c */
int32_t BrDiAcquire(void);

/* 0x10078BC0 */
/* WHAT IT DOES: lets go of the keyboard, but only once as many parts of the
 * game have finished with it as asked for it in the first place -- it counts
 * users rather than shutting down on the first call. An extra call after the
 * count has already reached zero does nothing at all. */
/* declared only (the Mac port keeps its own body in ports/brally-wasm/patch/); Glide match is src/brally/core/generated/0x10071EB0.c */
void BrDiKeyboardShutdown(void);

/* 0x10078C30 BrDiSetPropRange, 0x10078C80 BrDiSetPropDword,
 * 0x10078ED0 BrFfbCommitDuration and 0x100790B0 BrFfbSetSpringCoeff moved to
 * src/brally/core/controls/br_dicmd.c. They were the only users of BR_DI_SETPROP and
 * BR_DI_SETPARAMS above; the macros are left here so this file's preamble is
 * unchanged. The globals they read stay defined here, and slice3_45.h
 * declares all four for the callers that remain (BrFfbUpdateSpring below
 * calls BrFfbSetSpringCoeff). */

int32_t BrFfbInit(void)
{
    BrDiObj *pDev;
    BrDbgSink pfnDbg;
    int32_t ret;

    ret = g_brB4E1D0;
    if (ret != 0) {
        g_brFfbInitCount += 1;
        if (g_brFfbInitCount == 1) {
            pfnDbg = BR_DBG_SINK;

            if (g_brB4E1E0 != 0 &&
                BrDiRoot(g_pBr18ABD70)->EnumDevices(4u,
                    (BrDiEnumDevicesCb)BrFfbEnumDevice, (void *)(uintptr_t)5u, 0x101u) == 0 &&
                g_brFfbDevice != NULL) {

                /* pvRef 5 = DISCL_EXCLUSIVE|DISCL_FOREGROUND,
                 * flags 0x101 = DIEDFL_ATTACHEDONLY|DIEDFL_FORCEFEEDBACK. */
                BrDiPropDword d;
                

                /* The vtable pointer is a NAMED local loaded before the
                 * struct fill -- that hoist is what makes VC5 push the three
                 * arguments first and sink every store past them, as the
                 * original does. Residue: the original creates the &d temp
                 * before this load (ecx/edx swapped on three instructions);
                 * pHdr hoists, call-embedded assignment, decl order all
                 * probed dead 2026-09-12. */
                pDev = g_brFfbDevice;
                
                g_br18ABDBC = 1;

                d.dwSize       = 0x14u;
                d.dwHeaderSize = 0x10u;
                d.dwObj        = 0u;
                d.dwHow        = 0u;    /* DIPH_DEVICE */
                d.dwData       = 0u;    /* autocentre OFF */

                /* property 9 = DIPROP_AUTOCENTER */
                if (BrDiDev(pDev)->SetProperty(9u, &d) < 0) {
                    pfnDbg("Error: Failed to change device property.\n");
                }
                (void)BrDiAcquire();
                BrFfbSetup(0x3E8, 0x1F40);
            } else {
                /* pvRef 6 = DISCL_NONEXCLUSIVE|DISCL_FOREGROUND, flags 1. */
                (void)BrDiRoot(g_pBr18ABD70)->EnumDevices(4u,
                         (BrDiEnumDevicesCb)BrFfbEnumDevice, (void *)(uintptr_t)6u, 1u);
                (void)BrDiAcquire();
                g_br18ABDBC = 0;
            }

            pDev = g_brFfbDevice;
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
            pDev = g_brFfbDevice;
            if (BrDiSetPropDword(pDev, 5u, 0u, 1u, 0u) < 0) {
                goto failDword;
            }
            pDev = g_brFfbDevice;
            if (BrDiSetPropRange(pDev, 4u, 4u, 1u, -0x80, 0x80) < 0) {
failRange:
                pfnDbg("Error: IDirectInputDevice::SetProperty(DIPH_RANGE) FAILED\n");
                pDev = g_brFfbDevice;
                BrDiDev(pDev)->Unacquire();
                pDev = g_brFfbDevice;
                BrDiDev(pDev)->Release();
                g_brFfbDevice = NULL;
                /* NOTE: initCount stays raised. See slice1_10.h. */
                return 0;
            }
            pDev = g_brFfbDevice;
            if (BrDiSetPropDword(pDev, 5u, 4u, 1u, 0u) < 0) {
failDword:
                pfnDbg("Error: IDirectInputDevice::SetProperty(DIPH_WORD) FAILED\n");
                pDev = g_brFfbDevice;
                BrDiDev(pDev)->Unacquire();
                pDev = g_brFfbDevice;
                BrDiDev(pDev)->Release();
                g_brFfbDevice = NULL;
                /* NOTE: initCount stays raised. See slice1_10.h. */
                return 0;
            }
            ret = g_brB4E1D0;
        }
    }
    return ret;
}

}
