/* brr_metal_gi_src.h: the irradiance probes' shader source (brr_metal_gi.h
 * says what they are).  BRR_GI_SAMPLE_SRC (the volume's constants and
 * gi_irr, the lookup) goes into the lighting's own source (brr_metal_lit.h);
 * BRR_GI_SRC (the lookup and the update's kernels) into the ray tracing
 * library (brr_gi_init).  Plain macros, no Objective-C. */
#ifndef BRR_METAL_GI_SRC_H
#define BRR_METAL_GI_SRC_H

#define BRR_GI_IRR 8        /* irradiance texels a probe side (interior) */
#define BRR_GI_DST 8        /* distance texels a probe side (interior) */
#define BRR_GI_KEEP 0.75f   /* a bake pass's most share of the passes before it */
#define BRR_GI_CELL 32      /* lamps a light grid cube lists at most (the source's 32) */
#define BRR_GI_GRID 32      /* light grid cubes an axis at most */
/* the sizes as the shader source spells them */
#define BRR_GI_DEFS "#define BRR_GI_IRR_S 8\n#define BRR_GI_DST_S 8\n"

/* the probe volume's constants (BrrGiU below: the same layout) */
#define BRR_GI_U_SRC \
"struct GiU { float4 org; int4 n; int4 base; int4 pbase; float4 sun; float4 sunc; float4 sky; float4 gnd;\n" \
"             float4 par; float4 rot0; float4 rot1; float4 rot2; float4 lim; float4 gorg; int4 gdim; int4 upd; int4 bk; int4 bx; };\n"

#define BRR_GI_SAMPLE_SRC \
BRR_GI_U_SRC \
"static float2 gi_snz(float2 v) { return float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0); }\n" \
/* the octahedral map: a direction to [-1, 1]^2 and back */ \
"static float2 gi_oct(float3 d) {\n" \
"  float2 p = d.xy / (abs(d.x) + abs(d.y) + abs(d.z));\n" \
"  return d.z < 0.0 ? (1.0 - abs(p.yx)) * gi_snz(p) : p; }\n" \
/* the probes are baked over the whole track (brr_gi_bake), sparse: a grid of \
   n.xyz points spacing org.w from org.xyz, a probe only where a surface \
   needs one.  off holds, one after the other: each probe's place moved out \
   of the track's solid (gi_reloc), float4 a probe (bx.x of them); from float4 \
   bx.x the grid's points' probes (uint, ~0u none); from float4 bx.y each \
   probe's grid point (int4).  A probe's tile in the atlas: bk.y tiles a row */ \
"static int gi_find(constant GiU &u, device const float4 *off, int3 g) {\n" \
"  if (any(g < 0) || any(g >= u.n.xyz)) return -1;\n" \
"  uint s = ((device const uint *)(off + u.bx.x))[g.x + u.n.x * (g.y + u.n.y * g.z)];\n" \
"  return s == 0xFFFFFFFFu ? -1 : int(s); }\n" \
"static int3 gi_sgrid(constant GiU &u, device const float4 *off, int s) { return ((device const int4 *)(off + u.bx.y))[s].xyz; }\n" \
"static int2 gi_tile(constant GiU &u, int s) { return int2(s % u.bk.y, s / u.bk.y); }\n" \
"static float3 gi_pos(constant GiU &u, int3 g) { return u.org.xyz + float3(u.base.xyz + g) * u.org.w; }\n" \
/* the atlas coordinate of direction d in tile t (texels of side R plus border) */ \
"static float2 gi_uv(int2 t, float3 d, int R, float2 sz) {\n" \
"  float2 px = float2(t * (R + 2)) + 1.0 + (gi_oct(d) * 0.5 + 0.5) * float(R);\n" \
"  return px / sz; }\n" \
/* a probe's irradiance: spherical harmonics to order 2 (Ramamoorthi and \
   Hanrahan 2001), 9 coefficients a slot (sh[9 slot + k], rgb), already \
   convolved with the cosine (A0 pi, A1 2 pi / 3, A2 pi / 4): E(n) their \
   sum against the basis.  Exact for light of order 2 or less (a sky's \
   gradient: the octahedral 8 x 8 texels it replaced were off by up to 5.7% \
   there), its truncation within 3.2% of a lamp-lit box's first bounce */ \
