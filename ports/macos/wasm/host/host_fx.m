/* host_fx.m -- experimental modern lighting over the native renderer (port code).
 *
 * The game still draws every triangle exactly as before into the colour
 * target.  Alongside it, host_glide.m writes a G-buffer from the native 3D
 * leaves: the world-space position of every pixel (native/render.m rebuilds it
 * from the clip-space corners through the inverse of the game's view x
 * projection matrix) and the face normal.  At the swap this module relights the
 * finished picture:
 *
 *   sun shadows    the frame's opaque 3D triangles re-drawn from the sun into a
 *                  2048^2 shadow map (alpha-tested foliage included), 16-tap PCF
 *   AO + GI        world-space hemisphere samples tested against the G-buffer
 *                  (occlusion, and one bounce of the occluders' colour),
 *                  half resolution, bilateral blur
 *   reflections    screen-space ray march, Fresnel weighted, wet roads in rain
 *   sun specular   GGX, sharper when wet
 *   light shafts   radial scattering from the sky around the sun
 *   bloom          six-level down/up chain
 *   tonemap        ACES, exposure and grade per weather
 *
 * The weather the game is running (g_brCarPhysWeather 0x104B15E8: 0 sunny,
 * 1 fog, 2 storm, 3 snow, 4 rain) picks the light rig, and the storm's
 * lightning state (0x100A79CC) flashes the scene.  2D (menus, HUD) is left as
 * the game drew it.
 *
 * ~ (the key left of 1) switches live between Remastered (all of this) and
 * Original (the frame exactly as the game drew it), like Tab's PC / N64
 * switch; the game never sees the key, and the choice is remembered.
 * BR_FX=1 forces it on, BR_FX=0 off; default on in a window, off headless.
 */
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#include "host.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

CAMetalLayer *happ_metal_layer(void);
id<MTLTexture> hsky_tex(id<MTLDevice> dev, int weather);   /* host_sky.m */
static id<MTLTexture> g_skyt;   /* this frame's Remastered sky, or nil */

#define STR(...) #__VA_ARGS__
static const char *FXSRC = "#include <metal_stdlib>\n" STR(
using namespace metal;
struct CV { float x, y, z, w, r, g, b, a, s, t, wx, wy, wz, fl; };
struct FXU {
  float4x4 vp, ivp, svp;
  float4 eye, sun, sunc, skyc, grnd, fogc, vpt, scr, p0, p1, p2, p3, p4, flash, wb, nsky, sk2, tm, wx;
  float4x4 pvp; float4 jit; float4x4 svp2; float4 csm;
};
/* headlights: up to 32 spot lights (two per car); misc = count, beam
   strength, light intensity, air density */
struct HL { float4 p[32]; float4 d[32]; float4 col; float4 misc;
  float4 g[64]; float4 gd[64]; float4 gmisc; };   /* g: lamp position, w 0 head / 1 tail; gd: facing */
/* how much of headlight i reaches point x, and the direction to it */
float spotw(constant HL &hl, int i, float3 x, thread float3 &Ld) {
  float3 Lv = hl.p[i].xyz - x; float d = length(Lv); Ld = Lv / max(d, 1e-3);
  float cd = dot(-Ld, hl.d[i].xyz);
  float spot = smoothstep(0.86, 0.975, cd) + 0.25 * smoothstep(0.6, 0.9, cd);
  return spot / (1.0 + d * d * 0.012) * smoothstep(90.0, 35.0, d); }
struct SHU { int at_fn, at_ref, use_tex, pad; float su, sv, pad2, pad3; float4x4 svp; };
struct QO { float4 pos [[position]]; float2 uv; };
vertex QO fsq(uint vid [[vertex_id]]) {
  float2 p = float2((vid << 1) & 2, vid & 2); QO o;
  o.pos = float4(p * 2.0 - 1.0, 0, 1); o.uv = float2(p.x, 1.0 - p.y); return o; }
constexpr sampler ns(filter::nearest, address::clamp_to_edge);
constexpr sampler ls(filter::linear, address::clamp_to_edge);
float hash2(float2 p) { return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
float vnoise(float2 p) { float2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash2(i), hash2(i + float2(1, 0)), f.x), mix(hash2(i + float2(0, 1)), hash2(i + float2(1, 1)), f.x), f.y); }
float fbm(float2 p) { return vnoise(p) * 0.55 + vnoise(p * 2.03 + 17.1) * 0.3 + vnoise(p * 4.1 + 5.3) * 0.15; }
float ign(float2 p) { return fract(52.9829189 * fract(dot(p, float2(0.06711056, 0.00583715)))); }
float2 to_uv(constant FXU &u, float3 wp, thread float &cw) {
  float4 c = u.vp * float4(wp, 1); cw = c.w;
  float sx = u.vpt.x * c.x / c.w + u.vpt.y, sy = u.vpt.z * c.y / c.w + u.vpt.w;
  float v = sy / 480.0; if (u.p3.x > 0.5) v = 1.0 - v;
  return float2(sx / 640.0, v); }
float3 view_dir(constant FXU &u, float2 uv) {
  float sx = uv.x * 640.0, sy = (u.p3.x > 0.5 ? 1.0 - uv.y : uv.y) * 480.0;
  float4 h = u.ivp * float4((sx - u.vpt.y) / u.vpt.x, (sy - u.vpt.w) / u.vpt.z, 0.5, 1.0);
  float3 d = (h.xyz - u.eye.xyz * h.w) * (h.w < 0 ? -1.0 : 1.0);
  return length(d) > 1e-12 ? normalize(d) : float3(0, 0, 1); }
bool is_geo(float w) { return w > 0.5 && w < 2.5; }
/* anything solid: lit geometry or the pre-lit Remastered car (class 4) */
bool is_solid(float w) { return (w > 0.5 && w < 2.5) || (w > 3.5 && w < 4.5); }
bool cmpf(int f, float a, float b) {
  switch (f) { case 0: return false; case 1: return a < b; case 2: return a == b;
  case 3: return a <= b; case 4: return a > b; case 5: return a != b; case 6: return a >= b;
  default: return true; } }
float3 onormal(float3 n, float3 wp, float3 eye) {
  float l = length(n); n = l > 1e-6 ? n / l : float3(0, 0, 1); if (dot(n, eye - wp) < 0) n = -n; return n; }

/* the air's colour looking along `vd`: the sky (or, at night and in fog, the
   game's own fog colour), brightened toward the sun by forward scattering */
float3 air(constant FXU &u, float3 vd) {
  float mu = saturate(dot(vd, u.sun.xyz));
  float3 base = mix(u.skyc.rgb * u.skyc.w, u.fogc.rgb, u.grnd.w);
  return base + u.sunc.rgb * (pow(mu, 8.0) * 0.55 + pow(mu, 64.0) * 0.6) * u.sun.w; }

/* a clear daytime sky: deep blue overhead, pale toward the horizon, the
   sun's glow and its disc */
float3 atmos(constant FXU &u, float3 vd) {
  float t = saturate(vd.z);
  float3 c = mix(float3(0.62, 0.74, 0.94), float3(0.10, 0.26, 0.68), pow(t, 0.55));
  if (vd.z < 0.0) c = float3(0.62, 0.74, 0.94) * 0.95;
  float mu = saturate(dot(vd, u.sun.xyz));
  c += u.sunc.rgb * (pow(mu, 8.0) * 0.22 + pow(mu, 64.0) * 0.55);
  c += u.sunc.rgb * 22.0 * smoothstep(0.99962, 0.99990, mu);
  return c; }
/* the sea's surface normal at world xy: two layers of moving waves, their
   height flattened with distance so far water does not shimmer */
float3 waveN(float2 xy, float t, float amp) {
  float e = 0.12;
  float2 w1 = float2(t * 0.06, t * 0.04), w2 = float2(-t * 0.1, t * 0.07);
  float h0 = fbm(xy * 0.35 + w1) + 0.45 * fbm(xy * 1.3 + w2);
  float hx = fbm((xy + float2(e, 0)) * 0.35 + w1) + 0.45 * fbm((xy + float2(e, 0)) * 1.3 + w2);
  float hy = fbm((xy + float2(0, e)) * 0.35 + w1) + 0.45 * fbm((xy + float2(0, e)) * 1.3 + w2);
  return normalize(float3(-(hx - h0) / e * amp, -(hy - h0) / e * amp, 1.0)); }
/* blue-green, which only the sea is in these tracks */
float seamask(float3 sb) {
  /* green over red with blue at least level with red, and not bright: the
     open sea and the greyer shallows alike, but not grass, walls or snow */
  float mx = max(sb.r, max(sb.g, sb.b));
  return smoothstep(1.04, 1.09, sb.g / max(sb.r, 0.02)) * smoothstep(0.96, 1.0, sb.b / max(sb.r, 0.02))
       * (1.0 - smoothstep(0.66, 0.76, mx)); }
/* ---- volumetric clouds: a cumulus layer 1.2-2.6 km up, ray-marched, lit by
   the sun through the cloud (Beer-Lambert with a powder term and forward
   scattering) and by the sky from above */
float hash3(float3 p) { return fract(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float vnoise3(float3 p) {
  float3 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
  float a = mix(mix(hash3(i), hash3(i + float3(1, 0, 0)), f.x), mix(hash3(i + float3(0, 1, 0)), hash3(i + float3(1, 1, 0)), f.x), f.y);
  float b = mix(mix(hash3(i + float3(0, 0, 1)), hash3(i + float3(1, 0, 1)), f.x), mix(hash3(i + float3(0, 1, 1)), hash3(i + float3(1, 1, 1)), f.x), f.y);
  return mix(a, b, f.z); }
float cloud_d(float3 p, float t) {
  float h = (p.z - 1200.0) / 1400.0;
  if (h <= 0.0 || h >= 1.0) return 0.0;
  float3 q = p * 0.00055 + float3(t * 0.006, t * 0.002, 0.0);
  float n = vnoise3(q) * 0.55 + vnoise3(q * 2.3 + 7.1) * 0.3 + vnoise3(q * 5.1 + 3.7) * 0.15;
  float prof = smoothstep(0.0, 0.18, h) * smoothstep(1.0, 0.45, h);
  return saturate((n - 0.5) * 3.2) * prof; }
float4 clouds(constant FXU &u, float3 vd, float2 pos) {
  if (vd.z < 0.015) return float4(0, 0, 0, 1);
  float3 ro = u.eye.xyz;
  float t0 = (1200.0 - ro.z) / vd.z, t1 = (2600.0 - ro.z) / vd.z;
  if (t0 > 30000.0) return float4(0, 0, 0, 1);
  t1 = min(t1, t0 + 6000.0);
  const int NS = 18;
  float dt = (t1 - t0) / float(NS), t = t0 + dt * ign(pos), T = 1.0;
  float3 L = 0, sd = u.sun.xyz;
  float mu = dot(vd, sd), g = 0.6;
  float phase = (1.0 - g * g) / (4.0 * 3.14159 * pow(1.0 + g * g - 2.0 * g * mu, 1.5)) * 6.0 + 0.35;
  for (int i = 0; i < NS && T > 0.03; i++, t += dt) {
    float3 p = ro + vd * t;
    float d = cloud_d(p, u.tm.x);
    if (d <= 0.001) continue;
    float od = 0.0;
    for (int j = 1; j <= 3; j++) od += cloud_d(p + sd * (140.0 * float(j)), u.tm.x) * 140.0;
    float sunT = exp(-od * 0.012), powder = 1.0 - exp(-d * 4.0);
    float3 lum = u.sunc.rgb * sunT * phase * powder * 1.6 + mix(float3(0.55, 0.62, 0.75), float3(0.95, 0.97, 1.0), saturate((p.z - 1200.0) / 1400.0)) * 0.6;
    float a = exp(-d * dt * 0.012);
    L += T * (1.0 - a) * lum;
    T *= a; }
  /* fade into the haze toward the horizon */
  float f = smoothstep(0.015, 0.09, vd.z);
  return float4(L * f, mix(1.0, T, f)); }
/* ---- rain on standing water: rings spreading from where drops land; the
   gradient they add to a flat surface's normal */
float2 ripples(float2 xy, float t, float amt) {
  float2 g = 0;
  for (int l = 0; l < 2; l++) {
    float2 p = xy * (l == 0 ? 3.0 : 4.7) + float(l) * 17.0;
    float2 c = floor(p), f = fract(p) - 0.5;
    float h = hash2(c), ph = fract(t * (1.1 + 0.4 * float(l)) + h * 5.0);
    float2 o = float2(hash2(c + 3.1), hash2(c + 7.7)) * 0.5 - 0.25;
    float2 dv = f - o; float r = length(dv), R = ph * 0.45;
    float ring = exp(-pow((r - R) / 0.035, 2.0)) * (1.0 - ph) * step(h, amt);
    g += dv / max(r, 1e-3) * ring * cos((r - R) * 70.0); }
  return g; }

/* the colour a reflected ray sees when it leaves the screen */

/* ---- shadow map: the frame's opaque triangles from the sun */
struct SO { float4 pos [[position]]; float2 uv; };
vertex SO shvs(uint vid [[vertex_id]], const device CV *v [[buffer(0)]], constant SHU &s [[buffer(1)]]) {
  CV g = v[vid]; SO o;
  o.pos = g.fl == 1.0 ? s.svp * float4(g.wx, g.wy, g.wz, 1) : float4(0, 0, 2, 1);
  o.uv = float2(g.s * s.su, g.t * s.sv); return o; }
fragment void shfs(SO in [[stage_in]], constant SHU &s [[buffer(0)]],
                   texture2d<float> t [[texture(0)]], sampler smp [[sampler(0)]]) {
  if (s.use_tex && s.at_fn != 7) {
    float a = floor(t.sample(smp, in.uv).a * 255.0);
    if (!cmpf(s.at_fn, a, float(s.at_ref))) discard_fragment(); } }

/* ---- ambient occlusion + one bounce (half res): rgb bounce, a visibility */
fragment float4 aofs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                     texture2d<float> gn [[texture(0)]], texture2d<float> gp [[texture(1)]],
                     texture2d<float> col [[texture(2)]]) {
  float4 P = gp.sample(ns, in.uv);
  if (!is_geo(P.w)) return float4(0, 0, 0, 1);
  float3 wp = P.xyz, N = onormal(gn.sample(ns, in.uv).xyz, wp, u.eye.xyz);
  float R = u.p2.z * (1.0 + distance(u.eye.xyz, wp) * 0.004);
  float3 T = normalize(abs(N.z) < 0.9 ? cross(N, float3(0, 0, 1)) : cross(N, float3(1, 0, 0)));
  float3 B = cross(N, T);
  float rot = (ign(in.pos.xy) + u.p2.w * 0.618) * 6.2831853;
  float occ = 0; float3 gi = 0; const int NS = 10;
  float de = distance(u.eye.xyz, wp);
  for (int i = 0; i < NS; i++) {
    float fi = (float(i) + 0.5) / float(NS);
    float phi = rot + float(i) * 2.3999632, r = sqrt(fi);
    float3 dir = T * (cos(phi) * r) + B * (sin(phi) * r) + N * sqrt(max(0.0, 1.0 - fi));
    float len = R * (0.15 + 0.85 * fract(fi * 5.13 + rot * 0.159));
    float3 s = wp + N * (0.03 * R) + dir * len;
    float cw; float2 suv = to_uv(u, s, cw);
    if (cw <= 0 || any(suv < 0.0) || any(suv > 1.0)) continue;
    float4 Q = gp.sample(ns, suv);
    if (!is_geo(Q.w)) continue;
    float ds = distance(u.eye.xyz, s), dq = distance(u.eye.xyz, Q.xyz);
    if (dq < ds - 0.02 * R) {
      float3 dd = Q.xyz - wp; float L = length(dd);
      float fall = saturate(1.0 - L / (R * 2.5)) * saturate((de - dq) / R + 1.0);
      occ += fall;
      float3 c = pow(col.sample(ns, suv).rgb, 2.2);
      gi += c * saturate(dot(N, dd / max(L, 1e-4))) * fall; } }
  return float4(gi * (2.0 / float(NS)), 1.0 - occ / float(NS)); }
fragment float4 blurfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], constant float2 &dir [[buffer(1)]],
                       texture2d<float> src [[texture(0)]], texture2d<float> gp [[texture(1)]],
                       texture2d<float> gn [[texture(2)]]) {
  float4 P = gp.sample(ns, in.uv); float3 N = onormal(gn.sample(ns, in.uv).xyz, P.xyz, u.eye.xyz);
  if (!is_geo(P.w)) return src.sample(ns, in.uv);
  float2 px = dir / float2(src.get_width(), src.get_height());
  float de = distance(u.eye.xyz, P.xyz);
  float4 acc = 0; float wsum = 0;
  for (int i = -4; i <= 4; i++) {
    float2 q = in.uv + px * float(i) * 1.5;
    float4 Q = gp.sample(ns, q);
    if (!is_geo(Q.w)) continue;
    float w = exp(-float(i * i) / 12.0);
    w *= saturate(1.0 - abs(distance(u.eye.xyz, Q.xyz) - de) / (0.03 * de + 0.5));
    w *= pow(saturate(dot(N, onormal(gn.sample(ns, q).xyz, Q.xyz, u.eye.xyz)) * 0.5 + 0.5), 8.0);
    acc += src.sample(ns, q) * w; wsum += w; }
  return wsum > 0 ? acc / wsum : src.sample(ns, in.uv); }

