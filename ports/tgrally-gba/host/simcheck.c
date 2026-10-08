/* simcheck.c DIR [free] -- gba/../sim against the cartridge (tools/simref.py's DIR).
 *
 * The track, the grip table and both cars are loaded from the cartridge's memory;
 * then for every physics frame recorded, the cars as they were at its entry go
 * through BrCarPhysTick here and each field the simulation keeps is compared with
 * the cartridge's at its return.  Built with FX_FLOAT the two must agree to the bit;
 * built fixed, it reports how far they drift.  "free" runs on from the first frame
 * without reloading (the cartridge's pads fed in), to see a whole race diverge.
 *
 *   cc -O1 -DFX_FLOAT -ffp-contract=off -Isim host/simcheck.c sim/geom.c sim/rigid.c sim/coll.c sim/car.c -o simcheck */
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simload.h"
#ifndef DRIVE_SIGN
#define DRIVE_SIGN -1
#endif

#define CARS 2
#define CAR_SIZE 0x2090
#define PAD_SIZE 0x15C
#define NGLOB 9
#define REC (6 + CARS * CAR_SIZE + CARS * PAD_SIZE + NGLOB * 4 + CARS * 4)

static uint8_t *s_ram;
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static float bef(const uint8_t *p) { union { uint32_t u; float f; } x; x.u = be32(p); return x.f; }
static const uint8_t *R(uint32_t a) { return s_ram + (a & 0x7FFFFF); }

#ifdef FX_FLOAT
#define TOFX(f) (f)
#define FROMFX(v) ((double)(v))
#else
#define TOFX(f) ((fx)llround((double)(f) * 4294967296.0))
#define FROMFX(v) ((double)(v) / 4294967296.0)
#endif

/* ---- the track ---- */
static Track s_track;

static void track_load(void)
{
    const uint8_t *h = R(0x80025C00);
    int i, k, nv, nt, ncell;
    fx (*verts)[3];
    uint16_t (*tris)[4];
    uint8_t *surf;
    Plane *planes;
    uint16_t *cs, *ct, *trg, *ttrg;
    int ntrg = 0;

    nt = (int)be32(h + 0x08);
    nv = (int)be32(h + 0x10);
    verts = malloc(sizeof *verts * nv);
    for (i = 0; i < nv; i++)
        for (k = 0; k < 3; k++)
            verts[i][k] = TOFX(bef(R(be32(h + 0x14)) + i * 12 + k * 4));
    tris = malloc(sizeof *tris * nt);
    surf = malloc(nt);
    for (i = 0; i < nt; i++) {
        for (k = 0; k < 4; k++)
            tris[i][k] = be16(R(be32(h + 0x0C)) + i * 8 + k * 2);
        surf[i] = R(be32(h + 0x94))[i];
    }
    planes = calloc(nt, sizeof *planes);
    for (i = 0; i < nt; i++) {               /* BrCollGridCellAcquire's plane */
        Plane *p = &planes[i];
        fx a[3], b[3];
        p->v0 = verts[tris[i][0]];
        p->v1 = verts[tris[i][1]];
        p->v2 = verts[tris[i][2]];
        p->surface = surf[i] & 7;
        p->tri = (uint16_t)i;
        for (k = 0; k < 3; k++) {
            a[k] = p->v1[k] - p->v0[k];
            b[k] = p->v2[k] - p->v0[k];
        }
        p->n[0] = FMUL(a[1], b[2]) - FMUL(a[2], b[1]);
        p->n[1] = FMUL(a[2], b[0]) - FMUL(a[0], b[2]);
        p->n[2] = FMUL(a[0], b[1]) - FMUL(a[1], b[0]);
        BrVec3NormaliseF(p->n);
        p->d = -(FMUL(p->n[0], p->v0[0]) + FMUL(p->n[1], p->v0[1]) + FMUL(p->n[2], p->v0[2]));
        sim_plane_bounds(p);
    }
    cs = malloc(4097 * 2);
    for (i = 0; i < 4097; i++)
        cs[i] = be16(R(be32(h + 0x24)) + i * 2);
    ncell = cs[4096];
    ct = malloc((size_t)(ncell + 1) * 2);
    for (i = 0; i < ncell; i++)
        ct[i] = be16(R(be32(h + 0x20)) + i * 2);
    ttrg = malloc((size_t)nt * 2);
    for (i = 0; i < nt; i++) {
        ttrg[i] = be16(R(be32(h + 0x90)) + i * 2);
        if (ttrg[i] > ntrg)
            ntrg = ttrg[i];
    }
    for (; be16(R(be32(h + 0x8C)) + ntrg * 2) != 0; ntrg++)
        ;
    trg = malloc((size_t)(ntrg + 1) * 2);
    for (i = 0; i <= ntrg; i++)
        trg[i] = be16(R(be32(h + 0x8C)) + i * 2);
    s_track.nverts = nv;
    s_track.ntris = nt;
    s_track.verts = (const fx (*)[3])verts;
    s_track.tris = (const uint16_t (*)[4])tris;
    s_track.surf = surf;
    s_track.planes = planes;
    s_track.cellStart = cs;
    s_track.cellTris = ct;
    s_track.triggers = trg;
    s_track.triTrigger = ttrg;
    s_track.x28 = TOFX(bef(h + 0x28));
    s_track.x2c = TOFX(bef(h + 0x2C));
    s_track.x38 = TOFX(bef(h + 0x38));
    s_track.x3c = TOFX(bef(h + 0x3C));
    for (k = 0; k < 3; k++) {
        s_track.defNormFar[k] = TOFX(bef(R(0x8028B318) + k * 4));
        s_track.defNormNear[k] = TOFX(bef(R(0x8028B324) + k * 4));
    }
    g_track = &s_track;
    for (i = 0; i < 72; i++)
        g_grip[i] = TOFX(bef(R(0x802A4A38) + i * 4));
    printf("track: %d triangles, %d vertices, %d cell entries, %d trigger words\n", nt, nv, ncell, ntrg);
}

