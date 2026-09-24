/* br_dlvtx_nozlit.c -- drawing: lit no-Z vertex transform handler.
 *
 * ONE function, 0x10023360, the vertex handler br_dl.c installs when the
 * geometry mode has LIGHTING but not ZBUFFER.  The lit sibling of 0x10023110
 * (br_dlvtx_noz.c): same light cache as the texgen pair (br_dlvtx_texgen.c,
 * br_dlvtx_gen.c), same transform rows, then a clamped diffuse term per
 * vertex and the no-Z projector 0x10023760 (br_dlproj.c).
 *
 * Filed on its own, like the rest of the family: br_dl.c's TU context
 * carries byte-exact functions whose x87 scheduling moves when a body is
 * added there.  Matching arm only; the port runs br_dl.c's portable path.
 *
 * WHAT IT IS FOR: lighting a model drawn with the depth buffer OFF -- an
 * N64 idiom for lit geometry layered over the scene without depth tests.
 * The handler selector 0x1001FD70 installs it (0x1001FE9E, its only install
 * site) whenever the geometry mode has LIGHTING (0x20000) set and ZBUFFER
 * (0x1) clear.  The PC game never produces that state.
 *
 * DEAD CODE IN THE PC RELEASE -- settled 2026-09-24, do not re-litigate:
 *  - The geometry mode 0x105D17C8 has exactly two writers, the display-list
 *    handlers G_CLEARGEOMETRYMODE 0x1001FD40 and G_SETGEOMETRYMODE
 *    0x100211E0; BRally.exe and BossRally.exe never reference it.  Its
 *    initial value is 0.  So the state comes only from 0xB6/0xB7 commands.
 *  - No asset emits those: a census of all 1577 files under the CD (363 of
 *    them with >20 aligned combiner commands, i.e. readable display lists)
 *    finds no geometry-mode command with valid bits.  Every one is built in
 *    BRGlide.dll code: 59 emit sites, each traced to its operand.
 *  - Lighting is set without depth in ONE place, the object builder
 *    0x1000CBA0 (0x1000CEDE / 0x1000CEF6: 0x30004 / 0x20004), called only
 *    from the scene builder 0x1000EAF0.  Every scene-build pass first emits
 *    SET (cull | fog-if-0x106ED6A8 | 0xA0005) at 0x1000F083 / 0x1000F289 -- depth,
 *    lighting and shade -- with no return before it (br_scenedl.c).
 *  - Depth is cleared on its own only by the text drawer 0x100168C0
 *    (0x100168D7), the HUD dial 0x100140B0 (0x10014210, restored at
 *    0x10014729) and frame setup 0x10015630 (0x10015910, CLR 0xF0205, then
 *    SET 0x20205 at 0x10015ADE).  None is reachable from the scene builder:
 *    text runs after each view's world pass (HUD, FPS readout, captions,
 *    frame present); the object builder's one log call (0x1000D9DF, "BAD
 *    VTX DL", a fatal-data path) draws into its own stack display list at
 *    0x10008EF0 and never reaches the frame's.  Frame setup's early returns
 *    (rain/storm/no-sky plain clear; the never-written 0x106ED698) skip its
 *    reset, but the scene builder's own SET re-enables depth regardless.
 *  - Measured: with a code hook on both geometry-mode writers, a rainy
 *    quick race performs 18,528 writes and none leaves LIGHTING set with
 *    ZBUFFER clear; sunny, rainy, snowy and night races and all 25 brbox
 *    scripts call this function 0 times (0x10023110, its unlit twin, 337+).
 *  - BRD3D.dll: same executables, same data, same geometry-mode sources.
 *    The N64 original was not checked for this mode.
 *  Consequence: the live oracle (A5) can never reach it, so T3 is closed to
 *  this function; only byte-exact T4 finishes it.
 * STATUS: EXCLUDED (T2)
 */
#include <stdint.h>
#include <stddef.h>
typedef struct BrDlVtx {
    float   x, y, z;
    float   r, g, b;
    float   ooz;
    float   a;
    float   oow;
    float   tmu0[3];
    float   tmu1[3];
    int32_t outcode;
    float   f40;
    float   cx, cy, cz;
    float   s, t;
    float   cw;
    float   n0, n1, n2;
} BrDlVtx;

#ifdef BR_MATCHING_BUILD