/* ---- screen-space reflections (half res): rgb colour, a confidence */
fragment float4 ssrfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                      texture2d<float> gn [[texture(0)]], texture2d<float> gp [[texture(1)]],
                      texture2d<float> col [[texture(2)]]) {
  float4 P = gp.sample(ns, in.uv);
  if (!is_geo(P.w) || u.p0.z <= 0) return 0;
  float3 wp = P.xyz, N = onormal(gn.sample(ns, in.uv).xyz, wp, u.eye.xyz);
  float3 Vd = normalize(wp - u.eye.xyz), R = reflect(Vd, N);
  float de = distance(u.eye.xyz, wp);
  float step = max(0.6, de * 0.015) * (0.75 + 0.5 * ign(in.pos.xy + u.p2.w));
  float3 pos = wp + N * (0.01 * de);
  for (int i = 0; i < 48; i++) {
    pos += R * step; step *= 1.07;
    float cw; float2 q = to_uv(u, pos, cw);
    if (cw <= 0 || any(q < 0.0) || any(q > 1.0)) break;
    float4 Q = gp.sample(ns, q);
    float dr = distance(u.eye.xyz, pos);
    if (Q.w > 2.5 && Q.w < 3.5) continue;
    if (!is_geo(Q.w)) break;
    float dq = distance(u.eye.xyz, Q.xyz);
    if (dr > dq && dr - dq < step * 3.0) {
      float3 lo = pos - R * step, hi = pos;
      for (int k = 0; k < 5; k++) {
        float3 m = (lo + hi) * 0.5; float c2; float2 mq = to_uv(u, m, c2);
        float4 M = gp.sample(ns, mq);
        if (distance(u.eye.xyz, m) > distance(u.eye.xyz, M.xyz)) hi = m; else lo = m; }
      q = to_uv(u, hi, cw);
      float2 e = smoothstep(0.0, 0.08, q) * smoothstep(0.0, 0.08, 1.0 - q);
      return float4(pow(col.sample(ls, q).rgb, 2.2), e.x * e.y * saturate(1.0 - float(i) / 48.0)); } }
  /* missed: the sky where the ray leaves, when that pixel is sky */
  float cw; float2 q = to_uv(u, u.eye.xyz + R * 4000.0, cw);
  if (R.z > 0 && cw > 0 && all(q >= 0.0) && all(q <= 1.0) && abs(gp.sample(ns, q).w - 3.0) < 0.5)
    return float4(pow(col.sample(ls, q).rgb, 2.2), 0.8 * u.p0.w);
  /* off screen: the game's own fog colour stands for the sky (black at night) */
  return float4(u.fogc.rgb, R.z > 0 ? 0.5 * u.p0.w : 0.0); }

/* ---- lighting composite (full res, HDR): a = how much of the pixel is scene */
float shadow1(depth2d<float> sm, float4x4 m, float3 wp, float bias, float soft, float2 px, thread float &edge) {
  constexpr sampler cs(filter::linear, address::clamp_to_border, border_color::opaque_white, compare_func::less_equal);
  float4 c = m * float4(wp, 1);
  float2 st = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);
  edge = max(abs(c.x), abs(c.y));
  if (any(st < 0.0) || any(st > 1.0) || c.z > 1.0) { edge = 2.0; return 1.0; }
  float rot = ign(px) * 6.2831853, s = 0;
  for (int i = 0; i < 12; i++) {
    float r = sqrt((float(i) + 0.5) / 12.0), a = rot + float(i) * 2.3999632;
    s += sm.sample_compare(cs, st + float2(cos(a), sin(a)) * r * soft, c.z - bias); }
  return s / 12.0; }
/* two cascades: a sharp one around the car, a wide one to the distance */
float shadow_at(constant FXU &u, depth2d<float> sm, float3 wp, float3 N, float2 px, depth2d<float> sm2) {
  float e1, e2;
  float s1 = shadow1(sm, u.svp, wp + N * u.p3.w * 1.2, 0.00012, u.p2.y / 2048.0, px, e1);
  if (e1 < 0.85) return s1;
  float s2 = shadow1(sm2, u.svp2, wp + N * u.csm.x * 1.2, 0.00025, u.p2.y * 0.6 / 2048.0, px, e2);
  s2 = mix(s2, 1.0, smoothstep(0.85, 1.0, e2));
  return mix(s1, s2, smoothstep(0.85, 1.0, e1)); }
/* the Remastered sky (host_sky.m): each panorama is a level photograph, so it
   is laid out with a photograph's proportions: horizon on its bottom row,
   35 degrees up on its top row, 60 degrees of heading across, repeated six
   times around the eye; above 35 degrees, the top row's average colour.
   The chase camera sees about 39 x 30 degrees centred on the horizon, so the
   lower half of the picture is what is on screen. */
float3 pano(constant FXU &u, texture2d<float> sky, float3 vd) {
  constexpr sampler ws(filter::linear, mip_filter::linear, s_address::repeat, t_address::clamp_to_edge);
  float e = asin(clamp(vd.z, -1.0, 1.0)) / 0.6109;
  float2 st = float2(atan2(vd.y, vd.x) / 1.0472, 1.0 - saturate(e));
  float3 c = sky.sample(ws, st, level(0.0)).rgb;
  float3 z = sky.sample(ws, float2(st.x, 0.0), level(7.0)).rgb;
  return mix(c, z, smoothstep(0.85, 1.25, e)) * u.tm.z; }
/* the colour a reflected ray sees when it leaves the screen: the track's sky
   panorama when one is loaded, else the drawn sky (clear) or the air */
float3 skylight(constant FXU &u, texture2d<float> sky, float3 d) {
  if (u.tm.z > 0.0) return pano(u, sky, d);
  return u.sk2.x > 0.5 ? atmos(u, d) : air(u, d); }
fragment float4 compfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                       texture2d<float> gn [[texture(0)]], texture2d<float> gp [[texture(1)]],
                       texture2d<float> col [[texture(2)]], depth2d<float> sm [[texture(3)]],
                       texture2d<float> aot [[texture(4)]], texture2d<float> ssr [[texture(5)]],
                       texture2d<float> sky [[texture(6)]], texture2d<float> trk [[texture(7)]],
                       depth2d<float> sm2 [[texture(8)]],
                       constant HL &hl [[buffer(1)]]) {
  float4 C = col.sample(ns, in.uv), P = gp.sample(ns, in.uv), G = gn.sample(ns, in.uv);
  float3 lin = pow(C.rgb, 2.2);
  if (P.w > 3.5 && P.w < 4.5) {                      /* pre-lit surface (host_car.m): colour holds
                                                        (radiance / 4)^(1/2.2); only the sun's
                                                        shadow is applied here */
    float3 wp = P.xyz, N = normalize(G.xyz);
    float sh = u.sun.w > 0 ? shadow_at(u, sm, wp, N, in.pos.xy, sm2) : 1.0;
    float k = saturate(dot(N, u.sun.xyz) * 4.0) * (1.0 - sh);
    float3 o = lin * 4.0 * (1.0 - 0.6 * k);
    /* rain on the car: drops landing on its upper surfaces, a bright pop that
       fades, and beads standing on the paint catching the sky */
    if (u.wx.x > 0.0) {
      float3 q = wp * 26.0, cc = floor(q), f = fract(q) - 0.5;
      float h = hash3(cc), ph = fract(u.tm.x * 1.6 + h * 9.0), top = saturate(N.z * 1.8 - 0.2);
      float3 R = reflect(-normalize(u.eye.xyz - wp), N);
      float pop = step(h, 0.22 * u.wx.x) * smoothstep(0.42, 0.08, length(f)) * pow(1.0 - ph, 5.0) * top;
      float bead = step(hash3(cc + 11.0), 0.3 * u.wx.x) * smoothstep(0.3, 0.12, length(f)) * top;
      o += air(u, R) * (pop * 1.4 + bead * 0.35); }
    if (any(isnan(o)) || any(isinf(o))) o = lin;
    return float4(o, G.a); }
  if (P.w > 2.5 && int(u.p3.y) == 11) return float4(0, 0, 1, G.a);
  if (P.w > 2.5) {                                   /* sky */
    float3 vd = view_dir(u, in.uv); float g = saturate(dot(vd, u.sun.xyz));
    float3 o = lin * u.p4.x;
    /* the Remastered sky for this track and weather, where it is loaded, in
       place of the game's painted one, everywhere above the horizon: the
       scenery the game paints into its 64x64 backdrop is too coarse to keep
       against it (the real distant geometry is not sky and is untouched).
       The weather's own touches below (the night floor, fog, the sun's glow,
       lightning) apply over it. */
    if (u.tm.z > 0.0 && G.a > 0.01) o = mix(o, pano(u, sky, vd), smoothstep(-0.004, 0.004, vd.z));
    /* night: the sky is never black; a deep blue, lighter toward the horizon */
    o = max(o, u.nsky.rgb * (0.45 + 0.55 * pow(1.0 - saturate(vd.z), 3.0)));
    /* a luminous horizon and the sun's glow, as the air scatters it */
    /* gentle: translucent distant scenery the game draws over the sky
       shares these pixels */
    o = mix(o, air(u, vd), max(pow(1.0 - saturate(vd.z), 5.0) * 0.12, u.wb.w));
    o += u.sunc.rgb * (pow(g, 900.0) * 6.0 + pow(g, 12.0) * 0.12) * u.p4.y;
    /* clear weather: a real sky in place of the game's painted one, keeping
       its clouds (the whiter texels) and anything the game drew over it */
    if (u.sk2.x > 0.0 && G.a > 0.01 && u.tm.z <= 0.0) {
      float3 own = saturate(G.rgb / G.a);
      float mn = min(own.r, min(own.g, own.b)), cl = dot(own, float3(0.2126, 0.7152, 0.0722));
      /* the sky and its clouds are drawn, not the game's painted texture */
      float4 cv = clouds(u, vd, in.pos.xy);
      float3 ns = atmos(u, vd) * cv.a + cv.rgb;
      /* what the game drew over its sky: scenery near the horizon stays; higher
         up it is the old cloud layer, which the new clouds replace */
      ns += (lin - pow(own, 2.2)) * (1.0 - smoothstep(0.015, 0.04, vd.z));
      /* above the horizon only, fading in over the first few degrees so the
         game's distant scenery painted into its backdrop stays */
      /* sky is the blue texels and the clouds (bright, grey); the hills and
         scenery painted along the horizon are neither, and stay */
      float mx = max(own.r, max(own.g, own.b));
      float blue = smoothstep(1.15, 1.32, own.b / max(own.r, 0.02));
      float grey = smoothstep(0.34, 0.42, mn) * (1.0 - smoothstep(0.1, 0.2, (mx - mn) / max(mx, 0.05)));
      float skyish = max(blue, grey);
      o = mix(o, ns, u.sk2.x * smoothstep(-0.005, 0.02, vd.z) * max(skyish, smoothstep(0.03, 0.08, vd.z))); }
    /* the sea painted into the backdrop, below the horizon: water on a plane
       under the eye, reflecting the sky, with the sun's glitter */
    if (vd.z < 0.0 && u.sk2.w > 0.0 && G.a > 0.01) {
      /* the final colour: in some weathers the sea is a see-through layer
         over the backdrop, which the G-buffer's sky colour does not hold */
      float m = seamask(saturate(C.rgb)) * u.sk2.w;
      if (m > 0.0) {
        /* a sea plane fixed in the world (z = 0), not under the camera, so the
           waves stay put as the camera rises and falls */
        float t = min(max(u.eye.z, 1.0) / max(-vd.z, 1e-3), 3000.0);
        float2 xy = u.eye.xy + vd.xy * t;
        float3 Nw = waveN(xy * 0.5, u.tm.x, 0.22 * smoothstep(900.0, 40.0, t) + 0.03);
        float3 R = reflect(vd, Nw);
        float3 refl = skylight(u, sky, float3(R.xy, abs(R.z)));
        float Fw = 0.02 + 0.98 * pow(1.0 - saturate(dot(Nw, -vd)), 5.0);
        float3 Hw = normalize(u.sun.xyz - vd); float nhw = saturate(dot(Nw, Hw));
        float aw = 0.1 * 0.1 * 0.1 * 0.1, dw = nhw * nhw * (aw - 1.0) + 1.0;
        float glint = aw / (3.14159 * dw * dw) * 0.012 * saturate(dot(Nw, u.sun.xyz)) * u.sun.w;
        float3 ow = mix(lin * 0.7, refl, saturate(Fw * 1.1 + 0.1)) + u.sunc.rgb * glint;
        o = mix(o, ow, m); } }
    o += u.flash.rgb * u.flash.w * 0.35;
    if (any(isnan(o)) || any(isinf(o))) o = lin;
    return float4(o, G.a); }
  if (!is_geo(P.w)) return float4(lin, 0);
  float fogk = saturate((P.w - 1.0) / 0.9);
  /* the 1999 textures carry colour noise that relighting would amplify:
     keep every pixel's brightness but take its colour from a 3-pixel
     neighbourhood (a chroma-only filter; edges shift by under 2 pixels) */
  { float2 px = 1.5 / float2(col.get_width(), col.get_height());
    float3 cb = pow(col.sample(ls, in.uv + float2(px.x, px.y)).rgb, 2.2) + pow(col.sample(ls, in.uv - float2(px.x, px.y)).rgb, 2.2)
              + pow(col.sample(ls, in.uv + float2(px.x, -px.y)).rgb, 2.2) + pow(col.sample(ls, in.uv - float2(px.x, -px.y)).rgb, 2.2);
    cb *= 0.25;
    const float3 lw = float3(0.2126, 0.7152, 0.0722);
    float yl = dot(lin, lw), yb = dot(cb, lw);
    if (yb > 1e-4) lin = cb * (yl / yb); }
  float3 wp = P.xyz, N = onormal(G.xyz, wp, u.eye.xyz), V = normalize(u.eye.xyz - wp), L = u.sun.xyz;
  /* surface detail: the texture's own light and dark read as relief (a bump
     map from its brightness, by the surface gradient of Mikkelsen 2020),
     fading out where the texture is too far away to show it */
  { float2 px = 2.0 / float2(col.get_width(), col.get_height());
    const float3 lw = float3(0.2126, 0.7152, 0.0722);
    float h0 = dot(col.sample(ls, in.uv).rgb, lw);
    float hx = dot(col.sample(ls, in.uv + float2(px.x, 0)).rgb, lw) - h0;
    float hy = dot(col.sample(ls, in.uv + float2(0, px.y)).rgb, lw) - h0;
    float3 sx = gp.sample(ns, in.uv + float2(px.x, 0)).xyz - wp, sy = gp.sample(ns, in.uv + float2(0, px.y)).xyz - wp;
    float3 r1 = cross(sy, N), r2 = cross(N, sx);
    float det = dot(sx, r1);
    float k = u.tm.w * smoothstep(60.0, 8.0, distance(u.eye.xyz, wp));
    if (abs(det) > 1e-9 && k > 0.0) {
      float3 grad = sign(det) * (hx * r1 + hy * r2);
      N = normalize(abs(det) * N - grad * k); } }
  float ndl = saturate(dot(N, L)), up = saturate(N.z);
  float sh = u.sun.w > 0 ? shadow_at(u, sm, wp, N, in.pos.xy, sm2) : 1.0;
  float4 A = aot.sample(ls, in.uv);
  float ao = mix(1.0, A.a, u.p0.x);
  float wet = u.p0.w * smoothstep(0.55, 0.9, up);
  /* standing water: puddles in the dips of a world-space noise */
  /* more water, more and larger puddles */
  float puddle = wet * smoothstep(0.64 - 0.14 * u.p0.w, 0.74 - 0.1 * u.p0.w, fbm(wp.xy * 0.18));
  /* tyre tracks: pressed snow, darkened dirt, and on a wet road the lines
     the tyres squeezed dry */
  float tk = 0;
  { float4 TK = trk.sample(ls, in.uv);
    if (TK.r > 0.0 && abs(TK.g - wp.z) < 0.45 && N.z > 0.7) tk = saturate(TK.r); }
  wet *= 1.0 - 0.6 * tk * (1.0 - u.wx.z); puddle *= 1.0 - tk;
  /* rain landing on standing water */
  if (u.wx.x > 0.0 && wet > 0.0) {
    float2 rg = ripples(wp.xy, u.tm.x, 0.55 * u.wx.x) * (0.35 + 0.65 * puddle / max(wet, 1e-3));
    N = normalize(N + float3(rg * 0.28 * wet, 0.0)); }
  float3 alb = lin * mix(1.0, 0.6, wet) * mix(1.0, 0.7, puddle);
  alb *= mix(float3(1.0), mix(float3(0.82), float3(0.7, 0.74, 0.82), u.wx.z), tk);
  /* asphalt: the old road textures carry a purple cast; flat, unsaturated
     surfaces are pulled toward a neutral, very slightly cool grey */
  { float3 sb = saturate(C.rgb); float mx = max(sb.r, max(sb.g, sb.b)), mn = min(sb.r, min(sb.g, sb.b));
    float grey = 1.0 - smoothstep(0.08, 0.22, (mx - mn) / max(mx, 0.05));
    float road = smoothstep(0.8, 0.95, N.z) * grey * u.tm.y;
    float y = dot(alb, float3(0.2126, 0.7152, 0.0722));
    alb = mix(alb, y * float3(0.97, 1.0, 1.04), road); }
  /* asphalt, resynthesised: the game's road texture is baked streaks and
     colour speckle.  On flat grey surfaces (judged on a wide blur, which the
     speckle averages out of) keep only that blur's tone and the painted
     markings (texels well above it), and lay a world-space asphalt over it:
     patches, fine grain and aggregate */
  { const float3 lw = float3(0.2126, 0.7152, 0.0722);
    float2 px = 7.0 / float2(col.get_width(), col.get_height());
    float3 cbl = 0;
    for (int k = 0; k < 8; k++) { float a = float(k) * 0.785398; cbl += col.sample(ls, in.uv + float2(cos(a), sin(a)) * px).rgb; }
    cbl /= 8.0;
    float mx = max(cbl.r, max(cbl.g, cbl.b)), mn = min(cbl.r, min(cbl.g, cbl.b));
    float grey = 1.0 - smoothstep(0.1, 0.2, (mx - mn) / max(mx, 0.05));
    float road = smoothstep(0.85, 0.96, N.z) * grey * (1.0 - u.wx.z) * step(0.5, u.tm.y);
    if (road > 0.0) {
      float yb = dot(pow(cbl, 2.2), lw);
      float y = dot(lin, lw), dist = distance(u.eye.xyz, wp);
      float mark = smoothstep(1.4, 1.9, y / max(yb, 1e-3));
      float f1 = smoothstep(80.0, 20.0, dist), f2 = smoothstep(30.0, 6.0, dist);
      float n = 1.0 + (fbm(wp.xy * 0.35) - 0.5) * 0.35
              + (fbm(wp.xy * 6.0) - 0.5) * 0.3 * f1
              + (step(0.8, vnoise(wp.xy * 55.0)) * 0.3 - vnoise(wp.xy * 23.0) * 0.12) * f2;
      float3 asph = yb * n * float3(0.98, 1.0, 1.03) * mix(1.0, 0.6, wet) * mix(1.0, 0.7, puddle);
      alb = mix(alb, asph, road * (1.0 - mark)); }
    if (int(u.p3.y) == 13) return float4(road, grey, smoothstep(0.85, 0.96, N.z), G.a); }
  /* close up, the 1999 textures run out of detail: add the material's own
     fine structure, in world space -- aggregate in asphalt, blades in grass,
     grain in sand, stone on walls -- as colour variation and relief */
  { float dist = distance(u.eye.xyz, wp), k = smoothstep(26.0, 3.0, dist) * u.wx.w;
    if (k > 0.0) {
      float3 sb = saturate(C.rgb); float mx = max(sb.r, max(sb.g, sb.b)), mn = min(sb.r, min(sb.g, sb.b));
      float sat = (mx - mn) / max(mx, 0.05);
      float green = smoothstep(0.02, 0.08, sb.g - max(sb.r, sb.b));
      float sand = smoothstep(0.04, 0.14, sb.r - sb.b) * (1.0 - green);
      float grey = 1.0 - smoothstep(0.06, 0.18, sat);
      float3 an = abs(N);
      float2 q = an.z > 0.6 ? wp.xy : (an.x > an.y ? wp.yz : wp.xz);
      float f = green > 0.5 ? 60.0 : sand > 0.5 ? 45.0 : 30.0;
      float e = 0.35 / f;
      float h0 = fbm(q * f), hx = fbm((q + float2(e, 0)) * f), hy = fbm((q + float2(0, e)) * f);
      float spk = step(0.78, vnoise(q * 140.0)) * grey;              /* aggregate stones */
      float blades = green * (vnoise(float2(q.x * 180.0, q.y * 22.0)) * 0.5 + vnoise(float2(q.x * 25.0, q.y * 170.0)) * 0.5);
      float amp = grey * 0.22 + green * 0.3 + sand * 0.16 + 0.08;
      alb *= 1.0 + k * ((h0 - 0.5) * amp * 1.6 + spk * 0.25 - blades * 0.25);
      float3 g3 = float3(hx - h0, hy - h0, 0.0) * (0.9 * k);
      float3 t1 = an.z > 0.6 ? float3(1, 0, 0) : (an.x > an.y ? float3(0, 1, 0) : float3(1, 0, 0));
      float3 t2 = an.z > 0.6 ? float3(0, 1, 0) : float3(0, 0, 1);
      N = normalize(N - (t1 * g3.x + t2 * g3.y) * 6.0);
      ndl = saturate(dot(N, L)); } }
  float3 amb = mix(u.grnd.rgb, u.skyc.rgb, N.z * 0.5 + 0.5) * ao;
  float3 dl = u.sunc.rgb * ndl * sh;
  float3 fl = u.flash.rgb * u.flash.w * saturate(dot(N, normalize(float3(0.3, 0.2, 1.0))) * 0.7 + 0.3);
  /* bounce light carries the occluders' brightness, not their texture noise */
  float bounce = dot(A.rgb, float3(0.2126, 0.7152, 0.0722));
  float3 o = alb * ((dl + amb + fl) * u.p4.w + bounce * u.p0.y * ao);
  /* sun specular, GGX */
  float3 H = normalize(L + V); float nh = saturate(dot(N, H)), vh = saturate(dot(V, H));
  float rough = mix(u.p3.z, mix(0.3, 0.08, puddle), wet), a2 = rough * rough * rough * rough;
  float d = nh * nh * (a2 - 1.0) + 1.0, D = a2 / (3.14159 * d * d);
  float F = 0.04 + 0.96 * pow(1.0 - vh, 5.0);
  /* dry asphalt is matte: highlights come only from water on it */
  o += u.sunc.rgb * D * F * ndl * sh * 0.25 * wet * ao;
  /* headlights: each car's two beams light what is in front of it, with a
     glint where the road is wet */
  { float3 hd = 0, hs = 0; int n = int(hl.misc.x);
    for (int i = 0; i < n; i++) {
      float3 Ld; float w = spotw(hl, i, wp + N * 0.05, Ld);
      if (w <= 0.0) continue;
      float nl = saturate(dot(N, Ld));
      float3 H2 = normalize(Ld + V); float n2 = saturate(dot(N, H2));
      float d2 = n2 * n2 * (a2 - 1.0) + 1.0;
      hd += w * nl;
      hs += w * nl * (a2 / (3.14159 * d2 * d2)) * (0.04 + 0.96 * pow(1.0 - saturate(dot(V, H2)), 5.0)); }
    o += hl.col.rgb * hl.misc.z * (alb * hd * ao + hs * 0.25 * wet); }
  /* reflections */
  float4 S = ssr.sample(ls, in.uv);
  /* open water: flat, and blue-green where everything else is not.  Moving
     waves, the sky and the scene mirrored with Fresnel, the sun's glitter */
  { float3 sb = saturate(C.rgb);
    /* modelled water must be clearly blue-green: snowy and wet roads are
       greenish-grey too */
    /* modelled water: clearly blue-green, or sea-coloured and below the road
       (wet and snowy roads are greenish-grey too, but sit higher) */
    float water = u.sk2.w * step(0.97, N.z) * max(smoothstep(1.12, 1.24, sb.b / max(sb.r, 0.02))
                * smoothstep(1.08, 1.18, sb.g / max(sb.r, 0.02)), smoothstep(1.04, 1.1, sb.g / max(sb.r, 0.02)) * (1.0 - smoothstep(0.2, 0.45, wp.z)));
    if (water > 0.0) {
      float3 Nw = waveN(wp.xy, u.tm.x, 0.3 * smoothstep(250.0, 15.0, distance(u.eye.xyz, wp)) + 0.05);
      float3 R = reflect(-V, Nw);
      float3 refl = mix(skylight(u, sky, float3(R.xy, abs(R.z))), S.rgb, S.a * 0.8);
      float Fw = 0.02 + 0.98 * pow(1.0 - saturate(dot(Nw, V)), 5.0);
      float3 Hw = normalize(L + V); float nhw = saturate(dot(Nw, Hw));
      float aw = 0.1 * 0.1 * 0.1 * 0.1, dw = nhw * nhw * (aw - 1.0) + 1.0;
      float glint = aw / (3.14159 * dw * dw) * 0.012 * saturate(dot(Nw, L)) * sh;
      float3 body = lin * 0.5 * (amb + dl * 0.4);
      float3 ow = mix(body, refl, saturate(Fw * 1.2 + 0.08)) + u.sunc.rgb * glint;
      o = mix(o, ow, water); }
    if (int(u.p3.y) == 11) return float4(water, is_geo(P.w) ? 0.5 : 0.0, 0, G.a); }
  float Fe = 0.04 + 0.96 * pow(1.0 - saturate(dot(N, V)), 5.0);
  /* and so are reflections: none dry, a damp sheen, mirrors in the puddles */
  float rw = saturate(u.p0.z * wet * mix(0.12 + 0.3 * Fe, 0.45 + 0.55 * Fe, puddle / max(wet, 1e-3)) * S.a) * ao;
  o = mix(o, S.rgb * u.p4.w * 1.1, rw);
  /* aerial perspective: distant surfaces pick up the sky's colour */
  { float dd = distance(u.eye.xyz, wp), hz = (1.0 - exp(-dd * u.fogc.w)) * u.nsky.w;
    o = mix(o, air(u, -V), hz); }
  /* the game's fog already sits in the colour: fade the relighting with it */
  o = mix(o, lin * u.p4.z, fogk);
  int dbg = int(u.p3.y);
  if (dbg == 1) o = N * 0.5 + 0.5;
  else if (dbg == 2) o = float3(sh);
  else if (dbg == 3) o = float3(A.a);
  else if (dbg == 4) o = fract(wp / 50.0);
  else if (dbg == 5) o = A.rgb;
  else if (dbg == 6) o = S.rgb * S.a;
  else if (dbg == 12) o = float3(wp.z < 0.0, wp.z < 0.6, wp.z < 1.2);
  else if (dbg == 7 || dbg == 8) {
    float4 c = u.svp * float4(wp, 1); float2 st = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);
    o = dbg == 7 ? float3(sm.sample(ns, st), fract(st * 8.0)) : float3(c.z, fract(st * 8.0)); }
  if (any(isnan(o)) || any(isinf(o))) o = lin;
  return float4(o, G.a); }

