/* brr_vsm.h: the sun's virtual shadow map (DESIGN.md 7.4; Epic's Virtual
 * Shadow Maps, Unreal Engine 5).  The sun's depth at a texel size matched to
 * the screen everywhere on the track, out to its far end: the cascades it
 * replaces held 22 cm texels from 110 to 300 metres and nothing past them,
 * and a caster nearer its receiver than their bias (kerbs, walls, a hill's
 * grazing shadow) showed only once the car came close (BR_LIGHT_SUNCHECK
 * measured it against traced rays).
 *
 * The map: VSM_LEVELS levels in the sun's own axes (r, v across, f away from
 * the sun), level L's texel VSM_T0 2^L metres, each VSM_TAB x VSM_TAB pages
 * of VSM_PAGE texels about the camera (a level's window: its first page org,
 * in pages from the axes' origin, moved a page at a time, a page's table slot
 * its page number modulo VSM_TAB, so a moving window keeps the pages it still
 * holds).  A point's level: the finest whose texel is no more than
 * 2^VSM_LOD of its pixel's footprint and whose window holds it with
 * VSM_MARGIN texels to spare, dithered over VSM_BAND of a level at each change
 * (the temporal pass blends the two).  The sun is fixed for a track, so a
 * page drawn stays right until it leaves its window or goes unneeded for
 * VSM_AGE frames.
 *
 * Each frame, on the GPU (brr_metal_vsm.m):
 *   vs_mark      a thread a G-buffer pixel facing the sun: the pages its
 *                lookups can read, at both levels of its dither, marked
 *                needed this frame
 *   vs_release   a thread a table entry: a page left its window, or unneeded
 *                for VSM_AGE frames, back to the free stack
 *   vs_alloc     one threadgroup, the coarsest level first: each needed
 *                entry given a slot of the pool, and those not yet drawn
 *                listed to be drawn, at most budget a frame (a page not drawn
 *                yet is read from the next level up, then the track map)
 *   vs_bin       a threadgroup a page to draw: the track's triangles whose
 *                box in the sun's axes meets the page (a light-space grid of
 *                VSM_CELL metres, each triangle taken from the one cell
 *                holding the corner where its box and the page's first meet)
 *   vs_trk       the pages drawn into a scratch atlas of depth, each
 *                triangle placed in its page's slot by the vertex stage and
 *                clipped there (a whole-atlas render pass stores its whole
 *                target, so the pool is never one)
 *   vs_copy      the scratch's slots into the pool's (R32Float, a slot the
 *                page and a texel of its neighbours each side: VSM_SLOT)
 *   vs_dynmark   a thread a moving caster's triangle and level: the needed
 *                pages under its box in the sun's axes given a slot of the
 *                dynamic pool
 *   vs_dyn       the moving casters drawn into those slots (depth, cleared
 *                each frame); a lookup takes the nearer of the two
 * and once a track the track map: every triangle in one VSM_TMAP square over
 * the track, what a lookup falls back to (and the fog's sun).
 *
 * vsmvis: the sun's visibility at a G-buffer point: brr_metal_lit.h casvis's
 * soft shadow (PCSS) and bias, in its level's texels, each sample's texels
 * found through the page table, compared by hand (bilinear, as
 * sample_compare): the pool is not a depth texture. */
#ifndef BRR_VSM_H
#define BRR_VSM_H

#include <stdint.h>

#define VSM_PAGE 128
#define VSM_SLOT 130             /* a page's slot: the page and a texel of its neighbours each side */
#define VSM_TAB 64               /* pages a level's side */
#define VSM_LEVELS 8
#define VSM_ENTRIES (VSM_LEVELS * VSM_TAB * VSM_TAB)
#define VSM_T0 0.01f             /* metres: level 0's texel; level 7's is 1.28 m and reaches 5.2 km */
#define VSM_LOD (-1.0f)          /* log2 of a texel over its pixel's footprint */
#define VSM_BAND 0.5f            /* of a level: the dither at each change of level */
#define VSM_MARGIN 20.0f         /* texels: a lookup's reach (the blocker search, the normal offset, the bilinear texel) */
#define VSM_AGE 120u             /* frames a page may go unneeded and stay */
#define VSM_PX 63                /* the pool's slots across (8190 texels) */
#define VSM_PY 63                /* and down (31 on iOS: 1953 pages, 128 MB) */
#define VSM_PY_SMALL 31
#define VSM_SCRX 15              /* the scratch's slots across: pages drawn a frame, at most VSM_SCRX x VSM_SCRY */
#define VSM_SCRY 8
#define VSM_DYNX 12              /* the dynamic pool's slots a side (a car near the camera takes about 4) */
#define VSM_TMAP 2048            /* texels: the track map's side */
#define VSM_CELL 8.0f            /* metres: the light-space grid's cells */
#define VSM_BINCAP (1u << 20)    /* (page, triangle) entries a frame */
#define VSM_NONE 0xFFFu          /* an entry's empty slot (12 bits: the pool's 3969 slots) */
#define VSM_VALID 0x1000u        /* an entry's page is drawn */

