/* WHAT IT DOES: the per-frame race step (0x10019A70 BrRaceStep). Advances the
 * frame-delta ring buffer and the race clock; on the first frame runs the
 * scene/audio/limiter init; then dispatches on the race state (g_0A9360) through
 * intro/credits/outro, per-driver display-list build, HUD/pause/camera update,
 * and the replay-advance timer. One C++ TU (member-call heavy, no EH frame).
 *
 * The largest single function in BRGlide (11,223 B, 131 calls). */
/* @t3 0x10019A70 2026-09-15 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 10944/11223 insns 2913/2939 rows 354+328 regions 65 oracle EQUIV-MODULO-FP
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is register colouring/scheduling, behaviour-neutral: A5 proves
 * same-in/same-out (EQUIV-MODULO-FP over valid-state seeding -- every race
 * state, gating-flag combos, live-driver pointer graph). The byte gap is one
 * prologue scheduler tie-break (idx reuses eax vs a fresh reg) colouring the
 * whole function, FIRSTDIV +0xD; the O2y measure variant inflates the rows
 * (frameless fn, sweep-variant artifact). Dossier: memory bracestep-wall.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
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
 * bracestep dossier. Do not reopen the colouring before the end-grind
 * (CLAUDE.md rule 12).
 *
 * @t4-pass 0x10019A70 1 2026-09-15 probes 14 bytes 10944 insns 2913 regions 65 rows 682 census no  (region-1 register grind: the idx-reuses-eax prologue scheduler tie-break; cache-removal lever -- re-reading g_0B3858 snapped zero->ebp; delta/count reorder; inf[1] single-read; #pragma intrinsic(memcpy) so copies inline as rep movsd; delta/count/idx declaration permutations. FIRSTDIV held +0xD -- colouring wall.)
 * @t4-pass 0x10019A70 2 2026-09-15 probes 11 bytes 10944 insns 2913 regions 65 rows 682 census yes  (write-slot census + 4-variant sweep confirm the residue is register allocation/scheduling, not missing/wrong code -- the A5 oracle proves same-in/same-out incl. the g_226A44 branch fix; numbers unmoved.) */
#include <stdint.h>
#include <string.h>
#pragma intrinsic(memcpy)   /* original inlines the copies as rep movsd, not a call */

/* ---- opaque per-driver arrays (0x2b68 = 11112-byte stride) ---- */
typedef struct { char _[0x2b68]; } Driver;
extern Driver g_AF3BC8[];   /* 0x10AF3BC8 */
extern Driver g_AF1208[];   /* 0x10AF1208 (parallel this-ptr array) */
extern Driver g_AF3D70[];   /* 0x10AF3D70 */

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

#define PI(p,off)  (*(int  *)((char*)(p) + (off)))
#define PB(p,off)  (*(char *)((char*)(p) + (off)))

/* ---- cdecl callees (extern "C" = unmangled; (...) = polymorphic arity) ---- */
extern "C" {
    int sub_1006E280();
    int sub_10008D60(...);          /* called 0-arg and 1-arg */
    int sub_1000CB80();
    int sub_10031140(int);
    int sub_1006E030();
    int sub_100189C0();
    int sub_1002BF24(void*);
    int sub_100353C0(int);
    int sub_1006FD50(...);          /* also seen as thiscall; see Obj */
    int sub_100609D0(void*);
    int sub_10005CD0();
    int sub_10006400();
    int sub_10004E00();
    int sub_100060A0();
    int sub_10006250(int, void*);
    int sub_10009BA0();
    int sub_100060B0();
    int sub_1001C6A0(int);
    int sub_10019930();
    int sub_10069A80(const char*, int);
    int sub_10063B60();
    int sub_100627B0(int);
    int sub_10061310();
    int sub_100311C0(int);
    int sub_10034870(void*, void*, void*);
    float sub_100347F0(void*);
    int sub_1005D050();   /* stored as callback ptr */
    int sub_100199A0();
    int sub_1005E690();
    int sub_10062830(int);
    int sub_100181A0(int, int);
    int sub_10018230(int, int);
    int sub_10018290(int, int);
    int sub_10030270(void*, const char*, void*);
    int sub_1002ECAC(int);
    int sub_10033B50();
    int sub_10063DD0();
    int sub_1002E13B();
    int sub_10060E30();
    int sub_10063A00();
    int sub_10063A40();
    int sub_10002C00();
    int sub_10002AF0(int);
    int sub_10002D30(int);
    int sub_10013E80();
    int sub_10019A40();
    int sub_10006100();
    int sub_1001C7A0(int);
    int sub_1002E186();
    int sub_10060E00();
    int sub_10060DF0();
    int sub_1001C810();
    int sub_1005C450();
    int sub_10018310();
    int sub_100023F0(void*, int);
    int sub_1005F580();
    int sub_10033BB0();
    int sub_10016C90();
    int sub_1002A590(void*, int, int, int, int);
    int sub_10029D70(void*, void*, void*);
    int sub_10060E10();
    int sub_100611F0(int, void*, int);
    int sub_10061280(int, void*, int);
    int sub_10060F40();
    int sub_10019890();
    int sub_10013F20();
    int sub_10033C90(void*);
    int sub_10004F90();
    int sub_10005400();
    int sub_10019980();
    int sub_10018340();
    int sub_1006B4F0(int);
    int sub_10060A10();
    int sub_10002F10();
    int sub_100609F0();
    int sub_10002EB0();
    int sub_1006BD70();
    int sub_10061440();
    int sub_10006460();
    int sub_10072210(int, int, int);
    int sub_1006BDD0();
    int sub_10063CC0();
    int sub_10063AD0();
    int sub_10014CB0();
    int sub_1006E3B0();
    int sub_100064D0();
    int sub_1006E360();
    int sub_1002CB3F();
    int sub_1001C9D0();
    int sub_10019900();
    int sub_1001C890();
    int sub_100325B0(int);
    int sub_1002E317(void*);
    int sub_10019A10();
    int sub_10013F00();
    int sub_1006A070();
    int sub_10002460();
    int sub_10032680();
    int sub_1006C460();
    int sub_10037180();
    int sub_10006430();
    int sub_10059E50();
    int sub_10059DE0();
    int sub_10059E30();
    int sub_10059DC0();
    int sub_100131E0(int);
    int sub_100346D0(void*, void*, void*);
    float sub_10034760(void*, void*);
    int sub_10034560(void*, void*, void*);
    int sub_100344D0(void*);
    int sub_10034620(void*, void*, void*, float);
    int sub_100342B0(void*, void*, void*);
    int sub_10034390(void*, float);
}

/* ---- indirect-call function pointers ---- */
extern int (*g_B7352C)();
extern int (*g_18ED1E8)();
extern int (*g_0B849C)();
extern int (*g_B73528)();

/* ---- data globals used so far ---- */
extern int  g_5CCB90, g_0A935C, g_0A9358, g_5CCB7C, g_5BCAE4;
extern int  g_5BC900[];
extern int  g_5CCB94, g_6ED684, g_5CCB88, g_0A9360, g_0B3014;
extern int  g_6E86B8;               /* address-taken (push &g_6E86B8) */
extern int  g_5CCB8C, g_0BCBE8, g_1778848;
extern int  g_0B2F00, g_0B2F04;
extern int  g_226E7C, g_AF6724;
extern int  g_226A4C, g_226A48;
extern int  g_AF134C;
extern char g_B71648[];             /* string source (inlined strcpy) */
extern char g_AF1350[];             /* string dest */
extern int  g_5BC810;
extern int  g_2066C8;               /* 0x102066C8 base */
extern int  g_18EEF50, g_18EF0B8;   /* 0x18-stride zeroing loop bounds */
extern int  g_0B3858;
/* case 5 / case 4 */
extern int  g_AF2090, g_AF393C, g_AF3988, g_B1CF10, g_5BC8E0;
extern int  g_5BC760, g_5CCB9C, g_5BCAE0;
extern int  g_5BC7B8, g_1778850, g_5BC8D8;
extern char g_1787850[];
extern char g_0A9884[], g_0A9878[], g_0A9868[], g_0A9850[];
extern char g_AF3BB4, g_AF3BB5, g_AF3BB6;
extern int  g_AF3BAC, g_AF20A0, g_AF20A4, g_AF2098, g_AF209C, g_226E80, g_4B15E8;
/* La213 tail */
extern int  g_6EA3F4, g_BCAB0, g_6EEEFC, g_6EEE3C, g_5BCAE8, g_5BC778, g_6EED38;
extern int  g_5BC7C0, g_5BC7C4, g_5BC7C8, g_5BC7CC, g_5BC7D0, g_5BC7D4;
extern int  g_5BC7DC, g_5BC7E0;
extern float g_5BC7E4, g_5BC7E8;
extern float g_5BC7D8;
extern float g_5BC7EC, g_5BC7F0, g_5BC7F4, g_5BC7F8, g_5BC7FC, g_5BC800;
extern float g_5BC804, g_5BC808, g_5BC80C;
extern int  g_0AA044, g_6E86C8, g_6E8720;
extern char g_0A9840[], g_0A9824[], g_0A9808[], g_0A97F8[], g_0A97E0[], g_0A97D0[];
/* 0x1001a462 wheel/tyre/camera + callback loop */
extern int  g_AF2094, g_B71530, g_AF6730, g_AF4C00, g_AF4C04, g_AF4C08, g_AF4C0C;
extern int  g_5CCB60, g_AF2110, g_0BCDD0, g_6E86CC, g_0A6B68;
extern float g_AF397C, g_0773B0;
/* init-tail 0x1001a97c */
extern int   g_0A9578, g_5BC750, g_5BC8F8, g_18EE588, g_AF07F8, g_6ED6AC;
extern int   g_5BCAEC, g_5BCAF8, g_17A5910, g_5CCB5C, g_5CCB98;
extern int   g_5BC888, g_5BC768, g_6ED6D8;
extern float g_5BC884;
extern int   g_0A9368;
extern float g_0A936C;
extern float g_0A957C, g_5BC880;
extern char  g_0BB2E0;
/* per-frame tail 0x1001ab93 */
extern int   g_5CCB58, g_5CCB78, g_226A44, g_AF2210, g_18EEED8, g_AF0860, g_5CCB74;
extern short g_17A5F20;
extern float g_0A9548, g_5BC764, g_0773B4;
extern float g_6E9D8C, g_0773A4, g_0773C4, g_0773C8, g_0773CC, g_0773D0;
/* state-4 limiter */
extern int   g_AF2200, g_5BC8E4, g_5BC8E8, g_5BC8EC, g_AF2204, g_AF2208, g_AF220C, g_AF2214;
extern float g_AF21F4, g_0773B8, g_0773BC, g_0773C0;
extern char  g_0A97A8[];
/* merge tail 0x1001b1c9 */
extern int   g_AF3B54, g_AF3B14, g_6ED6B0, g_6ED6B4;
extern int   g_6E7970;
extern char  g_397950[];
extern int   g_AF3940, g_AF3944, g_AF3A98, g_AF39CC, g_AF3A10;
extern char  g_AF0858[], g_396FAC[], g_3C2FD0[];
extern int   g_0A5EA8, g_5BCAF0, g_5BC76C, g_5CCBA0, g_18EEBE8, g_5BC8DC, g_5CCB64, g_5CCB80;
extern int   g_B1F248, g_184C454, g_18EEF2C;
extern int   g_5CCB70, g_5BC814, g_5BC88C;
extern int   g_AF1370, g_AF1374, g_0B55F0;
extern char  g_AF1575, g_17A613C;
extern int   g_6EC760, g_B71A68, g_6E9A34, g_B71A6C, g_B71290, g_B72F48;
extern int   g_226A50;
extern char  g_6ED708[], g_6ED9C0[];
extern int   g_0A95B8[], g_5CCBA4;
extern char  g_0A978C[], g_0A9768[];