/* ---- bloom */
fragment float4 prefs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], texture2d<float> h [[texture(0)]]) {
  float2 px = 1.0 / float2(h.get_width(), h.get_height());
  float4 c = (h.sample(ls, in.uv + px * float2(-1, -1)) + h.sample(ls, in.uv + px * float2(1, -1)) +
              h.sample(ls, in.uv + px * float2(-1, 1)) + h.sample(ls, in.uv + px * float2(1, 1))) * 0.25;
  float l = dot(c.rgb, float3(0.2126, 0.7152, 0.0722));
  float k = max(l - u.scr.z, 0.0); k = k * k / (k + 0.4);
  return float4(c.rgb * (k / max(l, 1e-4)) * c.a, 1); }
fragment float4 downfs(QO in [[stage_in]], texture2d<float> h [[texture(0)]]) {
  float2 px = 1.0 / float2(h.get_width(), h.get_height());
  float3 c = h.sample(ls, in.uv).rgb * 4.0;
  c += h.sample(ls, in.uv + px * float2(-1, -1)).rgb + h.sample(ls, in.uv + px * float2(1, -1)).rgb;
  c += h.sample(ls, in.uv + px * float2(-1, 1)).rgb + h.sample(ls, in.uv + px * float2(1, 1)).rgb;
  return float4(c / 8.0, 1); }
fragment float4 upfs(QO in [[stage_in]], texture2d<float> h [[texture(0)]]) {
  float2 px = 1.0 / float2(h.get_width(), h.get_height());
  float3 c = h.sample(ls, in.uv).rgb * 4.0;
  c += (h.sample(ls, in.uv + float2(px.x, 0)).rgb + h.sample(ls, in.uv - float2(px.x, 0)).rgb +
        h.sample(ls, in.uv + float2(0, px.y)).rgb + h.sample(ls, in.uv - float2(0, px.y)).rgb) * 2.0;
  c += h.sample(ls, in.uv + px).rgb + h.sample(ls, in.uv - px).rgb +
       h.sample(ls, in.uv + float2(px.x, -px.y)).rgb + h.sample(ls, in.uv - float2(px.x, -px.y)).rgb;
  return float4(c / 16.0, 1); }

/* ---- light shafts (half res): radial scattering of the sky around the sun */
/* headlight beams: light the air scatters along the view ray, up to the
   surface the ray meets */
float3 beams(constant FXU &u, constant HL &hl, texture2d<float> gp, float2 uv, float2 pos) {
  int n = int(hl.misc.x);
  if (n == 0 || hl.misc.y <= 0.0) return 0;
  float4 P = gp.sample(ns, uv);
  float3 vd = view_dir(u, uv);
  float tmax = is_solid(P.w) ? distance(u.eye.xyz, P.xyz) : 90.0;
  tmax = min(tmax, 90.0);
  const int NS = 20;
  float dt = tmax / float(NS), t = dt * ign(pos), acc = 0;
  for (int k = 0; k < NS; k++, t += dt) {
    float3 x = u.eye.xyz + vd * t, Ld;
    for (int i = 0; i < n; i++) {
      float w = spotw(hl, i, x, Ld);
      /* forward scattering: brighter looking into a beam */
      acc += w * (0.35 + 0.65 * pow(saturate(dot(-vd, Ld) * 0.5 + 0.5), 4.0)); } }
  return hl.col.rgb * acc * dt * hl.misc.y * hl.misc.w; }
/* the lamps themselves: a glow where each lamp faces the eye and nothing
   stands in front of it (white heads, red tails), with a soft halo */
float3 lamps(constant FXU &u, constant HL &hl, texture2d<float> gp, float2 uv) {
  int n = int(hl.gmisc.x);
  float3 acc = 0;
  float2 scr = float2(gp.get_width(), gp.get_height());
  for (int i = 0; i < n; i++) {
    float3 lp = hl.g[i].xyz, toe = u.eye.xyz - lp;
    float d = length(toe), face = saturate(dot(hl.gd[i].xyz, toe / d));
    if (face <= 0.0) continue;
    float cw; float2 lu = to_uv(u, lp, cw);
    if (cw <= 0) continue;
    float4 Q = gp.sample(ns, lu);
    if (is_solid(Q.w) && distance(u.eye.xyz, Q.xyz) < d - 0.35) continue;   /* hidden */
    float px = length((uv - lu) * scr), s = clamp(40.0 / d, 0.8, 8.0);
    float core = exp(-px * px / (s * s)), halo = 1.0 / (1.0 + pow(px / (s * 2.5), 2.0)) * smoothstep(s * 10.0, s * 3.0, px);
    float3 c = hl.g[i].w > 0.5 ? float3(1.0, 0.06, 0.03) * 0.6 : float3(1.0, 0.95, 0.85);
    acc += c * (core * 6.0 + halo * 0.08) * pow(face, 2.0); }
  return acc * hl.gmisc.y; }
fragment float4 shaftfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                        texture2d<float> gp [[texture(0)]], texture2d<float> col [[texture(1)]],
                        constant HL &hl [[buffer(1)]]) {
  float3 bm = beams(u, hl, gp, in.uv, in.pos.xy) + lamps(u, hl, gp, in.uv);
  float cw; float2 sp = to_uv(u, u.eye.xyz + u.sun.xyz * 8000.0, cw);
  if (cw <= 0 || u.p2.x <= 0) return float4(bm, 1);
  float2 d = (in.uv - sp) / 32.0 * 0.9; float2 q = in.uv; float dec = 1.0, acc = 0;
  float j = ign(in.pos.xy);
  q -= d * j;
  for (int i = 0; i < 32; i++) {
    q -= d;
    if (all(q >= 0.0) && all(q <= 1.0) && abs(gp.sample(ns, q).w - 3.0) < 0.5) {
      float l = dot(pow(col.sample(ls, q).rgb, 2.2), float3(0.3, 0.5, 0.2));
      acc += l * l * dec * 2.0; }
    dec *= 0.94; }
  float g = saturate(1.0 - length((in.uv - sp) * float2(1.333, 1.0)) * 0.9);
  return float4(u.sunc.rgb * acc / 32.0 * (0.4 + g) * u.p2.x + bm, 1); }

/* ---- smooth shading: the game's models are low-poly and its G-buffer
   normals are per face.  Average each pixel's normal with its neighbours on
   the same continuous surface (close to its plane, bent by under ~50 deg),
   across a disc a fixed size in the world, as a modeller's smoothing groups
   would; creases sharper than that stay sharp */
fragment float4 nrmfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                      texture2d<float> gn [[texture(0)]], texture2d<float> gp [[texture(1)]]) {
  float4 G = gn.sample(ns, in.uv), P = gp.sample(ns, in.uv);
  if (!is_geo(P.w)) return G;
  float3 N0 = onormal(G.xyz, P.xyz, u.eye.xyz);
  float de = distance(u.eye.xyz, P.xyz);
  /* a 0.9 m disc, in pixels at this distance */
  float2 scr = float2(gn.get_width(), gn.get_height());
  float rpx = clamp(0.9 * u.scr.w / max(de, 0.5), 2.0, 60.0);
  float3 acc = N0; float wsum = 1.0;
  float rot = ign(in.pos.xy) * 6.2831853;
  for (int i = 0; i < 12; i++) {
    float r = sqrt((float(i) + 0.5) / 12.0) * rpx, a = rot + float(i) * 2.3999632;
    float2 q = in.uv + float2(cos(a), sin(a)) * r / scr;
    float4 Q = gp.sample(ns, q);
    if (!is_geo(Q.w)) continue;
    float3 Nk = onormal(gn.sample(ns, q).xyz, Q.xyz, u.eye.xyz);
    float plane = abs(dot(N0, Q.xyz - P.xyz)) / (0.02 * de + 0.05);
    float w = saturate(1.0 - plane) * smoothstep(0.62, 0.9, dot(N0, Nk));
    acc += Nk * w; wsum += w; }
  return float4(normalize(acc), G.a); }

/* ---- tyre tracks: ribbons along each wheel's path, drawn into a mask the
   lighting pass reads (r strength, g the ground's height, to reject pixels
   the ribbon passes in front of) */
struct TV { float4 p; float4 a; };                 /* world xyz + strength; across */
struct TO { float4 pos [[position]]; float k; float z; float a; };
vertex TO trkvs(uint vid [[vertex_id]], const device TV *v [[buffer(0)]], constant FXU &u [[buffer(1)]]) {
  TV g = v[vid]; TO o;
  float4 c = u.vp * float4(g.p.xyz, 1);
  float sx = u.vpt.x * c.x + u.vpt.y * c.w, sy = u.vpt.z * c.y + u.vpt.w * c.w;
  o.pos = float4(sx / 320.0 - c.w, u.p3.x > 0.5 ? sy / 240.0 - c.w : c.w - sy / 240.0, 0.5 * c.w, c.w);
  o.k = g.p.w; o.z = g.p.z; o.a = g.a.x; return o; }