/* the map's constants (VSU below: the same layout) */
typedef struct {
    float r[4], v[4], f[4];      /* the sun's axes: across, across, away from the sun */
    float eye[4];                /* the camera; w: radians a lit pixel */
    float z[4];                  /* depth: the near end along f, 1 / its range, the range, VSM_T0 */
    float tm[4];                 /* the track map: its first corner (r, v), side, texel (metres) */
    float lod[4];                /* VSM_LOD, VSM_BAND, the scratch's width and height in texels */
    float dsz[4];                /* the dynamic pool's width and height, the pool's */
    int32_t org[VSM_LEVELS * 2]; /* each level's window: its first page across r, v */
    uint32_t cnt[4];             /* the frame, pages drawn a frame at most, dynamic slots, 0 */
    uint32_t pool[4];            /* slots across: the pool, its rows, the dynamic pool, the scratch */
    float on[4];                 /* x: the map is in use */
} VsmU;

#define BRR_VSM_STR_(x) #x
#define BRR_VSM_STR(x) BRR_VSM_STR_(x)
/* the map's GPU half: in the lighting's library after BRR_SHADOW_SRC (vogel) */
#define BRR_VSM_SRC \
"#define VS_LEVELS " BRR_VSM_STR(VSM_LEVELS) "\n#define VS_MARGIN " BRR_VSM_STR(VSM_MARGIN) "\n#define VS_AGE " BRR_VSM_STR(VSM_AGE) "\n" \
"#define VS_CELL " BRR_VSM_STR(VSM_CELL) "\n#define VS_BINCAP " BRR_VSM_STR(VSM_BINCAP) "\n" \
"struct VSU { float4 r, v, f, eye, z, tm, lod, dsz; int4 org[4]; uint4 cnt, pool; float4 on; };\n" \
"struct VSS { uint nrend, nwant, nfail, nfree, ndyn, nbin, pad0, pad1; uint4 atrk, adyn, afloor; uint dlev[8]; uint4 pad2; uint4 acopy; };\n" \
"struct VSG { float2 lo; int nx, ny; };\n" \
"static float3 vs_ls(constant VSU &u, float3 p) { return float3(dot(p, u.r.xyz), dot(p, u.v.xyz), dot(p, u.f.xyz)); }\n" \
"static int2 vs_org(constant VSU &u, int L) { int4 o = u.org[L >> 1]; return (L & 1) != 0 ? o.zw : o.xy; }\n" \
"static float vs_T(constant VSU &u, int L) { return u.z.w * float(1 << L); }\n" \
"static uint vs_idx(int L, int2 a) { int2 m = a & 63; return uint((L * 64 + m.y) * 64 + m.x); }\n" \
/* an entry's page: the one its slot holds in the level's window */ \
"static int2 vs_abs(constant VSU &u, uint i) { int L = int(i >> 12); int2 m = int2(int(i & 63u), int((i >> 6) & 63u)), o = vs_org(u, L); return o + ((m - o) & 63); }\n" \
"static uint vs_tag(int2 a) { return (uint(a.x) & 0xFFFFu) | (uint(a.y) << 16); }\n" \
"static bool vs_inwin(constant VSU &u, int L, float2 xy) {\n" \
"  float2 a = xy / (128.0 * vs_T(u, L)), o = float2(vs_org(u, L)); float m = VS_MARGIN / 128.0;\n" \
"  return all(a >= o + m) && all(a < o + 64.0 - m); }\n" \
/* the level for a point: from its pixel's footprint (lf, fractional), dithered by j in [0, 1), then the first whose window holds it */ \
"static float vs_lf(constant VSU &u, float3 p) { return log2(max(distance(p, u.eye.xyz) * u.eye.w, 1e-7) / u.z.w) + u.lod.x; }\n" \
"static int vs_level(constant VSU &u, float lf, float2 xy, float j) {\n" \
"  int L = max(int(floor(lf + (j - 0.5) * u.lod.y)), 0);\n" \
"  while (L < VS_LEVELS && !vs_inwin(u, L, xy)) L++;\n" \
"  return L; }\n" \
/* the receiver's offset along its normal and its depth bias, in texels (LU.sbias: brr_metal_lit.h casvis) */ \
"static float vs_nofs(float4 sb, float sl) { return sb.x + sb.y * sl; }\n" \
/* marking: the pages a sun-facing G-buffer pixel's lookups can read, at both levels of its dither */ \
"kernel void vs_mark(uint2 px [[thread_position_in_grid]], constant VSU &u [[buffer(0)]], constant float4 &sb [[buffer(1)]],\n" \
"                    texture2d<float> pos [[texture(0)]], texture2d<float> nrm [[texture(1)]], device uint *need [[buffer(2)]]) {\n" \
"  if (px.x >= pos.get_width() || px.y >= pos.get_height()) return;\n" \
"  float3 p = pos.read(px).xyz, n = nrm.read(px).xyz; if (all(p == 0.0)) return;\n" \
"  if (dot(n, u.eye.xyz - p) < 0.0) n = -n;\n" \
"  float nl = dot(n, -u.f.xyz); if (nl <= 0.0) return;\n" \
"  float sl = sqrt(max(1.0 - nl * nl, 0.0)), lf = vs_lf(u, p); float2 xy0 = vs_ls(u, p).xy; int last = -1;\n" \
"  for (int k = 0; k < 2; k++) { int L = vs_level(u, lf, xy0, k == 0 ? 0.0 : 0.999); if (L >= VS_LEVELS || L == last) continue; last = L;\n" \
"    float T = vs_T(u, L); float2 tc = vs_ls(u, p + n * T * vs_nofs(sb, sl)).xy / T;\n" \
"    int2 a0 = int2(floor((tc - VS_MARGIN) / 128.0)), a1 = int2(floor((tc + VS_MARGIN) / 128.0));\n" \
"    for (int y = a0.y; y <= a1.y; y++) for (int x = a0.x; x <= a1.x; x++) need[vs_idx(L, int2(x, y))] = u.cnt.x; } }\n" \
/* a page out of its window, or unneeded too long, back to the free stack; the dynamic slots emptied */ \
"kernel void vs_release(uint i [[thread_position_in_grid]], constant VSU &u [[buffer(0)]], device uint *ent [[buffer(1)]],\n" \
"                       const device uint *tag [[buffer(2)]], const device uint *need [[buffer(3)]], device uint *dyn [[buffer(4)]],\n" \
"                       device atomic_int *fcnt [[buffer(5)]], device uint *fst [[buffer(6)]]) {\n" \
"  if (i >= uint(VS_LEVELS * 4096)) return;\n" \
"  dyn[i] = 0xFFFFFFFFu;\n" \
"  uint e = ent[i], s = e & 0xFFFu; if (s == 0xFFFu) return;\n" \
"  if (tag[i] != vs_tag(vs_abs(u, i)) || u.cnt.x - need[i] > VS_AGE) {\n" \
"    int c = atomic_fetch_add_explicit(fcnt, 1, memory_order_relaxed); fst[c] = s; ent[i] = 0xFFFu; } }\n" \
/* the needed entries given slots, the coarsest level first; the undrawn listed to be drawn, at most cnt.y */ \
"kernel void vs_alloc(uint t [[thread_index_in_threadgroup]], constant VSU &u [[buffer(0)]], device uint *ent [[buffer(1)]],\n" \
"                     device uint *tag [[buffer(2)]], const device uint *need [[buffer(3)]], device atomic_int *fcnt [[buffer(5)]],\n" \
"                     const device uint *fst [[buffer(6)]], device uint *rl [[buffer(7)]], device VSS &st [[buffer(8)]]) {\n" \
"  threadgroup atomic_uint nr, nw, nf;\n" \
"  if (t == 0u) { atomic_store_explicit(&nr, 0u, memory_order_relaxed); atomic_store_explicit(&nw, 0u, memory_order_relaxed); atomic_store_explicit(&nf, 0u, memory_order_relaxed); }\n" \
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"  for (int L = VS_LEVELS - 1; L >= 0; L--) {\n" \
"    for (uint k = t; k < 4096u; k += 1024u) { uint i = uint(L) * 4096u + k; if (need[i] != u.cnt.x) continue;\n" \
"      uint e = ent[i];\n" \
"      if ((e & 0xFFFu) == 0xFFFu) { int c = atomic_fetch_sub_explicit(fcnt, 1, memory_order_relaxed);\n" \
"        if (c <= 0) { atomic_fetch_add_explicit(fcnt, 1, memory_order_relaxed); atomic_fetch_add_explicit(&nf, 1u, memory_order_relaxed); continue; }\n" \
"        e = fst[c - 1]; tag[i] = vs_tag(vs_abs(u, i)); }\n" \
"      if ((e & 0x1000u) == 0u) { uint r = atomic_fetch_add_explicit(&nr, 1u, memory_order_relaxed);\n" \
"        if (r < u.cnt.y) { rl[r] = i; e |= 0x1000u; } else atomic_fetch_add_explicit(&nw, 1u, memory_order_relaxed); }\n" \
"      ent[i] = e; }\n" \
"    threadgroup_barrier(mem_flags::mem_device | mem_flags::mem_threadgroup); }\n" \
"  if (t == 0u) { uint n = min(atomic_load_explicit(&nr, memory_order_relaxed), u.cnt.y); st.nrend = n;\n" \
"    st.nwant = atomic_load_explicit(&nw, memory_order_relaxed); st.nfail = atomic_load_explicit(&nf, memory_order_relaxed);\n" \
"    st.nfree = uint(max(atomic_load_explicit(fcnt, memory_order_relaxed), 0)); st.ndyn = 0u; st.nbin = 0u; } }\n" \
/* the track's triangles of each page to draw: a threadgroup a page */ \
"kernel void vs_bin(uint k [[threadgroup_position_in_grid]], uint i [[thread_index_in_threadgroup]], uint nt [[threads_per_threadgroup]],\n" \
"                   constant VSU &u [[buffer(0)]], const device uint *rl [[buffer(7)]], const device VSS &st [[buffer(8)]],\n" \
"                   const device int *start [[buffer(1)]], const device int *list [[buffer(2)]], const device float4 *tb [[buffer(3)]],\n" \
"                   constant VSG &g [[buffer(4)]], device atomic_uint *used [[buffer(5)]], device uint *out [[buffer(6)]]) {\n" \
"  if (k >= st.nrend) return;\n" \
"  threadgroup atomic_uint cnt; threadgroup uint base, ok;\n" \
"  uint idx = rl[k]; int L = int(idx >> 12); int2 a = vs_abs(u, idx); float T = vs_T(u, L), pw = 128.0 * T;\n" \
"  float4 rc = float4(float(a.x) * pw - T, float(a.y) * pw - T, float(a.x + 1) * pw + T, float(a.y + 1) * pw + T);\n" \
"  int x0 = clamp(int(floor((rc.x - g.lo.x) / VS_CELL)), 0, g.nx - 1), x1 = clamp(int(floor((rc.z - g.lo.x) / VS_CELL)), 0, g.nx - 1);\n" \
"  int y0 = clamp(int(floor((rc.y - g.lo.y) / VS_CELL)), 0, g.ny - 1), y1 = clamp(int(floor((rc.w - g.lo.y) / VS_CELL)), 0, g.ny - 1);\n" \
"  int w = x1 - x0 + 1, nc = w * (y1 - y0 + 1);\n" \
"  if (i == 0u) atomic_store_explicit(&cnt, 0u, memory_order_relaxed);\n" \
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"  for (int pass = 0; pass < 2; pass++) {\n" \
"    for (int q = int(i); q < nc; q += int(nt)) { int cx = x0 + q % w, cy = y0 + q / w, c = cy * g.nx + cx;\n" \
"      for (int e = start[c]; e < start[c + 1]; e++) { int tri = list[e]; float4 b = tb[tri];\n" \
"        if (b.x > rc.z || b.z < rc.x || b.y > rc.w || b.w < rc.y) continue;\n" \
"        int hx = clamp(int(floor((max(rc.x, b.x) - g.lo.x) / VS_CELL)), 0, g.nx - 1), hy = clamp(int(floor((max(rc.y, b.y) - g.lo.y) / VS_CELL)), 0, g.ny - 1);\n" \
"        if (hx != cx || hy != cy) continue;\n" \
"        uint o = atomic_fetch_add_explicit(&cnt, 1u, memory_order_relaxed);\n" \
"        if (pass == 1) out[base + o] = k << 20 | uint(tri); } }\n" \
"    threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"    if (pass == 0 && i == 0u) { uint n1 = atomic_load_explicit(&cnt, memory_order_relaxed), b = n1 ? atomic_fetch_add_explicit(used, n1, memory_order_relaxed) : 0u;\n" \
"      base = b; ok = n1 != 0u && b + n1 <= VS_BINCAP; atomic_store_explicit(&cnt, 0u, memory_order_relaxed); }\n" \
"    threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"    if (ok == 0u) return; } }\n" \
"kernel void vs_binargs(device VSS &st [[buffer(8)]], const device uint *used [[buffer(5)]]) {\n" \
"  uint n = min(used[0], uint(VS_BINCAP)); st.nbin = used[0]; st.atrk = uint4(3u * n, 1u, 0u, 0u);\n" \
"  st.acopy = uint4(9u, 9u, st.nrend, 0u); }   /* vs_copy's threadgroups: 9 x 9 of 16 x 16 a slot of 130 */\n" \
/* the scratch's slot r: a world point to the slot's clip space, the slot's edges its clip distances \
   (texel t of a page at slot texel t + 1: VSM_SLOT holds the page and a texel each side) */ \
