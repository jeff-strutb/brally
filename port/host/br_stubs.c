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

/* Capacity, AND the "of N linked" figure the exit report prints.
 *
 * Keep it equal to the number of stub definitions below. Too small silently
 * drops hits past the limit; too large only overstates how much is still
 * missing, so a stale value is safe but misleading.
 *
 * This is hand-maintained and goes stale every time a packet lands -- it has
 * already drifted twice while packets were being merged in parallel. It should
 * be derived from the table rather than typed; until it is, re-check it with
 *     grep -cE '^long [A-Za-z0-9_]+\(void\); long' port/host/br_stubs.c */
/* Capacity of the hit table, NOT a claim about how many stubs are linked.
 *
 * This was previously sized to the exact stub count and drifted three times in
 * one session (152 -> 135 -> 131) as packets deleted stub lines. A number that
 * has to be retyped every time the file changes will be wrong, and it was also
 * being PRINTED as "of N linked", so the drift became a false statistic in the
 * report rather than a silent one.
 *
 * Now it is a generous bound with an overflow guard, and the report states only
 * what it actually counted. The true linked count is `grep -c "return br_stub("`
 * on this file -- derived, not typed. */
#define BR_STUB_MAX 512
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
    else { static int warned; if (!warned) { warned = 1;
        fprintf(stderr, "br_stubs: hit table full at %d -- report truncated\n",
                BR_STUB_MAX); } }
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
    printf("\nstubs reached: %d distinct\n", g_nHit);
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
long BrCarRecordFromState(void); long BrCarRecordFromState(void) { return br_stub("BrCarRecordFromState"); }
long BrCarRecordToState(void); long BrCarRecordToState(void) { return br_stub("BrCarRecordToState"); }
long BrCdTrackGet(void); long BrCdTrackGet(void) { return br_stub("BrCdTrackGet"); }
long BrChkFClose(void); long BrChkFClose(void) { return br_stub("BrChkFClose"); }
long BrChkFReadOpen(void); long BrChkFReadOpen(void) { return br_stub("BrChkFReadOpen"); }
long BrChkFileSize(void); long BrChkFileSize(void) { return br_stub("BrChkFileSize"); }
long BrDlIsRegistered(void); long BrDlIsRegistered(void) { return br_stub("BrDlIsRegistered"); }
long BrEnt35CE0(void); long BrEnt35CE0(void) { return br_stub("BrEnt35CE0"); }
long BrErrorf(void); long BrErrorf(void) { return br_stub("BrErrorf"); }
long BrExt_1003D0B0(void); long BrExt_1003D0B0(void) { return br_stub("BrExt_1003D0B0"); }
long BrExt_1003DFC0(void); long BrExt_1003DFC0(void) { return br_stub("BrExt_1003DFC0"); }
long BrExt_1003E0E0(void); long BrExt_1003E0E0(void) { return br_stub("BrExt_1003E0E0"); }
long BrExt_10041AC0(void); long BrExt_10041AC0(void) { return br_stub("BrExt_10041AC0"); }
long BrExt_10041BD0(void); long BrExt_10041BD0(void) { return br_stub("BrExt_10041BD0"); }
long BrExt_10043260(void); long BrExt_10043260(void) { return br_stub("BrExt_10043260"); }
long BrExt_10043330(void); long BrExt_10043330(void) { return br_stub("BrExt_10043330"); }
long BrExt_10043CD0(void); long BrExt_10043CD0(void) { return br_stub("BrExt_10043CD0"); }
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
long BrFn1003D210(void); long BrFn1003D210(void) { return br_stub("BrFn1003D210"); }
long BrGbiCall1001D420(void); long BrGbiCall1001D420(void) { return br_stub("BrGbiCall1001D420"); }
long BrGbiCall10029470(void); long BrGbiCall10029470(void) { return br_stub("BrGbiCall10029470"); }
long BrGfx2F900(void); long BrGfx2F900(void) { return br_stub("BrGfx2F900"); }
long BrGlobalFree(void); long BrGlobalFree(void) { return br_stub("BrGlobalFree"); }
long BrGlobalHandle(void); long BrGlobalHandle(void) { return br_stub("BrGlobalHandle"); }
long BrGlobalUnlock(void); long BrGlobalUnlock(void) { return br_stub("BrGlobalUnlock"); }
long BrItoa(void); long BrItoa(void) { return br_stub("BrItoa"); }
long BrModelVtxResolve(void); long BrModelVtxResolve(void) { return br_stub("BrModelVtxResolve"); }
long BrNetAnnounce(void); long BrNetAnnounce(void) { return br_stub("BrNetAnnounce"); }
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
long BrSub100027F0(void); long BrSub100027F0(void) { return br_stub("BrSub100027F0"); }
long BrSub10002870(void); long BrSub10002870(void) { return br_stub("BrSub10002870"); }
long BrSub10005FE0(void); long BrSub10005FE0(void) { return br_stub("BrSub10005FE0"); }
long BrSub1000BAF0(void); long BrSub1000BAF0(void) { return br_stub("BrSub1000BAF0"); }
long BrSub1003445A(void); long BrSub1003445A(void) { return br_stub("BrSub1003445A"); }
long BrSub10035BD1(void); long BrSub10035BD1(void) { return br_stub("BrSub10035BD1"); }
long BrSub100360F0(void); long BrSub100360F0(void) { return br_stub("BrSub100360F0"); }
long BrSub10037990(void); long BrSub10037990(void) { return br_stub("BrSub10037990"); }
long BrSub1003C520(void); long BrSub1003C520(void) { return br_stub("BrSub1003C520"); }
long BrSub1003C550(void); long BrSub1003C550(void) { return br_stub("BrSub1003C550"); }
long BrSub1003C5C0(void); long BrSub1003C5C0(void) { return br_stub("BrSub1003C5C0"); }
long BrSub1003C740(void); long BrSub1003C740(void) { return br_stub("BrSub1003C740"); }
long BrSub1003CC70(void); long BrSub1003CC70(void) { return br_stub("BrSub1003CC70"); }
long BrSub1003D210(void); long BrSub1003D210(void) { return br_stub("BrSub1003D210"); }
long BrSub1003D480(void); long BrSub1003D480(void) { return br_stub("BrSub1003D480"); }
long BrSub1003E1D0(void); long BrSub1003E1D0(void) { return br_stub("BrSub1003E1D0"); }
long BrSub100484E0(void); long BrSub100484E0(void) { return br_stub("BrSub100484E0"); }
long BrSub10058700(void); long BrSub10058700(void) { return br_stub("BrSub10058700"); }
long BrSub1005F5A0(void); long BrSub1005F5A0(void) { return br_stub("BrSub1005F5A0"); }
long BrSub100607B0(void); long BrSub100607B0(void) { return br_stub("BrSub100607B0"); }
long BrSub10061010(void); long BrSub10061010(void) { return br_stub("BrSub10061010"); }
long BrSub10062C50(void); long BrSub10062C50(void) { return br_stub("BrSub10062C50"); }
long BrSub1006F4A0(void); long BrSub1006F4A0(void) { return br_stub("BrSub1006F4A0"); }
long BrSub10070610(void); long BrSub10070610(void) { return br_stub("BrSub10070610"); }
long BrSub10070E60(void); long BrSub10070E60(void) { return br_stub("BrSub10070E60"); }
long BrSub10071480(void); long BrSub10071480(void) { return br_stub("BrSub10071480"); }
long BrSub10072270(void); long BrSub10072270(void) { return br_stub("BrSub10072270"); }
long BrSub100773F0(void); long BrSub100773F0(void) { return br_stub("BrSub100773F0"); }
long BrSub1007A840(void); long BrSub1007A840(void) { return br_stub("BrSub1007A840"); }
long BrSub1007A940(void); long BrSub1007A940(void) { return br_stub("BrSub1007A940"); }
long BrSub_10019240(void); long BrSub_10019240(void) { return br_stub("BrSub_10019240"); }
long BrSub_10019250(void); long BrSub_10019250(void) { return br_stub("BrSub_10019250"); }
long BrSub_100290A0(void); long BrSub_100290A0(void) { return br_stub("BrSub_100290A0"); }
long BrSwapRec24Array(void); long BrSwapRec24Array(void) { return br_stub("BrSwapRec24Array"); }
long BrSwapRec8Array(void); long BrSwapRec8Array(void) { return br_stub("BrSwapRec8Array"); }
long BrTextAlignCentre(void); long BrTextAlignCentre(void) { return br_stub("BrTextAlignCentre"); }
long BrTextFlag358Clear(void); long BrTextFlag358Clear(void) { return br_stub("BrTextFlag358Clear"); }
long BrTextSetColor6(void); long BrTextSetColor6(void) { return br_stub("BrTextSetColor6"); }
long BrTextSetSize(void); long BrTextSetSize(void) { return br_stub("BrTextSetSize"); }
long BrUiPageVtbl_1008F6F8(void); long BrUiPageVtbl_1008F6F8(void) { return br_stub("BrUiPageVtbl_1008F6F8"); }
long BrX10005DE0(void); long BrX10005DE0(void) { return br_stub("BrX10005DE0"); }
long BrX10005E70(void); long BrX10005E70(void) { return br_stub("BrX10005E70"); }
long BrX1002C2C0(void); long BrX1002C2C0(void) { return br_stub("BrX1002C2C0"); }
long BrX1002C500(void); long BrX1002C500(void) { return br_stub("BrX1002C500"); }
long BrX10034C66(void); long BrX10034C66(void) { return br_stub("BrX10034C66"); }
long BrX1003563A(void); long BrX1003563A(void) { return br_stub("BrX1003563A"); }
long BrX100397C0(void); long BrX100397C0(void) { return br_stub("BrX100397C0"); }
long BrX100664C0(void); long BrX100664C0(void) { return br_stub("BrX100664C0"); }
long BrX10068260(void); long BrX10068260(void) { return br_stub("BrX10068260"); }
long BrX100751D0(void); long BrX100751D0(void) { return br_stub("BrX100751D0"); }
long BrX10075F10(void); long BrX10075F10(void) { return br_stub("BrX10075F10"); }
long BrXAtExit(void); long BrXAtExit(void) { return br_stub("BrXAtExit"); }


