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
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
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
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

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

typedef char chk_140[(unsigned)&((Car5C8B0 *)0)->f140  == 0x140  ? 1 : -1];
typedef char chk_366[(unsigned)&((Car5C8B0 *)0)->b366  == 0x366  ? 1 : -1];
typedef char chk_e20[(unsigned)&((Car5C8B0 *)0)->fE20  == 0xE20  ? 1 : -1];
typedef char chk_f24[(unsigned)&((Car5C8B0 *)0)->fF24  == 0xF24  ? 1 : -1];
typedef char chk_f78[(unsigned)&((Car5C8B0 *)0)->fF78  == 0xF78  ? 1 : -1];
typedef char chk_ff4[(unsigned)&((Car5C8B0 *)0)->fFF4  == 0xFF4  ? 1 : -1];
typedef char chk_1038[(unsigned)&((Car5C8B0 *)0)->f1038 == 0x1038 ? 1 : -1];
typedef char chk_2720[(unsigned)&((Car5C8B0 *)0)->f2720 == 0x2720 ? 1 : -1];
typedef char chk_273c[(unsigned)&((Car5C8B0 *)0)->f273C == 0x273C ? 1 : -1];
typedef char chk_2780[(unsigned)&((Car5C8B0 *)0)->f2780 == 0x2780 ? 1 : -1];
typedef char chk_27c4[(unsigned)&((Car5C8B0 *)0)->f27C4 == 0x27C4 ? 1 : -1];
typedef char chk_2808[(unsigned)&((Car5C8B0 *)0)->f2808 == 0x2808 ? 1 : -1];
typedef char chk_2838[(unsigned)&((Car5C8B0 *)0)->f2838 == 0x2838 ? 1 : -1];
typedef char chk_29c0[(unsigned)&((Car5C8B0 *)0)->p29C0 == 0x29C0 ? 1 : -1];

extern "C" {
extern int   DAT_106ea3f4;              /* mirror steering */
extern float DAT_100778a4;
extern float DAT_100778a8;
extern float DAT_100778ac;
extern int   DAT_106e8720;
extern int   DAT_106e86c8;
extern int   DAT_100aa044;
extern int   DAT_100a9360;              /* game mode */
extern int   DAT_105ccb88;
extern int   DAT_10b1cf10;              /* lock-on timer */
extern int   DAT_10b1cf14;
extern int   DAT_10b1cf18;
extern int   DAT_106eed60;              /* checkpoint count */
extern float *DAT_106eed5c;             /* checkpoint xyz table */
extern float DAT_10077898;
extern float DAT_100778b4;
extern float DAT_100778b8;
extern float DAT_100778bc;
extern float DAT_100778c0;
extern float DAT_100778c4;
extern float DAT_100778c8;
extern float DAT_100778cc;
extern float DAT_100778d0;
extern float DAT_100778d4;
extern float DAT_100778d8;
extern float DAT_100778dc;
extern float DAT_100778e0;
extern float DAT_100778e4;
extern float DAT_100778e8;
extern float DAT_100778f0;
extern unsigned DAT_118eebe8;           /* keyboard bits */
extern int   DAT_118eeee4;              /* deferred reset request */
void  FUN_100346a0(float *pOut, Car5C8B0 *pCar, float k);
void  BrVec3Scale(float *pOut, float *pIn, float k);            /* 0x10034360 */
float FUN_10034310(float *pA, float *pB);
void  FUN_100345c0(float *pOut, float *pA, void *pB);
void  FUN_100345f0(float *pOut, float *pB);
void  FUN_10034560(float *pOut, float *pA, float *pB);
float FUN_100347f0(float *pV);
void  FUN_10034390(float *pV, float k);
float FUN_10034760(float *pV, float *pW);
float FUN_10002570(float v);
void  FUN_1002de6b(int lap, float v);
}

#define SQ(a) ((a) * (a))
#define MINE (f140 == DAT_106e86c8 || (DAT_100aa044 > 1 && f140 == DAT_106e8720))

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

    if (DAT_106ea3f4 != 0)
        p29C0->f20 = -p29C0->f20;
    if (p29C0->f20 > DAT_100778a4) {
        d = (p29C0->f20 - DAT_100778a4) * DAT_100778a8;
        f2720 = -(d * d);
    } else if (p29C0->f20 < DAT_100778ac) {
        d = (p29C0->f20 - DAT_100778ac) * DAT_100778a8;
        f2720 = d * d;
    } else {
        *(int *)&f2720 = 0;
    }

    if ((pF00->b68 & 2) && MINE) {
        if (DAT_10b1cf10 == 0)
            goto scan;
        goto flags;
    }
    if ((DAT_100a9360 == 4 || DAT_100a9360 == 5 || DAT_105ccb88 != 0) && MINE)
        goto hold;
    if (MINE)
        DAT_10b1cf10 = 0;
    goto flags;
