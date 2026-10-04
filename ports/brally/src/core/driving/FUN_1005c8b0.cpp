#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "br_race.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* BrDriverCar, the canonical record */
/* WHAT IT DOES: per-frame control step for one car from its input record:
 * squares the steering axis into +0x2720 with a dead zone (optionally
 * mirrored), then -- when the car is the local player's, or in the
 * spectator modes -- runs the checkpoint lock-on: finds the nearest
 * checkpoint ahead of the car and either latches it as the camera target
 * (+0x2808, raising the redraw flag when it changed) or, when nothing is
 * near, flips between the two fallback camera anchors on a 60-frame
 * timer; the four camera-select buttons override the target directly.
 * Squares the throttle axis into +0x2728 the same way, folds the digital
 * buttons into the three axis words (+0x2720/24/28/2C, differently when
 * the car is under +0xF7C control), and in game mode 5 recomputes the
 * look vector from two probe points on the car frame, scaling it b[1] the
 * +0xFF4 speed and stamping the two speed bits into the input flags. Then
 * the five keyboard latches, the controller poll, and -- when not under
 * external control -- the spring between the two +0x1038/+0x1044 frames
 * (over-extension warns the driver record), the +0x104C decay, and the
 * deferred reset request, before the respawn check. */
/* @t3 0x1005C8B0 2026-09-13 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1951/1951 insns 526/526 rows 0+0 regions 11 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * Residue: four register-colouring clusters over an identical instruction
 * stream (global load order, ext/flags eax<->edx, &f30/&fF24 ebx<->edi
 * with the rotation it seeds, one float-temp home). Dossier, levers and
 * the two ledger passes are in the block below.
 * Do not reopen before the end-grind. */
/* @implements 0x1005C8B0 glide FUN_1005c8b0
 * @cpp_kind method
 * @cpp_symbol ?Step@Car5C8B0@@QAEXXZ
 *
 * Thiscall, no stack args (`ret`), 1951 B. Callees: the car's own thiscall
 * methods (0x1005D3C0, 0x10001C90, 0x1006F170, SetVel 0x1006FA10, Respawn
 * 0x1005C6D0), the cdecl BrVec3 helpers, the input record's own thiscall
 * acknowledge method (0x1002F640, one stack arg, callee-cleaned -- its
 * `this` is why the record pointer is re-read after the 0x10001C90 call)
 * and two cdecl float helpers. Same object as 0x1005C6D0.cpp (Car5C6D0).
 *
 * Residue (128 positional diffs, 1951/1951 B, 526/526 insns, rows 0+0):
 * four register-colouring clusters, no instruction differs --
 *   (1) the two lock-on globals are loaded edi/edx in the other order
 *       (0x8d; the MINE test's registers themselves match);
 *   (2) ext/flags swap eax<->edx in the button block (0x3d0);
 *   (3) &f30 / &fF24 swap ebx<->edi in the game-mode-5 block (0x4da..0x566)
 *       and the eax/ecx/edx rotation that follows it (0x5aa..0x76c);
 *   (4) the spring block's float temp homes at [esp+0x10] instead of
 *       reusing dy's [esp+0x18] (0x6da).
 * Levers that landed on the way (all needed for rows 0): SQ(expr) over the
 * raw differences (a CSE temp homed and multiplied by its home) with the
 * z difference first and the two squares as separate statements; the sum
 * left UNNAMED in the condition (`fcom` keeps it for `best = ...`); the
 * latch triple as an array (aggregate slot between the spilled scalars
 * and vecA); the spring temp reusing `dy`; `p29C0->Ack()` as the input
 * record's own thiscall; signed `char` latches; `int` flags so the `& 1`
 * mask joins the 1-web; then-arm-first spellings of every float gate.
 *
 * @t4-pass 0x1005C8B0 1 2026-09-13 probes 18 bytes 1951 insns 526 regions 11 rows 0 census no  (generated: all 6 orders of {dz, dx2, dy2}, 4 sum shapes, spring temp as q/dx, 1<AA044, p30/pF24/fl/ext-late pointer and copy locals; best 122 = pF24, no region moved, none 0)
 * @t4-pass 0x1005C8B0 2 2026-09-13 probes 22 bytes 1951 insns 526 regions 11 rows 0 census yes  (22 compiler options incl. /Gi /Op /G3 /G4 /G5 /Ow /Ob1 /Ob2 /Ox /O1 /Oa /Os /Ot /Oy- /Za /Gf /Gy /Gr /Ge, all 128 or worse; corpus query MISS at +0x8d len 12; per-cluster registers named above)
 */
