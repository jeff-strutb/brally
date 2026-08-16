/* br_wire77.c -- host stand-ins for two PLATFORM RESOURCES the ported core
 * reads but cannot own: the IDirectInput root object, and the localised string
 * table. Neither is a function that could be ported; both are things Win32
 * hands the game at startup through a path this port has not ported.
 *
 * The other br_wire*.c files supply a slice's module context. This one is not
 * about a slice -- both symbols here belong to modules (slice3_45, slice4_52)
 * that are fully ported and correct, and are empty only because nothing has
 * initialised them.
 *
 * ==========================================================================
 * 1. The IDirectInput root the force-feedback probe dereferences
 * ==========================================================================
 *
 * WHY THE HOST OWNS THIS AND THE CORE DOES NOT
 *
 * slice6_77.c's BrFfbReprobe (0x100795D0) is portable: it saves two globals,
 * forces a known configuration, calls the already-ported BrFfbInit and
 * BrFfbShutdown, and picks one of four binding records. Nothing in it is
 * platform-specific.
 *
 * BrFfbInit (0x100791D0, slice3_45.c) is a different matter. It reaches the
 * IDirectInput root at 0x118ABD70 and calls EnumDevices through its COM
 * vtable WITHOUT a NULL test -- faithfully, because the original does not test
 * it either. The original can get away with that: DirectInputCreate runs
 * during startup and the root is live by the time any screen is built. This
 * port has no DirectInput and no ported startup path, so `g_pBr18ABD70` is
 * NULL and the first ported caller of the probe would fault inside slice3_45.
 *
 * That pointer is a COM interface. It cannot be modelled in portable code, so
 * it is modelled here, which is what "wire it through the host" means for a
 * platform object rather than a platform function.
 *
 * WHAT IT REPORTS, AND WHY THAT IS NOT AN INVENTION
 *
 * EnumDevices succeeds and enumerates nothing: it returns DI_OK and never
 * invokes the callback, so BrFfbEnumDevice never runs and g_brFfb.pDevice
 * stays NULL. That is precisely the state of a machine with no force-feedback
 * wheel attached -- a state the shipped game genuinely has and handles, not a
 * failure this file is faking to dodge the code path.
 *
 * BrFfbInit then takes its documented non-exclusive fallback: it re-enumerates
 * (again finding nothing), calls BrDiAcquire (which is NULL-safe), clears
 * 0x118ABDBC, and returns 0 on the `pDevice == NULL` test. BrFfbShutdown
 * unwinds the nested-init counter back to 0 and finds all three COM pointers
 * already NULL, so every release is skipped. The whole probe is exercised end
 * to end and nothing is released that was never created.
 *
 * This is the same pattern -- and the same justification -- as the "finds
 * nothing" file-list scan hooks in br_wire71.c and br_wire72.c.
 *
 * The other two root slots are wired to loud stubs rather than left NULL: no
 * path this harness runs reaches them, and if one ever does, an abort naming
 * the slot is more use than a jump to 0.
 */
#include "slice3_45.h"
#include "slice4_52.h"   /* g_apBrStrTable, BR_STR_TABLE_COUNT (0x11829370) */

#include <stdio.h>
#include <stdlib.h>

static long RootUnexpected(const char *pszSlot)
{
    fprintf(stderr, "br_wire77: IDirectInput::%s called -- the host's root is "
                    "a no-device stand-in and cannot service it\n", pszSlot);
    abort();
}

static long RootRelease(BrDiObj *pThis)
{
    (void)pThis;
    return RootUnexpected("Release");
}

static long RootCreateDevice(BrDiObj *pThis, const void *rguid,
                             BrDiObj **ppDev, void *pUnkOuter)
{
    (void)pThis; (void)rguid; (void)ppDev; (void)pUnkOuter;
    return RootUnexpected("CreateDevice");
}

