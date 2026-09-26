/* br_unresolved.c -- port-only storage and stubs for symbols the matching
 * TUs reference but only the Windows image resolves (to original addresses,
 * config/globals_learned.csv) or only a BR_MATCHING_BUILD arm defines.
 *
 * GLOBALS: zero-filled, 4 KB each so an array or struct of unknown extent
 * cannot overrun a neighbour. Names that alias ONE original address in the
 * image are separate objects here -- correct only while nothing relies on
 * the aliasing, which is why this file is a link shim, not a model.
 * FUNCTIONS: br_stub() no-ops, reported at exit like br_stubs.c.
 * WIN32: the six imports these TUs call directly, answered as absent.
 */
#include <stddef.h>

long br_stub_hit(const char *name);

#define BR_UNRES_G(n) __attribute__((aligned(16))) unsigned char n[4096];
BR_UNRES_G(DAT_100788e8)
BR_UNRES_G(DAT_100abde8)
BR_UNRES_G(DAT_100abdf4)
BR_UNRES_G(DAT_100abdf8)
BR_UNRES_G(DAT_100b3014)
BR_UNRES_G(DAT_100bcbe8)
BR_UNRES_G(DAT_102066c8)
BR_UNRES_G(DAT_10226e80)
BR_UNRES_G(DAT_102e16b4)
BR_UNRES_G(DAT_105bc72c)
BR_UNRES_G(DAT_105e1828)
BR_UNRES_G(DAT_10ac40a8)
BR_UNRES_G(DAT_10ac5d58)
BR_UNRES_G(DAT_10ac5d70)
BR_UNRES_G(DAT_10af3bc8)
BR_UNRES_G(g_aBrAiDiffScale)
BR_UNRES_G(g_aBrAiScanA)
BR_UNRES_G(g_aBrAiScanB)
BR_UNRES_G(g_aBrDriverSlot)
BR_UNRES_G(g_aBrRaceSpecial)
BR_UNRES_G(g_apBrImgTintTex)
BR_UNRES_G(g_br100BCBE8)
BR_UNRES_G(g_br100BCBF0)
BR_UNRES_G(g_br100BCBF4)
BR_UNRES_G(g_br100BCBF8)
BR_UNRES_G(g_br100BCBFC)
BR_UNRES_G(g_br100BCC00)
BR_UNRES_G(g_br100BCC04)
BR_UNRES_G(g_br10226A44)
BR_UNRES_G(g_br10226A48)
BR_UNRES_G(g_br10226A50)
BR_UNRES_G(g_br105CCB5C)
BR_UNRES_G(g_br105CCB88)
BR_UNRES_G(g_br10AF21B0)
BR_UNRES_G(g_br118EEE18)
BR_UNRES_G(g_br118EEE8C)
BR_UNRES_G(g_br118EEEE0)
BR_UNRES_G(g_br118EEEE4)
BR_UNRES_G(g_brAiBiasNeg)
BR_UNRES_G(g_brAiBiasPos)
BR_UNRES_G(g_brAiBlend)
BR_UNRES_G(g_brAiScanAim)
BR_UNRES_G(g_brAiScanN)
BR_UNRES_G(g_brAiScanPt)
BR_UNRES_G(g_brInJoyPrev)
BR_UNRES_G(g_brInKeyPrev)
BR_UNRES_G(g_brInMousePrev)
BR_UNRES_G(g_brInputFrame)
BR_UNRES_G(g_brInputLast)
BR_UNRES_G(g_brMouseDivTable)
BR_UNRES_G(g_brMouseSens)
BR_UNRES_G(g_brRaceAirSuppressA)
BR_UNRES_G(g_brRaceAirSuppressB)
BR_UNRES_G(g_brRaceAirSuppressC)
BR_UNRES_G(g_brRaceFlyBlendA)
BR_UNRES_G(g_brRaceFlyBlendB)
BR_UNRES_G(g_brRaceFlyDirCur)
BR_UNRES_G(g_brRaceFlyDirNext)
BR_UNRES_G(g_brRaceFlyDirPrev)
BR_UNRES_G(g_brRaceFlyNode)
BR_UNRES_G(g_brRaceFlyNodeN)
BR_UNRES_G(g_brRaceFlyRemain)
BR_UNRES_G(g_brRaceFlyScale)
BR_UNRES_G(g_brRaceFlySegLen)
BR_UNRES_G(g_brRaceFlySpeedA)
BR_UNRES_G(g_brRaceFlySpeedB)
BR_UNRES_G(g_brRaceFlyStep)
BR_UNRES_G(g_brRaceMode)
BR_UNRES_G(g_brRaceSpecialM)
BR_UNRES_G(g_brRaceSpecialN)
BR_UNRES_G(g_brRaceTrack)
BR_UNRES_G(g_pBrAiPathRoot)
BR_UNRES_G(g_pBrDik18ABDD0)
BR_UNRES_G(g_pBrInDiRoot)
BR_UNRES_G(g_pBrInJoyDev)
BR_UNRES_G(g_pBrMenuRec)
BR_UNRES_G(g_pBrRaceFlyAim)
BR_UNRES_G(g_pBrRaceFlyPos)
BR_UNRES_G(g_pBrRaceObjRec)
BR_UNRES_G(s_SAVING_LAST_LAP_INFO_100b382c)

#define BR_UNRES_F(n) long n(void); long n(void) { return br_stub_hit(#n); }
BR_UNRES_F(BrAiScanCorridor)
BR_UNRES_F(BrCarCtlChain_1006F170)
BR_UNRES_F(BrCtlAiLineStep)
BR_UNRES_F(BrCtlAiRespawn)
BR_UNRES_F(BrExt_10008D60)
BR_UNRES_F(BrExt_10033BB0)
BR_UNRES_F(BrF3DListFixup)
BR_UNRES_F(BrGetFlag_AB4F0)
BR_UNRES_F(BrPodNop)
BR_UNRES_F(BrReplayIsOn)
BR_UNRES_F(BrSub10004F20)
BR_UNRES_F(BrSub10004F50)
BR_UNRES_F(BrSub10063A40)
BR_UNRES_F(BrSub10071550)
BR_UNRES_F(BrVec3Predict)
BR_UNRES_F(FUN_10024490)
BR_UNRES_F(FUN_10036740)
BR_UNRES_F(FUN_10036f40)
BR_UNRES_F(FUN_10072960)
BR_UNRES_F(br_dl_normalise)

/* Win32: no async key state, no loaded modules, no global handles. */
short GetAsyncKeyState(int vk) { (void)vk; return 0; }
void *GetModuleHandleA(const char *n) { (void)n; return NULL; }
void *GetProcAddress(void *m, const char *n) { (void)m; (void)n; return NULL; }
void *GlobalHandle(const void *p) { return (void *)p; }
int GlobalUnlock(void *h) { (void)h; return 0; }
void *GlobalFree(void *h) { (void)h; return NULL; }