"static void gi_y9(float3 d, thread float *y) {\n" \
"  y[0] = 0.282095; y[1] = 0.488603 * d.y; y[2] = 0.488603 * d.z; y[3] = 0.488603 * d.x;\n" \
"  y[4] = 1.092548 * d.x * d.y; y[5] = 1.092548 * d.y * d.z; y[6] = 0.315392 * (3.0 * d.z * d.z - 1.0);\n" \
"  y[7] = 1.092548 * d.x * d.z; y[8] = 0.546274 * (d.x * d.x - d.y * d.y); }\n" \
"static float3 gi_sh_eval(device const float4 *sh, int slot, float3 n) {\n" \
"  float y[9]; gi_y9(n, y); float3 e = 0.0;\n" \
"  for (int k = 0; k < 9; k++) e += sh[9 * slot + k].rgb * y[k];\n" \
"  return max(e, 0.0); }\n" \
/* the indirect irradiance at p (normal n, v toward the eye) from last \
   frame's probes; lim.y: Chebyshev visibility on */ \
"static float3 gi_irr(constant GiU &u, device const float4 *off, device const float4 *sh, texture2d<float> dst, float3 p, float3 n, float3 v) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge);\n" \
"  float sp = u.org.w; float3 ps = p + (n * 0.2 + v * 0.8) * (0.75 * sp) * u.par.w;\n" \
"  float3 g = (ps - u.org.xyz) / sp - float3(u.base.xyz);\n" \
"  float3 g0 = clamp(floor(g), float3(0), float3(u.n.xyz - 2));\n" \
"  float3 al = clamp(g - g0, 0.0, 1.0);\n" \
"  float2 dsz = float2(dst.get_width(), dst.get_height());\n" \
"  float3 sum = 0.0; float ws = 0.0;\n" \
"  for (int i = 0; i < 8; i++) {\n" \
"    int3 o = int3(i & 1, (i >> 1) & 1, i >> 2), gi = int3(g0) + o;\n" \
"    int sl = gi_find(u, off, gi); if (sl < 0) continue;   /* no probe there: no surface near it */\n" \
"    float3 pp = gi_pos(u, gi) + off[sl].xyz; int2 t = gi_tile(u, sl);\n" \
"    float3 tri = mix(1.0 - al, al, float3(o)); float w = tri.x * tri.y * tri.z;\n" \
"    float3 tp = normalize(pp - p); float bf = (dot(tp, n) + 1.0) * 0.5; w *= bf * bf + 0.2;\n" \
"    if (u.lim.y > 0.5) {\n" \
"      float3 r = ps - pp; float d = length(r);\n" \
"      float2 m = dst.sample(s, gi_uv(t, d > 0.0 ? r / d : float3(0, 0, 1), BRR_GI_DST_S, dsz)).rg;\n" \
"      if (m.x <= 0.0) continue;   /* a probe inside the track's solid (gi_fold): none of its light */\n" \
"      float ch = 1.0;\n" \
"      if (d > m.x) { float var = abs(m.y - m.x * m.x), dd = d - m.x; ch = var / (var + dd * dd); ch = max(ch * ch * ch, 0.0); }\n" \
"      w *= ch;\n" \
"    }\n" \
"    w = max(w, 1e-6);\n" \
"    if (w < 0.2) w *= w * w / 0.04;\n" \
"    sum += w * gi_sh_eval(sh, sl, n); ws += w;\n" \
"  }\n" \
"  return ws > 0.0 ? sum / ws : mix(u.gnd.rgb, u.sky.rgb, n.z * 0.5 + 0.5) * 3.14159265; }   /* every probe about it buried: a miss's light */\n" \
/* the same weights for a second direction r2 (the lighting's specular \
   lookup, BRR_GISPEC_R0): its irradiance in e2 */ \