#define _CRTIMP __declspec(dllimport)

struct In5C8B0 {
    int      flags;                     /* +0x00 */
    char     pad04[0x1C - 4];
    float    f1C;                       /* +0x1C throttle axis */
    float    f20;                       /* +0x20 steering axis */

    void Ack(unsigned bit);             /* 0x1002F640, thiscall, one stack arg */
};

struct Drv5C8B0 {
    char          pad00[0x68];
    unsigned char b68;                  /* +0x68 */
};

struct Vec5C8B0 {
    float x, y, z;
};

class Car5C8B0 {
public:
    char       pad0000[0x10];
    float      f10[3];                  /* +0x010 */
    char       pad001C[0x30 - 0x1C];
    float      f30;                     /* +0x030 position */
    float      f34;
    float      f38;
    char       pad003C[0x140 - 0x3C];
    int        f140;                    /* +0x140 */
    char       pad0144[0x35C - 0x144];
    int        f35C;                    /* +0x35C reset flag */
    char       pad0360[0x366 - 0x360];
    char       b366;                 /* +0x366 keyboard latches */
    char       b367;
    char       b368;
    char       b369;
    char       b36A;
    char       pad036B[0xE20 - 0x36B];
    float      fE20;                    /* +0xE20 */
    char       pad0E24[0xF00 - 0xE24];
    Drv5C8B0  *pF00;                    /* +0xF00 driver record */
    char       pad0F04[0xF24 - 0xF04];
    float      fF24[3];                 /* +0xF24 */
    char       pad0F30[0xF4C - 0xF30];
    float      fF4C;                    /* +0xF4C */
    float      fF50;                    /* +0xF50 */
    char       pad0F54[0xF78 - 0xF54];
    int        fF78;                    /* +0xF78 redraw flag */
    int        fF7C;                    /* +0xF7C external control */
    char       pad0F80[0xFF4 - 0xF80];
    float      fFF4;                    /* +0xFF4 speed */
    char       pad0FF8[0x1038 - 0xFF8];
    float      f1038[3];                /* +0x1038 spring frame A */
    float      f1044[3];                /* +0x1044 spring frame B */
    float      f1050[3];                /* +0x1050 */
    char       pad105C[0x2720 - 0x105C];
    float      f2720;                   /* +0x2720 steering */
    float      f2724;
    float      f2728;                   /* +0x2728 throttle */
    float      f272C;
    char       pad2730[0x2734 - 0x2730];
    float     *p2734;                   /* +0x2734 camera target */
    char       pad2738[0x273C - 0x2738];
    float      f273C[3];                /* +0x273C anchor 0 */
    char       pad2748[0x2780 - 0x2748];
    float      f2780[3];                /* +0x2780 anchor 1 */
    char       pad278C[0x27C4 - 0x278C];
    float      f27C4[3];                /* +0x27C4 anchor 2 */
    char       pad27D0[0x2808 - 0x27D0];
    float      f2808[3];                /* +0x2808 checkpoint target */
    char       pad2814[0x2838 - 0x2814];
    float      f2838;                   /* +0x2838 latched checkpoint */
    float      f283C;
    float      f2840;
    char       pad2844[0x29C0 - 0x2844];
    In5C8B0   *p29C0;                   /* +0x29C0 input record */

    void Step();                        /* 0x1005C8B0 -- defined here */
    void Sub5D3C0();                    /* 0x1005D3C0 */
    void Sub1C90();                     /* 0x10001C90 */
    void Poll6F170();                   /* 0x1006F170 */
    void SetVel(float x, float y, float z);     /* 0x1006FA10 */
    void Respawn();                     /* 0x1005C6D0 */
};













