/* brr_metal_plan.h: the lamps' shadow tiles planned and drawn on the GPU, each
 * frame, the CPU's light/light_cluster.c light_shadow_plan its reference
 * (BR_LIGHT_PLANCHECK steps both on the same inputs and compares them):
 *
 *   plan_lights  one threadgroup: a thread a light its shadow transform
 *                (light_shadow_matrix), importance (candela x the screen it
 *                covers, cl_axes' this frame, over its distance squared) and
 *                tile size, and the hash of the moving casters near it (the
 *                cars' boxes); then one thread the plan itself, as the CPU's:
 *                the most important first, halved or dropped to fit the
 *                atlas, last frame's places kept, the rest packed in Morton
 *                order, drawn within the texel budget unless its cached
 *                depth stands.  The cache (a light identity's tile, the light
 *                and the casters it was drawn for) lives in st.  Out: the
 *                set's tiles (LSet s[], h.atl), the tile list (PL) and the
 *                pass's indirect draws (clears, moving casters, floors, over
 *                the tiles drawn)
 *   lc_*         the pass: one render over the whole atlas, each triangle
 *                placed in its tile by the vertex stage (the tile's light
 *                transform, then its rectangle) and clipped to it */
#ifndef BRR_METAL_PLAN_H
#define BRR_METAL_PLAN_H

#include "light_cluster.h"

#define PLAN_SC_STRIDE 256           /* bytes a light identity's cache entry, at most */
#define PLAN_GRID_MAX 64             /* atlas cells a side (light_cluster.c LS_GRID_MAX) */

/* plan_lights' constants (PU below: the same layout) */
typedef struct {
    float eye[4];
    int32_t n, atlas, noshadow, ndyn;
    uint32_t budget, nfloor, dyncap, pad1;   /* dyncap: cast_pick's room, vertices */
} PlanU;

/* the plan's tiles (PL below): its counts, then each tile, then the drawn
 * tiles' slots; then the indirect draws: clears, moving casters, floors
 * (MTLDrawPrimitivesIndirectArguments each) */
typedef struct {
    int32_t n, nrend, rendered, cached, stale, dropped, repacked, texels;
    int32_t t[LC_LIGHTS][8];         /* light, x, y, size, render */
    int32_t rend[LC_LIGHTS];
    uint32_t aclr[4], adyn[4], afloor[4], atrk[4];
} PlanOut;