fragment float4 trkfs(TO in [[stage_in]]) {
  float e = 1.0 - in.a * in.a;
  /* the tread: fine bands across the ribbon */
  float tread = 0.8 + 0.2 * step(0.5, fract(in.a * 3.0 + 0.5));
  return float4(in.k * e * tread, in.z, 0, 1); }

/* ---- volumetric particles (smoke, dust, snow and water spray) and falling
   rain and snow, ray-marched per pixel at half resolution; premultiplied */
struct PF { float4 p[160]; float4 c[160]; float4 misc; float4 amb; };
fragment float4 pfxfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], constant HL &hl [[buffer(1)]],
                      const device PF &pf [[buffer(2)]], texture2d<float> gp [[texture(0)]]) {
  float3 vd = view_dir(u, in.uv), eye = u.eye.xyz;
  float4 P = gp.sample(ns, in.uv);
  float dmax = is_solid(P.w) ? distance(eye, P.xyz) : 1e5;
  float T = 1.0; float3 Lc = 0;
  float t = u.tm.x, j0 = ign(in.pos.xy);
  int n = int(pf.misc.x);
  float mu = dot(vd, u.sun.xyz);
  float phase = 0.55 + 1.1 * pow(saturate(mu), 6.0);
  for (int i = 0; i < n && T > 0.02; i++) {
    float3 c = pf.p[i].xyz; float r = pf.p[i].w;
    float3 oc = c - eye; float b = dot(oc, vd), dd = dot(oc, oc) - b * b;
    if (dd >= r * r) continue;
    float h = sqrt(r * r - dd), t0 = max(b - h, 0.05), t1 = min(b + h, dmax);
    if (t1 <= t0) continue;
    float od = 0;
    for (int k = 0; k < 3; k++) {
      float3 x = eye + vd * mix(t0, t1, (float(k) + j0) / 3.0);
      float q = length(x - c) / r;
      float3 np = x * 1.6 + float3(float(i) * 7.31, 0, -t * 0.7);
      float dens = saturate(1.0 - q * q) * (0.35 + 1.1 * vnoise3(np) * vnoise3(np * 2.3 + 5.0));
      od += dens; }
    od *= (t1 - t0) / 3.0 * pf.c[i].w;
    float a = 1.0 - exp(-od);
    /* sun (forward-scattering), sky and ground, the headlights */
    float3 hd = 0, Ld;
    for (int m = 0; m < int(hl.misc.x); m++) hd += spotw(hl, m, c, Ld);
    float3 lum = pf.c[i].rgb * (u.sunc.rgb * phase * 0.8 + pf.amb.rgb + hl.col.rgb * hl.misc.z * hd * 1.5);
    Lc += T * a * lum; T *= 1.0 - a; }
  /* falling rain: thin streaks on columns fixed in the world, in four
     layers of distance; lit by the air's light and the headlights */
  if (u.wx.x > 0.0 || u.wx.y > 0.0) {
    float2 fw = normalize(vd.xy + float2(1e-5, 0)), side = float2(-fw.y, fw.x);
    for (int l = 0; l < 4; l++) {
      float d = 1.6 * exp2(float(l)) * (0.85 + 0.3 * j0);
      if (d >= dmax) break;
      float3 p = eye + vd * d;
      bool snow = u.wx.y > 0.0;
      float g = snow ? 0.5 : 0.28;
      float2 cell = floor(p.xy / g);
      float cover = 0;
      for (int k = 0; k < 2; k++) {
        float2 ck = cell + float2(k * 41, l * 17);
        float h = hash2(ck), h2 = hash2(ck + 9.1);
        float2 col = (cell + float2(h, h2)) * g;
        if (snow) col += float2(sin(t * 0.9 + h * 30.0), cos(t * 0.7 + h2 * 30.0)) * 0.15;
        float2 dv = p.xy - col;
        float along = dot(dv, fw), lat = abs(dot(dv, side));
        if (abs(along) > g * 0.5) continue;
        float w = 0.0018 * d + 0.0015;
        if (snow) {
          /* a flake every S metres down the column, each one only sometimes
             there and each at its own sideways offset */
          float S = 1.4, zz = (p.z + t * 1.1) / S + h * 3.0, rep = floor(zz), z = fract(zz) - 0.5;
          float hr = hash2(ck + rep * 1.37);
          if (hr > 0.45) continue;
          float lat2 = abs(lat - (hr - 0.22) * g * 0.8);
          float rr = length(float2(lat2, z * S)) / (w * 1.6);
          cover += smoothstep(1.0, 0.35, rr) * 0.85;
        } else {
          /* a drop every S metres down the column, each one only sometimes
             there and each at its own sideways offset: short streaks, no dashes */
          float S = 2.6, len = 0.42, zz = (p.z + t * 9.5) / S + h * 5.0, rep = floor(zz);
          float hr = hash2(ck + rep * 1.37);
          if (hr > 0.35) continue;
          float lat2 = abs(lat - (hr - 0.17) * g * 1.2);
          float z = fract(zz) * S;
          cover += smoothstep(w, w * 0.2, lat2) * smoothstep(0.0, 0.1, z) * smoothstep(len, len * 0.5, z) * 0.4;
        } }
      cover = saturate(cover) * (snow ? u.wx.y : u.wx.x) / (1.0 + float(l) * 0.35);
      if (cover <= 0.0) continue;
      float3 hd = 0, Ld;
      for (int m = 0; m < int(hl.misc.x); m++) hd += spotw(hl, m, p, Ld);
      float3 lum = (snow ? float3(0.95) : float3(0.7, 0.75, 0.8)) * (pf.amb.rgb * 1.3 + u.sunc.rgb * 0.3)
                 + hl.col.rgb * hl.misc.z * hd * 2.5;
      Lc += T * cover * lum; T *= 1.0 - cover; } }
  return float4(Lc, 1.0 - T); }

/* ---- temporal anti-aliasing and motion blur.  Every 3D triangle is drawn
   with a sub-pixel jitter (host_glide.m), so over frames each pixel sees its
   whole area; each pixel's position last frame comes from its world position
   through last frame's camera, or through its car's last transform when it
   is on a car. */
struct MVC { float4 cur[16][4]; float4 prev[16][4]; float4 misc; };   /* rows 0-2 + position; box in misc */
float2 prev_uv(constant FXU &u, constant MVC &mv, float4 P, float2 uv) {
  float3 wp;
  if (is_solid(P.w)) {
    wp = P.xyz;
    for (int i = 0; i < int(mv.misc.x); i++) {
      float3 d = wp - mv.cur[i][3].xyz;
      float3 l = float3(dot(d, mv.cur[i][0].xyz), dot(d, mv.cur[i][1].xyz), dot(d, mv.cur[i][2].xyz));
      if (abs(l.x) < 2.6 && abs(l.y) < 1.3 && l.z > -1.2 && l.z < 1.8) {
        wp = mv.prev[i][3].xyz + l.x * mv.prev[i][0].xyz + l.y * mv.prev[i][1].xyz + l.z * mv.prev[i][2].xyz;
        break; } }
  } else wp = u.eye.xyz + view_dir(u, uv) * 20000.0;
  float4 c = u.pvp * float4(wp, 1);
  if (c.w <= 0) return float2(-1);
  float sx = u.vpt.x * c.x / c.w + u.vpt.y, sy = u.vpt.z * c.y / c.w + u.vpt.w;
  float v = sy / 480.0; if (u.p3.x > 0.5) v = 1.0 - v;
  return float2(sx / 640.0, v); }
float3 ycc(float3 c) { return float3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * c.r - 0.5 * c.b, -0.25 * c.r + 0.5 * c.g - 0.25 * c.b); }
float3 rgb_(float3 y) { return float3(y.x + y.y - y.z, y.x + y.z, y.x - y.y - y.z); }
/* Catmull-Rom history fetch, 5 taps (Jimenez) */
float3 hist_cr(texture2d<float> h, float2 uv) {
  float2 sz = float2(h.get_width(), h.get_height()), p = uv * sz - 0.5, t1 = floor(p) + 0.5, f = p - floor(p);
  float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f)), w1 = 1.0 + f * f * (-2.5 + 1.5 * f), w2 = f * (0.5 + f * (2.0 - 1.5 * f)), w3 = f * f * (-0.5 + 0.5 * f);
  float2 w12 = w1 + w2, t0 = (t1 - 1.0) / sz, t3 = (t1 + 2.0) / sz, t12 = (t1 + w2 / w12) / sz;
  float3 c = h.sample(ls, float2(t12.x, t0.y)).rgb * (w12.x * w0.y) + h.sample(ls, float2(t0.x, t12.y)).rgb * (w0.x * w12.y)
           + h.sample(ls, t12).rgb * (w12.x * w12.y) + h.sample(ls, float2(t3.x, t12.y)).rgb * (w3.x * w12.y)
           + h.sample(ls, float2(t12.x, t3.y)).rgb * (w12.x * w3.y);
  float ws = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
  return max(c / ws, 0.0); }
fragment float4 taafs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], constant MVC &mv [[buffer(1)]],
                      texture2d<float> cur [[texture(0)]], texture2d<float> hist [[texture(1)]], texture2d<float> gp [[texture(2)]]) {
  float4 C = cur.sample(ns, in.uv);
  if (u.jit.w < 0.5) return C;                       /* no history yet */
  float2 px = 1.0 / float2(cur.get_width(), cur.get_height());
  float3 m1 = 0, m2 = 0;
  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
    float3 c = ycc(cur.sample(ns, in.uv + float2(x, y) * px).rgb / (1.0 + dot(cur.sample(ns, in.uv + float2(x, y) * px).rgb, float3(0.2126, 0.7152, 0.0722))));
    m1 += c; m2 += c * c; }
  m1 /= 9.0; float3 sd = sqrt(max(m2 / 9.0 - m1 * m1, 0.0));
  float2 pu = prev_uv(u, mv, gp.sample(ns, in.uv), in.uv);
  if (any(pu < 0.0) || any(pu > 1.0)) return C;
  float3 H = hist_cr(hist, pu);
  float3 hy = ycc(H / (1.0 + dot(H, float3(0.2126, 0.7152, 0.0722))));
  float3 lo = m1 - sd * 1.25, hi = m1 + sd * 1.25;
  /* clip the history toward the neighbourhood's mean */
  float3 d = hy - m1, e = max(abs(d) / max(hi - m1, 1e-4), 1.0);
  hy = m1 + d / max(e.x, max(e.y, e.z));
  float3 cy = ycc(C.rgb / (1.0 + dot(C.rgb, float3(0.2126, 0.7152, 0.0722))));
  float speed = length((in.uv - pu) / px);
  float a = mix(0.08, 0.25, saturate(speed / 30.0));
  float3 r = mix(hy, cy, a);
  float3 o = rgb_(r); o = o / max(1.0 - dot(o, float3(0.2126, 0.7152, 0.0722)), 1e-3);
  return float4(max(o, 0.0), C.a); }
fragment float4 mbfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], constant MVC &mv [[buffer(1)]],
                     texture2d<float> src [[texture(0)]], texture2d<float> gp [[texture(1)]]) {
  float4 C = src.sample(ns, in.uv);
  if (C.a < 0.5 || u.jit.w < 0.5) return C;
  float2 pu = prev_uv(u, mv, gp.sample(ns, in.uv), in.uv);
  if (any(pu < -0.5)) return C;
  float2 sz = float2(src.get_width(), src.get_height());
  float2 v = (in.uv - pu) * u.jit.z;                 /* shutter */
  float l = length(v * sz);
  if (l < 1.0) return C;
  v *= min(l, 0.04 * sz.x) / l;
  float3 acc = C.rgb; float n = 1.0, j = ign(in.pos.xy) - 0.5;
  for (int i = 1; i <= 8; i++) {
    float t = (float(i) + j) / 8.0 - 0.5;
    float4 s = src.sample(ls, in.uv + v * t);
    if (s.a < 0.5) continue;
    acc += s.rgb; n += 1.0; }
  return float4(acc / n, C.a); }

fragment float4 pfxcfs(QO in [[stage_in]], texture2d<float> p [[texture(0)]]) { return p.sample(ls, in.uv); }

/* ---- tonemap and grade, over the untouched 2D */
/* Khronos PBR Neutral: colours below ~0.76 come through exactly, highlights
   roll off toward white without the hue shifts and greying of a film curve */
float3 neutral(float3 c) {
  const float start = 0.76, desat = 0.15;
  float x = min(c.r, min(c.g, c.b)), off = x < 0.08 ? x - 6.25 * x * x : 0.04;
  c -= off;
  float peak = max(c.r, max(c.g, c.b));
  if (peak < start) return c;
  float d = 1.0 - start, np = 1.0 - d * d / (peak + d - start);
  c *= np / peak;
  float g = 1.0 - 1.0 / (desat * (peak - np) + 1.0);
  return mix(c, float3(np), g); }
/* the scene's tone map and grade, shared by the passes below */
float3 grade(constant FXU &u, float4 H, texture2d<float> bl, texture2d<float> sh, float2 uv, float2 pos) {
  float2 in_uv = uv;
  float3 x = H.rgb + bl.sample(ls, in_uv).rgb * u.p1.w + sh.sample(ls, in_uv).rgb * H.a;
  float3 c = pow(saturate(neutral(max(x * u.p1.x * u.wb.rgb, 0.0))), 1.0 / 2.2);
  float l = dot(c, float3(0.2126, 0.7152, 0.0722));
  c = mix(float3(l), c, u.p1.y);
  c = saturate((c - 0.5) * u.p1.z + 0.5);
  float v = length((in_uv - 0.5) * float2(1.2, 1.0));
  c *= mix(1.0, smoothstep(1.05, 0.35, v), 0.12);
  c += (ign(pos) - 0.5) / 255.0;
  return c; }
fragment float4 finfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                      texture2d<float> h [[texture(0)]], texture2d<float> col [[texture(1)]],
                      texture2d<float> bl [[texture(2)]], texture2d<float> sh [[texture(3)]]) {
  float4 H = h.sample(ns, in.uv), C = col.sample(ns, in.uv);
  if (H.a <= 0.001) return C;
  if (u.p3.y > 8.5 && u.p3.y < 9.5) return C;
  if ((u.p3.y > 0.5 && u.p3.y < 8.5) || u.p3.y > 10.5) return float4(mix(C.rgb, H.rgb, H.a), 1);
  if (u.p3.y > 9.5 && u.p3.y < 10.5) {
    float3 hb = H.rgb, bb = bl.sample(ls, in.uv).rgb, sb = sh.sample(ls, in.uv).rgb;
    return float4(any(isnan(hb)) || any(isinf(hb)) ? 1.0 : (any(hb < 0.0) ? 0.5 : 0.0),
                  any(isnan(bb)) || any(isinf(bb)) ? 1.0 : (any(bb < 0.0) ? 0.5 : 0.0),
                  any(isnan(sb)) || any(isinf(sb)) ? 1.0 : (any(sb < 0.0) ? 0.5 : 0.0), 1); }
  float3 c = grade(u, H, bl, sh, in.uv, in.pos.xy);
  return float4(mix(C.rgb, c, H.a), 1); }
/* the graded scene alone, for the anti-aliasing pass: no 2D in it */
fragment float4 scenefs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                        texture2d<float> h [[texture(0)]], texture2d<float> bl [[texture(2)]], texture2d<float> sh [[texture(3)]]) {
  float4 H = h.sample(ns, in.uv);
  if (H.a <= 0.001) return float4(0);
  return float4(grade(u, H, bl, sh, in.uv, in.pos.xy), 1); }
/* the 2D layer (the game's own colour) over the anti-aliased scene */
fragment float4 mixfs(QO in [[stage_in]], texture2d<float> a [[texture(0)]], texture2d<float> h [[texture(1)]],
                      texture2d<float> col [[texture(2)]]) {
  float4 H = h.sample(ns, in.uv), C = col.sample(ns, in.uv);
  if (H.a <= 0.001) return C;
  return float4(mix(C.rgb, a.sample(ns, in.uv).rgb, H.a), 1); }

/* ---- anti-aliasing: FXAA (quality preset) on the graded scene layer, before the
   2D is composited over it, so the HUD, text and menus never pass through it */