"static float3 gi_irr2(constant GiU &u, device const float4 *off, device const float4 *sh, texture2d<float> dst, float3 p, float3 n, float3 v, float3 r2, thread float3 &e2) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge);\n" \
"  float sp = u.org.w; float3 ps = p + (n * 0.2 + v * 0.8) * (0.75 * sp) * u.par.w;\n" \
"  float3 g = (ps - u.org.xyz) / sp - float3(u.base.xyz);\n" \
"  float3 g0 = clamp(floor(g), float3(0), float3(u.n.xyz - 2));\n" \
"  float3 al = clamp(g - g0, 0.0, 1.0);\n" \
"  float2 dsz = float2(dst.get_width(), dst.get_height());\n" \
"  float3 sum = 0.0, sum2 = 0.0; float ws = 0.0;\n" \
"  for (int i = 0; i < 8; i++) {\n" \
"    int3 o = int3(i & 1, (i >> 1) & 1, i >> 2), gi = int3(g0) + o;\n" \
"    int sl = gi_find(u, off, gi); if (sl < 0) continue;   /* no probe there: no surface near it */\n" \
"    float3 pp = gi_pos(u, gi) + off[sl].xyz; int2 t = gi_tile(u, sl);\n" \
"    float3 tri = mix(1.0 - al, al, float3(o)); float w = tri.x * tri.y * tri.z;\n" \
"    float3 tp = normalize(pp - p); float bf = (dot(tp, n) + 1.0) * 0.5; w *= bf * bf + 0.2;\n" \
"    if (u.lim.y > 0.5) {\n" \
"      float3 r = ps - pp; float d = length(r);\n" \
"      float2 m = dst.sample(s, gi_uv(t, d > 0.0 ? r / d : float3(0, 0, 1), BRR_GI_DST_S, dsz)).rg;\n" \
"      if (m.x <= 0.0) continue;   /* a probe inside the track's solid (gi_fold): none of its light */\n" \
"      float ch = 1.0;\n" \
"      if (d > m.x) { float var = abs(m.y - m.x * m.x), dd = d - m.x; ch = var / (var + dd * dd); ch = max(ch * ch * ch, 0.0); }\n" \
"      w *= ch;\n" \
"    }\n" \
"    w = max(w, 1e-6);\n" \
"    if (w < 0.2) w *= w * w / 0.04;\n" \
"    sum += w * gi_sh_eval(sh, sl, n); sum2 += w * gi_sh_eval(sh, sl, r2); ws += w;\n" \
"  }\n" \
"  e2 = ws > 0.0 ? sum2 / ws : mix(u.gnd.rgb, u.sky.rgb, r2.z * 0.5 + 0.5) * 3.14159265;\n" \
"  return ws > 0.0 ? sum / ws : mix(u.gnd.rgb, u.sky.rgb, n.z * 0.5 + 0.5) * 3.14159265; }\n"

/* the update's kernels (needs BRR_RT_SRC and BRR_LIGHTS_SRC before it) */
#define BRR_GI_SRC \
BRR_GI_SAMPLE_SRC \
"constant float GI_PI = 3.14159265;\n" \
/* a probe whose rays meet back faces past this share is inside the track's \
   solid (under the ground, in a hill or a wall: the track's faces wind \
   outward), its light none of the scene's (Majercik et al. 2021, probe \
   classification); its distances written 0, gi_irr leaves it out */ \
