/* night_test.m: the night's checks (MEASUREMENTS.md, "Night: the moon, the
 * eye, the glow"), no game run.  Includes the renderer (brr_metal.m) for its
 * weather table; the night's maths is light_night.h, the track's lights
 * light_world.c.
 *   1 the moon's illuminance against the published full moon
 *   2 the Purkinje weight: 0 photopic, 1 scotopic, monotonic
 *   3 the meter: a moonlit scene (albedo 0.2, 0.25 lux) to the night's grey
 *   4 the glow: a car's texture never glows
 *   5 the track's lights carry the windows' flux */
#include "brr_metal.m"
#include "light_world.h"

int plat_flag(int flag) { (void)flag; return 0; }
int host_window_visible(void) { return 0; }
int light_capture_camera(LightCamera *c) { (void)c; return 0; }
int light_capture_eye(float eye[3]) { (void)eye; return 0; }
const LightTrack *light_game_track(void) { return NULL; }
int light_game_weather(void) { return 0; }
int light_glide_flip(void) { return 0; }
int light_capture_ontrack(const float tri[9]) { (void)tri; return 0; }
int light_scene_spots(LightSpot *out, int max) { (void)out; (void)max; return 0; }
int brr_rt_update(BrrRtWorld *w, id<MTLDevice> dev, id<MTLCommandQueue> q, const LightTrack *t)
{
    (void)w, (void)dev, (void)q, (void)t;
    return 0;
}

static int g_fail;
#define CHECK(ok, ...) do { int ok_ = (ok); printf("%s  ", ok_ ? "PASS" : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!ok_) g_fail = 1; } while (0)

