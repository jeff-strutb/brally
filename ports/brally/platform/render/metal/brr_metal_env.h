/* brr_metal_env.h: the sky's environment on the GPU (the skymap, light_sky.h).
 *
 * From the skymap's radiance (the sun painted out, physical units) the
 * renderer makes, once a map:
 *
 *   cube   the map as a cube, ENV_CUBE texels a face, mipmapped, shared-
 *          exponent float (RGB9E5: half RGBA16F's memory, 9-bit mantissas;
 *          RG11B10's 6 bits banded the sky's gradients): the sky as drawn
 *          behind the scene, and the source of the specular;
 *   spec   the specular environment: ENV_SPEC texels a face, ENV_SPEC_MIPS
 *          mips, mip m the radiance under a GGX lobe of perceptual
 *          roughness m / (ENV_SPEC_MIPS - 1) about the mirror direction (the
 *          split-sum's first sum, Karis 2013, normal = view), by importance
 *          sampling the lobe from the cube's mips (filtered importance
 *          sampling, Krivanek and Colbert 2008).
 *
 * The diffuse is the map's nine irradiance harmonics (light_sky.c), in LU.
 * The sun is the scene's directional light, its disc drawn on the sky at its
 * illuminance over the disc's solid angle.
 *
 * Shaders:
 *   BRR_ENV_SRC        the helpers (needs LU): env_irr (the harmonics),
 *                      env_spec (the specular cube by roughness), the cube's
 *                      face directions, the camera's ray per pixel
 *   BRR_ENV_PASS_SRC   the passes: the map into the cube, the prefilter, the sky
 *
 * Directions are world space, Z up; the map's bearing turned by LU.env.y. */
#ifndef BRR_METAL_ENV_H
#define BRR_METAL_ENV_H

#define ENV_CUBE 2048          /* the drawn sky at most: an 8K map holds 2048 texels a quarter turn (a 4K map 1024) */
#define ENV_SPEC 256           /* the mirror mip: about a third of a degree a texel */
#define ENV_SPEC_MIPS 5        /* 256 .. 16: a face of fewer texels cannot hold the lobe toward an arbitrary direction */

#define BRR_ENV_SRC \
/* the sky's irradiance on a face of normal n (lux): the harmonics, LU.sh */ \
"static float3 env_irr(constant LU &u, float3 n) {\n" \
"  float3 e = u.sh[0].rgb * 0.282095 + (u.sh[1].rgb * n.y + u.sh[2].rgb * n.z + u.sh[3].rgb * n.x) * 0.488603\n" \
"           + (u.sh[4].rgb * (n.x * n.y) + u.sh[5].rgb * (n.y * n.z) + u.sh[7].rgb * (n.x * n.z)) * 1.092548\n" \
"           + u.sh[6].rgb * (0.315392 * (3.0 * n.z * n.z - 1.0)) + u.sh[8].rgb * (0.546274 * (n.x * n.x - n.y * n.y));\n" \
"  return max(e, 0.0); }\n" \
/* the specular environment along r for perceptual roughness rough (cd/m2) */ \
"static float3 env_spec(constant LU &u, texturecube<float> sp, float3 r, float rough) {\n" \
"  constexpr sampler s(filter::linear, mip_filter::linear);\n" \
"  return sp.sample(s, r, level(saturate(rough) * u.env.x)).rgb; }\n" \
/* the direction through texel uv (0..1, v down) of cube face f (Metal's \
   faces, +X -X +Y -Y +Z -Z, as its sampling selects them) */ \
"static float3 env_facedir(uint f, float2 uv) {\n" \
"  float a = 2.0 * uv.x - 1.0, b = 2.0 * uv.y - 1.0; float3 d;\n" \
"  switch (f) { case 0: d = float3(1, -b, -a); break; case 1: d = float3(-1, -b, a); break;\n" \
"    case 2: d = float3(a, 1, b); break; case 3: d = float3(a, -1, -b); break;\n" \
"    case 4: d = float3(a, -b, 1); break; default: d = float3(-a, -b, -1); }\n" \
"  return normalize(d); }\n" \
/* the map's texel coordinates for direction d, its bearings turned by rot \
   (light_sky.h: texel (x, y) at elevation pi/2 - pi (y + 0.5) / h, bearing \
   2 pi (x + 0.5) / w + rot) */ \
"static float2 env_equv(float3 d, float rot) {\n" \
"  float el = asin(clamp(d.z, -1.0, 1.0)), az = atan2(d.y, d.x) - rot;\n" \
"  return float2(fract(az * 0.15915494), 0.5 - el * 0.31830989); }\n" \
/* the camera's ray for a pixel of the lit target (through the main 3D \
   view's screen map, the game's viewport and its view x projection's \
   inverse): EU, the sky's pass constants (brr_metal.m fills them) */ \