extern "C" {
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
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_100346a0: prototype in br_funcs.h */
/* BrVec3Scale: prototype in br_funcs.h */
/* FUN_10034310: prototype in br_funcs.h */
/* FUN_100345c0: prototype in br_funcs.h */
/* FUN_100345f0: prototype in br_funcs.h */
/* FUN_10034560: prototype in br_funcs.h */
/* FUN_100347f0: prototype in br_funcs.h */
/* FUN_10034390: prototype in br_funcs.h */
/* FUN_10034760: prototype in br_funcs.h */
/* FUN_10002570: prototype in br_funcs.h */
/* FUN_1002de6b: prototype in br_funcs.h */
}

#define SQ(a) ((a) * (a))
#define MINE (((BrDriverCar *)(this))->f140 == g_aBrView[0].iCar || (g_brMode0AA8B4 > 1 && ((BrDriverCar *)(this))->f140 == g_aBrView[1].iCar))

void Car5C8B0::Step()
{
    float    best;
    float    b[3];
    float    vecA[3];
    float    tmpC[3];
    float    dx, dy, dz, d, dxy, dx2, dy2;
    float   *p;
    int      n;
    float    r, v;
    float   *pA, *pB, *pV;
    int      ext;

    if ((*(int *)&g_brRaceBeginDifficulty) != 0)
        (*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->f20 = -(*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->f20;
    if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->f20 > DAT_100778a4) {
        d = ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->f20 - DAT_100778a4) * DAT_100778a8;
        (*(float *)&((BrDriverCar *)(this))->f2720) = -(d * d);
    } else if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->f20 < DAT_100778ac) {
        d = ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->f20 - DAT_100778ac) * DAT_100778a8;
        (*(float *)&((BrDriverCar *)(this))->f2720) = d * d;
    } else {
        *(int *)&(*(float *)&((BrDriverCar *)(this))->f2720) = 0;
    }

    if ((((BrDriver *)((BrDriverCar *)(this))->pProfile)->f68 & 2) && MINE) {
        if ((*(int *)&g_BrCamHold2) == 0)
            goto scan;
        goto flags;
    }
    if (((*(int *)&g_brRaceRules.mode) == 4 || (*(int *)&g_brRaceRules.mode) == 5 || (*(int *)&DAT_105ccb68[8]) != 0) && MINE)
        goto hold;
    if (MINE)
        (*(int *)&g_BrCamHold2) = 0;
    goto flags;
