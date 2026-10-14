// kernels.metal: the verification suite's test kernels.  lighttest.m appends
// this file to the renderer's own shader source (k_src, BRR_LIGHTS_SRC,
// BRR_LIT_SRC and BRR_VOL_SRC, exactly as brr_open compiles them) and
// compiles the whole as one library, so every kernel below calls the
// renderer's functions as they are: brdf, lamp, lin, srgb1, delit, neutral,
// radiance, fogged.  Nothing here re-implements them.

struct TBrdf { float4 n; float4 v; float4 l; float4 p; };   // p.x albedo, p.y roughness
kernel void t_brdf(const device TBrdf *in [[buffer(0)]], device float4 *out [[buffer(1)]], texture2d<float> bE [[texture(0)]],
                   uint i [[thread_position_in_grid]]) {
  TBrdf b = in[i];
  out[i] = float4(brdf(b.n.xyz, b.v.xyz, b.l.xyz, float3(b.p.x), b.p.y, bE), 0.0);
}

// the directional albedo of brdf for a view at cosine mu: one stratified row
// of an N x N grid per thread.  x: the specular part (albedo 0), GGX
// importance sampled (h from D(h)(n.h), l the reflection of v; the pdf is
// computed with 1 - (n.h)^2 in closed form so it stays exact for very smooth
// lobes); y: the same with the Fresnel term divided out (the lobe's own
// energy); z: the diffuse part (albedo 1 minus albedo 0), cosine sampled.
kernel void t_furnace(constant float *rough [[buffer(0)]], constant float *muv [[buffer(1)]], constant uint4 &P [[buffer(2)]],
                      device float4 *out [[buffer(3)]], texture2d<float> bE [[texture(0)]], uint3 g [[thread_position_in_grid]]) {
  uint N = P.x, j = g.x, im = g.y, ir = g.z;
  if (j >= N || im >= P.y || ir >= P.z) return;
  float r = rough[ir], mu = muv[im];
  float3 n = float3(0, 0, 1), v = float3(sqrt(max(1.0 - mu * mu, 0.0)), 0.0, mu);
  float a = max(r * r, 2e-3), a2 = a * a;
  float es = 0.0, el = 0.0, ed = 0.0;
  float u1 = (float(j) + 0.5) / float(N);
  float den = 1.0 + (a2 - 1.0) * u1, ch2 = (1.0 - u1) / den, sh2 = a2 * u1 / den;
  float ch = sqrt(ch2), sh = sqrt(sh2);
  float dd = ch2 * a2 + sh2, D = a2 / (M_PI_F * dd * dd);
  float cl = sqrt(1.0 - u1), sl = sqrt(u1);
  for (uint i = 0; i < N; i++) {
    float ph = 2.0 * M_PI_F * (float(i) + 0.5) / float(N), cp = cos(ph), sp = sin(ph);
    float3 h = float3(sh * cp, sh * sp, ch);
    float vh = dot(v, h);
    float3 l = 2.0 * vh * h - v;
    if (l.z > 0.0 && vh > 0.0) {
      float pdf = D * ch / (4.0 * vh);
      float f = brdf(n, v, l, float3(0.0), r, bE).x;
      float F = 0.04 + 0.96 * pow(1.0 - vh, 5.0);
      es += f / pdf;
      el += f / F / pdf;
    }
    float3 ld = float3(sl * cp, sl * sp, cl);
    float fd = brdf(n, v, ld, float3(1.0), r, bE).x - brdf(n, v, ld, float3(0.0), r, bE).x;
    ed += fd / (cl / M_PI_F);
  }
  out[(ir * P.y + im) * N + j] = float4(es, el, ed, 0.0) / float(N);
}

// the BRDF table as brdf reads it: x E(mu), y E_avg, for (mu, roughness)
kernel void t_btab(texture2d<float> bE [[texture(0)]], const device float2 *q [[buffer(0)]], device float2 *o [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  constexpr sampler s(filter::linear, address::clamp_to_edge); float2 sz = float2(bE.get_width(), bE.get_height());
  o[i] = bE.sample(s, (0.5 + float2(sqrt(q[i].x), saturate((q[i].y - 0.0447) / 0.9553)) * (sz - 1.0)) / sz).rg;
}

// a lamp's candela toward d as the lighting applies it: lamp() through its
// window lwin() (the outer cone), at 1 m, well inside its range
kernel void t_lamp(constant LS &l [[buffer(0)]], const device float4 *d [[buffer(1)]], device float *o [[buffer(2)]], uint i [[thread_position_in_grid]]) {
  o[i] = lamp(l, d[i].xyz) * lwin(l, d[i].xyz, 1.0);
}

// x -> lin(x) (one channel), srgb1(x)
kernel void t_srgb(const device float *x [[buffer(0)]], device float2 *o [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  o[i] = float2(lin(float3(x[i])).x, srgb1(x[i]));
}

kernel void t_neutral(const device float4 *c [[buffer(0)]], device float4 *o [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  o[i] = float4(neutral(c[i].rgb), 0.0);
}

// in[2i] the texture colour, in[2i+1] the vertex colour
kernel void t_delit(const device float4 *in [[buffer(0)]], device float4 *o [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  o[i] = float4(delit(in[2 * i].rgb, in[2 * i + 1].rgb), 0.0);
}

// the light leaving a surface as the deferred pass computes it: the normal to
// the eye, the sun's soft-shadowed visibility, radiance (sun, sky, the
// cluster's lamps through their atlas, the sky's specular cube sp, no
// occlusion), pre-exposure and the fog over it
struct TRad { float4 p; float4 n; float4 alb; float4 fog; };   // alb.w roughness, fog.x amount, fog.y glow
kernel void t_radiance(constant LU &u [[buffer(0)]], const device LSet &ls [[buffer(1)]], depth2d_array<float> sm [[texture(0)]],
                       const device TRad *in [[buffer(2)]], device float4 *o [[buffer(3)]], texture2d<float> bE [[texture(1)]],
                       depth2d<float> atl [[texture(2)]], texturecube<float> sp [[texture(3)]], uint i [[thread_position_in_grid]]) {
  TRad t = in[i];
  float3 n = toeye(u, t.p.xyz, t.n.xyz);
  float sv = dot(n, u.sun.xyz) > 0.0 ? sunvis(u, sm, t.p.xyz, n, float(i % 64) * 0.098) : 0.0;
  float3 c = radiance(u, ls, atl, t.p.xyz, n, t.alb.rgb, t.alb.w, t.fog.y, sv, bE, float3(0), float(i % 64) * 0.098, sp, 1.0);
  o[i] = float4(fogged(u, preexp(u, c), t.fog.x, t.alb.rgb), 0.0);
}

// a value written to a half-float target, as the HDR target stores it
kernel void t_half(const device float4 *x [[buffer(0)]], device half4 *o [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  o[i] = half4(x[i]);
}

// the face normal from the world position's screen derivatives: mode 0 a
// constant position (no face), 1 the xy plane, 2 the xz plane
fragment float4 t_facen(LO in [[stage_in]], constant int &mode [[buffer(0)]]) {
  float3 wp = mode == 0 ? float3(1.0, 2.0, 3.0) : mode == 1 ? float3(in.pos.x, in.pos.y, 0.0) : float3(in.pos.x * 0.5, 7.0, in.pos.y * 3.0);
  return float4(facen(wp), 1.0);
}
