/* WHAT IT DOES: tears down the current session (net, handles, video) and
 * brings the renderer back up at 640x480x16 if the clock pair drifted. */
/* @implements 0x1002F282 glide BrSessionReinitVideo
 * @cpp_kind free
 * @cpp_symbol _BrSessionReinitVideo
 *
 * cdecl, no args, 142 B, an odd-address /Od function in the 0x1002Fxxx
 * debug stretch (frame pointer, memory-operand compares, every global
 * re-read).  Moved out of ghidra_batch.c on 2026-09-13: that batch is /O2,
 * and the C lane's struct-by-value spelling of the one thiscall
 * (`push 0x10b72f48; mov ecx,0x10b71290; call`) homes the struct in a frame
 * slot under /Od; a class method call spells it directly.
 */
#define _CRTIMP __declspec(dllimport)
#include "slice1_05.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
#include <windows.h>

class Save1290 {
public:
    void Set(void *p);              /* 0x100634B0, thiscall, one stack arg */
};

extern "C" {
/* FUN_1006c460: prototype in br_funcs.h */
/* FUN_10072840: prototype in br_funcs.h */
/* FUN_1006a320: prototype in br_funcs.h */
/* FUN_10005cd0: prototype in br_funcs.h */
/* FUN_1001cd50: prototype in br_funcs.h */
/* FUN_10063970: prototype in br_funcs.h */
/* FUN_1005a420: prototype in br_funcs.h */

/* 64-bit core: DAT_106ec760 is defined once, in br_globals.c */
/* 64-bit core: DAT_10b71a68 is defined once, in br_globals.c */
/* 64-bit core: DAT_106e9a34 is defined once, in br_globals.c */
/* 64-bit core: DAT_10b71a6c is defined once, in br_globals.c */
/* 64-bit core: DAT_10b72f48 is defined once, in br_globals.c */
/* 64-bit core: DAT_10b71290 is defined once, in br_globals.c */
/* 64-bit core: DAT_10226a48 is defined once, in br_globals.c */
/* 64-bit core: DAT_106ed6e0 is defined once, in br_globals.c */

void BrSessionReinitVideo(void)
{
    if (((*(int *)&g_brRace6EC760) != (*(int *)&g_brItemIconCount)) || ((*(int *)&g_brRace6E9A34) != (*(int *)&g_brRaceB71A6C))) {
        (*(Save1290 *)&g_BrCtrlCfg).Set(&g_navArg);
    }
    BrSndBankFree();
    BrExt_10079550();
    if ((*(int *)&g_brRaceNet) != 0) {
        if ((*(int *)&g_brRaceNet) > 1) {
            FUN_1006a330();
        }
        BrNetReset();
    }
    BrFlagInit_1002F690();
    CloseHandle((*(HANDLE *)((char *)&g_aBrEntRecs + 0xB0)));
    (*(HANDLE *)((char *)&g_aBrEntRecs + 0xB0)) = 0;
    BrRenderModeStart(3, 0x280, 0x1e0, 0x10, 0);
    FUN_1005a420();
}
}

/* Methods of the local classes above that other files define: each is
 * the function at its original address, reached through its C entry. */
/* 0x100634B0: the original calls BrGlCfgSave by address */
inline void Save1290::Set(void * a1)
{
    BrGlCfgSave((void *)this, (const char *)a1);
}
