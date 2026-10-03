// The draw's Glide state (brr_state), one std140 block: the C side is the
// same struct as the Metal renderer's U (brr_vulkan.c).
layout(std140, set = 0, binding = 0) uniform U {
    int cc_fn, cc_factor, cc_local, cc_other, cc_invert;
    int ac_fn, ac_factor, ac_local, ac_other, ac_invert;
    int tc_rgb_fn, tc_rgb_factor, tc_alpha_fn, tc_alpha_factor, tc_rgb_invert, tc_alpha_invert;
    int has_tex, atest_fn, fog_mode, pad;
    float atest_ref, vw, vh, pad2;
    vec4 konst, fog_color;
    vec4 fog[16];                 // the 64-entry fog table, four to a vec4
} u;
