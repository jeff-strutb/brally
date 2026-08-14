/* br_stubs.c -- GENERATED. One stub per not-yet-ported function.
 *
 * Purpose: make the game LINK before the core is finished, so it can be RUN.
 * A stub that is never called costs nothing; a stub that IS called tells us
 * exactly which function the boot path needs next. That turns the remaining
 * decompilation into a runtime-ordered priority list instead of guesswork.
 *
 * WHY EVERY STUB IS long(void):
 * C does not mangle names, so a definition links against any caller
 * declaration. Returning `long` puts 0 in rax, which reads correctly as int,
 * pointer or bool at every call site. It is WRONG for a float/double-returning
 * callee (those return in xmm0, which this leaves untouched) -- such a caller
 * will see garbage rather than 0. Stubs that matter get reported below, so a
 * float-returning gap shows up as a hit and gets ported rather than trusted.
 *
 * Regenerate with tools/genstubs.py. Do not hand-edit.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BR_STUB_MAX 152
static const char *g_hit[BR_STUB_MAX];
static unsigned    g_cnt[BR_STUB_MAX];
static int         g_nHit;
static int         g_abort;   /* BR_STUB_ABORT=1 -> die on first hit */

static long br_stub(const char *name)
{
    int i;
    for (i = 0; i < g_nHit; i++)
        if (g_hit[i] == name) { g_cnt[i]++; return 0; }
    if (g_nHit < BR_STUB_MAX) { g_hit[g_nHit] = name; g_cnt[g_nHit++] = 1; }
    if (!g_abort) { static int once; if (!once) { once = 1;
        g_abort = getenv("BR_STUB_ABORT") ? atoi(getenv("BR_STUB_ABORT")) : 0; } }
    if (g_abort) { fprintf(stderr, "\nSTUB HIT (fatal): %s\n", name); abort(); }
    return 0;
}

/* Called at exit: the boot path's actual demand, most-wanted first. */
void BrStubReport(void)
{
    int i, j;
    if (!g_nHit) { printf("stubs: none reached -- everything the run touched is ported\n"); return; }
    printf("\nstubs reached: %d distinct (of %d linked)\n", g_nHit, BR_STUB_MAX);
    for (i = 0; i < g_nHit; i++) {
        int best = i;
        for (j = i + 1; j < g_nHit; j++) if (g_cnt[j] > g_cnt[best]) best = j;
        { const char *tn = g_hit[i]; unsigned tc = g_cnt[i];
          g_hit[i] = g_hit[best]; g_cnt[i] = g_cnt[best];
          g_hit[best] = tn; g_cnt[best] = tc; }
        printf("  %6u  %s\n", g_cnt[i], g_hit[i]);
    }
}