int main(void)
{
    int i;
    {   /* 1 the moon */
        float e90 = light_moon_ground(90), e45 = light_moon_ground(45), el = light_moon_ground(LIGHT_MOON_ELEV);
        /* published: a full moon overhead about 0.25 lux at the ground (Krisciunas and Schaefer 1991, Kyba et al. 2017),
           0.1 to 0.3 lux high in the sky */
        CHECK(fabsf(e90 - 0.25f) / 0.25f < 0.10f, "moon overhead %.4f lux on the ground (published 0.25, within 10%%)", e90);
        CHECK(e45 > 0.1f && e45 < 0.3f && el > 0.1f && el < 0.3f, "moon at 45 deg %.4f lux, at %.0f deg (Coastline) %.4f lux (published 0.1..0.3)",
              e45, LIGHT_MOON_ELEV, el);
        CHECK(fabsf(k_weather[4].sun - light_moon_normal(LIGHT_MOON_ELEV)) / k_weather[4].sun < 0.01f,
              "the weather table's moon %.4f lux = light_moon_normal(%.0f) %.4f", k_weather[4].sun, LIGHT_MOON_ELEV, light_moon_normal(LIGHT_MOON_ELEV));
        CHECK(k_weather[4].sky > 0.0005f && k_weather[4].sky < 0.002f && k_weather[4].skyc[2] > k_weather[4].skyc[0],
              "night sky %.4f lux, blue (starlight and airglow ~0.001)", k_weather[4].sky);
    }
    {   /* 2 the Purkinje weight */
        float prev = 2.0f;
        int mono = 1;
        for (i = 0; i <= 200; i++) {
            float L = powf(10.0f, -5.0f + i * 0.04f), wv = light_purkinje_w(L);
            if (wv > prev + 1e-7f)
                mono = 0;
            prev = wv;
        }
        CHECK(light_purkinje_w(3.0f) == 0.0f && light_purkinje_w(100.0f) == 0.0f && light_purkinje_w(30000.0f) == 0.0f,
              "Purkinje weight photopic (3, 100, 30000 cd/m2): %g %g %g", light_purkinje_w(3.0f), light_purkinje_w(100.0f), light_purkinje_w(30000.0f));
        CHECK(light_purkinje_w(0.001f) == 1.0f && light_purkinje_w(1e-5f) == 1.0f,
              "Purkinje weight scotopic (0.001, 1e-5 cd/m2): %g %g", light_purkinje_w(0.001f), light_purkinje_w(1e-5f));
        CHECK(mono, "Purkinje weight monotonic over 1e-5..1e3 cd/m2; at 0.01 %.3f, 0.1 %.3f, 1 %.3f",
              light_purkinje_w(0.01f), light_purkinje_w(0.1f), light_purkinje_w(1.0f));
    }
    {   /* 3 the meter: albedo 0.2 under 0.25 lux, L = 0.2 x 0.25 / pi; most of the frame, and a headlamp pool
           (1,000 times brighter) on 15% of it */
        uint32_t h[LUM_BINS];
        float L = 0.2f * 0.25f / (float)M_PI, Lp = L * 1000.0f, ev, ex, out, grey, comp = k_weather[4].comp;
        memset(h, 0, sizeof h);
        h[(int)((log2f(L) + 10.0f) * 16.0f)] = 8500;
        h[(int)((log2f(Lp) + 10.0f) * 16.0f)] = 1500;
        ev = light_meter_ev(h, LUM_BINS, k_weather[4].plo, k_weather[4].phi, k_weather[4].evlo, k_weather[4].evhi, comp);
        ex = 1.0f / (1.2f * exp2f(ev));
        out = L * ex;
        grey = 1.0f / (1.2f * 8.0f);   /* what the meter makes of what it metered with no compensation: L x 1/(1.2 L 100/12.5) */
        CHECK(fabsf(log2f(out / grey) - comp) < 0.1f,
              "moonlit scene L %.4f cd/m2 meters to EV %.2f; exposed %.4f = mid-grey %.4f x 2^%.2f (compensation %.1f, within 0.1 EV)",
              L, ev, out, grey, log2f(out / grey), comp);
        CHECK(ev > k_weather[4].evlo && ev < k_weather[4].evhi, "night EV range %.1f..%.1f holds it", k_weather[4].evlo, k_weather[4].evhi);
        ev = light_meter_ev(h, LUM_BINS, 0.80f, 0.983f, -2.5f, 7.5f, 0);
        printf("INFO  the old meter (80..98.3%%, EV -2.5..7.5): EV %.2f, the moonlit ground %.1f stops under mid-grey\n",
               ev, log2f(grey / (L / (1.2f * exp2f(ev)))));
    }
    {   /* 4 the glow: a car's texture never glows, whatever order it is drawn in */
        static LightGlow g;
        int bad = 0, t;
        memset(&g, 0, sizeof g);
        for (t = 1; t < 64; t++)
            light_glow_new(&g, (uint32_t)t);
        for (i = 0; i < 10000; i++) {
            uint32_t car = 1 + (uint32_t)(i % 31), trk = 32 + (uint32_t)(i % 31);
            /* a car texture: drawn off the track, and sometimes a triangle of it over the track (the car on the road) */
            light_glow_drawn(&g, car, i >= 31 && (i % 7) == 0);   /* its first draws are its body, off the track */
            light_glow_drawn(&g, trk, 1);
            bad += light_glow_on(&g, car);
            bad += !light_glow_on(&g, trk);
        }
        light_glow_new(&g, 40);
        CHECK(bad == 0 && !light_glow_on(&g, 40) && !light_glow_on(&g, 5000),
              "glow: no car texture emissive in 10000 draws, every track texture is; a new texture starts unseen");
        light_glow_new(&g, 7);
        light_glow_drawn(&g, 7, 1);
        light_glow_drawn(&g, 7, 0);
        light_glow_drawn(&g, 7, 1);
        CHECK(!light_glow_on(&g, 7), "glow: a texture once drawn on a car never glows again until it is remade");
    }
    {   /* 5 the windows: a street of 3 buildings, 40 windows each (1.2 x 1.5 m, 100 cd/m2 x 60% of texels) */
        LightEmitter e[120];
        LightSpot s[LIGHT_WORLD_MAX];
        double flux = 0;
        int n, b, wdw;
        for (b = 0; b < 3; b++)
            for (wdw = 0; wdw < 40; wdw++) {
                LightEmitter *o = &e[b * 40 + wdw];
                memset(o, 0, sizeof *o);
                o->p[0] = b * 30.0f + (wdw % 10) * 2.0f;
                o->p[1] = b == 1 ? 12.0f : 0.0f;
                o->p[2] = 2.0f + (wdw / 10) * 3.0f;
                o->n[1] = b == 1 ? -1.0f : 1.0f;
                o->area = 1.2f * 1.5f;
                o->lum = 100.0f * 0.6f;
                o->rgb[0] = 1.0f; o->rgb[1] = 0.8f; o->rgb[2] = 0.5f;
                flux += M_PI * o->lum * o->area;
            }
        n = light_world_cluster(e, 120, s, LIGHT_WORLD_MAX);
        CHECK(fabs(light_world_flux(s, n) - flux) / flux < 0.05,
              "windows: 120 emitters to %d lights, %.0f lm against the surfaces' %.0f lm (%.3f%%)", n, light_world_flux(s, n), flux,
              100.0 * fabs(light_world_flux(s, n) - flux) / flux);
        {
            int front = 1;
            for (i = 0; i < n; i++)
                front &= s[i].pos[1] < 0.5f || s[i].pos[1] > 11.5f ? (s[i].pos[1] > 6 ? s[i].pos[1] < 12.0f : s[i].pos[1] > 0.0f) : 0;
            CHECK(front, "windows: every light in front of its wall (0.3 m along the normal)");
        }
        {
            LightSpot lh;
            float p[3] = { 0, 0, 30 };
            light_world_lighthouse(&lh, p, 9.6f, LIGHT_LH_DEG_S);
            CHECK(fabsf(lh.axis[0] - 1.0f) < 1e-3f && lh.kind == LIGHT_SPOT && lh.cosout > 0.99f,
                  "lighthouse: one turn in %.1f s (axis back to +x: %.4f %.4f)", 360.0f / LIGHT_LH_DEG_S, lh.axis[0], lh.axis[1]);
        }
    }
    printf(g_fail ? "night: FAIL\n" : "night: all pass\n");
    return g_fail;
}
