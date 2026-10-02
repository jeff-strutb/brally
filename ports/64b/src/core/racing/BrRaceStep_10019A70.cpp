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
#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "br_mat.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "br_racebegin.h"   /* br_globals: its objects */
#include "br_sfxsrc.h"   /* br_globals: its objects */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include "slice2_16.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
#include "br_racestep.h"   /* g_aBrRaceLightScript, g_aBrRaceBeepT */
#include "br_bits.h"   /* BrBitLatch, the control block's first word */
#include <stdint.h>
#include <string.h>
#pragma intrinsic(memcpy)   /* original inlines the copies as rep movsd, not a call */

/* The 64-bit core addresses every record by type: the cars (BrDriverCar),
 * the drivers (BrDriver), each car's control block (BrRaceCtl through
 * pCtl), the screen views (BrView), the loaded track header (g_brTrkHdr)
 * and the replay snapshot (BrSnap).  The original's thiscall member calls
 * are the C entries of the functions at those addresses. */

/* one car-slot record in the 0x100BCDD0 area (BrCarSlotLoad fills them) */
#define BR_CAR_SLOT_SIZE 0x15F88
#define BR_CAR_SLOT(k)   (&g_ab0C12A0[(k) * BR_CAR_SLOT_SIZE])

/* the track's 0x54-byte instance records (matrix first) */
static float *BrRaceInst(int i)
{
    return (float *)((char *)BR_PTR32(void *, g_brTrkHdr.aInstances) + i * 0x54);
}

/* a snapshot copy of a car's model-matrix pointer: the same matrix of the
 * copied car, or 0 when it points at none of the five the original tests */
static BrSnapMtx *BrRaceSnapMat(const BrDriverCar *src, BrDriverCar *dst, const BrSnapMtx *p)
{
    if (p == &src->aSnap[0]) return &dst->aSnap[0];
    if (p == &src->aSnap[5]) return &dst->aSnap[5];
    if (p == &src->aSnap[1]) return &dst->aSnap[1];
    if (p == &src->aSnap[2]) return &dst->aSnap[2];
    if (p == &src->aSnap[3]) return &dst->aSnap[3];
    return 0;
}