extern int (*g_18ED1C4)(int,int,int,int,int,int,int,int,int,int,int,int,int,int,int);
extern int (*g_18ED1C0)(int,int,int,int,int,int,int,int);
extern int g_18ED1B4;
extern "C" int sub_1006D280(int);
extern "C" int sub_10008EF0(int);

extern "C" void BrRaceStep(void)
{
    int loc10, loc14, loc18, loc1c;     /* esp+0x10/14/18/1c render-loop state */
    int now   = sub_1006E280();
    int delta = now - g_5CCB90;
    g_5CCB90  = now;
    int idx   = g_0A935C;
    int count = g_0A9358;

    /* region 1: frame-delta ring buffer (0x10019A70-0x10019AE4) */
    if (idx < 0) {
        idx = 0;
        if (count > 0) {
            int i;
            for (i = 0; i < count; i++) g_5BC900[i] = delta;
            idx = count;
        }
    }
    if (++g_5BCAE4 == 0) g_5CCB7C = 0; else g_5CCB7C += delta;
    g_0A935C = ++idx;                       /* store idx+1 unconditionally (0x1001a acf) */
    if (idx >= count) { idx = 0; g_0A935C = idx; }   /* then conditionally re-store 0 */
    g_5BC900[idx] = delta;

    /* region 2: first-frame init gate (0x10019AE4) */
    if (g_5CCB94 == 0) {
        g_6ED684 = 1;
        sub_10008D60(1);
        sub_1000CB80();
        if (g_5CCB88 == 0) {
            sub_10008D60();
            sub_10008D60();
            g_B7352C();
            g_18ED1E8();
            g_0B849C();
            sub_10031140(g_0A9360 == 5 ? 0xc : g_0B3014);
            sub_1006E030();
            g_B73528();
            sub_100189C0();
        }
        sub_1002BF24(&g_6E86B8);
        sub_10008D60(1);
        sub_1002BF24(&g_6E86B8);
        sub_100353C0(0x7b);
        g_5CCB8C = 0;
        if (g_0A9360 != 1 && g_0A9360 != 6 && g_0A9360 != 2) g_0BCBE8 = 3;
        g_1778848 = 0;
        if ((unsigned)g_0A9360 > 6) goto Ldefault;
        switch (g_0A9360) {
        case 0:     /* 0x10019bc8 */
            g_0B2F00 = 0x14; g_0B2F04 = 3;
            ((Obj*)&g_AF1208)->m_1006FD50(g_226E7C);
            goto Lcf3;
        case 1:     /* 0x10019bf1 */
            g_0B2F00 = 2; g_0B2F04 = 2;
            ((Obj*)&g_AF1208)->m_1006FD50(g_226E7C);
            ((Obj*)&g_AF3D70)->m_1006FD50(g_226E7C);
            g_AF6724 = 0;
            goto Lcf3;
        case 6:     /* 0x10019c2a */
            if (g_5CCB88 != 0) goto Lcfb;
            g_0B3858 = 1;
            if (g_226A4C == 0) { g_0B2F00 = 1; g_0B2F04 = 1; }
            else               { g_0B2F00 = 0; g_0B2F04 = 0; }
            ((Obj*)&g_AF1208)->m_1006FD50(g_226E7C);
            if (g_226A48 != 0) {
                sub_10005CD0();
                sub_10006400();
                sub_10004E00();
                g_AF134C = sub_100060A0();
                strcpy(g_AF1350, g_B71648);
                sub_10006250(g_AF134C, g_B71648);
            }
            while (sub_10009BA0() > (unsigned)g_0B2F04) {
                int r = sub_100060B0();
                if (r >= 0) sub_1001C6A0(r);
            }
            goto Lcf3;
        case 5:     /* 0x10019d87 */
            g_0B3858 = 1; g_0B2F00 = 1; g_0B2F04 = 1;
            g_5CCB8C = 0; g_5CCB88 = 0; g_AF2090 = 0;
            g_AF393C = (int)&g_AF3988; g_B1CF10 = 0;
            goto La213;
        case 4:     /* 0x10019dc0 */
        {
            int *pd0 = *(int**)&g_AF3BC8;
            g_4B15E8 = 1; g_0B3858 = 1; g_0B2F00 = 1; g_0B2F04 = 1;
            g_5CCB8C = 0; g_AF2090 = 0;
            PI(pd0, 0x44) = (int)&g_5BC8E0;
            {
                int which = g_5BC760;                 /* 0x10019df1 */
                if (which == 0) goto Lintro;
                if (which == 1) goto Lcredits;
                if (which == 2) goto Loutro;
                goto Le63;
            Loutro:                                   /* 0x10019e00 */
                sub_10019930();
                sub_10069A80("RallyOutro.dat", 0);
                goto Le58;
            Lcredits:                                 /* 0x10019e15 */
                sub_10069A80("RallyCredits.dat", 0);
                goto Le58;
            Lintro:                                   /* 0x10019e25 */
                sub_10069A80(g_5CCB9C ? "RallyIntro2.dat" : "RallyIntro1.dat", 0);
                { int nv = g_5CCB9C + 1;
                  if (nv <= 1) g_5CCB9C = nv; else g_5CCB9C = 0; }
            Le58:                                     /* 0x10019e58 */
                sub_10063B60();
                g_5BCAE0 = 0;
            }
        Le63:                                         /* 0x10019e63 */
            {
                int  *pObj  = *(int**)&g_AF3BC8;
                char *pInfo;
                PI(pObj, 0x2c) = 0;
                PI(pObj, 0x30) = 0;
                g_AF3BB4 = (char)0xff; g_AF3BB5 = (char)0xff; g_AF3BB6 = (char)0xff;
                pInfo = (char*)PI(pObj, 0x44);
                g_0B3014 = (signed char)pInfo[0];
                g_AF3BAC = (signed char)pInfo[1];
                ((Obj*)&g_AF1208)->m_1006FD50((signed char)pInfo[1]);
                g_AF20A0 = (signed char)pInfo[2];
                g_AF20A4 = (signed char)pInfo[3];
                g_AF2098 = (signed char)pInfo[4];
                g_AF209C = (signed char)pInfo[5];
                PB(pObj, 0x25) = pInfo[6];
                g_226E80 = (signed char)pInfo[7];
                g_4B15E8 = (signed char)pInfo[7];
            }
            goto La213;
        }
        case 2:                 /* 0x10019f0e */
        {
            ((Obj*)&g_AF1208)->m_1006FD50(g_226E7C);
            g_5CCB8C = 0;
            g_5BC7B8 = 1;
            g_0B2F00 = g_0B3858 + 1;
            g_0B2F04 = g_0B3858 + 1;
            PI(*(int**)&g_AF3BC8[g_0B3858], 0x44) = (int)&g_1778850;
            sub_10008D60(&g_0A9884, g_5BC8D8);
            memcpy((void*)PI(*(int**)&g_AF3BC8[g_0B3858], 0x44), &g_5BC8E0, g_5BC8D8);
            {
                char *inf = (char*)PI(*(int**)&g_AF3BC8[g_0B3858], 0x44);
                int v1 = (signed char)inf[1];
                *(int*)((char*)&g_AF3BAC + g_0B3858 * 0x2b68) = v1;
                ((Obj*)((char*)&g_AF1208 + g_0B3858 * 0x2b68))->m_1006FD50(v1);
            }
            sub_10063B60();
            {
                char *inf = (char*)PI(*(int**)&g_AF3BC8[g_0B3858], 0x44);
                if ((signed char)inf[0] == g_0B3014 &&
                    (signed char)inf[7] == g_226E80) {          /* 0x1001a02b */
                    sub_10008D60(&g_0A9878, (signed char)inf[0]);
                    PI(*(int**)&g_AF3BC8[g_0B3858], 0x48) = 8;
                    sub_10008D60(&g_0A9868, g_5BC8D8);
                    PI(*(int**)&g_AF3BC8[g_0B3858], 0x4c) = g_5BC8D8;
                } else {                                        /* 0x1001a09c */
                    sub_10008D60(&g_0A9850, (signed char)inf[0], g_0B3014);
                    PI(*(int**)&g_AF3BC8[g_0B3858], 0x44) = 0;
                    g_0B2F04 = 1; g_0B2F00 = 1;
                }
            }
            /* 0x1001a0d9 .. 0x1001a1ec: per-driver display-list build loop */
            if (g_0B3858 > 0) {
                char *base = (char*)&g_AF3BC8;     /* eax -> driver[i] */
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
                    q[0] = (char)g_0B3014;
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
                    q[7] = (char)g_226E80;
                    pd = *(int**)(base - 0x2b68);
                    *(int*)((char*)pd + off + 4) = 8;
                    pd = *(int**)(base - 0x2b68);
                    *(int*)((char*)pd + off + 0xc) = 0xde5c;
                } while (i < g_0B3858);
            }
            goto La213;
        }
        default: Ldefault:      /* 0x1001a1ee (== case 3) */
        case 3:
            ((Obj*)&g_AF1208)->m_1006FD50(g_226E7C);
            g_5CCB8C = 0;
            g_0B2F00 = g_0B3858;
            g_0B2F04 = g_0B3858;
            goto La213;
        }

    Lcf3:   /* 0x10019cf3 */
        if (g_5CCB88 == 0) goto Ld39;
    Lcfb:   /* 0x10019cfb */
        {
            int a = g_5BC810;
            sub_100609D0((char*)&g_2066C8 - a * 89992);
            {
                int p = (int)&g_18EEF50;        /* 0x10019d23 zero loop (signed cmp) */
                do { *(int*)(p + 4) = 0; *(int*)p = 0; p += 0x18; }
                while (p < (int)&g_18EF0B8);
            }
        }
        goto Ld70;
    Ld39:   /* 0x10019d39 */
        {
            int i;
            for (i = 0; i < g_0B3858; i++) {
                int *p = *(int**)&g_AF3BC8[i];
                PI(p, 0x44) = 0; PI(p, 0x2c) = 0; PI(p, 0x30) = 0;
            }
        }
    Ld70:   /* 0x10019d70 */
        g_5CCB8C = (g_5CCB88 == 0);
        goto La213;

    La213:  /* 0x1001a213 -- common tail after the state switch */
        if (g_0A9360 == 5) {
            g_6EA3F4 = 0;
        } else {
            g_6EA3F4 = (PI(*(int**)((char*)&g_BCAB0 + g_0B3014 * 4), 4) >> 4) & 1;
        }
        sub_100627B0(g_4B15E8);
        if (g_0A9360 != 4 && g_5CCB88 == 0) {
            sub_10008D60();
            sub_10008D60();
        }
        sub_10061310();
        if (g_5CCB88 != 0) goto La430;
        sub_100311C0(g_0A9360 == 5 ? 0xc : g_0B3014);
        if (g_5CCB88 != 0) goto La430;
        {
            int  cnt;
            g_5BC7CC = 0; g_5BC7C4 = 0; g_5BC7C0 = 0; g_5BC7E0 = 0;
            g_5BC7DC = 0; g_5BC7E4 = 0; g_5BC7E8 = 0; g_5BC7D0 = 0; g_5BC7D4 = 0;
            sub_10008D60(&g_0A9840, g_6EEEFC);
            g_5BCAE8 = 0;
            cnt = g_6EEEFC;
            if (cnt > 0) {
                char *e = (char*)&g_6EEE3C;     /* 0x106eee3c */
                int   i = 0;
                do {
                    int          arg = 0;
                    const char  *str = 0;
                    switch ((signed char)PB(e, 8) - 3) {   /* jmp [.. 0x1001c664] */
                    case 0:  arg = PI(e, 0); g_5BC7C0 = arg; str = (char*)&g_0A97F8; break;
                    case 1:  g_5BC7D4 = PI(e, 4); arg = PI(e, 0); g_5BC7DC = arg;
                             str = (char*)&g_0A9824; break;
                    case 2:  arg = PI(e, 0); g_5BC7E0 = arg; str = (char*)&g_0A9808; break;
                    case 3:  arg = PI(e, 0); g_5BC7C4 = arg; str = (char*)&g_0A97E0; break;
                    case 4:  { int c = g_5BCAE8; arg = PI(e, 0);
                               *(int*)((char*)&g_5BC778 + c * 4) = arg;
                               g_5BCAE8 = c + 1; str = (char*)&g_0A97D0; } break;
                    default: goto Lskip;
                    }
                    sub_10008D60(str, arg);
                Lskip:
                    i++;
                    e += 0xc;
                } while (i < g_6EEEFC);
            }
            {
                int sel;                        /* 0x1001a3a6 */
                if (g_5BC7C4 == 0 || g_5BC7DC == 0 || g_5BC7E0 == 0) {
                    sel = 0; g_5BC7C0 = 0;
                } else {
                    sel = g_5BC7C0;
                }
                if (sel != 0) {
                    float m[3];
                    int  *row = (int*)((char*)&g_6EED38 + (sel + sel * 20) * 4);
                    m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f;
                    sub_10034870(&m[1], &m[0], row);
                    g_5BC7D8 = sub_100347F0(&m[0]);
                }
            }
            g_5BC7C8 = g_0BCBE8 - 1;
        }
    La430:  /* 0x1001a430 */
        g_0AA044 = 1;
        if (g_5CCB88 == 0) g_0AA044 = g_0B3858;
        g_6E86C8 = g_5CCB88 ? g_5BC810 : 0;
        g_6E8720 = (g_6E86C8 == 0);

        /* 0x1001a462: wheel/tyre + camera-mode setup */
        if (g_0A9360 == 4) {
            g_AF393C = (int)&g_AF3988;
            g_B1CF10 = 0xb4;
            goto La58d;
        }
        if (g_0B3858 > 0) {                     /* 0x1001a485 tyre loop */
            char *b = (char*)&g_AF2094;
            int   i = 0;
            do {
                int *p, *q;
                p = *(int**)b; *(int*)(b + 0x10) = PI(p, 0xfc);
                p = *(int**)b; *(int*)(b + 0x08) = PI(p, 0x104);
                p = *(int**)b; *(int*)(b + 0x04) = PI(p, 0x100);
                p = *(int**)b; *(int*)(b + 0x0c) = PI(p, 0xf8);
                q = *(int**)(b + 0x1b34);
                if (g_B71530 == 1 || g_B71530 == 2 || g_B71530 == 3)
                    PB(q, 0x25) = 5;
                else
                    PB(q, 0x25) = 2;
                i++;
                b += 0x2b68;
            } while (i < g_0B3858);
        }
        {
            int ecx = g_0B3858;                 /* 0x1001a4f1 */
            if (g_0A9360 == 2) {
                int *p = *(int**)&g_AF6730;
                if (PI(p, 0x44) != 0) {
                    char *inf = (char*)PI(p, 0x44);
                    g_AF4C08 = (signed char)inf[2];
                    g_AF4C0C = (signed char)inf[3];
                    g_AF4C00 = (signed char)inf[4];
                    g_AF4C04 = (signed char)inf[5];
                    PB(p, 0x25) = inf[6];
                    goto La58d;
                }
            }
            /* 0x1001a547 */
            if (ecx < g_0B2F04) {
                do {
                    char *e = (char*)&g_AF209C + ecx * 0x2b68;
                    int  *q;
                    *(int*)(e + 8) = 1;
                    *(int*)(e) = 1;
                    *(int*)(e - 4) = 2;
                    *(int*)(e + 4) = 0;
                    q = *(int**)(e + 0x1b2c);
                    ecx++;
                    PB(q, 0x25) = 0;
                } while (ecx < g_0B2F04);
            }
        }
    La58d:  /* 0x1001a58d */
        if (g_5CCB88 != 0) goto La973;
        g_5CCB60 = 0;
        if (g_0B2F04 > 0) {                     /* 0x1001a5ba callback-wiring loop */
            char *s = (char*)&g_AF2110;
            int   i = 0;
            do {
                int n  = g_0B3858;
                int st = g_0A9360;
                *(int*)(s + 0x1aa8) = 0x3f800000;
                if (i < n) {
                    *(int*)s = (int)&sub_1005D050;
                } else if (st == 2) {
                    *(int*)s = (int)&sub_1005D050;
                    PB(s, 0x1aa7) = (char)st;
                    *(int*)(s + 0x1aa8) = 0x3ec00000;
                    goto Lwire_after;
                } else if (st == 6) {
                    *(int*)s = (int)&sub_100199A0;
                } else {
                    ((Obj*)(s - 0xf08))->m_1005C490();
                    *(int*)s = (int)&sub_1005E690;
                }
                PB(s, 0x1aa7) = 0;              /* 0x1001a612 */
            Lwire_after:                        /* 0x1001a619 */
                if (*(int*)(s - 0x80) == 0)
                    ((Obj*)(s - 0xf08))->m_1006FCE0(i, *(int*)(s + 0x1aa0));
                ((Obj*)(s - 0xf08))->m_1005E7B0();
                *(int*)(s + 0x74)  = 0;
                *(int*)(s + 0xf4)  = 0;
                *(int*)(s + 0xfc)  = 0;
                i++;
                s += 0x2b68;
            } while (i < g_0B2F04);
        }
        if (g_0B2F04 == 0) {                    /* 0x1001a65f */
            int j;
            for (j = 0; j < 0x57e2; j++) ((int*)&g_0BCDD0)[j] = 0;
            ((Obj*)&g_AF1208)->m_1006FCB0(0);
            ((Obj*)&g_AF1208)->m_1005E7B0();
            ((Obj*)&g_AF1208)->m_10001CF0();
            g_AF397C = g_AF397C - g_0773B0;
        }
        loc14 = 0;                              /* 0x1001a6a0 */
        if (g_0AA044 > 0) {
            loc1c = (int)&g_0A6B68;
            loc10 = (int)&g_6E86CC;
            do {                                /* 0x1001a6c5 outer render loop */
                int   ix  = *(int*)(loc10 - 4);
                char *s   = (char*)&g_0BCDD0 + ix * 89992;   /* [edx*8+g_0BCDD0] */
                char *b   = (g_4B15E8 == 4) ? s + 0x300 : s + 0x100;
                int   a   = (signed char)PB(s, 0xe4);
                int   d   = (signed char)PB(s, 0xe5);
                char  kind;
                *(int*)loc10 = g_18ED1C4((int)(s + 0x500), (int)b, a, d, a,
                                       1, 2, 0, 0, 1, 1, 0, 0, 0, 0);
                kind = PB(s, 0xdb);
                if (kind == 1 || kind == 2) {          /* 0x1001a743 */
                    char *ep = (char*)loc10 + 4;
                    char *w  = b + 0x1fe;
                    g_18ED1B4 = 1;
                    loc18 = 0xf;
                    do {                               /* 0x1001a75e */
                        int a2, d2;
                        char k2 = PB(s, 0xdb);
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
                        a2 = (signed char)PB(s, 0xe4);
                        d2 = (signed char)PB(s, 0xe5);
                        *(int*)ep = g_18ED1C4((int)(s + 0x500), (int)b, a2, d2, a2,
                                              1, 2, 0, 0, 1, 1, 0, 0, 0, 0);
                        ep += 4;
                        w  -= 2;
                    } while (--loc18 != 0);
                    g_18ED1B4 = 0;
                }
                {                                      /* 0x1001a7f0 */
                    int a3 = (signed char)PB(s, 0xe8);
                    int c3 = (signed char)PB(s, 0xe9);
                    int d3 = (signed char)PB(s, 0xe4);
                    int e3 = (signed char)PB(s, 0xe5);
                    char *dst;
                    int   sz;
                    PI(loc10, 0x40) = g_18ED1C4((int)(d3 * e3 + s + 0x500), (int)b,
                                                a3, c3, a3, 1, d3, 0, 0, 1, 2,
                                                0, 0, 1, 0);
                    /* 0x1001a847: memmove of the emitted span */
                    sz  = ((signed char)PB(s, 0xd8) + 2)
                          * (signed char)PB(s, 0xe8) * (signed char)PB(s, 0xe9);
                    dst = s - sz + 0x8000;
                    memmove(dst,
                            (signed char)PB(s, 0xe4) * (signed char)PB(s, 0xe5)
                                + s + 0x500,
                            sz);
                    loc18 = 0;
                    if ((signed char)PB(s, 0xd8) + 2 > 0) {
                        char *ep = s + 0x500;
                        do {                           /* 0x1001a8ab */
                            int aa = (signed char)PB(s, 0xe8);
                            int cc = (signed char)PB(s, 0xe9);
                            int r  = g_18ED1C0((int)ep, (int)dst, (int)b,
                                               aa, cc, aa, 1, 2);
                            ep += r;
                            PI(s, 0xfc) = r;
                            if ((unsigned)ep > (unsigned)dst)
                                sub_10008EF0(sub_1006D280(0x12c));
                            dst += (signed char)PB(s, 0xe8) * (signed char)PB(s, 0xe9);
                            if ((unsigned)(ep - s) > 0x8000)
                                sub_10008EF0(sub_1006D280(0x12d));
                            loc18++;
                        } while (loc18 < (signed char)PB(s, 0xd8) + 2);
                    }
                }
                /* 0x1001a93e */
                *(int*)loc1c = 0xffffffff;
                loc1c += 4;
                loc14++;
                loc10 += 0x58;
            } while (loc14 < g_0AA044);
        }
        goto La97c;
    La973:  /* 0x1001a973 */
        loc14 = g_0B3858;
    La97c:  /* 0x1001a97c */
        {
            int sel = (g_5CCB88 != 0) ? 4 : ((g_0A9360 == 5) ? 4 : 0);
            g_5BC880 = *(float*)((char*)&g_0A957C + sel * 8);
            g_5BC8F8 = *(int*)((char*)&g_0A9578 + sel * 8);
            g_5BC750 = sel;
        }
        if (g_5CCB88 == 0) {
            g_18EE588 = 0;
            if (g_0B2F00 > 0) {                 /* 0x1001a9d3 */
                char *s = (char*)&g_AF07F8;
                int   i = 0;
                do { ((Obj*)s)->m_1005F310(); i++; s += 0x80; } while (i < g_0B2F00);
            }
        }
        sub_10062830(g_4B15E8);                 /* 0x1001a9ef */
        if (g_6ED6AC == 0) {
            int v = g_0B3014;
            if (v == 0 || v == 6) g_5BC7C0 = 0;
        }
        sub_100181A0(0x3f800000, 0x3e4ccccd);   /* 0x1001aa1c */
        sub_10018230(0x3f800000, 0x3e4ccccd);
        sub_10018290(0x3f800000, 0x3e4ccccd);
        g_5CCB94 = 1;                           /* 0x1001aa5e */
        if (g_5CCB88 == 0) {                    /* 0x1001aa66 */
            g_5BCAEC = (int)&g_5BCAF8;
            sub_10030270(&g_5BCAF8, "misc\\modelLights.blob", &g_17A5910);
            sub_1002ECAC(g_5BCAEC);
        }
        sub_10033B50();                         /* 0x1001aa96 */
        sub_10063DD0();
        g_5CCB5C = 0;
        g_5CCB98 = 0;
        g_5BC888 = 0xffffffff;
        g_5BC884 = 0;
        g_5BC768 = 0;
        g_6ED6D8 = 1;
        if (g_5CCB88 == 0) { sub_1002E13B(); sub_10060E30(); }
        g_6ED684 = 0;                           /* 0x1001aadf */
        if (g_5CCB88 != 0) {
            sub_10063B60();
        } else {
            sub_10063A00();                     /* 0x1001aaf5 */
            if (g_0A9360 == 4) sub_10063A40();
            if (g_0BB2E0 != 0) {                /* 0x1001ab0e */
                int c = g_5BC760;
                int arg;
                if (g_0A9360 == 4 && c == 2)      arg = 0xc;
                else if (g_0A9360 == 4 && c == 1) arg = 0xd;
                else                              arg = sub_10002C00();
                sub_10002AF0(arg);
                sub_10002D30((unsigned char)g_0BB2E0);
            }
        }
        sub_10013E80();                         /* 0x1001ab56 */
        sub_10008D60();
        sub_10019A40();
    }
    else {                                      /* 0x1001ab71 (g_5CCB94 != 0) */
        if (g_226A48 != 0) {
            int r = sub_10006100();
            if (r >= 0 && g_5CCB88 == 0) sub_1001C7A0(r);
        }
    }

    /* 0x1001ab93: per-frame tail (merge point) */
    g_17A5F20 = 0;
    if (g_5BC8F8 == 4 && g_0B3858 > 0) {
        char *s = (char*)&g_AF1208;
        int   i = 0;
        do { ((Obj*)s)->m_10060A30(); i++; s += 0x2b68; } while (i < g_0B3858);
    }
    if (g_0A9360 != 4 && g_5CCB5C == 0)      /* 0x1001abd0 */
        g_1778848 ^= 1;
    sub_1002E186();
    g_5CCB58 = 0;
    loc10 = g_5CCB5C;
    g_5CCB78 = 0;
    if (g_5BC8F8 < 3) {                       /* 0x1001ac00 */
        if (g_226A44 != 0) {                  /* 0x1001ac0f: je ac42 skips when ==0 */
            if (g_0B3858 > 0) {
                char *p = (char*)&g_AF2210;
                int   c = g_0B3858;
                do { *(int*)(p - 4) = 0; *(int*)p = 0x3c23d70a; p += 0x2b68; }
                while (--c);
            }
        } else if (g_226A48 == 0 || g_18EEED8 == 0) {   /* 0x1001ac8c */
            if (g_0B3858 > 0) {
                char *s = (char*)&g_AF2210;
                int   i = 0;
                do { *(int*)(s - 4) = sub_1006D280(0xee); *(int*)s = 0x3c23d70a;
                     i++; s += 0x2b68; } while (i < g_0B3858);
            }
        } else {                              /* 0x1001ac52 */
            if (g_0B3858 > 0) {
                char *s = (char*)&g_AF2210;
                int   i = 0;
                do { *(int*)(s - 4) = sub_1006D280(0xed); *(int*)s = 0x3c23d70a;
                     i++; s += 0x2b68; } while (i < g_0B3858);
            }
        }
        g_5CCB78 = 1;                         /* 0x1001acc4 */
        if (g_0B2F00 > 0) {
            char *s = (char*)&g_AF0860;
            int   i = 0;
            do {
                int *c;
                *(int*)s |= 1;
                c = *(int**)(s - 8);
                if (c != 0 && PI(c, 0x140) < g_0B3858) {
                    if (g_0A9360 == 1 || g_0A9360 == 6) {   /* 0x1001ad17 */
                        short dx = (short)(g_4B15E8 - 1);
                        int   e, base;
                        if (dx > 2 || dx < 0) dx = 0;
                        e = PI(c, 0xe64) * 3 + dx;
                        base = *(int*)((char*)&g_BCAB0 + g_0B3014 * 4);
                        PI(c, 0xff0) = *(int*)(base + (e * 7) * 4 + 0x44);
                    }
                    PI(c, 0x1000) = 0x3f800000;     /* 0x1001ad56 */
                    if (g_5BC8F8 == 0) {            /* 0x1001adac */
                        g_5CCB58 = 1;
                        g_5CCB74 = 0;
                    } else if (g_5BC8F8 == 2) {     /* 0x1001ad6e */
                        int k = g_5CCB74;
                        g_5CCB58 = 1;
                        if (*(float*)((char*)&g_0A9548 + k * 4) > g_5BC880) {
                            k++;
                            g_5CCB74 = k;
                            if (k == 4) sub_10060E00();
                            else        sub_10060DF0();
                        }
                    }
                }
                i++;
                s += 0x80;
            } while (i < g_0B2F00);
        }
        g_5BC764 = 0.0f;                       /* 0x1001adcc */
        goto Lb0cd;
    }
    else {                                    /* 0x1001addb: g_5BC8F8 >= 3 */
        if (g_5BC8F8 == 3) {                  /* 0x1001addd */
            g_5CCB58 = 1; g_5CCB78 = 1;
            if (g_0B2F00 > 0) {
                char *p = (char*)&g_AF0860;
                int   c = g_0B2F00;
                do { *(int*)p &= 0xfffffffe; p += 0x80; } while (--c);
            }
            { int k = g_5BC750;
              float t = *(float*)((char*)&g_0A957C + k * 8);
              float a = (t - g_5BC880) / t;
              g_5BC764 = a * a * g_0773B4; }
            goto Lb0cd;
        } else if (g_5BC8F8 == 4) {           /* 0x1001ae38 */
            loc14 = 1;
            if (g_0A9360 == 4) {              /* 0x1001ae54 */
                int c = g_5BC760;
                if (c == 2) {
                    if (g_AF21F4 > g_0773B8) goto Lae_a1;
                    goto Lae89;
                }
                if (g_AF21F4 > g_0773BC) goto Lae_a1;
            }
        Lae89:  /* 0x1001ae89 */
            if (g_0A9360 == 5) {
                if (g_AF21F4 <= g_0773C0) goto Laee2;
            }
        Lae_a1: /* 0x1001aea1 */
            if (sub_10018310() == 0) {
                sub_100181A0(0, 0x3e4ccccd);
                sub_10018230(0, 0x3e4ccccd);
                g_5CCB98 = 1; g_5CCB88 = 0; g_5CCB8C = 0;
                loc14 = 0;
            }
        Laee2:  /* 0x1001aee2 */
            if (g_0B3858 > 0) {
                int i = 0;
                do {
                    if ((PB((char*)&g_AF0860 + (i << 7), 0) & 2) != 0) {
                        int handled = 0;
                        if (g_0A9360 == 2) {              /* 0x1001af07 */
                            int *pd = *(int**)&g_AF3BC8[i];
                            if (PI(pd, i * 4 + 0x2c) != 0) {
                                if (i == 0) {             /* 0x1001af38 */
                                    int  e8 = g_5BC8D8;
                                    int  clx = (signed char)g_5BC8E0;
                                    if (!(g_AF2200 != 0 && clx == g_0B3014 && e8 > 8)) {
                                        int *p0 = *(int**)&g_AF3BC8;
                                        int  s34 = PI(p0, 0x34);
                                        int *q;
                                        sub_10008D60(&g_0A97A8, 0, s34, e8, (s34 < e8),
                                                     clx, g_0B3014, (clx != g_0B3014),
                                                     e8, 8, (e8 <= 8));
                                        g_5BC8D8 = 0x10;
                                        q = *(int**)PI(*(int**)&g_AF3BC8, 0x2c);
                                        g_5BC8E0 = q[0];
                                        g_5BC8E4 = q[1];
                                        g_5BC8E8 = 0; g_5BC8EC = 0;
                                        g_AF2204 = sub_1006D280(0xef);
                                        g_AF2208 = 0x3f800000;
                                        sub_100023F0(&g_AF2214, g_AF21F4);
                                        g_AF220C = (int)&g_AF2214;
                                        g_AF2210 = 0x3f800000;
                                        g_5BC7B8 = 1;
                                    }
                                }
                                PI(*(int**)&g_AF3BC8[i], i * 4 + 0x2c) = 0;   /* 0x1001b01b */
                                handled = 1;
                            }
                        }
                        if (!handled &&                    /* 0x1001b03a */
                            (g_0A9360 == 1 || g_0A9360 == 6) && g_0B3858 > 0) {
                            int *pd = *(int**)&g_AF3BC8[i];
                            int  off = 0x3c;
                            int  j = 0;
                            do {
                                *(int*)((char*)pd + off) = *(int*)((char*)pd + off - 8);
                                off += 4; j++;
                            } while (j < g_0B3858);
                        }
                    } else {
                        loc14 = 0;                         /* 0x1001b085 */
                    }
                    i++;
                } while (i < g_0B3858);
            }
            if (loc14 != 0) goto Lb0f8;                    /* 0x1001b092 */
            goto Lb171;
        } else if (g_5BC8F8 == 5) {           /* 0x1001b09d */
            int st = g_0A9360;
            if (st == 2 || st == 1 || st == 0 || st == 6)
                if (g_5CCB88 == 0) sub_1001C810();
            goto Lb0f8;
        } else if (g_5BC8F8 == 6) {           /* 0x1001b0c8 */
            goto Lb0cd;
        } else if (g_5BC8F8 == 7) {           /* 0x1001b127 */
            if (g_5CCB98 == 0) {
                sub_10018230(0, 0x3e4ccccd);
                if (g_5CCB8C == 0) sub_10018290(0, 0x3e4ccccd);
                sub_100181A0(0, 0x3e4ccccd);
                g_5CCB98 = 1;
            }
            goto Lb171;
        } else goto Lb171;
    }
