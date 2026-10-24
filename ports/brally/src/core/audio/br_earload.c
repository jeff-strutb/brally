/* br_earload.c -- the EAR sound-DLL loader (0x10017910).
 *
 * The original loaded the engine's DLL; the port links it in (br_ear.h).
 */

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include <windows.h>
#include "br_ear.h"
#include <string.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* the EAR module */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* asked for earpds */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* had to LoadLibrary */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* fell back to earpds */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* the registered window message */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: fills the table of the EAR 3D-sound engine's thirty-one
 * entry points the game calls it through. The original loaded the engine's
 * DLL to find them (earpds.dll when asked for the software mixer, else
 * earias.dll with earpds.dll as the fallback) and failed if any was
 * missing; the port links the engine in (br_ear.h), so every entry is
 * there. Registers the "EAR Interactive Around-Sound" window message and
 * returns 1; a table already filled is kept. */
/* @implements 0x10017910 glide BrEarLoad */
int BrEarLoad(int usePds)
{
    static int s_loaded;            /* in the process already: the original's GetModuleHandleA */

    if (DAT_104b160c != (HMODULE)0 && DAT_104b1664 != (FARPROC)0) {
        return 1;
    }
    if (usePds != 0) {
        _DAT_104b1684 = 1;
    }
    if (!s_loaded) {
        _DAT_104b167c = 1;          /* loaded now rather than found loaded */
        s_loaded = 1;
    }
    DAT_104b160c = (HMODULE)&s_loaded;   /* the engine, as the module handle the game tests */
    if (DAT_104b1664 == (FARPROC)0) {
        DAT_104b1658 = (FARPROC)br_ear_AAA_Validate;
        DAT_104b1634 = (FARPROC)br_ear_AssignHwnd;
        (*(FARPROC *)&DAT_104b162c) = (FARPROC)br_ear_ChangeChannelControl;
        (*(FARPROC *)&g_pfn575480) = (FARPROC)br_ear_ClearChannel;
        DAT_104b1608 = (FARPROC)br_ear_EarInactive;
        DAT_104b165c = (FARPROC)br_ear_GetEventStatus;
        DAT_104b166c = (FARPROC)br_ear_GetLastError;
        DAT_104b15f8 = (FARPROC)br_ear_GetVersion;
        DAT_104b1668 = (FARPROC)br_ear_InitializeEar;
        (*(FARPROC *)&g_pfn57546C) = (FARPROC)br_ear_MixEvent;
        DAT_104b1630 = (FARPROC)br_ear_MoveEvent;
        DAT_104b1674 = (FARPROC)br_ear_RegisterBank;
        (*(FARPROC *)&DAT_104b1648) = (FARPROC)br_ear_RegisterChannel;
        DAT_104b1654 = (FARPROC)br_ear_RegisterEnvironment;
        (*(FARPROC *)&DAT_104b1678) = (FARPROC)br_ear_RegisterMatrix;
        DAT_104b1670 = (FARPROC)br_ear_RegisterPreset;
        DAT_104b163c = (FARPROC)br_ear_ResetEar;
        (*(FARPROC *)&g_575454) = (FARPROC)br_ear_SetAttenuationLevel;
        DAT_104b1660 = (FARPROC)br_ear_SetUserDistanceUnit;
        DAT_104b1650 = (FARPROC)br_ear_ShowLastError;
        DAT_104b1600 = (FARPROC)br_ear_ShutDownBank;
        (*(FARPROC *)&g_575470) = (FARPROC)br_ear_ShutDownChannel;
        DAT_104b161c = (FARPROC)br_ear_ShutDownEar;
        DAT_104b164c = (FARPROC)br_ear_ShutDownEnvironment;
        DAT_104b1640 = (FARPROC)br_ear_ShutDownEvent;
        DAT_104b1604 = (FARPROC)br_ear_ShutDownMatrix;
        DAT_104b1638 = (FARPROC)br_ear_ShutDownPreset;
        DAT_104b1624 = (FARPROC)br_ear_StartEvent;
        DAT_104b1644 = (FARPROC)br_ear_StartTimer;
        DAT_104b1610 = (FARPROC)br_ear_ShutDownTimer;
        DAT_104b1664 = (FARPROC)br_ear_UpdateEar;
        DAT_104b1620 = RegisterWindowMessageA(s_EAR_Interactive_Around_Sound_100a7190);
    }
    return 1;
}