"struct VSO { float4 pos [[position]]; float clip [[clip_distance]] [4]; };\n" \
"static VSO vs_place(constant VSU &u, float3 p, uint idx, uint slot, uint across, float2 wh) {\n" \
"  int L = int(idx >> 12); int2 a = vs_abs(u, idx); float T = vs_T(u, L); float3 P = vs_ls(u, p);\n" \
"  float2 t = P.xy / T - float2(a) * 128.0 + 1.0, o = float2(float(slot % across), float(slot / across)) * 130.0 + t;\n" \
"  VSO v; v.pos = float4(o.x / wh.x * 2.0 - 1.0, 1.0 - o.y / wh.y * 2.0, (P.z - u.z.x) * u.z.y, 1.0);\n" \
"  v.clip[0] = t.x; v.clip[1] = 130.0 - t.x; v.clip[2] = t.y; v.clip[3] = 130.0 - t.y; return v; }\n" \
"vertex VSO vs_trk(uint v [[vertex_id]], const device uint *bin [[buffer(0)]], const device packed_float3 *trk [[buffer(1)]],\n" \
"                  constant VSU &u [[buffer(2)]], const device uint *rl [[buffer(3)]]) {\n" \
"  uint e = bin[v / 3u], r = e >> 20;\n" \
"  return vs_place(u, float3(trk[3u * (e & 0xFFFFFu) + v % 3u]), rl[r], r, u.pool.w, u.lod.zw); }\n" \
"kernel void vs_copy(uint3 q [[thread_position_in_grid]], constant VSU &u [[buffer(0)]], const device uint *ent [[buffer(1)]],\n" \
"                    const device uint *rl [[buffer(7)]], const device VSS &st [[buffer(8)]],\n" \
"                    depth2d<float> scr [[texture(0)]], texture2d<float, access::write> pool [[texture(1)]]) {\n" \
"  if (q.z >= st.nrend || q.x >= 130u || q.y >= 130u) return;   /* q: thread in grid (threadgroups 9 x 9 x pages drawn) */\n" \
"  uint s = ent[rl[q.z]] & 0xFFFu;\n" \
"  uint2 a = uint2(q.z % u.pool.w, q.z / u.pool.w) * 130u + q.xy, b = uint2(s % u.pool.x, s / u.pool.x) * 130u + q.xy;\n" \
"  pool.write(float4(scr.read(a)), b); }\n" \
/* the moving casters' pages: a thread a triangle of cast_pick's list (castargs[0] vertices, at most k.y) and a level, \
   the needed pages under its box in the sun's axes given a slot of the dynamic pool (one slot a page) */ \