hold:
    if ((*(int *)&g_BrCamHold2) == 0) {
scan:
    best = 16777216.0f;
    if (g_brTrkHdr.cPayload > 0) {
        p = BR_PTR32(float *, g_brTrkHdr.aPayload) + 2;
        n = g_brTrkHdr.cPayload;
        do {
            dz = (*(float *)&((BrDriverCar *)(this))->pos.z) - p[0];
            dx2 = SQ(((BrDriverCar *)(this))->pos.x - p[-2]);
            dy2 = SQ(((BrDriverCar *)(this))->pos.y - p[-1]);
            if (dz <= DAT_10077898 && dz * dz + dy2 + dx2 < best) {
                b[0] = p[-2];
                b[1] = p[-1];
                b[2] = p[0];
                best = dz * dz + dy2 + dx2;
            }
            p += 3;
            n--;
        } while (n != 0);
    }
    if (best < DAT_100778b4) {
        if ((*(float * *)&((BrDriverCar *)(this))->pMatA) != (*(float (*)[3])&((BrDriverCar *)(this))->aSnap[3].m[0][0]) || (*(float *)&((BrDriverCar *)(this))->aSnap[3].m[3][0]) != b[0] || (*(float *)&((BrDriverCar *)(this))->aSnap[3].m[3][1]) != b[1] || (*(float *)&((BrDriverCar *)(this))->aSnap[3].m[3][2]) != b[2])
            (*(int *)&((BrDriverCar *)(this))->fF78) = 1;
        (*(float *)&((BrDriverCar *)(this))->aSnap[3].m[3][0]) = b[0];
        (*(float *)&((BrDriverCar *)(this))->aSnap[3].m[3][1]) = b[1];
        (*(float *)&((BrDriverCar *)(this))->aSnap[3].m[3][2]) = b[2];
        (*(float * *)&((BrDriverCar *)(this))->pMatA) = (*(float (*)[3])&((BrDriverCar *)(this))->aSnap[3].m[0][0]);
    } else {
        if ((*(float * *)&((BrDriverCar *)(this))->pMatA) == (*(float (*)[3])&((BrDriverCar *)(this))->aSnap[3].m[0][0])) {
            (*(int *)&((BrDriverCar *)(this))->fF78) = 1;
            if (DAT_10b1cf18 == 0 && (*(int *)&g_BrCamHold) == 0)
                (*(float * *)&((BrDriverCar *)(this))->pMatA) = (*(float (*)[3])&((BrDriverCar *)(this))->aSnap[1].m[0][0]);
            else
                (*(float * *)&((BrDriverCar *)(this))->pMatA) = (*(float (*)[3])&((BrDriverCar *)(this))->aSnap[0].m[0][0]);
            (*(int *)&g_BrCamHold2) = 0x3c;
            DAT_10b1cf18 = (DAT_10b1cf18 == 0);
        }
    }
    } else {
        (*(int *)&g_BrCamHold2)--;
    }
    if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags & 0xf000000)
        (*(int *)&g_BrCamHold2) = 0x1c2;

