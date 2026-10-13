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

# Backdrop sky mask on the game's own textures (brr_skymask.h)

Coastline's backdrop, dumped as drawn (sunny and fog races: the same 13
textures of 64 x 64 and two 4 x 1 fills). Every texture is the picture over
its mirror image (row y equals row 63 - y, exactly); the lower half is the
upright picture: its top row is the 4 x 1 zenith fill colour (4a626a), the
hills' flat bottoms sit on the sea there. Three kinds: sky (6, 8, 11 to 14:
the zenith-to-horizon gradient 4a626a to 5a6a73, small clouds), the
horizon band (9, 10, 22, 23: hills or islands; 21, 24: haze over the sea)
and the sea (7: flat 4a5a62 with glints).

Before, scanning the whole texture from row 0 (the mirrored, upside-down
picture): 21, 24 and the sea tile 7 all sky (1.000), so the sea under the
hills showed the physical sky; 9, 10, 22, 23 cut in the upper half at the
sea's edge (sky 0.06 to 0.32), their real strip of sky scenery.

Fix: a mirrored texture is analysed by its lower half and the mask
mirrored; a soft edge (one 5-bit step a row, every channel darker by 0.06
over 3 rows, the 10 rows under it level within 0.035) starts scenery, which
finds haze going down into the sea; a texture with no edge and no gradient
(row means within 0.03) is a flat panel, scenery.

skymask.sh against the classification read from the texels (column 20, the
first row of hill paint or sea under the sky's gradient), both races: all
26 pass, worst skyline error 1 row (21, 24: 47 against 48, the haze's last
row 5a626a); sky 1.000 on the sky tiles but 12 (0.990: a cloud at the
edge notches one column), 0.000 on the sea, 0.09 / 0.23 / 0.43 / 0.27 on
9 / 10 / 22 / 23, 0.47 on 21 and 24. Synthetic panoramas unchanged
(skyline within 1 texel 0.998, clouds all sky, hills and sea none).

Open: the panorama's tile layout (which K_BACK quad uses which half) was
read from the texels, not from the op recording.