"kernel void vs_dynmark(uint t [[thread_position_in_grid]], constant VSU &u [[buffer(0)]], const device packed_float3 *mv [[buffer(1)]],\n" \
"                       const device uint *need [[buffer(3)]], device atomic_uint *dyn [[buffer(4)]], device atomic_uint *dc [[buffer(5)]],\n" \
"                       device uint *dl [[buffer(6)]], const device uint *castargs [[buffer(9)]], constant uint4 &k [[buffer(10)]]) {\n" \
"  uint tri = t / uint(VS_LEVELS); int L = int(t % uint(VS_LEVELS)); if (3u * tri + 3u > min(castargs[0], k.y)) return;\n" \
"  float2 lo = float2(1e30), hi = float2(-1e30); for (uint c = 0u; c < 3u; c++) { float2 q = vs_ls(u, float3(mv[3u * tri + c])).xy; lo = min(lo, q); hi = max(hi, q); }\n" \
"  float T = vs_T(u, L), pw = 128.0 * T; int2 o = vs_org(u, L);\n" \
"  int2 a0 = max(int2(floor((lo - T) / pw)), o), a1 = min(int2(floor((hi + T) / pw)), o + 63);\n" \
"  for (int y = a0.y; y <= a1.y; y++) for (int x = a0.x; x <= a1.x; x++) { uint i = vs_idx(L, int2(x, y)); if (need[i] != u.cnt.x) continue;\n" \
"    uint ex = 0xFFFFFFFFu; bool won = false;   /* a weak exchange may fail spuriously: again while the entry is still empty */\n" \
"    while (!(won = atomic_compare_exchange_weak_explicit(&dyn[i], &ex, 0xFFFFFFFEu, memory_order_relaxed, memory_order_relaxed)) && ex == 0xFFFFFFFFu) {}\n" \
"    if (!won) continue;\n" \
"    atomic_fetch_add_explicit(&dc[4 + L], 1u, memory_order_relaxed);   /* by level, for BR_LIGHT_VSMLOG */\n" \
"    uint d = atomic_fetch_add_explicit(dc, 1u, memory_order_relaxed);\n" \
"    if (d < u.cnt.z) { dl[d] = i; atomic_store_explicit(&dyn[i], d, memory_order_relaxed); } else atomic_store_explicit(&dyn[i], 0xFFFFFFFFu, memory_order_relaxed); } }\n" \
"kernel void vs_dynargs(device VSS &st [[buffer(8)]], const device uint *dc [[buffer(5)]], const device uint *castargs [[buffer(9)]], constant uint4 &k [[buffer(10)]]) {\n" \
"  uint n = min(dc[0], k.x), nv = min(castargs[0], k.y) / 3u * 3u; st.ndyn = dc[0]; for (int q = 0; q < 8; q++) st.dlev[q] = dc[4 + q];\n" \
"  st.adyn = uint4(nv, nv ? n : 0u, 0u, 0u); st.afloor = uint4(k.z, k.z ? n : 0u, 0u, 0u); }\n" \
"vertex VSO vs_dyn(uint v [[vertex_id]], uint d [[instance_id]], const device packed_float3 *pts [[buffer(0)]],\n" \
"                  constant VSU &u [[buffer(2)]], const device uint *dl [[buffer(3)]]) {\n" \
"  return vs_place(u, float3(pts[v]), dl[d], d, u.pool.z, u.dsz.xy); }\n" \
/* four texels' depths around tc (level L's texels, absolute) through the page table: the static page's, \
   the nearer of it and the dynamic page's where a moving caster has one; ok false where the page is not \
   drawn.  c holds the last page looked up: a lookup's samples mostly share the centre's page */ \