"constant float GI_BACK = 0.25;\n" \
"static float3 gi_unoct(float2 e) {\n" \
"  float3 v = float3(e, 1.0 - abs(e.x) - abs(e.y));\n" \
"  if (v.z < 0.0) v.xy = (1.0 - abs(v.yx)) * gi_snz(v.xy);\n" \
"  return normalize(v); }\n" \
/* ray i of n: a spherical Fibonacci point turned by this frame's rotation */ \
"static float3 gi_ray(constant GiU &u, int i) {\n" \
"  float N = float(u.n.w), z = 1.0 - (2.0 * float(i) + 1.0) / N, r = sqrt(max(1.0 - z * z, 0.0));\n" \
"  float ph = float(i) * 2.39996323; float3 d = float3(r * cos(ph), r * sin(ph), z);\n" \
"  return float3(dot(u.rot0.xyz, d), dot(u.rot1.xyz, d), dot(u.rot2.xyz, d)); }\n" \
/* rgba8 surface record colours */ \
"static float3 gi_rgb(uint c) { return float3(c & 255u, (c >> 8) & 255u, (c >> 16) & 255u) / 255.0; }\n" \
/* the light grid: cubes of side gorg.w from gorg.xyz, gdim.xyz of them, \
   covering the volume and the rays' reach; a cube's list holds the lamps \
   whose sphere of influence (range: lwin is zero from it on) overlaps it, \
   at most gdim.w, the most intense at the cube's centre first.  Not the \
   camera's clusters, which a hit off screen is outside */ \
"static int gi_cell(constant GiU &u, float3 p) {\n" \
"  int3 c = clamp(int3(floor((p - u.gorg.xyz) / u.gorg.w)), int3(0), u.gdim.xyz - 1);\n" \
"  return c.x + u.gdim.x * (c.y + u.gdim.y * c.z); }\n" \
/* one thread a cube: the lamps of the set (lim.z of them) binned */ \
"kernel void gi_grid(constant GiU &u [[buffer(0)]], const device LSet &ls [[buffer(1)]],\n" \
"                    device uint *cnt [[buffer(2)]], device ushort *lst [[buffer(3)]], uint gid [[thread_position_in_grid]]) {\n" \
"  int nc = u.gdim.x * u.gdim.y * u.gdim.z; if (int(gid) >= nc) return;\n" \
"  int3 c = int3(int(gid) % u.gdim.x, (int(gid) / u.gdim.x) % u.gdim.y, int(gid) / (u.gdim.x * u.gdim.y));\n" \
"  float3 lo = u.gorg.xyz + float3(c) * u.gorg.w, hi = lo + u.gorg.w, mid = (lo + hi) * 0.5;\n" \
"  float sc[32]; ushort id[32]; int k = 0, cap = min(u.gdim.w, 32), nl = int(u.lim.z);\n" \
"  for (int i = 0; i < nl; i++) {\n" \
"    LS l = ls.l[i]; if ((u.bx.w & 1) != 0 && l.kind < 1.5) continue;   /* a bake: the track's own lamps, not the cars' */\n" \
"    float3 q = float3(l.pos), e = q - clamp(q, lo, hi);\n" \
"    if (!(dot(e, e) < l.range * l.range)) continue;\n" \
"    float3 m = q - mid; float s = l.cd * max(max(l.rgb[0], l.rgb[1]), l.rgb[2]) / max(dot(m, m), 1.0);\n" \
"    if (k == cap && s <= sc[cap - 1]) continue;\n" \
"    int j = k < cap ? k++ : cap - 1;\n" \
"    while (j > 0 && sc[j - 1] < s) { sc[j] = sc[j - 1]; id[j] = id[j - 1]; j--; }\n" \
"    sc[j] = s; id[j] = ushort(i);\n" \
"  }\n" \
"  cnt[gid] = uint(k);\n" \
"  for (int j = 0; j < k; j++) lst[int(gid) * 32 + j] = id[j];\n" \
"}\n" \
/* the lamps' irradiance at a hit (lux), unshadowed: the lamps of the hit's cube */ \
"static float3 gi_lamps(constant GiU &u, const device LSet &ls, device const uint *cnt, device const ushort *lst, float3 p, float3 n) {\n" \
"  float3 E = 0.0; if (u.lim.z < 0.5) return E;\n" \
"  int ce = gi_cell(u, p), nl = int(cnt[ce]);\n" \
"  for (int k = 0; k < nl; k++) {\n" \
"    LS l = ls.l[lst[ce * 32 + k]]; float3 L = float3(l.pos) - p; float d2 = max(dot(L, L), 0.01), dl = sqrt(d2); float3 ld = L / dl;\n" \
"    float c = dot(n, ld); if (c <= 0.0) continue;\n" \
"    E += float3(l.rgb) * (lamp(l, -ld) * lwin(l, -ld, dl) / d2 * c);\n" \
"  }\n" \
"  return E; }\n" \
/* a bake pass works on a batch of probes, bk.z from bk.w of them: a thread's \
   probe, -1 past the batch; bx.z the pass (0: the probes start from their \
   rays alone and their grid places) */ \
