/* brr_metal_rcube.h: the reflections' local cube (brr_metal.m lit_rcube):
 * each frame the track drawn around the eye into a cube map, 6 faces of
 * BRR_RCUBE_N texels, so a reflection that leaves the screen still finds the
 * bank, the wall or the hill beside the road (Forza Horizon's dynamic cube
 * for its cars' paint; no rays).
 *
 *   rc_skyvs, rc_skyfs   the sky's specular cube (pre-exposed, as the
 *                        lighting's) where the track is not
 *   rc_vs, rc_fs         the track's triangles (the shadows' static casters),
 *                        each face's projection taken from Metal's own cube
 *                        addressing (rc_clip: the face's major axis as w, its
 *                        two others as the texel's place), lit as the old
 *                        reflection hits were: the triangle's albedo learned
 *                        from the frames (brr_metal_talb.h; grey 0.3 until
 *                        seen) over pi times the sun (shadowed by the whole
 *                        track's shadow map where the sun's map is on,
 *                        brr_vsm.h vs_tmapvis) and the baked probes'
 *                        irradiance (brr_metal_gi.h; the sky's hemisphere
 *                        without them)
 *
 * Each texel's alpha its distance from the eye (60000 the sky's): a
 * reflection looks its ray up through it (rc_trace: Szirmay-Kalos et al.
 * 2005, distance impostors), so a surface near the road is found where the
 * ray meets it, not where the eye's direction would (the cube is centred
 * 1 m and more above the road).  The cars are not in it (the screen-space
 * march finds them on screen). */
#ifndef BRR_METAL_RCUBE_H
#define BRR_METAL_RCUBE_H

#include <stdint.h>

#define BRR_RCUBE_N 512
#define BRR_RCUBE_NEAR 0.1f
#define BRR_RCUBE_FAR 4000.0f

/* RCU: the eye; near, far, the grey for an unlearned triangle, the face's
 * texels; the face, the probes on */
typedef struct BrrRcubeU { float eye[4]; float nf[4]; uint32_t k[4]; } BrrRcubeU;

#define BRR_RCUBE_SRC \
"struct RCU { float4 eye; float4 nf; uint4 k; };\n" \
/* a direction from the eye to face f's clip space: Metal's cube addressing, face f's texel (u, v) in \
   [-1, 1] (u to the right, v down) holding the direction (+X) (1, -v, -u), (-X) (-1, -v, u), \
   (+Y) (u, 1, v), (-Y) (u, -1, -v), (+Z) (u, -v, 1), (-Z) (-u, -v, -1) */ \
"static float4 rc_clip(float3 d, uint f, float4 nf) {\n" \
"  float3 c = f == 0u ? float3(-d.z, d.y, d.x) : f == 1u ? float3(d.z, d.y, -d.x) : f == 2u ? float3(d.x, -d.z, d.y)\n" \
"           : f == 3u ? float3(d.x, d.z, -d.y) : f == 4u ? float3(d.x, d.y, d.z) : float3(-d.x, d.y, -d.z);\n" \
"  return float4(c.xy, (c.z - nf.x) * nf.y / (nf.y - nf.x), c.z); }\n" \
"static float3 rc_dir(float2 px, uint f, float N) {\n" \
"  float u = 2.0 * px.x / N - 1.0, v = 2.0 * px.y / N - 1.0;\n" \
"  return normalize(f == 0u ? float3(1, -v, -u) : f == 1u ? float3(-1, -v, u) : f == 2u ? float3(u, 1, v)\n" \
"                 : f == 3u ? float3(u, -1, -v) : f == 4u ? float3(u, -v, 1) : float3(-u, -v, -1)); }\n" \
"struct RSO { float4 pos [[position]]; };\n" \
"vertex RSO rc_skyvs(uint i [[vertex_id]]) { RSO o; float2 q = float2((i << 1) & 2, i & 2); o.pos = float4(q * 2.0 - 1.0, 1.0, 1.0); return o; }\n" \
"fragment float4 rc_skyfs(RSO in [[stage_in]], constant RCU &c [[buffer(1)]], constant LU &u [[buffer(2)]], texturecube<float> sp [[texture(2)]]) {\n" \
"  float3 d = rc_dir(in.pos.xy, c.k.x, c.nf.w);\n" \
"  return float4(min(env_spec(u, sp, d, 0.0) * u.wb.rgb * u.misc.x, float3(60000.0)), 60000.0); }\n" \
"struct RCO { float4 pos [[position]]; float3 wp; uint t [[flat]]; };\n" \
"vertex RCO rc_vs(uint v [[vertex_id]], const device packed_float3 *p [[buffer(0)]], constant RCU &c [[buffer(1)]]) {\n" \
"  RCO o; o.wp = float3(p[v]); o.t = v / 3u; o.pos = rc_clip(o.wp - c.eye.xyz, c.k.x, c.nf); return o; }\n" \
"fragment float4 rc_fs(RCO in [[stage_in]], const device packed_float3 *p [[buffer(0)]], constant RCU &c [[buffer(1)]], constant LU &u [[buffer(2)]],\n" \
"                      const device uint4 *prim [[buffer(3)]], constant VSU &vsu [[buffer(4)]], constant GiU &gu [[buffer(5)]],\n" \
"                      device const float4 *goff [[buffer(6)]], device const float4 *gsh [[buffer(7)]],\n" \
"                      texture2d<float> gdst [[texture(0)]], depth2d<float> tmap [[texture(1)]]) {\n" \
"  uint t = in.t; float3 a0 = float3(p[3u * t]), a1 = float3(p[3u * t + 1u]), a2 = float3(p[3u * t + 2u]), n = cross(a1 - a0, a2 - a0);\n" \
"  float l = length(n); float3 v = normalize(c.eye.xyz - in.wp); n = l > 0.0 ? n / l : v; if (dot(n, v) < 0.0) n = -n;\n" \
"  uint ac = prim[t].z; float3 alb = ac != 0u ? float3(ac & 255u, (ac >> 8) & 255u, (ac >> 16) & 255u) / 255.0 : float3(c.nf.z);\n" \
"  float nl = dot(n, u.sun.xyz), vis = nl > 0.0 ? 1.0 : 0.0;\n" \
"  if (nl > 0.0 && vsu.on.x > 0.5) { float nf = clamp(dot(n, -vsu.f.xyz), 0.0, 1.0); vis = vs_tmapvis(vsu, u.sbias, tmap, in.wp, n, sqrt(1.0 - nf * nf)); }\n" \
"  float3 E = u.sunc.rgb * max(nl, 0.0) * vis + (c.k.y != 0u ? gi_irr(gu, goff, gsh, gdst, in.wp, n, v) : u.skyc.rgb * (0.5 + 0.5 * n.z));\n" \
"  return float4(min(alb / 3.14159 * E * u.wb.rgb * u.misc.x, float3(60000.0)), distance(in.wp, c.eye.xyz)); }\n"

#endif