#define BRR_PLAN_STR_(x) #x
#define BRR_PLAN_STR(x) BRR_PLAN_STR_(x)
#define BRR_PLAN_SRC \
"#define PL_LIGHTS " BRR_PLAN_STR(LC_LIGHTS) "\n#define PL_IDS " BRR_PLAN_STR(LC_IDS) "\n" \
"#define PL_TMIN " BRR_PLAN_STR(LS_TILE_MIN) "\n#define PL_TMAX " BRR_PLAN_STR(LS_TILE_MAX) "\n#define PL_NEAR " BRR_PLAN_STR(LS_NEARP) "\n" \
"#define PL_GMAX " BRR_PLAN_STR(PLAN_GRID_MAX) "\n" \
"struct PU { float4 eye; int n, atlas, noshadow, ndyn; uint budget, nfloor, dyncap, pad1; };\n" \
"struct PL { int n, nrend, rendered, cached, stale, dropped, repacked, texels; int t[PL_LIGHTS][8]; int rend[PL_LIGHTS];\n" \
"            uint aclr[4], adyn[4], afloor[4], atrk[4]; };\n" \
/* a light identity's cache (light_cluster.c s_sc) */ \
"struct SC { int has, x, y, size; LS key; ulong dyn; uint placed, pad; LSH rec; };\n" \
"struct PLH { uint fr, pad0, pad1, pad2; };\n" \
/* light_shadow_matrix */ \
"static bool pl_matrix(LS l, thread LSH &r) {\n" \
"  float n = PL_NEAR, fr = l.range;\n" \
"  if ((l.kind > 1.5 && l.kind < 2.5) || l.kind > 3.5) return false;\n" \
"  if (!(l.cosout >= 0.17364818) || !(fr > n * 2.0)) return false;\n" \
"  float3 ax = float3(l.axis); float len = sqrt(dot(ax, ax)); if (!(len > 0.0)) return false;\n" \
"  float3 f = ax / len, up = float3(0, 0, 1); if (abs(f.z) > 0.99) up = float3(1, 0, 0);\n" \
"  float3 rt = float3(f.y * up.z - f.z * up.y, f.z * up.x - f.x * up.z, f.x * up.y - f.y * up.x); rt /= sqrt(dot(rt, rt));\n" \
"  float3 u = float3(rt.y * f.z - rt.z * f.y, rt.z * f.x - rt.x * f.z, rt.x * f.y - rt.y * f.x);\n" \
"  float t = sqrt(1.0 - l.cosout * l.cosout) / l.cosout, A = fr / (fr - n), B = -n * fr / (fr - n); float3 p = float3(l.pos);\n" \
"  r.m = float4x4(float4(rt.x / t, u.x / t, A * f.x, f.x), float4(rt.y / t, u.y / t, A * f.y, f.y), float4(rt.z / t, u.z / t, A * f.z, f.z),\n" \
"                 float4(-dot(rt, p) / t, -dot(u, p) / t, -A * dot(f, p) + B, -dot(f, p)));\n" \
"  r.rect = float4(0); r.prm = float4(A, B, t, n); return true; }\n" \
/* the moving casters near the light: FNV-1a over the bytes of each triangle whose box meets the light's (dyn_hash) */ \
"static ulong pl_dyn(LS l, const device float *dyn, int nd) {\n" \
"  ulong h = 1469598103934665603ul; float3 p = float3(l.pos);\n" \
"  for (int i = 0; i + 2 < nd; i += 3) { const device float *v = dyn + 3 * i; bool out = false;\n" \
"    for (int k = 0; k < 3 && !out; k++) { float lo = min(v[k], min(v[3 + k], v[6 + k])), hi = max(v[k], max(v[3 + k], v[6 + k]));\n" \
"      out = lo > p[k] + l.range || hi < p[k] - l.range; }\n" \
"    if (out) continue;\n" \
"    for (int k = 0; k < 9; k++) { uint w = as_type<uint>(v[k]);\n" \
"      for (int b = 0; b < 4; b++) h = (h ^ ulong((w >> (8 * b)) & 255u)) * 1099511628211ul; } }\n" \
"  return h; }\n" \
"static bool pl_same(LS a, LS b) {\n" \
"  return all(float3(a.pos) == float3(b.pos)) && a.kind == b.kind && all(float3(a.axis) == float3(b.axis)) && a.cd == b.cd\n" \
"      && all(float3(a.side) == float3(b.side)) && a.range == b.range && all(float3(a.rgb) == float3(b.rgb)) && a.cosout == b.cosout; }\n" \
/* the occupancy grid: a row's cells as bits */ \
"static ulong pl_mask(int x, int s) { return (s >= 64 ? ~0ul : ((1ul << ulong(s)) - 1ul)) << ulong(x); }\n" \
"static bool pl_free(thread ulong *occ, int x, int y, int s) { ulong m = pl_mask(x, s); for (int j = y; j < y + s; j++) if (occ[j] & m) return false; return true; }\n" \
"static void pl_set(thread ulong *occ, int x, int y, int s) { ulong m = pl_mask(x, s); for (int j = y; j < y + s; j++) occ[j] |= m; }\n" \
"static void pl_demorton(uint i, thread int &x, thread int &y) { uint a = 0u, b = 0u;\n" \
"  for (uint k = 0u; k < 16u; k++) { a |= ((i >> (2u * k)) & 1u) << k; b |= ((i >> (2u * k + 1u)) & 1u) << k; } x = int(a); y = int(b); }\n" \
"static bool pl_place(thread ulong *occ, int g, int s, thread int &x, thread int &y) {\n" \
"  uint nb = uint(g / s) * uint(g / s);\n" \
"  for (uint i = 0u; i < nb; i++) { int bx, by; pl_demorton(i, bx, by);\n" \
"    if (pl_free(occ, bx * s, by * s, s)) { x = bx * s; y = by * s; pl_set(occ, x, y, s); return true; } }\n" \
"  return false; }\n" \
"kernel void plan_lights(uint t [[thread_index_in_threadgroup]], device LSet &ls [[buffer(0)]], constant PU &u [[buffer(1)]],\n" \
"                        const device float *cov [[buffer(2)]], const device int *ids [[buffer(3)]], const device float *dyn [[buffer(4)]],\n" \
"                        device PLH &sh [[buffer(5)]], device uchar *scb [[buffer(6)]], device PL &p [[buffer(7)]],\n" \
"                        const device uint *castargs [[buffer(8)]]) {\n" \
"  threadgroup float imp[PL_LIGHTS]; threadgroup int size[PL_LIGHTS], ok[PL_LIGHTS], cid[PL_LIGHTS]; threadgroup ulong dh[PL_LIGHTS];\n" \
"  threadgroup LSH rec[PL_LIGHTS];\n" \
"  int n = min(u.n, PL_LIGHTS), atlas = min(u.atlas, PL_GMAX * PL_TMIN); atlas -= atlas % PL_TMIN; int g = atlas / PL_TMIN;\n" \
"  uint fr = sh.fr + 1u;\n" \
"  for (int i = int(t); i < n; i += 128) {   /* a thread a light */\n" \
"    LSH z; z.m = float4x4(0.0); z.rect = float4(0); z.prm = float4(0); ls.s[i] = z;\n" \
"    imp[i] = 0.0; ok[i] = 0; cid[i] = ids[i] & (PL_IDS - 1);\n" \
"    LSH r; if (!(cov[i] > 0.0) || g <= 0 || !pl_matrix(ls.l[i], r)) continue;\n" \
"    device SC &c = *(device SC *)(scb + uint(cid[i]) * " BRR_PLAN_STR(PLAN_SC_STRIDE) "u);\n" \
"    float3 d = float3(ls.l[i].pos) - u.eye.xyz; float im = ls.l[i].cd * cov[i] / max(dot(d, d), 1.0);\n" \
"    if (c.placed + 1u == fr) im *= 1.25;\n" \
"    imp[i] = im; rec[i] = r; ok[i] = 1;\n" \
"    int s = cov[i] > 0.25 ? 8 : cov[i] > 0.0625 ? 4 : cov[i] > 0.015 ? 2 : 1;\n" \
"    while (s * PL_TMIN > PL_TMAX) s /= 2;\n" \
"    while (s > g) s /= 2;\n" \
"    size[i] = s; dh[i] = pl_dyn(ls.l[i], dyn, u.ndyn); }\n" \
"  threadgroup_barrier(mem_flags::mem_threadgroup | mem_flags::mem_device);\n" \
"  if (t != 0u) return;\n" \
/* the plan, one thread (light_shadow_plan) */ \
"  sh.fr = fr; p.n = 0; p.nrend = 0; p.rendered = p.cached = p.stale = p.dropped = p.repacked = 0; p.texels = 0;\n" \
"  int cand[PL_LIGHTS], nc = 0;\n" \
"  for (int i = 0; i < n; i++) if (ok[i]) {   /* by importance, then index */\n" \
"    int k = nc++; while (k > 0 && (imp[cand[k - 1]] < imp[i])) { cand[k] = cand[k - 1]; k--; } cand[k] = i; }\n" \
"  long area = 0, cap = long(g) * long(g);\n" \
"  for (int k = 0; k < nc; k++) area += long(size[cand[k]]) * long(size[cand[k]]);\n" \
"  while (area > cap) { int k = nc - 1; while (k >= 0 && size[cand[k]] == 1) k--;\n" \
"    if (k >= 0) { area -= long(size[cand[k]]) * long(size[cand[k]]) * 3 / 4; size[cand[k]] /= 2; } else { area--; nc--; p.dropped++; } }\n" \
"  ulong occ[PL_GMAX]; for (int j = 0; j < PL_GMAX; j++) occ[j] = 0ul;\n" \
"  int px[PL_LIGHTS], py[PL_LIGHTS], placed[PL_LIGHTS]; for (int i = 0; i < n; i++) placed[i] = 0;\n" \
"  for (int k = 0; k < nc; k++) { int i = cand[k]; device SC &c = *(device SC *)(scb + uint(cid[i]) * " BRR_PLAN_STR(PLAN_SC_STRIDE) "u);\n" \
"    if (c.has && c.placed + 1u == fr && c.size == size[i] * PL_TMIN && c.x / PL_TMIN + size[i] <= g && c.y / PL_TMIN + size[i] <= g\n" \
"        && pl_free(occ, c.x / PL_TMIN, c.y / PL_TMIN, size[i])) {\n" \
"      px[i] = c.x / PL_TMIN; py[i] = c.y / PL_TMIN; pl_set(occ, px[i], py[i], size[i]); placed[i] = 1; } }\n" \
"  int ord[PL_LIGHTS];   /* by size, largest first, then importance */\n" \
"  for (int k = 0; k < nc; k++) { int i = cand[k], m = k;\n" \
"    while (m > 0 && size[ord[m - 1]] < size[i]) { ord[m] = ord[m - 1]; m--; } ord[m] = i; }\n" \
"  bool fail = false;\n" \
"  for (int k = 0; k < nc && !fail; k++) if (!placed[ord[k]]) { int x, y; fail = !pl_place(occ, g, size[ord[k]], x, y); px[ord[k]] = x; py[ord[k]] = y; placed[ord[k]] = !fail; }\n" \
"  if (fail) { for (int j = 0; j < PL_GMAX; j++) occ[j] = 0ul;\n" \
"    for (int k = 0; k < nc; k++) { int x, y; placed[ord[k]] = pl_place(occ, g, size[ord[k]], x, y); px[ord[k]] = x; py[ord[k]] = y; }\n" \
"    p.repacked = 1; }\n" \
"  long texels = 0;\n" \
"  for (int k = 0; k < nc; k++) { int i = cand[k], sz = size[i] * PL_TMIN, x = px[i] * PL_TMIN, y = py[i] * PL_TMIN, render = 0;\n" \
"    device SC &c = *(device SC *)(scb + uint(cid[i]) * " BRR_PLAN_STR(PLAN_SC_STRIDE) "u);\n" \
"    if (!placed[i] || u.noshadow) continue;\n" \
"    ulong h = dh[i]; bool same = c.has && c.placed + 1u == fr && c.x == x && c.y == y && c.size == sz;\n" \
"    if (same && pl_same(c.key, ls.l[i]) && c.dyn == h) p.cached++;\n" \
"    else if (texels + long(sz) * long(sz) <= long(u.budget)) { render = 1; texels += long(sz) * long(sz); p.rendered++;\n" \
"      c.has = 1; c.key = ls.l[i]; c.dyn = h; c.rec = rec[i]; }\n" \
"    else if (same && c.dyn == h) p.stale++;\n" \
"    else { c.has = 0; c.placed = fr; c.x = x; c.y = y; c.size = sz; p.dropped++; continue; }\n" \
"    c.placed = fr; c.x = x; c.y = y; c.size = sz;\n" \
"    LSH o = c.rec; o.rect = float4(float(x) / float(atlas), float(y) / float(atlas), float(sz) / float(atlas), 1.0); ls.s[i] = o;\n" \
"    int q = p.n++; p.t[q][0] = i; p.t[q][1] = x; p.t[q][2] = y; p.t[q][3] = sz; p.t[q][4] = render;\n" \
"    if (render) p.rend[p.nrend++] = q; }\n" \
"  p.texels = int(texels);\n" \
"  for (int k = 0; k < PL_IDS; k++) { device SC &c = *(device SC *)(scb + uint(k) * " BRR_PLAN_STR(PLAN_SC_STRIDE) "u); if (c.placed != fr) c.has = 0; }\n" \
"  ls.h.atl = float4(float(atlas), 1.0 / float(max(atlas, 1)), float(p.n), 0.0);\n" \
"  uint nr = uint(p.nrend);\n" \
"  p.aclr[0] = 6u; p.aclr[1] = nr; p.aclr[2] = 0u; p.aclr[3] = 0u;\n" \
"  uint nd = min(castargs[0], u.dyncap) / 3u * 3u;   /* cast_pick counts past its room; it writes none there */\n" \
"  p.adyn[0] = nd; p.adyn[1] = nd ? nr : 0u; p.adyn[2] = 0u; p.adyn[3] = 0u;\n" \
"  p.afloor[0] = u.nfloor; p.afloor[1] = u.nfloor ? nr : 0u; p.afloor[2] = 0u; p.afloor[3] = 0u;\n" \
"  p.atrk[0] = 0u; p.atrk[1] = 1u; p.atrk[2] = 0u; p.atrk[3] = 0u; }\n" \
/* the track's casters of each drawn tile (light_cluster.c light_shadow_track_casters, \
   as brr_metal_cast.h cast_tiles): one threadgroup a plan slot; the track grid's cells \
   under the lamp that meet its frustum, their triangles appended to ent (its slot << 24 \
   | the triangle), a run a tile; plan_tracks_args then sets the pass's draw */ \