Lb0cd:  /* 0x1001b0cd shared timing */
    if (g_5CCB5C != 0) goto Lb171;
    g_5BC880 = g_5BC880 - g_6E9D8C;
    if (!(g_5BC880 < g_0773A4)) goto Lb171;
Lb0f8:  /* 0x1001b0f8 step advance */
    if (g_226A44 == 0) goto Lb171;
    {
        int k = ++g_5BC750;
        g_5BC880 = *(float*)((char*)&g_0A957C + k * 8);
        g_5BC8F8 = *(int*)((char*)&g_0A9578 + k * 8);
    }
Lb171:  /* 0x1001b171 merge */
    sub_10008D60(0, 0, 0, 0xc8, 0xff);
    sub_1005C450();
    if (g_0B2F04 > 0) {
        char *s = (char*)&g_AF1208;
        int   i = 0;
        do { ((Obj*)s)->m_10061430(); i++; s += 0x2b68; } while (i < g_0B2F04);
    }
    if (g_0B2F00 > 0) {
        char *s = (char*)&g_AF07F8;
        int   i = 0;
        do { ((Obj*)s)->m_10061F60(); i++; s += 0x80; } while (i < g_0B2F00);
    }
    if (!(g_226A48 == 0 && g_5CCB88 == 0)) {      /* 0x1001b1d9 */
        if (g_0B2F00 > 0) {
            char *s = (char*)&g_AF07F8;
            int   i = 0;
            do { ((Obj*)s)->m_100623A0(); i++; s += 0x80; } while (i < g_0B2F00);
        }
    }
    if (g_0B2F00 > 0) {                           /* 0x1001b20b */
        char *s = (char*)&g_AF07F8;
        int   i = 0;
        do { ((Obj*)s)->m_100623E0(); i++; s += 0x80; } while (i < g_0B2F00);
    }
    if (g_0A9360 == 0 && g_0B3858 > 0) {          /* 0x1001b22d */
        char *s = (char*)&g_AF1208;
        int   i = 0;
        do { ((Obj*)s)->m_1005F6C0(); i++; s += 0x2b68; } while (i < g_0B3858);
    }
    sub_1005F580();                               /* 0x1001b25c */
    if (g_5CCB5C != 0) goto Lb887;
    if (g_5CCB88 == 2) goto Lb887;
    sub_10008D60(0, 0x80, 0x80, 0, 0xff);
    sub_10033BB0();
    sub_10008D60(0, 0, 0xff, 0xff, 0xff);
    sub_10016C90();
    if (g_5BC7C0 != 0 && g_5BC7CC == 0) {         /* 0x1001b2b6 leaderboard scan */
        if (g_6ED6B4 != 0 || g_6ED6B0 != 0 || g_6ED6AC != 0) {
            if (g_0B3014 == 2 || g_0B3014 == 8) goto Lb365;
        }
        if (g_0B3858 > 0) {
            char *s   = (char*)&g_AF3B54;
            int   edx = 0;
            int   rem = g_0B3858;
            do {
                int c = *(int*)s;
                int a;
                for (a = 0; a < c; a++) {
                    if (*(int*)(s - 0x19a4) == g_5BC7C8) {
                        unsigned short w =
                            *(unsigned short*)((char*)&g_AF3B14 + (a + edx) * 2);
                        if ((int)w == g_5BC7C4) { g_5BC7CC = 1; break; }
                    }
                }
                edx += 0x15b4;
                s += 0x2b68;
            } while (--rem);
        }
    }
