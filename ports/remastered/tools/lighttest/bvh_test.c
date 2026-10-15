/* bvh_test.c: the track's BVH (platform/light/light_bvh.c) against brute
 * force, on the CPU.  For each scene (the track's OBJ dump, and the
 * synthetic ones in lt_scene.h) and each ray set: every ray's closest hit by
 * the BVH and by testing every triangle in order with the same triangle
 * test, then any-hit against the brute force's answer for a bounded
 * segment.  The closest hit is the same when its triangle and its distance
 * (bit for bit) are; a tie is a different triangle at the same distance
 * (shared edges, duplicates, coplanar faces), which the two find in a
 * different order.  Anything else fails.
 *
 * A second, independent check of the triangle test itself: Moller-Trumbore in
 * double precision, its distance compared within 1e-4 relative.
 *
 * Usage: bvh_test [track.obj] [rays per set]   (run.sh builds and runs it) */
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#include "light_bvh.h"
#include "lt_scene.h"

static double now(void)
{
    static mach_timebase_info_data_t tb;
    if (!tb.denom)
        mach_timebase_info(&tb);
    return (double)mach_absolute_time() * tb.numer / tb.denom * 1e-9;
}

typedef struct {
    float t;
    int tri;
} Res;

static void brute(const LtScene *s, const LtRay *r, Res *out)
{
    float best = r->tmax, t, u, v;
    int i, hit = -1;
    for (i = 0; i < s->ntri; i++)
        if (light_tri_hit(r->o, r->d, s->tri + 9 * i, r->tmin, best, &t, &u, &v)) {
            best = t;
            hit = i;
        }
    out->t = hit >= 0 ? best : INFINITY;
    out->tri = hit;
}

/* Moller-Trumbore, double, two-sided: the closest distance or INFINITY */
static double mt_closest(const LtScene *s, const LtRay *r)
{
    double best = INFINITY;
    int i, k;
    for (i = 0; i < s->ntri; i++) {
        const float *v = s->tri + 9 * i;
        double e1[3], e2[3], p[3], q[3], tv[3], det, inv, u, w, t;
        for (k = 0; k < 3; k++) {
            e1[k] = (double)v[3 + k] - v[k];
            e2[k] = (double)v[6 + k] - v[k];
            tv[k] = (double)r->o[k] - v[k];
        }
        p[0] = r->d[1] * e2[2] - r->d[2] * e2[1];
        p[1] = r->d[2] * e2[0] - r->d[0] * e2[2];
        p[2] = r->d[0] * e2[1] - r->d[1] * e2[0];
        det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
        if (det == 0.0)
            continue;
        inv = 1.0 / det;
        u = (tv[0] * p[0] + tv[1] * p[1] + tv[2] * p[2]) * inv;
        if (u < 0.0 || u > 1.0)
            continue;
        q[0] = tv[1] * e1[2] - tv[2] * e1[1];
        q[1] = tv[2] * e1[0] - tv[0] * e1[2];
        q[2] = tv[0] * e1[1] - tv[1] * e1[0];
        w = (r->d[0] * q[0] + r->d[1] * q[1] + r->d[2] * q[2]) * inv;
        if (w < 0.0 || u + w > 1.0)
            continue;
        t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
        if (t > r->tmin && t < best)
            best = t;
    }
    return best;
}

static int ulps(float a, float b)
{
    int32_t x, y;
    memcpy(&x, &a, 4);
    memcpy(&y, &b, 4);
    return x > y ? x - y : y - x;
}

/* a tie: the BVH's triangle and the brute force's are both hit (each by the
 * same test, at the distance reported), and their hit points are the same
 * point to the precision the scene's coordinates have, a unit in the last
 * place of the point's largest coordinate (coincident or shared faces,
 * duplicates; the order of the search decides which is reported) */