extern "C" void BrRaceStep(void)
{
    int loc10, loc14, loc18;
    int now   = BrSub10075020();
    int delta = now - DAT_105ccb68[10];
    DAT_105ccb68[10]  = now;
    int idx   = g_brRaceRules.iFpsSample;
    int count = g_brRaceRules.nFpsSamples;

    /* region 1: frame-delta ring buffer (0x10019A70-0x10019AE4) */
    if (idx < 0) {
        idx = 0;
        if (count > 0) {
            int i;
            for (i = 0; i < count; i++) g_aBrFpsSamples[i] = delta;
            idx = count;
        }
    }
    if (++(*(int *)&g_brRaceClockCount) == 0) DAT_105ccb68[5] = 0; else DAT_105ccb68[5] += delta;
    g_brRaceRules.iFpsSample = ++idx;                       /* store idx+1 unconditionally (0x1001aacf) */
    if (idx >= count) { idx = 0; g_brRaceRules.iFpsSample = idx; }   /* then conditionally re-store 0 */
    g_aBrFpsSamples[idx] = delta;

    /* region 2: first-frame init gate (0x10019AE4) */
    if (DAT_105ccb68[11] == 0) {
        (*(int *)((char *)&g_aBrEntRecs + 0x54)) = 1;
        BrPodNop(1);
        BrClearTables_1000F620();
        if (DAT_105ccb68[8] == 0) {
            BrPodNop();
            BrPodNop();
            (*(int (**)())&DAT_10b7352c)();
            g_18ED1E8();
            (*(int (**)())&PTR_FUN_100b849c)();
            BrTrackLoadHandling(g_brRaceRules.mode == 5 ? 0xc : (*(int *)&g_Br0B380C));
            BrFontTexInitAll();
            (*(int (**)())&DAT_10b73528)();
            BrFlagInit_1002B950();
        }
        BrFrameBeginRec(&g_aBrView[0]);
        BrPodNop(1);
        BrFrameBeginRec(&g_aBrView[0]);
        BrStore_1003BD40(0x7b);
        DAT_105ccb68[9] = 0;
        if (g_brRaceRules.mode != 1 && g_brRaceRules.mode != 6 && g_brRaceRules.mode != 2) g_CBE8 = 3;
        (*(int *)&g_brRaceBeginMirrorOff) = 0;
        if ((unsigned)g_brRaceRules.mode > 6) goto Ldefault;
        switch (g_brRaceRules.mode) {
        case 0:     /* 0x10019bc8 */
            (*(int *)&g_brRaceNDriver) = 0x14; (*(int *)&g_BrCarCount) = 3;
            BrEntitySetIndex(&g_aBrRaceCar[0], g_226e7c);
            goto Lcf3;
        case 1:     /* 0x10019bf1 */
            (*(int *)&g_brRaceNDriver) = 2; (*(int *)&g_BrCarCount) = 2;
            BrEntitySetIndex(&g_aBrRaceCar[0], g_226e7c);
            BrEntitySetIndex(&g_aBrRaceCar[1], g_226e7c);
            g_aBrRaceCar[1].i29B4 = 0;
            goto Lcf3;
        case 6:     /* 0x10019c2a */
            if (DAT_105ccb68[8] != 0) goto Lcfb;
            (*(int *)&g_brRaceNEntrant) = 1;
            if ((*(int *)&g_brRaceBegin226A4C) == 0) { (*(int *)&g_brRaceNDriver) = 1; (*(int *)&g_BrCarCount) = 1; }
            else               { (*(int *)&g_brRaceNDriver) = 0; (*(int *)&g_BrCarCount) = 0; }
            BrEntitySetIndex(&g_aBrRaceCar[0], g_226e7c);
            if ((*(int *)&g_brRaceNet) != 0) {
                BrNetReset();
                BrNetSetF10220DD0();
                BrNetOpenAnnounce();
                g_aBrRaceCar[0].iNetPlayer = BrGetGlobal_94294();
                strcpy(g_aBrRaceCar[0].szName, g_aBrCfgPlayerName);
                BrNetSlotSetName(g_aBrRaceCar[0].iNetPlayer, g_aBrCfgPlayerName);
            }
            while (BrDPlayGetCurrentPlayers() > (unsigned)(*(int *)&g_BrCarCount)) {
                int r = BrNetStackPop();
                if (r >= 0) BrCarTableAdd(r);
            }
            goto Lcf3;
        case 5:     /* 0x10019d87 */
            (*(int *)&g_brRaceNEntrant) = 1; (*(int *)&g_brRaceNDriver) = 1; (*(int *)&g_BrCarCount) = 1;
            DAT_105ccb68[9] = 0; DAT_105ccb68[8] = 0; g_aBrRaceCar[0].fE88 = 0;
            g_aBrRaceCar[0].pMatA = &g_aBrRaceCar[0].aSnap[1]; (*(int *)&g_BrCamHold2) = 0;
            goto La213;
        case 4:     /* 0x10019dc0 */
        {
            BrRaceCtl *pd0 = g_aBrRaceCar[0].pCtl;
            DAT_104b15e8 = 1; (*(int *)&g_brRaceNEntrant) = 1; (*(int *)&g_brRaceNDriver) = 1; (*(int *)&g_BrCarCount) = 1;
            DAT_105ccb68[9] = 0; g_aBrRaceCar[0].fE88 = 0;
            pd0->pHdr = g_aBrRaceBeginRec;
            {
                int which = g_5bc760;                 /* 0x10019df1 */
                if (which == 0) goto Lintro;
                if (which == 1) goto Lcredits;
                if (which == 2) goto Loutro;
                goto Le63;
            Loutro:                                   /* 0x10019e00 */
                BrRaceCueLayout();
                BrGhostLoad((char *)"RallyOutro.dat", 0);
                goto Le58;
            Lcredits:                                 /* 0x10019e15 */
                BrGhostLoad((char *)"RallyCredits.dat", 0);
                goto Le58;
            Lintro:                                   /* 0x10019e25 */
                BrGhostLoad((char *)(DAT_105ccb68[13] ? "RallyIntro2.dat" : "RallyIntro1.dat"), 0);
                { int nv = DAT_105ccb68[13] + 1;
                  if (nv <= 1) DAT_105ccb68[13] = nv; else DAT_105ccb68[13] = 0; }
            Le58:                                     /* 0x10019e58 */
                BrReplayRewind();
                (*(int *)&g_brRaceBeginMovieDone) = 0;
            }
        Le63:                                         /* 0x10019e63 */
            {
                BrRaceCtl     *pObj = g_aBrRaceCar[0].pCtl;
                const uint8_t *pInfo;
                pObj->apRec[0] = 0;
                pObj->apRec[1] = 0;
                g_aBrRaceCar[0].f29AC = 0xff; g_aBrRaceCar[0].f29AD = 0xff; g_aBrRaceCar[0].f29AE = 0xff;
                pInfo = pObj->pHdr;
                (*(int *)&g_Br0B380C) = (signed char)pInfo[0];
                g_aBrRaceCar[0].f29A4 = (signed char)pInfo[1];
                BrEntitySetIndex(&g_aBrRaceCar[0], (signed char)pInfo[1]);
                g_aBrRaceCar[0].fE98 = (signed char)pInfo[2];
                g_aBrRaceCar[0].fE9C = (signed char)pInfo[3];
                g_aBrRaceCar[0].fE90 = (signed char)pInfo[4];
                g_aBrRaceCar[0].fE94 = (signed char)pInfo[5];
                pObj->b25 = pInfo[6];
                g_226e80 = (signed char)pInfo[7];
                DAT_104b15e8 = (signed char)pInfo[7];
            }
            goto La213;
        }
        case 2:                 /* 0x10019f0e */
        {
            int nE;
            BrEntitySetIndex(&g_aBrRaceCar[0], g_226e7c);
            DAT_105ccb68[9] = 0;
            (*(int *)&g_brRaceBeginRecArmed) = 1;
            (*(int *)&g_brRaceNDriver) = (*(int *)&g_brRaceNEntrant) + 1;
            (*(int *)&g_BrCarCount) = (*(int *)&g_brRaceNEntrant) + 1;
            nE = (*(int *)&g_brRaceNEntrant);
            g_aBrRaceCar[nE].pCtl->pHdr = g_aBrRaceBeginRecIn;
            BrPodNop(&g_0A9884, (*(int *)&g_brRace5BC8D8));
            memcpy(g_aBrRaceCar[nE].pCtl->pHdr, g_aBrRaceBeginRec, (*(int *)&g_brRace5BC8D8));
            {
                const uint8_t *inf = g_aBrRaceCar[nE].pCtl->pHdr;
                int v1 = (signed char)inf[1];
                g_aBrRaceCar[nE].f29A4 = v1;
                BrEntitySetIndex(&g_aBrRaceCar[nE], v1);
            }
            BrReplayRewind();
            {
                nE = (*(int *)&g_brRaceNEntrant);
                const uint8_t *inf = g_aBrRaceCar[nE].pCtl->pHdr;
                if ((signed char)inf[0] == (*(int *)&g_Br0B380C) &&
                    (signed char)inf[7] == g_226e80) {          /* 0x1001a02b */
                    BrPodNop(&g_0A9878, (signed char)inf[0]);
                    g_aBrRaceCar[nE].pCtl->f48 = 8;
                    BrPodNop(&g_0A9868, (*(int *)&g_brRace5BC8D8));
                    g_aBrRaceCar[nE].pCtl->f4C = (*(int *)&g_brRace5BC8D8);
                } else {                                        /* 0x1001a09c */
                    BrPodNop(&g_0A9850, (signed char)inf[0], (*(int *)&g_Br0B380C));
                    g_aBrRaceCar[nE].pCtl->pHdr = 0;
                    (*(int *)&g_BrCarCount) = 1; (*(int *)&g_brRaceNDriver) = 1;
                }
            }
            /* 0x1001a0d9 .. 0x1001a1ec: each entrant's ghost-record header,
             * in its own 0xF000-byte buffer at 0x11787850 */
            if ((*(int *)&g_brRaceNEntrant) > 0) {
                int i = 0;
                do {
                    BrDriverCar *pc = &g_aBrRaceCar[i];
                    uint8_t     *q;
                    pc->pCtl->apRec[i] = (uint8_t *)g_1787850 + i * 0xf000;
                    q = (uint8_t *)pc->pCtl->apRec[i];
                    q[0] = (uint8_t)(*(int *)&g_Br0B380C);
                    q[1] = *(uint8_t *)&pc->f29A8;
                    q[2] = pc->pEquip[0xf8];
                    q[3] = pc->pEquip[0xfc];
                    q[4] = pc->pEquip[0x100];
                    q[5] = pc->pEquip[0x104];
                    q[6] = pc->pEquip[0x108];
                    q[7] = (uint8_t)g_226e80;
                    pc->pCtl->aLen[i] = 8;
                    pc->pCtl->aCap[i] = 0xde5c;
                    i++;
                } while (i < (*(int *)&g_brRaceNEntrant));
            }
            goto La213;
        }
        default: Ldefault:      /* 0x1001a1ee (== case 3) */
        case 3:
            BrEntitySetIndex(&g_aBrRaceCar[0], g_226e7c);
            DAT_105ccb68[9] = 0;
            (*(int *)&g_brRaceNDriver) = (*(int *)&g_brRaceNEntrant);
            (*(int *)&g_BrCarCount) = (*(int *)&g_brRaceNEntrant);
            goto La213;
        }

    Lcf3:   /* 0x10019cf3 */
        if (DAT_105ccb68[8] == 0) goto Ld39;
    Lcfb:   /* 0x10019cfb */
        {
            int a = (*(int *)&g_brRaceBeginBestCar);
            int c;
            /* 0x102066C8 is car slot 15 */
            BrWrap_10067960(BR_CAR_SLOT(15 - a));
            for (c = 0; c < 15; c++) {           /* 0x10019d23 zero loop */
                g_aBrSfxChan[c].packed = 0;
                g_aBrSfxChan[c].f10 = 0;
            }
        }
        goto Ld70;
    Ld39:   /* 0x10019d39 */
        {
            int i;
            for (i = 0; i < (*(int *)&g_brRaceNEntrant); i++) {
                BrRaceCtl *p = g_aBrRaceCar[i].pCtl;
                p->pHdr = 0; p->apRec[0] = 0; p->apRec[1] = 0;
            }
        }
    Ld70:   /* 0x10019d70 */
        DAT_105ccb68[9] = (DAT_105ccb68[8] == 0);
        goto La213;

    La213:  /* 0x1001a213 -- common tail after the state switch */
        if (g_brRaceRules.mode == 5) {
            (*(int *)&g_brRaceBeginDifficulty) = 0;
        } else {
            (*(int *)&g_brRaceBeginDifficulty) =
                ((*(const int32_t *)((const char *)g_apBrRaceDiff[(*(int *)&g_Br0B380C)] + 4)) >> 4) & 1;
        }
        BrRaceDifficultySet(DAT_104b15e8);
        if (g_brRaceRules.mode != 4 && DAT_105ccb68[8] == 0) {
            BrPodNop();
            BrPodNop();
        }
        FUN_10061310();
        if (DAT_105ccb68[8] != 0) goto La430;
        BrTrackLoad(g_brRaceRules.mode == 5 ? 0xc : (*(int *)&g_Br0B380C));
        if (DAT_105ccb68[8] != 0) goto La430;
        {
            int cnt;
            (*(int *)&g_brRaceBeginAirArmed) = 0; (*(int *)&g_brRaceBeginAirTrigger) = 0; (*(int *)&g_brRaceBeginAirplane) = 0; g_pBrRaceFlyAim = 0;
            g_pBrRaceFlyPos = 0; g_brRaceBeginPathT = 0; g_brRaceBeginPathSeg = 0; (*(int *)&g_brRaceBeginPathIdx) = 0; (*(int *)&g_brRaceBeginPathLen) = 0;
            BrPodNop(&g_0A9840, g_brTrkHdr.nSpecial);
            (*(int *)&g_brRaceBeginFxCount) = 0;
            cnt = g_brTrkHdr.nSpecial;
            if (cnt > 0) {
                const BrRaceSpecial *e = g_brTrkHdr.aSpecial;
                int i = 0;
                do {
                    int          arg = 0;
                    const char  *str = 0;
                    switch ((signed char)e->axis - 3) {   /* jmp [.. 0x1001c664] */
                    case 0:  arg = e->f00; (*(int *)&g_brRaceBeginAirplane) = arg; str = (char*)&g_0A97F8; break;
                    /* kinds 4 and 5 hold the addresses BrGlTrackFixupCmds rebased */
                    case 1:  (*(int *)&g_brRaceBeginPathLen) = e->f04; arg = e->f00;
                             g_pBrRaceFlyPos = BR_PTR32(BrVec3 *, e->f00);
                             str = (char*)&g_0A9824; break;
                    case 2:  arg = e->f00; g_pBrRaceFlyAim = BR_PTR32(BrVec3 *, e->f00); str = (char*)&g_0A9808; break;
                    case 3:  arg = e->f00; (*(int *)&g_brRaceBeginAirTrigger) = arg; str = (char*)&g_0A97E0; break;
                    case 4:  { int c = (*(int *)&g_brRaceBeginFxCount); arg = e->f00;
                               g_aBrRaceBeginFx[c] = arg;
                               (*(int *)&g_brRaceBeginFxCount) = c + 1; str = (char*)&g_0A97D0; } break;
                    default: goto Lskip;
                    }
                    BrPodNop(str, arg);
                Lskip:
                    i++;
                    e++;
                } while (i < g_brTrkHdr.nSpecial);
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
                    m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f;
                    /* in place: both leas are [esp+0x2c] */
                    BrVec3TransformDivW(&m[0], &m[0], (const float (*)[4])BrRaceInst(sel));
                    g_brRaceBeginPathScale = BrVec3Length((const struct BrVec3 *)(&m[0]));
                }
            }
            (*(int *)&g_brRaceBeginCamMinus1) = g_CBE8 - 1;
        }
    La430:  /* 0x1001a430 */
        g_brMode0AA8B4 = 1;
        if (DAT_105ccb68[8] == 0) g_brMode0AA8B4 = (*(int *)&g_brRaceNEntrant);
        g_aBrView[0].iCar = DAT_105ccb68[8] ? (*(int *)&g_brRaceBeginBestCar) : 0;
        g_aBrView[1].iCar = (g_aBrView[0].iCar == 0);

        /* 0x1001a462: wheel/tyre + camera-mode setup */
        if (g_brRaceRules.mode == 4) {
            g_aBrRaceCar[0].pMatA = &g_aBrRaceCar[0].aSnap[1];
            (*(int *)&g_BrCamHold2) = 0xb4;
            goto La58d;
        }
        if ((*(int *)&g_brRaceNEntrant) > 0) {                     /* 0x1001a485 tyre loop */
            int i = 0;
            do {
                BrDriverCar *pc = &g_aBrRaceCar[i];
                pc->fE9C = *(const int32_t *)(pc->pEquip + 0xfc);
                pc->fE94 = *(const int32_t *)(pc->pEquip + 0x104);
                pc->fE90 = *(const int32_t *)(pc->pEquip + 0x100);
                pc->fE98 = *(const int32_t *)(pc->pEquip + 0xf8);
                if ((*(int *)&g_BrCtrlCfg.active) == 1 || (*(int *)&g_BrCtrlCfg.active) == 2 || (*(int *)&g_BrCtrlCfg.active) == 3)
                    pc->pCtl->b25 = 5;
                else
                    pc->pCtl->b25 = 2;
                i++;
            } while (i < (*(int *)&g_brRaceNEntrant));
        }
        {
            int ecx = (*(int *)&g_brRaceNEntrant);                 /* 0x1001a4f1 */
            if (g_brRaceRules.mode == 2) {
                BrRaceCtl *p = g_aBrRaceCar[1].pCtl;
                if (p->pHdr != 0) {
                    const uint8_t *inf = p->pHdr;
                    g_aBrRaceCar[1].fE98 = (signed char)inf[2];
                    g_aBrRaceCar[1].fE9C = (signed char)inf[3];
                    g_aBrRaceCar[1].fE90 = (signed char)inf[4];
                    g_aBrRaceCar[1].fE94 = (signed char)inf[5];
                    p->b25 = inf[6];
                    goto La58d;
                }
            }
            /* 0x1001a547 */
            if (ecx < (*(int *)&g_BrCarCount)) {
                do {
                    BrDriverCar *pc = &g_aBrRaceCar[ecx];
                    pc->fE9C = 1;
                    pc->fE94 = 1;
                    pc->fE90 = 2;
                    pc->fE98 = 0;
                    ecx++;
                    pc->pCtl->b25 = 0;
                } while (ecx < (*(int *)&g_BrCarCount));
            }
        }
    La58d:  /* 0x1001a58d */
        if (DAT_105ccb68[8] != 0) goto La973;
        DAT_105ccb60 = 0;
        if ((*(int *)&g_BrCarCount) > 0) {                     /* 0x1001a5ba callback-wiring loop */
            int i = 0;
            do {
                BrDriverCar *pc = &g_aBrRaceCar[i];
                int n  = (*(int *)&g_brRaceNEntrant);
                int st = g_brRaceRules.mode;
                pc->f29B0 = 1.0f;
                if (i < n) {
                    pc->pfnControl = BrCtlHuman;
                } else if (st == 2) {
                    pc->pfnControl = BrCtlHuman;
                    pc->b29AF = (uint8_t)st;
                    pc->f29B0 = 0.375f;                    /* 0x3ec00000 */
                    goto Lwire_after;
                } else if (st == 6) {
                    pc->pfnControl = BrRaceCarCtlOutro;
                } else {
                    BrRaceCarPickIndex(pc);
                    pc->pfnControl = BrCtlAi;
                }
                pc->b29AF = 0;                             /* 0x1001a612 */
            Lwire_after:                                   /* 0x1001a619 */
                if (pc->fE88 == 0)
                    BrCarSlotSetup_1006FCE0(pc, i, pc->f29A8);
                BrCarStartInit_1005E7B0(pc);
                pc->fF7C = 0;
                pc->pszBanner = 0;
                pc->psz1004 = 0;
                i++;
            } while (i < (*(int *)&g_BrCarCount));
        }
        if ((*(int *)&g_BrCarCount) == 0) {                    /* 0x1001a65f */
            memset(BR_CAR_SLOT(0), 0, BR_CAR_SLOT_SIZE);
            BrCarSlotBind_1006FCB0(&g_aBrRaceCar[0], 0);
            BrCarStartInit_1005E7B0(&g_aBrRaceCar[0]);
            BrCamChaseStep((struct BrCamCar *)&g_aBrRaceCar[0]);
            g_aBrRaceCar[0].aSnap[0].m[3][2] = g_aBrRaceCar[0].aSnap[0].m[3][2] - g_0773B0;
        }
        loc14 = 0;                              /* 0x1001a6a0 */
        if (g_brMode0AA8B4 > 0) {
            do {                                /* 0x1001a6c5 render loop, one pass per view */
                BrView  *pv = &g_aBrView[loc14];
                uint8_t *s  = BR_CAR_SLOT(pv->iCar);
                uint8_t *b  = (DAT_104b15e8 == 4) ? s + 0x300 : s + 0x100;
                int      a  = (signed char)s[0xe4];
                int      d  = (signed char)s[0xe5];
                char     kind;
                pv->ahTex[0] = (int32_t)g_pfn18ED1C4((uintptr_t)(s + 0x500), (uintptr_t)b, a, d, a,
                                                     1, 2, 0, 0, 1, 1, 0, 0, 0, 0);
                kind = (char)s[0xdb];
                if (kind == 1 || kind == 2) {          /* 0x1001a743 */
                    uint8_t *w = b + 0x1fe;
                    int      k;
                    g_18ED1B4 = 1;
                    for (k = 1; k < 16; k++) {         /* 0x1001a75e, 15 passes */
                        int a2, d2;
                        char k2 = (char)s[0xdb];
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
                        a2 = (signed char)s[0xe4];
                        d2 = (signed char)s[0xe5];
                        pv->ahTex[k] = (int32_t)g_pfn18ED1C4((uintptr_t)(s + 0x500), (uintptr_t)b, a2, d2, a2,
                                                             1, 2, 0, 0, 1, 1, 0, 0, 0, 0);
                        w -= 2;
                    }
                    g_18ED1B4 = 0;
                }
                {                                      /* 0x1001a7f0 */
                    int a3 = (signed char)s[0xe8];
                    int c3 = (signed char)s[0xe9];
                    int d3 = (signed char)s[0xe4];
                    int e3 = (signed char)s[0xe5];
                    uint8_t *dst;
                    int      sz;
                    pv->hTexB = (int32_t)g_pfn18ED1C4((uintptr_t)(s + d3 * e3 + 0x500), (uintptr_t)b,
                                                      a3, c3, a3, 1, 2, 0, 0, 1, 1,
                                                      0, 0, 1, 0);
                    /* 0x1001a847: memmove of the emitted span */
                    sz  = ((signed char)s[0xd8] + 2) * (signed char)s[0xe8] * (signed char)s[0xe9];
                    dst = s - sz + 0x8000;
                    memmove(dst, s + (signed char)s[0xe4] * (signed char)s[0xe5] + 0x500, sz);
                    loc18 = 0;
                    if ((signed char)s[0xd8] + 2 > 0) {
                        uint8_t *ep = s + 0x500;
                        do {                           /* 0x1001a8ab */
                            int aa = (signed char)s[0xe8];
                            int cc = (signed char)s[0xe9];
                            int r  = g_18ED1C0((unsigned short *)ep, dst, b, aa, cc, aa, 1, 2);
                            ep += r;
                            *(int32_t *)(s + 0xfc) = r;
                            if (ep > dst)
                                BrLogPrint(BrStrGet(0x12c));
                            dst += (signed char)s[0xe8] * (signed char)s[0xe9];
                            if ((unsigned)(ep - s) > 0x8000)
                                BrLogPrint(BrStrGet(0x12d));
                            loc18++;
                        } while (loc18 < (signed char)s[0xd8] + 2);
                    }
                }
                /* 0x1001a93e */
                DAT_100a6b68[loc14] = (int32_t)0xffffffff;
                loc14++;
            } while (loc14 < g_brMode0AA8B4);
        }
        goto La97c;
    La973:  /* 0x1001a973 */
        loc14 = (*(int *)&g_brRaceNEntrant);
    La97c:  /* 0x1001a97c */
        {
            int sel = (DAT_105ccb68[8] != 0) ? 4 : ((g_brRaceRules.mode == 5) ? 4 : 0);
            g_brRaceLightT = g_aBrRaceLightScript[sel].dur;
            (*(int *)&g_brRaceLights) = g_aBrRaceLightScript[sel].state;
            (*(int *)&g_brRaceScript) = sel;
        }
        if (DAT_105ccb68[8] == 0) {
            (*(int *)&g_brRaceNFinished) = 0;
            if ((*(int *)&g_brRaceNDriver) > 0) {                 /* 0x1001a9d3 */
                int i = 0;
                do { BrRaceGridPlace((unsigned char *)&g_aBrRaceDriver[i]); i++; } while (i < (*(int *)&g_brRaceNDriver));
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
        DAT_105ccb68[11] = 1;                           /* 0x1001aa5e */
        if (DAT_105ccb68[8] == 0) {                    /* 0x1001aa66 */
            DAT_105bcaec = (void *)&g_5BCAF8;
            BrModelLoad(&g_5BCAF8, "misc\\modelLights.blob");   /* a third argument the original never reads */
            BrAnimSetOnce((struct BrAnimSet *)DAT_105bcaec);
        }
        BrPfxReset();                         /* 0x1001aa96 */
        BrCollRespReset();
        (*(int *)&g_BrX06909B4) = 0;
        DAT_105ccb68[12] = 0;
        (*(int *)&g_brRaceBeginSeq888) = 0xffffffff;
        g_brRaceBeginSeqT = 0;
        (*(int *)&g_brRaceBeginSeqIdx) = 0;
        (*(int *)((char *)&g_aBrEntRecs + 0xA8)) = 1;
        if (DAT_105ccb68[8] == 0) { BrReset_1002E13B(); BrSndNearestReset(); }
        (*(int *)((char *)&g_aBrEntRecs + 0x54)) = 0;                           /* 0x1001aadf */
        if (DAT_105ccb68[8] != 0) {
            BrReplayRewind();
        } else {
            BrReplayReset();                     /* 0x1001aaf5 */
            if (g_brRaceRules.mode == 4) BrSet_1006AA90();
            if ((*(char *)&DAT_100bb2e0) != 0) {                /* 0x1001ab0e */
                int c = g_5bc760;
                int arg;
                if (g_brRaceRules.mode == 4 && c == 2)      arg = 0xc;
                else if (g_brRaceRules.mode == 4 && c == 1) arg = 0xd;
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
            if (r >= 0 && DAT_105ccb68[8] == 0) BrCarTableRemove(r);
        }
    }

    /* 0x1001ab93: per-frame tail (merge point) */
    g_17A5F20 = 0;
    if ((*(int *)&g_brRaceLights) == 4 && (*(int *)&g_brRaceNEntrant) > 0) {
        int i = 0;
        do { BrRaceSaveLastLapInfo(&g_aBrRaceCar[i]); i++; } while (i < (*(int *)&g_brRaceNEntrant));
    }
    if (g_brRaceRules.mode != 4 && (*(int *)&g_BrX06909B4) == 0)      /* 0x1001abd0 */
        (*(int *)&g_brRaceBeginMirrorOff) ^= 1;
    BrFrameClockStep();
    (*(int *)&g_brRaceHudA) = 0;
    loc10 = (*(int *)&g_BrX06909B4);
    DAT_105ccb68[4] = 0;
    if ((*(int *)&g_brRaceLights) < 3) {                       /* 0x1001ac00 */
        if ((*(int *)&g_brRaceTick) != 0) {                  /* 0x1001ac0f: je ac42 skips when ==0 */
            int i;
            for (i = 0; i < (*(int *)&g_brRaceNEntrant); i++) {
                g_aBrRaceCar[i].psz1004 = 0; g_aBrRaceCar[i].f1008 = 0.01f;   /* 0x3c23d70a */
            }
        } else if ((*(int *)&g_brRaceNet) == 0 || (*(int *)&g_brRace18EEED8) == 0) {   /* 0x1001ac8c */
            int i;
            for (i = 0; i < (*(int *)&g_brRaceNEntrant); i++) {
                g_aBrRaceCar[i].psz1004 = BrStrGet(0xee); g_aBrRaceCar[i].f1008 = 0.01f;
            }
        } else {                              /* 0x1001ac52 */
            int i;
            for (i = 0; i < (*(int *)&g_brRaceNEntrant); i++) {
                g_aBrRaceCar[i].psz1004 = BrStrGet(0xed); g_aBrRaceCar[i].f1008 = 0.01f;
            }
        }
        DAT_105ccb68[4] = 1;                         /* 0x1001acc4 */
        if ((*(int *)&g_brRaceNDriver) > 0) {
            int i = 0;
            do {
                BrDriver    *pd = &g_aBrRaceDriver[i];
                BrDriverCar *c;
                pd->f68 |= 1;
                c = pd->pCar;
                if (c != 0 && c->f140 < (*(int *)&g_brRaceNEntrant)) {
                    if (g_brRaceRules.mode == 1 || g_brRaceRules.mode == 6) {   /* 0x1001ad17 */
                        short dx = (short)(DAT_104b15e8 - 1);
                        int   e;
                        if (dx > 2 || dx < 0) dx = 0;
                        e = c->f0E64 * 3 + dx;
                        *(int32_t *)&c->fFF0 = *(const int32_t *)((const char *)g_apBrRaceDiff[(*(int *)&g_Br0B380C)] + (e * 7) * 4 + 0x44);
                    }
                    c->f1000 = 1.0f;                       /* 0x1001ad56 */
                    if ((*(int *)&g_brRaceLights) == 0) {            /* 0x1001adac */
                        (*(int *)&g_brRaceHudA) = -1;                /* ebx = -1 (0x10019ae9 `or ebx,-1`), NOT 1 */
                        DAT_105ccb68[3] = 0;
                    } else if ((*(int *)&g_brRaceLights) == 2) {     /* 0x1001ad6e */
                        int k = DAT_105ccb68[3];
                        (*(int *)&g_brRaceHudA) = 1;
                        if (g_aBrRaceBeepT[k] > g_brRaceLightT) {
                            k++;
                            DAT_105ccb68[3] = k;
                            if (k == 4) BrSfxSrcBeep2();
                            else        BrSfxSrcBeep();
                        }
                    }
                }
                i++;
            } while (i < (*(int *)&g_brRaceNDriver));
        }
        g_brRaceFade = 0.0f;                       /* 0x1001adcc */
        goto Lb0cd;
    }
    else {                                    /* 0x1001addb: lights >= 3 */
        if ((*(int *)&g_brRaceLights) == 3) {                  /* 0x1001addd */
            (*(int *)&g_brRaceHudA) = 1; DAT_105ccb68[4] = 1;
            {
                int i;
                for (i = 0; i < (*(int *)&g_brRaceNDriver); i++) g_aBrRaceDriver[i].f68 &= 0xfffffffe;
            }
            { int k = (*(int *)&g_brRaceScript);
              float t = g_aBrRaceLightScript[k].dur;
              float a = (t - g_brRaceLightT) / t;
              g_brRaceFade = a * a * g_0773B4; }
            goto Lb0cd;
        } else if ((*(int *)&g_brRaceLights) == 4) {           /* 0x1001ae38 */
            loc14 = 1;
            if (g_brRaceRules.mode == 4) {              /* 0x1001ae54 */
                int c = g_5bc760;
                if (c == 2) {
                    if (g_aBrRaceCar[0].tFinal > g_0773B8) goto Lae_a1;
                    goto Lae89;
                }
                if (g_aBrRaceCar[0].tFinal > g_0773BC) goto Lae_a1;
            }
        Lae89:  /* 0x1001ae89 */
            /* 0x1001ae8c `jne 0x1001aee2`: every mode but 5 skips the check
             * below entirely -- it is NOT a fall-through into Lae_a1.  Mode 5
             * reaches it only on an ordered greater-than (`test ah,0x41 /
             * jne` at 0x1001ae9f: less, equal and unordered all skip). */
            if (g_brRaceRules.mode != 5) goto Laee2;
            if (!(g_aBrRaceCar[0].tFinal > g_0773C0)) goto Laee2;
        Lae_a1: /* 0x1001aea1 */
            if (BrFadeIsClosing() == 0) {
                BrFadeSetTarget(0, 0x3e4ccccd);
                BrFadeSetTargetA(0, 0x3e4ccccd);
                DAT_105ccb68[12] = 1; DAT_105ccb68[8] = 0; DAT_105ccb68[9] = 0;
                loc14 = 0;
            }
        Laee2:  /* 0x1001aee2 */
            if ((*(int *)&g_brRaceNEntrant) > 0) {
                int i = 0;
                do {
                    if ((g_aBrRaceDriver[i].f68 & 2) != 0) {
                        int handled = 0;
                        if (g_brRaceRules.mode == 2) {              /* 0x1001af07 */
                            BrRaceCtl *pd = g_aBrRaceCar[i].pCtl;
                            if (pd->apRec[i] != 0) {
                                if (i == 0) {             /* 0x1001af38 */
                                    int  e8 = (*(int *)&g_brRace5BC8D8);
                                    int  clx = (signed char)g_aBrRaceBeginRec[0];
                                    if (!(g_aBrRaceCar[0].fFF8 != 0 && clx == (*(int *)&g_Br0B380C) && e8 > 8)) {
                                        BrRaceCtl     *p0  = g_aBrRaceCar[0].pCtl;
                                        int            s34 = p0->aLen[0];
                                        const int32_t *q;
                                        BrPodNop(&g_0A97A8, 0, s34, e8, (s34 < e8),
                                                 clx, (*(int *)&g_Br0B380C), (clx != (*(int *)&g_Br0B380C)),
                                                 e8, 8, (e8 <= 8));
                                        (*(int *)&g_brRace5BC8D8) = 0x10;
                                        /* 0x1001afad: q IS the record car 0's control block
                                         * points at (mov eax,[edx+0x2c]; mov ecx,[eax]) */
                                        q = (const int32_t *)p0->apRec[0];
                                        *(int32_t *)&g_aBrRaceBeginRec[0] = q[0];
                                        *(int32_t *)&g_aBrRaceBeginRec[4] = q[1];
                                        *(int32_t *)&g_aBrRaceBeginRec[8] = 0;
                                        *(int32_t *)&g_aBrRaceBeginRec[12] = 0;
                                        g_aBrRaceCar[0].pszBanner = BrStrGet(0xef);
                                        g_aBrRaceCar[0].f1000 = 1.0f;
                                        BrTimeFormat(g_aBrRaceCar[0].sz100C, g_aBrRaceCar[0].tFinal);
                                        g_aBrRaceCar[0].psz1004 = g_aBrRaceCar[0].sz100C;
                                        g_aBrRaceCar[0].f1008 = 1.0f;
                                        (*(int *)&g_brRaceBeginRecArmed) = 1;
                                    }
                                }
                                g_aBrRaceCar[i].pCtl->apRec[i] = 0;   /* 0x1001b01b */
                                handled = 1;
                            }
                        }
                        if (!handled &&                    /* 0x1001b03a */
                            (g_brRaceRules.mode == 1 || g_brRaceRules.mode == 6) && (*(int *)&g_brRaceNEntrant) > 0) {
                            BrRaceCtl *pd = g_aBrRaceCar[i].pCtl;
                            int        j = 0;
                            do {
                                pd->aCap[j] = pd->aLen[j];
                                j++;
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
            int st = g_brRaceRules.mode;
            if (st == 2 || st == 1 || st == 0 || st == 6)
                if (DAT_105ccb68[8] == 0) BrCarStateSave();
            goto Lb0f8;
        } else if ((*(int *)&g_brRaceLights) == 6) {           /* 0x1001b0c8 */
            goto Lb0cd;
        } else if ((*(int *)&g_brRaceLights) == 7) {           /* 0x1001b127 */
            if (DAT_105ccb68[12] == 0) {
                BrFadeSetTargetA(0, 0x3e4ccccd);
                if (DAT_105ccb68[9] == 0) BrFadeSetTargetB(0, 0x3e4ccccd);
                BrFadeSetTarget(0, 0x3e4ccccd);
                DAT_105ccb68[12] = 1;
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
        g_brRaceLightT = g_aBrRaceLightScript[k].dur;
        (*(int *)&g_brRaceLights) = g_aBrRaceLightScript[k].state;
    }
Lb171:  /* 0x1001b171 merge */
    BrPodNop((struct BrMat4 *)0, 0, 0, 0xc8, 0xff);
    BrZeroRegions();
    {
        int i;
        for (i = 0; i < (*(int *)&g_BrCarCount); i++) BrRaceCarPre(&g_aBrRaceCar[i]);
        for (i = 0; i < (*(int *)&g_brRaceNDriver); i++) BrGhostPlaybackStep(&g_aBrRaceDriver[i]);
        if (!((*(int *)&g_brRaceNet) == 0 && DAT_105ccb68[8] == 0)) {      /* 0x1001b1d9 */
            for (i = 0; i < (*(int *)&g_brRaceNDriver); i++) BrRaceDriverAnim(&g_aBrRaceDriver[i]);
        }
        for (i = 0; i < (*(int *)&g_brRaceNDriver); i++) BrRaceDriverPost(&g_aBrRaceDriver[i]);   /* 0x1001b20b */
        if (g_brRaceRules.mode == 0) {                                                       /* 0x1001b22d */
            for (i = 0; i < (*(int *)&g_brRaceNEntrant); i++) BrLapSaveRestore(&g_aBrRaceCar[i]);
        }
    }
    BrRankAssign();                               /* 0x1001b25c */
    if ((*(int *)&g_BrX06909B4) != 0) goto Lb887;
    if (DAT_105ccb68[8] == 2) goto Lb887;
    BrPodNop(0, 0x80, 0x80, 0, 0xff);
    BrPfxTick();
    BrPodNop(0, 0, 0xff, 0xff, 0xff);
    BrWeatherStepParticles();
    if ((*(int *)&g_brRaceBeginAirplane) != 0 && (*(int *)&g_brRaceBeginAirArmed) == 0) {         /* 0x1001b2b6 leaderboard scan */
        if ((*(int *)((char *)&g_aBrEntRecs + 0x84)) != 0 || (*(int *)((char *)&g_aBrEntRecs + 0x80)) != 0 || (*(int *)((char *)&g_aBrEntRecs + 0x7C)) != 0) {
            if ((*(int *)&g_Br0B380C) == 2 || (*(int *)&g_Br0B380C) == 8) goto Lb365;
        }
        if ((*(int *)&g_brRaceNEntrant) > 0) {
            int k = 0;
            do {
                const BrDriverCar *pc = &g_aBrRaceCar[k];
                int c = pc->gotHit;
                int a;
                for (a = 0; a < c; a++) {
                    if (pc->lap == (*(int *)&g_brRaceBeginCamMinus1)) {
                        if ((int)pc->aNearIds[a] == (*(int *)&g_brRaceBeginAirTrigger)) { (*(int *)&g_brRaceBeginAirArmed) = 1; break; }
                    }
                }
                k++;
            } while (k < (*(int *)&g_brRaceNEntrant));
        }
    }
Lb365:  /* 0x1001b365 */
    if (g_brTrkHdr.nSpecial > 0) {
        const BrRaceSpecial *e = g_brTrkHdr.aSpecial;
        int b = 0;
        do {
            int sw = (signed char)e->axis;
            if ((unsigned)sw <= 3) {                /* jmp [.. 0x1001c678] */
                switch (sw) {
                case 0:  /* 0x1001b393 */
                    BrMat4RotateAxis(&g_brRaceSpecialM, e->angle, 0.0f, 0.0f, 1.0f);
                    goto Lb_emit;
                case 1:  /* 0x1001b3a0 */
                    BrMat4RotateAxis(&g_brRaceSpecialM, e->angle, 1.0f, 0.0f, 0.0f);
                    goto Lb_emit;
                case 2:  /* 0x1001b3ad */
                    BrMat4RotateAxis(&g_brRaceSpecialM, e->angle, 0.0f, 1.0f, 0.0f);
                Lb_emit: /* 0x1001b3b8 */
                    {
                        float *m = BrRaceInst(e->iObj);
                        BrMat4Mul(&g_brRaceSpecialM, (const struct BrMat4 *)m, (struct BrMat4 *)m);
                        m = BrRaceInst(e->iObj);
                        *(unsigned short*)((char*)m + 0x4c) &= 0xdfff;
                    }
                    break;
                case 3:  /* 0x1001b403 */
                    if ((*(int *)&g_brRaceBeginAirplane) == 0 || (*(int *)&g_brRaceBeginAirArmed) == 0) break;
                    {
                        BrVec3 v20, v2c, v38;
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
                                BrVec3Midpoint(&v2c, &g_pBrRaceFlyPos[wi - 1], &g_pBrRaceFlyAim[wi - 1]);
                                BrVec3Midpoint(&v20, &g_pBrRaceFlyPos[wi], &g_pBrRaceFlyAim[wi]);
                                g_brRaceBeginPathSeg = BrVec3Dist(&v2c, &v20);
                                BrVec3Sub(&g_brRaceFlyDirCur, &v20, &v2c);
                                br_dl_normalise(&g_brRaceFlyDirCur);
                                if ((*(int *)&g_brRaceBeginPathIdx) < 2) {         /* 0x1001b525 */
                                    g_brRaceFlyDirPrev = g_brRaceFlyDirCur;
                                } else {                    /* 0x1001b549 */
                                    int p = (*(int *)&g_brRaceBeginPathIdx) - 2;
                                    BrVec3Midpoint(&v38, &g_pBrRaceFlyPos[p], &g_pBrRaceFlyAim[p]);
                                    BrVec3Sub(&g_brRaceFlyDirPrev, &v2c, &v38);
                                    br_dl_normalise(&g_brRaceFlyDirPrev);
                                }
                                if ((*(int *)&g_brRaceBeginPathIdx) + 1 == (*(int *)&g_brRaceBeginPathLen)) {   /* 0x1001b5a8 */
                                    g_brRaceFlyDirNext = g_brRaceFlyDirCur;
                                } else {                    /* 0x1001b5ca: the NEXT point, wi+1 (eax from 0x1001b5a1) */
                                    int n = (*(int *)&g_brRaceBeginPathIdx) + 1;
                                    BrVec3Midpoint(&v38, &g_pBrRaceFlyPos[n], &g_pBrRaceFlyAim[n]);
                                    BrVec3Sub(&g_brRaceFlyDirNext, &v38, &v20);
                                    br_dl_normalise(&g_brRaceFlyDirNext);
                                }
                            } while (g_brRaceBeginPathT > g_brRaceBeginPathSeg);   /* 0x1001b619 */
                            if (ended) (*(int *)&g_brRaceBeginAirplane) = 0;         /* 0x1001b632 */
                        }
                        if ((*(int *)&g_brRaceBeginAirplane) != 0) {                /* 0x1001b64a transform+emit */
                            float  lerpT;
                            int    lerpBits;
                            int    wi = (*(int *)&g_brRaceBeginPathIdx);
                            float *row;
                            BrVec3 *r0, *r1, *r2;
                            /* 0x1001b665: stored, and every use reads the float */
                            lerpT = g_brRaceBeginPathT / g_brRaceBeginPathSeg;
                            lerpBits = *(int*)&lerpT;
                            lerpT = *(float*)&lerpBits;
                            row = BrRaceInst((*(int *)&g_brRaceBeginAirplane));
                            r0 = (BrVec3 *)&row[0];         /* the matrix rows */
                            r1 = (BrVec3 *)&row[4];
                            r2 = (BrVec3 *)&row[8];
                            /* 0x1001b670: the interpolated position lands in v2c
                             * (lea [esp+0x38] under three pushes) */
                            BrVec3Lerp(&v2c, &g_pBrRaceFlyPos[wi], &g_pBrRaceFlyPos[wi - 1], lerpT);
                            BrVec3Lerp(&v20, &g_pBrRaceFlyAim[wi], &g_pBrRaceFlyAim[wi - 1], lerpT);
                            BrVec3Midpoint((BrVec3 *)&row[12], &v2c, &v20);
                            /* 0x1001b6c6: [esp+0x20] under three pushes is the
                             * lerpT slot, not v20 -- the heading blends on t */
                            if (lerpT > g_brRaceFlyBlendA)
                                BrVec3Lerp(r0, &g_brRaceFlyDirNext, &g_brRaceFlyDirCur, lerpT - g_brRaceFlyBlendA);
                            else                            /* 0x1001b708 */
                                BrVec3Lerp(r0, &g_brRaceFlyDirCur, &g_brRaceFlyDirPrev, lerpT - g_brRaceFlyBlendB);
                            br_dl_normalise(r0);
                            /* 0x1001b774 -> 0x1001b799: the forward difference goes
                             * into the first cross product UNnormalised */
                            BrVec3Sub(r2, &v2c, &v20);
                            BrVec3Cross(r1, r0, r2);
                            br_dl_normalise(r1);
                            BrVec3Cross(r2, r1, r0);
                            br_dl_normalise(r2);
                            BrVec3ScaleBy(r0, g_brRaceBeginPathScale);
                            BrVec3ScaleBy(r2, -g_brRaceBeginPathScale);
                            BrVec3ScaleBy(r1, g_brRaceBeginPathScale);
                        }
                    }
                    break;
                }
            }
            b++;
            e++;
        } while (b < g_brTrkHdr.nSpecial);
    }
Lb887:  /* 0x1001b887 */
    {
        int i;
        for (i = 0; i < (*(int *)&g_BrCarCount); i++) BrSndCarStep(&g_aBrRaceCar[i]);
    }
    BrSndNearestInvalidate();                               /* 0x1001b8ae */
    {
        int v = 0;
        if (g_brMode0AA8B4 > 0) {                           /* 0x1001b8c9 leader-attach loop */
            do {
                /* every pass reads the FIRST view's car */
                const BrMat4 *active = (const BrMat4 *)g_aBrRaceCar[g_aBrView[0].iCar].pMatA;
                if ((*(int *)&g_brRaceBeginAirplane) != 0 && (*(int *)&g_brRaceBeginAirArmed) != 0) {
                    BrSndNearestOfferTrack((*(int *)&g_brRaceBeginAirplane),
                                           (const BrVec3 *)&BrRaceInst((*(int *)&g_brRaceBeginAirplane))[12], active);
                }
                if (active != 0) {                    /* 0x1001b918 */
                    int j;
                    for (j = 0; j < (*(int *)&g_brRaceBeginFxCount); j++) {
                        BrSndNearestOfferDefault((*(int *)&g_brRaceBeginAirplane),
                                                 (const BrVec3 *)&BrRaceInst(g_aBrRaceBeginFx[j])[12], active);
                    }
                }
                BrPodNop((const void *)active);                 /* 0x1001b954 */
                v++;
            } while (v < g_brMode0AA8B4);
        }
    }
    BrSndNearestCommit();                               /* 0x1001b977 */
    BrRaceHudFrame();
    loc18 = BrSnapPickSlot();
    {                                             /* 0x1001b990 deep-copy loop */
        BrSnap *ps = &g_aBrSnap[loc18];
        int     ct = (*(int *)&g_BrCarCount); if (ct == 0) ct = 1;
        int     k;
        for (k = 0; k < ct; k++) {
            BrDriverCar *out = &ps->car[k];
            memcpy(out, &g_aBrRaceCar[k], sizeof(BrDriverCar));      /* rep movsd 0xada */
            if (g_aBrRaceCar[k].pMatA != 0)                          /* active-model fixup */
                out->pMatA = BrRaceSnapMat(&g_aBrRaceCar[k], out, g_aBrRaceCar[k].pMatA);
            if (g_aBrRaceCar[k].pMatB != 0)
                out->pMatB = BrRaceSnapMat(&g_aBrRaceCar[k], out, g_aBrRaceCar[k].pMatB);
        }
        if ((*(int *)&g_brRaceNDriver) > 0) {                       /* 0x1001bb2a record-copy loop */
            int d;
            for (d = 0; d < (*(int *)&g_brRaceNDriver); d++) {
                BrDriverCar *v = g_aBrRaceDriver[d].pCar;
                memcpy(&ps->drv[d], &g_aBrRaceDriver[d], sizeof(BrDriver));   /* rep movsd 0x20 */
                if (v != 0)                                                   /* re-index the car */
                    ps->drv[d].pCar = &ps->car[v - g_aBrRaceCar];
            }
        }
        /* 0x1001bbcd: the first view's record, then the particle state */
        memcpy(&ps->tailA, &g_aBrView[0], 0x16 * 4);   /* rep movsd 0x16 */
        BrPfxSaveState((short *)&ps->tailC);
    }
    if ((*(int *)&g_brRaceBeginLimitOn) == 0) goto Lc583;                /* 0x1001bc04 -> replay-advance */
    g_5BCAF0 = BrSub10075020();                    /* 0x1001bc16 */
    g_5BC76C = 0;
    (*(int *)&g_brRaceBeginLimitOn) = 0;
    DAT_105ccb68[14] = 0;
    GhostBody:  /* 0x1001bc37 */
        if ((*(int *)&g_BrX06909B4) == 2) {                      /* 0x1001bc44 */
            int f = (*(int *)&g_BrX18ABAD0);
            if (f & 0x8000) {
                if (DAT_105bc8dc == 0) {              /* 0x1001bca5 */
                    BrFadeSetTarget(0, 0x3e4ccccd);
                    BrFadeSetTargetA(0, 0x3e4ccccd);
                    BrFadeSetTargetB(0, 0x3e4ccccd);
                    DAT_105ccb68[9] = 0; DAT_105ccb68[12] = 2;
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
            if (DAT_105ccb68[6] == 0) goto Lc002;
            DAT_105ccb68[6] = 0;                          /* 0x1001bd1b */
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
            int i = 0;
            do {                                  /* loop top 0x1001bd94 */
                BrRaceCtl *pCtl = g_aBrRaceCar[i].pCtl;
                int f = (*(int *)&g_BrX18ABAD0);
                int skip = 0;
                if (!(f & 0x8000)) {              /* 0x1001bd99 gate */
                    if (!(DAT_105bc8dc == 0 && (f & 4) && !(f & 0x3000) && !(f & 3)))
                        skip = 1;
                }
                if (!skip) {
                    if (DAT_105bc8dc != 0 || ((uint8_t)pCtl->ctl & 0x10) == 0)   /* 0x1001bdbf */
                        BrBitLatchTake((BrBitLatch *)pCtl, 0, 0xc010);       /* 0x1001bdca */
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
                            DAT_105ccb68[12] = 0; DAT_105ccb68[9] = 0; DAT_105ccb68[11] = 0;
                            BrRaceDriverReset(); (*(int *)&g_brRaceTick) = 0; BrSndBankFree(); BrClearFlag_AB504();
                            break;
                        case 4:  /* 0x1001be73 */
                            BrFadeSetTarget(0, 0x3e4ccccd);
                            BrFadeSetTargetA(0, 0x3e4ccccd);
                            BrFadeSetTargetB(0, 0x3e4ccccd);
                            DAT_105ccb68[9] = 0; DAT_105ccb68[12] = 1;
                            if ((*(char *)&DAT_100bb2e0) != 0) { BrCdResume();
                                                 BrCdVolumeSet((unsigned char)(*(char *)&DAT_100bb2e0)); }
                            if (g_brRaceRules.mode == 6) { BrDPlayMsg6SendSelf(); BrNetClearF10220DD0(); }
                            break;
                        case 5:  /* 0x1001bedf */
                            loc10 = 1; (*(int *)&g_BrX06909B4) = 1; DAT_105bc8dc = 1;
                            break;
                        case 2: case 3: default: break;   /* 0x1001befc */
                        }
                    }
                }
                /* 0x1001befc tail */
                if (DAT_105ccb68[6] != 0) {
                    DAT_105ccb68[6] = 0;
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
            } while (i < (*(int *)&g_brRaceNEntrant));
        }
    Lc002:  /* 0x1001c002 */
        if ((*(int *)&g_brRaceNet) == 0 || (*(int *)&g_brRaceTick) == 0) goto Lc13d;
        BrNetSlotBroadcastTick();
        goto Lc13d;
    Lc024:  /* 0x1001c024 */
        if (g_brRaceRules.mode == 4 || g_brRaceRules.mode == 5) {     /* 0x1001c02c */
            int k;
            if (BrFadeIsClosing() != 0) goto Lc13d;
            for (k = 0; k < 2; k++) {             /* 0x1001c0e0: the two entity records */
                if ((*(int *)&g_BrX18ABAD0) & 0x4000) {
                    BrBitLatchTake((BrBitLatch *)&g_aBrEnts[k], 0, 0xc010);
                    (*(int *)&g_brRaceBeginMovieDone) = 1; DAT_105ccb68[12] = 1; DAT_105ccb68[9] = 0;
                    BrFadeSetTargetA(0, 0x3e4ccccd);
                    BrFadeSetTarget(0, 0x3e4ccccd);
                }
            }
            goto Lc13d;
        }
        if ((*(int *)&g_brRaceNEntrant) > 0) {                       /* 0x1001c03b */
            if (DAT_10226a50 != 0) {                  /* 0x1001c05f */
                int b88 = DAT_105ccb68[8];
                DAT_10226a50 = 0;
                if (b88 != 0) {
                    BrFadeSetTarget(0, 0x3e4ccccd);
                    DAT_105ccb68[9] = 0; DAT_105ccb68[12] = 1;
                } else {                          /* 0x1001c08e */
                    loc10 = 1; DAT_105bc8dc = 0;
                }
                BrFadeSetTargetA(0, 0x3e4ccccd);      /* 0x1001c09c */
                BrFadeSetTargetB(0, 0x3e4ccccd);
                BrBitLatchTake((BrBitLatch *)g_aBrRaceCar[0].pCtl, 0, 0x4000);
                BrBitLatchTake((BrBitLatch *)g_aBrRaceCar[1].pCtl, 0, 0x4000);
            }
            /* else 0x1001c051: a counting loop with no effect */
        }
        goto Lc13d;
    Lc13d:  /* 0x1001c13d */
        if (g_brRaceRules.mode == 4) {
            if (g_5bc760 == 2) BrRaceCueRewind();
            if (g_brRaceRules.mode == 4 && g_5bc760 == 1) {   /* 0x1001c158 credits-card timer */
                const BrCreditCard *pc = &g_brRaceRules.aCard[(*(int *)&g_brRaceBeginSeqIdx)];
                if (pc->pszTitle != 0) {
                    g_brRaceBeginSeqT = g_brRaceBeginSeqT + g_brRaceFlyStep;  /* 0x1001c179 x87 timer */
                    if (g_brRaceBeginSeqT > pc->t) {
                        g_brRaceBeginSeqT = 0.0f;
                        (*(int *)&g_brRaceBeginSeqIdx)++;
                    }
                }
            }
        }
        if (BrFadeIsClosing() != 0 && BrFadeIsSettled() != 0) {   /* 0x1001c1ad */
            int k;
            for (k = 0; k < 16; k++) {            /* 0x1001c1c7 shift-copy, every car slot */
                if ((*(int *)&g_brRaceNEntrant) > 0) {
                    BrRaceCtl *c = g_aBrRaceCar[k].pCtl;
                    int        j = 0;
                    do {
                        c->aCap[j] = c->aLen[j];
                        j++;
                    } while (j < (*(int *)&g_brRaceNEntrant));
                }
            }
            g_184C454 = 0;                        /* 0x1001c201 */
            for (k = 0; k < 15; k++) {            /* 0x1001c207 sound-channel reset */
                g_aBrSfxChan[k].packed = 0; g_aBrSfxChan[k].f10 = 0;
                g_aBrSfxChan[k].ratio = 0;
                /* the original stores the address of the empty group at
                 * 0x118EEF2C; its 32-bit form is the same token */
                g_aBrSfxChan[k].group = (int32_t)br_addr32(&g_18EEF2C);
                BrX10072580(k);
            }
            (*(int *)((char *)&g_aBrEntRecs + 0xA8)) = 0;                         /* 0x1001c236 finalize dispatch */
            if (DAT_105ccb68[12] != 0) {
                /* 0x1001c249: `je` -- the race-end path runs when the flag is CLEAR */
                if (DAT_105ccb68[9] == 0) goto Lc372;
                if (DAT_105ccb68[8] != 0) goto Lc368;
                {                                 /* 0x1001c261 leader min-search */
                    int n = (*(int *)&g_brRaceNEntrant);
                    int mx;
                    DAT_105ccb68[2] = g_aBrRaceCar[0].pCtl->aLen[0];
                    (*(int *)&g_brRaceBeginBestCar) = 0;
                    mx = DAT_105ccb68[2];
                    if (n > 1) {
                        int i = 1;
                        do {
                            int v = g_aBrRaceCar[i].pCtl->aLen[i];
                            if (v < mx) { mx = v; (*(int *)&g_brRaceBeginBestCar) = i; DAT_105ccb68[2] = v; }
                            i++;
                        } while (i < n);
                    }
                    if (n > 0) {                  /* 0x1001c2bd gather leader records */
                        int best = (*(int *)&g_brRaceBeginBestCar);
                        int k2 = 0;
                        do {
                            BrRaceCtl *d = g_aBrRaceCar[k2].pCtl;
                            /* 0x1001c2dc: `inc ecx` before the stores -- slots 1..n */
                            k2++;
                            g_apBrRaceLeaderRec[k2] = d->apRec[best];
                            g_aBrRaceLeaderLen[k2] = d->aLen[best];
                        } while (k2 < n);
                    }
                    if ((*(int *)&g_brRaceNEntrant) > 0) {           /* 0x1001c305 report + clear */
                        int j = 0;
                        do {
                            BrRaceCtl *c = g_aBrRaceCar[j].pCtl;
                            BrPodNop(&g_0A978C, 0, j, &c->aLen[0]);
                            c->apRec[j] = 0;
                            j++;
                        } while (j < (*(int *)&g_brRaceNEntrant));
                    }
                    BrPodNop(&g_0A9768, (*(int *)&g_brRaceBeginBestCar), DAT_105ccb68[2]);   /* 0x1001c351 */
                    goto Lc45c_1;
                }
            }
            goto Lc475;
        Lc368:  /* 0x1001c368 */
            goto Lc45c_1;
        Lc372:  /* 0x1001c372 */
            BrNop_1002CB3F();
            (*(int *)&g_brRaceBeginDifficulty) = 0;
            if (g_brRaceRules.mode == 5) {
                if (g_aBrRaceCar[0].pEquip[4] != 0) {   /* 0x1001c387 */
                    g_brRaceRules.mode = 0;
                    BrSelLookup();
                    g_17A613C = 1;
                    BrGameStepSet((void (*)(void))BrSetMode5);
                    goto Lc45c_0;
                }
                BrRaceEnterOutro();                   /* 0x1001c3af */
                goto Lc45c_0;
            }
            {                                     /* 0x1001c3b9 mode dispatch */
                int st = g_brRaceRules.mode, c = g_5bc760;
                if (st == 4 && c == 2) goto Lc44d;
                if (DAT_105ccb60 != 0 &&
                    (st == 2 || st == 1 || st == 0 || st == 6)) {
                    BrCarStateRestore();               /* 0x1001c3e8 */
                    BrGameStepSet(BrRaceSelFromMenu);
                    goto Lc45c_0;
                }
                if (st == 4) {                    /* 0x1001c3f4: both tests under st == 4 */
                    if (c == 1) goto Lc44d;
                    if (c == 0 && (*(int *)&g_brRaceBeginMovieDone) == 0) goto Lc44d;
                }
                if (DAT_105ccb68[12] != 2) goto Lc44d;
                if (!((*(int *)&g_brRace6EC760) == (*(int *)&g_brItemIconCount) && (*(int *)&g_brRace6E9A34) == (*(int *)&g_brRaceB71A6C)))   /* 0x1001c417 */
                    BrGlCfgSave(&g_BrCtrlCfg, (const char *)&g_navArg);
                BrExt_10038F30(0);                  /* 0x1001c444 */
            }
        Lc44d:  /* 0x1001c44d */
            BrGameStepSet(BrInit220B20);
        Lc45c_0:  /* 0x1001c45c, newB88 == 0 */
            DAT_105ccb68[8] = 0; DAT_105ccb68[11] = 0;
            BrRaceDriverReset();
            BrClearFlag_AB504();
            goto Lc475;
        Lc45c_1:  /* 0x1001c45c, newB88 == 1 */
            DAT_105ccb68[8] = 1; DAT_105ccb68[11] = 0;
            BrClearFlag_AB504();
        Lc475:  /* 0x1001c475 */
            g_17A5F20 = 1;
            for (k = 0; k < 16; k++) g_aBrRaceCar[k].pCtl->pHdr = 0;
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
        if (g_brRaceRules.mode == 6) FUN_10006460();        /* 0x1001c4e3 */
        {                                         /* 0x1001c4f1 force-feedback spring */
            int flag = (*(int *)&g_aBrRaceCar[0].aBody[0].rb.child[0]->f1B4 == 0 &&
                        *(int *)&g_aBrRaceCar[0].aBody[0].rb.child[1]->f1B4 == 0) ? 0 : 1;
            BrFfbUpdateSpring((unsigned char)g_aBrRaceCar[0].aBody[0].f0209, flag, g_aBrRaceCar[0].fE90);
        }
        if ((*(int *)&DAT_100b51e4[1036]) != 0 && (*(int *)&g_BrX06909B4) == 0) FUN_1006bdd0();   /* 0x1001c52a */
        if (DAT_105ccb68[8] != 0) BrReplaySeek();        /* 0x1001c543 */
        else               BrReplayAdvance();
        FUN_10014cb0();
        BrSub10075150();
        if ((*(int *)&g_brRaceTick) == 0) {                      /* 0x1001c561 */
            if ((*(int *)&g_brRaceNet) != 0) BrNetCheckDeadline();
            BrTimeUpdate();
        }
        return;                                   /* 0x1001c57b epilogue */

    Lc583:  /* 0x1001c583 replay-advance clock (after the ret; forward-goto only) */
        g_5BCAF0 += g_0A95B8[g_5BC76C];
        { int nx = g_5BC76C + 1; g_5BC76C = (nx <= 2) ? nx : 0; }
        DAT_105ccb68[14]++;
        if (DAT_105ccb68[14] > 3) {                       /* 0x1001c5c7 */
            if (BrSnapInterpDraw(1) == 0) goto GhostBody;
            DAT_105ccb68[14] = 0;
            DAT_105ccb68[15] = BrSub10075020();
            goto GhostBody;
        }
        {                                         /* 0x1001c5ee time-sync loop */
            int t, esi = 1, ba4;
            t = BrSub10075020();
        Lc5f3:
            ba4 = DAT_105ccb68[15];
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
            DAT_105ccb68[14] = 0;
            DAT_105ccb68[15] = ba4 = t;
            goto Lc5f9;
        }
}