/* the differences between a car here and the cartridge's record of it */
static int field_cmp(const Field *f, const uint8_t *obj, const uint8_t *rec, const char *where, double *worst)
{
    int i, bad = 0;
    for (i = 0; i < f->n; i++) {
        double a = 0, b = 0;
        switch (f->type) {
        case SL_F32: a = FROMFX(((const fx *)(obj + f->at))[i]); b = bef(rec + f->off + i * 4); break;
        case SL_I32: a = ((const int *)(obj + f->at))[i]; b = (int)be32(rec + f->off + i * 4); break;
        case SL_U8: a = ((const uint8_t *)(obj + f->at))[i]; b = rec[f->off + i]; break;
        case SL_S8: a = ((const int8_t *)(obj + f->at))[i]; b = (int8_t)rec[f->off + i]; break;
        case SL_U16: a = ((const uint16_t *)(obj + f->at))[i]; b = be16(rec + f->off + i * 2); break;
        case SL_HIT: a = *(const Plane *const *)(obj + f->at) != 0; b = be32(rec + f->off) != 0; break;
        }
#ifdef FX_FLOAT
        if (a != b && !(a != a && b != b)) {
#else
        if (fabs(a - b) > 1e-3 * (1 + fabs(b))) {
#endif
            if (worst == 0 || bad < 3)
                printf("    %s %s[%d] (0x%X): %.9g, the cartridge's %.9g\n", where, f->name, i, f->off + (f->type == SL_U16 ? i * 2 : f->type >= SL_U8 ? i : i * 4), a, b);
            bad++;
        }
        if (worst && f->type == SL_F32 && fabs(a - b) > *worst)
            *worst = fabs(a - b);
    }
    return bad;
}

static Car s_cars[CARS];
static Pad s_pads[CARS];

static int car_cmp(const Car *c, const uint8_t *rec, int slot, double *worst)
{
    unsigned i, k;
    int bad = 0;
    char where[32];
    snprintf(where, sizeof where, "car%d", slot);
    for (i = 0; i < (unsigned)sl_ncar; i++)
        bad += field_cmp(&sl_car[i], (const uint8_t *)c, rec, where, worst);
    for (k = 0; k < 5; k++) {
        const Body *b = k ? &c->wb[k - 1] : &c->body;
        snprintf(where, sizeof where, "car%d.%s", slot, k ? (const char *[]){ "", "wb0", "wb1", "wb2", "wb3" }[k] : "body");
        for (i = 0; i < (unsigned)sl_nbody; i++)
            bad += field_cmp(&sl_body[i], (const uint8_t *)b, rec + sl_bodies[k], where, worst);
    }
    for (k = 0; k < 16; k++) {
        for (i = 0; i < 3; i++) {
            double a = FROMFX(c->force[k].f[i]), b = bef(rec + 0xB70 + k * 0x20 + 8 + i * 4);
#ifdef FX_FLOAT
            if (a != b) {
#else
            if (fabs(a - b) > 1e-3 * (1 + fabs(b))) {
#endif
                if (!worst || bad < 3)
                    printf("    car%d force 0x%X f[%d]: %.9g, the cartridge's %.9g\n", slot, 0xB70 + k * 0x20, i, a, b);
                bad++;
            }
        }
    }
    return bad;
}

int main(int argc, char **argv)
{
    char path[512];
    FILE *f;
    long size;
    uint8_t *ticks;
    int n, t, k, fails = 0, checked = 0, drive = argc > 2 && !strcmp(argv[2], "drive");
    int traj = drive || (argc > 2 && !strcmp(argv[2], "traj"));
    int freerun = traj || (argc > 2 && !strcmp(argv[2], "free"));
    double offsum = 0, offmax = 0, vsum = 0, vref = 0;
    int stuck = 0;
    double sum = 0;
    int nsum = 0;
    double worst = 0;

    snprintf(path, sizeof path, "%s/ram.bin", argv[1]);
    f = fopen(path, "rb");
    s_ram = malloc(0x800000);
    if (!f || fread(s_ram, 1, 0x800000, f) != 0x800000)
        return 1;
    fclose(f);
    snprintf(path, sizeof path, "%s/ticks.bin", argv[1]);
    f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    ticks = malloc((size_t)size);
    if (fread(ticks, 1, (size_t)size, f) != (size_t)size)
        return 1;
    fclose(f);
    n = (int)(size / REC);
    track_load();
    g_world.ncars = CARS;
    g_world.lens = TOFX(bef(R(0x8028AAC0)));
    g_world.views = (int)be32(R(0x8028AB0C));
    g_world.replay = (int)be32(R(0x80270788));
    g_world.viewCar = (int)be32(R(0x8031B2C8 + 16));
    sim_camera = BrCamChaseStep;
    for (k = 0; k < CARS; k++)
        g_world.cars[k] = &s_cars[k];
#ifndef FX_FLOAT
    if (argc > 2 && !strcmp(argv[2], "test")) {      /* a set drive from the grid: throttle 6 s, half lock */
        const uint8_t *in = ticks;                    /* 4 s, the brake 3 s; a tick every SIM_EVERY frames */
        const uint8_t *g = in + 6 + CARS * CAR_SIZE + CARS * PAD_SIZE;
        int every = getenv("SIM_EVERY") ? atoi(getenv("SIM_EVERY")) : 1, f;
        g_world.dt = TOFX(bef(g + 4));
        g_world.weather = (int)be32(g + 8);
        g_world.mode = (int)be32(g + 12);
        g_world.players = (int)be32(g + 16);
        for (k = 0; k < CARS; k++) {
            const uint8_t *cr = in + 6 + k * CAR_SIZE;
            int i;
            sl_car_load(&s_cars[k], &s_pads[k], cr, in + 6 + CARS * CAR_SIZE + k * PAD_SIZE, 0);   /* (as gba/race.c) */
            for (i = 0; i < 3; i++)
                s_cars[k].camView[i] = TOFX(bef(R(be32(cr + 0x2078)) + 0xB0 + i * 4));
        }
        for (f = 0; f < 30; f++) {                    /* settled on the grid in the game's ticks */
            g_world.walkBack ^= 1;
            BrCarPhysTick(&s_cars[0]);
            BrCarPhysTick(&s_cars[1]);
        }
        g_sim_dt = FX(0.033333335) * every;
        g_sim_dtk = FX(1.0) * every;
        for (f = 0; f < 13 * 30; f += every) {
            Car *c = &s_cars[0];
            double sec = f / 30.0;
            s_pads[0].flags = sec < 10 ? 0x10 | 0x10000 : 0x20 | 0x40000;
            s_pads[0].steer = sec >= 6 && sec < 10 ? FX(0.5) : 0;
            s_pads[1].flags = 0;
            s_pads[1].steer = 0;
            g_world.walkBack ^= 1;
            BrCarPhysTick(&s_cars[0]);
            BrCarPhysTick(&s_cars[1]);
            if (f % 15 < every)
                printf("%5.1f s %6.1f mph  at %8.2f %8.2f %6.2f  heading %7.1f  up %5.2f  gear %d\n", sec,
                       FROMFX(c->speedMph), FROMFX(c->body.st.pos[0]), FROMFX(c->body.st.pos[1]), FROMFX(c->body.st.pos[2]),
                       atan2(FROMFX(c->body.m[0][1]), FROMFX(c->body.m[0][0])) * 57.29578, FROMFX(c->body.m[2][2]), c->gear);
        }
        return 0;
    }
#endif
    for (t = 0; t + 1 < n; t += 2) {
        const uint8_t *in = ticks + (size_t)t * REC, *out = in + REC;
        const uint8_t *g = in + 6 + CARS * CAR_SIZE + CARS * PAD_SIZE;
        int slot = in[1], bad = 0;
        if (in[0] != 'I' || out[0] != 'O' || out[1] != slot) {
            printf("record %d: out of step\n", t);
            return 1;
        }
        g_world.walkBack = (int)be32(g);
        g_world.dt = TOFX(bef(g + 4));
        g_world.weather = (int)be32(g + 8);
        g_world.mode = (int)be32(g + 12);
        g_world.players = (int)be32(g + 16);
        if (!freerun || t == 0) {
            for (k = 0; k < CARS; k++)
            {
                const uint8_t *cr = in + 6 + k * CAR_SIZE;
                int i;
                sl_car_load(&s_cars[k], &s_pads[k], cr, in + 6 + CARS * CAR_SIZE + k * PAD_SIZE, be32(g + NGLOB * 4 + k * 4));
                for (i = 0; i < 3; i++)
                    s_cars[k].camView[i] = TOFX(bef(R(be32(cr + 0x2078)) + 0xB0 + i * 4));
            }
        } else {
            for (k = 0; k < CARS; k++) {           /* the pads as the cartridge had them */
                const uint8_t *pr = in + 6 + CARS * CAR_SIZE + k * PAD_SIZE;
                s_pads[k].flags = be32(pr);
                s_pads[k].steer = TOFX(bef(pr + 0x20));
                s_pads[k].kind = pr[0x25];
                s_cars[k].linkFlags = be32(g + NGLOB * 4 + k * 4);
            }
        }
        if (freerun) {                               /* what the race keeps outside the physics */
            for (k = 0; k < CARS; k++) {
                const uint8_t *cr = in + 6 + k * CAR_SIZE;
                s_cars[k].xfac = (int)be32(cr + 0xFAC);
                s_cars[k].xfa8 = TOFX(bef(cr + 0xFA8));
                s_cars[k].body.landed = cr[0x148 + 0x203];
            }
        }
#ifndef FX_FLOAT
        if (drive && getenv("SIM_EVERY")) {          /* (a tick every k frames, k/30 s long) */
            int every = atoi(getenv("SIM_EVERY"));
            g_sim_dt = FX(0.033333335) * every;
            g_sim_dtk = FX(1.0) * every;
            if ((t / (2 * CARS)) % every != 0)
                continue;
        }
#endif
        if (drive) {                                  /* steered back onto the cartridge's path */
            Car *c = &s_cars[slot];
            int ahead = t + 1 + 2 * CARS * 20, j;
            double px = FROMFX(c->body.st.pos[0]), py = FROMFX(c->body.st.pos[1]), dx, dy, fw, sd, st;
            if (ahead >= n)
                ahead = n - 1 - ((n - 1 - (t + 1)) % (2 * CARS));
            for (j = ahead; j > t && (ticks[(size_t)j * REC] != 'O'); j--)
                ;
            dx = bef(ticks + (size_t)j * REC + 6 + slot * CAR_SIZE + 0x1C0) - px;
            dy = bef(ticks + (size_t)j * REC + 6 + slot * CAR_SIZE + 0x1C4) - py;
            fw = dx * FROMFX(c->mtx0[0][0]) + dy * FROMFX(c->mtx0[0][1]);
            sd = dx * FROMFX(c->mtx0[1][0]) + dy * FROMFX(c->mtx0[1][1]);
            st = DRIVE_SIGN * atan2(sd, fabs(fw) + 4.0) * 1.2;
            if (st > 1) st = 1;
            if (st < -1) st = -1;
            s_pads[slot].steer = TOFX(st);
        }
        BrCarPhysTick(&s_cars[slot]);
        sim_car_wheels(&s_cars[slot]);       /* (the drawing's, compared too) */
        if (drive && slot == 0) {                     /* how far from the path, how fast */
            double px = FROMFX(s_cars[0].body.st.pos[0]), py = FROMFX(s_cars[0].body.st.pos[1]), best = 1e9;
            int j;
            for (j = t + 1 - 400; j <= t + 1 + 400; j += 2 * CARS) {
                double ex, ey;
                if (j < 1 || j >= n || ticks[(size_t)j * REC] != 'O')
                    continue;
                ex = bef(ticks + (size_t)j * REC + 6 + 0x1C0) - px;
                ey = bef(ticks + (size_t)j * REC + 6 + 0x1C4) - py;
                if (ex * ex + ey * ey < best)
                    best = ex * ex + ey * ey;
            }
            best = sqrt(best);
            offsum += best;
            if (best > offmax)
                offmax = best;
            vsum += FROMFX(s_cars[0].speedMph);
            vref += bef(out + 6 + 0xFE4);
            if (FROMFX(s_cars[0].speedMph) < 2 && bef(out + 6 + 0xFE4) > 20)
                stuck++;
        }
        checked++;
        if (traj) {                                   /* the path against the cartridge's */
            const uint8_t *cr = out + 6 + slot * CAR_SIZE;
            double dx = FROMFX(s_cars[slot].body.st.pos[0]) - bef(cr + 0x1C0);
            double dy = FROMFX(s_cars[slot].body.st.pos[1]) - bef(cr + 0x1C4);
            double dz = FROMFX(s_cars[slot].body.st.pos[2]) - bef(cr + 0x1C8);
            double dd = sqrt(dx * dx + dy * dy + dz * dz);
            sum += dd;
            nsum++;
            if (t % 200 == 0)
                printf("frame %4d car%d at %9.3f %9.3f %7.3f, %6.1f mph; the cartridge's %9.3f %9.3f %7.3f, %6.1f mph; apart %.2f\n",
                       t / 2, slot, FROMFX(s_cars[slot].body.st.pos[0]), FROMFX(s_cars[slot].body.st.pos[1]),
                       FROMFX(s_cars[slot].body.st.pos[2]), FROMFX(s_cars[slot].speedMph), bef(cr + 0x1C0), bef(cr + 0x1C4),
                       bef(cr + 0x1C8), bef(cr + 0xFE4), dd);
            checked++;
            continue;
        }
        if (freerun && t % 400 == 0)
            for (k = 0; k < CARS; k++) {
                const uint8_t *cr = out + 6 + k * CAR_SIZE;
                printf("frame %4d car%d at %9.3f %9.3f %7.3f, the cartridge's %9.3f %9.3f %7.3f\n", t / 2, k,
                       FROMFX(s_cars[k].body.st.pos[0]), FROMFX(s_cars[k].body.st.pos[1]), FROMFX(s_cars[k].body.st.pos[2]),
                       bef(cr + 0x1C0), bef(cr + 0x1C4), bef(cr + 0x1C8));
            }
        for (k = 0; k < CARS; k++)
            bad += car_cmp(&s_cars[k], out + 6 + k * CAR_SIZE, k, freerun ? &worst : 0);
        if (bad) {
            fails++;
            if (fails <= 5 || freerun)
                printf("physics frame %d (car %d, retrace %u): %d fields differ\n", t / 2, slot, be32(in + 2), bad);
            if (!freerun && fails >= 5)
                break;
            if (freerun && fails >= 40)
                break;
        }
    }
    if (drive)
        printf("driven: off the path %.2f on average, %.2f at most; %.1f mph on average, the cartridge's %.1f; %d frames stuck\n",
               offsum / (checked / 2), offmax, vsum / (checked / 2), vref / (checked / 2), stuck);
    if (traj) {
        printf("%d physics frames run, the cars on average %.2f from the cartridge's\n", checked, sum / nsum);
        return 0;
    }
    printf("%d physics frames checked, %d differ\n", checked, fails);
    return fails != 0;
}