/* MVP matrix, 4x4 row-major at 0x105D1760. */
extern float DAT_105d1760, DAT_105d1764, DAT_105d1768, DAT_105d176c;
extern float DAT_105d1790, DAT_105d1794, DAT_105d1798, DAT_105d179c;
#define DAT_105d1770 (*(float *)0x105d1770)
#define DAT_105d1774 (*(float *)0x105d1774)
#define DAT_105d1778 (*(float *)0x105d1778)
#define DAT_105d177c (*(float *)0x105d177c)
#define DAT_105d1780 (*(float *)0x105d1780)
#define DAT_105d1784 (*(float *)0x105d1784)
#define DAT_105d1788 (*(float *)0x105d1788)
#define DAT_105d178c (*(float *)0x105d178c)

extern int DAT_105d17d0;          /* fLightCached */
extern int DAT_105ccfd0;          /* nLights */
extern int DAT_100a9a50;          /* iModel */
typedef struct { float m[16]; } BrDlMtx;
extern BrDlMtx DAT_105ccd50[];    /* model matrix stack, 1-based: [top - 1] */

typedef struct {
    unsigned char col[4];
    unsigned char colc[4];
    signed char   dir[4];
    unsigned char pad[4];
} BrDlLight;
extern BrDlLight DAT_105ccc78[2];

extern float DAT_105ce210, DAT_105ce214, DAT_105ce218;  /* lightScale[3] */
extern float DAT_105ce21c, DAT_105ce220, DAT_105ce224;  /* lightDir[3] */
extern float DAT_105ce228, DAT_105ce22c, DAT_105ce230;  /* lightAmb[3] */

extern float DAT_105d17a4, DAT_105d17b4, DAT_105ce2d0;  /* unlit colour */

extern BrDlVtx DAT_105ce318[];

extern float DAT_10077410;        /* 0.0f   */
extern float DAT_10077418;        /* 255.0f */
extern float DAT_10077420;        /* 128.0f */

typedef struct { float x, y, z, s, t, n0, n1, n2; } BrDlSrcVtx;

extern void    FUN_100344D0(void *);
extern int32_t FUN_10022120(void *);
extern void    FUN_10023760(void *, void *, float, float, float);

/* Hand-transcribed from the Glide bytes, compiled /O2 /Op like the rest of
 * its original TU (pinned in config/t3_variant_c.csv).  1017/1019 B,
 * 288/289 insns, every row paired.  Source facts carried from the family
 * (br_dlvtx_texgen.c): light records read as bytes, 1-based matrix stack,
 * z/y MVP columns as absolute derefs and x/w as externs, pSrc++ at the tail,
 * and the vertex pointer copy formed before the count.
 * Open (x87 scheduling only):
 *  - the four transform rows: the original loads the z and y columns, does
 *    the y product, then loads x; ours issues the x product second.  Dead:
 *    all six term orders x two groupings, operand order inside every term
 *    (24), the translation term in eleven positions, (double) on x or the
 *    column (5 forms; they move x to the memory side), the x column as an
 *    absolute deref, array or struct (6), a plain float[4][4] matrix (12);
 *  - the light setup's colour/direction interleave (one fxch short, the
 *    2-byte gap).  Dead: statement-order hill-climb over the six setup
 *    statements and the three ambient ones, direction bytes as int temps
 *    or inline casts (3);
 *  - the dot product's first load and the ambient byte-load schedule.
 *    Dead: all 12 term orders/groupings of the dot product.
 * @t4-pass 0x10023360 1 2026-09-24 probes 74 bytes 1017 insns 288 regions 7 rows 7 census no  (hand, fn.py variants: transform-row term orders, groupings, operand orders, translation position, double casts, x-column kinds, float[4][4] matrix; nothing adopted moved the gate numbers)
 * @t4-pass 0x10023360 2 2026-09-24 probes 78 bytes 1017 insns 288 regions 7 rows 7 census yes  (census: every row is an x87 issue-order pair plus three singleton fxch; light-setup and ambient statement hill-climb, dot-product orders, direction int temps; nothing moved) */
/* WHAT IT DOES: transforms a batch of vertices through the combined matrix,
 * copies their texture coordinates, lights each one -- the diffuse term from
 * the one directional light, clamped to 255 per channel, or the ambient
 * colour alone when the vertex faces away, or a fixed colour when lighting
 * is off -- and projects every vertex that lies inside the view. */
