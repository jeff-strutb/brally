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
