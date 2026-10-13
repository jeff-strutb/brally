/* brr_metal_night.h: the eye at night, in the post pass (brr_metal_lit.h
 * pfs): the Purkinje shift as a display-referred grade.  The rods' share w
 * of the pixel's scene luminance L (cd/m2, the exposure divided back out,
 * before the tonemap) is light_night.h's light_purkinje_w, the same numbers:
 * 0 at 3 cd/m2 and up, 1 at 0.003 and under, a smoothstep in log L between.
 * After the tonemap the colour goes toward its own luminance carried by
 * light_night.h's night blue (LIGHT_PURK_BLUE, Forza Horizon 6's measured
 * 0.45 : 1 : 2.4, of luminance 1) by w: the same hue at every level, as the
 * reference keeps it from its darkest ground (0.008) to its clouds (0.09);
 * a pool lit at a few cd/m2 stays neutral.  Done before the tonemap, the
 * blue came from PBR Neutral's toe and held only under display 0.03. */
#ifndef BRR_METAL_NIGHT_H
#define BRR_METAL_NIGHT_H

#define BRR_NIGHT_SRC \
"static float purkinje_w(float L) {\n" \
"  float t = saturate((log10(max(L, 1e-9)) - log10(0.003)) / (log10(3.0) - log10(0.003)));\n" \
"  return 1.0 - t * t * (3.0 - 2.0 * t); }\n" \
"static float3 purkinje(float3 c, float w) {   /* c: display linear, after the tonemap; w: purkinje_w of its scene luminance */\n" \
"  const float3 blue = float3(0.45, 1.00, 2.40) / dot(float3(0.45, 1.00, 2.40), float3(0.2126, 0.7152, 0.0722));\n" \
"  return saturate(mix(c, dot(c, float3(0.2126, 0.7152, 0.0722)) * blue, w)); }\n"

#endif