Lb365:  /* 0x1001b365 */
    if (g_6EEEFC > 0) {
        char *e = (char*)&g_6EEE3C;
        int   b = 0;
        do {
            int sw = (signed char)PB(e, 8);
            if ((unsigned)sw <= 3) {                /* jmp [.. 0x1001c678] */
                switch (sw) {
                case 0:  /* 0x1001b393 */
                    sub_1002A590(&g_6E7970, PI(e, 4), 0, 0, 0x3f800000);
                    goto Lb_emit;
                case 1:  /* 0x1001b3a0 */
                    sub_1002A590(&g_6E7970, PI(e, 4), 0x3f800000, 0, 0);
                    goto Lb_emit;
                case 2:  /* 0x1001b3ad */
                    sub_1002A590(&g_6E7970, PI(e, 4), 0, 0x3f800000, 0);
                Lb_emit: /* 0x1001b3b8 */
                    {
                        int  *m = (int*)((char*)&g_6EED38 + PI(e, 0) * 84);
                        sub_10029D70(&g_6E7970, m, m);
                        m = (int*)((char*)&g_6EED38 + PI(e, 0) * 84);
                        *(unsigned short*)((char*)m + 0x4c) &= 0xdfff;
                    }
                    break;
                case 3:  /* 0x1001b403 */
                    if (g_5BC7C0 == 0 || g_5BC7CC == 0) break;
                    {
                        float v20[3], v2c[3], v38[3];
                        float dt = (g_0B3014 == 1 || g_0B3014 == 7)
                                   ? g_6E9D8C * g_0773C8    /* 0x1001b436 */
                                   : g_6E9D8C * g_0773C4;   /* 0x1001b428 */
                        g_5BC7E4 = g_5BC7E4 - dt;           /* 0x1001b442 fsubr */
                        if (g_5BC7E4 > g_5BC7E8) {          /* 0x1001b465 interp loop */
                            int ended = 0;
                            do {
                                int   wi;
                                g_5BC7E4 = g_5BC7E4 - g_5BC7E8;
                                wi = g_5BC7D0 + 1;
                                g_5BC7D0 = wi;
                                if (wi >= g_5BC7D4) { ended = 1; break; }
                                sub_100346D0(v2c, (char*)g_5BC7DC + wi * 12 - 0xc,
                                                  (char*)g_5BC7E0 + wi * 12 - 0xc);
                                sub_100346D0(v20, (char*)g_5BC7DC + wi * 12,
                                                  (char*)g_5BC7E0 + wi * 12);
                                g_5BC7E8 = sub_10034760(v2c, v20);
                                sub_10034560(&g_5BC7F8, v20, v2c);
                                sub_100344D0(&g_5BC7F8);
                                if (g_5BC7D0 < 2) {         /* 0x1001b525 */
                                    g_5BC7EC = g_5BC7F8; g_5BC7F0 = g_5BC7FC; g_5BC7F4 = g_5BC800;
                                } else {                    /* 0x1001b549 */
                                    sub_100346D0(v38, (char*)g_5BC7DC + (g_5BC7D0 - 2) * 12,
                                                      (char*)g_5BC7E0 + (g_5BC7D0 - 2) * 12);
                                    sub_10034560(&g_5BC7EC, v2c, v38);
                                    sub_100344D0(&g_5BC7EC);
                                }
                                if (g_5BC7D0 + 1 == g_5BC7D4) {   /* 0x1001b5a8 */
                                    g_5BC804 = g_5BC7F8; g_5BC808 = g_5BC7FC; g_5BC80C = g_5BC800;
                                } else {                    /* 0x1001b5ca */
                                    sub_100346D0(v38, (char*)g_5BC7DC + g_5BC7D0 * 12,
                                                      (char*)g_5BC7E0 + g_5BC7D0 * 12);
                                    sub_10034560(&g_5BC804, v38, v20);
                                    sub_100344D0(&g_5BC804);
                                }
                            } while (g_5BC7E4 > g_5BC7E8);   /* 0x1001b619 */
                            if (ended) g_5BC7C0 = 0;         /* 0x1001b632 */
                        }
                        if (g_5BC7C0 != 0) {                /* 0x1001b64a transform+emit */
                            float lerpT = g_5BC7E4 / g_5BC7E8;
                            char *row   = (char*)&g_6EED38 + g_5BC7C0 * 84;
                            sub_10034620(v38, (char*)g_5BC7DC + g_5BC7D0 * 12,
                                              (char*)g_5BC7DC + g_5BC7D0 * 12 - 0xc, lerpT);
                            sub_10034620(v20, (char*)g_5BC7E0 + g_5BC7D0 * 12,
                                              (char*)g_5BC7E0 + g_5BC7D0 * 12 - 0xc, lerpT);
                            sub_100346D0(row + 0x30, v2c, v20);
                            if (v20[0] > g_0773CC)          /* 0x1001b6c6 */
                                sub_10034620(row, &g_5BC804, &g_5BC7F8, v20[0] - g_0773CC);
                            else                            /* 0x1001b708 */
                                sub_10034620(row, &g_5BC7F8, &g_5BC7EC, v20[0] - g_0773D0);
                            sub_100344D0(row);
                            sub_10034560(row + 0x20, v2c, v20);
                            sub_100344D0(row + 0x20);
                            sub_100342B0(row + 0x10, row, row + 0x20);
                            sub_100344D0(row + 0x10);
                            sub_100342B0(row + 0x20, row + 0x10, row);
                            sub_100344D0(row + 0x20);
                            sub_10034390(row, g_5BC7D8);
                            sub_10034390(row + 0x20, -g_5BC7D8);
                            sub_10034390(row + 0x10, g_5BC7D8);
                        }
                    }
                    break;
                }
            }
            b++;
            e += 0xc;
        } while (b < g_6EEEFC);
    }