flags:
    if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags & 0x1000000) {
        (*(float * *)&((BrDriverCar *)(this))->pMatA) = (*(float (*)[3])&((BrDriverCar *)(this))->aSnap[0].m[0][0]);
        (*(int *)&((BrDriverCar *)(this))->fF78) = 1;
        (*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->Ack(0x1000000);
    }
    if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags & 0x2000000) {
        Sub1C90();
        (*(int *)&((BrDriverCar *)(this))->fF78) = 1;
        (*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->Ack(0x2000000);
    }
    if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags & 0x4000000) {
        (*(float * *)&((BrDriverCar *)(this))->pMatA) = (*(float (*)[3])&((BrDriverCar *)(this))->aSnap[1].m[0][0]);
        (*(int *)&((BrDriverCar *)(this))->fF78) = 1;
        (*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->Ack(0x4000000);
    }
    if (g_brMode0AA8B4 == 1 && ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags & 0x8000000)) {
        (*(float * *)&((BrDriverCar *)(this))->pMatA) = (*(float (*)[3])&((BrDriverCar *)(this))->aSnap[2].m[0][0]);
        (*(int *)&((BrDriverCar *)(this))->fF78) = 1;
        (*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->Ack(0x8000000);
    }

    if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->f1C > DAT_100778b8) {
        d = ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->f1C - DAT_100778b8) * DAT_100778bc;
        (*(float *)&((BrDriverCar *)(this))->aimFwd) = d * d;
    } else if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->f1C < DAT_100778c0) {
        d = ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->f1C - DAT_100778c0) * DAT_100778bc;
        (*(float *)&((BrDriverCar *)(this))->aimFwd) = -(d * d);
    } else {
        *(int *)&(*(float *)&((BrDriverCar *)(this))->aimFwd) = 0;
    }
    ext = (*(int *)&((BrDriverCar *)(this))->fF7C);
    if (ext != 0)
        *(int *)&(*(float *)&((BrDriverCar *)(this))->f272C) = 0;
    if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags & 0x8000) {
        if (ext == 0)
            *(int *)&(*(float *)&((BrDriverCar *)(this))->aimFwd) = 0x3f800000;
    } else {
        if (ext != 0) {
            int t = *(int *)&(*(float *)&((BrDriverCar *)(this))->aimFwd);
            *(int *)&(*(float *)&((BrDriverCar *)(this))->aimFwd) = 0;
            *(int *)&(*(float *)&((BrDriverCar *)(this))->f2724) = t;
        }
    }
    if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags & 8)
        *(int *)&(*(float *)&((BrDriverCar *)(this))->aimFwd) = 0x3f800000;
    if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags & 2)
        *(int *)&(*(float *)&((BrDriverCar *)(this))->aimFwd) = 0xbf800000;
    if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags & 1) {
        if (ext != 0)
            *(int *)&(*(float *)&((BrDriverCar *)(this))->f272C) = 0x3f800000;
        else
            *(int *)&(*(float *)&((BrDriverCar *)(this))->f2720) = 0xbf800000;
    }
    if ((*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags & 4) {
        if (ext != 0)
            *(int *)&(*(float *)&((BrDriverCar *)(this))->f272C) = 0xbf800000;
        else
            *(int *)&(*(float *)&((BrDriverCar *)(this))->f2720) = 0x3f800000;
    }

    if ((*(int *)&g_brRaceRules.mode) == 5) {
        BrVec3MulAddTo((struct BrVec3 *)(&(*(float *)&((BrDriverCar *)(this))->pos.x)), (const struct BrVec3 *)(this), 15.0f);
        Sub5D3C0();
        vecA[0] = (*(float (*)[3])&((BrDriverCar *)(this))->tangent)[0];
        vecA[1] = (*(float (*)[3])&((BrDriverCar *)(this))->tangent)[1];
        vecA[2] = (*(float (*)[3])&((BrDriverCar *)(this))->tangent)[2];
        BrVec3MulAddTo((struct BrVec3 *)(&(*(float *)&((BrDriverCar *)(this))->pos.x)), (const struct BrVec3 *)(this), -15.0f);
        Sub5D3C0();
        (*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags &= 0xf0c0ffff;
        if ((*(float *)&((BrDriverCar *)(this))->fFF4) < DAT_100778c4) {
            BrVec3Scale(b, (*(float (*)[3])&((BrDriverCar *)(this))->tangent), 27.0f);
        } else {
            BrVec3Scale(b, (*(float (*)[3])&((BrDriverCar *)(this))->tangent), (DAT_100778c8 - (*(float *)&((BrDriverCar *)(this))->fFF4)) * DAT_100778cc);
            if ((*(float *)&((BrDriverCar *)(this))->fFF4) > DAT_100778d0) {
                (*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags |= 0x40000;
                if ((*(float *)&((BrDriverCar *)(this))->fFF4) > DAT_100778d4) {
                    b[2] = 0.0f;
                    b[1] = 0.0f;
                    b[0] = 0.0f;
                }
            } else {
                (*(In5C8B0 * *)&((BrDriverCar *)(this))->pCtl)->flags |= 0x80000;
            }
        }
        (*(float *)&((BrDriverCar *)(this))->f0E20) = BrVec3Dot(vecA, (*(float (*)[3])&((BrDriverCar *)(this))->right)) * DAT_100778dc;
        b[0] = b[0] - (*(float *)&((BrDriverCar *)(this))->d.x) * DAT_100778e0;
        b[1] = b[1] - (*(float *)&((BrDriverCar *)(this))->d.y) * DAT_100778e0;
        SetVel(b[0], b[1], b[2]);
    }

    if ((*(unsigned int *)&g_BrX18ABAD0) & 0x10000)
        (*(char *)&((BrDriverCar *)(this))->aBody[0].f0202) = 0x80;
    if ((*(unsigned int *)&g_BrX18ABAD0) & 0x20000)
        (*(char *)&((BrDriverCar *)(this))->aBody[0].f0203) = 0x80;
    if ((*(unsigned int *)&g_BrX18ABAD0) & 0x40000)
        (*(char *)&((BrDriverCar *)(this))->aBody[0].f0204) = 0x80;
    if ((*(unsigned int *)&g_BrX18ABAD0) & 0x80000)
        (*(char *)&((BrDriverCar *)(this))->aBody[0].f0205) = 0x80;
    if ((*(unsigned int *)&g_BrX18ABAD0) & 0x80)
        (*(char *)&((BrDriverCar *)(this))->aBody[0].f0206) = 0x80;
    Poll6F170();

    if ((*(int *)&((BrDriverCar *)(this))->fF7C) == 0) {
        BrVec3Add(b, &((BrDriverCar *)(this))->pos.x, (const float *)this);
        BrVec3AddTo(b, (*(float (*)[3])&((BrDriverCar *)(this))->right));
        pB = (*(float (*)[3])&((BrDriverCar *)(this))->f1040[1]);
        pA = (*(float (*)[3])&((BrDriverCar *)(this))->f1038[0]);
        BrVec3Add(tmpC, pA, pB);
        vecA[0] = (*(float (*)[3])&((BrDriverCar *)(this))->f1050[0])[0];
        pV = (*(float (*)[3])&((BrDriverCar *)(this))->f1050[0]);
        vecA[1] = pV[1];
        vecA[2] = pV[2];
        BrVec3Sub((struct BrVec3 *)(pV), (const struct BrVec3 *)(pA), b);
        r = BrVec3Length((const struct BrVec3 *)(pV));
        if (r != DAT_100778d8)
            BrVec3ScaleBy((struct BrVec3 *)(pV), (r / (r - DAT_100778e4)) / r);
        dy = BrVec3Dist((const struct BrVec3 *)(pV), vecA);
        dy = BrSqrtF(dy);
        v = dy / (BrVec3Length((const struct BrVec3 *)(pB)) - DAT_100778e4);
        if (v > DAT_100778e8)
            BrAccumAddClamp((*(int *)&((BrDriverCar *)(this))->f140), v + v);
        pA[0] = b[0];
        pA[1] = b[1];
        pA[2] = b[2];
        BrVec3Sub((struct BrVec3 *)(pB), (const struct BrVec3 *)(pA), tmpC);
        (*(float (*)[3])&((BrDriverCar *)(this))->f1040[1])[2] = (*(float (*)[3])&((BrDriverCar *)(this))->f1040[1])[2] - DAT_100778f0;
    }

    if (DAT_118eeee4 != 0) {
        if (!(((BrDriver *)((BrDriverCar *)(this))->pProfile)->f68 & 3) && (*(int *)&DAT_105ccb68[8]) == 0)
            (*(int *)&((BrDriverCar *)(this))->aBody[0].f01F8) = -1;
        DAT_118eeee4 = 0;
    }
    Respawn();
}

/* C entry points (generated by ports/brally/tools/methodfwd.py) */
extern "C" void FUN_1005c8b0(void *self)
{
    ((class Car5C8B0 *)self)->Step();
}
/* end of C entry points */

/* Methods of the local classes above that other files define: each is
 * the function at its original address, reached through its C entry. */
/* 0x1002F640: the original calls BrBitLatchTake by address */
inline void In5C8B0::Ack(unsigned int a1)
{
    BrBitLatchTake((struct BrBitLatch *)this, (unsigned int)a1);
}

/* 0x1005D3C0: the original calls BrCarPathEval by address */
inline void Car5C8B0::Sub5D3C0()
{
    BrCarPathEval((struct BrDriverCar *)this);
}

/* 0x10001C90: the original calls BrVec3Predict by address */
inline void Car5C8B0::Sub1C90()
{
    BrVec3Predict((struct BrDriverCar *)this);
}

/* 0x1006F170: the original calls BrCarStep by address */
inline void Car5C8B0::Poll6F170()
{
    BrCarStep((struct BrDriverCar *)this);
}

/* 0x1006FA10: the original calls BrEntSetVel by address */
inline void Car5C8B0::SetVel(float a1, float a2, float a3)
{
    BrEntSetVel((struct BrEntCar *)this, (float)a1, (float)a2, (float)a3);
}

/* 0x1005C6D0: the original calls BrCarRespawn_1005C6D0 by address */
inline void Car5C8B0::Respawn()
{
    BrCarRespawn_1005C6D0((void *)this);
}
