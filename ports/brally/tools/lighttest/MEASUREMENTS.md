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