"static int gi_bat(constant GiU &u, uint i) { return int(i) < u.bk.w ? u.bk.z + int(i) : -1; }\n" \
/* the rays: x the ray, y the probe in the batch; out: radiance (cd/m2, \
   not pre-exposed), distance.  leaf: the track's triangle -> its leaf slot */ \
"kernel void gi_trace(constant GiU &u [[buffer(0)]], device float4 *out [[buffer(1)]], device const uint *leaf [[buffer(2)]],\n" \
"                     const device LSet &ls [[buffer(3)]],\n" \
"                     device const RtNode *node [[buffer(4)]], device const float *tri [[buffer(5)]],\n" \
"                     device const uint *id [[buffer(6)]], device const RtPrim *prim [[buffer(7)]] RT_AS(8),\n" \
"                     device const uint *cnt [[buffer(9)]], device const ushort *lst [[buffer(10)]], device const float4 *off [[buffer(11)]],\n" \
"                     device const float4 *sh [[buffer(12)]], texture2d<float> dst [[texture(1)]], texturecube<float> env [[texture(2)]],\n" \
"                     uint2 gid [[thread_position_in_grid]]) {\n" \
"  int nr = u.n.w, pl = gi_bat(u, gid.y);\n" \
"  if (int(gid.x) >= nr || pl < 0) return;\n" \
"  float3 o = gi_pos(u, gi_sgrid(u, off, pl)) + off[pl].xyz, d = gi_ray(u, int(gid.x));\n" \
"  RtHit h = RT_CLOSEST(o, d, 0.0, u.lim.x);\n" \
"  float3 L; bool back = false;\n" \
"  constexpr sampler es(filter::linear, mip_filter::linear);\n" \
"  if (h.tri < 0) L = u.sky.w > 0.5 ? env.sample(es, d, level(u.gnd.w)).rgb : mix(u.gnd.rgb, u.sky.rgb, d.z * 0.5 + 0.5);\n" \
"  else {\n" \
"    device const packed_float3 *tv = (device const packed_float3 *)tri; uint j = leaf[h.tri];\n" \
"    float3 p0 = float3(tv[3 * j]), p1 = float3(tv[3 * j + 1]), p2 = float3(tv[3 * j + 2]);\n" \
"    float3 n = cross(p1 - p0, p2 - p0); float ln = length(n); n = ln > 0.0 ? n / ln : -d;\n" \
"    back = dot(n, d) > 0.0; if (back) n = -n;\n" \
"    RtPrim pr = prim[h.tri];\n" \
"    float3 alb = pr.albedo ? gi_rgb(pr.albedo) : float3(u.par.y);\n" \
"    float3 hp = o + d * h.t, E = 0.0;\n" \
"    float nl = dot(n, u.sun.xyz);\n" \
"    if (nl > 0.0 && any(u.sunc.rgb > 0.0) && !RT_ANY(hp + n * 1e-3 * max(1.0, h.t), u.sun.xyz, 0.0, 1e30))\n" \
"      E += u.sunc.rgb * nl;\n" \
"    E += gi_lamps(u, ls, cnt, lst, hp, n);\n" \
"    if (u.lim.w > 0.5) E += gi_irr(u, off, sh, dst, hp, n, -d);\n" \
"    L = gi_rgb(pr.emit) * u.par.z + alb / GI_PI * E;\n" \
"  }\n" \
"  out[int(gid.y) * nr + int(gid.x)] = float4(L, h.tri < 0 ? u.lim.x : back ? -h.t : h.t);   /* a back face's hit: its distance negated */\n" \
"}\n" \
/* probe relocation (Majercik et al. 2021; RTXGI's DDGIProbeRelocation): \
   a probe starts at its grid place (gi_offreset, the first pass, before \
   the trace); after it, a probe whose rays meet back faces past GI_BACK \
   moves through the nearest one to GI_MINFRONT / 2 beyond it, one nearer \
   than GI_MINFRONT to a front face moves off it, never past 0.45 of the \
   spacing on an axis; the next update traces from there, the lookups \
   weigh it there (off) */ \
