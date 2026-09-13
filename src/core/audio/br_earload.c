/* br_earload.c -- the EAR sound-DLL loader (0x10017910).
 *
 * Fresh transcription from build/ghidra_decomp/0x10017910.c against the
 * original bytes, 2026-09-13.  Matching arm only; the port keeps its own
 * loader (slice1_04).
 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <string.h>

extern HMODULE DAT_104b160c;        /* the EAR module */
extern int     _DAT_104b1684;       /* asked for earpds */
extern int     _DAT_104b167c;       /* had to LoadLibrary */
extern int     _DAT_104b1680;       /* fell back to earpds */
extern UINT    DAT_104b1620;        /* the registered window message */

extern FARPROC DAT_104b15f8;
extern FARPROC DAT_104b15fc;
extern FARPROC DAT_104b1600;
extern FARPROC DAT_104b1604;
extern FARPROC DAT_104b1608;
extern FARPROC DAT_104b1610;
extern FARPROC DAT_104b1614;
extern FARPROC DAT_104b1618;
extern FARPROC DAT_104b161c;
extern FARPROC DAT_104b1624;
extern FARPROC DAT_104b1628;
extern FARPROC DAT_104b162c;
extern FARPROC DAT_104b1630;
extern FARPROC DAT_104b1634;
extern FARPROC DAT_104b1638;
extern FARPROC DAT_104b163c;
extern FARPROC DAT_104b1640;
extern FARPROC DAT_104b1644;
extern FARPROC DAT_104b1648;
extern FARPROC DAT_104b164c;
extern FARPROC DAT_104b1650;
extern FARPROC DAT_104b1654;
extern FARPROC DAT_104b1658;
extern FARPROC DAT_104b165c;
extern FARPROC DAT_104b1660;
extern FARPROC DAT_104b1664;
extern FARPROC DAT_104b1668;
extern FARPROC DAT_104b166c;
extern FARPROC DAT_104b1670;
extern FARPROC DAT_104b1674;
extern FARPROC DAT_104b1678;
extern char s_earias_dll_100a74e4[];
extern char s_earpds_dll_100a74f0[];
extern char s_EAR_Interactive_Around_Sound_100a7190[];
extern char s__EAR_DLL_AAA_Validate_4_100a74cc[];
extern char s__EAR_DLL_AssignHwnd_4_100a74b4[];
extern char s__EAR_DLL_ChangeChannelControl_8_100a7494[];
extern char s__EAR_DLL_ClearChannel_8_100a747c[];
extern char s__EAR_DLL_EarInactive_0_100a7464[];
extern char s__EAR_DLL_GetEventStatus_8_100a7448[];
extern char s__EAR_DLL_GetLastError_0_100a7430[];
extern char s__EAR_DLL_GetVersion_0_100a7418[];
extern char s__EAR_DLL_InitializeEar_4_100a73fc[];
extern char s__EAR_DLL_MixEvent_4_100a73e8[];
extern char s__EAR_DLL_MoveEvent_4_100a73d0[];
extern char s__EAR_DLL_RegisterBank_8_100a73b8[];
extern char s__EAR_DLL_RegisterChannel_16_100a739c[];
extern char s__EAR_DLL_RegisterEnvironment_4_100a737c[];
extern char s__EAR_DLL_RegisterMatrix_4_100a7360[];
extern char s__EAR_DLL_RegisterPreset_8_100a7344[];
extern char s__EAR_DLL_ResetEar_0_100a7330[];
extern char s__EAR_DLL_SetAttenuationLevel_8_100a7310[];
extern char s__EAR_DLL_SetUserDistanceUnit_8_100a72f0[];
extern char s__EAR_DLL_ShowLastError_0_100a72d4[];
extern char s__EAR_DLL_ShutDownBank_4_100a72bc[];
extern char s__EAR_DLL_ShutDownChannel_4_100a72a0[];
extern char s__EAR_DLL_ShutDownEar_0_100a7288[];
extern char s__EAR_DLL_ShutDownEnvironment_4_100a7268[];
extern char s__EAR_DLL_ShutDownEvent_8_100a724c[];
extern char s__EAR_DLL_ShutDownMatrix_4_100a7230[];
extern char s__EAR_DLL_ShutDownPreset_4_100a7214[];
extern char s__EAR_DLL_ShutDownTimer_0_100a71c8[];
extern char s__EAR_DLL_StartEvent_4_100a71fc[];
extern char s__EAR_DLL_StartTimer_0_100a71e4[];
extern char s__EAR_DLL_UpdateEar_0_100a71b0[];