long BrAppMsg107(void); long BrAppMsg107(void) { return br_stub("BrAppMsg107"); }
long BrBitReaderRead(void); long BrBitReaderRead(void) { return br_stub("BrBitReaderRead"); }
long BrBitStreamWriteBits(void); long BrBitStreamWriteBits(void) { return br_stub("BrBitStreamWriteBits"); }
long BrCarRecordFromState(void); long BrCarRecordFromState(void) { return br_stub("BrCarRecordFromState"); }
long BrCarRecordToState(void); long BrCarRecordToState(void) { return br_stub("BrCarRecordToState"); }
long BrCdTrackGet(void); long BrCdTrackGet(void) { return br_stub("BrCdTrackGet"); }
long BrChkFClose(void); long BrChkFClose(void) { return br_stub("BrChkFClose"); }
long BrChkFReadOpen(void); long BrChkFReadOpen(void) { return br_stub("BrChkFReadOpen"); }
long BrChkFileSize(void); long BrChkFileSize(void) { return br_stub("BrChkFileSize"); }
long BrDlIsRegistered(void); long BrDlIsRegistered(void) { return br_stub("BrDlIsRegistered"); }
long BrEnt35CE0(void); long BrEnt35CE0(void) { return br_stub("BrEnt35CE0"); }
long BrErrorf(void); long BrErrorf(void) { return br_stub("BrErrorf"); }
long BrExt_10008B80(void); long BrExt_10008B80(void) { return br_stub("BrExt_10008B80"); }
long BrExt_1003D0B0(void); long BrExt_1003D0B0(void) { return br_stub("BrExt_1003D0B0"); }
long BrExt_1003DFC0(void); long BrExt_1003DFC0(void) { return br_stub("BrExt_1003DFC0"); }
long BrExt_1003E0E0(void); long BrExt_1003E0E0(void) { return br_stub("BrExt_1003E0E0"); }
long BrExt_1003E310(void); long BrExt_1003E310(void) { return br_stub("BrExt_1003E310"); }
long BrExt_10041AC0(void); long BrExt_10041AC0(void) { return br_stub("BrExt_10041AC0"); }
long BrExt_10041BD0(void); long BrExt_10041BD0(void) { return br_stub("BrExt_10041BD0"); }
long BrExt_10043260(void); long BrExt_10043260(void) { return br_stub("BrExt_10043260"); }
long BrExt_10043330(void); long BrExt_10043330(void) { return br_stub("BrExt_10043330"); }
long BrExt_10043BF0(void); long BrExt_10043BF0(void) { return br_stub("BrExt_10043BF0"); }
long BrExt_10043CD0(void); long BrExt_10043CD0(void) { return br_stub("BrExt_10043CD0"); }
long BrExt_10044280(void); long BrExt_10044280(void) { return br_stub("BrExt_10044280"); }
long BrExt_100443E0(void); long BrExt_100443E0(void) { return br_stub("BrExt_100443E0"); }
long BrExt_10045A00(void); long BrExt_10045A00(void) { return br_stub("BrExt_10045A00"); }
long BrExt_10047660(void); long BrExt_10047660(void) { return br_stub("BrExt_10047660"); }
long BrExt_10049C20(void); long BrExt_10049C20(void) { return br_stub("BrExt_10049C20"); }
long BrExt_1004A260(void); long BrExt_1004A260(void) { return br_stub("BrExt_1004A260"); }
long BrExt_1004D1F0(void); long BrExt_1004D1F0(void) { return br_stub("BrExt_1004D1F0"); }
long BrExt_1004DB00(void); long BrExt_1004DB00(void) { return br_stub("BrExt_1004DB00"); }
long BrExt_100509F0(void); long BrExt_100509F0(void) { return br_stub("BrExt_100509F0"); }
long BrExt_10052F50(void); long BrExt_10052F50(void) { return br_stub("BrExt_10052F50"); }
long BrExt_10053CF0(void); long BrExt_10053CF0(void) { return br_stub("BrExt_10053CF0"); }
long BrExt_10058750(void); long BrExt_10058750(void) { return br_stub("BrExt_10058750"); }
long BrExt_10059BB0(void); long BrExt_10059BB0(void) { return br_stub("BrExt_10059BB0"); }
long BrExt_1006A4A0(void); long BrExt_1006A4A0(void) { return br_stub("BrExt_1006A4A0"); }
long BrExt_10072AF0(void); long BrExt_10072AF0(void) { return br_stub("BrExt_10072AF0"); }
long BrExt_10074030(void); long BrExt_10074030(void) { return br_stub("BrExt_10074030"); }
long BrExt_10079550(void); long BrExt_10079550(void) { return br_stub("BrExt_10079550"); }
long BrFn1003D210(void); long BrFn1003D210(void) { return br_stub("BrFn1003D210"); }
long BrFtolArg(void); long BrFtolArg(void) { return br_stub("BrFtolArg"); }
long BrGbiCall1001D420(void); long BrGbiCall1001D420(void) { return br_stub("BrGbiCall1001D420"); }
long BrGbiCall10029470(void); long BrGbiCall10029470(void) { return br_stub("BrGbiCall10029470"); }
long BrGfx2F900(void); long BrGfx2F900(void) { return br_stub("BrGfx2F900"); }
long BrGfx42AF0_3(void); long BrGfx42AF0_3(void) { return br_stub("BrGfx42AF0_3"); }
long BrGlobalFree(void); long BrGlobalFree(void) { return br_stub("BrGlobalFree"); }
long BrGlobalHandle(void); long BrGlobalHandle(void) { return br_stub("BrGlobalHandle"); }
long BrGlobalUnlock(void); long BrGlobalUnlock(void) { return br_stub("BrGlobalUnlock"); }
long BrItoa(void); long BrItoa(void) { return br_stub("BrItoa"); }
long BrModelVtxResolve(void); long BrModelVtxResolve(void) { return br_stub("BrModelVtxResolve"); }
long BrNetAnnounce(void); long BrNetAnnounce(void) { return br_stub("BrNetAnnounce"); }
long BrNetMutexLock(void); long BrNetMutexLock(void) { return br_stub("BrNetMutexLock"); }
long BrNetMutexUnlock(void); long BrNetMutexUnlock(void) { return br_stub("BrNetMutexUnlock"); }
long BrNetSend4760(void); long BrNetSend4760(void) { return br_stub("BrNetSend4760"); }
long BrNetSendDelta(void); long BrNetSendDelta(void) { return br_stub("BrNetSendDelta"); }
long BrNetSendFull(void); long BrNetSendFull(void) { return br_stub("BrNetSendFull"); }
long BrNetSlotPredictOrig(void); long BrNetSlotPredictOrig(void) { return br_stub("BrNetSlotPredictOrig"); }
long BrOptFn10044970(void); long BrOptFn10044970(void) { return br_stub("BrOptFn10044970"); }
long BrOptFn1004CAC0(void); long BrOptFn1004CAC0(void) { return br_stub("BrOptFn1004CAC0"); }
long BrOptFn10056FF0(void); long BrOptFn10056FF0(void) { return br_stub("BrOptFn10056FF0"); }
long BrOptFn10058750(void); long BrOptFn10058750(void) { return br_stub("BrOptFn10058750"); }
long BrPhaseCtor(void); long BrPhaseCtor(void) { return br_stub("BrPhaseCtor"); }
long BrPhaseEnterPlaceholder_1004A580(void); long BrPhaseEnterPlaceholder_1004A580(void) { return br_stub("BrPhaseEnterPlaceholder_1004A580"); }
long BrPhaseEnterPlaceholder_1004B430(void); long BrPhaseEnterPlaceholder_1004B430(void) { return br_stub("BrPhaseEnterPlaceholder_1004B430"); }
long BrPhaseEnterPlaceholder_1004BDC0(void); long BrPhaseEnterPlaceholder_1004BDC0(void) { return br_stub("BrPhaseEnterPlaceholder_1004BDC0"); }
long BrPhaseEnterPlaceholder_1004C4A0(void); long BrPhaseEnterPlaceholder_1004C4A0(void) { return br_stub("BrPhaseEnterPlaceholder_1004C4A0"); }
long BrPhaseVtbl_1008F700(void); long BrPhaseVtbl_1008F700(void) { return br_stub("BrPhaseVtbl_1008F700"); }
long BrPlatGetUserName(void); long BrPlatGetUserName(void) { return br_stub("BrPlatGetUserName"); }
long BrPlatQueryPerfCounter(void); long BrPlatQueryPerfCounter(void) { return br_stub("BrPlatQueryPerfCounter"); }
long BrPlatQueryPerfFreq(void); long BrPlatQueryPerfFreq(void) { return br_stub("BrPlatQueryPerfFreq"); }
long BrPlatTimeGetTime(void); long BrPlatTimeGetTime(void) { return br_stub("BrPlatTimeGetTime"); }
long BrPodWriterMakeName(void); long BrPodWriterMakeName(void) { return br_stub("BrPodWriterMakeName"); }
long BrProbe1006F310(void); long BrProbe1006F310(void) { return br_stub("BrProbe1006F310"); }
long BrRand(void); long BrRand(void) { return br_stub("BrRand"); }
long BrScrSleep(void); long BrScrSleep(void) { return br_stub("BrScrSleep"); }
long BrSegSetFlag(void); long BrSegSetFlag(void) { return br_stub("BrSegSetFlag"); }
long BrStub10008B80(void); long BrStub10008B80(void) { return br_stub("BrStub10008B80"); }
long BrStub8B80_0(void); long BrStub8B80_0(void) { return br_stub("BrStub8B80_0"); }
long BrStub8B80_1i(void); long BrStub8B80_1i(void) { return br_stub("BrStub8B80_1i"); }
long BrStub8B80_1p(void); long BrStub8B80_1p(void) { return br_stub("BrStub8B80_1p"); }
long BrStub8B80_5i(void); long BrStub8B80_5i(void) { return br_stub("BrStub8B80_5i"); }
long BrSub100027F0(void); long BrSub100027F0(void) { return br_stub("BrSub100027F0"); }
long BrSub10002870(void); long BrSub10002870(void) { return br_stub("BrSub10002870"); }
long BrSub10005D30(void); long BrSub10005D30(void) { return br_stub("BrSub10005D30"); }
long BrSub10005FE0(void); long BrSub10005FE0(void) { return br_stub("BrSub10005FE0"); }
long BrSub1000BAF0(void); long BrSub1000BAF0(void) { return br_stub("BrSub1000BAF0"); }
long BrSub1003289F(void); long BrSub1003289F(void) { return br_stub("BrSub1003289F"); }
long BrSub1003445A(void); long BrSub1003445A(void) { return br_stub("BrSub1003445A"); }
long BrSub10035BD1(void); long BrSub10035BD1(void) { return br_stub("BrSub10035BD1"); }
long BrSub100360F0(void); long BrSub100360F0(void) { return br_stub("BrSub100360F0"); }
long BrSub10037990(void); long BrSub10037990(void) { return br_stub("BrSub10037990"); }
long BrSub1003C520(void); long BrSub1003C520(void) { return br_stub("BrSub1003C520"); }
long BrSub1003C550(void); long BrSub1003C550(void) { return br_stub("BrSub1003C550"); }
long BrSub1003C5C0(void); long BrSub1003C5C0(void) { return br_stub("BrSub1003C5C0"); }
long BrSub1003C740(void); long BrSub1003C740(void) { return br_stub("BrSub1003C740"); }
long BrSub1003CC70(void); long BrSub1003CC70(void) { return br_stub("BrSub1003CC70"); }
long BrSub1003CDA0(void); long BrSub1003CDA0(void) { return br_stub("BrSub1003CDA0"); }
long BrSub1003D210(void); long BrSub1003D210(void) { return br_stub("BrSub1003D210"); }
long BrSub1003D480(void); long BrSub1003D480(void) { return br_stub("BrSub1003D480"); }
long BrSub1003E1D0(void); long BrSub1003E1D0(void) { return br_stub("BrSub1003E1D0"); }
long BrSub100484E0(void); long BrSub100484E0(void) { return br_stub("BrSub100484E0"); }
long BrSub100586A0(void); long BrSub100586A0(void) { return br_stub("BrSub100586A0"); }
long BrSub10058700(void); long BrSub10058700(void) { return br_stub("BrSub10058700"); }
long BrSub1005F5A0(void); long BrSub1005F5A0(void) { return br_stub("BrSub1005F5A0"); }
long BrSub100607B0(void); long BrSub100607B0(void) { return br_stub("BrSub100607B0"); }
long BrSub10060D90(void); long BrSub10060D90(void) { return br_stub("BrSub10060D90"); }
long BrSub10061010(void); long BrSub10061010(void) { return br_stub("BrSub10061010"); }
long BrSub10062C50(void); long BrSub10062C50(void) { return br_stub("BrSub10062C50"); }
long BrSub100695D0(void); long BrSub100695D0(void) { return br_stub("BrSub100695D0"); }
long BrSub1006F4A0(void); long BrSub1006F4A0(void) { return br_stub("BrSub1006F4A0"); }
long BrSub10070610(void); long BrSub10070610(void) { return br_stub("BrSub10070610"); }
long BrSub10070E60(void); long BrSub10070E60(void) { return br_stub("BrSub10070E60"); }
long BrSub10071480(void); long BrSub10071480(void) { return br_stub("BrSub10071480"); }
long BrSub10072270(void); long BrSub10072270(void) { return br_stub("BrSub10072270"); }
long BrSub10074DC0(void); long BrSub10074DC0(void) { return br_stub("BrSub10074DC0"); }
long BrSub100773F0(void); long BrSub100773F0(void) { return br_stub("BrSub100773F0"); }
long BrSub1007A840(void); long BrSub1007A840(void) { return br_stub("BrSub1007A840"); }
long BrSub1007A940(void); long BrSub1007A940(void) { return br_stub("BrSub1007A940"); }
long BrSub_10019240(void); long BrSub_10019240(void) { return br_stub("BrSub_10019240"); }
long BrSub_10019250(void); long BrSub_10019250(void) { return br_stub("BrSub_10019250"); }
long BrSub_100193C0(void); long BrSub_100193C0(void) { return br_stub("BrSub_100193C0"); }
long BrSub_100290A0(void); long BrSub_100290A0(void) { return br_stub("BrSub_100290A0"); }
long BrSub_1002F900(void); long BrSub_1002F900(void) { return br_stub("BrSub_1002F900"); }
long BrSwapRec24Array(void); long BrSwapRec24Array(void) { return br_stub("BrSwapRec24Array"); }
long BrSwapRec8Array(void); long BrSwapRec8Array(void) { return br_stub("BrSwapRec8Array"); }
long BrTextAlignCentre(void); long BrTextAlignCentre(void) { return br_stub("BrTextAlignCentre"); }
long BrTextFlag358Clear(void); long BrTextFlag358Clear(void) { return br_stub("BrTextFlag358Clear"); }
long BrTextSetColor6(void); long BrTextSetColor6(void) { return br_stub("BrTextSetColor6"); }
long BrTextSetSize(void); long BrTextSetSize(void) { return br_stub("BrTextSetSize"); }
long BrUiPageVtbl_1008F6F8(void); long BrUiPageVtbl_1008F6F8(void) { return br_stub("BrUiPageVtbl_1008F6F8"); }
long BrVec3Len(void); long BrVec3Len(void) { return br_stub("BrVec3Len"); }
long BrX10005DE0(void); long BrX10005DE0(void) { return br_stub("BrX10005DE0"); }
long BrX10005E70(void); long BrX10005E70(void) { return br_stub("BrX10005E70"); }
long BrX1002C2C0(void); long BrX1002C2C0(void) { return br_stub("BrX1002C2C0"); }
long BrX1002C500(void); long BrX1002C500(void) { return br_stub("BrX1002C500"); }
long BrX10034C66(void); long BrX10034C66(void) { return br_stub("BrX10034C66"); }
long BrX1003563A(void); long BrX1003563A(void) { return br_stub("BrX1003563A"); }
long BrX10035BBA(void); long BrX10035BBA(void) { return br_stub("BrX10035BBA"); }
long BrX100397C0(void); long BrX100397C0(void) { return br_stub("BrX100397C0"); }
long BrX10042AF0(void); long BrX10042AF0(void) { return br_stub("BrX10042AF0"); }
long BrX10060E90(void); long BrX10060E90(void) { return br_stub("BrX10060E90"); }
long BrX100664C0(void); long BrX100664C0(void) { return br_stub("BrX100664C0"); }
long BrX10068260(void); long BrX10068260(void) { return br_stub("BrX10068260"); }
long BrX10069490(void); long BrX10069490(void) { return br_stub("BrX10069490"); }
long BrX10069530(void); long BrX10069530(void) { return br_stub("BrX10069530"); }
long BrX10072580(void); long BrX10072580(void) { return br_stub("BrX10072580"); }
long BrX100751D0(void); long BrX100751D0(void) { return br_stub("BrX100751D0"); }
long BrX10075F10(void); long BrX10075F10(void) { return br_stub("BrX10075F10"); }
long BrX10076AE0(void); long BrX10076AE0(void) { return br_stub("BrX10076AE0"); }
long BrXAtExit(void); long BrXAtExit(void) { return br_stub("BrXAtExit"); }