"kernel void plan_tracks(uint k [[threadgroup_position_in_grid]], uint i [[thread_index_in_threadgroup]], uint nt [[threads_per_threadgroup]],\n" \
"                        const device LSet &ls [[buffer(0)]], constant CTG &g [[buffer(1)]], const device PL &p [[buffer(2)]],\n" \
"                        const device int *start [[buffer(3)]], const device int *list [[buffer(4)]], const device float *box [[buffer(5)]],\n" \
"                        device atomic_uint *used [[buffer(6)]], device uint *ent [[buffer(8)]]) {\n" \
"  threadgroup atomic_uint cnt; threadgroup uint base, ok;\n" \
"  bool on = int(k) < p.n && p.t[k][4] != 0; uint li = on ? uint(p.t[k][0]) : 0u; float4 pl[6]; int x0 = 0, y0 = 0, w = 0, nc = 0;\n" \
"  if (i == 0u) atomic_store_explicit(&cnt, 0u, memory_order_relaxed);\n" \
"  if (on) {\n" \
"    float4x4 m = ls.s[li].m; float4 rx = float4(m[0].x, m[1].x, m[2].x, m[3].x), ry = float4(m[0].y, m[1].y, m[2].y, m[3].y);\n" \
"    float4 rz = float4(m[0].z, m[1].z, m[2].z, m[3].z), rw = float4(m[0].w, m[1].w, m[2].w, m[3].w);\n" \
"    pl[0] = rw - rx; pl[1] = rw + rx; pl[2] = rw - ry; pl[3] = rw + ry; pl[4] = rz; pl[5] = rw - rz;\n" \
"    for (int j = 0; j < 6; j++) { float l = length(pl[j].xyz); if (l > 0.0) pl[j] /= l; }\n" \
"    float3 lp = float3(ls.l[li].pos); float r = ls.l[li].range;\n" \
"    x0 = max(int(floor((lp.x - r - g.lo.x) / CAST_TCELL)) - 1, 0); int x1 = min(int(floor((lp.x + r - g.lo.x) / CAST_TCELL)) + 1, g.nx - 1);\n" \
"    y0 = max(int(floor((lp.y - r - g.lo.y) / CAST_TCELL)) - 1, 0); int y1 = min(int(floor((lp.y + r - g.lo.y) / CAST_TCELL)) + 1, g.ny - 1);\n" \
"    w = x1 - x0 + 1; nc = x1 >= x0 && y1 >= y0 ? w * (y1 - y0 + 1) : 0; }\n" \
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"  for (int q = int(i); q < nc; q += int(nt)) { int c = (y0 + q / w) * g.nx + x0 + q % w;\n" \
"    if (start[c] != start[c + 1] && !cast_out(pl, box + 6 * c)) atomic_fetch_add_explicit(&cnt, uint(start[c + 1] - start[c]), memory_order_relaxed); }\n" \
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"  if (i == 0u) { uint n1 = atomic_load_explicit(&cnt, memory_order_relaxed), b = n1 ? atomic_fetch_add_explicit(used, n1, memory_order_relaxed) : 0u;\n" \
"    base = b; ok = n1 != 0u && b + n1 <= g.cap; atomic_store_explicit(&cnt, 0u, memory_order_relaxed); }\n" \
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"  if (ok == 0u) return;\n" \
"  for (int q = int(i); q < nc; q += int(nt)) { int c = (y0 + q / w) * g.nx + x0 + q % w;\n" \
"    if (start[c] == start[c + 1] || cast_out(pl, box + 6 * c)) continue;\n" \
"    uint o = base + atomic_fetch_add_explicit(&cnt, uint(start[c + 1] - start[c]), memory_order_relaxed);\n" \
"    for (int e = start[c]; e < start[c + 1]; e++, o++) ent[o] = k << 24 | uint(list[e]); } }\n" \
"kernel void plan_tracks_args(device PL &p [[buffer(2)]], const device uint *used [[buffer(6)]], constant CTG &g [[buffer(1)]]) {\n" \
"  p.atrk[0] = 3u * min(used[0], g.cap); }\n" \
/* the pass: a tile's place in the atlas (pixels x, y, size of S), a clip-space position c in the tile's \
   own clip space (its light's transform) to the atlas's, the tile's four edges as clip distances */ \