/* WHAT IT DOES: loads the EAR 3D-sound library and looks up its thirty-one
 * entry points. Asking for the software mixer names earpds.dll, otherwise
 * earias.dll is tried first and earpds.dll used as the fallback; a library
 * already loaded is reused. Any missing entry point fails the whole load.
 * On success the "EAR Interactive Around-Sound" window message is
 * registered and 1 is returned. */
/* T2 -> T3 2026-09-13 (fresh transcription, 30 fn.py compiles): 1306/1306 B,
 * 406/406 insns, register-blind residue 0+0.  What the bytes taught: the
 * DLL name is strcpy'd INSIDE each arm of the usePds test (the destination
 * lea is duplicated into both arms and VC5 cross-jumps the copy body), the
 * pds arm is the if body; the thirty-one null tests are one bitwise `|`
 * chain in the order the draft prints them (`||` branches; VC5 keeps the
 * `|` chain branchless with sete/or).  RESIDUE: the chain's register plan --
 * the original loads its first two terms into edx/ebx and tests the
 * register-resident last-lookup value ninth; ours loads into ebx/edi and
 * tests it sixth.  Dead: the resident term at positions 10..16, a
 * right-associated chain, `== 0` / `!x` / `== NULL` terms, pairwise
 * grouping, a module-handle local (drops 29 insns), swapped arm order. */
