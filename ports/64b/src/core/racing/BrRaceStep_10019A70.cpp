/* WHAT IT DOES: the per-frame race step (0x10019A70 BrRaceStep). Advances the
 * frame-delta ring buffer and the race clock; on the first frame runs the
 * scene/audio/limiter init; then dispatches on the race state (g_0A9360) through
 * intro/credits/outro, per-driver display-list build, HUD/pause/camera update,
 * and the replay-advance timer. One C++ TU (member-call heavy, no EH frame).
 *
 * The largest single function in BRGlide (11,223 B, 131 calls). */
/* @t3 0x10019A70 2026-09-23 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 10952/11223 insns 2920/2939 rows 341+322 regions 69 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is register colouring/scheduling, behaviour-neutral: A5 proves
 * same-in/same-out (EQUIV-MODULO-FP over valid-state seeding -- every race
 * state, gating-flag combos, live-driver pointer graph). The byte gap is one
 * prologue scheduler tie-break (idx reuses eax vs a fresh reg) colouring the
 * whole function, FIRSTDIV +0xD; the O2y measure variant inflates the rows
 * (frameless fn, sweep-variant artifact). Dossier: memory bracestep-wall.
 * Do not reopen before the end-grind. */
/* @implements 0x10019A70 glide BrRaceStep
 * @cpp_symbol _BrRaceStep
 *
 * T3 candidate (behavioural). Complete block-by-block transcription of all
 * 2,939 instructions. Behaviourally verified EQUIV-MODULO-FP by the A5 image
 * oracle on valid-state seeding (tools/oracle_profiles.py) across every race
 * state, the gating-flag combinations and a live-driver pointer graph -- same
 * inputs, same outputs. One real bug the byte view hid was found and fixed by
 * the oracle: g_226A44 (the je-skips-when-zero driver loop) was transcribed
 * `==0`; it is `!=0`.
 *
 * NOT byte-exact (T4): residue is register colouring, cpp_score FIRSTDIV +0xD,
 * driven by one prologue scheduler tie-break (idx reuses eax vs a fresh reg)
 * that colours the whole function. Behaviour-neutral. See docs and the
 * bracestep dossier. Do not reopen the colouring before the end-grind.
 *
 *
 * @t4-pass 0x10019A70 1 2026-09-15 probes 14 bytes 10944 insns 2913 regions 65 rows 682 census no  (region-1 register grind: the idx-reuses-eax prologue scheduler tie-break; cache-removal lever -- re-reading g_0B3858 snapped zero->ebp; delta/count reorder; inf[1] single-read; #pragma intrinsic(memcpy) so copies inline as rep movsd; delta/count/idx declaration permutations. FIRSTDIV held +0xD -- colouring wall.)
 * @t4-pass 0x10019A70 2 2026-09-15 probes 11 bytes 10944 insns 2913 regions 65 rows 682 census yes  (write-slot census + 4-variant sweep confirm the residue is register allocation/scheduling, not missing/wrong code -- the A5 oracle proves same-in/same-out incl. the g_226A44 branch fix; numbers unmoved.) */
#include "br_mat.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "br_racebegin.h"   /* br_globals: its objects */
#include "br_sfxsrc.h"   /* br_globals: its objects */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include "slice2_16.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
#include <stdint.h>
#include <string.h>
#pragma intrinsic(memcpy)   /* original inlines the copies as rep movsd, not a call */

/* ---- opaque per-driver arrays (0x2b68 = 11112-byte stride) ---- */
typedef struct { char _[0x2b68]; } Driver;
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AF3BC8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AF1208 (parallel this-ptr array) */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AF3D70 */

/* catch-all class for thiscall member callees (this in ecx, no vtable) */
struct Obj {
    int  m_1006FD50(int);   /* 0x1006FD50(this, int)  callee-clean */
    int  m_1005C490();
    int  m_1006FCE0(int, int);
    int  m_1005E7B0();
    int  m_1006FCB0(int);
    int  m_10001CF0();
    int  m_1005F310();
    int  m_10060A30();
    int  m_10061430();
    int  m_10061F60();
    int  m_100623A0();
    int  m_100623E0();
    int  m_1005F6C0();
    int  m_10061470();
    int  m_100634B0(int);
    int  m_1002F640(int);
};


/* ---- cdecl callees (extern "C" = unmangled; (...) = polymorphic arity) ---- */
extern "C" {
/* sub_1006E280: prototype in br_funcs.h */
/* sub_10008D60: prototype in br_funcs.h */
/* sub_1000CB80: prototype in br_funcs.h */
/* sub_10031140: prototype in br_funcs.h */
/* sub_1006E030: prototype in br_funcs.h */
/* sub_100189C0: prototype in br_funcs.h */
/* sub_1002BF24: prototype in br_funcs.h */
/* sub_100353C0: prototype in br_funcs.h */
    int sub_1006FD50(...);          /* also seen as thiscall; see Obj */
/* sub_100609D0: prototype in br_funcs.h */
/* sub_10005CD0: prototype in br_funcs.h */
/* sub_10006400: prototype in br_funcs.h */
/* sub_10004E00: prototype in br_funcs.h */
/* sub_100060A0: prototype in br_funcs.h */
/* sub_10006250: prototype in br_funcs.h */
/* sub_10009BA0: prototype in br_funcs.h */
/* sub_100060B0: prototype in br_funcs.h */
/* sub_1001C6A0: prototype in br_funcs.h */
/* sub_10019930: prototype in br_funcs.h */
/* sub_10069A80: prototype in br_funcs.h */
/* sub_10063B60: prototype in br_funcs.h */
/* sub_100627B0: prototype in br_funcs.h */
/* sub_10061310: prototype in br_funcs.h */
/* sub_100311C0: prototype in br_funcs.h */
/* sub_10034870: prototype in br_funcs.h */
/* sub_100347F0: prototype in br_funcs.h */
/* sub_1005D050: prototype in br_funcs.h */
/* sub_100199A0: prototype in br_funcs.h */
/* sub_1005E690: prototype in br_funcs.h */
/* sub_10062830: prototype in br_funcs.h */
/* sub_100181A0: prototype in br_funcs.h */
/* sub_10018230: prototype in br_funcs.h */
/* sub_10018290: prototype in br_funcs.h */
/* sub_10030270: prototype in br_funcs.h */
/* sub_1002ECAC: prototype in br_funcs.h */
/* sub_10033B50: prototype in br_funcs.h */
/* sub_10063DD0: prototype in br_funcs.h */
/* sub_1002E13B: prototype in br_funcs.h */
/* sub_10060E30: prototype in br_funcs.h */
/* sub_10063A00: prototype in br_funcs.h */
/* sub_10063A40: prototype in br_funcs.h */
/* sub_10002C00: prototype in br_funcs.h */
/* sub_10002AF0: prototype in br_funcs.h */
/* sub_10002D30: prototype in br_funcs.h */
/* sub_10013E80: prototype in br_funcs.h */
/* sub_10019A40: prototype in br_funcs.h */
/* sub_10006100: prototype in br_funcs.h */
/* sub_1001C7A0: prototype in br_funcs.h */
/* sub_1002E186: prototype in br_funcs.h */
/* sub_10060E00: prototype in br_funcs.h */
/* sub_10060DF0: prototype in br_funcs.h */
/* sub_1001C810: prototype in br_funcs.h */
/* sub_1005C450: prototype in br_funcs.h */
/* sub_10018310: prototype in br_funcs.h */
/* sub_100023F0: prototype in br_funcs.h */
/* sub_1005F580: prototype in br_funcs.h */
/* sub_10033BB0: prototype in br_funcs.h */
/* sub_10016C90: prototype in br_funcs.h */
/* sub_1002A590: prototype in br_funcs.h */
/* sub_10029D70: prototype in br_funcs.h */
/* sub_10060E10: prototype in br_funcs.h */
/* sub_100611F0: prototype in br_funcs.h */
/* sub_10061280: prototype in br_funcs.h */
/* sub_10060F40: prototype in br_funcs.h */
/* sub_10019890: prototype in br_funcs.h */
/* sub_10013F20: prototype in br_funcs.h */
/* sub_10033C90: prototype in br_funcs.h */
/* sub_10004F90: prototype in br_funcs.h */
/* sub_10005400: prototype in br_funcs.h */
/* sub_10019980: prototype in br_funcs.h */
/* sub_10018340: prototype in br_funcs.h */
/* sub_1006B4F0: prototype in br_funcs.h */
/* sub_10060A10: prototype in br_funcs.h */
/* sub_10002F10: prototype in br_funcs.h */
/* sub_100609F0: prototype in br_funcs.h */
/* sub_10002EB0: prototype in br_funcs.h */
/* sub_1006BD70: prototype in br_funcs.h */
/* sub_10061440: prototype in br_funcs.h */
/* sub_10006460: prototype in br_funcs.h */
/* sub_10072210: prototype in br_funcs.h */
/* sub_1006BDD0: prototype in br_funcs.h */
/* sub_10063CC0: prototype in br_funcs.h */
/* sub_10063AD0: prototype in br_funcs.h */
/* sub_10014CB0: prototype in br_funcs.h */
/* sub_1006E3B0: prototype in br_funcs.h */
/* sub_100064D0: prototype in br_funcs.h */
/* sub_1006E360: prototype in br_funcs.h */
/* sub_1002CB3F: prototype in br_funcs.h */
/* sub_1001C9D0: prototype in br_funcs.h */
/* sub_10019900: prototype in br_funcs.h */
/* sub_1001C890: prototype in br_funcs.h */
/* sub_100325B0: prototype in br_funcs.h */
/* sub_1002E317: prototype in br_funcs.h */
/* sub_10019A10: prototype in br_funcs.h */
/* sub_10013F00: prototype in br_funcs.h */
/* sub_1006A070: prototype in br_funcs.h */
/* sub_10002460: prototype in br_funcs.h */
/* sub_10032680: prototype in br_funcs.h */
/* sub_1006C460: prototype in br_funcs.h */
/* sub_10037180: prototype in br_funcs.h */
/* sub_10006430: prototype in br_funcs.h */
/* sub_10059E50: prototype in br_funcs.h */
/* sub_10059DE0: prototype in br_funcs.h */
/* sub_10059E30: prototype in br_funcs.h */
/* sub_10059DC0: prototype in br_funcs.h */
/* sub_100131E0: prototype in br_funcs.h */
/* sub_100346D0: prototype in br_funcs.h */
/* sub_10034760: prototype in br_funcs.h */
/* sub_10034560: prototype in br_funcs.h */
/* sub_100344D0: prototype in br_funcs.h */
/* sub_10034620: prototype in br_funcs.h */
/* sub_100342B0: prototype in br_funcs.h */
/* sub_10034390: prototype in br_funcs.h */
}

/* ---- indirect-call function pointers ---- */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
extern int (*g_18ED1E8)();
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* ---- data globals used so far ---- */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* address-taken (push &g_6E86B8) */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* string source (inlined strcpy) */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* string dest */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0x102066C8 base */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* case 5 / case 4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* La213 tail */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 0x1001a462 wheel/tyre/camera + callback loop */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* init-tail 0x1001a97c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* per-frame tail 0x1001ab93 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* state-4 limiter */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* merge tail 0x1001b1c9 */
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

extern int (*(*(int (**)(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int))&g_pfn18ED1C4))(int,int,int,int,int,int,int,int,int,int,int,int,int,int,int);
extern int (*g_18ED1C0)(int,int,int,int,int,int,int,int);
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* sub_1006D280: prototype in br_funcs.h */
/* sub_10008EF0: prototype in br_funcs.h */