float fl_(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }
fragment float4 aafs(QO in [[stage_in]], texture2d<float> t [[texture(0)]], texture2d<float> hd [[texture(1)]]) {
  float2 rc = 1.0 / float2(t.get_width(), t.get_height()), uv = in.uv;
  float4 C = t.sample(ls, uv);
  if (hd.sample(ns, uv).a <= 0.001) return C;
  float lM = fl_(C.rgb);
  float lN = fl_(t.sample(ls, uv + float2(0, -rc.y)).rgb), lS = fl_(t.sample(ls, uv + float2(0, rc.y)).rgb);
  float lW = fl_(t.sample(ls, uv + float2(-rc.x, 0)).rgb), lE = fl_(t.sample(ls, uv + float2(rc.x, 0)).rgb);
  float lo = min(lM, min(min(lN, lS), min(lW, lE))), hi = max(lM, max(max(lN, lS), max(lW, lE)));
  float range = hi - lo;
  if (range < max(0.0312, hi * 0.125)) return C;
  float lNW = fl_(t.sample(ls, uv + float2(-rc.x, -rc.y)).rgb), lNE = fl_(t.sample(ls, uv + float2(rc.x, -rc.y)).rgb);
  float lSW = fl_(t.sample(ls, uv + float2(-rc.x, rc.y)).rgb), lSE = fl_(t.sample(ls, uv + float2(rc.x, rc.y)).rgb);
  float eH = abs(-2.0 * lW + lNW + lSW) + 2.0 * abs(-2.0 * lM + lN + lS) + abs(-2.0 * lE + lNE + lSE);
  float eV = abs(-2.0 * lN + lNW + lNE) + 2.0 * abs(-2.0 * lM + lW + lE) + abs(-2.0 * lS + lSW + lSE);
  bool horz = eH >= eV;
  float l1 = horz ? lN : lW, l2 = horz ? lS : lE, g1 = l1 - lM, g2 = l2 - lM;
  bool steep1 = abs(g1) >= abs(g2);
  float gs = 0.25 * max(abs(g1), abs(g2)), st = horz ? rc.y : rc.x, la;
  if (steep1) { st = -st; la = 0.5 * (l1 + lM); } else la = 0.5 * (l2 + lM);
  float2 cur = uv; if (horz) cur.y += st * 0.5; else cur.x += st * 0.5;
  float2 off = horz ? float2(rc.x, 0) : float2(0, rc.y), u1 = cur - off, u2 = cur + off;
  float e1 = fl_(t.sample(ls, u1).rgb) - la, e2 = fl_(t.sample(ls, u2).rgb) - la;
  bool r1 = abs(e1) >= gs, r2 = abs(e2) >= gs;
  const float Q[12] = { 1, 1, 1, 1, 1, 1.5, 2, 2, 2, 2, 4, 8 };
  for (int i = 0; i < 12 && !(r1 && r2); i++) {
    if (!r1) { u1 -= off * Q[i]; e1 = fl_(t.sample(ls, u1).rgb) - la; r1 = abs(e1) >= gs; }
    if (!r2) { u2 += off * Q[i]; e2 = fl_(t.sample(ls, u2).rgb) - la; r2 = abs(e2) >= gs; } }
  float d1 = horz ? uv.x - u1.x : uv.y - u1.y, d2 = horz ? u2.x - uv.x : u2.y - uv.y;
  float pxo = -min(d1, d2) / (d1 + d2) + 0.5;
  bool good = ((d1 < d2 ? e1 : e2) < 0.0) != (lM < la);
  float fin = good ? pxo : 0.0;
  float lA = (2.0 * (lN + lS + lW + lE) + lNW + lNE + lSW + lSE) / 12.0;
  float sb = saturate(abs(lA - lM) / range), s2 = (-2.0 * sb + 3.0) * sb * sb;
  fin = max(fin, s2 * s2 * 0.75);
  float2 f = uv; if (horz) f.y += fin * st; else f.x += fin * st;
  return float4(t.sample(ls, f).rgb, 1); }
);

typedef struct {
    float vp[16], ivp[16], svp[16];
    float eye[4], sun[4], sunc[4], skyc[4], grnd[4], fogc[4], vpt[4], scr[4];
    float p0[4], p1[4], p2[4], p3[4], p4[4], flash[4], wb[4], nsky[4], sk2[4], tm[4], wx[4];
    float pvp[16], jit[4];   /* last frame's view x projection; this frame's jitter (ndc) and the last one's */
    float svp2[16], csm[4];  /* the far shadow cascade; csm.x its texel size */   /* wx: rain, snowfall, snow ground, - */
} fxu;
typedef struct { float p[32][4], d[32][4], col[4], misc[4]; float g[64][4], gd[64][4], gmisc[4]; } hlu;
static float g_hl_int, g_hl_beam, g_hl_air;
typedef struct { int at_fn, at_ref, use_tex, pad; float su, sv, pad2, pad3; float svp[16]; } shu;

static id<MTLDevice> D;
static id<MTLLibrary> L;
static id<MTLRenderPipelineState> p_trk, p_pfx, p_pfxc, p_nrm, p_taa, p_mb;
static id<MTLRenderPipelineState> p_sh, p_ao, p_blur, p_ssr, p_comp, p_pre, p_down, p_up, p_shaft, p_fin, p_aa, p_scene, p_mix;
static id<MTLDepthStencilState> ds_sh;
static id<MTLTexture> t_trk, t_pfx, t_nrm, t_hist[2], t_mb;
static int g_hist_ok, g_hist_i;
static id<MTLTexture> t_sm2;
static id<MTLTexture> t_sm, t_ao, t_ao2, t_ssr, t_hdr, t_bl[6], t_shaft, t_out, t_aa;
static int tw, th;
static int g_on = -1;

typedef struct { __unsafe_unretained id<MTLBuffer> buf; size_t off; int n; __unsafe_unretained id<MTLTexture> tex;
                 __unsafe_unretained id<MTLSamplerState> smp; shu s; } shrec;
static shrec *g_rec; static int g_nrec, g_caprec;
static NSMutableArray *g_keep;           /* keeps recorded buffers/textures alive for the frame */
static int g_havecam;
static float g_P[16], g_vpt[4];
static unsigned g_frame;

int hfx_on(void)
{
    if (g_on < 0) {
        const char *e = getenv("BR_FX");
        if (e) g_on = atoi(e) != 0;
        else if (happ_metal_layer() == nil) g_on = 0;
        else {
            NSString *r = [[NSUserDefaults standardUserDefaults] stringForKey:@"Renderer"];
            g_on = !r || ![r isEqualToString:@"original"];
        }
    }
    return g_on;
}

/* A word in the top left corner, faded in, held and out: which renderer. */
NSWindow *happ_window(void);
static CATextLayer *g_badge;
static void badge(void)
{
    NSView *v = happ_window().contentView;
    CAKeyframeAnimation *a;
    CGFloat h, fs;
    if (!v) return;
    if (!v.layer) v.wantsLayer = YES;
    if (!g_badge) {
        g_badge = [CATextLayer layer];
        g_badge.foregroundColor = NSColor.whiteColor.CGColor;
        g_badge.shadowColor = NSColor.blackColor.CGColor;
        g_badge.shadowOpacity = 0.7f; g_badge.shadowRadius = 3; g_badge.shadowOffset = CGSizeMake(0, -1);
        g_badge.font = (__bridge CFTypeRef)[NSFont systemFontOfSize:12 weight:NSFontWeightHeavy];
        g_badge.opacity = 0;
        [v.layer addSublayer:g_badge];
    }
    h = v.bounds.size.height;
    fs = fmin(fmax(h * 0.035, 14), 34);
    g_badge.contentsScale = happ_window().backingScaleFactor;
    g_badge.fontSize = fs;
    g_badge.string = g_on ? @"REMASTERED" : @"ORIGINAL";
    g_badge.frame = CGRectMake(fmax(h * 0.025, 8), h - fmax(h * 0.025, 8) - fs * 1.3, fs * 12, fs * 1.3);
    a = [CAKeyframeAnimation animationWithKeyPath:@"opacity"];
    a.values = @[ @0, @0.95, @0.95, @0 ];
    a.keyTimes = @[ @0, @0.06, @0.75, @1 ];
    a.duration = 2.0;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    g_badge.opacity = 0;
    [g_badge removeAnimationForKey:@"show"];
    [g_badge addAnimation:a forKey:@"show"];
    [CATransaction commit];
}

void hfx_toggle(void)
{
    g_on = !hfx_on();
    [[NSUserDefaults standardUserDefaults] setObject:g_on ? @"remastered" : @"original" forKey:@"Renderer"];
    fprintf(stderr, "renderer: %s\n", g_on ? "Remastered" : "Original");
    nmusic_remastered(g_on);
    badge();
}

/* host_app.m, for every key event: 1 if it was the switch's (~ / `, the key
 * left of 1, with no Command held) and must not reach the game. */
int hfx_key(NSEvent *e)
{
    /* 0x32 is ` ~ on ANSI keyboards; ISO ones put § there (0x0A) and move ` */
    if ((e.keyCode != 0x32 && e.keyCode != 0x0A) || (e.modifierFlags & NSEventModifierFlagCommand)) return 0;
    if (e.type == NSEventTypeKeyDown && !e.isARepeat) hfx_toggle();
    return 1;
}

/* The main camera's view x projection (the game's row-vector matrix) and
 * the viewport mapping clip space to the 640x480 screen. */
void hfx_set_cam(const float *P, float sx, float tx, float sy, float ty)
{
    memcpy(g_P, P, sizeof g_P);
    g_vpt[0] = sx; g_vpt[1] = tx; g_vpt[2] = sy; g_vpt[3] = ty;
    g_havecam = 1;
}

void hfx_shadow_batch(id<MTLBuffer> buf, size_t off, int n, id<MTLTexture> tex, id<MTLSamplerState> smp,
                      int at_fn, int at_ref, int use_tex, float su, float sv)
{
    if (!g_havecam) return;
    if (g_nrec == g_caprec) { g_caprec = g_caprec ? g_caprec * 2 : 1024; g_rec = realloc(g_rec, sizeof *g_rec * (size_t)g_caprec); }
    if (!g_keep) g_keep = [NSMutableArray new];
    [g_keep addObject:buf]; if (tex) [g_keep addObject:tex]; if (smp) [g_keep addObject:smp];
    g_rec[g_nrec++] = (shrec){ buf, off, n, tex, smp, { at_fn, at_ref, use_tex, 0, su, sv, 0, 0, {0} } };
}

static void inv4(const double *m, double *o)
{
    double a[4][8];
    int i, j, k;
    for (i = 0; i < 4; i++) for (j = 0; j < 8; j++) a[i][j] = j < 4 ? m[i * 4 + j] : (j - 4 == i);
    for (i = 0; i < 4; i++) {
        int p = i; double t;
        for (k = i + 1; k < 4; k++) if (fabs(a[k][i]) > fabs(a[p][i])) p = k;
        for (j = 0; j < 8; j++) { t = a[i][j]; a[i][j] = a[p][j]; a[p][j] = t; }
        t = a[i][i]; if (t == 0) t = 1e-30;
        for (j = 0; j < 8; j++) a[i][j] /= t;
        for (k = 0; k < 4; k++) if (k != i) { t = a[k][i]; for (j = 0; j < 8; j++) a[k][j] -= t * a[i][j]; }
    }
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) o[i * 4 + j] = a[i][j + 4];
}
/* row vector h = v * M */
static void rowmul(const double *v, const double *M, double *h)
{
    int j;
    for (j = 0; j < 4; j++) h[j] = v[0] * M[j] + v[1] * M[4 + j] + v[2] * M[8 + j] + v[3] * M[12 + j];
}
static void norm3(double *v) { double l = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0) { v[0] /= l; v[1] /= l; v[2] /= l; } }
static void cross3(const double *a, const double *b, double *o)
{ o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0]; }

static id<MTLRenderPipelineState> mkpipe(NSString *vs, NSString *fs, MTLPixelFormat f, int add)
{
    NSError *err = nil;
    MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
    d.vertexFunction = [L newFunctionWithName:vs];
    d.fragmentFunction = [L newFunctionWithName:fs];
    if (f == MTLPixelFormatDepth32Float) d.depthAttachmentPixelFormat = f;
    else {
        d.colorAttachments[0].pixelFormat = f;
        if (add) {
            d.colorAttachments[0].blendingEnabled = YES;
            d.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
            d.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
            d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
            d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
        }
    }
    id<MTLRenderPipelineState> p = [D newRenderPipelineStateWithDescriptor:d error:&err];
    if (!p) { fprintf(stderr, "fx pipeline %s: %s\n", fs.UTF8String, err.localizedDescription.UTF8String); exit(1); }
    return p;
}

static id<MTLTexture> mktex(int w, int h, MTLPixelFormat f, int shared)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f
                                   width:(NSUInteger)(w < 1 ? 1 : w) height:(NSUInteger)(h < 1 ? 1 : h) mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = shared ? MTLStorageModeShared : MTLStorageModePrivate;
    return [D newTextureWithDescriptor:td];
}

static void setup(id<MTLDevice> dev)
{
    NSError *err = nil;
    if (L) return;
    D = dev;
    L = [D newLibraryWithSource:[NSString stringWithUTF8String:FXSRC] options:nil error:&err];
    if (!L) { fprintf(stderr, "fx shader: %s\n", err.localizedDescription.UTF8String); exit(1); }
    {
        MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
        d.vertexFunction = [L newFunctionWithName:@"shvs"];
        d.fragmentFunction = [L newFunctionWithName:@"shfs"];
        d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        p_sh = [D newRenderPipelineStateWithDescriptor:d error:&err];
        if (!p_sh) { fprintf(stderr, "fx shadow pipeline: %s\n", err.localizedDescription.UTF8String); exit(1); }
    }
    p_ao = mkpipe(@"fsq", @"aofs", MTLPixelFormatRGBA16Float, 0);
    p_blur = mkpipe(@"fsq", @"blurfs", MTLPixelFormatRGBA16Float, 0);
    p_ssr = mkpipe(@"fsq", @"ssrfs", MTLPixelFormatRGBA16Float, 0);
    p_comp = mkpipe(@"fsq", @"compfs", MTLPixelFormatRGBA16Float, 0);
    p_pre = mkpipe(@"fsq", @"prefs", MTLPixelFormatRGBA16Float, 0);
    p_down = mkpipe(@"fsq", @"downfs", MTLPixelFormatRGBA16Float, 0);
    p_up = mkpipe(@"fsq", @"upfs", MTLPixelFormatRGBA16Float, 1);
    p_shaft = mkpipe(@"fsq", @"shaftfs", MTLPixelFormatRGBA16Float, 0);
    p_fin = mkpipe(@"fsq", @"finfs", MTLPixelFormatRGBA8Unorm, 0);
    p_aa = mkpipe(@"fsq", @"aafs", MTLPixelFormatRGBA8Unorm, 0);
    p_scene = mkpipe(@"fsq", @"scenefs", MTLPixelFormatRGBA8Unorm, 0);
    p_mix = mkpipe(@"fsq", @"mixfs", MTLPixelFormatRGBA8Unorm, 0);
    p_pfx = mkpipe(@"fsq", @"pfxfs", MTLPixelFormatRGBA16Float, 0);
    p_nrm = mkpipe(@"fsq", @"nrmfs", MTLPixelFormatRGBA16Float, 0);
    p_taa = mkpipe(@"fsq", @"taafs", MTLPixelFormatRGBA16Float, 0);
    p_mb = mkpipe(@"fsq", @"mbfs", MTLPixelFormatRGBA16Float, 0);
    {   /* track ribbons: the strongest wins where they overlap */
        MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
        d.vertexFunction = [L newFunctionWithName:@"trkvs"];
        d.fragmentFunction = [L newFunctionWithName:@"trkfs"];
        d.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
        d.colorAttachments[0].blendingEnabled = YES;
        d.colorAttachments[0].rgbBlendOperation = d.colorAttachments[0].alphaBlendOperation = MTLBlendOperationMax;
        p_trk = [D newRenderPipelineStateWithDescriptor:d error:&err];
        if (!p_trk) { fprintf(stderr, "fx track pipeline: %s\n", err.localizedDescription.UTF8String); exit(1); }
        /* particles over the lit scene, premultiplied; the scene's coverage kept */
        d = [MTLRenderPipelineDescriptor new];
        d.vertexFunction = [L newFunctionWithName:@"fsq"];
        d.fragmentFunction = [L newFunctionWithName:@"pfxcfs"];
        d.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
        d.colorAttachments[0].blendingEnabled = YES;
        d.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
        d.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
        d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
        p_pfxc = [D newRenderPipelineStateWithDescriptor:d error:&err];
        if (!p_pfxc) { fprintf(stderr, "fx particle pipeline: %s\n", err.localizedDescription.UTF8String); exit(1); }
    }
    {
        MTLDepthStencilDescriptor *d = [MTLDepthStencilDescriptor new];
        d.depthCompareFunction = MTLCompareFunctionLess;
        d.depthWriteEnabled = YES;
        ds_sh = [D newDepthStencilStateWithDescriptor:d];
    }
    {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                     width:2048 height:2048 mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        t_sm = [D newTextureWithDescriptor:td];
        t_sm2 = [D newTextureWithDescriptor:td];
    }
}

static void size(int w, int h)
{
    int i, hw = (w + 1) / 2, hh = (h + 1) / 2;
    if (w == tw && h == th) return;
    tw = w; th = h;
    t_ao = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_ao2 = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_ssr = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_shaft = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_hdr = mktex(w, h, MTLPixelFormatRGBA16Float, 0);
    for (i = 0; i < 6; i++) t_bl[i] = mktex(hw >> i, hh >> i, MTLPixelFormatRGBA16Float, 0);
    t_out = mktex(w, h, MTLPixelFormatRGBA8Unorm, 1);
    t_aa = mktex(w, h, MTLPixelFormatRGBA8Unorm, 1);
    t_trk = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_pfx = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_nrm = mktex(w, h, MTLPixelFormatRGBA16Float, 0);
    t_hist[0] = mktex(w, h, MTLPixelFormatRGBA16Float, 0);
    t_hist[1] = mktex(w, h, MTLPixelFormatRGBA16Float, 0);
    t_mb = mktex(w, h, MTLPixelFormatRGBA16Float, 0);
    g_hist_ok = 0;
}

static id<MTLRenderCommandEncoder> pass(id<MTLCommandBuffer> cb, id<MTLTexture> t, int load)
{
    MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture = t;
    rp.colorAttachments[0].loadAction = load ? MTLLoadActionLoad : MTLLoadActionClear;
    rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    return [cb renderCommandEncoderWithDescriptor:rp];
}
static void quad(id<MTLRenderCommandEncoder> e, id<MTLRenderPipelineState> p, const fxu *u, NSArray *tex)
{
    NSUInteger i;
    [e setRenderPipelineState:p];
    if (u) [e setFragmentBytes:u length:sizeof *u atIndex:0];
    for (i = 0; i < tex.count; i++) [e setFragmentTexture:tex[i] atIndex:i];
    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [e endEncoding];
}