/* @t4-pass 0x10017910 1 2026-09-13 probes 10 bytes 1306 insns 406 regions 1 rows 0 census no  (hand, fn.py variants: resident-term positions 10/11/12/14/16, right-assoc chain, == 0 / !x / == NULL terms, pairwise groups) */
/* @t4-pass 0x10017910 2 2026-09-13 probes 14 bytes 1306 insns 406 regions 1 rows 0 census yes  (slot census: szName is the only frame object, usePds read once; fn.py variants: guard spellings, szName sizing, assignment-in-condition forms, arm order, return forms, chain indentation) */
/* @implements 0x10017910 glide BrEarLoad */
int BrEarLoad(int usePds)
{
    char  szName[12];

    if (DAT_104b160c != (HMODULE)0 && DAT_104b1664 != (FARPROC)0) {
        return 1;
    }
    if (usePds != 0) {
        _DAT_104b1684 = 1;
        strcpy(szName, s_earpds_dll_100a74f0);
    } else {
        strcpy(szName, s_earias_dll_100a74e4);
    }
    DAT_104b160c = GetModuleHandleA(szName);
    if (DAT_104b160c == (HMODULE)0) {
        _DAT_104b167c = 1;
        DAT_104b160c = LoadLibraryA(szName);
        if (DAT_104b160c == (HMODULE)0 && usePds == 0) {
            _DAT_104b167c = usePds;
            _DAT_104b1680 = 1;
            DAT_104b160c = GetModuleHandleA(s_earpds_dll_100a74f0);
            if (DAT_104b160c == (HMODULE)0) {
                _DAT_104b167c = 1;
                DAT_104b160c = LoadLibraryA(s_earpds_dll_100a74f0);
                if (DAT_104b160c == (HMODULE)0) {
                    return 0;
                }
            }
        }
    }
    if (DAT_104b1664 == (FARPROC)0) {
        DAT_104b1658 = GetProcAddress(DAT_104b160c, s__EAR_DLL_AAA_Validate_4_100a74cc);
        DAT_104b1634 = GetProcAddress(DAT_104b160c, s__EAR_DLL_AssignHwnd_4_100a74b4);
        DAT_104b162c = GetProcAddress(DAT_104b160c, s__EAR_DLL_ChangeChannelControl_8_100a7494);
        DAT_104b1628 = GetProcAddress(DAT_104b160c, s__EAR_DLL_ClearChannel_8_100a747c);
        DAT_104b1608 = GetProcAddress(DAT_104b160c, s__EAR_DLL_EarInactive_0_100a7464);
        DAT_104b165c = GetProcAddress(DAT_104b160c, s__EAR_DLL_GetEventStatus_8_100a7448);
        DAT_104b166c = GetProcAddress(DAT_104b160c, s__EAR_DLL_GetLastError_0_100a7430);
        DAT_104b15f8 = GetProcAddress(DAT_104b160c, s__EAR_DLL_GetVersion_0_100a7418);
        DAT_104b1668 = GetProcAddress(DAT_104b160c, s__EAR_DLL_InitializeEar_4_100a73fc);
        DAT_104b1614 = GetProcAddress(DAT_104b160c, s__EAR_DLL_MixEvent_4_100a73e8);
        DAT_104b1630 = GetProcAddress(DAT_104b160c, s__EAR_DLL_MoveEvent_4_100a73d0);
        DAT_104b1674 = GetProcAddress(DAT_104b160c, s__EAR_DLL_RegisterBank_8_100a73b8);
        DAT_104b1648 = GetProcAddress(DAT_104b160c, s__EAR_DLL_RegisterChannel_16_100a739c);
        DAT_104b1654 = GetProcAddress(DAT_104b160c, s__EAR_DLL_RegisterEnvironment_4_100a737c);
        DAT_104b1678 = GetProcAddress(DAT_104b160c, s__EAR_DLL_RegisterMatrix_4_100a7360);
        DAT_104b1670 = GetProcAddress(DAT_104b160c, s__EAR_DLL_RegisterPreset_8_100a7344);
        DAT_104b163c = GetProcAddress(DAT_104b160c, s__EAR_DLL_ResetEar_0_100a7330);
        DAT_104b15fc = GetProcAddress(DAT_104b160c, s__EAR_DLL_SetAttenuationLevel_8_100a7310);
        DAT_104b1660 = GetProcAddress(DAT_104b160c, s__EAR_DLL_SetUserDistanceUnit_8_100a72f0);
        DAT_104b1650 = GetProcAddress(DAT_104b160c, s__EAR_DLL_ShowLastError_0_100a72d4);
        DAT_104b1600 = GetProcAddress(DAT_104b160c, s__EAR_DLL_ShutDownBank_4_100a72bc);
        DAT_104b1618 = GetProcAddress(DAT_104b160c, s__EAR_DLL_ShutDownChannel_4_100a72a0);
        DAT_104b161c = GetProcAddress(DAT_104b160c, s__EAR_DLL_ShutDownEar_0_100a7288);
        DAT_104b164c = GetProcAddress(DAT_104b160c, s__EAR_DLL_ShutDownEnvironment_4_100a7268);
        DAT_104b1640 = GetProcAddress(DAT_104b160c, s__EAR_DLL_ShutDownEvent_8_100a724c);
        DAT_104b1604 = GetProcAddress(DAT_104b160c, s__EAR_DLL_ShutDownMatrix_4_100a7230);
        DAT_104b1638 = GetProcAddress(DAT_104b160c, s__EAR_DLL_ShutDownPreset_4_100a7214);
        DAT_104b1624 = GetProcAddress(DAT_104b160c, s__EAR_DLL_StartEvent_4_100a71fc);
        DAT_104b1644 = GetProcAddress(DAT_104b160c, s__EAR_DLL_StartTimer_0_100a71e4);
        DAT_104b1610 = GetProcAddress(DAT_104b160c, s__EAR_DLL_ShutDownTimer_0_100a71c8);
        DAT_104b1664 = GetProcAddress(DAT_104b160c, s__EAR_DLL_UpdateEar_0_100a71b0);
        if ((DAT_104b164c == (FARPROC)0) |
            (DAT_104b161c == (FARPROC)0) |
            (DAT_104b1604 == (FARPROC)0) |
            (DAT_104b1640 == (FARPROC)0) |
            (DAT_104b1610 == (FARPROC)0) |
            (DAT_104b1638 == (FARPROC)0) |
            (DAT_104b1644 == (FARPROC)0) |
            (DAT_104b1624 == (FARPROC)0) |
            (DAT_104b1664 == (FARPROC)0) |
            (DAT_104b15f8 == (FARPROC)0) |
            (DAT_104b166c == (FARPROC)0) |
            (DAT_104b1614 == (FARPROC)0) |
            (DAT_104b1668 == (FARPROC)0) |
            (DAT_104b1674 == (FARPROC)0) |
            (DAT_104b1630 == (FARPROC)0) |
            (DAT_104b1654 == (FARPROC)0) |
            (DAT_104b1648 == (FARPROC)0) |
            (DAT_104b1670 == (FARPROC)0) |
            (DAT_104b1678 == (FARPROC)0) |
            (DAT_104b15fc == (FARPROC)0) |
            (DAT_104b163c == (FARPROC)0) |
            (DAT_104b1650 == (FARPROC)0) |
            (DAT_104b1660 == (FARPROC)0) |
            (DAT_104b1618 == (FARPROC)0) |
            (DAT_104b1600 == (FARPROC)0) |
            (DAT_104b1634 == (FARPROC)0) |
            (DAT_104b1658 == (FARPROC)0) |
            (DAT_104b1628 == (FARPROC)0) |
            (DAT_104b162c == (FARPROC)0) |
            (DAT_104b165c == (FARPROC)0) |
            (DAT_104b1608 == (FARPROC)0)) {
            return 0;
        }
        DAT_104b1620 = RegisterWindowMessageA(s_EAR_Interactive_Around_Sound_100a7190);
    }
    return 1;
}

#endif /* BR_MATCHING_BUILD */
