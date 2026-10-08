/* arcade.c -- the GBA's own car physics: far simpler than the game's (sim/, which the GBA
 * cannot run 30 times a second), stepped 30 times a second like the game's.
 *
 * The car is a point with a heading, a speed along it and a slide across it.  It sits on the
 * highest floor triangle under it within a step's reach (floors face up more than FLOOR), its
 * nose pitched and its body rolled to that triangle; with no floor under where it would go it
 * stays where it was (the world's edge, a drop it cannot have).  Steep triangles are walls: the
 * way from where it was to where it would be is tested against each one near it, so no speed
 * takes it through; a wall pushes it out to RADIUS and takes the speed into it away.  Off the
 * ground it falls.  The engine pulls less as the speed nears the top; brakes, drag and the
 * slide's grip slow it; steering turns it by the speed and the lock, the lock narrowing with
 * speed (a bicycle's turn).  The in-car camera sits at the game's lens offsets for the car. */
#include <stdint.h>
#include "world.h"
#include "race.h"
SimCell BrCollGridCellAcquire(fx x, fx y);
fx BrAtan2(fx x, fx y);

#define DT FX(1.0 / 30)
#define VMAX FX(54.0)                    /* top speed, units (metres) a second: 121 mph */
#define ACCEL FX(15.0)                   /* the engine's pull from rest */
#define BRAKE FX(26.0)
#define REVERSE FX(9.0)                  /* the fastest backwards */
#define DRAG FX(1.2)                     /* rolling, off the throttle */
#define GRAV FX(19.6)                    /* the fall (the game's world is a little heavy) */
#define GRIP FX(0.30)                    /* the slide lost each tick */
#define LOCK FX(0.55)                    /* the steering at rest, radians */
#define LOCKHI FX(0.10)                  /* at the top speed */
#define BASE FX(2.6)                     /* the wheelbase */
#define RIDE FX(0.0)                     /* the car's point above the ground */
#define STEP FX(1.2)                     /* the highest step up it drives over */
#define FLOOR FX(0.55)                   /* a floor faces up more than this */
#define RADIUS FX(1.1)                   /* how near a wall it comes */
#define PIVOT FX(4.0)                    /* the least speed it steers as, on the pedals */

static fx s_pos[3], s_vel, s_side, s_vz, s_yaw, s_nrm[3], s_eye[3];
static fx s_view[3];
static int s_air;
volatile int32_t g_arc[4];               /* (the host's view: x, y, z in 1/16 units, the speed in mph) */

static fx dot3(const fx *a, const fx *b) { return FMUL(a[0], b[0]) + FMUL(a[1], b[1]) + FMUL(a[2], b[2]); }

/* the floor under (x, y) within a step of z: its height, its normal; 0 for none */
static int ground(const fx *p, fx *gz, fx *gn)
{
    SimCell cell = BrCollGridCellAcquire(p[0], p[1]);
    uint16_t pick[256];
    int32_t q[6];
    int k, n, found = 0;
    fx best = -FX(30000.0);
    sim_pick_box(q, p, FX(0.05));
    q[4] = (int32_t)(((p[2] - FX(40.0)) >> 29)) - 1;    /* (down far, up a step: in eighths) */
    q[5] = (int32_t)(((p[2] + STEP) >> 29)) + 2;
    n = sim_cell_pick(&cell, q, pick);
    for (k = 0; k < n; k++) {
        const Plane *pl = &g_track->planes[cell.tris[pick[k]]];
        fx z, pt[3];
        if (pl->n[2] <= FLOOR)
            continue;
        z = -FDIV(FMUL(pl->n[0], p[0]) + FMUL(pl->n[1], p[1]) + pl->d, pl->n[2]);
        if (z > p[2] + STEP || z <= best)
            continue;
        pt[0] = p[0];
        pt[1] = p[1];
        pt[2] = z;
        if (!sim_tri_contains(pl, pt))
            continue;
        best = z;
        gn[0] = pl->n[0];
        gn[1] = pl->n[1];
        gn[2] = pl->n[2];
        found = 1;
    }
    *gz = best;
    return found;
}