/* @implements 0x10023360 glide BrDlVtxNoZLit */
const uint8_t *BrDlVtxNoZLit(const uint8_t *p)
{
    int v0;
    int n;
    int i;
    float *m;
    float dx, dy, dz;
    float t, c;
    uint32_t w0;
    const BrDlSrcVtx *pSrc;
    BrDlVtx *pV, *pVc;
    float *pf;
    int32_t oc;

    if (!DAT_105d17d0) {
        if (DAT_105ccfd0 != 0) {
            m = DAT_100a9a50 ? DAT_105ccd50[DAT_100a9a50 - 1].m : NULL;
            DAT_105ce210 = (float)DAT_105ccc78[0].col[0];
            DAT_105ce214 = (float)DAT_105ccc78[0].col[1];
            dz = (float)DAT_105ccc78[0].dir[2];
            dx = (float)DAT_105ccc78[0].dir[0];
            DAT_105ce218 = (float)DAT_105ccc78[0].col[2];
            dy = (float)DAT_105ccc78[0].dir[1];
            DAT_105ce21c = ((m[2] * dz + m[0] * dx) + m[1] * dy) / DAT_10077420;
            DAT_105ce220 = ((m[6] * dz + m[4] * dx) + m[5] * dy) / DAT_10077420;
            DAT_105ce224 = ((m[10] * dz + m[8] * dx) + m[9] * dy) / DAT_10077420;
            FUN_100344D0(&DAT_105ce21c);
            DAT_105ce228 = (float)DAT_105ccc78[1].col[0];
            DAT_105ce22c = (float)DAT_105ccc78[1].col[1];
            DAT_105ce230 = (float)DAT_105ccc78[1].col[2];
        }
        DAT_105d17d0 = 1;
    }

    w0 = *(const uint32_t *)p;
    pSrc = *(const BrDlSrcVtx **)(p + 4);
    v0 = (w0 >> 16) & 0xFF;
    pV = &DAT_105ce318[v0];
    pVc = pV;
    n  = (w0 >> 10) & 0x3F;

    for (i = 0; i < n; i++) {
        pV[i].cx = DAT_105d1780 * pSrc->z + DAT_105d1770 * pSrc->y + pSrc->x * DAT_105d1760 + DAT_105d1790;
        pV[i].cy = DAT_105d1784 * pSrc->z + DAT_105d1774 * pSrc->y + pSrc->x * DAT_105d1764 + DAT_105d1794;
        pV[i].cz = DAT_105d1788 * pSrc->z + DAT_105d1778 * pSrc->y + pSrc->x * DAT_105d1768 + DAT_105d1798;
        pV[i].cw = DAT_105d178c * pSrc->z + DAT_105d177c * pSrc->y + pSrc->x * DAT_105d176c + DAT_105d179c;
        pV[i].s = pSrc->s;
        pV[i].t = pSrc->t;

        if (DAT_105ccfd0 != 0) {
            t = (pSrc->n1 * DAT_105ce220 + pSrc->n2 * DAT_105ce224) + pSrc->n0 * DAT_105ce21c;
            if (t >= DAT_10077410) {
                c = t * DAT_105ce210 + DAT_105ce228;
                pV[i].n0 = (c > DAT_10077418) ? 255.0f : c;
                c = t * DAT_105ce214 + DAT_105ce22c;
                pV[i].n1 = (c > DAT_10077418) ? 255.0f : c;
                c = t * DAT_105ce218 + DAT_105ce230;
                pV[i].n2 = (c > DAT_10077418) ? 255.0f : c;
            } else {
                pV[i].n0 = DAT_105ce228;
                pV[i].n1 = DAT_105ce22c;
                pV[i].n2 = DAT_105ce230;
            }
        } else {
            pV[i].n0 = DAT_105d17a4;
            pV[i].n1 = DAT_105d17b4;
            pV[i].n2 = DAT_105ce2d0;
        }

        pf = &pV[i].f40;
        oc = FUN_10022120(pf);
        pV[i].outcode = oc;
        if (oc == 0)
            FUN_10023760(pVc, pf, pV[i].n0, pV[i].n1, pV[i].n2);
        pSrc++;
        pVc++;
    }
    return p + 8;
}

#endif /* BR_MATCHING_BUILD */