/* Every car the frame drew (BrCarDrawVehicle, native/car.m): its record
 * starts with its car -> world matrix (row vectors; the model's own units are
 * 1/255 of the world's).  Headlights are placed from it. */
#define FX_CARS 16
static float g_cars[FX_CARS][16];
static u32 g_carptr[FX_CARS], g_carmodel[FX_CARS];
static int g_ncars;
void hfx_car_seen(u32 car)
{
    int i;
    for (i = 0; i < g_ncars; i++) if (g_carptr[i] == car) return;   /* the mirror draws them again */
    if (g_ncars == FX_CARS) return;
    g_carptr[g_ncars] = car;
    g_carmodel[g_ncars] = W_LD(u32, car, 0x29C4);                    /* its .rca model record */
    for (i = 0; i < 16; i++) g_cars[g_ncars][i] = W_LD(f32, car, 4 * i);
    g_ncars++;
}

/* A car model's extent in its own frame (x forward, y left, z up; world
 * metres), measured once from the vertices of its full-detail body display
 * list: G_VTX loads (0x04) of eight floats per vertex, x y z first, followed
 * into called lists (0x06) until G_ENDDL (0xB8).  The lamps sit on its faces. */
typedef struct { u32 model; float lo[3], hi[3]; int ok; } carbox;
static carbox g_box[32];
static int g_nbox;
static void box_dl(u32 dl, carbox *b, int depth)
{
    int k;
    if (!dl || depth > 6) return;
    for (k = 0; k < 4096; k++, dl += 8) {
        u32 w0 = W_LD(u32, dl, 0), w1 = W_LD(u32, dl, 4), op = w0 >> 24;
        if (op == 0xB8) return;
        if (op == 0x06) { box_dl(w1, b, depth + 1); if (w0 & 0x00010000u) return; continue; }
        if (op == 0x04) {
            int n = (int)((w0 >> 10) & 0x3F), v, c;
            for (v = 0; v < n; v++)
                for (c = 0; c < 3; c++) {
                    float x = W_LD(f32, w1 + (u32)v * 0x20u, 4 * c) / 255.0f;
                    if (!(x > -100 && x < 100)) continue;
                    if (!b->ok || x < b->lo[c]) b->lo[c] = x;
                    if (!b->ok || x > b->hi[c]) b->hi[c] = x;
                    if (c == 2) b->ok = 1;
                }
        }
    }
}
static const carbox *car_box(u32 model)
{
    int i;
    for (i = 0; i < g_nbox; i++) if (g_box[i].model == model) return &g_box[i];
    if (g_nbox == 32 || !model) return NULL;
    memset(&g_box[g_nbox], 0, sizeof g_box[g_nbox]);
    g_box[g_nbox].model = model;
    box_dl(W_LD(u32, model, 0x8024), &g_box[g_nbox], 0);   /* LOD 0 body */
    if (getenv("BR_FX_CARPROBE"))
        fprintf(stderr, "carbox %08X: x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f\n", model,
                g_box[g_nbox].lo[0], g_box[g_nbox].hi[0], g_box[g_nbox].lo[1], g_box[g_nbox].hi[1],
                g_box[g_nbox].lo[2], g_box[g_nbox].hi[2]);
    return &g_box[g_nbox++];
}
/* The Remastered car can say exactly where its lamps are (car frame, metres:
 * front left, front right, rear left, rear right). */
__attribute__((weak)) int hcar_lamps(u32 car, float out[4][3]) { (void)car; (void)out; return 0; }

/* The light rig for physically lit models drawn into the scene (the
 * Remastered car).  The scene rig above is relative to the game's own
 * shading, which already darkens its textures and vertex colours at night
 * and in storms; a model with true albedo gets no such darkening, so its sky
 * light is scaled by how dark the game makes the world in that weather.
 * Filled at every swap; zero before the first. */
static float g_car_rig[4][4];
static int g_car_rig_ok;
int hfx_car_rig(float *sun, float *sunc, float *skyc, float *grnd)
{
    if (!g_car_rig_ok) return 0;
    memcpy(sun, g_car_rig[0], 16); memcpy(sunc, g_car_rig[1], 16);
    memcpy(skyc, g_car_rig[2], 16); memcpy(grnd, g_car_rig[3], 16);
    return 1;
}

static float rdf(u32 a) { float f; u32 v = H32(a); memcpy(&f, &v, 4); return f; }

/* The light rig for the game's weather. */
static void weather(fxu *u, const float *fogc)
{
    int w = (int)H32(0x104B15E8u);
    int lightning = (int)H32(0x100A79CCu);
    const char *ov = getenv("BR_FX_WEATHER");
    float sunk, sk[3], gr[3], sc[3];
    if (ov) w = atoi(ov);
    /* p0: ao, gi, ssr, wet   p1: exposure, saturation, contrast, bloom
     * p2: shafts, shadow softness (texels), ao radius, frame
     * p3: origin_ll, -, rough, shadow texel   p4: sky gain, sun glow, fog light, light scale */
    /* Levels are relative to the game's own shading, which the textures and
     * vertex colours already carry: a surface facing the sun ends near 1.15x,
     * one in shadow near 0.6x, and shadow is filled with the sky's colour.
     * hz: the air's brightness at the horizon; hf: 1 = the air takes the
     * game's fog colour (night, fog) instead of the sky's. */
    float hz = 1.3f, hf = 0.0f;
    float wb[3] = { 1.03f, 1.0f, 0.96f }, ns[3] = { 0, 0, 0 };
    switch (w) {
    case 4:  /* rain, at night: moonlight, wet, lit windows glowing */
        sunk = 0.14f; sc[0] = 0.6f; sc[1] = 0.72f; sc[2] = 1.0f;
        sk[0] = 0.5f; sk[1] = 0.56f; sk[2] = 0.72f; gr[0] = 0.3f; gr[1] = 0.3f; gr[2] = 0.35f;
        memcpy(u->p0, (float[4]){ 1.0f, 0.5f, 0.75f, 0.85f }, 16);
        memcpy(u->p1, (float[4]){ 1.25f, 0.95f, 1.04f, 0.3f }, 16);
        u->p2[0] = 0.0f; u->p2[1] = 7.0f; u->p3[2] = 0.35f; u->p4[0] = 1.0f; u->p4[1] = 0.0f;
        u->scr[2] = 0.3f; hf = 1.0f;
        wb[0] = 0.82f; wb[1] = 0.95f; wb[2] = 1.25f;
        ns[0] = 0.018f; ns[1] = 0.03f; ns[2] = 0.065f;
        break;
    case 2:  /* storm: overcast, wet, lightning */
        sunk = 0.10f; sc[0] = 0.8f; sc[1] = 0.85f; sc[2] = 1.0f;
        sk[0] = 0.78f; sk[1] = 0.82f; sk[2] = 0.9f; gr[0] = 0.42f; gr[1] = 0.42f; gr[2] = 0.45f;
        memcpy(u->p0, (float[4]){ 1.0f, 0.35f, 0.75f, 1.0f }, 16);
        memcpy(u->p1, (float[4]){ 1.12f, 0.92f, 1.04f, 0.1f }, 16);
        u->p2[0] = 0.0f; u->p2[1] = 9.0f; u->p3[2] = 0.35f; u->p4[0] = 1.05f; u->p4[1] = 0.0f;
        hz = 1.0f; wb[0] = 0.94f; wb[1] = 1.0f; wb[2] = 1.1f;
        break;
    case 3:  /* snow: bright overcast, strong bounce */
        sunk = 0.55f; sc[0] = 1.0f; sc[1] = 0.97f; sc[2] = 0.93f;
        sk[0] = 0.62f; sk[1] = 0.68f; sk[2] = 0.8f; gr[0] = 0.5f; gr[1] = 0.52f; gr[2] = 0.58f;
        memcpy(u->p0, (float[4]){ 1.0f, 0.35f, 0.1f, 0.0f }, 16);
        memcpy(u->p1, (float[4]){ 1.0f, 1.0f, 1.08f, 0.08f }, 16);
        u->p2[0] = 0.12f; u->p2[1] = 3.0f; u->p3[2] = 0.45f; u->p4[0] = 1.1f; u->p4[1] = 0.5f;
        hz = 1.25f; wb[0] = 0.97f; wb[1] = 1.0f; wb[2] = 1.06f;
        break;
    case 1:  /* fog: soft and diffuse */
        sunk = 0.3f; sc[0] = 1.0f; sc[1] = 0.97f; sc[2] = 0.92f;
        sk[0] = 0.8f; sk[1] = 0.82f; sk[2] = 0.86f; gr[0] = 0.5f; gr[1] = 0.49f; gr[2] = 0.47f;
        memcpy(u->p0, (float[4]){ 0.9f, 0.35f, 0.12f, 0.2f }, 16);
        memcpy(u->p1, (float[4]){ 1.08f, 0.95f, 1.0f, 0.1f }, 16);
        u->p2[0] = 0.0f; u->p2[1] = 10.0f; u->p3[2] = 0.5f; u->p4[0] = 1.05f; u->p4[1] = 0.2f;
        hf = 1.0f; wb[0] = 1.0f; wb[1] = 1.0f; wb[2] = 1.02f;
        break;
    default: /* sunny: clean high-key daylight */
        sunk = 1.45f; sc[0] = 1.0f; sc[1] = 0.93f; sc[2] = 0.8f;
        sk[0] = 0.4f; sk[1] = 0.5f; sk[2] = 0.72f; gr[0] = 0.36f; gr[1] = 0.31f; gr[2] = 0.25f;
        memcpy(u->p0, (float[4]){ 1.0f, 0.4f, 0.07f, 0.0f }, 16);
        memcpy(u->p1, (float[4]){ 1.08f, 1.12f, 1.06f, 0.07f }, 16);
        u->p2[0] = 0.3f; u->p2[1] = 2.2f; u->p3[2] = 0.45f; u->p4[0] = 1.15f; u->p4[1] = 1.0f;
        u->scr[2] = 1.1f;
        break;
    }
    u->sunc[0] = sc[0] * sunk; u->sunc[1] = sc[1] * sunk; u->sunc[2] = sc[2] * sunk;
    memcpy(u->skyc, sk, 12); memcpy(u->grnd, gr, 12);
    u->skyc[3] = hz; u->grnd[3] = hf;
    /* haze: how far toward the air colour distance takes a surface, and the
     * sky's share of fog; headlights: on at night, in storms and in fog */
    u->nsky[3] = w == 1 ? 0.9f : 0.3f;
    /* falling rain (storm heaviest), falling snow, snow on the ground */
    u->wx[0] = w == 2 ? 1.0f : w == 4 ? 0.75f : 0.0f;
    u->wx[1] = w == 3 ? 1.0f : 0.0f;
    u->wx[2] = w == 3 ? 1.0f : 0.0f;
    u->wx[3] = getenv("BR_FX_DETAIL") ? (float)atof(getenv("BR_FX_DETAIL")) : 1.0f;
    /* a real sky in clear weather (clouds kept from texels whose darkest
     * channel is between sk2.y and sk2.z), and water shading */
    u->sk2[0] = w == 0 ? 1.0f : 0.0f; u->sk2[1] = 0.36f; u->sk2[2] = 0.55f;
    u->sk2[3] = w == 1 ? 0.5f : 1.0f;
    u->wb[3] = w == 1 ? 0.8f : 0.0f;
    g_hl_int = w == 4 ? 1.0f : w == 2 ? 0.6f : w == 1 ? 0.5f : 0.0f;
    g_hl_beam = w == 4 ? 1.0f : w == 2 ? 0.8f : w == 1 ? 1.4f : 0.0f;
    /* how much the air scatters a beam: clear night air barely, rain a
     * little, fog most; any more and the scene reads as seen through film */
    g_hl_air = w == 1 ? 0.0035f : w == 2 ? 0.0012f : 0.0007f;
    {   /* the world's darkness per weather, for the car rig (see hfx_car_rig) */
        float wk = w == 4 ? 0.15f : w == 2 ? 0.7f : w == 1 ? 0.9f : 1.0f;
        int i;
        for (i = 0; i < 3; i++) {
            g_car_rig[1][i] = u->sunc[i];
            g_car_rig[2][i] = u->skyc[i] * wk * (w == 4 ? 0.9f : 1.0f);
            g_car_rig[3][i] = u->grnd[i] * wk;
        }
        g_car_rig_ok = 1;
    }
    memcpy(u->wb, wb, 12); memcpy(u->nsky, ns, 12);
    u->p4[2] = 1.0f;      /* fogged pixels keep the game's own light */
    if (u->scr[2] == 0) u->scr[2] = 0.8f;    /* bloom threshold */
    u->p4[3] = 1.0f;
    u->sun[3] = sunk > 0.2f ? 1.0f : 0.0f;
    u->fogc[0] = fogc[0]; u->fogc[1] = fogc[1]; u->fogc[2] = fogc[2];
    /* haze density per metre: clear air, then rain, snow, storm, fog */
    u->fogc[3] = w == 1 ? 0.014f : w == 2 ? 0.003f : w == 4 ? 0.0025f : w == 3 ? 0.002f : 0.0012f;
    /* lightning: the strike counts 3..1 while it lights the sky */
    if (w == 2 && lightning > 0 && lightning <= 3) {
        u->flash[0] = 0.8f; u->flash[1] = 0.85f; u->flash[2] = 1.0f; u->flash[3] = 2.5f * (float)lightning / 3.0f;
    }
    { const char *e;
      if ((e = getenv("BR_FX_AOR"))) u->p2[2] = (float)atof(e);
      if ((e = getenv("BR_FX_DEBUG"))) u->p3[1] = (float)atof(e);
      if (getenv("BR_FX_NOSHAFT")) u->p2[0] = 0;
      if (getenv("BR_FX_NOSHADOW")) u->sun[3] = 0;
      if (getenv("BR_FX_NOBLOOM")) u->p1[3] = 0; }
}

    /* BR_FX_TESTKEY=N,M,...: post a real ~ key press at those swaps (a
     * windowed check of the switch's keyboard path) */
void hfx_tick(void)
{
    if (!hfx_on()) g_ncars = 0;
    { static unsigned n; const char *t = getenv("BR_FX_TESTKEY"); n++;
      if (t && happ_window()) {
          char buf[256], *q, *sv; snprintf(buf, sizeof buf, "%s", t);
          for (q = strtok_r(buf, ",", &sv); q; q = strtok_r(NULL, ",", &sv))
              if ((unsigned)atoi(q) == n) {
                  NSEventType ty[2] = { NSEventTypeKeyDown, NSEventTypeKeyUp }; int k;
                  for (k = 0; k < 2; k++)
                      [NSApp postEvent:[NSEvent keyEventWithType:ty[k] location:NSZeroPoint modifierFlags:0
                                          timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:happ_window().windowNumber
                                          context:nil characters:@"`" charactersIgnoringModifiers:@"`" isARepeat:NO keyCode:0x32] atStart:NO];
              } } }
}

/* ---- temporal: the jitter this frame's 3D is drawn with (Halton 2,3 over
 * 8 frames, in the target's pixels, as clip-space NDC for host_glide.m), last
 * frame's camera, and each car's last transform */
static float g_jit[2], g_pvp[16];
static int g_pvp_ok, g_jit_i;
static int g_taa = -1;
static int taa_on(void) { if (g_taa < 0) g_taa = !getenv("BR_FX_TAA") || atoi(getenv("BR_FX_TAA")); return g_taa; }
static float halton(int i, int b) { float f = 1, r = 0; while (i > 0) { f /= (float)b; r += f * (float)(i % b); i /= b; } return r; }
/* host_glide.m: the offset (NDC) to add to every 3D corner this frame */
void hfx_jitter(float *jx, float *jy, int rw, int rh)
{
    if (!hfx_on() || !taa_on()) { *jx = *jy = 0; return; }
    *jx = g_jit[0] * 2.0f / (float)rw; *jy = g_jit[1] * 2.0f / (float)rh;
}
static void next_jitter(void)
{
    g_jit_i = (g_jit_i + 1) % 8;
    g_jit[0] = halton(g_jit_i + 1, 2) - 0.5f; g_jit[1] = halton(g_jit_i + 1, 3) - 0.5f;
}
typedef struct { float cur[16][4][4], prev[16][4][4], misc[4]; } mvu;
static u32 g_prevcar[FX_CARS]; static float g_prevmat[FX_CARS][16]; static int g_nprev;

/* ---- weather effects the port draws itself in Remastered: particles
 * (the game's smoke, dust and snow spray, and water spray and splashes from
 * every wheel in the wet), falling rain and snow, tyre tracks.  The game's
 * own particle and rain draws are left out (hfx_game_particles). */
int hfx_game_particles(void) { return !hfx_on() || getenv("BR_FX_GAMEPFX") != NULL; }

static float fhash2(float x, float y)
{
    float v = sinf(x * 127.1f + y * 311.7f) * 43758.5453f;
    return v - floorf(v);
}
static float fvnoise(float x, float y)
{
    float ix = floorf(x), iy = floorf(y), fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    float a = fhash2(ix, iy), b = fhash2(ix + 1, iy), c = fhash2(ix, iy + 1), d = fhash2(ix + 1, iy + 1);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}
static float ffbm(float x, float y)   /* the shader's fbm, for where its puddles are */
{
    return fvnoise(x, y) * 0.55f + fvnoise(x * 2.03f + 17.1f, y * 2.03f + 17.1f) * 0.3f
         + fvnoise(x * 4.1f + 5.3f, y * 4.1f + 5.3f) * 0.15f;
}
static float frand(void) { return (float)rand() / (float)RAND_MAX; }

typedef struct { float p[3], v[3], age, life, r0, r1, dens, col[3], grav, drag; } sprt;
#define NSPR 700
static sprt g_spr[NSPR];
static int g_nspr;
static float g_emit[FX_CARS][4];