"struct EU { float4 xft; float4 vpt; float4 m0; float4 m1; float4 m2; float4 off; float4 eye; float4 tgt; float4 sun; float4 sunl; float4 wb; float4 misc; };\n" \
"static float3 env_camray(constant EU &k, float2 px) {\n" \
"  float2 ndc = float2((px.x + 0.5) / k.tgt.x * 2.0 - 1.0, 1.0 - (px.y + 0.5) / k.tgt.y * 2.0);\n" \
"  float2 g = float2((ndc.x - k.xft.y) / k.xft.x, (ndc.y - k.xft.w) / k.xft.z);\n" \
"  if (k.tgt.w > 0.0) g.y = k.tgt.w - g.y;   /* the game's rows run up (Glide's lower-left origin) */\n" \
"  float3 c = float3((g.x - k.vpt.y) / k.vpt.x - k.off.x, (g.y - k.vpt.w) / k.vpt.z - k.off.y, 1.0 - k.off.z);\n" \
"  float3 p = float3(dot(k.m0.xyz, c), dot(k.m1.xyz, c), dot(k.m2.xyz, c));\n" \
"  return normalize(p - k.eye.xyz); }\n"

#define BRR_ENV_STR_(x) #x
#define BRR_ENV_STR(x) BRR_ENV_STR_(x)
#define BRR_ENV_PASS_SRC \
"#define ENV_SPEC " BRR_ENV_STR(ENV_SPEC) "\n" \
/* the map into the cube: p.x the face, p.y the face's size, p.z the bearing's turn */ \
"fragment float4 env_eq2cube(LO in [[stage_in]], texture2d<float> eq [[texture(0)]], constant float4 &p [[buffer(0)]]) {\n" \
"  constexpr sampler s(filter::linear, s_address::repeat, t_address::clamp_to_edge);\n" \
"  float3 d = env_facedir(uint(p.x), in.pos.xy / p.y);\n" \
"  return float4(eq.sample(s, env_equv(d, p.z), level(0)).rgb, 1); }\n" \
/* Hammersley point i of n */ \
"static float2 env_ham(uint i, uint n) { return float2((float(i) + 0.5) / float(n), float(reverse_bits(i)) * 2.3283064365386963e-10); }\n" \
/* one mip of the specular cube: p.x the face, p.y this mip's size, p.z its \
   perceptual roughness, p.w the source cube's face size.  The lobe about \
   n = v = r; each sample read from the source's mip whose texel spans the \
   sample's share of the lobe's solid angle */ \
"fragment float4 env_prefilter(LO in [[stage_in]], texturecube<float> src [[texture(0)]], constant float4 &p [[buffer(0)]]) {\n" \
"  constexpr sampler s(filter::linear, mip_filter::linear);\n" \
"  float3 n = env_facedir(uint(p.x), in.pos.xy / p.y);\n" \
"  if (p.z <= 0.0) return float4(src.sample(s, n, level(log2(p.w / p.y))).rgb, 1);\n" \
"  float a = p.z * p.z, a2 = a * a, wp = 4.0 * 3.14159265 / (6.0 * p.w * p.w), q = float(ENV_SPEC) * 0.5 / p.y;\n" \
"  uint ns = uint(min(512.0 * q * q, 8192.0));   /* 512 samples at the first rough mip, four times as many a mip after (the wider lobe's variance), to 8192 */\n" \
"  float3 t = abs(n.z) < 0.999 ? normalize(cross(float3(0, 0, 1), n)) : float3(1, 0, 0), b = cross(n, t), acc = 0.0; float ws = 0.0;\n" \
"  for (uint i = 0; i < ns; i++) {\n" \
"    float2 x = env_ham(i, ns);\n" \
"    float ct = sqrt((1.0 - x.y) / (1.0 + (a2 - 1.0) * x.y)), st = sqrt(1.0 - ct * ct), ph = 6.2831853 * x.x;\n" \
"    float3 h = (cos(ph) * st) * t + (sin(ph) * st) * b + ct * n, l = 2.0 * dot(n, h) * h - n;\n" \
"    float nl = dot(n, l); if (nl <= 0.0) continue;\n" \
"    float dd = ct * ct * (a2 - 1.0) + 1.0, D = a2 / (3.14159265 * dd * dd), pdf = D * 0.25;\n" \
"    float lod = max(0.5 * log2(1.0 / (float(ns) * pdf * wp)), 0.0);\n" \
"    acc += src.sample(s, l, level(lod)).rgb * nl; ws += nl;\n" \
"  }\n" \
"  return float4(acc / max(ws, 1e-6), 1); }\n" \
/* the sky behind the scene: the camera's ray for the pixel (env_camray), \
   the cube along it, the sun's disc (its illuminance over its solid \
   angle), white-balanced and pre-exposed */ \
"fragment float4 env_sky(LO in [[stage_in]], constant EU &k [[buffer(0)]], texturecube<float> cube [[texture(0)]]) {\n" \
"  constexpr sampler s(filter::linear, mip_filter::linear);\n" \
"  float3 d = env_camray(k, in.pos.xy), L = cube.sample(s, d).rgb;\n" \
"  if (dot(d, k.sun.xyz) > k.sun.w) L += k.sunl.rgb;   /* sun.w: the cosine of the disc's radius */\n" \
"  if (int(k.misc.x) == 13) return float4(d * 0.5 + 0.5, 1);   /* BR_LIGHT_VIEW=13: the ray */\n" \
"  return float4(min(L * k.wb.rgb * k.tgt.z, float3(60000.0)), 1); }\n"

#endif