/* ==========================================================================
 * PROVISIONAL STORAGE -- RETIRED.
 *
 * This block used to hold 64 zeroed 1 MiB blocks standing in for data symbols
 * whose owning module is not ported. They are now real definitions in
 * port/src/br_data.c, read back out of orig/BRD3D.dll: 28 of them live in
 * .data and carry initialisers this file could not have guessed, and the rest
 * were confirmed to be genuine .bss -- zero in the original too, so zero here
 * is the right answer rather than an unexamined default.
 *
 * Three of the 64 were not separate objects at all and are now aliases of
 * storage another module already owned (0x100C12A0, 0x10AA26F4, 0x10220D68);
 * see the ALIAS RESOLVED notes in slice3_45.h, slice5_61.h and slice2_11.h.
 *
 * One more was miscategorised by the generator: g_brPAA29D0 (0x10AA29D0) was
 * emitted as a FUNCTION, so it could never compare equal to NULL and the
 * consumer's null guard was dead. Do not let it come back as a function.
 *
 * The original header is kept below because its reasoning is still the right
 * reasoning for any NEW provisional symbol.
 * --------------------------------------------------------------------------
 * ORIGINAL NOTE:
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
/* (the 1 MiB blocks that used to sit here now live, correctly sized and
 * correctly initialised, in port/src/br_data.c) */

/* Count of provisional data symbols, reported at exit so the number cannot
 * quietly grow without anyone noticing. */
const int g_brProvisionalData = 0;