"constant float GI_MINFRONT = 1.0;\n" \
"kernel void gi_offreset(constant GiU &u [[buffer(0)]], device float4 *off [[buffer(11)]], uint i [[thread_position_in_grid]]) {\n" \
"  int pl = gi_bat(u, i); if (pl >= 0 && u.bx.z == 0) off[pl] = float4(0); }\n" \
"kernel void gi_reloc(constant GiU &u [[buffer(0)]], device const float4 *ray [[buffer(1)]], device float4 *off [[buffer(11)]],\n" \
"                     uint bi [[thread_position_in_grid]]) {\n" \
"  int nr = u.n.w, pl = gi_bat(u, bi); if (pl < 0) return;\n" \
"  float bt = 1e30, ft = 1e30; float3 bd = 0.0, fd = 0.0; int nb = 0;\n" \
"  for (int i = 0; i < nr; i++) { float t = ray[int(bi) * nr + i].w; float3 d = gi_ray(u, i);\n" \
"    if (t < 0.0) { nb++; if (-t < bt) { bt = -t; bd = d; } }\n" \
"    else if (t < u.lim.x && t < ft) { ft = t; fd = d; } }\n" \
"  float3 cur = off[pl].xyz, nw = cur;\n" \
"  if (float(nb) > GI_BACK * float(nr)) nw = cur + bd * (bt + GI_MINFRONT * 0.5);\n" \
"  else if (ft < GI_MINFRONT) nw = cur - fd * (GI_MINFRONT - ft);\n" \
"  if (all(abs(nw) < 0.45 * u.org.w)) off[pl] = float4(nw, 0.0); }\n" \
/* an atlas texel's interior texel (the border repeats the octahedron's \
   neighbour across the edge) */ \
"static int2 gi_inner(int2 l, int R) {\n" \
"  int x = l.x, y = l.y, E = R + 1;\n" \
"  if ((x == 0 || x == E) && (y == 0 || y == E)) { x = x == 0 ? R : 1; y = y == 0 ? R : 1; }\n" \
"  else if (y == 0 || y == E) { x = E - x; y = y == 0 ? 1 : R; }\n" \
"  else if (x == 0 || x == E) { y = E - y; x = x == 0 ? 1 : R; }\n" \
"  return int2(x - 1, y - 1); }\n" \
/* one atlas texel of the batch's rows (gid.y from the batch's first row): \
   the probe's rays folded in, the cosine (irradiance) or its 50th power \
   (distance) the weight; the first pass starts from its rays alone */ \
