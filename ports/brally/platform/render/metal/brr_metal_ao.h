/* brr_metal_ao.h: ground-truth ambient occlusion (GTAO, Jimenez et al. 2016,
 * "Practical Real-Time Strategies for Accurate Indirect Occlusion"; the slice
 * integral as Intel's XeGTAO writes it): how much of the sky a surface sees,
 * for the sky's diffuse and specular (brr_metal_lit.h radiance).
 *
 *   gtao    half resolution, from the G-buffer's world positions and normals:
 *           AO_SLICES slices a pixel about the view ray, turned per pixel and
 *           per frame (interleaved gradient noise; TAA averages the turns),
 *           each searched AO_STEPS texels a side out to AO_RADIUS metres
 *           (at most AO_MAXPX pixels), samples spaced by the square of
 *           their index; a side's horizon the highest sample's, faded to the
 *           tangent plane over the radius' last AO_FALLOFF; the slice's
 *           visibility the cosine-weighted arc between its horizons about
 *           the normal projected into it, weighted by that projection's length
 *   gtaof   the same size: a 3 x 3 filter weighted by how far each
 *           neighbour's surface stands off the pixel's plane
 *   ao_at   the lighting's read at full resolution: the four nearest half
 *           texels, bilinear, each by its surface's distance from the pixel's
 *
 * The camera's rays per pixel come from the sky's pass constants (EU,
 * brr_metal_env.h): the slice's plane holds the pixel's ray and its
 * neighbour's along the slice. */
#ifndef BRR_METAL_AO_H
#define BRR_METAL_AO_H

#define AO_RADIUS 1.5f     /* metres: kerbs, walls' feet, a car's underside and wheel arches */
#define AO_MAXPX 96.0f     /* the search's reach on screen at most, half-resolution pixels */
#define AO_SLICES 2
#define AO_STEPS 4
#define AO_FALLOFF 0.4f