Lb887:  /* 0x1001b887 */
    if (g_0B2F04 > 0) {
        char *s = (char*)&g_AF1208;
        int   i = 0;
        do { ((Obj*)s)->m_10061470(); i++; s += 0x2b68; } while (i < g_0B2F04);
    }
    sub_10060E10();                               /* 0x1001b8ae */
    loc1c = 0;
    if (g_0AA044 > 0) {                           /* 0x1001b8c9 leader-attach loop */
        do {
            int   drv    = *(int*)&g_6E86C8;
            int   active = *(int*)((char*)&g_AF393C + drv * 0x2b68);
            if (g_5BC7C0 != 0 && g_5BC7CC != 0) {
                int *m = (int*)((char*)&g_6EED38 + g_5BC7C0 * 84 + 0x30);
                sub_100611F0(g_5BC7C0, m, active);
            }
            if (active != 0) {                    /* 0x1001b918 */
                char *s = (char*)&g_5BC778;
                int   j = 0;
                while (j < g_5BCAE8) {
                    int *m = (int*)((char*)&g_6EED38 + *(int*)s * 84 + 0x30);
                    sub_10061280(g_5BC7C0, m, active);
                    j++;
                    s += 4;
                }
            }
            sub_10008D60(active);                 /* 0x1001b954 */
            loc1c++;
        } while (loc1c < g_0AA044);
    }
    sub_10060F40();                               /* 0x1001b977 */
    sub_10019890();
    loc18 = sub_10013F20();
    {                                             /* 0x1001b990 deep-copy loop */
        char *obase = (char*)&g_397950 + 188656 * loc18;
        int   doff  = 0;
        int   ct    = g_0B2F04; if (ct == 0) ct = 1;
        int   k     = 0;
        while (k < ct) {
            char *out = obase + doff;
            int   a;
            memcpy(out, (char*)&g_AF1208 + doff, 11112);      /* rep movsd 0xada */
            a = *(int*)((char*)&g_AF393C + doff);             /* active-model fixup */
            if (a != 0) {
                int fx = 0;
                if      (a == (int)((char*)&g_AF3944 + doff)) fx = (int)(out + 0x273C);
                else if (a == (int)((char*)&g_AF3A98 + doff)) fx = (int)(out + 0x2890);
                else if (a == (int)((char*)&g_AF3988 + doff)) fx = (int)(out + 0x2780);
                else if (a == (int)((char*)&g_AF39CC + doff)) fx = (int)(out + 0x27C4);
                else if (a == (int)((char*)&g_AF3A10 + doff)) fx = (int)(out + 0x2808);
                *(int*)(out + 0x2734) = fx;
            }
            a = *(int*)((char*)&g_AF3940 + doff);
            if (a != 0) {
                int fx = 0;
                if      (a == (int)((char*)&g_AF3944 + doff)) fx = (int)(out + 0x273C);
                else if (a == (int)((char*)&g_AF3A98 + doff)) fx = (int)(out + 0x2890);
                else if (a == (int)((char*)&g_AF3988 + doff)) fx = (int)(out + 0x2780);
                else if (a == (int)((char*)&g_AF39CC + doff)) fx = (int)(out + 0x27C4);
                else if (a == (int)((char*)&g_AF3A10 + doff)) fx = (int)(out + 0x2808);
                *(int*)(out + 0x2738) = fx;
            }
            k++;
            doff += 0x2b68;
        }
    }
    if (g_0B2F00 > 0) {                           /* 0x1001bb2a record-copy loop */
        char *src = (char*)&g_AF0858;
        char *dst = (char*)&g_396FAC + 188656 * loc18;
        int   n   = g_0B2F00;
        do {
            int v = *(int*)src;
            memcpy(dst - 0x60, src - 0x60, 0x80);         /* rep movsd 0x20 */
            if (v != 0) {                                 /* re-index self pointer */
                int di = (v - (int)&g_AF1208) / 0x2b68;   /* magic /0x2b68 */
                *(int*)dst = (int)((char*)&g_397950 + 188656 * loc18 + di * 0x2b68);
            }
            src += 0x80;
            dst += 0x80;
        } while (--n);
    }
    /* 0x1001bbcd: camera-state snapshot + fixup */
    memcpy((char*)&g_3C2FD0 + 188656 * loc18, &g_6E86B8, 0x16 * 4);   /* rep movsd 0x16 */
    sub_10033C90((char*)&g_3C2FD0 + 188656 * loc18 + 0x5c);
    if (g_0A5EA8 == 0) goto Lc583;                /* 0x1001bc04 -> replay-advance */
    g_5BCAF0 = sub_1006E280();                    /* 0x1001bc16 */
    g_5BC76C = 0;
    g_0A5EA8 = 0;
    g_5CCBA0 = 0;
    GhostBody:  /* 0x1001bc37 */
        if (g_5CCB5C == 2) {                      /* 0x1001bc44 */
            int f = g_18EEBE8;
            if (f & 0x8000) {
                if (g_5BC8DC == 0) {              /* 0x1001bca5 */
                    sub_100181A0(0, 0x3e4ccccd);
                    sub_10018230(0, 0x3e4ccccd);
                    sub_10018290(0, 0x3e4ccccd);
                    g_5CCB8C = 0; g_5CCB98 = 2;
                } else if (g_5BC8DC == 1) {       /* 0x1001bc5e */
                    if (g_226A48 != 0 && g_226A44 != 0) {
                        sub_10004F90();
                    } else {                      /* 0x1001bc75 */
                        sub_10018230(0x3f800000, 0x3e4ccccd);
                        sub_10018290(0x3f800000, 0x3e4ccccd);
                        g_5CCB64 = 2; loc10 = 0;
                    }
                }
            }
            f = g_18EEBE8;                         /* 0x1001bcdb */
            if (f & 0x1000) g_5BC8DC = 1 - g_5BC8DC;
            if (f & 0x2000) g_5BC8DC = 1 - g_5BC8DC;
            if (g_5CCB80 == 0) goto Lc002;
            g_5CCB80 = 0;                          /* 0x1001bd1b */
            sub_10018230(0x3f800000, 0x3e4ccccd);
            sub_10018290(0x3f800000, 0x3e4ccccd);
            g_5CCB64 = 2; loc10 = 0;
            if (g_226A48 == 0 || g_226A44 == 0) goto Lc13d;
            sub_10005400();
            goto Lc13d;
        }
        /* 0x1001bd72: g_5CCB5C != 2 main path */
        if (g_5CCB5C == 0) goto Lc024;
        if (g_0B3858 > 0) {                       /* 0x1001bd7a per-driver replay */
            char *s = (char*)&g_AF3BC8;
            int   i = 0;
            do {                                  /* loop top 0x1001bd94 */
                int f = g_18EEBE8;
                int skip = 0;
                if (!(f & 0x8000)) {              /* 0x1001bd99 gate */
                    if (!(g_5BC8DC == 0 && (f & 4) && !(f & 0x3000) && !(f & 3)))
                        skip = 1;
                }
                if (!skip) {
                    if (g_5BC8DC != 0 || (*(char*)*(int**)s & 0x10) == 0)   /* 0x1001bdbf */
                        ((Obj*)*(int**)s)->m_1002F640(0xc010);              /* 0x1001bdca */
                    if ((unsigned)g_5BC8DC <= 5) {                          /* 0x1001bddc */
                        switch (g_5BC8DC) {        /* jmp [.. 0x1001c688] */
                        case 0:  /* 0x1001bdec */
                            if (g_226A48 != 0 && g_226A44 != 0) sub_10004F90();
                            else { sub_10018230(0x3f800000, 0x3e4ccccd);
                                   sub_10018290(0x3f800000, 0x3e4ccccd);
                                   g_5CCB64 = 2; loc10 = 0; }
                            break;
                        case 1:  /* 0x1001be39 */
                            sub_100181A0(0, 0x3e4ccccd);
                            g_5CCB98 = 0; g_5CCB8C = 0; g_5CCB94 = 0;
                            sub_10019A10(); g_226A44 = 0; sub_1006C460(); sub_10013F00();
                            break;
                        case 4:  /* 0x1001be73 */
                            sub_100181A0(0, 0x3e4ccccd);
                            sub_10018230(0, 0x3e4ccccd);
                            sub_10018290(0, 0x3e4ccccd);
                            g_5CCB8C = 0; g_5CCB98 = 1;
                            if (g_0BB2E0 != 0) { sub_10002F10();
                                                 sub_10002D30((unsigned char)g_0BB2E0); }
                            if (g_0A9360 == 6) { sub_10037180(); sub_10006430(); }
                            break;
                        case 5:  /* 0x1001bedf */
                            loc10 = 1; g_5CCB5C = 1; g_5BC8DC = 1;
                            break;
                        case 2: case 3: default: break;   /* 0x1001befc */
                        }
                    }
                }
                /* 0x1001befc tail */
                if (g_5CCB80 != 0) {
                    g_5CCB80 = 0;
                    sub_10018230(0x3f800000, 0x3e4ccccd);
                    sub_10018290(0x3f800000, 0x3e4ccccd);
                    g_5CCB64 = 2; loc10 = 0;
                }
                f = g_18EEBE8;
                if (f & 0x1000) {
                    g_5BC8DC = (g_5BC8DC + 5) % 6;
                    if (g_226A48 != 0 && g_5BC8DC == 1) g_5BC8DC = (g_5BC8DC + 5) % 6;
                }
                if (f & 0x2000) {
                    g_5BC8DC = (g_5BC8DC + 1) % 6;
                    if (g_226A48 != 0 && g_5BC8DC == 1) g_5BC8DC = (g_5BC8DC + 1) % 6;
                }
                if (f & 1) {
                    if (g_5BC8DC == 2)      sub_10059E50();
                    else if (g_5BC8DC == 3) sub_10059DE0();
                }
                if (f & 2) {
                    if (g_5BC8DC == 2)      sub_10059E30();
                    else if (g_5BC8DC == 3) sub_10059DC0();
                }
                i++;
                s += 0x2b68;
            } while (i < g_0B3858);
        }
    Lc002:  /* 0x1001c002 */
        if (g_226A48 == 0 || g_226A44 == 0) goto Lc13d;
        sub_10005400();
        goto Lc13d;
    Lc024:  /* 0x1001c024 */
        if (g_0A9360 == 4 || g_0A9360 == 5) {     /* 0x1001c02c */
            if (sub_10018310() != 0) goto Lc13d;
            {                                     /* 0x1001c0e0 particle-mute loop */
                char *p = (char*)&g_6ED708;
                do {
                    if (g_18EEBE8 & 0x4000) {
                        ((Obj*)p)->m_1002F640(0xc010);
                        g_5BCAE0 = 1; g_5CCB98 = 1; g_5CCB8C = 0;
                        sub_10018230(0, 0x3e4ccccd);
                        sub_100181A0(0, 0x3e4ccccd);
                    }
                    p += 0x15c;
                } while (p < (char*)&g_6ED9C0);
            }
            goto Lc13d;
        }
        if (g_0B3858 > 0) {                       /* 0x1001c03b */
            if (g_226A50 != 0) {                  /* 0x1001c05f */
                int b88 = g_5CCB88;
                g_226A50 = 0;
                if (b88 != 0) {
                    sub_100181A0(0, 0x3e4ccccd);
                    g_5CCB8C = 0; g_5CCB98 = 1;
                } else {                          /* 0x1001c08e */
                    loc10 = 1; g_5BC8DC = 0;
                }
                sub_10018230(0, 0x3e4ccccd);      /* 0x1001c09c */
                sub_10018290(0, 0x3e4ccccd);
                ((Obj*)*(int**)&g_AF3BC8)->m_1002F640(0x4000);
                ((Obj*)*(int**)&g_AF6730)->m_1002F640(0x4000);
            } else {                              /* 0x1001c051 no-op scan */
                int a = 0;
                do { a++; } while (a < g_0B3858);
            }
        }
        goto Lc13d;
    Lc13d:  /* 0x1001c13d */
        if (g_0A9360 == 4) {
            if (g_5BC760 == 2) sub_10019980();
            if (g_0A9360 == 4 && g_5BC760 == 1) {   /* 0x1001c158 lap-time gate */
                int slot = g_5BC768 << 5;
                if (*(int*)((char*)&g_0A9368 + slot) != 0) {
                    g_5BC884 = g_5BC884 + g_6E9D8C;  /* 0x1001c179 x87 timer */
                    if (g_5BC884 > *(float*)((char*)&g_0A936C + slot)) {
                        g_5BC884 = 0.0f;
                        g_5BC768++;
                    }
                }
            }
        }
        if (sub_10018310() != 0 && sub_10018340() != 0) {   /* 0x1001c1ad */
            {                                     /* 0x1001c1c7 shift-copy all drivers */
                char *s = (char*)&g_AF3BC8;
                do {
                    if (g_0B3858 > 0) {
                        int off = 0x3c;
                        int j   = 0;
                        do {
                            int *c = *(int**)s;
                            *(int*)((char*)c + off) = *(int*)((char*)c + off - 8);
                            off += 4;
                            j++;
                        } while (j < g_0B3858);
                    }
                    s += 0x2b68;
                } while (s < (char*)&g_B1F248);
            }
            g_184C454 = 0;                        /* 0x1001c201 */
            {                                     /* 0x1001c207 fog/particle reset */
                char *p = (char*)&g_18EEF50;
                int   i = 0;
                do {
                    *(int*)(p + 4) = 0; *(int*)p = 0;
                    *(int*)(p - 8) = 0; *(int*)(p - 4) = 0;
                    *(int*)(p - 0x10) = (int)&g_18EEF2C;
                    sub_1006B4F0(i);
                    p += 0x18;
                    i++;
                } while (p < (char*)&g_18EF0B8);
            }
            g_6ED6D8 = 0;                         /* 0x1001c236 finalize dispatch */
            if (g_5CCB98 != 0) {
                if (g_5CCB8C != 0) goto Lc372;
                if (g_5CCB88 != 0) goto Lc368;
                {                                 /* 0x1001c261 leader min-search */
                    int  n   = g_0B3858;
                    int  mx;
                    g_5CCB70 = PI(*(int**)&g_AF3BC8, 0x34);
                    g_5BC810 = 0;
                    mx = g_5CCB70;
                    if (n > 1) {
                        char *b   = (char*)&g_AF6730;
                        int   off = 0x38;
                        int   i   = 1;
                        do {
                            int v = PI(*(int**)b, off);
                            if (v < mx) { mx = v; g_5BC810 = i; g_5CCB70 = v; }
                            i++; off += 4; b += 0x2b68;
                        } while (i < n);
                    }
                    if (n > 0) {                  /* 0x1001c2bd gather leader deltas */
                        char *s  = (char*)&g_AF3BC8;
                        int   ea = g_5BC810 * 4 + 0x2c;
                        int   eb = g_5BC810 * 4 + 0x34;
                        int   k  = 0;
                        do {
                            int *d = *(int**)s;
                            *(int*)((char*)&g_5BC814 + k * 4) = PI(d, ea);
                            *(int*)((char*)&g_5BC88C + k * 4) = PI(d, eb);
                            k++; s += 0x2b68;
                        } while (k < n);
                    }
                    if (g_0B3858 > 0) {           /* 0x1001c305 report + clear */
                        char *b   = (char*)&g_AF3BC8;
                        int   off = 0x2c;
                        int   j   = 0;
                        do {
                            int *c = *(int**)b;
                            sub_10008D60(&g_0A978C, 0, j, (int)((char*)c + 0x34));
                            *(int*)((char*)(*(int**)b) + off) = 0;
                            j++; off += 4;
                        } while (j < g_0B3858);
                    }
                    sub_10008D60(&g_0A9768, g_5BC810, g_5CCB70);   /* 0x1001c351 */
                    goto Lc45c_1;
                }
            }
            goto Lc475;
        Lc368:  /* 0x1001c368 */
            goto Lc45c_1;
        Lc372:  /* 0x1001c372 */
            sub_1002CB3F();
            g_6EA3F4 = 0;
            if (g_0A9360 == 5) {
                if (*(char*)((char*)*(int**)&g_AF2094 + 4) != 0) {   /* 0x1001c387 */
                    g_0A9360 = 0;
                    sub_1001C9D0();
                    g_17A613C = 1;
                    sub_1002E317((void*)&sub_1006A070);
                    goto Lc45c_0;
                }
                sub_10019900();                   /* 0x1001c3af */
                goto Lc45c_0;
            }
            {                                     /* 0x1001c3b9 mode dispatch */
                int st = g_0A9360, c = g_5BC760;
                if (st == 4 && c == 2) goto Lc44d;
                if (g_5CCB60 != 0 &&
                    (st == 2 || st == 1 || st == 0 || st == 6)) {
                    sub_1001C890();               /* 0x1001c3e8 */
                    sub_1002E317((void*)&sub_10002460);
                    goto Lc45c_0;
                }
                if (st == 4 && c == 1) goto Lc44d;
                if (c == 0 && g_5BCAE0 == 0) goto Lc44d;
                if (g_5CCB98 != 2) goto Lc44d;
                if (!(g_6EC760 == g_B71A68 && g_6E9A34 == g_B71A6C))   /* 0x1001c417 */
                    ((Obj*)&g_B71290)->m_100634B0((int)&g_B72F48);
                sub_100325B0(0);                  /* 0x1001c444 */
            }
        Lc44d:  /* 0x1001c44d */
            sub_1002E317((void*)&sub_10032680);
        Lc45c_0:  /* 0x1001c45c, newB88 == 0 */
            g_5CCB88 = 0; g_5CCB94 = 0;
            sub_10019A10();
            sub_10013F00();
            goto Lc475;
        Lc45c_1:  /* 0x1001c45c, newB88 == 1 */
            g_5CCB88 = 1; g_5CCB94 = 0;
            sub_10013F00();
        Lc475:  /* 0x1001c475 */
            g_17A5F20 = 1;
            {
                char *s = (char*)&g_AF3BC8;
                do { PI(*(int**)s, 0x44) = 0; s += 0x2b68; } while (s < (char*)&g_B1F248);
            }
        }
        /* 0x1001c494: shared tail (both gate arms reach here) */
        {
            int prev = loc10;
            if (g_5CCB5C != prev) {
                if (prev == 0) {                  /* 0x1001c4bb */
                    sub_10060A10();
                    if (g_0BB2E0 != 0) {
                        sub_10002F10();
                        sub_10002D30((unsigned char)g_0BB2E0);
                    }
                } else {                          /* 0x1001c4a5 */
                    sub_100609F0();
                    sub_10002EB0();
                    sub_1006BD70();
                    sub_10061440();
                }
                g_5CCB5C = prev;
            }
        }
        if (g_0A9360 == 6) sub_10006460();        /* 0x1001c4e3 */
        {                                         /* 0x1001c4f1 HUD update */
            int flag = (PI(*(int**)&g_AF1370, 0x1b4) == 0 &&
                        PI(*(int**)&g_AF1374, 0x1b4) == 0) ? 0 : 1;
            sub_10072210((unsigned char)g_AF1575, flag, g_AF2098);
        }
        if (g_0B55F0 != 0 && g_5CCB5C == 0) sub_1006BDD0();   /* 0x1001c52a */
        if (g_5CCB88 != 0) sub_10063CC0();        /* 0x1001c543 */
        else               sub_10063AD0();
        sub_10014CB0();
        sub_1006E3B0();
        if (g_226A44 == 0) {                      /* 0x1001c561 */
            if (g_226A48 != 0) sub_100064D0();
            sub_1006E360();
        }
        return;                                   /* 0x1001c57b epilogue */

    Lc583:  /* 0x1001c583 replay-advance clock (after the ret; forward-goto only) */
        g_5BCAF0 += ((int*)&g_0A95B8)[g_5BC76C];
        { int nx = g_5BC76C + 1; g_5BC76C = (nx <= 2) ? nx : 0; }
        g_5CCBA0++;
        if (g_5CCBA0 > 3) {                       /* 0x1001c5c7 */
            if (sub_100131E0(1) == 0) goto GhostBody;
            g_5CCBA0 = 0;
            g_5CCBA4 = sub_1006E280();
            goto GhostBody;
        }
        {                                         /* 0x1001c5ee time-sync loop */
            int t, esi = 1, ba4;
            t = sub_1006E280();
        Lc5f3:
            ba4 = g_5CCBA4;
        Lc5f9:
            if ((unsigned)t >= (unsigned)g_5BCAF0 &&
                (unsigned)t <= (unsigned)(ba4 + 0x14d)) goto GhostBody;
            if (esi == 0) goto GhostBody;
            {
                int arg = ((unsigned)(ba4 + 0x14d) < (unsigned)t) ? 1 : 0;
                esi = sub_100131E0(arg);
                t   = sub_1006E280();
            }
            if (esi == 0) goto Lc5f3;
            g_5CCBA0 = 0;
            g_5CCBA4 = t;
            goto Lc5f9;
        }
}