#define TRK_N 320
typedef struct { float p[TRK_N][3], k[TRK_N], t[TRK_N]; int head, n; u32 car; } trkw;
static trkw g_trk[FX_CARS * 4];
static float g_time;

static const u32 WHEEL_REC[4] = { 0x994, 0x57C, 0x370, 0x788 };

static void emit(const float *p, const float *v, float life, float r0, float r1, float dens,
                 float cr, float cg, float cb, float grav, float drag)
{
    sprt *s;
    if (g_nspr == NSPR) return;
    s = &g_spr[g_nspr++];
    memcpy(s->p, p, 12); memcpy(s->v, v, 12);
    s->age = 0; s->life = life; s->r0 = r0; s->r1 = r1; s->dens = dens;
    s->col[0] = cr; s->col[1] = cg; s->col[2] = cb; s->grav = grav; s->drag = drag;
}

/* One frame of the port's own weather effects: spray and splashes from each
 * wheel, and the tyre-track samples. */
static void fx_sim(int w, float wetness, float dt)
{
    int c, k, i;
    float wetroad = wetness;
    g_time += dt;
    /* age and move the spray */
    for (i = 0; i < g_nspr; ) {
        sprt *s = &g_spr[i];
        s->age += dt;
        if (s->age >= s->life) { g_spr[i] = g_spr[--g_nspr]; continue; }
        for (k = 0; k < 3; k++) { s->v[k] *= 1.0f - s->drag * dt; s->p[k] += s->v[k] * dt; }
        s->v[2] -= s->grav * dt;
        i++;
    }
    for (c = 0; c < g_ncars; c++) {
        u32 car = g_carptr[c];
        const float *m = g_cars[c];
        float up[3] = { m[8], m[9], m[10] }, fw[3] = { m[0], m[1], m[2] };
        float vel[3] = { W_LD(f32, car, 0x1024), W_LD(f32, car, 0x1028), W_LD(f32, car, 0x102C) };
        float spd = sqrtf(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]);
        for (k = 0; k < 4; k++) {
            float hub[3] = { W_LD(f32, car, 0x70 + 0x40 * k), W_LD(f32, car, 0x74 + 0x40 * k), W_LD(f32, car, 0x78 + 0x40 * k) };
            float gp[3];
            int on = W_LD(s32, car, WHEEL_REC[k] + 0x1B4) != 0;
            int surf = W_LD(s8, car, WHEEL_REC[k] + 0x1A0);
            int j;
            for (j = 0; j < 3; j++) gp[j] = hub[j] - up[j] * 0.3f;
            /* tracks: a sample every 0.3 m while the tyre is down on something
             * that keeps a mark */
            {
                trkw *t = &g_trk[c * 4 + k];
                float kk = 0;
                if (t->car != car) { memset(t, 0, sizeof *t); t->car = car; }
                if (on) kk = (surf >= 1 && surf <= 3) ? (w == 3 ? 1.0f : 0.8f) : (wetroad > 0.5f ? 0.6f : 0.0f);
                if (kk > 0) {
                    int last = (t->head + TRK_N - 1) % TRK_N;
                    float dx = gp[0] - t->p[last][0], dy = gp[1] - t->p[last][1], dz = gp[2] - t->p[last][2];
                    float d2 = dx * dx + dy * dy + dz * dz;
                    if (t->n == 0 || d2 > 0.09f) {
                        if (t->n > 0 && d2 > 4.0f) t->k[last] = -1;     /* a gap: the ribbon breaks here */
                        memcpy(t->p[t->head], gp, 12); t->k[t->head] = kk; t->t[t->head] = g_time;
                        t->head = (t->head + 1) % TRK_N; if (t->n < TRK_N) t->n++;
                    }
                } else if (t->n > 0) t->k[(t->head + TRK_N - 1) % TRK_N] = -1;
            }
            /* water: a fine spray off every tyre on a wet road, and a splash
             * where the tyre goes through standing water */
            if (on && wetroad > 0.0f && spd > 4.0f && surf != 3) {
                float pud = ffbm(gp[0] * 0.18f, gp[1] * 0.18f), lo = 0.64f - 0.14f * wetroad;
                int inpud = pud > lo + 0.04f;
                float rate = (inpud ? 70.0f : 22.0f * wetroad) * fminf(spd / 30.0f, 1.6f);
                g_emit[c][k] += rate * dt;
                while (g_emit[c][k] >= 1.0f) {
                    float p[3], v[3], sd = (k == 0 || k == 3) ? 1.0f : -1.0f;
                    float lf[3] = { m[4], m[5], m[6] };
                    g_emit[c][k] -= 1.0f;
                    for (j = 0; j < 3; j++) {
                        p[j] = gp[j] - fw[j] * 0.35f + up[j] * 0.05f;
                        v[j] = vel[j] * (inpud ? 0.45f : 0.55f) - fw[j] * spd * 0.05f
                             + up[j] * (inpud ? 1.6f + 2.2f * frand() : 0.5f + 0.8f * frand())
                             + lf[j] * sd * (inpud ? 1.5f + 1.5f * frand() : 0.4f * frand());
                    }
                    if (inpud) emit(p, v, 0.55f + 0.3f * frand(), 0.12f, 0.55f, 3.5f, 0.78f, 0.82f, 0.86f, 7.0f, 0.8f);
                    else emit(p, v, 0.7f + 0.5f * frand(), 0.25f, 1.1f, 0.9f, 0.8f, 0.83f, 0.87f, 0.6f, 1.6f);
                }
            } else g_emit[c][k] = 0;
        }
    }
}

/* The particles for this frame, nearest first: the game's (read from its
 * pool) and the port's spray. */
typedef struct { float p[160][4], c[160][4], misc[4], amb[4]; } pfu;
static u32 g_pborn_id[512]; static float g_pborn[512];
static int fx_particles(pfu *out, const float *eye, int w)
{
    typedef struct { float d, p[4], c[4]; } cand;
    static cand cs[1400];
    int n = 0, i, l;
    static const u32 HEADS[3] = { 0x10AC0C40u, 0x10AC0C3Cu, 0x10AC0C44u };
    for (l = 0; l < 3; l++) {
        u32 idx = W_LD(u16, HEADS[l], 0), guard = 0;
        while (idx && idx < 512 && guard++ < 512 && n < 1400) {
            u32 r = 0x10AC0C48u + idx * 0x20u;
            float p[3] = { W_LD(f32, r, 0), W_LD(f32, r, 4), W_LD(f32, r, 8) };
            /* the game fades each particle out through its byte at +0x1E
             * (255 new, 0 gone): that is its opacity, and its spread */
            float a = (float)W_LD(u8, r, 0x1E) / 255.0f, age = 1.0f - a;
            {
                cand *q = &cs[n++];
                float dx = p[0] - eye[0], dy = p[1] - eye[1], dz = p[2] - eye[2];
                q->d = dx * dx + dy * dy + dz * dz;
                memcpy(q->p, p, 12);
                q->p[3] = (l == 0 ? 0.25f : 0.35f) + (l == 0 ? 0.9f : 1.4f) * age;
                if (l == 1) { q->c[0] = 0.62f; q->c[1] = 0.55f; q->c[2] = 0.45f; }             /* dust */
                else if (l == 2 && w == 3) { q->c[0] = 0.92f; q->c[1] = 0.94f; q->c[2] = 0.97f; } /* snow */
                else if (l == 2) { q->c[0] = 0.8f; q->c[1] = 0.83f; q->c[2] = 0.87f; }           /* water */
                else { q->c[0] = 0.55f; q->c[1] = 0.55f; q->c[2] = 0.56f; }                       /* smoke */
                q->c[3] = (l == 2 ? 1.4f : l == 0 ? 0.6f : 1.0f) * a;
            }
            idx = W_LD(u16, r, 0x1C);
        }
    }
    if (getenv("BR_FX_PFXPROBE") && ((int)(g_time * 60) % 30) == 0) {
        for (l = 0; l < 3; l++) {
            u32 idx = W_LD(u16, HEADS[l], 0); int cnt = 0;
            fprintf(stderr, "pfx list %d:", l);
            while (idx && idx < 512 && cnt < 200) {
                u32 r = 0x10AC0C48u + idx * 0x20u;
                if (cnt < 3) fprintf(stderr, " [%u p %.1f %.1f %.2f v %.1f %.1f %.1f age %.3f e %u f %u]", idx, W_LD(f32, r, 0), W_LD(f32, r, 4), W_LD(f32, r, 8),
                                     W_LD(f32, r, 12), W_LD(f32, r, 16), W_LD(f32, r, 20), W_LD(f32, r, 0x18), W_LD(u8, r, 0x1E), W_LD(u8, r, 0x1F));
                cnt++; idx = W_LD(u16, r, 0x1C); }
            fprintf(stderr, " n=%d\n", cnt); }
        fprintf(stderr, "pfx eye %.1f %.1f %.1f own %d\n", eye[0], eye[1], eye[2], g_nspr);
    }
    for (i = 0; i < g_nspr && n < 1400; i++) {
        const sprt *s = &g_spr[i];
        float f = s->age / s->life;
        cand *q = &cs[n++];
        float dx = s->p[0] - eye[0], dy = s->p[1] - eye[1], dz = s->p[2] - eye[2];
        q->d = dx * dx + dy * dy + dz * dz;
        memcpy(q->p, s->p, 12);
        q->p[3] = s->r0 + (s->r1 - s->r0) * sqrtf(f);
        memcpy(q->c, s->col, 12);
        q->c[3] = s->dens * (1.0f - f) * (1.0f - f);
    }
    /* nearest 160, nearest first */
    {
        int cnt = 0, j;
        for (i = 0; i < n; i++) if (cs[i].d < 140.0f * 140.0f && cs[i].c[3] > 0.01f) cs[cnt++] = cs[i];
        for (i = 1; i < cnt; i++) { cand t = cs[i]; for (j = i - 1; j >= 0 && cs[j].d > t.d; j--) cs[j + 1] = cs[j]; cs[j + 1] = t; }
        if (cnt > 160) cnt = 160;
        for (i = 0; i < cnt; i++) { memcpy(out->p[i], cs[i].p, 16); memcpy(out->c[i], cs[i].c, 16); }
        return cnt;
    }
}

/* the tyre-track ribbons near the camera */
static id<MTLBuffer> fx_tracks(id<MTLDevice> dev, const float *eye, int *nv)
{
    static float *v; static int cap;
    int n = 0, i, s;
    for (i = 0; i < FX_CARS * 4; i++) {
        trkw *t = &g_trk[i];
        for (s = 1; s < t->n; s++) {
            int a = (t->head - t->n + s - 1 + 2 * TRK_N) % TRK_N, b = (a + 1) % TRK_N;
            float ka, kb, dx, dy, len, ax, ay, fa, fb;
            if (t->k[a] < 0 || t->k[b] < 0) continue;
            dx = t->p[b][0] - eye[0]; dy = t->p[b][1] - eye[1];
            if (dx * dx + dy * dy > 110.0f * 110.0f) continue;
            fa = 1.0f - (g_time - t->t[a]) / 120.0f; fb = 1.0f - (g_time - t->t[b]) / 120.0f;
            if (fa <= 0 || fb <= 0) continue;
            ka = t->k[a] * fa; kb = t->k[b] * fb;
            dx = t->p[b][0] - t->p[a][0]; dy = t->p[b][1] - t->p[a][1]; len = sqrtf(dx * dx + dy * dy);
            if (len < 1e-3f) continue;
            ax = -dy / len * 0.11f; ay = dx / len * 0.11f;
            if (n + 6 > cap) { cap = cap ? cap * 2 : 6144; v = realloc(v, (size_t)cap * 8 * sizeof(float)); }
            {
                float q[4][8] = {
                    { t->p[a][0] - ax, t->p[a][1] - ay, t->p[a][2], ka, -1, 0, 0, 0 },
                    { t->p[a][0] + ax, t->p[a][1] + ay, t->p[a][2], ka,  1, 0, 0, 0 },
                    { t->p[b][0] - ax, t->p[b][1] - ay, t->p[b][2], kb, -1, 0, 0, 0 },
                    { t->p[b][0] + ax, t->p[b][1] + ay, t->p[b][2], kb,  1, 0, 0, 0 } };
                memcpy(v + (n + 0) * 8, q[0], 32); memcpy(v + (n + 1) * 8, q[1], 32); memcpy(v + (n + 2) * 8, q[2], 32);
                memcpy(v + (n + 3) * 8, q[1], 32); memcpy(v + (n + 4) * 8, q[3], 32); memcpy(v + (n + 5) * 8, q[2], 32);
                n += 6;
            }
        }
    }
    *nv = n;
    if (!n) return nil;
    return [dev newBufferWithBytes:v length:(NSUInteger)n * 8 * sizeof(float) options:MTLResourceStorageModeShared];
}

/* Relight the finished frame.  Returns the picture to present, or nil when
 * this frame had no main-camera 3D (menus). */