/* ==========================================================================
 * PROVISIONAL STORAGE for data symbols that no ported module defines yet.
 *
 * These are `extern` arrays and objects the original keeps in .data/.bss.
 * The module that will own each one has not been ported, so nothing defines
 * them and the link fails on DATA rather than on code.
 *
 * Each is given a generous, 8-byte-aligned zeroed block. That is safe for the
 * bring-up harness for two reasons: the original's .bss starts zeroed too, and
 * over-allocating cannot corrupt a neighbour the way under-allocating would.
 * It is NOT a substitute for porting the owning module -- the real definitions
 * carry initialisers this cannot know. Anything whose behaviour depends on a
 * non-zero initial value will read 0 here and behave differently.
 *
 * Sizes are deliberately uniform rather than guessed per symbol: a wrong guess
 * that is too SMALL is a silent heap corruption, and there is no evidence here
 * to guess correctly with. 1 MiB of .bss costs nothing on disk.
 *
 * Every symbol below is a porting TODO, not a finished decision.
 * ========================================================================== */
#define BR_PROV_QWORDS (1024 * 1024 / 8)

long long g_aBr0B3820[BR_PROV_QWORDS];
long long g_aBrA9D078[BR_PROV_QWORDS];
long long g_aBrAC308[BR_PROV_QWORDS];
long long g_aBrAC368[BR_PROV_QWORDS];
long long g_aBrAC3B0[BR_PROV_QWORDS];
long long g_aBrAC3C8[BR_PROV_QWORDS];
long long g_aBrAC420[BR_PROV_QWORDS];
long long g_aBrAC4A0[BR_PROV_QWORDS];
long long g_aBrAC4B0[BR_PROV_QWORDS];
long long g_aBrAC4C0[BR_PROV_QWORDS];
long long g_aBrAC4D8[BR_PROV_QWORDS];
long long g_aBrAC518[BR_PROV_QWORDS];
long long g_aBrAC520[BR_PROV_QWORDS];
long long g_aBrAC530[BR_PROV_QWORDS];
long long g_aBrAC538[BR_PROV_QWORDS];
long long g_aBrAC540[BR_PROV_QWORDS];
long long g_aBrAC548[BR_PROV_QWORDS];
long long g_aBrB4FBE8[BR_PROV_QWORDS];
long long g_aBrBD2A8[BR_PROV_QWORDS];
long long g_aPoolNodes[BR_PROV_QWORDS];
long long g_ab0C12A0[BR_PROV_QWORDS];
long long g_abrNetPeak[BR_PROV_QWORDS];
long long g_apszCarFiles[BR_PROV_QWORDS];
long long g_apszTrackFiles[BR_PROV_QWORDS];
long long g_br0AB3F4[BR_PROV_QWORDS];
long long g_brAA26F4[BR_PROV_QWORDS];
long long g_brAA26F5[BR_PROV_QWORDS];
long long g_brCamCollided[BR_PROV_QWORDS];
long long g_brCdEnabled[BR_PROV_QWORDS];
long long g_brCdPlaying[BR_PROV_QWORDS];
long long g_brCdTrackCur[BR_PROV_QWORDS];
long long g_brCdTrackFirst[BR_PROV_QWORDS];
long long g_brCdTrackLast[BR_PROV_QWORDS];
long long g_brFlag6909E0[BR_PROV_QWORDS];
long long g_brMode0AA010[BR_PROV_QWORDS];
long long g_brMode0AA8B4[BR_PROV_QWORDS];
long long g_brNet220D68[BR_PROV_QWORDS];
long long g_brNetLastFull[BR_PROV_QWORDS];
long long g_brNetSendCount[BR_PROV_QWORDS];
long long g_brNetTickCount[BR_PROV_QWORDS];
long long g_f6C2CFC[BR_PROV_QWORDS];
long long g_i0AC300[BR_PROV_QWORDS];
long long g_i0B8C90[BR_PROV_QWORDS];
long long g_i10AA3444[BR_PROV_QWORDS];
long long g_i10AA3460[BR_PROV_QWORDS];
long long g_i4BBE08[BR_PROV_QWORDS];
long long g_i6C661C[BR_PROV_QWORDS];
long long g_i6C6624[BR_PROV_QWORDS];
long long g_p6C7C3C[BR_PROV_QWORDS];
long long g_pBrCollGrid[BR_PROV_QWORDS];
long long g_pBrCollGridCount[BR_PROV_QWORDS];
long long g_pBrCollTriFlags[BR_PROV_QWORDS];
long long g_pBrCollTriIdx[BR_PROV_QWORDS];
long long g_pBrCollVerts[BR_PROV_QWORDS];
long long g_pBrMenuACED34[BR_PROV_QWORDS];
long long g_pBrU16QueueTable[BR_PROV_QWORDS];
long long g_pfn18AA084[BR_PROV_QWORDS];
long long g_pfn18AA0C4[BR_PROV_QWORDS];
long long g_pfn18AA0C8[BR_PROV_QWORDS];
long long g_pfn18AA0CC[BR_PROV_QWORDS];
long long g_pszBr0A73C4[BR_PROV_QWORDS];
long long g_uPoolFree[BR_PROV_QWORDS];
long long g_uPoolHead[BR_PROV_QWORDS];
long g_brPAA29D0(void); long g_brPAA29D0(void) { return br_stub("g_brPAA29D0"); }

/* Count of provisional data symbols, reported at exit so the number cannot
 * quietly grow without anyone noticing. */
const int g_brProvisionalData = 63;