/* the walls between a and b (b moved out of them); the first's normal into wn: 1 if any */
static int walls(const fx *a, fx *b, fx *wn)
{
    fx mid[3], r = FX(4.0);
    SimCell cell;
    uint16_t pick[256];
    int32_t q[6];
    int k, n, hit = 0, i;
    for (i = 0; i < 3; i++)
        mid[i] = (a[i] + b[i]) >> 1;
    mid[2] += FX(0.8);
    cell = BrCollGridCellAcquire(mid[0], mid[1]);
    sim_pick_box(q, mid, r);
    n = sim_cell_pick(&cell, q, pick);
    for (k = 0; k < n; k++) {
        const Plane *pl = &g_track->planes[cell.tris[pick[k]]];
        fx hn[3], l, d0, d1, pa[3], pb[3], c[3], t;
        if (pl->n[2] > FLOOR)
            continue;
        hn[0] = pl->n[0];                    /* the wall's facing on the ground */
        hn[1] = pl->n[1];
        hn[2] = 0;
        l = FSQRT(FMUL(hn[0], hn[0]) + FMUL(hn[1], hn[1]));
        if (l < FX(0.2))
            continue;
        hn[0] = FDIV(hn[0], l);
        hn[1] = FDIV(hn[1], l);
        for (i = 0; i < 3; i++) {
            pa[i] = a[i];
            pb[i] = b[i];
        }
        pa[2] += FX(0.8);
        pb[2] += FX(0.8);
        d0 = dot3(pl->n, pa) + pl->d;            /* (the plane's own distances) */
        d1 = dot3(pl->n, pb) + pl->d;
        if (d0 < -FX(0.3) || d1 >= FMUL(RADIUS, l))
            continue;                            /* behind it at the start, or clear of it */
        /* where the way meets the wall pushed out by the radius: on the triangle? */
        t = d0 > d1 ? FDIV(d0 - FMUL(RADIUS, l), d0 - d1) : 0;
        if (t < 0)
            t = 0;
        if (t > FX(1.0))
            t = FX(1.0);
        for (i = 0; i < 3; i++)
            c[i] = pa[i] + FMUL(pb[i] - pa[i], t);
        {   fx s = dot3(pl->n, c) + pl->d;       /* onto the plane */
            for (i = 0; i < 3; i++)
                c[i] -= FMUL(pl->n[i], s);
        }
        if (!sim_tri_contains(pl, c))
            continue;
        {   fx push = FDIV(FMUL(RADIUS, l) - d1, l);   /* out to the radius, on the ground */
            b[0] += FMUL(hn[0], push);
            b[1] += FMUL(hn[1], push);
        }
        if (!hit) {
            wn[0] = hn[0];
            wn[1] = hn[1];
        }
        hit = 1;
    }
    return hit;
}

void arc_init(const fx m[4][4], const fx view[3])
{
    fx gz, gn[3];
    int i;
    for (i = 0; i < 3; i++) {
        s_pos[i] = m[3][i];
        s_view[i] = view[i];
    }
    s_yaw = BrAtan2(m[0][0], m[0][1]);
    s_vel = s_side = s_vz = 0;
    s_nrm[0] = s_nrm[1] = 0;
    s_nrm[2] = FX(1.0);
    if (ground(s_pos, &gz, gn)) {
        s_pos[2] = gz + RIDE;
        for (i = 0; i < 3; i++)
            s_nrm[i] = gn[i];
    }
    s_air = 0;
    for (i = 0; i < 3; i++)
        s_eye[i] = s_pos[i];
}

fx arc_speed_mph(void);