/* DI_OK with no callback invocation == "no attached device matched". */
static long RootEnumDevices(BrDiObj *pThis, uint32_t devType,
                            BrDiEnumDevicesCb cb, void *pvRef, uint32_t flags)
{
    (void)pThis; (void)devType; (void)cb; (void)pvRef; (void)flags;
    return 0;
}

static const BrDiRootVtbl g_rootVtbl = {
    NULL,                /* QueryInterface -- unreached, and typed void * */
    NULL,                /* AddRef         -- likewise                    */
    RootRelease,
    RootCreateDevice,
    RootEnumDevices
};

static BrDiObj g_root;

/* ==========================================================================
 * 2. The localised string table at 0x11829370
 * ==========================================================================
 *
 * WHY IT IS EMPTY, AND WHY THAT IS A CRASH RATHER THAN A BLANK
 *
 * BrStrGet (0x10074030, slice4_52.c) indexes `g_apBrStrTable` and slice4_52.h
 * says outright that the table is "filled by whoever loads the localised
 * string resource". Nobody does: the strings live in the satellite
 * `BRString.dll` (the name is at 0x100B7CA8 in the image), a Win32 resource
 * DLL that is not in this tree and could not be read by portable code if it
 * were. So every id returns NULL.
 *
 * For fifteen of the sixteen ported builders that is harmless -- they hand the
 * pointer straight to the control's setText slot, which tolerates NULL.
 * BrOptFn100575F0 is the exception and the reason this exists: it is the only
 * builder that DEREFERENCES a string it looked up, at slice6_71.c:660,
 *
 *     if (strlen(g_brS71.pA9D018) > 1) pszSrc = g_brS71.pA9D018;
 *     else                             pszSrc = BrStrGet(0xC1);
 *     ... strlen(pszSrc) ...
 *
 * i.e. "no session name yet, use the default name". The harness's session name
 * IS empty -- BrPlatGetUserName is still a stub -- so the fallback is the
 * genuine path, not an edge case, and it strlen'd NULL.
 *
 * WHAT IS PUT IN THE TABLE, AND WHY IT LOOKS LIKE THAT
 *
 * Every valid handle gets `<str NNN>` with its own id in decimal. Three
 * properties are deliberate:
 *
 *   - It is NOT plausible UI text. br_wire71.c/br_wire72.c refuse to invent
 *     save-game names on the grounds that fictional data on screen is worse
 *     than none; the same rule applies to labels. A marker that is obviously a
 *     marker does not pretend to be a translation.
 *   - It is NOT empty. An empty string would keep the table non-NULL while
 *     silently zeroing every measured width, which would quietly gut the one
 *     thing the geometry half of this harness is for.
 *   - It NAMES THE ID, so a control's rectangle can be traced back to the
 *     string it was sized from.
 *
 * Handle 0 stays NULL: it is the reserved "no string" and BrStrGet rejects it
 * by range test anyway, so filling it would model something the original does
 * not have.
 * ========================================================================== */

#define BR_HOST_STR_ROOM  16
static char g_aStrText[BR_STR_TABLE_COUNT][BR_HOST_STR_ROOM];

static void WireStringTable(void)
{
    int id;

    for (id = BR_HANDLE_MIN; id <= BR_HANDLE_MAX; ++id) {
        snprintf(g_aStrText[id], sizeof g_aStrText[id], "<str %d>", id);
        g_apBrStrTable[id] = g_aStrText[id];
    }
    g_apBrStrTable[0] = NULL;       /* reserved "none"; see above */
}

void BrHostWire77(void);
void BrHostWire77(void)
{
    /* BrDiObj declares the vtable as BrDiVtbl *; slice3_45.c's BrDiRoot()
     * helper casts it back on every use, which is how the original's one
     * object type serves three different interfaces. The cast here is the
     * matching half of that and is the same one test_slice3_45.c makes. */
    g_root.pVtbl   = (const BrDiVtbl *)(const void *)&g_rootVtbl;
    g_pBr18ABD70   = &g_root;

    WireStringTable();
}