"struct LCT { float4 pos [[position]]; float clip [[clip_distance]] [4]; };\n" \
"static LCT lc_place(float4 c, int4 tl, float S) {\n" \
"  LCT o; float k = float(tl.w) / S;\n" \
"  o.pos = float4(c.x * k + c.w * ((2.0 * float(tl.y) + float(tl.w)) / S - 1.0), c.y * k + c.w * (1.0 - (2.0 * float(tl.z) + float(tl.w)) / S), c.z, c.w);\n" \
"  o.clip[0] = c.w - c.x; o.clip[1] = c.w + c.x; o.clip[2] = c.w - c.y; o.clip[3] = c.w + c.y; return o; }\n" \
/* a drawn tile cleared to the far plane: instance r of the drawn tiles */ \
"vertex LCT lc_clr(uint v [[vertex_id]], uint r [[instance_id]], const device PL &p [[buffer(0)]], constant float &S [[buffer(1)]]) {\n" \
"  int q = p.rend[r]; float2 e = float2((v == 1u || v == 2u || v == 4u) ? 1.0 : -1.0, (v == 2u || v == 4u || v == 5u) ? 1.0 : -1.0);\n" \
"  return lc_place(float4(e, 1.0, 1.0), int4(p.t[q][0], p.t[q][1], p.t[q][2], p.t[q][3]), S); }\n" \
/* the moving casters and the floors: instance r of the drawn tiles */ \
"vertex LCT lc_inst(uint v [[vertex_id]], uint r [[instance_id]], const device packed_float3 *pts [[buffer(0)]], const device PL &p [[buffer(1)]],\n" \
"                   const device LSet &ls [[buffer(2)]], constant float &S [[buffer(3)]]) {\n" \
"  int q = p.rend[r]; int4 tl = int4(p.t[q][0], p.t[q][1], p.t[q][2], p.t[q][3]);\n" \
"  return lc_place(ls.s[tl.x].m * float4(float3(pts[v]), 1.0), tl, S); }\n" \
/* the track's casters: cast_tiles' list, an entry a triangle (its tile's slot << 24 | the track's triangle) */ \
"vertex LCT lc_trk(uint v [[vertex_id]], const device uint *ent [[buffer(0)]], const device packed_float3 *trk [[buffer(1)]],\n" \
"                  const device PL &p [[buffer(2)]], const device LSet &ls [[buffer(3)]], constant float &S [[buffer(4)]]) {\n" \
"  uint e = ent[v / 3u]; int q = int(e >> 24); int4 tl = int4(p.t[q][0], p.t[q][1], p.t[q][2], p.t[q][3]);\n" \
"  return lc_place(ls.s[tl.x].m * float4(float3(trk[3u * (e & 0xFFFFFFu) + v % 3u]), 1.0), tl, S); }\n"

#endif