id<MTLTexture> hfx_run(id<MTLDevice> dev, id<MTLCommandBuffer> cb, id<MTLTexture> col, id<MTLTexture> gn,
                       id<MTLTexture> gp, int w, int h, int origin_ll, const float *fogc)
{
    fxu u;
    hlu hl;
    double P[16], IP[16], v[4], e[4], f[4], sun[3], cf[3];
    int i, aa = 0;
    id<MTLRenderCommandEncoder> enc;
    if (!g_havecam) { g_nrec = 0; g_ncars = 0; [g_keep removeAllObjects]; return nil; }
    setup(dev);
    size(w, h);
    memset(&u, 0, sizeof u);
    g_frame++;
    for (i = 0; i < 16; i++) P[i] = g_P[i];
    inv4(P, IP);
    memcpy(u.vp, g_P, sizeof u.vp);
    for (i = 0; i < 16; i++) u.ivp[i] = (float)IP[i];
    /* the eye: the clip-space point at infinity along z */
    v[0] = 0; v[1] = 0; v[2] = 1; v[3] = 0; rowmul(v, IP, e);
    for (i = 0; i < 3; i++) u.eye[i] = (float)(e[i] / e[3]);
    /* the forward direction through the screen centre */
    v[0] = 0; v[1] = 0; v[2] = 0.5; v[3] = 1; rowmul(v, IP, f);
    for (i = 0; i < 3; i++) cf[i] = f[i] / f[3] - u.eye[i];
    norm3(cf);
    memcpy(u.vpt, g_vpt, sizeof u.vpt);
    u.scr[0] = (float)w; u.scr[1] = (float)h;
    /* pixels per metre at 1 m: the projection's x scale times half the width */
    u.scr[3] = (float)(sqrt(P[0] * P[0] + P[4] * P[4] + P[8] * P[8]) * w * 0.5);
    u.tm[3] = getenv("BR_FX_BUMP") ? (float)atof(getenv("BR_FX_BUMP")) : 0.18f;
    memcpy(u.pvp, g_pvp_ok ? g_pvp : g_P, sizeof u.pvp);
    u.jit[2] = getenv("BR_FX_SHUTTER") ? (float)atof(getenv("BR_FX_SHUTTER")) : 0.5f;
    u.jit[3] = (g_pvp_ok && g_hist_ok && taa_on()) ? 1.0f : 0.0f;
    u.p3[0] = (float)origin_ll;
    u.p2[2] = 6.0f;
    u.p2[3] = (float)(g_frame % 64);
    u.tm[0] = (float)((double)g_frame / 60.0);
    u.tm[1] = 0.75f;                           /* asphalt neutralising */
    weather(&u, fogc);
    /* the Remastered sky for the track and weather (tm.z: its gain, 0 = none) */
    { const char *ov = getenv("BR_FX_WEATHER");
      g_skyt = hsky_tex(dev, ov ? atoi(ov) : (int)H32(0x104B15E8u));
      u.tm[2] = g_skyt ? 1.0f : 0.0f; }
    /* the sun: the game's own light direction (1,1,1) in a Z-up world */
    { const char *s = getenv("BR_FX_SUN");
      sun[0] = 1; sun[1] = 1; sun[2] = 1.1;
      if (s) sscanf(s, "%lf,%lf,%lf", &sun[0], &sun[1], &sun[2]); }
    norm3(sun);
    for (i = 0; i < 3; i++) u.sun[i] = (float)sun[i];
    memcpy(g_car_rig[0], u.sun, 16);
    /* lamps: each car's two headlights and two tail lights, where the car's
     * own model puts them (its measured front and rear faces, or the exact
     * places the Remastered car reports), carried by its car -> world matrix
     * (row 0 forward, row 1 left, row 2 up) */
    memset(&hl, 0, sizeof hl);
    if (g_hl_int > 0 && !getenv("BR_FX_NOLIGHTS")) {
        int c, k, n = 0, ng = 0;
        for (c = 0; c < g_ncars && n + 2 <= 32 && ng + 4 <= 64; c++) {
            const float *m = g_cars[c];
            double f[3] = { m[0], m[1], m[2] }, lf[3] = { m[4], m[5], m[6] }, up[3] = { m[8], m[9], m[10] }, dv[3];
            float L[4][3];
            const carbox *b = NULL;
            int j, have = hcar_lamps(g_carptr[c], L);
            if (!have && (b = car_box(g_carmodel[c])) && b->ok) {
                float hw = (b->hi[1] - b->lo[1]) * 0.5f * 0.72f, yc = (b->hi[1] + b->lo[1]) * 0.5f;
                float zh = b->lo[2] + (b->hi[2] - b->lo[2]) * 0.36f, zt = b->lo[2] + (b->hi[2] - b->lo[2]) * 0.48f;
                float L0[4][3] = { { b->hi[0] - 0.08f, yc + hw, zh }, { b->hi[0] - 0.08f, yc - hw, zh },
                                   { b->lo[0] + 0.05f, yc + hw, zt }, { b->lo[0] + 0.05f, yc - hw, zt } };
                memcpy(L, L0, sizeof L);
                have = 1;
            }
            if (!have) continue;
            norm3(f); norm3(lf); norm3(up);
            for (k = 0; k < 3; k++) dv[k] = f[k] - up[k] * 0.07;
            norm3(dv);
            for (j = 0; j < 4; j++) {
                float w[3];
                for (k = 0; k < 3; k++) w[k] = (float)(m[12 + k] + f[k] * L[j][0] + lf[k] * L[j][1] + up[k] * L[j][2]);
                memcpy(hl.g[ng], w, 12); hl.g[ng][3] = j < 2 ? 0.0f : 1.0f;
                for (k = 0; k < 3; k++) hl.gd[ng][k] = (float)(j < 2 ? f[k] : -f[k]);
                ng++;
                if (j < 2) {
                    memcpy(hl.p[n], w, 12);
                    for (k = 0; k < 3; k++) hl.d[n][k] = (float)dv[k];
                    n++;
                }
            }
        }
        hl.misc[0] = (float)n;
        hl.gmisc[0] = (float)ng; hl.gmisc[1] = 1.0f;
    }
    hl.col[0] = 1.0f; hl.col[1] = 0.93f; hl.col[2] = 0.8f;
    hl.misc[1] = g_hl_beam; hl.misc[2] = g_hl_int; hl.misc[3] = g_hl_air;

    /* the sun's view: an orthographic box ahead of the camera, snapped to
     * whole shadow-map texels so the edges do not crawl as the camera moves */
    {
        double R = getenv("BR_FX_SHADOWR") ? atof(getenv("BR_FX_SHADOWR")) : 110.0, depth = 800.0;
        double c[3], fw[3] = { -sun[0], -sun[1], -sun[2] }, upw[3] = { 0, 0, 1 }, rt[3], up[3], M[16], tx, ty, tz, tex;
        double fl[3] = { cf[0], cf[1], 0 };
        norm3(fl);
        for (i = 0; i < 3; i++) c[i] = u.eye[i] + fl[i] * R * 0.7;
        cross3(fw, upw, rt); norm3(rt); cross3(rt, fw, up); norm3(up);
        tx = rt[0] * c[0] + rt[1] * c[1] + rt[2] * c[2];
        ty = up[0] * c[0] + up[1] * c[1] + up[2] * c[2];
        tz = fw[0] * c[0] + fw[1] * c[1] + fw[2] * c[2];
        tex = 2.0 * R / 2048.0;
        tx = floor(tx / tex) * tex; ty = floor(ty / tex) * tex;
        /* row-vector: clip = [x y z 1] * M; x' = (dot(rt,p) - tx)/R, z' = (dot(fw,p) - tz + depth/2)/depth */
        memset(M, 0, sizeof M);
        for (i = 0; i < 3; i++) { M[i * 4 + 0] = rt[i] / R; M[i * 4 + 1] = up[i] / R; M[i * 4 + 2] = fw[i] / depth; }
        M[12] = -tx / R; M[13] = -ty / R; M[14] = (-tz + depth * 0.5) / depth; M[15] = 1;
        for (i = 0; i < 16; i++) u.svp[i] = (float)M[i];
        u.p3[3] = (float)tex;
        /* the far cascade: the same view, 4x wider */
        {
            double R2 = R * 4.0, c2[3], tex2 = 2.0 * R2 / 2048.0;
            for (i = 0; i < 3; i++) c2[i] = u.eye[i] + fl[i] * R2 * 0.6;
            tx = rt[0] * c2[0] + rt[1] * c2[1] + rt[2] * c2[2];
            ty = up[0] * c2[0] + up[1] * c2[1] + up[2] * c2[2];
            tz = fw[0] * c2[0] + fw[1] * c2[1] + fw[2] * c2[2];
            tx = floor(tx / tex2) * tex2; ty = floor(ty / tex2) * tex2;
            memset(M, 0, sizeof M);
            for (i = 0; i < 3; i++) { M[i * 4 + 0] = rt[i] / R2; M[i * 4 + 1] = up[i] / R2; M[i * 4 + 2] = fw[i] / (depth * 2); }
            M[12] = -tx / R2; M[13] = -ty / R2; M[14] = (-tz + depth) / (depth * 2); M[15] = 1;
            for (i = 0; i < 16; i++) u.svp2[i] = (float)M[i];
            u.csm[0] = (float)tex2;
        }
    }

    if (getenv("BR_FX_STAT") && g_frame % 500 == 0) {
        long tris = 0; for (i = 0; i < g_nrec; i++) tris += g_rec[i].n / 3;
        fprintf(stderr, "fx: frame %u shadow batches %d tris %ld eye %.1f %.1f %.1f fwd %.2f %.2f %.2f weather %d chosen %d rain %d storm %d snow %d carlight %d %d %d amb %d %d %d f661c %d\n",
                g_frame, g_nrec, tris, u.eye[0], u.eye[1], u.eye[2], cf[0], cf[1], cf[2], (int)H32(0x104B15E8u),
                (int)H32(0x10226E80u), (int)H32(0x106C6620u), (int)H32(0x106C6624u), (int)H32(0x106ED6B0u),
                W_LD(u8, 0x106C1580u, 0), W_LD(u8, 0x106C335Cu, 0), W_LD(u8, 0x106C0968u, 0),
                W_LD(u8, 0x106B7C80u, 0), W_LD(u8, 0x106C0960u, 0), W_LD(u8, 0x106C65BCu, 0), (int)H32(0x106C661Cu));
    }
    if (getenv("BR_FX_CARPROBE") && g_frame % 200 == 0) {
        u32 vc = H32(0x106E9D88u);
        for (i = 0; i < g_ncars; i++) if (g_carptr[i] == vc) {
            static float lp[3]; float *m = g_cars[i];
            { u32 c = g_carptr[i]; int k; static const u32 WO[4] = { 0x994, 0x57C, 0x370, 0x788 };
              for (k = 0; k < 4; k++)
                  fprintf(stderr, "wheel%d: %.2f %.2f %.2f contact %d surf %d\n", k, W_LD(f32, c, 0x70 + 0x40 * k), W_LD(f32, c, 0x74 + 0x40 * k),
                          W_LD(f32, c, 0x78 + 0x40 * k), W_LD(s32, c, WO[k] + 0x1B4), W_LD(s8, c, WO[k] + 0x1A0));
              fprintf(stderr, "speed %.1f h %.2f vel %.2f %.2f %.2f\n", W_LD(f32, c, 0x1030), W_LD(f32, c, 0x2994),
                      W_LD(f32, c, 0x1024), W_LD(f32, c, 0x1028), W_LD(f32, c, 0x102C)); }
            fprintf(stderr, "car: r0 %.2f %.2f %.2f r1 %.2f %.2f %.2f r2 %.2f %.2f %.2f pos %.1f %.1f %.1f d %.2f %.2f %.2f camfwd %.2f %.2f %.2f\n",
                    m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10], m[12], m[13], m[14],
                    m[12] - lp[0], m[13] - lp[1], m[14] - lp[2], cf[0], cf[1], cf[2]);
            lp[0] = m[12]; lp[1] = m[13]; lp[2] = m[14];
        }
    }
    /* 1. shadow maps */
    if (u.sun[3] > 0) for (int cas = 0; cas < 2; cas++) {
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.depthAttachment.texture = cas ? t_sm2 : t_sm;
        rp.depthAttachment.loadAction = MTLLoadActionClear;
        rp.depthAttachment.clearDepth = 1.0;
        rp.depthAttachment.storeAction = MTLStoreActionStore;
        enc = [cb renderCommandEncoderWithDescriptor:rp];
        [enc setRenderPipelineState:p_sh];
        [enc setDepthStencilState:ds_sh];
        [enc setCullMode:MTLCullModeNone];
        [enc setDepthBias:2.0f slopeScale:3.0f clamp:0.02f];
        for (i = 0; i < g_nrec; i++) {
            shrec *r = &g_rec[i];
            memcpy(r->s.svp, cas ? u.svp2 : u.svp, sizeof u.svp);
            [enc setVertexBuffer:r->buf offset:r->off atIndex:0];
            [enc setVertexBytes:&r->s length:sizeof r->s atIndex:1];
            [enc setFragmentBytes:&r->s length:sizeof r->s atIndex:0];
            if (r->tex) [enc setFragmentTexture:r->tex atIndex:0];
            if (r->smp) [enc setFragmentSamplerState:r->smp atIndex:0];
            [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)r->n];
        }
        [enc endEncoding];
    }
    /* 2. occlusion + bounce, blurred */
    /* 1b. smoothed normals: every pass after this reads them */
    if (!getenv("BR_FX_FLAT")) { quad(pass(cb, t_nrm, 0), p_nrm, &u, @[gn, gp]); gn = t_nrm; }
    quad(pass(cb, t_ao, 0), p_ao, &u, @[gn, gp, col]);
    {
        float d1[2] = { 1, 0 }, d2[2] = { 0, 1 };
        enc = pass(cb, t_ao2, 0);
        [enc setFragmentBytes:d1 length:8 atIndex:1];
        quad(enc, p_blur, &u, @[t_ao, gp, gn]);
        enc = pass(cb, t_ao, 0);
        [enc setFragmentBytes:d2 length:8 atIndex:1];
        quad(enc, p_blur, &u, @[t_ao2, gp, gn]);
    }
    /* 3. reflections, blurred like the occlusion; dry roads get none */
    if (u.p0[3] <= 0) [pass(cb, t_ssr, 0) endEncoding];
    else {
    quad(pass(cb, t_ssr, 0), p_ssr, &u, @[gn, gp, col]);
        float d1[2] = { 1, 0 }, d2[2] = { 0, 1 };
        enc = pass(cb, t_ao2, 0);
        [enc setFragmentBytes:d1 length:8 atIndex:1];
        quad(enc, p_blur, &u, @[t_ssr, gp, gn]);
        enc = pass(cb, t_ssr, 0);
        [enc setFragmentBytes:d2 length:8 atIndex:1];
        quad(enc, p_blur, &u, @[t_ao2, gp, gn]);
    }
    /* 3b. this frame's spray and track samples, and the track mask */
    {
        int w = (int)H32(0x104B15E8u), nv = 0;
        id<MTLBuffer> tb;
        if (getenv("BR_FX_WEATHER")) w = atoi(getenv("BR_FX_WEATHER"));
        fx_sim(w, u.p0[3], 1.0f / 60.0f);
        tb = fx_tracks(dev, u.eye, &nv);
        {
            MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
            rp.colorAttachments[0].texture = t_trk;
            rp.colorAttachments[0].loadAction = MTLLoadActionClear;
            rp.colorAttachments[0].clearColor = MTLClearColorMake(0, -10000, 0, 0);
            rp.colorAttachments[0].storeAction = MTLStoreActionStore;
            enc = [cb renderCommandEncoderWithDescriptor:rp];
            if (tb) {
                if (!g_keep) g_keep = [NSMutableArray new];
                [g_keep addObject:tb];
                [enc setRenderPipelineState:p_trk];
                [enc setVertexBuffer:tb offset:0 atIndex:0];
                [enc setVertexBytes:&u length:sizeof u atIndex:1];
                [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)nv];
            }
            [enc endEncoding];
        }
    }
    /* 4. lighting */
    enc = pass(cb, t_hdr, 0);
    [enc setFragmentBytes:&hl length:sizeof hl atIndex:1];
    quad(enc, p_comp, &u, @[gn, gp, col, t_sm, t_ao, t_ssr, g_skyt ? g_skyt : col, t_trk, t_sm2]);
    /* 4b. particles and falling rain/snow over the lit scene */
    if (!getenv("BR_FX_NOPFX")) {
        pfu *pu = calloc(1, sizeof *pu);
        id<MTLBuffer> pb;
        pu->misc[0] = (float)fx_particles(pu, u.eye, (int)H32(0x104B15E8u));
        for (i = 0; i < 3; i++) pu->amb[i] = g_car_rig[2][i] * 0.8f + g_car_rig[3][i] * 0.4f;
        pb = [dev newBufferWithBytes:pu length:sizeof *pu options:MTLResourceStorageModeShared];
        free(pu);
        if (!g_keep) g_keep = [NSMutableArray new];
        [g_keep addObject:pb];
        {
            enc = pass(cb, t_pfx, 0);
            [enc setFragmentBytes:&hl length:sizeof hl atIndex:1];
            [enc setFragmentBuffer:pb offset:0 atIndex:2];
            quad(enc, p_pfx, &u, @[gp]);
            quad(pass(cb, t_hdr, 1), p_pfxc, NULL, @[t_pfx]);
        }
    }
    /* 4c. temporal anti-aliasing, then motion blur */
    {
        mvu *mv = calloc(1, sizeof *mv);
        int c, k, nm = 0;
        for (c = 0; c < g_ncars && nm < 16; c++) {
            int j2;
            for (j2 = 0; j2 < g_nprev && g_prevcar[j2] != g_carptr[c]; j2++) ;
            if (j2 == g_nprev) continue;
            for (k = 0; k < 4; k++) {
                int r;
                for (r = 0; r < 3; r++) { mv->cur[nm][k][r] = g_cars[c][k * 4 + r]; mv->prev[nm][k][r] = g_prevmat[j2][k * 4 + r]; }
                if (k < 3) {       /* unit rows */
                    float l1 = sqrtf(mv->cur[nm][k][0] * mv->cur[nm][k][0] + mv->cur[nm][k][1] * mv->cur[nm][k][1] + mv->cur[nm][k][2] * mv->cur[nm][k][2]);
                    float l2 = sqrtf(mv->prev[nm][k][0] * mv->prev[nm][k][0] + mv->prev[nm][k][1] * mv->prev[nm][k][1] + mv->prev[nm][k][2] * mv->prev[nm][k][2]);
                    for (r = 0; r < 3; r++) { mv->cur[nm][k][r] /= l1 > 0 ? l1 : 1; mv->prev[nm][k][r] /= l2 > 0 ? l2 : 1; }
                }
            }
            nm++;
        }
        mv->misc[0] = (float)nm;
        {
            id<MTLBuffer> mb = [dev newBufferWithBytes:mv length:sizeof *mv options:MTLResourceStorageModeShared];
            free(mv);
            if (!g_keep) g_keep = [NSMutableArray new];
            [g_keep addObject:mb];
            if (taa_on()) {
                id<MTLTexture> dst = t_hist[g_hist_i], his = t_hist[g_hist_i ^ 1];
                enc = pass(cb, dst, 0);
                [enc setFragmentBuffer:mb offset:0 atIndex:1];
                quad(enc, p_taa, &u, @[t_hdr, his, gp]);
                g_hist_i ^= 1; g_hist_ok = 1;
                enc = pass(cb, t_hdr, 0);
                [enc setFragmentBuffer:mb offset:0 atIndex:1];
                if (getenv("BR_FX_NOMB")) u.jit[2] = 0;       /* shutter 0: a copy */
                quad(enc, p_mb, &u, @[dst, gp]);
            }
        }
        /* remember this frame for the next */
        memcpy(g_pvp, g_P, sizeof g_pvp); g_pvp_ok = 1;
        g_nprev = g_ncars;
        for (c = 0; c < g_ncars; c++) { g_prevcar[c] = g_carptr[c]; memcpy(g_prevmat[c], g_cars[c], sizeof g_prevmat[c]); }
        next_jitter();
    }
    /* 5. bloom */
    quad(pass(cb, t_bl[0], 0), p_pre, &u, @[t_hdr]);
    for (i = 1; i < 6; i++) quad(pass(cb, t_bl[i], 0), p_down, NULL, @[t_bl[i - 1]]);
    for (i = 4; i >= 0; i--) quad(pass(cb, t_bl[i], 1), p_up, NULL, @[t_bl[i + 1]]);
    /* 6. shafts */
    if (u.p2[0] > 0 || (hl.misc[0] > 0 && hl.misc[1] > 0)) {
        enc = pass(cb, t_shaft, 0);
        [enc setFragmentBytes:&hl length:sizeof hl atIndex:1];
        quad(enc, p_shaft, &u, @[gp, col]);
    }
    else [pass(cb, t_shaft, 0) endEncoding];
    /* 7. tone map and grade; with anti-aliasing (BR_FX_AA=0 leaves it out,
     * the debug views skip it) the scene is graded alone, anti-aliased, and
     * the game's 2D laid over it afterwards */
    if ((!getenv("BR_FX_AA") || atoi(getenv("BR_FX_AA"))) && u.p3[1] < 0.5 && !taa_on()) {
        quad(pass(cb, t_aa, 0), p_scene, &u, @[t_hdr, col, t_bl[0], t_shaft]);
        quad(pass(cb, t_out, 0), p_aa, NULL, @[t_aa, t_hdr]);
        quad(pass(cb, t_aa, 0), p_mix, NULL, @[t_out, t_hdr, col]);
        aa = 1;
    } else
        quad(pass(cb, t_out, 0), p_fin, &u, @[t_hdr, col, t_bl[0], t_shaft]);

    /* keep this frame's shadow inputs alive until the GPU is done */
    {
        NSArray *keep = [g_keep copy];
        int stat = getenv("BR_FX_STAT") != NULL;
        [cb addCompletedHandler:^(id<MTLCommandBuffer> b) {
            static double acc; static int n;
            (void)keep;
            if (!stat) return;
            acc += (b.GPUEndTime - b.GPUStartTime) * 1000.0;
            if (++n == 240) { fprintf(stderr, "fx: gpu %.2f ms per frame (last 240, whole frame)\n", acc / n); acc = 0; n = 0; }
        }];
    }
    [g_keep removeAllObjects];
    g_nrec = 0;
    g_havecam = 0;
    g_ncars = 0;
    (void)rdf;
    return aa ? t_aa : t_out;
}
