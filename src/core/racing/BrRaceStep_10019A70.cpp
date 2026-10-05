/* BrRaceStep_10019A70.cpp -- racing: the per-frame race step.
 *
 * One C++ translation unit in the original, where the gated scene draw at
 * 0x10019840 precedes the race step. That draw is matched and claimed in
 * src/core/drawing/br_drawgated.c (one body shared with the D3D twin); a copy
 * is carried here, unclaimed, in its original position. It is the first
 * function in the unit to read g_6E9D8C, and that first reference is what puts
 * g_6E9D8C second in the race step's `fld [g_5BC884]; fadd [g_6E9D8C]`.
 * Without it VC5 swaps the two operands.
 *
 * Compiled /O2 /Gi like the rest of the race TU: the codegen depends on the
 * accumulated vc50.idb, so it is measured through the serial /Gi chain.
 */
#include <string.h>
#pragma intrinsic(memcpy, memset, strcpy)

struct Vec3 { float x, y, z; };

/* the per-view texture block, 89992 bytes (g_0BCDD0[16]) */
struct Tex {
    char          _000[0x0d8];
    signed char   nMip;                 /* +0x0d8 */
    char          _0d9[0x0db - 0x0d9];
    char          kind;                 /* +0x0db */
    char          _0dc[0x0e4 - 0x0dc];
    signed char   w, h;                 /* +0x0e4 */
    char          _0e6[2];
    signed char   mw, mh;               /* +0x0e8 */
    char          _0ea[0x0fc - 0x0ea];
    int           used;                 /* +0x0fc */
    unsigned short pal[256];            /* +0x100 */
    unsigned short pal4[256];           /* +0x300 */
    char          data[0x15f88 - 0x500];/* +0x500 */
};

/* a car's recorded-input block (g_6ED708[], car->pCtl) */
struct Ctl {
    int           flags;                /* +0x00 */
    char          _04[0x25 - 0x04];
    char          b25;                  /* +0x25 */
    char          _26[0x2c - 0x26];
    char         *buf[2];               /* +0x2c */
    int           recIndex[2];          /* +0x34 */
    int           recPrev[2];           /* +0x3c */
    signed char  *info;                 /* +0x44 */
    int           infoType;             /* +0x48 */
    int           infoLen;              /* +0x4c */
    char          _50[0x15c - 0x50];
    void m_1002F640(int);
};

struct Equip { char _00[4]; char b4; char _05[0xf8 - 5]; int fF8, fFC, f100, f104, f108; };
struct Path  { char _00[0x1b4]; int f1B4; };
struct Mode  { char _00[0x38]; float f38; char _3c[8]; };

struct Car;
typedef void (*CarFn)(Car *);

struct Car {
    char          _0000[0x140];
    int           f140;                 /* +0x140 */
    int           f144;                 /* +0x144 */
    char          name[0x168 - 0x148];  /* +0x148 */
    Path         *p168;                 /* +0x168 */
    Path         *p16C;                 /* +0x16c */
    char          _0170[0x36d - 0x170];
    unsigned char b36D;                 /* +0x36d */
    char          _036e[0xe64 - 0x36e];
    int           fE64;                 /* +0xe64 */
    char          _0e68[0xe88 - 0xe68];
    int           fE88;                 /* +0xe88 */
    Equip        *pEquip;               /* +0xe8c */
    int           fE90, fE94, fE98, fE9C; /* +0xe90 */
    char          _0ea0[0xf08 - 0xea0];
    CarFn         pfnControl;           /* +0xf08 */
    char          _0f0c[0xf7c - 0xf0c];
    int           fF7C;                 /* +0xf7c */
    char          _0f80[0xfa8 - 0xf80];
    int           lap;                  /* +0xfa8 */
    char          _0fac[0xfec - 0xfac];
    float         tFinal;               /* +0xfec */
    int           fFF0;                 /* +0xff0 */
    char          _0ff4[4];
    int           fFF8;                 /* +0xff8 */
    int           fFFC;                 /* +0xffc */
    float         f1000;                /* +0x1000 */
    int           f1004;                /* +0x1004 */
    float         f1008;                /* +0x1008 */
    char          sz100C[0x1024 - 0x100c]; /* +0x100c */
    char          _1024[0x2734 - 0x1024];
    void         *f2734;                /* +0x2734 */
    void         *f2738;                /* +0x2738 */
    Mode          mode[6];              /* +0x273c */
    char          _28d4[0x290c - 0x28d4];
    unsigned short u290C[0x20];         /* +0x290c */
    int           n294C;                /* +0x294c */
    char          _2950[0x29a4 - 0x2950];
    int           f29A4;                /* +0x29a4 */
    int           f29A8;                /* +0x29a8 */
    char          b29AC, b29AD, b29AE, b29AF; /* +0x29ac */
    float         f29B0;                /* +0x29b0 */
    int           f29B4;                /* +0x29b4 */
    char          _29b8[0x29c0 - 0x29b8];
    Ctl          *pCtl;                 /* +0x29c0 */
    char          _29c4[0x2b68 - 0x29c4];

    void m_1006FD50(int);
    void m_1005C490();
    void m_1006FCE0(int, int);
    void m_1005E7B0();
    void m_1006FCB0(int);
    void m_10001CF0();
    void m_10060A30();
    void m_10061430();
    void m_1005F6C0();
    void m_10061470();
};

/* a player slot, 0x80 bytes (g_AF07F8[20]) */
struct Player {
    char          _00[0x60];
    Car          *car;                  /* +0x60 */
    char          _64[4];
    int           flags;                /* +0x68 */
    char          _6c[0x80 - 0x6c];
    void m_1005F310();
    void m_10061F60();
    void m_100623A0();
    void m_100623E0();
};

/* a camera view, 0x58 bytes (g_6E86B8[]) */
struct View {
    char          _00[0x10];
    int           car;                  /* +0x10 */
    int           h[17];                /* +0x14 */
};

/* one recorded frame of the replay ring, 0x2e0f0 bytes */
struct Snap {
    Player        players[20];          /* +0x00000 */
    int           _a00;
    Car           cars[16];             /* +0x00a04 */
    View          view;                 /* +0x2c084 */
    int           _2c0dc;
    char          x2C0E0[0x2e0f0 - 0x2c0e0];
};

/* a scene object, 0x54 bytes (g_6EED38) */
struct Obj {
    Vec3          r0; float w0;         /* +0x00 */
    Vec3          r1; float w1;         /* +0x10 */
    Vec3          r2; float w2;         /* +0x20 */
    Vec3          pos;                  /* +0x30 */
    char          _3c[0x4c - 0x3c];
    unsigned short flags;               /* +0x4c */
    char          _4e[0x54 - 0x4e];
};

/* a track special, 12 bytes (g_6EEE3C[]) */
struct Special { int a; int b; signed char type; char _9[3]; };

/* a hold-time entry, 32 bytes (g_0A9368[]) */
struct Hold { int on; float t; char _8[0x18]; };

/* the start-light sequence (g_0A9578[]) */
struct Phase { int phase; float t; };

/* a track's light table (g_0BCAB0[track]) */
struct LightRow { int v; char _4[0x18]; };
struct Track { int f0; unsigned int f4; char _8[0x44 - 8]; LightRow row[1]; };

struct Ent18 { int *p; int _4; int f8; int fC; int f10; int f14; };

class Mgr { public: void m_100634B0(void *); };