"static void gi_fold(constant GiU &u, device const float4 *ray, texture2d<float, access::read> prev,\n" \
"                    texture2d<float, access::write> next, uint2 gid, int R, bool dist) {\n" \
"  int T = R + 2; gid.y += uint(u.bk.z / u.bk.y * T); int2 tile = int2(gid) / T, l = int2(gid) % T;\n" \
"  int pl = tile.x + u.bk.y * tile.y, bi = pl - u.bk.z, nr = u.n.w;\n" \
"  if (tile.x >= u.bk.y || bi < 0 || bi >= u.bk.w) return;\n" \
"  int2 in = gi_inner(l, R); float3 dir = gi_unoct((float2(in) + 0.5) / float(R) * 2.0 - 1.0);\n" \
"  float3 s = 0.0; float ws = 0.0; int nb = 0;\n" \
"  for (int i = 0; i < nr; i++) {\n" \
"    float4 r = ray[bi * nr + i]; float c = max(dot(dir, gi_ray(u, i)), 0.0); nb += r.w < 0.0;\n" \
"    if (dist) { float w = pow(c, 50.0), t = min(abs(r.w), u.lim.x); s += w * float3(t, t * t, 0); ws += w; }\n" \
"    else { s += c * r.rgb; ws += c; }\n" \
"  }\n" \
"  float4 old = prev.read(gid);\n" \
"  if (float(nb) > GI_BACK * float(nr)) { next.write(float4(0.0, 0.0, 0.0, 1.0), gid); return; }   /* inside the track's solid: off */\n" \
"  if (ws <= 0.0) { next.write(old, gid); return; }\n" \
"  float3 nw = dist ? s / ws : s / ws * GI_PI;\n" \
/* par.x is the pass's share of the old: k / (k + 1) at pass k, every pass's rays weighed alike */ \
"  float h = u.bx.z == 0 ? 0.0 : u.par.x;\n" \
"  next.write(float4(mix(nw, old.rgb, h), 1.0), gid);\n" \
"}\n" \
/* a probe's rays into its 9 coefficients: (4 pi / N) sum L Y (unbiased), \
   times the cosine's A_l, blended as the atlas texels are; a probe inside \
   the track's solid holds none */ \
"kernel void gi_blend_sh(constant GiU &u [[buffer(0)]], device const float4 *ray [[buffer(1)]], device float4 *sh [[buffer(12)]],\n" \
"                        uint bi [[thread_position_in_grid]]) {\n" \
"  int nr = u.n.w, pl = gi_bat(u, bi); if (pl < 0) return;\n" \
"  float3 c[9]; float y[9]; int nb = 0;\n" \
"  for (int k = 0; k < 9; k++) c[k] = 0.0;\n" \
"  for (int i = 0; i < nr; i++) { float4 r = ray[int(bi) * nr + i]; nb += r.w < 0.0; gi_y9(gi_ray(u, i), y);\n" \
"    for (int k = 0; k < 9; k++) c[k] += r.rgb * y[k]; }\n" \
"  bool dead = float(nb) > GI_BACK * float(nr);\n" \
"  float h = u.bx.z == 0 ? 0.0 : u.par.x, sc = 4.0 * GI_PI / float(nr);\n" \
"  for (int k = 0; k < 9; k++) {\n" \
"    float A = k == 0 ? GI_PI : k < 4 ? 2.0 * GI_PI / 3.0 : GI_PI / 4.0;\n" \
"    float3 nw = dead ? float3(0) : c[k] * sc * A;\n" \
"    sh[9 * pl + k] = float4(dead ? nw : mix(nw, sh[9 * pl + k].rgb, h), 0.0); } }\n" \
"kernel void gi_blend_dst(constant GiU &u [[buffer(0)]], device const float4 *ray [[buffer(1)]],\n" \
"                         texture2d<float, access::read> prev [[texture(0)]], texture2d<float, access::write> next [[texture(1)]],\n" \
"                         uint2 gid [[thread_position_in_grid]]) { gi_fold(u, ray, prev, next, gid, BRR_GI_DST_S, true); }\n"


#endif
