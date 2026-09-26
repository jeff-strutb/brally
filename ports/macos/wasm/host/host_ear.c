/* host_ear.c -- the EAR "Interactive Around-Sound" engine the game loads by
 * name (earias.dll / earpds.dll), answered as the live oracle answers it: a
 * working engine that makes no sound. Register and Start calls hand back a
 * non-zero id, InitializeEar succeeds, status queries report idle. Real audio
 * is an open decision (ports/README.md).
 */
#include "host.h"
#include <string.h>
#include <stdlib.h>

extern _Thread_local const w_fentry *w_last;

static const struct { const char *name; u32 ret; } EAR_RET[] = {
    { "_EAR_DLL_AAA_Validate@4", 1 }, { "_EAR_DLL_AssignHwnd@4", 1 },
    { "_EAR_DLL_InitializeEar@4", 1 }, { "_EAR_DLL_GetLastError@0", 0 },
    { "_EAR_DLL_GetVersion@0", 0x100 }, { "_EAR_DLL_EarInactive@0", 0 },
    { "_EAR_DLL_GetEventStatus@8", 0 },
    { "_EAR_DLL_RegisterBank@8", 1 }, { "_EAR_DLL_RegisterChannel@16", 1 },
    { "_EAR_DLL_RegisterEnvironment@4", 1 }, { "_EAR_DLL_RegisterMatrix@4", 1 },
    { "_EAR_DLL_RegisterPreset@8", 1 }, { "_EAR_DLL_StartEvent@4", 1 },
    { "_EAR_DLL_MixEvent@4", 1 }, { "_EAR_DLL_MoveEvent@4", 1 }, { "_EAR_DLL_StartTimer@0", 1 },
    { "_EAR_DLL_UpdateEar@0", 1 }, { "_EAR_DLL_ResetEar@0", 1 },
};

static u32 ear_ret(void)
{
    const char *n = w_last ? w_last->name : "";
    size_t i;
    for (i = 0; i < sizeof EAR_RET / sizeof EAR_RET[0]; i++)
        if (!strcmp(EAR_RET[i].name, n)) return EAR_RET[i].ret;
    return 0;
}
static u32 e0(void) { return ear_ret(); }
static u32 e1(u32 a) { (void)a; return ear_ret(); }
static u32 e2(u32 a, u32 b) { (void)a; (void)b; return ear_ret(); }
static u32 e3(u32 a, u32 b, u32 c) { (void)a; (void)b; (void)c; return ear_ret(); }
static u32 e4(u32 a, u32 b, u32 c, u32 d) { (void)a; (void)b; (void)c; (void)d; return ear_ret(); }
static u32 e5(u32 a, u32 b, u32 c, u32 d, u32 e) { (void)a; (void)b; (void)c; (void)d; (void)e; return ear_ret(); }
static u32 e6(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; return ear_ret(); }
static u32 e7(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f, u32 g) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; return ear_ret(); }
static u32 e8(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f, u32 g, u32 h) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; return ear_ret(); }
static void *EARFN[] = { (void *)e0, (void *)e1, (void *)e2, (void *)e3, (void *)e4,
                         (void *)e5, (void *)e6, (void *)e7, (void *)e8 };
static const char *SIG[] = { "_i", "i_i", "ii_i", "iii_i", "iiii_i", "iiiii_i", "iiiiii_i",
                             "iiiiiii_i", "iiiiiiii_i" };

u32 hear_proc(const char *name)
{
    const char *at = strrchr(name, '@');
    int n = at ? atoi(at + 1) / 4 : 0;
    if (n > 8) n = 8;
    return w_addr_of_host(EARFN[n], SIG[n], strdup(name));
}
