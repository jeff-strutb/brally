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
#include "brr_metal_taa.m"
#include "light_world.h"

#define LT_HAVE_LIGHT_WORLD
#include "game_stubs.h"

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
        CHECK(e45 > 0.1f && e45 < 0.3f, "moon at 45 deg %.4f lux on the ground (published 0.1..0.3 for a moon high in the sky)", e45);
        /* a low full moon (about 10 degrees, air mass about 5): published 0.02 to 0.05 lux on level ground */
        CHECK(el > 0.02f && el < 0.05f, "moon at %.0f deg (Coastline, low) %.4f lux on the ground (published 0.02..0.05)", LIGHT_MOON_ELEV, el);
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
    {   /* 6 the gather: the same window triangles drawn frame after frame count once; their flux kept */
        enum { NW = 60 };
        float tri[2 * NW * 9], eye[3] = { 20, -30, 5 }, rgb[3] = { 1.0f, 0.6f, 0.25f }, share = 0.35f;
        LightSpot s[LIGHT_WORLD_MAX];
        double flux = 0;
        int f, n, face = 1;
        for (i = 0; i < NW; i++) {   /* windows 1.2 x 1.5 m in a wall at y = 10 facing -y (toward the eye), two triangles each */
            float x0 = (i % 15) * 2.5f, z0 = 1.0f + (i / 15) * 3.0f, *a = &tri[18 * i];
            float q[18] = { x0, 10, z0, x0 + 1.2f, 10, z0, x0 + 1.2f, 10, z0 + 1.5f,
                            x0, 10, z0, x0 + 1.2f, 10, z0 + 1.5f, x0, 10, z0 + 1.5f };
            memcpy(a, q, sizeof q);
            flux += M_PI * 100.0 * share * 1.2 * 1.5;
        }
        for (f = 0; f < 5; f++)
            light_world_gather(7, tri, 2 * NW, share, rgb, eye);
        for (f = 0; f < 61; f++)
            n = light_world_refresh();
        n = light_world_spots(s, LIGHT_WORLD_MAX, 0);
        for (i = 0; i < n; i++)
            face &= s[i].pos[1] < 10.0f;
        CHECK(light_world_emitters() == 2 * NW && fabs(light_world_flux(s, n) - flux) / flux < 0.05 && face,
              "gather: %d triangles over 5 frames kept once (%d), %d lights, %.0f lm against %.0f lm, all on the eye's side",
              2 * NW, light_world_emitters(), n, light_world_flux(s, n), flux);
        light_world_gather(8, tri, 0, share, rgb, eye);
        CHECK(light_world_emitters() == 0, "gather: a new track generation starts empty");
    }
    {   /* 7 the lighthouses on Coastline's track (the OBJ fixture's instance records) */
        const char *path = getenv("TRACK_OBJ");
        FILE *fo = path ? fopen(path, "r") : NULL;
        if (!fo) {
            printf("SKIP  lighthouses: no track fixture (TRACK_OBJ)\n");
        } else {
            char line[1024];
            int nb = 0, ids[8];
            float bp[8][3], bx[8][3];
            while (fgets(line, sizeof line, fo) && line[0] == '#') {
                int o;
                unsigned f0, f1, f2, f3, dl;
                float m[16];
                if (sscanf(line, "# o%d flags %x %x %x %x dl %x m %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f", &o, &f0, &f1, &f2, &f3, &dl,
                           &m[0], &m[1], &m[2], &m[3], &m[4], &m[5], &m[6], &m[7], &m[8], &m[9], &m[10], &m[11], &m[12], &m[13], &m[14], &m[15]) != 22)
                    continue;
                if (light_beam_record((unsigned char)f0, (unsigned char)f1) && nb < 8) {
                    ids[nb] = o;
                    memcpy(bp[nb], m + 12, 12);
                    memcpy(bx[nb], m, 12);
                    nb++;
                }
            }
            fclose(fo);
            CHECK(nb == 2 && ids[0] == 64 && ids[1] == 142, "lighthouses: %d beam instances (%d, %d)", nb, nb > 0 ? ids[0] : -1, nb > 1 ? ids[1] : -1);
            for (i = 0; i < nb; i++) {
                LightSpot lh;
                light_world_beam(&lh, bp[i], bx[i]);
                printf("INFO  lighthouse o%d at %.2f %.2f %.2f, beam toward %.3f %.3f %.3f\n", ids[i], lh.pos[0], lh.pos[1], lh.pos[2],
                       lh.axis[0], lh.axis[1], lh.axis[2]);
            }
        }
    }
    {   /* 9 lamplight only from blended decals: a warm sandy texture passes lit_emask's colour test, but drawn
           opaque (Glide ONE, ZERO, as the rock and hill walls are) it neither glows nor makes lights; the
           same texels as a blended window decal (SRC_ALPHA, ONE_MINUS_SRC_ALPHA) do, their flux kept */
        enum { TW = 16 };
        static uint8_t sand[TW * TW * 4];
        float tri[6 * 9], eye[3] = { 5, -30, 3 }, rgb[3];
        LightSpot s[LIGHT_WORLD_MAX];
        double flux = 0;
        int n, nl_opq, nl_bl, q;
        for (i = 0; i < TW * TW; i++) {   /* sandstone: r 205, g 170, b 125 */
            sand[4 * i] = 205; sand[4 * i + 1] = 170; sand[4 * i + 2] = 125; sand[4 * i + 3] = 255;
        }
        lit_emask(9, sand, TW, TW);
        printf("INFO  a sandstone texture's glowing share by colour alone: %.2f (the colour test passes it)\n", s_eshare[9]);
        for (q = 0; q < 3; q++) {   /* three 2 x 2 m patches of wall at y = 10 facing the eye */
            float x0 = q * 4.0f, *a = &tri[18 * q];
            float p[18] = { x0, 10, 0, x0 + 2, 10, 0, x0 + 2, 10, 2, x0, 10, 0, x0 + 2, 10, 2, x0, 10, 2 };
            memcpy(a, p, sizeof p);
            flux += M_PI * 100.0 * s_eshare[9] * 4.0;
        }
        memcpy(rgb, s_ecol[9], sizeof rgb);
        /* the renderer's gate (brr_metal.m brr_draw): gather only when light_draw_may_glow */
        light_world_gather(20, tri, light_draw_may_glow(4, 0) ? 6 : 0, s_eshare[9], rgb, eye);
        for (q = 0; q < 61; q++)
            light_world_refresh();
        nl_opq = light_world_spots(s, LIGHT_WORLD_MAX, 0);
        CHECK(!light_draw_may_glow(4, 0) && light_world_emitters() == 0 && nl_opq == 0,
              "opaque sandstone (ONE, ZERO): may glow %d, %d emitters, %d lights", light_draw_may_glow(4, 0), light_world_emitters(), nl_opq);
        light_world_gather(21, tri, light_draw_may_glow(1, 5) ? 6 : 0, s_eshare[9], rgb, eye);
        for (q = 0; q < 61; q++)
            light_world_refresh();
        n = nl_bl = light_world_spots(s, LIGHT_WORLD_MAX, 0);
        CHECK(light_draw_may_glow(1, 5) && light_draw_may_glow(4, 4) && light_world_emitters() == 6 && n > 0
              && fabs(light_world_flux(s, n) - flux) / flux < 0.05,
              "blended decal (SRC_ALPHA, 1-SRC_ALPHA): %d emitters, %d lights, %.0f lm against %.0f lm; additive may glow too", light_world_emitters(),
              nl_bl, light_world_flux(s, n), flux);
    }
    {   /* 10 the lighthouse beam: which draws are it, and its light on the cone's facets */
        enum { NF = 10 };
        float apex[1][3] = { { 1296.18f, 853.266f, 17.1899f } }, cone[NF * 9], hill[9] = { 1290, 850, 10, 1300, 850, 12, 1295, 860, 30 };
        float L = 77.0f, R = 9.0f, wb = 1.0f, worst_new = 0, worst_old = 0;
        int f;
        for (f = 0; f < NF; f++) {   /* a cone fanned from (near) the apex along -y, 77 m long, 10 facets */
            float a0 = 2.0f * (float)M_PI * f / NF, a1 = 2.0f * (float)M_PI * (f + 1) / NF, *t = &cone[9 * f];
            t[0] = apex[0][0] + 0.2f; t[1] = apex[0][1] - 0.4f; t[2] = apex[0][2];
            t[3] = apex[0][0] + R * cosf(a0); t[4] = apex[0][1] - L; t[5] = apex[0][2] + R * sinf(a0);
            t[6] = apex[0][0] + R * cosf(a1); t[7] = apex[0][1] - L; t[8] = apex[0][2] + R * sinf(a1);
        }
        CHECK(light_beam_draw(cone, NF, (const float (*)[3])apex, 1) && !light_beam_draw(hill, 1, (const float (*)[3])apex, 1)
              && !light_beam_draw(cone, NF, (const float (*)[3])apex, 0),
              "beam draws: the cone fanned from the beam instance is the beam; a hill triangle beside it is not; none without an instance");
        /* the shader (brr_metal_lit.h fsf): a beam fragment is lin(colour) x 100 x white balance, the colour the
           game's Gouraud gradient (1 at the apex to 0 at the end); the old path lit it as a surface, Lambert on the
           facet's own normal (facen).  On each shared edge, at 9 points along it, both facets' values. */
        for (f = 0; f < NF; f++) {
            float *t = &cone[9 * f], *u2 = &cone[9 * ((f + 1) % NF)], na[3], nbv[3], l[3] = { 0.3f, 0.2f, 0.93f };
            float e1[3] = { t[3] - t[0], t[4] - t[1], t[5] - t[2] }, e2[3] = { t[6] - t[0], t[7] - t[1], t[8] - t[2] };
            float g1[3] = { u2[3] - u2[0], u2[4] - u2[1], u2[5] - u2[2] }, g2[3] = { u2[6] - u2[0], u2[7] - u2[1], u2[8] - u2[2] };
            int k;
            na[0] = e1[1] * e2[2] - e1[2] * e2[1]; na[1] = e1[2] * e2[0] - e1[0] * e2[2]; na[2] = e1[0] * e2[1] - e1[1] * e2[0];
            nbv[0] = g1[1] * g2[2] - g1[2] * g2[1]; nbv[1] = g1[2] * g2[0] - g1[0] * g2[2]; nbv[2] = g1[0] * g2[1] - g1[1] * g2[0];
            {
                float la = sqrtf(na[0] * na[0] + na[1] * na[1] + na[2] * na[2]), lb = sqrtf(nbv[0] * nbv[0] + nbv[1] * nbv[1] + nbv[2] * nbv[2]);
                for (k = 0; k < 3; k++) { na[k] /= la; nbv[k] /= lb; }
            }
            for (k = 1; k <= 9; k++) {
                float s01 = k / 10.0f, col = 1.0f - s01;   /* the shared edge apex -> t[6..8] = u2[3..5]: the gradient's value there */
                float ea = powf(col, 2.2f) * 100.0f * wb, eb = powf(col, 2.2f) * 100.0f * wb;   /* each facet's beam emission: no normal in it */
                float oa = powf(col, 2.2f) * fmaxf(na[0] * l[0] + na[1] * l[1] + na[2] * l[2], 0.0f);
                float ob = powf(col, 2.2f) * fmaxf(nbv[0] * l[0] + nbv[1] * l[1] + nbv[2] * l[2], 0.0f);
                worst_new = fmaxf(worst_new, fabsf(ea - eb) / fmaxf(fmaxf(ea, eb), 1e-6f));
                if (fmaxf(oa, ob) > 1e-6f)
                    worst_old = fmaxf(worst_old, fabsf(oa - ob) / fmaxf(oa, ob));
            }
        }
        CHECK(worst_new <= 0.01f, "beam emission: facets' values meet on every shared edge (worst step %.4f%%, limit 1%%); "
              "lit as a surface on facet normals the steps were up to %.1f%%", 100.0f * worst_new, 100.0f * worst_old);
        CHECK(LIGHT_BEAM_VOLW > 0.0f && LIGHT_BEAM_VOLW < 1.0f, "beam polygons at %.2f of their light with the volumetric fog on (the spot's in-scatter carries the beam)",
              LIGHT_BEAM_VOLW);
        {   /* the fixture's own beam triangles (Coastline 64 and 142) */
            const char *path = getenv("TRACK_OBJ");
            FILE *fo = path ? fopen(path, "r") : NULL;
            if (!fo) {
                printf("SKIP  beam draws on the fixture: no TRACK_OBJ\n");
            } else {
                static float vv[200000][3];
                char line[1024], grp[32] = "";
                int nv = 0, nbeam = 0, okb = 0, nother = 0, other = 0;
                float lp[2][3] = { { 1296.18f, 853.266f, 17.1899f }, { 324.469f, 1194.27f, 40.3786f } };
                while (fgets(line, sizeof line, fo)) {
                    if (line[0] == 'v' && line[1] == ' ' && nv < 200000) {
                        sscanf(line + 2, "%f %f %f", &vv[nv][0], &vv[nv][1], &vv[nv][2]);
                        nv++;
                    } else if (line[0] == 'g' && line[1] == ' ') {
                        sscanf(line + 2, "%31s", grp);
                    } else if (line[0] == 'f' && line[1] == ' ') {
                        int a, b, c;
                        float t9[9];
                        if (sscanf(line + 2, "%d %d %d", &a, &b, &c) != 3 || a > nv || b > nv || c > nv)
                            continue;
                        memcpy(t9, vv[a - 1], 12); memcpy(t9 + 3, vv[b - 1], 12); memcpy(t9 + 6, vv[c - 1], 12);
                        if (!strcmp(grp, "o64") || !strcmp(grp, "o142")) {
                            nbeam++;
                            okb += light_beam_draw(t9, 1, (const float (*)[3])lp, 2);
                        } else {
                            nother++;
                            other += light_beam_draw(t9, 1, (const float (*)[3])lp, 2);
                        }
                    }
                }
                fclose(fo);
                CHECK(nbeam > 0 && okb == nbeam && other == 0, "beam draws on Coastline's fixture: %d of %d beam triangles, %d of %d others",
                      okb, nbeam, other, nother);
            }
        }
    }
    {   /* 8 the shader's Purkinje weight on the GPU against the C copy */
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        NSError *err = nil;
        NSString *src = [@"#include <metal_stdlib>\nusing namespace metal;\n" stringByAppendingString:[@BRR_NIGHT_SRC stringByAppendingString:
            @"kernel void pw(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) { o[i] = purkinje_w(pow(10.0, -5.0 + float(i) * 0.04)); }\n"]];
        id<MTLLibrary> lib = dev ? [dev newLibraryWithSource:src options:nil error:&err] : nil;
        if (!lib) {
            CHECK(0, "Purkinje on the GPU: %s", dev ? [[err description] UTF8String] : "no device");
        } else {
            id<MTLComputePipelineState> ps = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"pw"] error:&err];
            id<MTLBuffer> b = [dev newBufferWithLength:201 * 4 options:MTLResourceStorageModeShared];
            id<MTLCommandQueue> q = [dev newCommandQueue];
            id<MTLCommandBuffer> cb = [q commandBuffer];
            id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
            float worst = 0;
            [ce setComputePipelineState:ps];
            [ce setBuffer:b offset:0 atIndex:0];
            [ce dispatchThreads:MTLSizeMake(201, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
            [ce endEncoding];
            [cb commit];
            [cb waitUntilCompleted];
            for (i = 0; i < 201; i++)
                worst = fmaxf(worst, fabsf(((float *)[b contents])[i] - light_purkinje_w(powf(10.0f, -5.0f + i * 0.04f))));
            CHECK(worst < 1e-4f, "Purkinje weight: the shader's on the GPU against the C copy, 201 luminances 1e-5..1e3: worst %.2e", worst);
        }
    }
    {   /* 6 the eye's adaptation: a cold start, then the time constants */
        float ev = 1e9f, tgt = -2.3f, e1, e2;
        ev = light_adapt_ev(ev, -1.0f, 1.0f / 60, 0);
        CHECK(ev > 1e8f, "no samples leave an unset EV unset (it was set to the range's middle)");
        ev = light_adapt_ev(ev, tgt, 1.0f / 60, 1);
        CHECK(fabsf(ev - tgt) < 0.1f, "cold start: the first metered frame's EV %.3f, target %.3f (within 0.1 EV)", ev, tgt);
        e1 = light_adapt_ev(-2.3f, -1.0f, 1.0f / 60, 0);
        CHECK(e1 == -2.3f, "a frame with no samples holds the EV");
        e1 = -2.0f; e2 = -2.0f;
        for (i = 0; i < 60; i++) { e1 = light_adapt_ev(e1, -1.0f, 1.0f / 60, 1); e2 = light_adapt_ev(e2, -3.0f, 1.0f / 60, 1); }
        CHECK(fabsf((e1 + 2.0f) - (1.0f - expf(-1.0f))) < 0.01f && fabsf((-2.0f - e2) - (1.0f - expf(-1.0f / 3.0f))) < 0.01f,
              "after 1 s (60 frames) of a 1 EV step: brighter %.3f of it (1 - 1/e = 0.632, tau 1 s), darker %.3f (1 - e^-1/3 = 0.283, tau 3 s)",
              e1 + 2.0f, -2.0f - e2);
        e1 = -2.0f;
        for (i = 0; i < 30; i++) e1 = light_adapt_ev(e1, -1.0f, 1.0f / 30, 1);
        CHECK(fabsf((e1 + 2.0f) - (1.0f - expf(-1.0f))) < 0.01f, "the same at 30 frames a second: %.3f (frame rate free)", e1 + 2.0f);
        CHECK(light_adapt_ev(-2.0f, 2.0f, 1.0f / 60, 1) == 2.0f, "a change of 4 stops (a cut, the race starting) snaps");
    }
    {   /* 7 the moon's disc integrates to its illuminance; the stars stay starlight */
        float E = light_moon_normal(LIGHT_MOON_ELEV), R = LIGHT_MOON_R;
        double sum = 0;
        int n = 4000, s2;
        for (s2 = 0; s2 < n; s2++) {   /* rings: L(r) 2 pi theta dtheta (small angles) */
            double r = (s2 + 0.5) / n, th = r * R;
            sum += light_moon_disc(E, (float)r) * 2.0 * M_PI * th * (R / n);
        }
        CHECK(fabs(sum - E) / E < 0.05, "moon's disc: %.4f lux integrated, %.4f lux its illuminance (within 5%%); centre %.0f cd/m2, mean %.0f cd/m2",
              sum, E, light_moon_disc(E, 0), E / (M_PI * R * R));
        {
            double Eg = 0, Ef = 0, mean = 0;
            int f, a, b, ncell = 0;
            for (a = 0; a < 20000; a++)
                mean += light_star_E((a + 0.5f) / 20000.0f) / 20000.0;
            /* every cell of the cube's faces above the horizon: a star's expected illuminance on level ground
               (sin of its elevation), dimmed by its air mass */
            for (f = 0; f < 5; f++)   /* the top face and the four sides (by symmetry: one side, four times) */
                for (a = 0; a < LIGHT_STAR_N; a++)
                    for (b = 0; b < LIGHT_STAR_N; b++) {
                        float u = (a + 0.5f) / LIGHT_STAR_N * 2 - 1, v = (b + 0.5f) / LIGHT_STAR_N * 2 - 1, d[3], l, el;
                        if (f == 4) { d[0] = u; d[1] = v; d[2] = 1; } else { d[0] = 1; d[1] = u; d[2] = v; }
                        if (d[2] <= 0)
                            continue;
                        l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                        el = asinf(d[2] / l) * 57.29578f;
                        Ef += LIGHT_STAR_P * mean;
                        Eg += LIGHT_STAR_P * mean * powf(10.0f, -0.4f * LIGHT_MOON_KV * light_airmass(el)) * (d[2] / l);
                        ncell++;
                    }
            CHECK(Eg < 0.001 && Eg > 1e-5, "stars: %.0f above the horizon, %.2e lux on the ground (%.2e above the air; starlight is about 2e-4, < 0.001)",
                  ncell * LIGHT_STAR_P, Eg, Ef);
        }
    }
    {   /* 8 the Purkinje tint: Forza Horizon 6's night blue (NIGHT.md: its moonlit ground, road and sky at blue/red
           2.4 / 0.45 = 5.3 in display-linear, hue 213 to 217); applied after the tonemap, a moonlit road at the rods' share
           w carries (1 - w) + w b / y of each channel of its grey */
        float b[3] = { LIGHT_PURK_BLUE_R, LIGHT_PURK_BLUE_G, LIGHT_PURK_BLUE_B }, y = 0.2126f * b[0] + 0.7152f * b[1] + 0.0722f * b[2];
        float L = 0.2f * 0.25f / (float)M_PI, w = light_purkinje_w(L), r = (1 - w) + w * b[0] / y, bl = (1 - w) + w * b[2] / y;
        CHECK(bl / r > 3.0f && bl / r < 6.0f, "moonlit road (%.4f cd/m2, rods' share %.2f): blue/red %.2f (FH6's moonlit 3 to 6)", L, w, bl / r);
        CHECK(strstr(BRR_NIGHT_SRC, "0.45, 1.00, 2.40") != NULL, "the post shader's tint is light_night.h's");
    }
    printf(g_fail ? "night: FAIL\n" : "night: all pass\n");
    return g_fail;
}