"struct VSC { int2 a; uint e, d; };\n" \
"static float4 vs_gather(constant VSU &u, const device uint *ent, const device uint *dyn, texture2d<float> pool, depth2d<float> dp,\n" \
"                        int L, float2 tc, thread VSC &c, thread float2 &f, thread bool &ok) {\n" \
"  constexpr sampler ps(filter::nearest, address::clamp_to_edge);\n" \
"  float2 b = tc - 0.5, i0 = floor(b); f = b - i0; int2 a = int2(floor(i0 / 128.0)), l = int2(i0 - float2(a) * 128.0);\n" \
"  if (any(a != c.a)) { uint i = vs_idx(L, a); c.a = a; c.e = ent[i]; c.d = dyn[i]; }\n" \
"  ok = (c.e & 0x1000u) != 0u; if (!ok) return float4(1.0);\n" \
"  uint s = c.e & 0xFFFu; float2 o = float2(float(s % u.pool.x), float(s / u.pool.x)) * 130.0 + float2(l) + 2.0;\n" \
"  float4 g = pool.gather(ps, o / u.dsz.zw);\n" \
"  if (c.d < u.cnt.z) { float2 od = float2(float(c.d % u.pool.z), float(c.d / u.pool.z)) * 130.0 + float2(l) + 2.0; g = min(g, dp.gather(ps, od / u.dsz.xy)); }\n" \
"  return g; }\n" \
/* the share of the 2x2 texels at least as deep as zr, bilinear (gather's order: x (0,1) y (1,1) z (1,0) w (0,0)) */ \
"static float vs_pcf(float4 g, float2 f, float zr) { float4 c = select(float4(0.0), float4(1.0), float4(zr) <= g); return mix(mix(c.w, c.z, f.x), mix(c.x, c.y, f.x), f.y); }\n" \
/* the track map's lookup, the last fallback (and the fog's sun): one bilinear comparison */ \
"static float vs_tmapvis(constant VSU &u, float4 sb, depth2d<float> tmap, float3 p, float3 n, float sl) {\n" \
"  constexpr sampler cs(filter::linear, address::clamp_to_edge, compare_func::less_equal);\n" \
"  float T = u.tm.w; float3 P = vs_ls(u, p + n * T * vs_nofs(sb, sl)); float2 st = (P.xy - u.tm.xy) / u.tm.z;\n" \
"  if (any(st <= 0.0) || any(st >= 1.0)) return 1.0;\n" \
"  return tmap.sample_compare(cs, st, (P.z - u.z.x) * u.z.y - (sb.z + 1.0) * T * u.z.y); }\n" \
"static float vsmvis(constant VSU &u, float4 sb, float4 cs, const device uint *ent, const device uint *dyn, texture2d<float> pool, depth2d<float> dp,\n" \
"                    depth2d<float> tmap, float3 p, float3 n, float j, float2 rot) {\n" \
"  float nl = clamp(dot(n, -u.f.xyz), 0.0, 1.0), sl = sqrt(1.0 - nl * nl), nz = max(nl, 0.125);\n" \
"  int L = vs_level(u, vs_lf(u, p), vs_ls(u, p).xy, j);\n" \
"  for (int tr = 0; tr < 3 && L < VS_LEVELS; tr++, L++) {\n" \
"    float T = vs_T(u, L); float3 P = vs_ls(u, p + n * T * vs_nofs(sb, sl)); float2 tc = P.xy / T, f; bool ok; VSC c; c.a = int2(0x7FFFFFFF);\n" \
"    vs_gather(u, ent, dyn, pool, dp, L, tc, c, f, ok); if (!ok) continue;\n" \
"    float zr = (P.z - u.z.x) * u.z.y - sb.z * T * u.z.y;\n" \
/* the receiver's plane: depth per texel across r and v (casvis) */ \
"    float2 dz = float2(dot(n, u.r.xyz), dot(n, u.v.xyz)) / nz * T * u.z.y;\n" \
"    float tpd = u.z.z * cs.x / T, rs = clamp(zr * tpd, 1.0, cs.y), zb = 0.0, nb = 0.0;\n" \
"    for (int i = 0; i < 16; i++) { float2 o = vogel(i, 16, rot) * rs, fo; bool ko; float4 g = vs_gather(u, ent, dyn, pool, dp, L, tc + o, c, fo, ko);\n" \
"      if (!ko) continue; float4 bk = select(float4(0.0), float4(1.0), g < zr + dot(dz, o)); zb += dot(g, bk); nb += dot(bk, float4(1.0)); }\n" \
"    if (nb == 0.0) return 1.0;\n" \
/* the filter's taps by its width: a penumbra under a texel (the sun's 0.535 degrees over less than \
   T / 0.0093 of blocker distance) needs no more than the bilinear comparison and the temporal pass */ \
"    float wr = min((zr - zb / nb) * tpd, cs.z), vis = 0.0; int nt = wr <= 1.0 ? 4 : wr <= 4.0 ? 12 : 24;\n" \
"    for (int i = 0; i < nt; i++) { float2 o = vogel(i, nt, rot) * wr, fo; bool ko; float4 g = vs_gather(u, ent, dyn, pool, dp, L, tc + o, c, fo, ko);\n" \
"      vis += ko ? vs_pcf(g, fo, zr + dot(dz, o)) : 1.0; }\n" \
"    return vis / float(nt); }\n" \
"  return vs_tmapvis(u, sb, tmap, p, n, sl); }\n"

#endif