void arc_tick(uint32_t keys)
{
    uint32_t down = ~keys & 0x3FF;
    fx cs = FCOS(s_yaw), sn = FSIN(s_yaw), fwd[3], side[3], vw[3], np[3], gz, gn[3], wn[3], steer, lock, a;
    int i;

    fwd[0] = cs;                             /* the heading on the ground; the slope's pull */
    fwd[1] = sn;
    fwd[2] = 0;
    side[0] = -sn;
    side[1] = cs;
    side[2] = 0;
    a = 0;
    if (down & 1)
        a += FMUL(ACCEL, FX(1.0) - FDIV(s_vel > 0 ? s_vel : 0, VMAX));
    if (down & 2)
        a -= s_vel > FX(0.5) ? BRAKE : (s_vel > -REVERSE ? FX(6.0) : 0);
    if (!(down & 3))
        a -= s_vel > FX(0.2) ? DRAG : s_vel < -FX(0.2) ? -DRAG : 0;
    if (!s_air)                              /* down the slope it is on */
        a -= FMUL(GRAV, FMUL(s_nrm[0], -fwd[0]) + FMUL(s_nrm[1], -fwd[1])) >> 1;
    s_vel += FMUL(a, DT);
    if (!(down & 3) && s_vel < FX(0.2) && s_vel > -FX(0.2))
        s_vel = 0;
    if (s_vel > VMAX)
        s_vel = VMAX;

    steer = (down & 0x10) ? -FX(1.0) : (down & 0x20) ? FX(1.0) : 0;   /* right: clockwise */
    lock = LOCK - FMUL(LOCK - LOCKHI, FDIV(s_vel > 0 ? s_vel : -s_vel, VMAX));
    if (!s_air) {                            /* (on the pedals it turns as if moving at PIVOT at least:
                                                not stuck nose to a wall) */
        fx vs = s_vel;
        if ((down & 3) && vs < PIVOT && vs > -PIVOT)
            vs = (down & 1) ? PIVOT : -PIVOT;
        s_yaw += FMUL(FDIV(FMUL(vs, FMUL(steer, lock)), BASE), DT);
    }
    s_side -= FMUL(s_side, GRIP);

    for (i = 0; i < 3; i++)                  /* where it would go */
        vw[i] = FMUL(fwd[i], s_vel) + FMUL(side[i], s_side);
    np[0] = s_pos[0] + FMUL(vw[0], DT);
    np[1] = s_pos[1] + FMUL(vw[1], DT);
    np[2] = s_pos[2];
    if (walls(s_pos, np, wn)) {              /* the speed into the wall taken away, as much again lost */
        fx into = FMUL(vw[0], wn[0]) + FMUL(vw[1], wn[1]);   /* to the scrape along it */
        if (into < 0) {
            fx v2 = FSQRT(FMUL(vw[0], vw[0]) + FMUL(vw[1], vw[1])), keep;
            vw[0] -= FMUL(wn[0], into);
            vw[1] -= FMUL(wn[1], into);
            keep = v2 > FX(0.01) ? FX(1.0) + FDIV(into, v2) : FX(1.0);   /* 1 - |into| / |v| */
            vw[0] = FMUL(vw[0], keep);
            vw[1] = FMUL(vw[1], keep);
            s_vel = FMUL(vw[0], fwd[0]) + FMUL(vw[1], fwd[1]);
            s_side = FMUL(vw[0], side[0]) + FMUL(vw[1], side[1]);
        }
    }
    if (!ground(np, &gz, gn)) {              /* no floor there: it stays, its speed turned back */
        np[0] = s_pos[0];
        np[1] = s_pos[1];
        s_vel = FMUL(s_vel, -FX(0.3));
        s_side = 0;
        if (!ground(np, &gz, gn)) {
            gz = s_pos[2];
            gn[0] = s_nrm[0];
            gn[1] = s_nrm[1];
            gn[2] = s_nrm[2];
        }
    }
    if (s_air || s_pos[2] > gz + RIDE + FX(0.3)) {   /* off the ground: falling */
        s_vz -= FMUL(GRAV, DT);
        np[2] = s_pos[2] + FMUL(s_vz, DT);
        s_air = np[2] > gz + RIDE;
        if (!s_air) {
            np[2] = gz + RIDE;
            s_vz = 0;
        }
    } else {
        fx rise = gz + RIDE - s_pos[2];          /* over a crest fast: off the ground */
        np[2] = gz + RIDE;
        s_vz = FMUL(rise, FX(30.0));
        if (s_vz < 0)
            s_vz = 0;
    }
    for (i = 0; i < 3; i++) {
        s_pos[i] = np[i];
        s_nrm[i] += (gn[i] - s_nrm[i]) >> 2;     /* (the body eased onto the new ground) */
        g_arc[i] = (int32_t)(s_pos[i] >> 28);
    }
    g_arc[3] = FTOI(arc_speed_mph());
}

fx arc_speed_mph(void)
{
    fx v = s_vel < 0 ? -s_vel : s_vel;
    return FMUL(v, FX(2.24));
}

/* the in-car camera, the game's rows (forward, side, up, position) and lens */
void arc_camera(fx out[17], fx lens)
{
    fx cs = FCOS(s_yaw), sn = FSIN(s_yaw), f[3], s[3], u[3], l;
    int i;
    u[0] = s_nrm[0];
    u[1] = s_nrm[1];
    u[2] = s_nrm[2];
    l = FSQRT(dot3(u, u));
    for (i = 0; i < 3; i++)
        u[i] = FDIV(u[i], l);
    s[0] = -sn;                              /* side: the heading's left, then forward = side x up */
    s[1] = cs;
    s[2] = 0;
    f[0] = FMUL(s[1], u[2]) - FMUL(s[2], u[1]);
    f[1] = FMUL(s[2], u[0]) - FMUL(s[0], u[2]);
    f[2] = FMUL(s[0], u[1]) - FMUL(s[1], u[0]);
    l = FSQRT(dot3(f, f));
    for (i = 0; i < 3; i++)
        f[i] = FDIV(f[i], l);
    s[0] = FMUL(u[1], f[2]) - FMUL(u[2], f[1]);
    s[1] = FMUL(u[2], f[0]) - FMUL(u[0], f[2]);
    s[2] = FMUL(u[0], f[1]) - FMUL(u[1], f[0]);
    for (i = 0; i < 3; i++) {
        out[i] = f[i];
        out[4 + i] = s[i];
        out[8 + i] = u[i];
        out[12 + i] = s_pos[i] + FMUL(f[i], s_view[0]) + FMUL(u[i], s_view[2]);
    }
    out[3] = out[7] = out[11] = 0;
    out[15] = FX(1.0);
    out[16] = lens;
}