extern "C" void BrRaceStep(void)
{
    int loc10, loc14, loc18, loc1c;     /* esp+0x10/14/18/1c render-loop state */
    int now   = BrSub10075020();
    int delta = now - (*(int *)&DAT_105ccb68[10]);
    (*(int *)&DAT_105ccb68[10])  = now;
    int idx   = (*(int *)((char *)&g_brRaceRules + 0x8)) /* BR_LP64_BYTE_VIEW */;
    int count = (*(int *)((char *)&g_brRaceRules + 0x4)) /* BR_LP64_BYTE_VIEW */;

    /* region 1: frame-delta ring buffer (0x10019A70-0x10019AE4) */
    if (idx < 0) {
        idx = 0;
        if (count > 0) {
            int i;
            for (i = 0; i < count; i++) g_BrFpsSamplesA[i] = delta;
            idx = count;
        }
    }
    if (++(*(int *)&g_brRaceClockCount) == 0) (*(int *)&DAT_105ccb68[5]) = 0; else (*(int *)&DAT_105ccb68[5]) += delta;
    (*(int *)((char *)&g_brRaceRules + 0x8)) /* BR_LP64_BYTE_VIEW */ = ++idx;                       /* store idx+1 unconditionally (0x1001a acf) */
    if (idx >= count) { idx = 0; (*(int *)((char *)&g_brRaceRules + 0x8)) /* BR_LP64_BYTE_VIEW */ = idx; }   /* then conditionally re-store 0 */
    g_BrFpsSamplesA[idx] = delta;

    /* region 2: first-frame init gate (0x10019AE4) */
    if ((*(int *)&DAT_105ccb68[11]) == 0) {
        (*(int *)((char *)&g_aBrEntRecs + 0x54)) = 1;
        BrPodNop(1);
        BrClearTables_1000F620();
        if ((*(int *)&DAT_105ccb68[8]) == 0) {
            BrPodNop();
            BrPodNop();
            (*(int (**)())&DAT_10b7352c)();
            g_18ED1E8();
            (*(int (**)())&PTR_FUN_100b849c)();
            BrTrackLoadHandling((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 5 ? 0xc : (*(int *)&g_Br0B380C));
            BrFontTexInitAll();
            (*(int (**)())&DAT_10b73528)();
            BrFlagInit_1002B950();
        }
        BrFrameBeginRec(&(BrG_6C1628[0]));
        BrPodNop(1);
        BrFrameBeginRec(&(BrG_6C1628[0]));
        BrStore_1003BD40(0x7b);
        (*(int *)&DAT_105ccb68[9]) = 0;
        if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ != 1 && (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ != 6 && (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ != 2) g_CBE8 = 3;
        (*(int *)&g_brRaceBeginMirrorOff) = 0;
        if ((unsigned)(*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ > 6) goto Ldefault;
        switch ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */) {
        case 0:     /* 0x10019bc8 */
            (*(int *)&g_brRaceNDriver) = 0x14; (*(int *)&g_BrCarCount) = 3;
            ((Obj*)&(*(Driver (*)[])&g_aBrRaceCar))->m_1006FD50(g_226e7c);
            goto Lcf3;
        case 1:     /* 0x10019bf1 */
            (*(int *)&g_brRaceNDriver) = 2; (*(int *)&g_BrCarCount) = 2;
            ((Obj*)&(*(Driver (*)[])&g_aBrRaceCar))->m_1006FD50(g_226e7c);
            ((Obj*)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x2B68)) /* BR_LP64_BYTE_VIEW */)->m_1006FD50(g_226e7c);
            (*(int *)((char *)&g_aBrRaceCar + 0x551C)) /* BR_LP64_BYTE_VIEW */ = 0;
            goto Lcf3;
        case 6:     /* 0x10019c2a */
            if ((*(int *)&DAT_105ccb68[8]) != 0) goto Lcfb;
            (*(int *)&g_brRaceNEntrant) = 1;
            if ((*(int *)&g_brRaceBegin226A4C) == 0) { (*(int *)&g_brRaceNDriver) = 1; (*(int *)&g_BrCarCount) = 1; }
            else               { (*(int *)&g_brRaceNDriver) = 0; (*(int *)&g_BrCarCount) = 0; }
            ((Obj*)&(*(Driver (*)[])&g_aBrRaceCar))->m_1006FD50(g_226e7c);
            if ((*(int *)&g_brRaceNet) != 0) {
                BrNetReset();
                BrNetSetF10220DD0();
                BrNetOpenAnnounce();
                (*(int *)((char *)&g_aBrRaceCar + 0x144)) /* BR_LP64_BYTE_VIEW */ = BrGetGlobal_94294();
                strcpy((*(char (*)[])((char *)&g_aBrRaceCar + 0x148)) /* BR_LP64_BYTE_VIEW */, g_aBrCfgPlayerName);
                BrNetSlotSetName((*(int *)((char *)&g_aBrRaceCar + 0x144)) /* BR_LP64_BYTE_VIEW */, g_aBrCfgPlayerName);
            }
            while (BrDPlayGetCurrentPlayers() > (unsigned)(*(int *)&g_BrCarCount)) {
                int r = BrNetStackPop();
                if (r >= 0) BrCarTableAdd(r);
            }
            goto Lcf3;
        case 5:     /* 0x10019d87 */
            (*(int *)&g_brRaceNEntrant) = 1; (*(int *)&g_brRaceNDriver) = 1; (*(int *)&g_BrCarCount) = 1;
            (*(int *)&DAT_105ccb68[9]) = 0; (*(int *)&DAT_105ccb68[8]) = 0; (*(int *)((char *)&g_aBrRaceCar + 0xE88)) /* BR_LP64_BYTE_VIEW */ = 0;
            (*(int *)((char *)&g_aBrRaceCar + 0x2734)) /* BR_LP64_BYTE_VIEW */ = (int)&(*(int *)((char *)&g_aBrRaceCar + 0x2780)) /* BR_LP64_BYTE_VIEW */; (*(int *)&g_BrCamHold2) = 0;
            goto La213;
        case 4:     /* 0x10019dc0 */
        {
            int *pd0 = *(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */;
            DAT_104b15e8 = 1; (*(int *)&g_brRaceNEntrant) = 1; (*(int *)&g_brRaceNDriver) = 1; (*(int *)&g_BrCarCount) = 1;
            (*(int *)&DAT_105ccb68[9]) = 0; (*(int *)((char *)&g_aBrRaceCar + 0xE88)) /* BR_LP64_BYTE_VIEW */ = 0;
            ((*(int  *)((char*)((pd0)) + ((0x44))))) = (int)&(*(int *)&g_aBrRaceBeginRec);
            {
                int which = g_5bc760;                 /* 0x10019df1 */
                if (which == 0) goto Lintro;
                if (which == 1) goto Lcredits;
                if (which == 2) goto Loutro;
                goto Le63;
            Loutro:                                   /* 0x10019e00 */
                BrRaceCueLayout();
                BrGhostLoad("RallyOutro.dat", 0);
                goto Le58;
            Lcredits:                                 /* 0x10019e15 */
                BrGhostLoad("RallyCredits.dat", 0);
                goto Le58;
            Lintro:                                   /* 0x10019e25 */
                BrGhostLoad((*(int *)&DAT_105ccb68[13]) ? "RallyIntro2.dat" : "RallyIntro1.dat", 0);
                { int nv = (*(int *)&DAT_105ccb68[13]) + 1;
                  if (nv <= 1) (*(int *)&DAT_105ccb68[13]) = nv; else (*(int *)&DAT_105ccb68[13]) = 0; }
            Le58:                                     /* 0x10019e58 */
                BrReplayRewind();
                (*(int *)&g_brRaceBeginMovieDone) = 0;
            }
        Le63:                                         /* 0x10019e63 */
            {
                int  *pObj  = *(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */;
                char *pInfo;
                ((*(int  *)((char*)((pObj)) + ((0x2c))))) = 0;
                ((*(int  *)((char*)((pObj)) + ((0x30))))) = 0;
                (*(char *)((char *)&g_aBrRaceCar + 0x29AC)) /* BR_LP64_BYTE_VIEW */ = (char)0xff; (*(char *)((char *)&g_aBrRaceCar + 0x29AD)) /* BR_LP64_BYTE_VIEW */ = (char)0xff; (*(char *)((char *)&g_aBrRaceCar + 0x29AE)) /* BR_LP64_BYTE_VIEW */ = (char)0xff;
                pInfo = (char*)((*(int  *)((char*)((pObj)) + ((0x44)))));
                (*(int *)&g_Br0B380C) = (signed char)pInfo[0];
                (*(int *)((char *)&g_aBrRaceCar + 0x29A4)) /* BR_LP64_BYTE_VIEW */ = (signed char)pInfo[1];
                ((Obj*)&(*(Driver (*)[])&g_aBrRaceCar))->m_1006FD50((signed char)pInfo[1]);
                (*(int *)((char *)&g_aBrRaceCar + 0xE98)) /* BR_LP64_BYTE_VIEW */ = (signed char)pInfo[2];
                (*(int *)((char *)&g_aBrRaceCar + 0xE9C)) /* BR_LP64_BYTE_VIEW */ = (signed char)pInfo[3];
                (*(int *)((char *)&g_aBrRaceCar + 0xE90)) /* BR_LP64_BYTE_VIEW */ = (signed char)pInfo[4];
                (*(int *)((char *)&g_aBrRaceCar + 0xE94)) /* BR_LP64_BYTE_VIEW */ = (signed char)pInfo[5];
                ((*(char *)((char*)((pObj)) + ((0x25))))) = pInfo[6];
                g_226e80 = (signed char)pInfo[7];
                DAT_104b15e8 = (signed char)pInfo[7];
            }
            goto La213;
        }
        case 2:                 /* 0x10019f0e */
        {
            ((Obj*)&(*(Driver (*)[])&g_aBrRaceCar))->m_1006FD50(g_226e7c);
            (*(int *)&DAT_105ccb68[9]) = 0;
            (*(int *)&g_brRaceBeginRecArmed) = 1;
            (*(int *)&g_brRaceNDriver) = (*(int *)&g_brRaceNEntrant) + 1;
            (*(int *)&g_BrCarCount) = (*(int *)&g_brRaceNEntrant) + 1;
            ((*(int  *)((char*)((*(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */[(*(int *)&g_brRaceNEntrant)])) + ((0x44))))) = (int)&(*(int *)&g_aBrRaceBeginRecIn);
            BrPodNop(&g_0A9884, (*(int *)&g_brRace5BC8D8));
            memcpy((void*)((*(int  *)((char*)((*(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */[(*(int *)&g_brRaceNEntrant)])) + ((0x44))))), &(*(int *)&g_aBrRaceBeginRec), (*(int *)&g_brRace5BC8D8));
            {
                char *inf = (char*)((*(int  *)((char*)((*(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */[(*(int *)&g_brRaceNEntrant)])) + ((0x44)))));
                int v1 = (signed char)inf[1];
                g_aBrRaceCar[(*(int *)&g_brRaceNEntrant)].f29A4 = v1;
                ((Obj *)&g_aBrRaceCar[(*(int *)&g_brRaceNEntrant)].fwd.x)->m_1006FD50(v1);
            }
            BrReplayRewind();
            {
                char *inf = (char*)((*(int  *)((char*)((*(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */[(*(int *)&g_brRaceNEntrant)])) + ((0x44)))));
                if ((signed char)inf[0] == (*(int *)&g_Br0B380C) &&
                    (signed char)inf[7] == g_226e80) {          /* 0x1001a02b */
                    BrPodNop(&g_0A9878, (signed char)inf[0]);
                    ((*(int  *)((char*)((*(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */[(*(int *)&g_brRaceNEntrant)])) + ((0x48))))) = 8;
                    BrPodNop(&g_0A9868, (*(int *)&g_brRace5BC8D8));
                    ((*(int  *)((char*)((*(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */[(*(int *)&g_brRaceNEntrant)])) + ((0x4c))))) = (*(int *)&g_brRace5BC8D8);
                } else {                                        /* 0x1001a09c */
                    BrPodNop(&g_0A9850, (signed char)inf[0], (*(int *)&g_Br0B380C));
                    ((*(int  *)((char*)((*(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */[(*(int *)&g_brRaceNEntrant)])) + ((0x44))))) = 0;
                    (*(int *)&g_BrCarCount) = 1; (*(int *)&g_brRaceNDriver) = 1;
                }
            }
            /* 0x1001a0d9 .. 0x1001a1ec: per-driver display-list build loop */
            if ((*(int *)&g_brRaceNEntrant) > 0) {
                char *base = (char*)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */;     /* eax -> driver[i] */
                int   off  = 0x2c;                 /* ecx */
                int   val  = (int)&g_1787850;      /* esi = 0x11787850 */
                int   i    = 0;                    /* edi */
                do {
                    int *pd; char *q, *f;
                    pd = *(int**)base;
                    i++;
                    base += 0x2b68;
                    *(int*)((char*)pd + off) = val;
                    pd = *(int**)(base - 0x2b68);
                    val += 0xf000;
                    q = (char*)*(int*)((char*)pd + off);
                    off += 4;
                    q[0] = (char)(*(int *)&g_Br0B380C);
                    pd = *(int**)(base - 0x2b68);
                    q  = (char*)*(int*)((char*)pd + off - 4);
                    q[1] = *(char*)(base - 0x2b80);
                    pd = *(int**)(base - 0x2b68);
                    f  = (char*)*(int*)(base - 0x469c);
                    q  = (char*)*(int*)((char*)pd + off - 4);
                    q[2] = f[0xf8];
                    pd = *(int**)(base - 0x2b68);
                    f  = (char*)*(int*)(base - 0x469c);
                    q  = (char*)*(int*)((char*)pd + off - 4);
                    q[3] = f[0xfc];
                    pd = *(int**)(base - 0x2b68);
                    f  = (char*)*(int*)(base - 0x469c);
                    q  = (char*)*(int*)((char*)pd + off - 4);
                    q[4] = f[0x100];
                    pd = *(int**)(base - 0x2b68);
                    f  = (char*)*(int*)(base - 0x469c);
                    q  = (char*)*(int*)((char*)pd + off - 4);
                    q[5] = f[0x104];
                    pd = *(int**)(base - 0x2b68);
                    f  = (char*)*(int*)(base - 0x469c);
                    q  = (char*)*(int*)((char*)pd + off - 4);
                    q[6] = f[0x108];
                    pd = *(int**)(base - 0x2b68);
                    q  = (char*)*(int*)((char*)pd + off - 4);
                    q[7] = (char)g_226e80;
                    pd = *(int**)(base - 0x2b68);
                    *(int*)((char*)pd + off + 4) = 8;
                    pd = *(int**)(base - 0x2b68);
                    *(int*)((char*)pd + off + 0xc) = 0xde5c;
                } while (i < (*(int *)&g_brRaceNEntrant));
            }
            goto La213;
        }
        default: Ldefault:      /* 0x1001a1ee (== case 3) */
        case 3:
            ((Obj*)&(*(Driver (*)[])&g_aBrRaceCar))->m_1006FD50(g_226e7c);
            (*(int *)&DAT_105ccb68[9]) = 0;
            (*(int *)&g_brRaceNDriver) = (*(int *)&g_brRaceNEntrant);
            (*(int *)&g_BrCarCount) = (*(int *)&g_brRaceNEntrant);
            goto La213;
        }

    Lcf3:   /* 0x10019cf3 */
        if ((*(int *)&DAT_105ccb68[8]) == 0) goto Ld39;
    Lcfb:   /* 0x10019cfb */
        {
            int a = (*(int *)&g_brRaceBeginBestCar);
            BrWrap_10067960((char*)&g_2066C8 - a * 89992);
            {
                int p = (int)&(*(int *)((char *)&g_aBrSfxChan + 0x10));        /* 0x10019d23 zero loop (signed cmp) */
                do { *(int*)(p + 4) = 0; *(int*)p = 0; p += 0x18; }
                while (p < (int)&g_18EF0B8);
            }
        }
        goto Ld70;
    Ld39:   /* 0x10019d39 */
        {
            int i;
            for (i = 0; i < (*(int *)&g_brRaceNEntrant); i++) {
                int *p = *(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */[i];
                ((*(int  *)((char*)((p)) + ((0x44))))) = 0; ((*(int  *)((char*)((p)) + ((0x2c))))) = 0; ((*(int  *)((char*)((p)) + ((0x30))))) = 0;
            }
        }
    Ld70:   /* 0x10019d70 */
        (*(int *)&DAT_105ccb68[9]) = ((*(int *)&DAT_105ccb68[8]) == 0);
        goto La213;

    La213:  /* 0x1001a213 -- common tail after the state switch */
        if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 5) {
            (*(int *)&g_brRaceBeginDifficulty) = 0;
        } else {
            (*(int *)&g_brRaceBeginDifficulty) = (((*(int  *)((char*)((*(int**)((char*)&(g_apBrRaceDiff[0]) + (*(int *)&g_Br0B380C) * 4))) + ((4))))) >> 4) & 1;
        }
        BrRaceDifficultySet(DAT_104b15e8);
        if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ != 4 && (*(int *)&DAT_105ccb68[8]) == 0) {
            BrPodNop();
            BrPodNop();
        }
        FUN_10061310();
        if ((*(int *)&DAT_105ccb68[8]) != 0) goto La430;
        BrTrackLoad((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 5 ? 0xc : (*(int *)&g_Br0B380C));
        if ((*(int *)&DAT_105ccb68[8]) != 0) goto La430;
        {
            int  cnt;
            (*(int *)&g_brRaceBeginAirArmed) = 0; (*(int *)&g_brRaceBeginAirTrigger) = 0; (*(int *)&g_brRaceBeginAirplane) = 0; g_pBrRaceFlyAim = 0;
            g_pBrRaceFlyPos = 0; g_brRaceBeginPathT = 0; g_brRaceBeginPathSeg = 0; (*(int *)&g_brRaceBeginPathIdx) = 0; (*(int *)&g_brRaceBeginPathLen) = 0;
            BrPodNop(&g_0A9840, (*(int *)&g_brRaceBeginSpecialsN));
            (*(int *)&g_brRaceBeginFxCount) = 0;
            cnt = (*(int *)&g_brRaceBeginSpecialsN);
            if (cnt > 0) {
                char *e = (char*)&(*(int *)&g_aBrRaceSpecial);     /* 0x106eee3c */
                int   i = 0;
                do {
                    int          arg = 0;
                    const char  *str = 0;
                    switch ((signed char)((*(char *)((char*)((e)) + ((8))))) - 3) {   /* jmp [.. 0x1001c664] */
                    case 0:  arg = ((*(int  *)((char*)((e)) + ((0))))); (*(int *)&g_brRaceBeginAirplane) = arg; str = (char*)&g_0A97F8; break;
                    case 1:  (*(int *)&g_brRaceBeginPathLen) = ((*(int  *)((char*)((e)) + ((4))))); arg = ((*(int  *)((char*)((e)) + ((0))))); g_pBrRaceFlyPos = arg;
                             str = (char*)&g_0A9824; break;
                    case 2:  arg = ((*(int  *)((char*)((e)) + ((0))))); g_pBrRaceFlyAim = arg; str = (char*)&g_0A9808; break;
                    case 3:  arg = ((*(int  *)((char*)((e)) + ((0))))); (*(int *)&g_brRaceBeginAirTrigger) = arg; str = (char*)&g_0A97E0; break;
                    case 4:  { int c = (*(int *)&g_brRaceBeginFxCount); arg = ((*(int  *)((char*)((e)) + ((0)))));
                               g_aBrRaceBeginFx[c] = arg;
                               (*(int *)&g_brRaceBeginFxCount) = c + 1; str = (char*)&g_0A97D0; } break;
                    default: goto Lskip;
                    }
                    BrPodNop(str, arg);
                Lskip:
                    i++;
                    e += 0xc;
                } while (i < (*(int *)&g_brRaceBeginSpecialsN));
            }
            {
                int sel;                        /* 0x1001a3a6 */
                if ((*(int *)&g_brRaceBeginAirTrigger) == 0 || g_pBrRaceFlyPos == 0 || g_pBrRaceFlyAim == 0) {
                    sel = 0; (*(int *)&g_brRaceBeginAirplane) = 0;
                } else {
                    sel = (*(int *)&g_brRaceBeginAirplane);
                }
                if (sel != 0) {
                    float m[3];
                    int  *row = (int*)((char*)g_BrDrawTrackFlags + (sel + sel * 20) * 4);
                    m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f;
                    BrVec3TransformDivW(&m[0], &m[0], row);   /* in place: both leas are [esp+0x2c] */
                    g_brRaceBeginPathScale = BrVec3Length((const struct BrVec3 *)(&m[0]));
                }
            }
            (*(int *)&g_brRaceBeginCamMinus1) = g_CBE8 - 1;
        }
    La430:  /* 0x1001a430 */
        g_brMode0AA8B4 = 1;
        if ((*(int *)&DAT_105ccb68[8]) == 0) g_brMode0AA8B4 = (*(int *)&g_brRaceNEntrant);
        (*(int *)&BrG_6C1628[4]) = (*(int *)&DAT_105ccb68[8]) ? (*(int *)&g_brRaceBeginBestCar) : 0;
        (*(int *)&g_brRaceBegin6E8720) = ((*(int *)&BrG_6C1628[4]) == 0);

        /* 0x1001a462: wheel/tyre + camera-mode setup */
        if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 4) {
            (*(int *)((char *)&g_aBrRaceCar + 0x2734)) /* BR_LP64_BYTE_VIEW */ = (int)&(*(int *)((char *)&g_aBrRaceCar + 0x2780)) /* BR_LP64_BYTE_VIEW */;
            (*(int *)&g_BrCamHold2) = 0xb4;
            goto La58d;
        }
        if ((*(int *)&g_brRaceNEntrant) > 0) {                     /* 0x1001a485 tyre loop */
            char *b = (char*)&(*(int *)((char *)&g_aBrRaceCar + 0xE8C)) /* BR_LP64_BYTE_VIEW */;
            int   i = 0;
            do {
                int *p, *q;
                p = *(int**)b; *(int*)(b + 0x10) = ((*(int  *)((char*)((p)) + ((0xfc)))));
                p = *(int**)b; *(int*)(b + 0x08) = ((*(int  *)((char*)((p)) + ((0x104)))));
                p = *(int**)b; *(int*)(b + 0x04) = ((*(int  *)((char*)((p)) + ((0x100)))));
                p = *(int**)b; *(int*)(b + 0x0c) = ((*(int  *)((char*)((p)) + ((0xf8)))));
                q = *(int**)(b + 0x1b34);
                if ((*(int *)((char *)&g_BrCtrlCfg + 0x2A0)) /* BR_LP64_BYTE_VIEW */ == 1 || (*(int *)((char *)&g_BrCtrlCfg + 0x2A0)) /* BR_LP64_BYTE_VIEW */ == 2 || (*(int *)((char *)&g_BrCtrlCfg + 0x2A0)) /* BR_LP64_BYTE_VIEW */ == 3)
                    ((*(char *)((char*)((q)) + ((0x25))))) = 5;
                else
                    ((*(char *)((char*)((q)) + ((0x25))))) = 2;
                i++;
                b += 0x2b68;
            } while (i < (*(int *)&g_brRaceNEntrant));
        }
        {
            int ecx = (*(int *)&g_brRaceNEntrant);                 /* 0x1001a4f1 */
            if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 2) {
                int *p = *(int**)&(*(int *)((char *)&g_aBrRaceCar + 0x5528)) /* BR_LP64_BYTE_VIEW */;
                if (((*(int  *)((char*)((p)) + ((0x44))))) != 0) {
                    char *inf = (char*)((*(int  *)((char*)((p)) + ((0x44)))));
                    (*(int *)((char *)&g_aBrRaceCar + 0x3A00)) /* BR_LP64_BYTE_VIEW */ = (signed char)inf[2];
                    (*(int *)((char *)&g_aBrRaceCar + 0x3A04)) /* BR_LP64_BYTE_VIEW */ = (signed char)inf[3];
                    (*(int *)((char *)&g_aBrRaceCar + 0x39F8)) /* BR_LP64_BYTE_VIEW */ = (signed char)inf[4];
                    (*(int *)((char *)&g_aBrRaceCar + 0x39FC)) /* BR_LP64_BYTE_VIEW */ = (signed char)inf[5];
                    ((*(char *)((char*)((p)) + ((0x25))))) = inf[6];
                    goto La58d;
                }
            }
            /* 0x1001a547 */
            if (ecx < (*(int *)&g_BrCarCount)) {
                do {
                    char *e = ((char *)&g_aBrRaceCar[ecx].fE94);
                    int  *q;
                    *(int*)(e + 8) = 1;
                    *(int*)(e) = 1;
                    *(int*)(e - 4) = 2;
                    *(int*)(e + 4) = 0;
                    q = *(int**)(e + 0x1b2c);
                    ecx++;
                    ((*(char *)((char*)((q)) + ((0x25))))) = 0;
                } while (ecx < (*(int *)&g_BrCarCount));
            }
        }
    La58d:  /* 0x1001a58d */
        if ((*(int *)&DAT_105ccb68[8]) != 0) goto La973;
        DAT_105ccb60 = 0;
        if ((*(int *)&g_BrCarCount) > 0) {                     /* 0x1001a5ba callback-wiring loop */
            char *s = (char*)&(*(int *)((char *)&g_aBrRaceCar + 0xF08)) /* BR_LP64_BYTE_VIEW */;
            int   i = 0;
            do {
                int n  = (*(int *)&g_brRaceNEntrant);
                int st = (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */;
                *(int*)(s + 0x1aa8) = 0x3f800000;
                if (i < n) {
                    *(int*)s = (int)&BrCtlHuman;
                } else if (st == 2) {
                    *(int*)s = (int)&BrCtlHuman;
                    ((*(char *)((char*)((s)) + ((0x1aa7))))) = (char)st;
                    *(int*)(s + 0x1aa8) = 0x3ec00000;
                    goto Lwire_after;
                } else if (st == 6) {
                    *(int*)s = (int)&BrRaceCarCtlOutro;
                } else {
                    ((Obj*)(s - 0xf08))->m_1005C490();
                    *(int*)s = (int)&BrCtlAi;
                }
                ((*(char *)((char*)((s)) + ((0x1aa7))))) = 0;              /* 0x1001a612 */
            Lwire_after:                        /* 0x1001a619 */
                if (*(int*)(s - 0x80) == 0)
                    ((Obj*)(s - 0xf08))->m_1006FCE0(i, *(int*)(s + 0x1aa0));
                ((Obj*)(s - 0xf08))->m_1005E7B0();
                *(int*)(s + 0x74)  = 0;
                *(int*)(s + 0xf4)  = 0;
                *(int*)(s + 0xfc)  = 0;
                i++;
                s += 0x2b68;
            } while (i < (*(int *)&g_BrCarCount));
        }
        if ((*(int *)&g_BrCarCount) == 0) {                    /* 0x1001a65f */
            int j;
            for (j = 0; j < 0x57e2; j++) ((int*)&(*(int *)&g_ab0C12A0))[j] = 0;
            ((Obj*)&(*(Driver (*)[])&g_aBrRaceCar))->m_1006FCB0(0);
            ((Obj*)&(*(Driver (*)[])&g_aBrRaceCar))->m_1005E7B0();
            ((Obj*)&(*(Driver (*)[])&g_aBrRaceCar))->m_10001CF0();
            (*(float *)((char *)&g_aBrRaceCar + 0x2774)) /* BR_LP64_BYTE_VIEW */ = (*(float *)((char *)&g_aBrRaceCar + 0x2774)) /* BR_LP64_BYTE_VIEW */ - g_0773B0;
        }
        loc14 = 0;                              /* 0x1001a6a0 */
        if (g_brMode0AA8B4 > 0) {
            loc1c = (int)&(DAT_100a6b68[0]);
            loc10 = (int)&(*(int *)&BrG_6C1628[5]);
            do {                                /* 0x1001a6c5 outer render loop */
                int   ix  = *(int*)(loc10 - 4);
                char *s   = (char*)&(*(int *)&g_ab0C12A0) + ix * 89992;   /* [edx*8+g_0BCDD0] */
                char *b   = (DAT_104b15e8 == 4) ? s + 0x300 : s + 0x100;
                int   a   = (signed char)((*(char *)((char*)((s)) + ((0xe4)))));
                int   d   = (signed char)((*(char *)((char*)((s)) + ((0xe5)))));
                char  kind;
                *(int*)loc10 = (*(int (**)(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int))&g_pfn18ED1C4)((int)(s + 0x500), (int)b, a, d, a,
                                       1, 2, 0, 0, 1, 1, 0, 0, 0, 0);
                kind = ((*(char *)((char*)((s)) + ((0xdb)))));
                if (kind == 1 || kind == 2) {          /* 0x1001a743 */
                    char *ep = (char*)loc10 + 4;
                    char *w  = b + 0x1fe;
                    g_18ED1B4 = 1;
                    loc18 = 0xf;
                    do {                               /* 0x1001a75e */
                        int a2, d2;
                        char k2 = ((*(char *)((char*)((s)) + ((0xdb)))));
                        if (k2 == 1) {                 /* 0x1001a768 attr repack */
                            unsigned int av = *(unsigned short*)w;
                            unsigned int cv = av;
                            cv &= 0xc6;
                            av &= 0x3000;
                            av |= 0x0800;                          /* or ah,8 */
                            av = (av & ~0xffffu) | ((unsigned short)av >> 11);
                            cv <<= 5;
                            av |= cv;
                            *(unsigned short*)w =
                                (unsigned short)(((av & 0xff) << 8) | ((av >> 8) & 0xff));
                        } else if (k2 == 2) {          /* 0x1001a793 */
                            *(unsigned short*)w &= 0xfeff;
                        }
                        a2 = (signed char)((*(char *)((char*)((s)) + ((0xe4)))));
                        d2 = (signed char)((*(char *)((char*)((s)) + ((0xe5)))));
                        *(int*)ep = (*(int (**)(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int))&g_pfn18ED1C4)((int)(s + 0x500), (int)b, a2, d2, a2,
                                              1, 2, 0, 0, 1, 1, 0, 0, 0, 0);
                        ep += 4;
                        w  -= 2;
                    } while (--loc18 != 0);
                    g_18ED1B4 = 0;
                }
                {                                      /* 0x1001a7f0 */
                    int a3 = (signed char)((*(char *)((char*)((s)) + ((0xe8)))));
                    int c3 = (signed char)((*(char *)((char*)((s)) + ((0xe9)))));
                    int d3 = (signed char)((*(char *)((char*)((s)) + ((0xe4)))));
                    int e3 = (signed char)((*(char *)((char*)((s)) + ((0xe5)))));
                    char *dst;
                    int   sz;
                    ((*(int  *)((char*)((loc10)) + ((0x40))))) = (*(int (**)(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int))&g_pfn18ED1C4)((int)(d3 * e3 + s + 0x500), (int)b,
                                                a3, c3, a3, 1, 2, 0, 0, 1, 1,
                                                0, 0, 1, 0);
                    /* 0x1001a847: memmove of the emitted span */
                    sz  = ((signed char)((*(char *)((char*)((s)) + ((0xd8))))) + 2)
                          * (signed char)((*(char *)((char*)((s)) + ((0xe8))))) * (signed char)((*(char *)((char*)((s)) + ((0xe9)))));
                    dst = s - sz + 0x8000;
                    memmove(dst,
                            (signed char)((*(char *)((char*)((s)) + ((0xe4))))) * (signed char)((*(char *)((char*)((s)) + ((0xe5)))))
                                + s + 0x500,
                            sz);
                    loc18 = 0;
                    if ((signed char)((*(char *)((char*)((s)) + ((0xd8))))) + 2 > 0) {
                        char *ep = s + 0x500;
                        do {                           /* 0x1001a8ab */
                            int aa = (signed char)((*(char *)((char*)((s)) + ((0xe8)))));
                            int cc = (signed char)((*(char *)((char*)((s)) + ((0xe9)))));
                            int r  = g_18ED1C0((int)ep, (int)dst, (int)b,
                                               aa, cc, aa, 1, 2);
                            ep += r;
                            ((*(int  *)((char*)((s)) + ((0xfc))))) = r;
                            if ((unsigned)ep > (unsigned)dst)
                                BrLogPrint(BrStrGet(0x12c));
                            dst += (signed char)((*(char *)((char*)((s)) + ((0xe8))))) * (signed char)((*(char *)((char*)((s)) + ((0xe9)))));
                            if ((unsigned)(ep - s) > 0x8000)
                                BrLogPrint(BrStrGet(0x12d));
                            loc18++;
                        } while (loc18 < (signed char)((*(char *)((char*)((s)) + ((0xd8))))) + 2);
                    }
                }
                /* 0x1001a93e */
                *(int*)loc1c = 0xffffffff;
                loc1c += 4;
                loc14++;
                loc10 += 0x58;
            } while (loc14 < g_brMode0AA8B4);
        }
        goto La97c;
    La973:  /* 0x1001a973 */
        loc14 = (*(int *)&g_brRaceNEntrant);
    La97c:  /* 0x1001a97c */
        {
            int sel = ((*(int *)&DAT_105ccb68[8]) != 0) ? 4 : (((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 5) ? 4 : 0);
            g_brRaceLightT = *(float*)((char*)&g_0A957C + sel * 8);
            (*(int *)&g_brRaceLights) = *(int*)((char*)&g_0A9578 + sel * 8);
            (*(int *)&g_brRaceScript) = sel;
        }
        if ((*(int *)&DAT_105ccb68[8]) == 0) {
            (*(int *)&g_brRaceNFinished) = 0;
            if ((*(int *)&g_brRaceNDriver) > 0) {                 /* 0x1001a9d3 */
                char *s = (char*)&(*(int *)&g_aBrRaceDriver);
                int   i = 0;
                do { ((Obj*)s)->m_1005F310(); i++; s += 0x80; } while (i < (*(int *)&g_brRaceNDriver));
            }
        }
        BrRaceDifficultyApply(DAT_104b15e8);                 /* 0x1001a9ef */
        if ((*(int *)((char *)&g_aBrEntRecs + 0x7C)) == 0) {
            int v = (*(int *)&g_Br0B380C);
            if (v == 0 || v == 6) (*(int *)&g_brRaceBeginAirplane) = 0;
        }
        BrFadeSetTarget(0x3f800000, 0x3e4ccccd);   /* 0x1001aa1c */
        BrFadeSetTargetA(0x3f800000, 0x3e4ccccd);
        BrFadeSetTargetB(0x3f800000, 0x3e4ccccd);
        (*(int *)&DAT_105ccb68[11]) = 1;                           /* 0x1001aa5e */
        if ((*(int *)&DAT_105ccb68[8]) == 0) {                    /* 0x1001aa66 */
            DAT_105bcaec = (int)&g_5BCAF8;
            BrModelLoad(&g_5BCAF8, "misc\\modelLights.blob");   /* a third argument the original never reads */
            BrAnimSetOnce(DAT_105bcaec);
        }
        BrPfxReset();                         /* 0x1001aa96 */
        BrCollRespReset();
        (*(int *)&g_BrX06909B4) = 0;
        (*(int *)&DAT_105ccb68[12]) = 0;
        (*(int *)&g_brRaceBeginSeq888) = 0xffffffff;
        g_brRaceBeginSeqT = 0;
        (*(int *)&g_brRaceBeginSeqIdx) = 0;
        (*(int *)((char *)&g_aBrEntRecs + 0xA8)) = 1;
        if ((*(int *)&DAT_105ccb68[8]) == 0) { BrReset_1002E13B(); BrSndNearestReset(); }
        (*(int *)((char *)&g_aBrEntRecs + 0x54)) = 0;                           /* 0x1001aadf */
        if ((*(int *)&DAT_105ccb68[8]) != 0) {
            BrReplayRewind();
        } else {
            BrReplayReset();                     /* 0x1001aaf5 */
            if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 4) BrSet_1006AA90();
            if ((*(char *)&DAT_100bb2e0) != 0) {                /* 0x1001ab0e */
                int c = g_5bc760;
                int arg;
                if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 4 && c == 2)      arg = 0xc;
                else if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 4 && c == 1) arg = 0xd;
                else                              arg = BrCdTrackRandom();
                BrCdTrackPlay(arg);
                BrCdVolumeSet((unsigned char)(*(char *)&DAT_100bb2e0));
            }
        }
        BrRaceBeginResetOnce();                         /* 0x1001ab56 */
        BrPodNop();
        BrRaceClockReset();
    }
    else {                                      /* 0x1001ab71 (g_5CCB94 != 0) */
        if ((*(int *)&g_brRaceNet) != 0) {
            int r = BrNetStackPop221288();
            if (r >= 0 && (*(int *)&DAT_105ccb68[8]) == 0) BrCarTableRemove(r);
        }
    }

    /* 0x1001ab93: per-frame tail (merge point) */
    g_17A5F20 = 0;
    if ((*(int *)&g_brRaceLights) == 4 && (*(int *)&g_brRaceNEntrant) > 0) {
        char *s = (char*)&(*(Driver (*)[])&g_aBrRaceCar);
        int   i = 0;
        do { ((Obj*)s)->m_10060A30(); i++; s += 0x2b68; } while (i < (*(int *)&g_brRaceNEntrant));
    }
    if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ != 4 && (*(int *)&g_BrX06909B4) == 0)      /* 0x1001abd0 */
        (*(int *)&g_brRaceBeginMirrorOff) ^= 1;
    BrFrameClockStep();
    (*(int *)&g_brRaceHudA) = 0;
    loc10 = (*(int *)&g_BrX06909B4);
    (*(int *)&DAT_105ccb68[4]) = 0;
    if ((*(int *)&g_brRaceLights) < 3) {                       /* 0x1001ac00 */
        if ((*(int *)&g_brRaceTick) != 0) {                  /* 0x1001ac0f: je ac42 skips when ==0 */
            if ((*(int *)&g_brRaceNEntrant) > 0) {
                char *p = (char*)&(*(int *)((char *)&g_aBrRaceCar + 0x1008)) /* BR_LP64_BYTE_VIEW */;
                int   c = (*(int *)&g_brRaceNEntrant);
                do { *(int*)(p - 4) = 0; *(int*)p = 0x3c23d70a; p += 0x2b68; }
                while (--c);
            }
        } else if ((*(int *)&g_brRaceNet) == 0 || (*(int *)&g_brRace18EEED8) == 0) {   /* 0x1001ac8c */
            if ((*(int *)&g_brRaceNEntrant) > 0) {
                char *s = (char*)&(*(int *)((char *)&g_aBrRaceCar + 0x1008)) /* BR_LP64_BYTE_VIEW */;
                int   i = 0;
                do { *(int*)(s - 4) = BrStrGet(0xee); *(int*)s = 0x3c23d70a;
                     i++; s += 0x2b68; } while (i < (*(int *)&g_brRaceNEntrant));
            }
        } else {                              /* 0x1001ac52 */
            if ((*(int *)&g_brRaceNEntrant) > 0) {
                char *s = (char*)&(*(int *)((char *)&g_aBrRaceCar + 0x1008)) /* BR_LP64_BYTE_VIEW */;
                int   i = 0;
                do { *(int*)(s - 4) = BrStrGet(0xed); *(int*)s = 0x3c23d70a;
                     i++; s += 0x2b68; } while (i < (*(int *)&g_brRaceNEntrant));
            }
        }
        (*(int *)&DAT_105ccb68[4]) = 1;                         /* 0x1001acc4 */
        if ((*(int *)&g_brRaceNDriver) > 0) {
            char *s = (char*)&(*(int *)((char *)&g_aBrRaceDriver + 0x68)) /* BR_LP64_BYTE_VIEW */;
            int   i = 0;
            do {
                int *c;
                *(int*)s |= 1;
                c = *(int**)(s - 8);
                if (c != 0 && ((*(int  *)((char*)((c)) + ((0x140))))) < (*(int *)&g_brRaceNEntrant)) {
                    if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 1 || (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 6) {   /* 0x1001ad17 */
                        short dx = (short)(DAT_104b15e8 - 1);
                        int   e, base;
                        if (dx > 2 || dx < 0) dx = 0;
                        e = ((*(int  *)((char*)((c)) + ((0xe64))))) * 3 + dx;
                        base = *(int*)((char*)&(g_apBrRaceDiff[0]) + (*(int *)&g_Br0B380C) * 4);
                        ((*(int  *)((char*)((c)) + ((0xff0))))) = *(int*)(base + (e * 7) * 4 + 0x44);
                    }
                    ((*(int  *)((char*)((c)) + ((0x1000))))) = 0x3f800000;     /* 0x1001ad56 */
                    if ((*(int *)&g_brRaceLights) == 0) {            /* 0x1001adac */
                        (*(int *)&g_brRaceHudA) = -1;                /* ebx = -1 (0x10019ae9 `or ebx,-1`), NOT 1 */
                        (*(int *)&DAT_105ccb68[3]) = 0;
                    } else if ((*(int *)&g_brRaceLights) == 2) {     /* 0x1001ad6e */
                        int k = (*(int *)&DAT_105ccb68[3]);
                        (*(int *)&g_brRaceHudA) = 1;
                        if (*(float*)((char*)&g_0A9548 + k * 4) > g_brRaceLightT) {
                            k++;
                            (*(int *)&DAT_105ccb68[3]) = k;
                            if (k == 4) BrSfxSrcBeep2();
                            else        BrSfxSrcBeep();
                        }
                    }
                }
                i++;
                s += 0x80;
            } while (i < (*(int *)&g_brRaceNDriver));
        }
        g_brRaceFade = 0.0f;                       /* 0x1001adcc */
        goto Lb0cd;
    }
    else {                                    /* 0x1001addb: g_5BC8F8 >= 3 */
        if ((*(int *)&g_brRaceLights) == 3) {                  /* 0x1001addd */
            (*(int *)&g_brRaceHudA) = 1; (*(int *)&DAT_105ccb68[4]) = 1;
            if ((*(int *)&g_brRaceNDriver) > 0) {
                char *p = (char*)&(*(int *)((char *)&g_aBrRaceDriver + 0x68)) /* BR_LP64_BYTE_VIEW */;
                int   c = (*(int *)&g_brRaceNDriver);
                do { *(int*)p &= 0xfffffffe; p += 0x80; } while (--c);
            }
            { int k = (*(int *)&g_brRaceScript);
              float t = *(float*)((char*)&g_0A957C + k * 8);
              float a = (t - g_brRaceLightT) / t;
              g_brRaceFade = a * a * g_0773B4; }
            goto Lb0cd;
        } else if ((*(int *)&g_brRaceLights) == 4) {           /* 0x1001ae38 */
            loc14 = 1;
            if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 4) {              /* 0x1001ae54 */
                int c = g_5bc760;
                if (c == 2) {
                    if ((*(float *)((char *)&g_aBrRaceCar + 0xFEC)) /* BR_LP64_BYTE_VIEW */ > g_0773B8) goto Lae_a1;
                    goto Lae89;
                }
                if ((*(float *)((char *)&g_aBrRaceCar + 0xFEC)) /* BR_LP64_BYTE_VIEW */ > g_0773BC) goto Lae_a1;
            }
        Lae89:  /* 0x1001ae89 */
            /* 0x1001ae8c `jne 0x1001aee2`: every mode but 5 skips the check
             * below entirely -- it is NOT a fall-through into Lae_a1.  Mode 5
             * reaches it only on an ordered greater-than (`test ah,0x41 /
             * jne` at 0x1001ae9f: less, equal and unordered all skip). */
            if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ != 5) goto Laee2;
            if (!((*(float *)((char *)&g_aBrRaceCar + 0xFEC)) /* BR_LP64_BYTE_VIEW */ > g_0773C0)) goto Laee2;
        Lae_a1: /* 0x1001aea1 */
            if (BrFadeIsClosing() == 0) {
                BrFadeSetTarget(0, 0x3e4ccccd);
                BrFadeSetTargetA(0, 0x3e4ccccd);
                (*(int *)&DAT_105ccb68[12]) = 1; (*(int *)&DAT_105ccb68[8]) = 0; (*(int *)&DAT_105ccb68[9]) = 0;
                loc14 = 0;
            }
        Laee2:  /* 0x1001aee2 */
            if ((*(int *)&g_brRaceNEntrant) > 0) {
                int i = 0;
                do {
                    if ((((*(char *)(((char *)&g_aBrRaceDriver[i].f68) + ((0))))) & 2) != 0) {
                        int handled = 0;
                        if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 2) {              /* 0x1001af07 */
                            int *pd = *(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */[i];
                            if (((*(int  *)((char*)((pd)) + ((i * 4 + 0x2c))))) != 0) {
                                if (i == 0) {             /* 0x1001af38 */
                                    int  e8 = (*(int *)&g_brRace5BC8D8);
                                    int  clx = (signed char)(*(int *)&g_aBrRaceBeginRec);
                                    if (!((*(int *)((char *)&g_aBrRaceCar + 0xFF8)) /* BR_LP64_BYTE_VIEW */ != 0 && clx == (*(int *)&g_Br0B380C) && e8 > 8)) {
                                        int *p0 = *(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */;
                                        int  s34 = ((*(int  *)((char*)((p0)) + ((0x34)))));
                                        int *q;
                                        BrPodNop((char *)((char *)((char *)((char *)(&g_0A97A8)))), 0, s34, e8, (s34 < e8),
                                                     clx, (*(int *)&g_Br0B380C), (clx != (*(int *)&g_Br0B380C)),
                                                     e8, 8, (e8 <= 8));
                                        (*(int *)&g_brRace5BC8D8) = 0x10;
                                        /* 0x1001afad: q IS the pointer at driver0+0x2c
                                         * (mov eax,[edx+0x2c]; mov ecx,[eax]) */
                                        q = (int*)((*(int  *)((char*)((*(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */)) + ((0x2c)))));
                                        (*(int *)&g_aBrRaceBeginRec) = q[0];
                                        (*(int *)&g_aBrRaceBeginRec[4]) = q[1];
                                        g_5BC8E8 = 0; g_5BC8EC = 0;
                                        (*(int *)((char *)&g_aBrRaceCar + 0xFFC)) /* BR_LP64_BYTE_VIEW */ = BrStrGet(0xef);
                                        (*(int *)((char *)&g_aBrRaceCar + 0x1000)) /* BR_LP64_BYTE_VIEW */ = 0x3f800000;
                                        BrTimeFormat(&(*(int *)((char *)&g_aBrRaceCar + 0x100C)) /* BR_LP64_BYTE_VIEW */, (*(float *)((char *)&g_aBrRaceCar + 0xFEC)) /* BR_LP64_BYTE_VIEW */);
                                        (*(int *)((char *)&g_aBrRaceCar + 0x1004)) /* BR_LP64_BYTE_VIEW */ = (int)&(*(int *)((char *)&g_aBrRaceCar + 0x100C)) /* BR_LP64_BYTE_VIEW */;
                                        (*(int *)((char *)&g_aBrRaceCar + 0x1008)) /* BR_LP64_BYTE_VIEW */ = 0x3f800000;
                                        (*(int *)&g_brRaceBeginRecArmed) = 1;
                                    }
                                }
                                ((*(int  *)((char*)((*(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */[i])) + ((i * 4 + 0x2c))))) = 0;   /* 0x1001b01b */
                                handled = 1;
                            }
                        }
                        if (!handled &&                    /* 0x1001b03a */
                            ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 1 || (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 6) && (*(int *)&g_brRaceNEntrant) > 0) {
                            int *pd = *(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */[i];
                            int  off = 0x3c;
                            int  j = 0;
                            do {
                                *(int*)((char*)pd + off) = *(int*)((char*)pd + off - 8);
                                off += 4; j++;
                            } while (j < (*(int *)&g_brRaceNEntrant));
                        }
                    } else {
                        loc14 = 0;                         /* 0x1001b085 */
                    }
                    i++;
                } while (i < (*(int *)&g_brRaceNEntrant));
            }
            if (loc14 != 0) goto Lb0f8;                    /* 0x1001b092 */
            goto Lb171;
        } else if ((*(int *)&g_brRaceLights) == 5) {           /* 0x1001b09d */
            int st = (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */;
            if (st == 2 || st == 1 || st == 0 || st == 6)
                if ((*(int *)&DAT_105ccb68[8]) == 0) BrCarStateSave();
            goto Lb0f8;
        } else if ((*(int *)&g_brRaceLights) == 6) {           /* 0x1001b0c8 */
            goto Lb0cd;
        } else if ((*(int *)&g_brRaceLights) == 7) {           /* 0x1001b127 */
            if ((*(int *)&DAT_105ccb68[12]) == 0) {
                BrFadeSetTargetA(0, 0x3e4ccccd);
                if ((*(int *)&DAT_105ccb68[9]) == 0) BrFadeSetTargetB(0, 0x3e4ccccd);
                BrFadeSetTarget(0, 0x3e4ccccd);
                (*(int *)&DAT_105ccb68[12]) = 1;
            }
            goto Lb171;
        } else goto Lb171;
    }
Lb0cd:  /* 0x1001b0cd shared timing */
    if ((*(int *)&g_BrX06909B4) != 0) goto Lb171;
    g_brRaceLightT = g_brRaceLightT - g_brRaceFlyStep;
    if (!(g_brRaceLightT < g_0773A4)) goto Lb171;
Lb0f8:  /* 0x1001b0f8 step advance */
    if ((*(int *)&g_brRaceTick) == 0) goto Lb171;
    {
        int k = ++(*(int *)&g_brRaceScript);
        g_brRaceLightT = *(float*)((char*)&g_0A957C + k * 8);
        (*(int *)&g_brRaceLights) = *(int*)((char*)&g_0A9578 + k * 8);
    }
Lb171:  /* 0x1001b171 merge */
    BrPodNop((struct BrMat4 *)((struct BrMat4 *)((struct BrMat4 *)((struct BrMat4 *)(0)))), 0, 0, 0xc8, 0xff);
    BrZeroRegions();
    if ((*(int *)&g_BrCarCount) > 0) {
        char *s = (char*)&(*(Driver (*)[])&g_aBrRaceCar);
        int   i = 0;
        do { ((Obj*)s)->m_10061430(); i++; s += 0x2b68; } while (i < (*(int *)&g_BrCarCount));
    }
    if ((*(int *)&g_brRaceNDriver) > 0) {
        char *s = (char*)&(*(int *)&g_aBrRaceDriver);
        int   i = 0;
        do { ((Obj*)s)->m_10061F60(); i++; s += 0x80; } while (i < (*(int *)&g_brRaceNDriver));
    }
    if (!((*(int *)&g_brRaceNet) == 0 && (*(int *)&DAT_105ccb68[8]) == 0)) {      /* 0x1001b1d9 */
        if ((*(int *)&g_brRaceNDriver) > 0) {
            char *s = (char*)&(*(int *)&g_aBrRaceDriver);
            int   i = 0;
            do { ((Obj*)s)->m_100623A0(); i++; s += 0x80; } while (i < (*(int *)&g_brRaceNDriver));
        }
    }
    if ((*(int *)&g_brRaceNDriver) > 0) {                           /* 0x1001b20b */
        char *s = (char*)&(*(int *)&g_aBrRaceDriver);
        int   i = 0;
        do { ((Obj*)s)->m_100623E0(); i++; s += 0x80; } while (i < (*(int *)&g_brRaceNDriver));
    }
    if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 0 && (*(int *)&g_brRaceNEntrant) > 0) {          /* 0x1001b22d */
        char *s = (char*)&(*(Driver (*)[])&g_aBrRaceCar);
        int   i = 0;
        do { ((Obj*)s)->m_1005F6C0(); i++; s += 0x2b68; } while (i < (*(int *)&g_brRaceNEntrant));
    }
    BrRankAssign();                               /* 0x1001b25c */
    if ((*(int *)&g_BrX06909B4) != 0) goto Lb887;
    if ((*(int *)&DAT_105ccb68[8]) == 2) goto Lb887;
    BrPodNop(0, 0x80, 0x80, 0, 0xff);
    BrPfxTick();
    BrPodNop(0, 0, 0xff, 0xff, 0xff);
    BrWeatherStepParticles();
    if ((*(int *)&g_brRaceBeginAirplane) != 0 && (*(int *)&g_brRaceBeginAirArmed) == 0) {         /* 0x1001b2b6 leaderboard scan */
        if ((*(int *)((char *)&g_aBrEntRecs + 0x84)) != 0 || (*(int *)((char *)&g_aBrEntRecs + 0x80)) != 0 || (*(int *)((char *)&g_aBrEntRecs + 0x7C)) != 0) {
            if ((*(int *)&g_Br0B380C) == 2 || (*(int *)&g_Br0B380C) == 8) goto Lb365;
        }
        if ((*(int *)&g_brRaceNEntrant) > 0) {
            char *s   = (char*)&(*(int *)((char *)&g_aBrRaceCar + 0x294C)) /* BR_LP64_BYTE_VIEW */;
            int   edx = 0;
            int   rem = (*(int *)&g_brRaceNEntrant);
            do {
                int c = *(int*)s;
                int a;
                for (a = 0; a < c; a++) {
                    if (*(int*)(s - 0x19a4) == (*(int *)&g_brRaceBeginCamMinus1)) {
                        unsigned short w =
                            *(unsigned short*)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x290C)) /* BR_LP64_BYTE_VIEW */ + (a + edx) * 2);
                        if ((int)w == (*(int *)&g_brRaceBeginAirTrigger)) { (*(int *)&g_brRaceBeginAirArmed) = 1; break; }
                    }
                }
                edx += 0x15b4;
                s += 0x2b68;
            } while (--rem);
        }
    }
Lb365:  /* 0x1001b365 */
    if ((*(int *)&g_brRaceBeginSpecialsN) > 0) {
        char *e = (char*)&(*(int *)&g_aBrRaceSpecial);
        int   b = 0;
        do {
            int sw = (signed char)((*(char *)((char*)((e)) + ((8)))));
            if ((unsigned)sw <= 3) {                /* jmp [.. 0x1001c678] */
                switch (sw) {
                case 0:  /* 0x1001b393 */
                    BrMat4RotateAxis(&(*(int *)&g_brRaceSpecialM), ((*(int  *)((char*)((e)) + ((4))))), 0, 0, 0x3f800000);
                    goto Lb_emit;
                case 1:  /* 0x1001b3a0 */
                    BrMat4RotateAxis((struct BrMat4 *)(&(*(int *)&g_brRaceSpecialM)), ((*(int  *)((char*)((e)) + ((4))))), 0x3f800000, 0, 0);
                    goto Lb_emit;
                case 2:  /* 0x1001b3ad */
                    BrMat4RotateAxis((struct BrMat4 *)(&(*(int *)&g_brRaceSpecialM)), ((*(int  *)((char*)((e)) + ((4))))), 0, 0x3f800000, 0);
                Lb_emit: /* 0x1001b3b8 */
                    {
                        int  *m = (int*)((char*)g_BrDrawTrackFlags + ((*(int  *)((char*)((e)) + ((0))))) * 84);
                        BrMat4Mul((const struct BrMat4 *)(&(*(int *)&g_brRaceSpecialM)), (const struct BrMat4 *)(m), (struct BrMat4 *)(m));
                        m = (int*)((char*)g_BrDrawTrackFlags + ((*(int  *)((char*)((e)) + ((0))))) * 84);
                        *(unsigned short*)((char*)m + 0x4c) &= 0xdfff;
                    }
                    break;
                case 3:  /* 0x1001b403 */
                    if ((*(int *)&g_brRaceBeginAirplane) == 0 || (*(int *)&g_brRaceBeginAirArmed) == 0) break;
                    {
                        float v20[3], v2c[3], v38[3];
                        float dt = ((*(int *)&g_Br0B380C) == 1 || (*(int *)&g_Br0B380C) == 7)
                                   ? g_brRaceFlyStep * g_brRaceFlySpeedB    /* 0x1001b436 */
                                   : g_brRaceFlyStep * g_brRaceFlySpeedA;   /* 0x1001b428 */
                        g_brRaceBeginPathT = g_brRaceBeginPathT - dt;           /* 0x1001b442 fsubr */
                        if (g_brRaceBeginPathT > g_brRaceBeginPathSeg) {          /* 0x1001b465 interp loop */
                            int ended = 0;
                            do {
                                int   wi;
                                g_brRaceBeginPathT = g_brRaceBeginPathT - g_brRaceBeginPathSeg;
                                wi = (*(int *)&g_brRaceBeginPathIdx) + 1;
                                (*(int *)&g_brRaceBeginPathIdx) = wi;
                                if (wi >= (*(int *)&g_brRaceBeginPathLen)) { ended = 1; break; }
                                BrVec3Midpoint(v2c, (char*)g_pBrRaceFlyPos + wi * 12 - 0xc,
                                                  (char*)g_pBrRaceFlyAim + wi * 12 - 0xc);
                                BrVec3Midpoint(v20, (char*)g_pBrRaceFlyPos + wi * 12,
                                                  (char*)g_pBrRaceFlyAim + wi * 12);
                                g_brRaceBeginPathSeg = BrVec3Dist(v2c, v20);
                                BrVec3Sub((struct BrVec3 *)(&(*(float *)&g_brRaceFlyDirCur)), v20, v2c);
                                br_dl_normalise(&(*(float *)&g_brRaceFlyDirCur));
                                if ((*(int *)&g_brRaceBeginPathIdx) < 2) {         /* 0x1001b525 */
                                    (*(float *)&g_brRaceFlyDirPrev) = (*(float *)&g_brRaceFlyDirCur); (*(float *)((char *)&g_brRaceFlyDirPrev + 0x4)) = (*(float *)((char *)&g_brRaceFlyDirCur + 0x4)); (*(float *)((char *)&g_brRaceFlyDirPrev + 0x8)) = (*(float *)((char *)&g_brRaceFlyDirCur + 0x8));
                                } else {                    /* 0x1001b549 */
                                    BrVec3Midpoint(v38, (char*)g_pBrRaceFlyPos + ((*(int *)&g_brRaceBeginPathIdx) - 2) * 12,
                                                      (char*)g_pBrRaceFlyAim + ((*(int *)&g_brRaceBeginPathIdx) - 2) * 12);
                                    BrVec3Sub((struct BrVec3 *)(&(*(float *)&g_brRaceFlyDirPrev)), v2c, v38);
                                    br_dl_normalise(&(*(float *)&g_brRaceFlyDirPrev));
                                }
                                if ((*(int *)&g_brRaceBeginPathIdx) + 1 == (*(int *)&g_brRaceBeginPathLen)) {   /* 0x1001b5a8 */
                                    (*(float *)&g_brRaceFlyDirNext) = (*(float *)&g_brRaceFlyDirCur); (*(float *)((char *)&g_brRaceFlyDirNext + 0x4)) = (*(float *)((char *)&g_brRaceFlyDirCur + 0x4)); (*(float *)((char *)&g_brRaceFlyDirNext + 0x8)) = (*(float *)((char *)&g_brRaceFlyDirCur + 0x8));
                                } else {                    /* 0x1001b5ca: the NEXT point, wi+1 (eax from 0x1001b5a1) */
                                    BrVec3Midpoint(v38, (char*)g_pBrRaceFlyPos + ((*(int *)&g_brRaceBeginPathIdx) + 1) * 12,
                                                      (char*)g_pBrRaceFlyAim + ((*(int *)&g_brRaceBeginPathIdx) + 1) * 12);
                                    BrVec3Sub((struct BrVec3 *)(&(*(float *)&g_brRaceFlyDirNext)), v38, v20);
                                    br_dl_normalise(&(*(float *)&g_brRaceFlyDirNext));
                                }
                            } while (g_brRaceBeginPathT > g_brRaceBeginPathSeg);   /* 0x1001b619 */
                            if (ended) (*(int *)&g_brRaceBeginAirplane) = 0;         /* 0x1001b632 */
                        }
                        if ((*(int *)&g_brRaceBeginAirplane) != 0) {                /* 0x1001b64a transform+emit */
                            float lerpT;
                            int   lerpBits;
                            /* 0x1001b665: stored, and every use reads the float */
                            lerpT = g_brRaceBeginPathT / g_brRaceBeginPathSeg;
                            lerpBits = *(int*)&lerpT;
                            lerpT = *(float*)&lerpBits;
                            char *row   = (char*)g_BrDrawTrackFlags + (*(int *)&g_brRaceBeginAirplane) * 84;
                            /* 0x1001b670: the interpolated position lands in v2c
                             * (lea [esp+0x38] under three pushes) */
                            BrVec3Lerp(v2c, (char*)g_pBrRaceFlyPos + (*(int *)&g_brRaceBeginPathIdx) * 12,
                                              (char*)g_pBrRaceFlyPos + (*(int *)&g_brRaceBeginPathIdx) * 12 - 0xc, lerpT);
                            BrVec3Lerp(v20, (char*)g_pBrRaceFlyAim + (*(int *)&g_brRaceBeginPathIdx) * 12,
                                              (char*)g_pBrRaceFlyAim + (*(int *)&g_brRaceBeginPathIdx) * 12 - 0xc, lerpT);
                            BrVec3Midpoint((struct BrVec3 *)(row + 0x30), v2c, v20);
                            /* 0x1001b6c6: [esp+0x20] under three pushes is the
                             * lerpT slot, not v20 -- the heading blends on t */
                            if (lerpT > g_brRaceFlyBlendA)
                                BrVec3Lerp((struct BrVec3 *)(row), (const struct BrVec3 *)(&(*(float *)&g_brRaceFlyDirNext)), (const struct BrVec3 *)(&(*(float *)&g_brRaceFlyDirCur)), lerpT - g_brRaceFlyBlendA);
                            else                            /* 0x1001b708 */
                                BrVec3Lerp((struct BrVec3 *)(row), (const struct BrVec3 *)(&(*(float *)&g_brRaceFlyDirCur)), (const struct BrVec3 *)(&(*(float *)&g_brRaceFlyDirPrev)), lerpT - g_brRaceFlyBlendB);
                            br_dl_normalise(row);
                            /* 0x1001b774 -> 0x1001b799: the forward difference goes
                             * into the first cross product UNnormalised */
                            BrVec3Sub((struct BrVec3 *)(row + 0x20), v2c, v20);
                            BrVec3Cross((struct BrVec3 *)(row + 0x10), (const struct BrVec3 *)(row), (const struct BrVec3 *)(row + 0x20));
                            br_dl_normalise(row + 0x10);
                            BrVec3Cross((struct BrVec3 *)(row + 0x20), (const struct BrVec3 *)(row + 0x10), (const struct BrVec3 *)(row));
                            br_dl_normalise(row + 0x20);
                            BrVec3ScaleBy((struct BrVec3 *)(row), g_brRaceBeginPathScale);
                            BrVec3ScaleBy((struct BrVec3 *)(row + 0x20), -g_brRaceBeginPathScale);
                            BrVec3ScaleBy((struct BrVec3 *)(row + 0x10), g_brRaceBeginPathScale);
                        }
                    }
                    break;
                }
            }
            b++;
            e += 0xc;
        } while (b < (*(int *)&g_brRaceBeginSpecialsN));
    }
Lb887:  /* 0x1001b887 */
    if ((*(int *)&g_BrCarCount) > 0) {
        char *s = (char*)&(*(Driver (*)[])&g_aBrRaceCar);
        int   i = 0;
        do { ((Obj*)s)->m_10061470(); i++; s += 0x2b68; } while (i < (*(int *)&g_BrCarCount));
    }
    BrSndNearestInvalidate();                               /* 0x1001b8ae */
    loc1c = 0;
    if (g_brMode0AA8B4 > 0) {                           /* 0x1001b8c9 leader-attach loop */
        do {
            int   drv    = *(int*)&(*(int *)&BrG_6C1628[4]);
            int   active = (*(int *)&g_aBrRaceCar[drv].pMatA);
            if ((*(int *)&g_brRaceBeginAirplane) != 0 && (*(int *)&g_brRaceBeginAirArmed) != 0) {
                int *m = (int*)((char*)g_BrDrawTrackFlags + (*(int *)&g_brRaceBeginAirplane) * 84 + 0x30);
                BrSndNearestOfferTrack((*(int *)&g_brRaceBeginAirplane), (const struct BrVec3 *)(m), active);
            }
            if (active != 0) {                    /* 0x1001b918 */
                char *s = (char*)&(g_aBrRaceBeginFx[0]);
                int   j = 0;
                while (j < (*(int *)&g_brRaceBeginFxCount)) {
                    int *m = (int*)((char*)g_BrDrawTrackFlags + *(int*)s * 84 + 0x30);
                    BrSndNearestOfferDefault((*(int *)&g_brRaceBeginAirplane), (const struct BrVec3 *)(m), active);
                    j++;
                    s += 4;
                }
            }
            BrPodNop((short *)((short *)((short *)((short *)(active)))));                 /* 0x1001b954 */
            loc1c++;
        } while (loc1c < g_brMode0AA8B4);
    }
    BrSndNearestCommit();                               /* 0x1001b977 */
    BrRaceHudFrame();
    loc18 = BrSnapPickSlot();
    {                                             /* 0x1001b990 deep-copy loop */
        char *obase = (char*)&(*(char (*)[])((char *)&g_aBrSnap + 0xA08)) /* BR_LP64_BYTE_VIEW */ + 188656 * loc18;
        int   doff  = 0;
        int   ct    = (*(int *)&g_BrCarCount); if (ct == 0) ct = 1;
        int   k     = 0;
        while (k < ct) {
            char *out = obase + doff;
            int   a;
            memcpy(out, (char*)&(*(Driver (*)[])&g_aBrRaceCar) + doff, 11112);      /* rep movsd 0xada */
            a = *(int*)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x2734)) /* BR_LP64_BYTE_VIEW */ + doff);             /* active-model fixup */
            if (a != 0) {
                int fx = 0;
                if      (a == (int)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x273C)) /* BR_LP64_BYTE_VIEW */ + doff)) fx = (int)(out + 0x273C);
                else if (a == (int)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x2890)) /* BR_LP64_BYTE_VIEW */ + doff)) fx = (int)(out + 0x2890);
                else if (a == (int)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x2780)) /* BR_LP64_BYTE_VIEW */ + doff)) fx = (int)(out + 0x2780);
                else if (a == (int)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x27C4)) /* BR_LP64_BYTE_VIEW */ + doff)) fx = (int)(out + 0x27C4);
                else if (a == (int)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x2808)) /* BR_LP64_BYTE_VIEW */ + doff)) fx = (int)(out + 0x2808);
                *(int*)(out + 0x2734) = fx;
            }
            a = *(int*)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x2738)) /* BR_LP64_BYTE_VIEW */ + doff);
            if (a != 0) {
                int fx = 0;
                if      (a == (int)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x273C)) /* BR_LP64_BYTE_VIEW */ + doff)) fx = (int)(out + 0x273C);
                else if (a == (int)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x2890)) /* BR_LP64_BYTE_VIEW */ + doff)) fx = (int)(out + 0x2890);
                else if (a == (int)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x2780)) /* BR_LP64_BYTE_VIEW */ + doff)) fx = (int)(out + 0x2780);
                else if (a == (int)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x27C4)) /* BR_LP64_BYTE_VIEW */ + doff)) fx = (int)(out + 0x27C4);
                else if (a == (int)((char*)&(*(int *)((char *)&g_aBrRaceCar + 0x2808)) /* BR_LP64_BYTE_VIEW */ + doff)) fx = (int)(out + 0x2808);
                *(int*)(out + 0x2738) = fx;
            }
            k++;
            doff += 0x2b68;
        }
    }
    if ((*(int *)&g_brRaceNDriver) > 0) {                           /* 0x1001bb2a record-copy loop */
        char *src = (char*)&(*(char (*)[])((char *)&g_aBrRaceDriver + 0x60)) /* BR_LP64_BYTE_VIEW */;
        char *dst = (char*)&(*(char (*)[])((char *)&g_aBrSnap + 0x64)) /* BR_LP64_BYTE_VIEW */ + 188656 * loc18;
        int   n   = (*(int *)&g_brRaceNDriver);
        do {
            int v = *(int*)src;
            memcpy(dst - 0x60, src - 0x60, 0x80);         /* rep movsd 0x20 */
            if (v != 0) {                                 /* re-index self pointer */
                int di = (v - (int)&(*(Driver (*)[])&g_aBrRaceCar)) / 0x2b68;   /* magic /0x2b68 */
                *(int*)dst = (int)((char*)&(*(char (*)[])((char *)&g_aBrSnap + 0xA08)) /* BR_LP64_BYTE_VIEW */ + 188656 * loc18 + di * 0x2b68);
            }
            src += 0x80;
            dst += 0x80;
        } while (--n);
    }
    /* 0x1001bbcd: camera-state snapshot + fixup */
    memcpy((char*)&(*(char (*)[])((char *)&g_aBrSnap + 0x2C088)) /* BR_LP64_BYTE_VIEW */ + 188656 * loc18, &(BrG_6C1628[0]), 0x16 * 4);   /* rep movsd 0x16 */
    BrPfxSaveState((char*)&(*(char (*)[])((char *)&g_aBrSnap + 0x2C088)) /* BR_LP64_BYTE_VIEW */ + 188656 * loc18 + 0x5c);
    if ((*(int *)&g_brRaceBeginLimitOn) == 0) goto Lc583;                /* 0x1001bc04 -> replay-advance */
    g_5BCAF0 = BrSub10075020();                    /* 0x1001bc16 */
    g_5BC76C = 0;
    (*(int *)&g_brRaceBeginLimitOn) = 0;
    (*(int *)&DAT_105ccb68[14]) = 0;
    GhostBody:  /* 0x1001bc37 */
        if ((*(int *)&g_BrX06909B4) == 2) {                      /* 0x1001bc44 */
            int f = (*(int *)&g_BrX18ABAD0);
            if (f & 0x8000) {
                if (DAT_105bc8dc == 0) {              /* 0x1001bca5 */
                    BrFadeSetTarget(0, 0x3e4ccccd);
                    BrFadeSetTargetA(0, 0x3e4ccccd);
                    BrFadeSetTargetB(0, 0x3e4ccccd);
                    (*(int *)&DAT_105ccb68[9]) = 0; (*(int *)&DAT_105ccb68[12]) = 2;
                } else if (DAT_105bc8dc == 1) {       /* 0x1001bc5e */
                    if ((*(int *)&g_brRaceNet) != 0 && (*(int *)&g_brRaceTick) != 0) {
                        BrNetLockSetIfZero221314();
                    } else {                      /* 0x1001bc75 */
                        BrFadeSetTargetA(0x3f800000, 0x3e4ccccd);
                        BrFadeSetTargetB(0x3f800000, 0x3e4ccccd);
                        g_5CCB64 = 2; loc10 = 0;
                    }
                }
            }
            f = (*(int *)&g_BrX18ABAD0);                         /* 0x1001bcdb */
            if (f & 0x1000) DAT_105bc8dc = 1 - DAT_105bc8dc;
            if (f & 0x2000) DAT_105bc8dc = 1 - DAT_105bc8dc;
            if ((*(int *)&DAT_105ccb68[6]) == 0) goto Lc002;
            (*(int *)&DAT_105ccb68[6]) = 0;                          /* 0x1001bd1b */
            BrFadeSetTargetA(0x3f800000, 0x3e4ccccd);
            BrFadeSetTargetB(0x3f800000, 0x3e4ccccd);
            g_5CCB64 = 2; loc10 = 0;
            if ((*(int *)&g_brRaceNet) == 0 || (*(int *)&g_brRaceTick) == 0) goto Lc13d;
            BrNetSlotBroadcastTick();
            goto Lc13d;
        }
        /* 0x1001bd72: g_5CCB5C != 2 main path */
        if ((*(int *)&g_BrX06909B4) == 0) goto Lc024;
        if ((*(int *)&g_brRaceNEntrant) > 0) {                       /* 0x1001bd7a per-driver replay */
            char *s = (char*)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */;
            int   i = 0;
            do {                                  /* loop top 0x1001bd94 */
                int f = (*(int *)&g_BrX18ABAD0);
                int skip = 0;
                if (!(f & 0x8000)) {              /* 0x1001bd99 gate */
                    if (!(DAT_105bc8dc == 0 && (f & 4) && !(f & 0x3000) && !(f & 3)))
                        skip = 1;
                }
                if (!skip) {
                    if (DAT_105bc8dc != 0 || (*(char*)*(int**)s & 0x10) == 0)   /* 0x1001bdbf */
                        ((Obj*)*(int**)s)->m_1002F640(0xc010);              /* 0x1001bdca */
                    if ((unsigned)DAT_105bc8dc <= 5) {                          /* 0x1001bddc */
                        switch (DAT_105bc8dc) {        /* jmp [.. 0x1001c688] */
                        case 0:  /* 0x1001bdec */
                            if ((*(int *)&g_brRaceNet) != 0 && (*(int *)&g_brRaceTick) != 0) BrNetLockSetIfZero221314();
                            else { BrFadeSetTargetA(0x3f800000, 0x3e4ccccd);
                                   BrFadeSetTargetB(0x3f800000, 0x3e4ccccd);
                                   g_5CCB64 = 2; loc10 = 0; }
                            break;
                        case 1:  /* 0x1001be39 */
                            BrFadeSetTarget(0, 0x3e4ccccd);
                            (*(int *)&DAT_105ccb68[12]) = 0; (*(int *)&DAT_105ccb68[9]) = 0; (*(int *)&DAT_105ccb68[11]) = 0;
                            BrRaceDriverReset(); (*(int *)&g_brRaceTick) = 0; BrSndBankFree(); BrClearFlag_AB504();
                            break;
                        case 4:  /* 0x1001be73 */
                            BrFadeSetTarget(0, 0x3e4ccccd);
                            BrFadeSetTargetA(0, 0x3e4ccccd);
                            BrFadeSetTargetB(0, 0x3e4ccccd);
                            (*(int *)&DAT_105ccb68[9]) = 0; (*(int *)&DAT_105ccb68[12]) = 1;
                            if ((*(char *)&DAT_100bb2e0) != 0) { BrCdResume();
                                                 BrCdVolumeSet((unsigned char)(*(char *)&DAT_100bb2e0)); }
                            if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 6) { BrDPlayMsg6SendSelf(); BrNetClearF10220DD0(); }
                            break;
                        case 5:  /* 0x1001bedf */
                            loc10 = 1; (*(int *)&g_BrX06909B4) = 1; DAT_105bc8dc = 1;
                            break;
                        case 2: case 3: default: break;   /* 0x1001befc */
                        }
                    }
                }
                /* 0x1001befc tail */
                if ((*(int *)&DAT_105ccb68[6]) != 0) {
                    (*(int *)&DAT_105ccb68[6]) = 0;
                    BrFadeSetTargetA(0x3f800000, 0x3e4ccccd);
                    BrFadeSetTargetB(0x3f800000, 0x3e4ccccd);
                    g_5CCB64 = 2; loc10 = 0;
                }
                f = (*(int *)&g_BrX18ABAD0);
                if (f & 0x1000) {
                    DAT_105bc8dc = (DAT_105bc8dc + 5) % 6;
                    if ((*(int *)&g_brRaceNet) != 0 && DAT_105bc8dc == 1) DAT_105bc8dc = (DAT_105bc8dc + 5) % 6;
                }
                if (f & 0x2000) {
                    DAT_105bc8dc = (DAT_105bc8dc + 1) % 6;
                    if ((*(int *)&g_brRaceNet) != 0 && DAT_105bc8dc == 1) DAT_105bc8dc = (DAT_105bc8dc + 1) % 6;
                }
                if (f & 1) {
                    if (DAT_105bc8dc == 2)      BrUiSelBDec();
                    else if (DAT_105bc8dc == 3) BrUiSelADec();
                }
                if (f & 2) {
                    if (DAT_105bc8dc == 2)      BrUiSelBInc();
                    else if (DAT_105bc8dc == 3) BrUiSelAInc();
                }
                i++;
                s += 0x2b68;
            } while (i < (*(int *)&g_brRaceNEntrant));
        }
    Lc002:  /* 0x1001c002 */
        if ((*(int *)&g_brRaceNet) == 0 || (*(int *)&g_brRaceTick) == 0) goto Lc13d;
        BrNetSlotBroadcastTick();
        goto Lc13d;
    Lc024:  /* 0x1001c024 */
        if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 4 || (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 5) {     /* 0x1001c02c */
            if (BrFadeIsClosing() != 0) goto Lc13d;
            {                                     /* 0x1001c0e0 particle-mute loop */
                char *p = (char*)&(*(char (*)[])&g_aBrEnts);
                do {
                    if ((*(int *)&g_BrX18ABAD0) & 0x4000) {
                        ((Obj*)p)->m_1002F640(0xc010);
                        (*(int *)&g_brRaceBeginMovieDone) = 1; (*(int *)&DAT_105ccb68[12]) = 1; (*(int *)&DAT_105ccb68[9]) = 0;
                        BrFadeSetTargetA(0, 0x3e4ccccd);
                        BrFadeSetTarget(0, 0x3e4ccccd);
                    }
                    p += 0x15c;
                } while (p < (char*)&(*(char (*)[])((char *)&g_aBrEnts + 0x2B8)) /* BR_LP64_BYTE_VIEW */);
            }
            goto Lc13d;
        }
        if ((*(int *)&g_brRaceNEntrant) > 0) {                       /* 0x1001c03b */
            if (DAT_10226a50 != 0) {                  /* 0x1001c05f */
                int b88 = (*(int *)&DAT_105ccb68[8]);
                DAT_10226a50 = 0;
                if (b88 != 0) {
                    BrFadeSetTarget(0, 0x3e4ccccd);
                    (*(int *)&DAT_105ccb68[9]) = 0; (*(int *)&DAT_105ccb68[12]) = 1;
                } else {                          /* 0x1001c08e */
                    loc10 = 1; DAT_105bc8dc = 0;
                }
                BrFadeSetTargetA(0, 0x3e4ccccd);      /* 0x1001c09c */
                BrFadeSetTargetB(0, 0x3e4ccccd);
                ((Obj*)*(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */)->m_1002F640(0x4000);
                ((Obj*)*(int**)&(*(int *)((char *)&g_aBrRaceCar + 0x5528)) /* BR_LP64_BYTE_VIEW */)->m_1002F640(0x4000);
            } else {                              /* 0x1001c051 no-op scan */
                int a = 0;
                do { a++; } while (a < (*(int *)&g_brRaceNEntrant));
            }
        }
        goto Lc13d;
    Lc13d:  /* 0x1001c13d */
        if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 4) {
            if (g_5bc760 == 2) BrRaceCueRewind();
            if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 4 && g_5bc760 == 1) {   /* 0x1001c158 lap-time gate */
                int slot = (*(int *)&g_brRaceBeginSeqIdx) << 5;
                if (*(int*)((char*)&(*(int *)((char *)&g_brRaceRules + 0x14)) /* BR_LP64_BYTE_VIEW */ + slot) != 0) {
                    g_brRaceBeginSeqT = g_brRaceBeginSeqT + g_brRaceFlyStep;  /* 0x1001c179 x87 timer */
                    if (g_brRaceBeginSeqT > *(float*)((char*)&g_0A936C + slot)) {
                        g_brRaceBeginSeqT = 0.0f;
                        (*(int *)&g_brRaceBeginSeqIdx)++;
                    }
                }
            }
        }
        if (BrFadeIsClosing() != 0 && BrFadeIsSettled() != 0) {   /* 0x1001c1ad */
            {                                     /* 0x1001c1c7 shift-copy all drivers */
                char *s = (char*)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */;
                do {
                    if ((*(int *)&g_brRaceNEntrant) > 0) {
                        int off = 0x3c;
                        int j   = 0;
                        do {
                            int *c = *(int**)s;
                            *(int*)((char*)c + off) = *(int*)((char*)c + off - 8);
                            off += 4;
                            j++;
                        } while (j < (*(int *)&g_brRaceNEntrant));
                    }
                    s += 0x2b68;
                } while (s < (char*)&g_B1F248);
            }
            g_184C454 = 0;                        /* 0x1001c201 */
            {                                     /* 0x1001c207 fog/particle reset */
                char *p = (char*)&(*(int *)((char *)&g_aBrSfxChan + 0x10));
                int   i = 0;
                do {
                    *(int*)(p + 4) = 0; *(int*)p = 0;
                    *(int*)(p - 8) = 0; *(int*)(p - 4) = 0;
                    *(int*)(p - 0x10) = (int)&g_18EEF2C;
                    BrX10072580(i);
                    p += 0x18;
                    i++;
                } while (p < (char*)&g_18EF0B8);
            }
            (*(int *)((char *)&g_aBrEntRecs + 0xA8)) = 0;                         /* 0x1001c236 finalize dispatch */
            if ((*(int *)&DAT_105ccb68[12]) != 0) {
                /* 0x1001c249: `je` -- the race-end path runs when the flag is CLEAR */
                if ((*(int *)&DAT_105ccb68[9]) == 0) goto Lc372;
                if ((*(int *)&DAT_105ccb68[8]) != 0) goto Lc368;
                {                                 /* 0x1001c261 leader min-search */
                    int  n   = (*(int *)&g_brRaceNEntrant);
                    int  mx;
                    (*(int *)&DAT_105ccb68[2]) = PI(*(int**)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */, 0x34);
                    (*(int *)&g_brRaceBeginBestCar) = 0;
                    mx = (*(int *)&DAT_105ccb68[2]);
                    if (n > 1) {
                        char *b   = (char*)&(*(int *)((char *)&g_aBrRaceCar + 0x5528)) /* BR_LP64_BYTE_VIEW */;
                        int   off = 0x38;
                        int   i   = 1;
                        do {
                            int v = PI(*(int**)b, off);
                            if (v < mx) { mx = v; (*(int *)&g_brRaceBeginBestCar) = i; (*(int *)&DAT_105ccb68[2]) = v; }
                            i++; off += 4; b += 0x2b68;
                        } while (i < n);
                    }
                    if (n > 0) {                  /* 0x1001c2bd gather leader deltas */
                        char *s  = (char*)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */;
                        int   ea = (*(int *)&g_brRaceBeginBestCar) * 4 + 0x2c;
                        int   eb = (*(int *)&g_brRaceBeginBestCar) * 4 + 0x34;
                        int   k  = 0;
                        do {
                            int *d = *(int**)s;
                            /* 0x1001c2dc: `inc ecx` before the stores -- slots 1..n */
                            k++; s += 0x2b68;
                            *(int*)((char*)&g_5BC814 + k * 4) = PI(d, ea);
                            *(int*)((char*)&g_5BC88C + k * 4) = PI(d, eb);
                        } while (k < n);
                    }
                    if ((*(int *)&g_brRaceNEntrant) > 0) {           /* 0x1001c305 report + clear */
                        char *b   = (char*)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */;
                        int   off = 0x2c;
                        int   j   = 0;
                        do {
                            int *c = *(int**)b;
                            BrPodNop(&g_0A978C, 0, j, (int)((char*)c + 0x34));
                            *(int*)((char*)(*(int**)b) + off) = 0;
                            j++; off += 4;
                        } while (j < (*(int *)&g_brRaceNEntrant));
                    }
                    BrPodNop(&g_0A9768, (*(int *)&g_brRaceBeginBestCar), (*(int *)&DAT_105ccb68[2]));   /* 0x1001c351 */
                    goto Lc45c_1;
                }
            }
            goto Lc475;
        Lc368:  /* 0x1001c368 */
            goto Lc45c_1;
        Lc372:  /* 0x1001c372 */
            BrNop_1002CB3F();
            (*(int *)&g_brRaceBeginDifficulty) = 0;
            if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 5) {
                if (*(char*)((char*)*(int**)&(*(int *)((char *)&g_aBrRaceCar + 0xE8C)) /* BR_LP64_BYTE_VIEW */ + 4) != 0) {   /* 0x1001c387 */
                    (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ = 0;
                    BrSelLookup();
                    g_17A613C = 1;
                    BrGameStepSet((void*)&BrSetMode5);
                    goto Lc45c_0;
                }
                BrRaceEnterOutro();                   /* 0x1001c3af */
                goto Lc45c_0;
            }
            {                                     /* 0x1001c3b9 mode dispatch */
                int st = (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */, c = g_5bc760;
                if (st == 4 && c == 2) goto Lc44d;
                if (DAT_105ccb60 != 0 &&
                    (st == 2 || st == 1 || st == 0 || st == 6)) {
                    BrCarStateRestore();               /* 0x1001c3e8 */
                    BrGameStepSet((void*)&BrRaceSelFromMenu);
                    goto Lc45c_0;
                }
                if (st == 4) {                    /* 0x1001c3f4: both tests under st == 4 */
                    if (c == 1) goto Lc44d;
                    if (c == 0 && (*(int *)&g_brRaceBeginMovieDone) == 0) goto Lc44d;
                }
                if ((*(int *)&DAT_105ccb68[12]) != 2) goto Lc44d;
                if (!((*(int *)&g_brRace6EC760) == (*(int *)&g_brItemIconCount) && (*(int *)&g_brRace6E9A34) == (*(int *)&g_brRaceB71A6C)))   /* 0x1001c417 */
                    ((Obj*)&(*(int *)&g_BrCtrlCfg))->m_100634B0((int)&g_navArg);
                BrExt_10038F30(0);                  /* 0x1001c444 */
            }
        Lc44d:  /* 0x1001c44d */
            BrGameStepSet((void*)&BrInit220B20);
        Lc45c_0:  /* 0x1001c45c, newB88 == 0 */
            (*(int *)&DAT_105ccb68[8]) = 0; (*(int *)&DAT_105ccb68[11]) = 0;
            BrRaceDriverReset();
            BrClearFlag_AB504();
            goto Lc475;
        Lc45c_1:  /* 0x1001c45c, newB88 == 1 */
            (*(int *)&DAT_105ccb68[8]) = 1; (*(int *)&DAT_105ccb68[11]) = 0;
            BrClearFlag_AB504();
        Lc475:  /* 0x1001c475 */
            g_17A5F20 = 1;
            {
                char *s = (char*)&(*(Driver (*)[])((char *)&g_aBrRaceCar + 0x29C0)) /* BR_LP64_BYTE_VIEW */;
                do { PI(*(int**)s, 0x44) = 0; s += 0x2b68; } while (s < (char*)&g_B1F248);
            }
        }
        /* 0x1001c494: shared tail (both gate arms reach here) */
        {
            int prev = loc10;
            if ((*(int *)&g_BrX06909B4) != prev) {
                if (prev == 0) {                  /* 0x1001c4bb */
                    BrWrap_100679A0();
                    if ((*(char *)&DAT_100bb2e0) != 0) {
                        BrCdResume();
                        BrCdVolumeSet((unsigned char)(*(char *)&DAT_100bb2e0));
                    }
                } else {                          /* 0x1001c4a5 */
                    BrWrap_10067980();
                    BrCdPause();
                    BrSndBankMute();
                    BrSndTableClear();
                }
                (*(int *)&g_BrX06909B4) = prev;
            }
        }
        if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 6) FUN_10006460();        /* 0x1001c4e3 */
        {                                         /* 0x1001c4f1 HUD update */
            int flag = (((*(int  *)((char*)((*(int**)&(*(int *)((char *)&g_aBrRaceCar + 0x168)) /* BR_LP64_BYTE_VIEW */)) + ((0x1b4))))) == 0 &&
                        ((*(int  *)((char*)((*(int**)&(*(int *)((char *)&g_aBrRaceCar + 0x16C)) /* BR_LP64_BYTE_VIEW */)) + ((0x1b4))))) == 0) ? 0 : 1;
            BrFfbUpdateSpring((unsigned char)(*(char *)((char *)&g_aBrRaceCar + 0x36D)) /* BR_LP64_BYTE_VIEW */, flag, (*(int *)((char *)&g_aBrRaceCar + 0xE90)) /* BR_LP64_BYTE_VIEW */);
        }
        if ((*(int *)&DAT_100b51e4[1036]) != 0 && (*(int *)&g_BrX06909B4) == 0) FUN_1006bdd0();   /* 0x1001c52a */
        if ((*(int *)&DAT_105ccb68[8]) != 0) BrReplaySeek();        /* 0x1001c543 */
        else               BrReplayAdvance();
        FUN_10014cb0();
        BrSub10075150();
        if ((*(int *)&g_brRaceTick) == 0) {                      /* 0x1001c561 */
            if ((*(int *)&g_brRaceNet) != 0) BrNetCheckDeadline();
            BrTimeUpdate();
        }
        return;                                   /* 0x1001c57b epilogue */

    Lc583:  /* 0x1001c583 replay-advance clock (after the ret; forward-goto only) */
        g_5BCAF0 += ((int*)&g_0A95B8)[g_5BC76C];
        { int nx = g_5BC76C + 1; g_5BC76C = (nx <= 2) ? nx : 0; }
        (*(int *)&DAT_105ccb68[14])++;
        if ((*(int *)&DAT_105ccb68[14]) > 3) {                       /* 0x1001c5c7 */
            if (BrSnapInterpDraw(1) == 0) goto GhostBody;
            (*(int *)&DAT_105ccb68[14]) = 0;
            (*(int *)&DAT_105ccb68[15]) = BrSub10075020();
            goto GhostBody;
        }
        {                                         /* 0x1001c5ee time-sync loop */
            int t, esi = 1, ba4;
            t = BrSub10075020();
        Lc5f3:
            ba4 = (*(int *)&DAT_105ccb68[15]);
        Lc5f9:
            if ((unsigned)t >= (unsigned)g_5BCAF0 &&
                (unsigned)t <= (unsigned)(ba4 + 0x14d)) goto GhostBody;
            if (esi == 0) goto GhostBody;
            {
                int arg = ((unsigned)(ba4 + 0x14d) < (unsigned)t) ? 1 : 0;
                esi = BrSnapInterpDraw(arg);
                t   = BrSub10075020();
            }
            if (esi == 0) goto Lc5f3;
            /* 0x1001c637: the fresh time is both the stored and the cached
             * frame stamp (mov ecx,eax); re-testing against the stale stamp
             * never passes and draws forever */
            (*(int *)&DAT_105ccb68[14]) = 0;
            (*(int *)&DAT_105ccb68[15]) = ba4 = t;
            goto Lc5f9;
        }
}