static int tie(const LtScene *s, const LtRay *r, const LightHit *bh, const Res *bf, double *dmax)
{
    float t, u, v, p, m = 0.0f;
    double dist;
    int k;
    if (!light_tri_hit(r->o, r->d, s->tri + 9 * bh->tri, r->tmin, INFINITY, &t, &u, &v) || t != bh->t)
        return 0;
    if (!light_tri_hit(r->o, r->d, s->tri + 9 * bf->tri, r->tmin, INFINITY, &t, &u, &v) || t != bf->t)
        return 0;
    for (k = 0; k < 3; k++) {
        p = fabsf(r->o[k] + bf->t * r->d[k]);
        if (p > m)
            m = p;
    }
    dist = ((double)bh->t - bf->t) * sqrt((double)r->d[0] * r->d[0] + (double)r->d[1] * r->d[1] + (double)r->d[2] * r->d[2]);
    if (dist > (double)(nextafterf(m, INFINITY) - m))
        return 0;
    if (dist > *dmax)
        *dmax = dist;
    return 1;
}

static void run_scene(const LtScene *s, int nray)
{
    LightBvh b;
    double t0, tb = 1e9;
    int rep, set;

    if (getenv("LT_ONLY") && strcmp(getenv("LT_ONLY"), s->name))
        return;     /* one scene (LT_ONLY=name) */
    for (rep = 0; rep < 5; rep++) {
        t0 = now();
        if (!light_bvh_build(&b, s->tri, s->obj, s->ntri)) {
            printf("%s: build failed\n", s->name);
            return;
        }
        t0 = now() - t0;
        if (t0 < tb)
            tb = t0;
        if (rep < 4)
            light_bvh_free(&b);
    }
    printf("\n%s: %d triangles, extent %.1f..%.1f %.1f..%.1f %.1f..%.1f\n", s->name, s->ntri,
           s->lo[0], s->hi[0], s->lo[1], s->hi[1], s->lo[2], s->hi[2]);
    printf("  build %.2f ms (best of 5), %d nodes (%d leaves, %.2f triangles a leaf), depth %d, SAH %.2f, %.0f KB\n",
           tb * 1e3, b.nnode, b.nleaf, (double)b.ntri / b.nleaf, b.depth, b.sah,
           (b.nnode * sizeof *b.node + b.ntri * (9 * sizeof(float) + sizeof(uint32_t) + sizeof(LightBvhPrim))) / 1024.0);
    {   /* the tree's own invariants: every triangle once, inside its leaf's
         * box, each child inside its parent */
        int *seen = calloc((size_t)b.ntri, sizeof *seen), bad = 0, i, j, k;
        for (i = 0; i < b.nnode; i++) {
            const LightBvhNode *nd = &b.node[i];
            if (nd->n) {
                for (j = nd->a; j < nd->a + nd->n; j++) {
                    seen[b.id[j]]++;
                    for (k = 0; k < 9; k++)
                        if (b.tri[9 * j + k] < nd->lo[k % 3] || b.tri[9 * j + k] > nd->hi[k % 3])
                            bad++;
                    if (memcmp(b.tri + 9 * j, s->tri + 9 * b.id[j], 36))
                        bad++;
                }
            } else {
                const LightBvhNode *c[2] = { &b.node[i + 1], &b.node[nd->a] };
                for (j = 0; j < 2; j++)
                    for (k = 0; k < 3; k++)
                        if (c[j]->lo[k] < nd->lo[k] || c[j]->hi[k] > nd->hi[k])
                            bad++;
            }
        }
        for (i = 0; i < b.ntri; i++)
            if (seen[i] != 1 || b.prim[i].obj != s->obj[i])
                bad++;
        printf("  structure: %s\n", bad ? "BROKEN" : "every triangle in one leaf, inside its box; children inside parents");
        free(seen);
        if (bad)
            exit(1);
    }

    for (set = 0; set < 2; set++) {
        LtRay *r = lt_rays(s, set, nray, 0xC0FFEEull + (uint64_t)set * 7919u);
        Res *bf = malloc((size_t)nray * sizeof *bf);
        LightHit *bh = malloc((size_t)nray * sizeof *bh);
        uint8_t *ba = malloc((size_t)nray), *fa = malloc((size_t)nray);
        double *mt = malloc((size_t)nray * sizeof *mt);
        LightBvhStats st = { 0, 0 }, sa = { 0, 0 };
        long same = 0, ties = 0, near = 0, bad = 0, miss = 0, anybad = 0, anyhit = 0, mtbad = 0;
        double tbf, tbv, tan, dmax = 0.0;
        int i;

        tbf = now();
        dispatch_apply((size_t)(nray + 999) / 1000, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(size_t c) {
            int j;
            for (j = (int)c * 1000; j < (int)c * 1000 + 1000 && j < nray; j++) {
                brute(s, &r[j], &bf[j]);
                mt[j] = mt_closest(s, &r[j]);
            }
        });
        tbf = now() - tbf;
        tbv = now();
        for (i = 0; i < nray; i++)       /* one thread: the per-ray cost */
            light_bvh_closest(&b, r[i].o, r[i].d, r[i].tmin, r[i].tmax, &bh[i], &st);
        tbv = now() - tbv;
        tan = now();
        for (i = 0; i < nray; i++) {
            float tm = lt_shadow_tmax(s, i, bf[i].t);
            fa[i] = (uint8_t)light_bvh_any(&b, r[i].o, r[i].d, r[i].tmin, tm, &sa);
        }
        tan = now() - tan;
        for (i = 0; i < nray; i++) {
            float tm = lt_shadow_tmax(s, i, bf[i].t);
            ba[i] = bf[i].tri >= 0 && bf[i].t < tm;
            anyhit += ba[i];
            if (fa[i] != ba[i])
                anybad++;
            if (bf[i].tri < 0)
                miss++;
            if (bh[i].tri == bf[i].tri && (bf[i].tri < 0 || bh[i].t == bf[i].t)) {
                same++;
            } else if (bh[i].tri >= 0 && bf[i].tri >= 0 && bh[i].t >= bf[i].t && tie(s, &r[i], &bh[i], &bf[i], &dmax)) {
                if (bh[i].t == bf[i].t)
                    ties++;
                else
                    near++;
            } else {
                if (bad < 50)
                    printf("  MISMATCH ray %d: bvh tri %d t %.9g, brute tri %d t %.9g (%d ulp)\n", i, bh[i].tri, bh[i].t, bf[i].tri, bf[i].t, ulps(bh[i].t, bf[i].t));
                bad++;
            }
            {   /* the triangle test against double Moller-Trumbore */
                double a = bf[i].tri >= 0 ? bf[i].t : INFINITY, m = mt[i];
                if (!(a == m || fabs(a - m) <= 1e-4 * fmax(1.0, fabs(m))))
                    mtbad++;
            }
        }
        printf("  %s rays: %d, %ld hit, %ld miss\n", set ? "surface (GI)" : "random", nray, nray - miss, miss);
        printf("    closest: %ld identical, %ld ties (%ld at the same distance, %ld within a unit of the hit's coordinates: %.2g m at most), %ld mismatches\n",
               same, ties + near, ties, near, dmax, bad);
        printf("    any-hit: %ld of %d agree (%ld occluded), %ld disagree\n", nray - anybad, nray, anyhit, anybad);
        printf("    per ray: %.2f nodes, %.2f triangles (closest); %.2f nodes, %.2f triangles (any)\n",
               (double)st.nodes / nray, (double)st.tris / nray, (double)sa.nodes / nray, (double)sa.tris / nray);
        printf("    one thread: %.2f Mrays/s closest, %.2f Mrays/s any; brute force %.1f s on every core\n",
               nray / tbv * 1e-6, nray / tan * 1e-6, tbf);
        printf("    double Moller-Trumbore: %ld of %d distances differ by more than 1e-4\n", mtbad, nray);
        if (bad || anybad)
            exit(1);
        free(r); free(bf); free(bh); free(ba); free(fa); free(mt);
    }
    light_bvh_free(&b);
}

int main(int argc, char **argv)
{
    int nray = argc > 2 ? atoi(argv[2]) : 1000000;
    LtScene s;
    if (argc > 1 && argv[1][0]) {
        if (!lt_load_obj(&s, argv[1])) {
            fprintf(stderr, "bvh_test: cannot read %s\n", argv[1]);
            return 1;
        }
        run_scene(&s, nray);
    }
    lt_soup(&s);
    run_scene(&s, nray);
    lt_terrain(&s);
    run_scene(&s, nray);
    lt_rooms(&s);
    run_scene(&s, nray);
    printf("\nall passed\n");
    return 0;
}
