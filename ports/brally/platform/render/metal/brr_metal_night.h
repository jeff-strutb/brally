/* brr_metal_night.h: the eye at night, in the post pass before PBR Neutral
 * (brr_metal_lit.h pfs): the Purkinje shift.  The rods' share w of the
 * pixel's luminance L (cd/m2, the exposure divided back out) is
 * light_night.h's light_purkinje_w, the same numbers: 0 at 3 cd/m2 and up, 1
 * at 0.001 and under, a smoothstep in log L between.  The colour goes toward
 * the scotopic luminance V = Y (1.33 (1 + (Y + Z) / X) - 1.68) (Jensen et al.
 * 2000), relative to a D65 white's, carried by a blue of luminance 1.  c is
 * pre-exposed linear sRGB; ex the exposure. */
#ifndef BRR_METAL_NIGHT_H
#define BRR_METAL_NIGHT_H

#define BRR_NIGHT_SRC \
"static float purkinje_w(float L) {\n" \
"  float t = saturate((log10(max(L, 1e-9)) + 3.0) / (log10(3.0) + 3.0));\n" \
"  return 1.0 - t * t * (3.0 - 2.0 * t); }\n" \
"static float3 purkinje(float3 c, float ex) {\n" \
"  float X = dot(c, float3(0.4124, 0.3576, 0.1805)), Y = dot(c, float3(0.2126, 0.7152, 0.0722)), Z = dot(c, float3(0.0193, 0.1192, 0.9505));\n" \
"  float w = purkinje_w(Y / max(ex, 1e-12));\n" \
"  if (w <= 0.0 || X <= 1e-12) return c;\n" \
"  float V = max(Y * (1.33 * (1.0 + (Y + Z) / X) - 1.68), 0.0) / 2.5731;   /* a white (D65) pixel keeps its luminance */\n" \
"  const float3 blue = float3(0.62, 0.92, 1.80) / dot(float3(0.62, 0.92, 1.80), float3(0.2126, 0.7152, 0.0722));\n" \
"  return mix(c, V * blue, w); }\n"

#endif
