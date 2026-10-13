# Reflections (brr_metal_refl.h)

Grain on road and wall reflections, causes:

- one GGX sample per half-resolution pixel, the same pattern every frame, and
  nothing averaging it over time or space;
- BVH hits shaded as flat grey ground (albedo 0.3, no shadow, no normal);
- the half-resolution target brought up by a 2 x 2 bilinear upsample whatever
  the roughness.

Fix: the sample rotates each frame (mirror direction below roughness 0.1);
reflt accumulates into a half-size history (reprojected through the reflected
point for mirrors, the surface for rough lobes, clamped to the 3 x 3 box of
new samples, weight 1/n up to 16); reflc filters it cross-bilaterally with a
Gaussian growing with roughness (none below 0.1, radius 4 half-res pixels at
0.45). BVH hits read the last frame's lit target when on screen and still
visible, else albedo 0.3 under the sun with a shadow ray and the sky's
hemisphere about the triangle's normal.

refl.sh, constant environment, +-35% samples, 16 frames, roughness 0.3:
mean 0.9958 of the split-sum value, per-pixel deviation 0.92%, no bleed
across depth edges.

Open: GPU cost at 1920x1080 not yet measured in a harness; the filter radius
does not yet grow with hit distance; off-screen hits use the sky hemisphere,
not the GI probes; no disocclusion test beyond the history clamp.

## Sun glare and lens flare (2026-10-13)

Problem: the sun's disc is a few pixels, written pre-exposed and clamped at
60000, and bloom is 4% of the post's mix, so the sun showed no glare.

Fix (brr_metal_glare.h, in pfs): the glare is computed from the light's
screen position and its unclipped energy E (its illuminance at the camera,
LU.sunc or at night the moon's lux, times the sky's scale and the exposure,
over one pixel's solid angle there). Veiling glare: 4% of E spread by a PSF
of four normalised Gaussians (2.5, 8, 24, 64 thousandths of the frame's
height; weights 0.55, 0.25, 0.13, 0.07). Flare: a chromatic halo, five
ghosts along the line through the centre with per-channel radii, a 6-spike
starburst. All scaled by visibility (16 G-buffer alpha taps about the light),
the C cloud density's transmittance toward it, and the air's (in sunc); none
behind the camera or more than 0.6 frame heights off it. BR_LIGHT_NOGLARE
turns it off. LU unchanged: the constants go in a 48-byte GLU at buffer 2.

glare_test.c (skymask.sh): PSF integral 1.00015 radially, 1.00000 summed on
a 640 x 480 pixel grid (veiling energy 0.0400 of E); projection round trip
to 0.01 px; zero behind the camera, far off frame, and with every tap
covered; visibility monotonic over 0..16 covered taps; cloud transmittance
falls with extinction (0.44 to 0.0003). maths.sh 75 PASS with the offline
compiles (macOS, iOS); shadow.sh all pass.

Open: the painted backdrop's hills are not in the G-buffer, so they do not
occlude the glare; flare strengths (halo 2e-4, ghosts 1.5e-4, starburst
6e-4 of E) are set by eye from lens references, not measured.