hold:
    if (DAT_10b1cf10 == 0) {
scan:
    best = 16777216.0f;
    if (DAT_106eed60 > 0) {
        p = DAT_106eed5c + 2;
        n = DAT_106eed60;
        do {
            dz = f38 - p[0];
            dx2 = SQ(f30 - p[-2]);
            dy2 = SQ(f34 - p[-1]);
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
        if (p2734 != f2808 || f2838 != b[0] || f283C != b[1] || f2840 != b[2])
            fF78 = 1;
        f2838 = b[0];
        f283C = b[1];
        f2840 = b[2];
        p2734 = f2808;
    } else {
        if (p2734 == f2808) {
            fF78 = 1;
            if (DAT_10b1cf18 == 0 && DAT_10b1cf14 == 0)
                p2734 = f2780;
            else
                p2734 = f273C;
            DAT_10b1cf10 = 0x3c;
            DAT_10b1cf18 = (DAT_10b1cf18 == 0);
        }
    }
    } else {
        DAT_10b1cf10--;
    }
    if (p29C0->flags & 0xf000000)
        DAT_10b1cf10 = 0x1c2;

flags:
    if (p29C0->flags & 0x1000000) {
        p2734 = f273C;
        fF78 = 1;
        p29C0->Ack(0x1000000);
    }
    if (p29C0->flags & 0x2000000) {
        Sub1C90();
        fF78 = 1;
        p29C0->Ack(0x2000000);
    }
    if (p29C0->flags & 0x4000000) {
        p2734 = f2780;
        fF78 = 1;
        p29C0->Ack(0x4000000);
    }
    if (DAT_100aa044 == 1 && (p29C0->flags & 0x8000000)) {
        p2734 = f27C4;
        fF78 = 1;
        p29C0->Ack(0x8000000);
    }

    if (p29C0->f1C > DAT_100778b8) {
        d = (p29C0->f1C - DAT_100778b8) * DAT_100778bc;
        f2728 = d * d;
    } else if (p29C0->f1C < DAT_100778c0) {
        d = (p29C0->f1C - DAT_100778c0) * DAT_100778bc;
        f2728 = -(d * d);
    } else {
        *(int *)&f2728 = 0;
    }
    ext = fF7C;
    if (ext != 0)
        *(int *)&f272C = 0;
    if (p29C0->flags & 0x8000) {
        if (ext == 0)
            *(int *)&f2728 = 0x3f800000;
    } else {
        if (ext != 0) {
            int t = *(int *)&f2728;
            *(int *)&f2728 = 0;
            *(int *)&f2724 = t;
        }
    }
    if (p29C0->flags & 8)
        *(int *)&f2728 = 0x3f800000;
    if (p29C0->flags & 2)
        *(int *)&f2728 = 0xbf800000;
    if (p29C0->flags & 1) {
        if (ext != 0)
            *(int *)&f272C = 0x3f800000;
        else
            *(int *)&f2720 = 0xbf800000;
    }
    if (p29C0->flags & 4) {
        if (ext != 0)
            *(int *)&f272C = 0xbf800000;
        else
            *(int *)&f2720 = 0x3f800000;
    }

    if (DAT_100a9360 == 5) {
        FUN_100346a0(&f30, this, 15.0f);
        Sub5D3C0();
        vecA[0] = fF24[0];
        vecA[1] = fF24[1];
        vecA[2] = fF24[2];
        FUN_100346a0(&f30, this, -15.0f);
        Sub5D3C0();
        p29C0->flags &= 0xf0c0ffff;
        if (fFF4 < DAT_100778c4) {
            BrVec3Scale(b, fF24, 27.0f);
        } else {
            BrVec3Scale(b, fF24, (DAT_100778c8 - fFF4) * DAT_100778cc);
            if (fFF4 > DAT_100778d0) {
                p29C0->flags |= 0x40000;
                if (fFF4 > DAT_100778d4) {
                    b[2] = 0.0f;
                    b[1] = 0.0f;
                    b[0] = 0.0f;
                }
            } else {
                p29C0->flags |= 0x80000;
            }
        }
        fE20 = FUN_10034310(vecA, f10) * DAT_100778dc;
        b[0] = b[0] - fF4C * DAT_100778e0;
        b[1] = b[1] - fF50 * DAT_100778e0;
        SetVel(b[0], b[1], b[2]);
    }

    if (DAT_118eebe8 & 0x10000)
        b366 = 0x80;
    if (DAT_118eebe8 & 0x20000)
        b367 = 0x80;
    if (DAT_118eebe8 & 0x40000)
        b368 = 0x80;
    if (DAT_118eebe8 & 0x80000)
        b369 = 0x80;
    if (DAT_118eebe8 & 0x80)
        b36A = 0x80;
    Poll6F170();

    if (fF7C == 0) {
        FUN_100345c0(b, &f30, this);
        FUN_100345f0(b, f10);
        pB = f1044;
        pA = f1038;
        FUN_100345c0(tmpC, pA, pB);
        vecA[0] = f1050[0];
        pV = f1050;
        vecA[1] = pV[1];
        vecA[2] = pV[2];
        FUN_10034560(pV, pA, b);
        r = FUN_100347f0(pV);
        if (r != DAT_100778d8)
            FUN_10034390(pV, (r / (r - DAT_100778e4)) / r);
        dy = FUN_10034760(pV, vecA);
        dy = FUN_10002570(dy);
        v = dy / (FUN_100347f0(pB) - DAT_100778e4);
        if (v > DAT_100778e8)
            FUN_1002de6b(f140, v + v);
        pA[0] = b[0];
        pA[1] = b[1];
        pA[2] = b[2];
        FUN_10034560(pB, pA, tmpC);
        f1044[2] = f1044[2] - DAT_100778f0;
    }

    if (DAT_118eeee4 != 0) {
        if (!(pF00->b68 & 3) && DAT_105ccb88 == 0)
            f35C = -1;
        DAT_118eeee4 = 0;
    }
    Respawn();
}