extern "C" {
    unsigned int sub_1006E280();
    int  sub_10008D60(...);
    void sub_1000CB80();
    void sub_10031140(int);
    void sub_1006E030();
    void sub_100189C0();
    void sub_1002BF24(View *);
    void sub_100353C0(int);
    void sub_100609D0(Tex *);
    void sub_10005CD0();
    void sub_10006400();
    void sub_10004E00();
    int  sub_100060A0();
    void sub_10006250(int, char *);
    unsigned int sub_10009BA0();
    int  sub_100060B0();
    void sub_1001C6A0(int);
    void sub_10019930();
    void sub_10069A80(const char *, int);
    void sub_10063B60();
    void sub_100627B0(int);
    void sub_10061310();
    void sub_100311C0(int);
    void sub_10034870(Vec3 *, Vec3 *, Obj *);
    float sub_100347F0(Vec3 *);
    void sub_1005D050(Car *);
    void sub_100199A0(Car *);
    void sub_1005E690(Car *);
    int  sub_1006D280(int);
    void sub_10008EF0(int);
    void sub_10062830(int);
    void sub_100181A0(float, float);
    void sub_10018230(float, float);
    void sub_10018290(float, float);
    void sub_10030270(void *, const char *, void *);
    void sub_1002ECAC(void *);
    void sub_10033B50();
    void sub_10063DD0();
    void sub_1002E13B();
    void sub_10060E30();
    void sub_10063A00();
    void sub_10063A40();
    int  sub_10002C00();
    void sub_10002AF0(int);
    void sub_10002D30(unsigned char);
    void sub_10013E80();
    void sub_10019A40();
    int  sub_10006100();
    void sub_1001C7A0(int);
    void sub_1002E186();
    void sub_10060E00();
    void sub_10060DF0();
    void sub_1001C810();
    void sub_1005C450();
    void sub_1005F580();
    void sub_10033BB0();
    void sub_10016C90();
    void sub_1002A590(void *, int, float, float, float);
    void sub_10029D70(void *, Obj *, Obj *);
    void sub_100346D0(Vec3 *, Vec3 *, Vec3 *);
    float sub_10034760(Vec3 *, Vec3 *);
    void sub_10034560(Vec3 *, Vec3 *, Vec3 *);
    void sub_100344D0(Vec3 *);
    void sub_10034620(Vec3 *, Vec3 *, Vec3 *, float);
    void sub_100342B0(Vec3 *, Vec3 *, Vec3 *);
    void sub_10034390(Vec3 *, float);
    void sub_10060E10();
    void sub_100611F0(int, Vec3 *, void *);
    void sub_10061280(int, Vec3 *, void *);
    void sub_10060F40();
    void sub_10019890();
    int  sub_10013F20();
    void sub_10033C90(void *);
    void sub_10004F90();
    void sub_10005400();
    void sub_10019A10();
    void sub_1006C460();
    void sub_10013F00();
    void sub_10002F10();
    void sub_10037180();
    void sub_10006430();
    void sub_10059E50();
    void sub_10059DE0();
    void sub_10059E30();
    void sub_10059DC0();
    int  sub_10018310();
    void sub_10019980();
    int  sub_10018340();
    void sub_1006B4F0(int);
    void sub_1002CB3F();
    void sub_1001C9D0();
    void sub_1002E317(void (*)());
    void sub_1006A070();
    void sub_10019900();
    void sub_1001C890();
    void sub_10002460();
    void sub_10032680();
    void sub_100325B0(int);
    void sub_100609F0();
    void sub_10002EB0();
    void sub_1006BD70();
    void sub_10061440();
    void sub_10060A10();
    void sub_10006460();
    void sub_10072210(int, int, int);
    void sub_1006BDD0();
    void sub_10063CC0();
    void sub_10063AD0();
    void sub_10014CB0();
    void sub_1006E3B0();
    void sub_100064D0();
    void sub_1006E360();
    int  sub_100131E0(int);
    void sub_100023F0(char *, float);
}

extern void (*g_B7352C)();
extern void (*g_18ED1E8)();
extern void (*g_0B849C)();
extern void (*g_B73528)();
extern int  (*g_18ED1C4)(char *, unsigned short *, int, int, int, int, int, int, int,
                         int, int, int, int, int, int);
extern int  (*g_18ED1C0)(char *, char *, unsigned short *, int, int, int, int, int);
extern int  g_18ED1B4;

extern Car     g_AF1208[16];
extern Player  g_AF07F8[20];
extern View    g_6E86B8[];
extern Tex     g_0BCDD0[16];
extern Snap    g_396F4C[];
extern Ctl     g_6ED708[2];
extern Obj    *g_6EED38;
extern Special g_6EEE3C[];
extern Track  *g_0BCAB0[];
extern Phase   g_0A9578[];
extern Hold    g_0A9368[];
extern Ent18   g_18EEF40[15];
extern int     g_18EEF2C;
extern char    g_1778850[][0xf000];
extern Mgr     g_B71290;
extern char    g_B72F48[];
extern char    g_B71648[];

extern unsigned int g_5CCB90, g_5BCAF0, g_5CCBA4;
extern int   g_0A935C, g_0A9358, g_5BC900[], g_5BCAE4, g_5CCB7C;
extern int   g_5CCB94, g_6ED684, g_5CCB88, g_0A9360, g_0B3014, g_5CCB8C;
extern int   g_0BCBE8, g_1778848, g_226E7C, g_0B2F00, g_0B2F04, g_226A4C;
extern int   g_0B3858, g_226A48, g_5BC810, g_4B15E8, g_5BC760, g_5CCB9C;
extern int   g_5BCAE0, g_226E80, g_5BC7B8, g_5BC8D8, g_6EA3F4, g_6EEEFC;
extern int   g_5BCAE8;
extern int   g_5BC778[], g_0AA044, g_B71530, g_5CCB60, g_0A6B68[];
extern int   g_5BC750, g_5BC8F8, g_18EE588, g_6ED6AC, g_5CCB5C, g_5CCB98;
extern int   g_5BC888, g_5BC768, g_6ED6D8, g_226A44, g_5CCB58, g_5CCB78;
extern int   g_18EEED8, g_5CCB74, g_6ED6B0, g_6ED6B4, g_0A5EA8, g_5BC76C;
extern int   g_5CCBA0, g_18EEBE8, g_5BC8DC, g_5CCB64, g_5CCB80, g_226A50;
extern int   g_184C454, g_5CCB70, g_5BC818[], g_5BC890[], g_6EC760, g_B71A68;
extern int   g_B1CF10;
extern int   g_HACK;
extern int   g_6E9A34, g_B71A6C, g_0B55F0, g_0A95B8[];
extern char  g_0BB2E0, g_17A613C;
extern short g_17A5F20;
extern float g_0A9548[], g_6E9D8C;
extern float g_5BC880, g_5BC884, g_5BC764;
struct Air {
    int   obj, trig, lap, active, seg, nseg;
    float scale;
    Vec3 *left, *right;
    float t, len;
    Vec3  dirPrev, dir, dirNext;
};
extern Air   g_5BC7C0;
extern int   g_5BC8E0, g_5BC8E4, g_5BC8E8, g_5BC8EC;
extern char  g_5BCAF8[], g_17A5910[];
extern char *g_5BCAEC;
extern char  g_6E7970[];

extern "C" { void sub_1002ECEB(char *); }
/* The gated scene draw (0x10019840, claimed in br_drawgated.c): draws the
 * scene only while drawing is switched on, zeroing one setting across the draw
 * in the -1 mode. Carried here for the unit's codegen; see the file header. */
extern "C" void BrRaceDrawGated(void)
{
    float saved;

    if (g_5CCB58) {
        if (g_5CCB58 == -1) {
            saved = g_6E9D8C;
            g_6E9D8C = 0;
        }
        sub_1002ECEB(g_5BCAEC);
        if (g_5CCB58 == -1)
            g_6E9D8C = saved;
    }
}

/* WHAT IT DOES: the per-frame race step. Measures the frame time into a
 * small ring and the race clock; on the first frame sets up the scene, audio
 * and frame limiter; then runs the race state machine (intro, credits, outro,
 * race), builds and draws each driver's view, steps the specials, waterfalls
 * and the airplane, handles the HUD, pause and camera keys, advances or ends
 * the replay, and finally holds the frame until its time slot comes round,
 * pumping messages while it waits. */