#define BRR_AO_STR_(x) #x
#define BRR_AO_STR(x) BRR_AO_STR_(x)
#define BRR_AO_SRC \
"#define AO_RADIUS " BRR_AO_STR(AO_RADIUS) "\n" \
"#define AO_MAXPX " BRR_AO_STR(AO_MAXPX) "\n" \
"#define AO_SLICES " BRR_AO_STR(AO_SLICES) "\n" \
"#define AO_STEPS " BRR_AO_STR(AO_STEPS) "\n" \
"#define AO_FALLOFF " BRR_AO_STR(AO_FALLOFF) "\n" \
/* a full-resolution pixel's world position; none where no 3D (0) */ \
"static bool ao_pos(texture2d<float> pos, int2 p, thread float3 &q) {\n" \
"  if (any(p < 0) || p.x >= int(pos.get_width()) || p.y >= int(pos.get_height())) return false;\n" \
"  q = pos.read(uint2(p)).xyz; return any(q != 0.0); }\n" \
"kernel void gtao(uint2 hp [[thread_position_in_grid]], constant EU &k [[buffer(0)]], constant float4 &fr [[buffer(1)]],\n" \
"                 texture2d<float> pos [[texture(0)]], texture2d<float> nrm [[texture(1)]], texture2d<float, access::write> ao [[texture(2)]]) {\n" \
"  if (hp.x >= ao.get_width() || hp.y >= ao.get_height()) return;\n" \
"  int2 px = int2(hp) * 2; float3 P;\n" \
"  if (!ao_pos(pos, px, P)) { ao.write(1.0, hp); return; }\n" \
"  float3 V = normalize(k.eye.xyz - P), n = nrm.read(uint2(px)).xyz; if (dot(n, V) < 0.0) n = -n;\n" \
"  float3 r0 = env_camray(k, float2(px));\n" \
/* the radius on screen: the ray's spread over one pixel at the point's distance */ \
"  float dist = length(k.eye.xyz - P), pxm = length(env_camray(k, float2(px) + float2(1, 0)) - r0) * dist;\n" \
"  float rpx = min(AO_RADIUS / max(pxm, 1e-6) * 0.5, AO_MAXPX);   /* half-resolution pixels */\n" \
"  if (rpx < 1.0) { ao.write(1.0, hp); return; }\n" \
"  float noise = fract(52.9829189 * fract(dot(float2(hp) + fr.xy, float2(0.06711056, 0.00583715))));\n" \
"  float noise2 = fract(noise * 1.6180339 + 0.5), vis = 0.0, wsum = 0.0;\n" \
"  for (int s = 0; s < AO_SLICES; s++) {\n" \
"    float phi = (float(s) + noise) * 3.14159265 / float(AO_SLICES); float2 d2 = float2(cos(phi), sin(phi));\n" \
"    float3 r1 = env_camray(k, float2(px) + d2 * 2.0), ax = cross(r0, r1);\n" \
"    if (dot(ax, ax) < 1e-14) continue;\n" \
"    ax = normalize(ax); float3 dt = normalize(cross(V, ax)); if (dot(dt, r1 - r0) < 0.0) dt = -dt;\n" \
"    float3 np = n - ax * dot(n, ax); float nl = length(np); if (nl < 1e-4) continue;\n" \
"    float cn = clamp(dot(np / nl, V), -1.0, 1.0), ng = sign(dot(np, dt)) * acos(cn);\n" \
"    float lo0 = cos(ng - 1.5707963), lo1 = cos(ng + 1.5707963), h0 = lo0, h1 = lo1;   /* side -dt, side +dt: from the tangent plane */\n" \
"    for (int side = 0; side < 2; side++) {\n" \
"      float sg = side ? 1.0 : -1.0, lo = side ? lo1 : lo0, hc = lo;\n" \
"      for (int i = 0; i < AO_STEPS; i++) {\n" \
"        float t = (float(i) + noise2) / float(AO_STEPS); t = t * t * rpx + 1.0;\n" \
"        float3 Q; if (!ao_pos(pos, px + int2(round(d2 * sg * t * 2.0)), Q)) continue;\n" \
"        float3 w = Q - P; float l2 = dot(w, w); if (l2 < 1e-8) continue;\n" \
"        float c = dot(w, V) * rsqrt(l2), f = saturate((AO_RADIUS * AO_RADIUS - l2) / (AO_RADIUS * AO_RADIUS * AO_FALLOFF));\n" \
"        hc = max(hc, mix(lo, c, f));\n" \
"      }\n" \
"      if (side) h1 = hc; else h0 = hc;\n" \
"    }\n" \
"    float a0 = -acos(clamp(h0, -1.0, 1.0)), a1 = acos(clamp(h1, -1.0, 1.0));\n" \
"    a0 = ng + max(a0 - ng, -1.5707963); a1 = ng + min(a1 - ng, 1.5707963);\n" \
"    float sn = sin(ng);\n" \
"    float arc = 0.25 * (-cos(2.0 * a0 - ng) + cn + 2.0 * a0 * sn) + 0.25 * (-cos(2.0 * a1 - ng) + cn + 2.0 * a1 * sn);\n" \
"    vis += nl * arc; wsum += nl;\n" \
"  }\n" \
"  ao.write(wsum > 0.0 ? saturate(vis / wsum) : 1.0, hp); }\n" \
/* the 3 x 3 filter, by distance from the pixel's plane */ \
"kernel void gtaof(uint2 hp [[thread_position_in_grid]], texture2d<float> pos [[texture(0)]], texture2d<float> nrm [[texture(1)]],\n" \
"                  texture2d<float> src [[texture(2)]], texture2d<float, access::write> dst [[texture(3)]]) {\n" \
"  if (hp.x >= dst.get_width() || hp.y >= dst.get_height()) return;\n" \
"  float3 P; if (!ao_pos(pos, int2(hp) * 2, P)) { dst.write(1.0, hp); return; }\n" \
"  float3 n = nrm.read(hp * 2).xyz; float acc = 0.0, ws = 0.0;\n" \
"  for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {\n" \
"    int2 q = int2(hp) + int2(dx, dy); float3 Q;\n" \
"    if (any(q < 0) || q.x >= int(src.get_width()) || q.y >= int(src.get_height()) || !ao_pos(pos, q * 2, Q)) continue;\n" \
"    float w = (dx == 0 && dy == 0 ? 2.0 : 1.0) * exp(-abs(dot(Q - P, n)) / (0.05 + 0.01 * length(Q - P)));\n" \
"    acc += src.read(uint2(q)).r * w; ws += w;\n" \
"  }\n" \
"  dst.write(ws > 0.0 ? acc / ws : 1.0, hp); }\n" \
/* the occlusion at full-resolution pixel px, surface P, normal n */ \
"static float ao_at(texture2d<float> ao, texture2d<float> pos, uint2 px, float3 P, float3 n) {\n" \
"  float2 h = (float2(px) + 0.5) * 0.5 - 0.5; int2 b = int2(floor(h)); float2 f = h - float2(b); float acc = 0.0, ws = 0.0;\n" \
"  for (int j = 0; j < 4; j++) {\n" \
"    int2 q = b + int2(j & 1, j >> 1); float3 Q;\n" \
"    if (any(q < 0) || q.x >= int(ao.get_width()) || q.y >= int(ao.get_height()) || !ao_pos(pos, q * 2, Q)) continue;\n" \
"    float w = ((j & 1) ? f.x : 1.0 - f.x) * ((j >> 1) ? f.y : 1.0 - f.y) * exp(-abs(dot(Q - P, n)) / (0.05 + 0.01 * length(Q - P))) + 1e-4;\n" \
"    acc += ao.read(uint2(q)).r * w; ws += w;\n" \
"  }\n" \
"  return ws > 0.0 ? acc / ws : 1.0; }\n" \
/* the occlusion's light bounced inside what occludes (Jimenez 2016's fit, \
   per channel of the albedo), and the specular's occlusion from it \
   (Lagarde and de Rousiers 2014) */ \
"static float3 ao_multi(float ao, float3 alb) {\n" \
"  float3 a = 2.0404 * alb - 0.3324, b = -4.7951 * alb + 0.6417, c = 2.7552 * alb + 0.6903;\n" \
"  return max(float3(ao), ((ao * a + b) * ao + c) * ao); }\n" \
"static float ao_spec(float ao, float nv, float rough) { return saturate(pow(nv + ao, exp2(-16.0 * rough - 1.0)) - 1.0 + ao); }\n"

#endif