/* @implements 0x10019A70 glide BrRaceStep
 * @cpp_symbol _BrRaceStep */
extern "C" void BrRaceStep(void)
{
    int i, j, k;
    int newMode;
    int flag;

    int frame;
    float t;
    unsigned int now;
    int delta;

    now = sub_1006E280();
    delta = now - g_5CCB90;
    g_5CCB90 = now;
    if (g_0A935C < 0) {
        g_0A935C = 0;
        for (i = 0; i < g_0A9358; i++) {
            g_5BC900[i] = delta;
            g_0A935C++;
        }
    }
    if (++g_5BCAE4 != 0)
        g_5CCB7C += delta;
    else
        g_5CCB7C = 0;
    if (++g_0A935C >= g_0A9358)
        g_0A935C = 0;
    g_5BC900[g_0A935C] = delta;

    if (g_5CCB94 == 0) {
        g_6ED684 = 1;
        sub_10008D60(1);
        sub_1000CB80();
        if (!g_5CCB88) {
            sub_10008D60();
            sub_10008D60();
            g_B7352C();
            g_18ED1E8();
            g_0B849C();
            sub_10031140(g_0A9360 == 5 ? 12 : g_0B3014);
            sub_1006E030();
            g_B73528();
            sub_100189C0();
        }
        sub_1002BF24(&g_6E86B8[0]);
        sub_10008D60(1);
        sub_1002BF24(&g_6E86B8[0]);
        sub_100353C0(0x7b);
        g_5CCB8C = 0;
        if (g_0A9360 != 1 && g_0A9360 != 6 && g_0A9360 != 2)
            g_0BCBE8 = 3;
        g_1778848 = 0;
        switch (g_0A9360) {
        case 0:
            g_0B2F00 = 20;
            g_0B2F04 = 3;
            g_AF1208[0].m_1006FD50(g_226E7C);
            goto race;
        case 1:
            g_0B2F00 = 2;
            g_0B2F04 = 2;
            g_AF1208[0].m_1006FD50(g_226E7C);
            g_AF1208[1].m_1006FD50(g_226E7C);
            g_AF1208[1].f29B4 = 0;
            goto race;
        case 6:
            if (!g_5CCB88) {
                g_0B3858 = 1;
                if (g_226A4C) {
                    g_0B2F00 = 0;
                    g_0B2F04 = 0;
                } else {
                    g_0B2F00 = 1;
                    g_0B2F04 = 1;
                }
                g_AF1208[0].m_1006FD50(g_226E7C);
                if (g_226A48) {
                    sub_10005CD0();
                    sub_10006400();
                    sub_10004E00();
                    g_AF1208[0].f144 = sub_100060A0();
                    strcpy(g_AF1208[0].name, g_B71648);
                    sub_10006250(g_AF1208[0].f144, g_B71648);
                }
                while ((unsigned)g_0B2F04 < sub_10009BA0()) {
                    int r = sub_100060B0();
                    if (r >= 0)
                        sub_1001C6A0(r);
                }
            }
        race:
            if (g_5CCB88) {
                sub_100609D0(&g_0BCDD0[15 - g_5BC810]);
                for (i = 0; i < 15; i++) {
                    g_18EEF40[i].f14 = 0;
                    g_18EEF40[i].f10 = 0;
                }
            } else {
                for (i = 0; i < g_0B3858; i++) {
                    g_AF1208[i].pCtl->info = 0;
                    g_AF1208[i].pCtl->buf[0] = 0;
                    g_AF1208[i].pCtl->buf[1] = 0;
                }
            }
            g_5CCB8C = g_5CCB88 == 0;
            break;
        case 5:
            g_0B3858 = 1;
            g_0B2F00 = 1;
            g_0B2F04 = 1;
            g_5CCB8C = 0;
            g_5CCB88 = 0;
            g_AF1208[0].fE88 = 0;
            g_AF1208[0].f2734 = &g_AF1208[0].mode[1];
            g_B1CF10 = 0;
            break;
        case 4:
            g_4B15E8 = 1;
            g_0B3858 = 1;
            g_0B2F00 = 1;
            g_0B2F04 = 1;
            g_5CCB8C = 0;
            g_AF1208[0].fE88 = 0;
            g_AF1208[0].pCtl->info = (signed char *)&g_5BC8E0;
            switch (g_5BC760) {
            case 0:
                if (g_5CCB9C == 0)
                    sub_10069A80("RallyIntro1.dat", 0);
                else
                    sub_10069A80("RallyIntro2.dat", 0);
                if (++g_5CCB9C > 1)
                    g_5CCB9C = 0;
                break;
            case 1:
                sub_10069A80("RallyCredits.dat", 0);
                break;
            case 2:
                sub_10019930();
                sub_10069A80("RallyOutro.dat", 0);
                break;
            default:
                goto car0;
            }
            sub_10063B60();
            g_5BCAE0 = 0;
        car0:
            g_AF1208[0].pCtl->buf[0] = 0;
            g_AF1208[0].pCtl->buf[1] = 0;
            g_AF1208[0].b29AC = -1;
            g_AF1208[0].b29AD = -1;
            g_AF1208[0].b29AE = -1;
            g_0B3014 = g_AF1208[0].pCtl->info[0];
            g_AF1208[0].m_1006FD50(g_AF1208[0].f29A4 = g_AF1208[0].pCtl->info[1]);
            g_AF1208[0].fE98 = g_AF1208[0].pCtl->info[2];
            g_AF1208[0].fE9C = g_AF1208[0].pCtl->info[3];
            g_AF1208[0].fE90 = g_AF1208[0].pCtl->info[4];
            g_AF1208[0].fE94 = g_AF1208[0].pCtl->info[5];
            g_AF1208[0].pCtl->b25 = g_AF1208[0].pCtl->info[6];
            g_4B15E8 = g_226E80 = g_AF1208[0].pCtl->info[7];
            break;
        case 2:
            g_AF1208[0].m_1006FD50(g_226E7C);
            g_5CCB8C = 0;
            g_5BC7B8 = 1;
            g_0B2F00 = g_0B3858 + 1;
            g_0B2F04 = g_0B3858 + 1;
            g_AF1208[g_0B3858].pCtl->info = (signed char *)g_1778850[0];
            sub_10008D60("savedRecordingLength = %d\n", g_5BC8D8);
            memcpy(g_AF1208[g_0B3858].pCtl->info, &g_5BC8E0, g_5BC8D8);
            g_AF1208[g_0B3858].m_1006FD50(g_AF1208[g_0B3858].f29A4 = g_AF1208[g_0B3858].pCtl->info[1]);
            sub_10063B60();
            if (g_AF1208[g_0B3858].pCtl->info[0] == g_0B3014 &&
                g_AF1208[g_0B3858].pCtl->info[7] == g_226E80) {
                sub_10008D60("TRACK=%d\n", g_AF1208[g_0B3858].pCtl->info[0]);
                g_AF1208[g_0B3858].pCtl->infoType = 8;
                sub_10008D60("PLAYLIMIT=%d\n", g_5BC8D8);
                g_AF1208[g_0B3858].pCtl->infoLen = g_5BC8D8;
            } else {
                sub_10008D60("WRONG TRACK (%d!=%d)\n", g_AF1208[g_0B3858].pCtl->info[0], g_0B3014);
                g_AF1208[g_0B3858].pCtl->info = 0;
                g_0B2F04 = 1;
                g_0B2F00 = 1;
            }
            {
            int q;
            for (q = 0; q < g_0B3858; q++) {
                g_AF1208[q].pCtl->buf[q] = g_1778850[q + 1];
                g_AF1208[q].pCtl->buf[q][0] = (char)g_0B3014;
                g_AF1208[q].pCtl->buf[q][1] = (char)g_AF1208[q].f29A8;
                g_AF1208[q].pCtl->buf[q][2] = (char)g_AF1208[q].pEquip->fF8;
                g_AF1208[q].pCtl->buf[q][3] = (char)g_AF1208[q].pEquip->fFC;
                g_AF1208[q].pCtl->buf[q][4] = (char)g_AF1208[q].pEquip->f100;
                g_AF1208[q].pCtl->buf[q][5] = (char)g_AF1208[q].pEquip->f104;
                g_AF1208[q].pCtl->buf[q][6] = (char)g_AF1208[q].pEquip->f108;
                g_AF1208[q].pCtl->buf[q][7] = (char)g_226E80;
                g_AF1208[q].pCtl->recIndex[q] = 8;
                g_AF1208[q].pCtl->recPrev[q] = 0xde5c;
            }
            }
            break;
        case 3:
        default:
            g_AF1208[0].m_1006FD50(g_226E7C);
            g_5CCB8C = 0;
            g_0B2F00 = g_0B3858;
            g_0B2F04 = g_0B3858;
            break;
        }

        if (g_0A9360 == 5)
            g_6EA3F4 = 0;
        else
            g_6EA3F4 = (g_0BCAB0[g_0B3014]->f4 >> 4) & 1;
        sub_100627B0(g_4B15E8);
        if (g_0A9360 != 4 && !g_5CCB88) {
            sub_10008D60();
            sub_10008D60();
        }
        sub_10061310();
        if (!g_5CCB88) {
            sub_100311C0(g_0A9360 == 5 ? 12 : g_0B3014);
            if (!g_5CCB88) {
                g_5BC7C0.active = 0;
                g_5BC7C0.trig = 0;
                g_5BC7C0.obj = 0;
                g_5BC7C0.right = 0;
                g_5BC7C0.left = 0;
                g_5BC7C0.t = 0.0f;
                g_5BC7C0.len = 0.0f;
                g_5BC7C0.seg = 0;
                g_5BC7C0.nseg = 0;
                sub_10008D60("specials: %d\n", g_6EEEFC);
                g_5BCAE8 = 0;
                for (i = 0; i < g_6EEEFC; i++) {
                    switch (g_6EEE3C[i].type) {
                    case 4:
                        g_5BC7C0.nseg = g_6EEE3C[i].b;
                        g_5BC7C0.left = (Vec3 *)g_6EEE3C[i].a;
                        sub_10008D60("airplanePathLeft = %08x\n", g_6EEE3C[i].a);
                        break;
                    case 5:
                        g_5BC7C0.right = (Vec3 *)g_6EEE3C[i].a;
                        sub_10008D60("airplanePathRight = %08x\n", g_6EEE3C[i].a);
                        break;
                    case 3:
                        g_5BC7C0.obj = g_6EEE3C[i].a;
                        sub_10008D60("airplane = %d\n", g_6EEE3C[i].a);
                        break;
                    case 6:
                        g_5BC7C0.trig = g_6EEE3C[i].a;
                        sub_10008D60("airplanetrigger = %d\n", g_6EEE3C[i].a);
                        break;
                    case 7:
                        g_5BC778[g_5BCAE8++] = g_6EEE3C[i].a;
                        sub_10008D60("waterfall = %d\n", g_6EEE3C[i].a);
                        break;
                    }
                }
                if (!g_5BC7C0.trig || !g_5BC7C0.left || !g_5BC7C0.right)
                    g_5BC7C0.obj = 0;
                if (g_5BC7C0.obj) {
                    Vec3 v;
                    v.x = 1.0f;
                    v.y = 0.0f;
                    v.z = 0.0f;
                    sub_10034870(&v, &v, &g_6EED38[g_5BC7C0.obj]);
                    g_5BC7C0.scale = sub_100347F0(&v);
                }
                g_5BC7C0.lap = g_0BCBE8 - 1;
            }
        }
        g_0AA044 = g_5CCB88 ? 1 : g_0B3858;
        g_6E86B8[0].car = g_5CCB88 ? g_5BC810 : 0;
        g_6E86B8[1].car = g_6E86B8[0].car == 0;
        if (g_0A9360 == 4) {
            g_AF1208[0].f2734 = &g_AF1208[0].mode[1];
            g_B1CF10 = 180;
        } else {
            for (i = 0; i < g_0B3858; i++) {
                g_AF1208[i].fE9C = g_AF1208[i].pEquip->fFC;
                g_AF1208[i].fE94 = g_AF1208[i].pEquip->f104;
                g_AF1208[i].fE90 = g_AF1208[i].pEquip->f100;
                g_AF1208[i].fE98 = g_AF1208[i].pEquip->fF8;
                switch (g_B71530) {
                case 1:
                    g_AF1208[i].pCtl->b25 = 5;
                    break;
                case 2:
                    g_AF1208[i].pCtl->b25 = 5;
                    break;
                case 3:
                    g_AF1208[i].pCtl->b25 = 5;
                    break;
                default:
                    g_AF1208[i].pCtl->b25 = 2;
                    break;
                }
            }
            if (g_0A9360 == 2 && g_AF1208[1].pCtl->info) {
                g_AF1208[1].fE98 = g_AF1208[1].pCtl->info[2];
                g_AF1208[1].fE9C = g_AF1208[1].pCtl->info[3];
                g_AF1208[1].fE90 = g_AF1208[1].pCtl->info[4];
                g_AF1208[1].fE94 = g_AF1208[1].pCtl->info[5];
                g_AF1208[1].pCtl->b25 = g_AF1208[1].pCtl->info[6];
            } else {
                for (; i < g_0B2F04; i++) {
                    g_AF1208[i].fE9C = 1;
                    g_AF1208[i].fE94 = 1;
                    g_AF1208[i].fE90 = 2;
                    g_AF1208[i].fE98 = 0;
                    g_AF1208[i].pCtl->b25 = 0;
                }
            }
        }
        if (!g_5CCB88) {
            g_5CCB60 = 0;
            for (i = 0; i < g_0B2F04; i++) {
                g_AF1208[i].f29B0 = 1.0f;
                if (i < g_0B3858) {
                    g_AF1208[i].pfnControl = sub_1005D050;
                } else if (g_0A9360 == 2) {
                    g_AF1208[i].pfnControl = sub_1005D050;
                    g_AF1208[i].b29AF = (char)g_0A9360;
                    g_AF1208[i].f29B0 = 0.375f;
                    goto wired;
                } else if (g_0A9360 == 6) {
                    g_AF1208[i].pfnControl = sub_100199A0;
                } else {
                    g_AF1208[i].m_1005C490();
                    g_AF1208[i].pfnControl = sub_1005E690;
                }
                g_AF1208[i].b29AF = 0;
            wired:
                if (!g_AF1208[i].fE88)
                    g_AF1208[i].m_1006FCE0(i, g_AF1208[i].f29A8);
                g_AF1208[i].m_1005E7B0();
                g_AF1208[i].fF7C = 0;
                g_AF1208[i].fFFC = 0;
                g_AF1208[i].f1004 = 0;
            }
            if (g_0B2F04 == 0) {
                memset(&g_0BCDD0[0], 0, sizeof g_0BCDD0[0]);
                g_AF1208[0].m_1006FCB0(0);
                g_AF1208[0].m_1005E7B0();
                g_AF1208[0].m_10001CF0();
                g_AF1208[0].mode[0].f38 -= -1.0f;
            }
            for (k = 0; k < g_0AA044; k++) {
                Tex *s = &g_0BCDD0[g_6E86B8[k].car];
                unsigned short *pal = g_4B15E8 == 4 ? s->pal4 : s->pal;
                char *dst, *src;
                int n;
                g_6E86B8[k].h[0] = g_18ED1C4(s->data, pal, s->w, s->h, s->w, 1, 2, 0, 0, 1, 1, 0, 0, 0, 0);
                if (s->kind == 1 || s->kind == 2) {
                    g_18ED1B4 = 1;
                    for (j = 1; j < 16; j++) {
                        if (s->kind == 1) {
                            unsigned short v = ((pal[256 - j] & 0x3000 | 0x800) >> 11) | ((pal[256 - j] & 0xc6) << 5);
                            pal[256 - j] = (v >> 8) | (v << 8);
                        } else if (s->kind == 2) {
                            pal[256 - j] &= 0xfeff;
                        }
                        g_6E86B8[k].h[j] = g_18ED1C4(s->data, pal, s->w, s->h, s->w, 1, 2, 0, 0, 1, 1, 0, 0, 0, 0);
                    }
                    g_18ED1B4 = 0;
                }
                g_6E86B8[k].h[16] = g_18ED1C4(s->data + s->w * s->h, pal, s->mw, s->mh, s->mw, 1, 2, 0, 0, 1, 1, 0, 0, 1, 0);
                src = s->data;
                n = (s->nMip + 2) * s->mw * s->mh;
                dst = (char *)s + 0x8000 - n;
                memmove(dst, s->data + s->w * s->h, n);
                for (j = 0; j < s->nMip + 2; j++) {
                    src += s->used = g_18ED1C0(src, dst, pal, s->mw, s->mh, s->mw, 1, 2);
                    if (src > dst)
                        sub_10008EF0(sub_1006D280(300));
                    dst += s->mw * s->mh;
                    if (src - (char *)s > 0x8000)
                        sub_10008EF0(sub_1006D280(301));
                }
                g_0A6B68[k] = -1;
            }
        } else {
            /* An empty walk over the non-player entities: the original
             * keeps its first store of k and nothing else. */
            for (k = g_0B3858; k < g_0B2F04; k++)
                ;
        }
        j = g_5CCB88 ? 4 : (g_0A9360 == 5 ? 4 : 0);
        g_5BC880 = g_0A9578[j].t;
        g_5BC8F8 = g_0A9578[j].phase;
        g_5BC750 = j;
        if (!g_5CCB88) {
            g_18EE588 = 0;
            for (i = 0; i < g_0B2F00; i++)
                g_AF07F8[i].m_1005F310();
        }
        sub_10062830(g_4B15E8);
        if (!g_6ED6AC && (g_0B3014 == 0 || g_0B3014 == 6))
            g_5BC7C0.obj = 0;
        sub_100181A0(1.0f, 0.2f);
        sub_10018230(1.0f, 0.2f);
        sub_10018290(1.0f, 0.2f);
        g_5CCB94 = 1;
        if (!g_5CCB88) {
            g_5BCAEC = g_5BCAF8;
            sub_10030270(g_5BCAF8, "misc\\modelLights.blob", g_17A5910);
            sub_1002ECAC(g_5BCAEC);
        }
        sub_10033B50();
        sub_10063DD0();
        g_5CCB5C = 0;
        g_5CCB98 = 0;
        g_5BC888 = -1;
        g_5BC884 = 0.0f;
        g_5BC768 = 0;
        g_6ED6D8 = 1;
        if (!g_5CCB88) {
            sub_1002E13B();
            sub_10060E30();
        }
        g_6ED684 = 0;
        if (g_5CCB88) {
            sub_10063B60();
        } else {
            sub_10063A00();
            if (g_0A9360 == 4)
                sub_10063A40();
            if (g_0BB2E0) {
                if (g_0A9360 == 4 && g_5BC760 == 2)
                    sub_10002AF0(12);
                else if (g_0A9360 == 4 && g_5BC760 == 1)
                    sub_10002AF0(13);
                else
                    sub_10002AF0(sub_10002C00());
                sub_10002D30(g_0BB2E0);
            }
        }
        sub_10013E80();
        sub_10008D60();
        sub_10019A40();
    } else if (g_226A48) {
        int r = sub_10006100();
        if (r >= 0 && !g_5CCB88)
            sub_1001C7A0(r);
    }

    g_17A5F20 = 0;
    if (g_5BC8F8 == 4)
        for (i = 0; i < g_0B3858; i++)
            g_AF1208[i].m_10060A30();
    if (g_0A9360 != 4 && !g_5CCB5C)
        g_1778848 ^= 1;
    sub_1002E186();
    g_5CCB58 = 0;
    newMode = g_5CCB5C;
    g_5CCB78 = 0;
    if (g_5BC8F8 < 3) {
        if (g_226A44) {
            for (i = 0; i < g_0B3858; i++) {
                g_AF1208[i].f1004 = 0;
                g_AF1208[i].f1008 = 0.01f;
            }
        } else if (g_226A48 && g_18EEED8) {
            for (i = 0; i < g_0B3858; i++) {
                g_AF1208[i].f1004 = sub_1006D280(237);
                g_AF1208[i].f1008 = 0.01f;
            }
        } else {
            for (i = 0; i < g_0B3858; i++) {
                g_AF1208[i].f1004 = sub_1006D280(238);
                g_AF1208[i].f1008 = 0.01f;
            }
        }
        g_5CCB78 = 1;
        for (i = 0; i < g_0B2F00; i++) {
            Car *car;
            g_AF07F8[i].flags |= 1;
            car = g_AF07F8[i].car;
            if (car && car->f140 < g_0B3858) {
                if (g_0A9360 == 1 || g_0A9360 == 6) {
                    short n = g_4B15E8 - 1;
                    if (n > 2 || n < 0)
                        n = 0;
                    car->fFF0 = g_0BCAB0[g_0B3014]->row[car->fE64 * 3 + n].v;
                }
                car->f1000 = 1.0f;
                switch (g_5BC8F8) {
                case 0:
                    g_5CCB58 = -1;
                    g_5CCB74 = 0;
                    break;
                case 2:
                    g_5CCB58 = 1;
                    if (g_0A9548[g_5CCB74] > g_5BC880) {
                        if (++g_5CCB74 == 4)
                            sub_10060E00();
                        else
                            sub_10060DF0();
                    }
                    break;
                }
            }
        }
        g_5BC764 = 0.0f;
        goto count;
    } else if (g_5BC8F8 == 3) {
        g_5CCB58 = 1;
        g_5CCB78 = 1;
        for (i = 0; i < g_0B2F00; i++)
            g_AF07F8[i].flags &= ~1;
        t = (g_0A9578[g_5BC750].t - g_5BC880) / g_0A9578[g_5BC750].t;
        g_5BC764 = t * t * 15.0f;
        goto count;
    } else if (g_5BC8F8 == 4) {
        flag = 1;
        if ((g_0A9360 == 4 && (g_5BC760 == 2 && g_AF1208[0].tFinal > 140.0f ||
                               g_5BC760 != 2 && g_AF1208[0].tFinal > 39.6f)) ||
            (g_0A9360 == 5 && g_AF1208[0].tFinal > 16.0f)) {
            if (!sub_10018310()) {
                sub_100181A0(0.0f, 0.2f);
                sub_10018230(0.0f, 0.2f);
                g_5CCB98 = 1;
                g_5CCB88 = 0;
                g_5CCB8C = 0;
                flag = 0;
            }
        }
        for (i = 0; i < g_0B3858; i++) {
            if (g_AF07F8[i].flags & 2) {
                if (g_0A9360 == 2 && g_AF1208[i].pCtl->buf[i]) {
                    if (i == 0 &&
                        !(g_AF1208[0].fFF8 && (signed char)g_5BC8E0 == g_0B3014 && g_5BC8D8 > 8)) {
                        sub_10008D60("%d: %d<%d=%d || %d!=%d=%d || %d<=%d=%d\n", 0,
                                     g_AF1208[0].pCtl->recIndex[0], g_5BC8D8,
                                     g_AF1208[0].pCtl->recIndex[0] < g_5BC8D8,
                                     (signed char)g_5BC8E0, g_0B3014,
                                     (signed char)g_5BC8E0 != g_0B3014,
                                     g_5BC8D8, 8, g_5BC8D8 <= 8);
                        g_5BC8D8 = 16;
                        memcpy(&g_5BC8E0, g_AF1208[0].pCtl->buf[0], 8);
                        memset(&g_5BC8E8, 0, 8);
                        g_AF1208[0].fFFC = sub_1006D280(239);
                        g_AF1208[0].f1000 = 1.0f;
                        sub_100023F0(g_AF1208[0].sz100C, g_AF1208[0].tFinal);
                        g_AF1208[0].f1004 = (int)g_AF1208[0].sz100C;
                        g_AF1208[0].f1008 = 1.0f;
                        g_5BC7B8 = 1;
                    }
                    g_AF1208[i].pCtl->buf[i] = 0;
                } else if (g_0A9360 == 1 || g_0A9360 == 6) {
                    for (j = 0; j < g_0B3858; j++)
                        g_AF1208[i].pCtl->recPrev[j] = g_AF1208[i].pCtl->recIndex[j];
                }
            } else {
                flag = 0;
            }
        }
        if (flag)
            goto next;
        goto step;
    } else if (g_5BC8F8 == 5) {
        if ((g_0A9360 == 2 || g_0A9360 == 1 || g_0A9360 == 0 || g_0A9360 == 6) && !g_5CCB88)
            sub_1001C810();
        goto next;
    } else if (g_5BC8F8 == 6) {
    count:
        if (!g_5CCB5C && (g_5BC880 -= g_6E9D8C) < 0.0f) {
    next:
            if (g_226A44) {
                g_5BC750++;
                g_5BC880 = g_0A9578[g_5BC750].t;
                g_5BC8F8 = g_0A9578[g_5BC750].phase;
            }
        }
    } else if (g_5BC8F8 == 7) {
        if (!g_5CCB98) {
            sub_10018230(0.0f, 0.2f);
            if (!g_5CCB8C)
                sub_10018290(0.0f, 0.2f);
            sub_100181A0(0.0f, 0.2f);
            g_5CCB98 = 1;
        }
    }
step:
    sub_10008D60(0, 0, 0, 200, 255);
    sub_1005C450();
    for (i = 0; i < g_0B2F04; i++)
        g_AF1208[i].m_10061430();
    for (i = 0; i < g_0B2F00; i++)
        g_AF07F8[i].m_10061F60();
    if (g_226A48 || g_5CCB88)
        for (i = 0; i < g_0B2F00; i++)
            g_AF07F8[i].m_100623A0();
    for (i = 0; i < g_0B2F00; i++)
        g_AF07F8[i].m_100623E0();
    if (g_0A9360 == 0)
        for (i = 0; i < g_0B3858; i++)
            g_AF1208[i].m_1005F6C0();
    sub_1005F580();
    if (!g_5CCB5C && g_5CCB88 != 2) {
        sub_10008D60(0, 0x80, 0x80, 0, 255);
        sub_10033BB0();
        sub_10008D60(0, 0, 255, 255, 255);
        sub_10016C90();
        if (g_5BC7C0.obj && !g_5BC7C0.active &&
            !((g_6ED6B4 || g_6ED6B0 || g_6ED6AC) && (g_0B3014 == 2 || g_0B3014 == 8))) {
            for (i = 0; i < g_0B3858; i++) {
                for (j = 0; j < g_AF1208[i].n294C; j++) {
                    if (g_AF1208[i].lap == g_5BC7C0.lap && g_AF1208[i].u290C[j] == g_5BC7C0.trig) {
                        g_5BC7C0.active = 1;
                        break;
                    }
                }
            }
        }
        for (i = 0; i < g_6EEEFC; i++) {
            switch (g_6EEE3C[i].type) {
            case 0:
                sub_1002A590(g_6E7970, g_6EEE3C[i].b, 0.0f, 0.0f, 1.0f);
                goto spin;
            case 1:
                sub_1002A590(g_6E7970, g_6EEE3C[i].b, 1.0f, 0.0f, 0.0f);
                goto spin;
            case 2:
                sub_1002A590(g_6E7970, g_6EEE3C[i].b, 0.0f, 1.0f, 0.0f);
            spin:
                sub_10029D70(g_6E7970, &g_6EED38[g_6EEE3C[i].a], &g_6EED38[g_6EEE3C[i].a]);
                g_6EED38[g_6EEE3C[i].a].flags &= ~0x2000;
                break;
            case 3:
                if (g_5BC7C0.obj && g_5BC7C0.active) {
                    Vec3 a, b, c;
                    switch (g_0B3014) {
                    case 1:
                    case 7:
                        g_5BC7C0.t -= g_6E9D8C * -18.0f;
                        break;
                    default:
                        g_5BC7C0.t -= g_6E9D8C * -50.0f;
                        break;
                    }
                    while (g_5BC7C0.t > g_5BC7C0.len) {
                        g_5BC7C0.t -= g_5BC7C0.len;
                        if (++g_5BC7C0.seg >= g_5BC7C0.nseg) {
                            g_5BC7C0.obj = 0;
                            break;
                        }
                        sub_100346D0(&a, &g_5BC7C0.left[g_5BC7C0.seg - 1], &g_5BC7C0.right[g_5BC7C0.seg - 1]);
                        sub_100346D0(&b, &g_5BC7C0.left[g_5BC7C0.seg], &g_5BC7C0.right[g_5BC7C0.seg]);
                        g_5BC7C0.len = sub_10034760(&a, &b);
                        sub_10034560(&g_5BC7C0.dir, &b, &a);
                        sub_100344D0(&g_5BC7C0.dir);
                        if (g_5BC7C0.seg < 2) {
                            g_5BC7C0.dirPrev = g_5BC7C0.dir;
                        } else {
                            sub_100346D0(&c, &g_5BC7C0.left[g_5BC7C0.seg - 2], &g_5BC7C0.right[g_5BC7C0.seg - 2]);
                            sub_10034560(&g_5BC7C0.dirPrev, &a, &c);
                            sub_100344D0(&g_5BC7C0.dirPrev);
                        }
                        if (g_5BC7C0.seg + 1 == g_5BC7C0.nseg) {
                            g_5BC7C0.dirNext = g_5BC7C0.dir;
                        } else {
                            sub_100346D0(&c, &g_5BC7C0.left[g_5BC7C0.seg + 1], &g_5BC7C0.right[g_5BC7C0.seg + 1]);
                            sub_10034560(&g_5BC7C0.dirNext, &c, &b);
                            sub_100344D0(&g_5BC7C0.dirNext);
                        }
                    }
                    if (g_5BC7C0.obj) {
                        t = g_5BC7C0.t / g_5BC7C0.len;
                        sub_10034620(&a, &g_5BC7C0.left[g_5BC7C0.seg], &g_5BC7C0.left[g_5BC7C0.seg - 1], t);
                        sub_10034620(&b, &g_5BC7C0.right[g_5BC7C0.seg], &g_5BC7C0.right[g_5BC7C0.seg - 1], t);
                        sub_100346D0(&g_6EED38[g_5BC7C0.obj].pos, &a, &b);
                        if (t > 0.5f)
                            sub_10034620(&g_6EED38[g_5BC7C0.obj].r0, &g_5BC7C0.dirNext, &g_5BC7C0.dir, t - 0.5f);
                        else
                            sub_10034620(&g_6EED38[g_5BC7C0.obj].r0, &g_5BC7C0.dir, &g_5BC7C0.dirPrev, t - -0.5f);
                        sub_100344D0(&g_6EED38[g_5BC7C0.obj].r0);
                        sub_10034560(&g_6EED38[g_5BC7C0.obj].r2, &a, &b);
                        sub_100342B0(&g_6EED38[g_5BC7C0.obj].r1, &g_6EED38[g_5BC7C0.obj].r0, &g_6EED38[g_5BC7C0.obj].r2);
                        sub_100344D0(&g_6EED38[g_5BC7C0.obj].r1);
                        sub_100342B0(&g_6EED38[g_5BC7C0.obj].r2, &g_6EED38[g_5BC7C0.obj].r1, &g_6EED38[g_5BC7C0.obj].r0);
                        sub_100344D0(&g_6EED38[g_5BC7C0.obj].r2);
                        sub_10034390(&g_6EED38[g_5BC7C0.obj].r0, g_5BC7C0.scale);
                        sub_10034390(&g_6EED38[g_5BC7C0.obj].r2, -g_5BC7C0.scale);
                        sub_10034390(&g_6EED38[g_5BC7C0.obj].r1, g_5BC7C0.scale);
                    }
                }
                break;
            }
        }
    }
    for (i = 0; i < g_0B2F04; i++)
        g_AF1208[i].m_10061470();
    sub_10060E10();
    {
    int wk;
    for (wk = 0; wk < g_0AA044; wk++) {
        void *m = g_AF1208[g_6E86B8[wk].car].f2734;
        if (g_5BC7C0.obj && g_5BC7C0.active)
            sub_100611F0(g_5BC7C0.obj, &g_6EED38[g_5BC7C0.obj].pos, m);
        for (j = 0; m && j < g_5BCAE8; j++)
                sub_10061280(g_5BC7C0.obj, &g_6EED38[g_5BC778[j]].pos, m);
        sub_10008D60(m);
    }
    }
    sub_10060F40();
    sub_10019890();
    frame = sub_10013F20();
    for (i = 0; i < (g_0B2F04 ? g_0B2F04 : 1); i++) {
        g_396F4C[frame].cars[i] = g_AF1208[i];
        if (g_AF1208[i].f2734) {
            if (g_AF1208[i].f2734 == &g_AF1208[i].mode[0])
                g_396F4C[frame].cars[i].f2734 = &g_396F4C[frame].cars[i].mode[0];
            else if (g_AF1208[i].f2734 == &g_AF1208[i].mode[5])
                g_396F4C[frame].cars[i].f2734 = &g_396F4C[frame].cars[i].mode[5];
            else if (g_AF1208[i].f2734 == &g_AF1208[i].mode[1])
                g_396F4C[frame].cars[i].f2734 = &g_396F4C[frame].cars[i].mode[1];
            else if (g_AF1208[i].f2734 == &g_AF1208[i].mode[2])
                g_396F4C[frame].cars[i].f2734 = &g_396F4C[frame].cars[i].mode[2];
            else if (g_AF1208[i].f2734 == &g_AF1208[i].mode[3])
                g_396F4C[frame].cars[i].f2734 = &g_396F4C[frame].cars[i].mode[3];
            else
                g_396F4C[frame].cars[i].f2734 = 0;
        }
        if (g_AF1208[i].f2738) {
            if (g_AF1208[i].f2738 == &g_AF1208[i].mode[0])
                g_396F4C[frame].cars[i].f2738 = &g_396F4C[frame].cars[i].mode[0];
            else if (g_AF1208[i].f2738 == &g_AF1208[i].mode[5])
                g_396F4C[frame].cars[i].f2738 = &g_396F4C[frame].cars[i].mode[5];
            else if (g_AF1208[i].f2738 == &g_AF1208[i].mode[1])
                g_396F4C[frame].cars[i].f2738 = &g_396F4C[frame].cars[i].mode[1];
            else if (g_AF1208[i].f2738 == &g_AF1208[i].mode[2])
                g_396F4C[frame].cars[i].f2738 = &g_396F4C[frame].cars[i].mode[2];
            else if (g_AF1208[i].f2738 == &g_AF1208[i].mode[3])
                g_396F4C[frame].cars[i].f2738 = &g_396F4C[frame].cars[i].mode[3];
            else
                g_396F4C[frame].cars[i].f2738 = 0;
        }
    }
    for (i = 0; i < g_0B2F00; i++) {
        g_396F4C[frame].players[i] = g_AF07F8[i];
        if (g_AF07F8[i].car)
            g_396F4C[frame].players[i].car = &g_396F4C[frame].cars[g_AF07F8[i].car - g_AF1208];
    }
    g_396F4C[frame].view = g_6E86B8[0];
    sub_10033C90(g_396F4C[frame].x2C0E0);

    if (g_0A5EA8) {
        g_5BCAF0 = sub_1006E280();
        g_5BC76C = 0;
        g_0A5EA8 = 0;
        g_5CCBA0 = 0;
    } else
        {
        int ok = 1;
        g_5BCAF0 += g_0A95B8[g_5BC76C];
        if (++g_5BC76C > 2)
            g_5BC76C = 0;
        if (++g_5CCBA0 > 3) {
            ok = sub_100131E0(1);
            if (ok) {
                g_5CCBA0 = 0;
                g_5CCBA4 = sub_1006E280();
            }
        } else {
            now = sub_1006E280();
            while ((now < g_5BCAF0 || now > g_5CCBA4 + 333) && ok) {
                unsigned int t2;

                /* Both arms copy the clock into now. The copies vanish, but
                 * the else arm is a real block on the failed-pump path, and
                 * VC5 places the g_5CCBA4 reload there (merged with the one
                 * before the loop) instead of splitting the back edge. The
                 * split block would cut the zero constant's register web and
                 * recolour this whole tail. */
                ok = sub_100131E0(now > g_5CCBA4 + 333);
                t2 = sub_1006E280();
                if (ok) {
                    g_5CCBA0 = 0;
                    g_5CCBA4 = t2;
                    now = t2;
                } else {
                    now = t2;
                }
            }
        }
        }
    if (g_5CCB5C == 2) {
        if (g_18EEBE8 & 0x8000) {
            switch (g_5BC8DC) {
            case 0:
                sub_100181A0(0.0f, 0.2f);
                sub_10018230(0.0f, 0.2f);
                sub_10018290(0.0f, 0.2f);
                g_5CCB8C = 0;
                g_5CCB98 = 2;
                break;
            case 1:
                if (g_226A48 && g_226A44) {
                    sub_10004F90();
                } else {
                    sub_10018230(1.0f, 0.2f);
                    sub_10018290(1.0f, 0.2f);
                    g_5CCB64 = 2;
                    newMode = 0;
                }
                break;
            }
        }
        if (g_18EEBE8 & 0x1000)
            g_5BC8DC = 1 - g_5BC8DC;
        if (g_18EEBE8 & 0x2000)
            g_5BC8DC = 1 - g_5BC8DC;
        if (g_5CCB80) {
            g_5CCB80 = 0;
            sub_10018230(1.0f, 0.2f);
            sub_10018290(1.0f, 0.2f);
            g_5CCB64 = 2;
            newMode = 0;
        }
        if (g_226A48 && g_226A44)
            sub_10005400();
    } else if (g_5CCB5C) {
        for (i = 0; i < g_0B3858; i++) {
            if ((g_18EEBE8 & 0x8000) ||
                (g_5BC8DC == 0 && (g_18EEBE8 & 4) && !(g_18EEBE8 & 0x3000) && !(g_18EEBE8 & 3))) {
                if (g_5BC8DC || !(g_AF1208[i].pCtl->flags & 0x10))
                    g_AF1208[i].pCtl->m_1002F640(0xc010);
                switch (g_5BC8DC) {
                case 0:
                    if (g_226A48 && g_226A44) {
                        sub_10004F90();
                    } else {
                        sub_10018230(1.0f, 0.2f);
                        sub_10018290(1.0f, 0.2f);
                        g_5CCB64 = 2;
                        newMode = 0;
                    }
                    break;
                case 1:
                    sub_100181A0(0.0f, 0.2f);
                    g_5CCB98 = 0;
                    g_5CCB8C = 0;
                    g_5CCB94 = 0;
                    sub_10019A10();
                    g_226A44 = 0;
                    sub_1006C460();
                    sub_10013F00();
                    break;
                case 4:
                    sub_100181A0(0.0f, 0.2f);
                    sub_10018230(0.0f, 0.2f);
                    sub_10018290(0.0f, 0.2f);
                    g_5CCB8C = 0;
                    g_5CCB98 = 1;
                    if (g_0BB2E0) {
                        sub_10002F10();
                        sub_10002D30(g_0BB2E0);
                    }
                    if (g_0A9360 == 6) {
                        sub_10037180();
                        sub_10006430();
                    }
                    break;
                case 5:
                    newMode = 2;
                    g_5CCB5C = 2;
                    g_5BC8DC = 1;
                    break;
                }
            }
            if (g_5CCB80) {
                g_5CCB80 = 0;
                sub_10018230(1.0f, 0.2f);
                sub_10018290(1.0f, 0.2f);
                g_5CCB64 = 2;
                newMode = 0;
            }
            if (g_18EEBE8 & 0x1000) {
                g_5BC8DC = (g_5BC8DC + 5) % 6;
                if (g_226A48 && g_5BC8DC == 1)
                    g_5BC8DC = (g_5BC8DC + 5) % 6;
            }
            if (g_18EEBE8 & 0x2000) {
                g_5BC8DC = (g_5BC8DC + 1) % 6;
                if (g_226A48 && g_5BC8DC == 1)
                    g_5BC8DC = (g_5BC8DC + 1) % 6;
            }
            if (g_18EEBE8 & 1) {
                switch (g_5BC8DC) {
                case 2: sub_10059DE0(); break;
                case 3: sub_10059E50(); break;
                }
            }
            if (g_18EEBE8 & 2) {
                switch (g_5BC8DC) {
                case 2: sub_10059DC0(); break;
                case 3: sub_10059E30(); break;
                }
            }
        }
        if (g_226A48 && g_226A44)
            sub_10005400();
    } else if (g_0A9360 == 4 || g_0A9360 == 5) {
        if (!sub_10018310()) {
            for (i = 0; i < 2; i++) {
                if (g_18EEBE8 & 0x4000) {
                    g_6ED708[i].m_1002F640(0xc010);
                    g_5BCAE0 = 1;
                    g_5CCB98 = 1;
                    g_5CCB8C = 0;
                    sub_10018230(0.0f, 0.2f);
                    sub_100181A0(0.0f, 0.2f);
                }
            }
        }
    } else {
        for (i = 0; i < g_0B3858; i++) {
            if (g_226A50) {
                g_226A50 = 0;
                if (g_5CCB88) {
                    sub_100181A0(0.0f, 0.2f);
                    g_5CCB8C = 0;
                    g_5CCB98 = 1;
                } else {
                    newMode = 1;
                    g_5BC8DC = 0;
                }
                sub_10018230(0.0f, 0.2f);
                sub_10018290(0.0f, 0.2f);
                g_AF1208[0].pCtl->m_1002F640(0x4000);
                g_AF1208[1].pCtl->m_1002F640(0x4000);
                break;
            }
        }
    }

    if (g_0A9360 == 4) {
        if (g_5BC760 == 2)
            sub_10019980();
        if (g_0A9360 == 4 && g_5BC760 == 1 && g_0A9368[g_5BC768].on &&
            (g_5BC884 += g_6E9D8C) > g_0A9368[g_5BC768].t) {
            g_5BC884 = 0.0f;
            g_5BC768++;
        }
    }
    if (sub_10018310() && sub_10018340()) {
        for (i = 0; i < 16; i++)
            for (j = 0; j < g_0B3858; j++)
                g_AF1208[i].pCtl->recPrev[j] = g_AF1208[i].pCtl->recIndex[j];
        g_184C454 = 0;
        for (i = 0; i < 15; i++) {
            g_18EEF40[i].f14 = 0;
            g_18EEF40[i].f10 = 0;
            g_18EEF40[i].f8 = 0;
            g_18EEF40[i].fC = 0;
            g_18EEF40[i].p = &g_18EEF2C;
            sub_1006B4F0(i);
        }
        g_6ED6D8 = 0;
        if (g_5CCB98) {
            if (g_5CCB8C) {
                if (!g_5CCB88) {
                    g_5CCB70 = g_AF1208[0].pCtl->recIndex[0];
                    g_5BC810 = 0;
                    {
                    int bi;
                    for (bi = 1; bi < g_0B3858; bi++) {
                        if (g_5CCB70 > g_AF1208[bi].pCtl->recIndex[bi]) {
                            g_5CCB70 = g_AF1208[bi].pCtl->recIndex[bi];
                            g_5BC810 = bi;
                        }
                    }
                    }
                    {
                    int bj1;
                    for (bj1 = 0; bj1 < g_0B3858; bj1++) {
                        g_5BC818[bj1] = (int)g_AF1208[bj1].pCtl->buf[g_5BC810];
                        g_5BC890[bj1] = g_AF1208[bj1].pCtl->recIndex[g_5BC810];
                    }
                    }
                    {
                    int bj0;
                    for (bj0 = 0; bj0 < g_0B3858; bj0++) {
                        for (j = 0; j < g_0B3858; j++) {
                            sub_10008D60("veh[%d]->recIndex[%d] = %d\n", bj0, j, g_AF1208[bj0].pCtl->recIndex);
                            g_AF1208[bj0].pCtl->buf[j] = 0;
                        }
                    }
                    }
                    sub_10008D60("bestNumber = %d, bestIndex = %d\n", g_5BC810, g_5CCB70);
                }
                g_5CCB88 = 1;
            } else {
                sub_1002CB3F();
                g_6EA3F4 = 0;
                if (g_0A9360 == 5) {
                    if (g_AF1208[0].pEquip->b4) {
                        g_0A9360 = 0;
                        sub_1001C9D0();
                        g_17A613C = 1;
                        sub_1002E317(sub_1006A070);
                    } else {
                        sub_10019900();
                    }
                } else if (g_0A9360 == 4 && g_5BC760 == 2) {
                    sub_1002E317(sub_10032680);
                } else if (g_5CCB60 &&
                           (g_0A9360 == 2 || g_0A9360 == 1 || g_0A9360 == 0 || g_0A9360 == 6)) {
                    sub_1001C890();
                    sub_1002E317(sub_10002460);
                } else if (g_0A9360 == 4 && g_5BC760 == 1 ||
                           g_0A9360 == 4 && g_5BC760 == 0 && !g_5BCAE0) {
                    sub_1002E317(sub_10032680);
                } else {
                    if (g_5CCB98 == 2) {
                        if (g_6EC760 != g_B71A68 || g_6E9A34 != g_B71A6C)
                            g_B71290.m_100634B0(g_B72F48);
                        sub_100325B0(0);
                    }
                    sub_1002E317(sub_10032680);
                }
                g_5CCB88 = 0;
            }
            g_5CCB94 = 0;
            if (!g_5CCB88)
                sub_10019A10();
            sub_10013F00();
        }
        g_17A5F20 = 1;
        for (i = 0; i < 16; i++)
            g_AF1208[i].pCtl->info = 0;
    }
    if (g_5CCB5C != newMode) {
        if (newMode) {
            sub_100609F0();
            sub_10002EB0();
            sub_1006BD70();
            sub_10061440();
        } else {
            sub_10060A10();
            if (g_0BB2E0) {
                sub_10002F10();
                sub_10002D30(g_0BB2E0);
            }
        }
        g_5CCB5C = newMode;
    }
    if (g_0A9360 == 6)
        sub_10006460();
    sub_10072210(g_AF1208[0].b36D, g_AF1208[0].p168->f1B4 || g_AF1208[0].p16C->f1B4, g_AF1208[0].fE90);
    if (g_0B55F0 && !g_5CCB5C)
        sub_1006BDD0();
    if (g_5CCB88)
        sub_10063CC0();
    else
        sub_10063AD0();
    sub_10014CB0();
    sub_1006E3B0();
    if (!g_226A44) {
        if (g_226A48)
            sub_100064D0();
        sub_1006E360();
    }
}
