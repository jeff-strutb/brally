# Night: design and targets

The Remastered night is built to the Forza Horizon night structure: a dark,
saturated blue moonlit ambient, a dark blue sky with moonlit clouds, and
neutral artificial lights 5 to 6 stops above the ambient that carry the
image.  Targets are measured from Forza Horizon 6 frames (Steam community
top-rated screenshots, app 2483190, and a reference frame), sampled in
display-linear luminance Y and HSV on 13 x 13 px medians.

## Measured targets (display-linear Y; sRGB in brackets)

| element | Forza Horizon 6 | Remastered at 5b404d62 |
|---|---|---|
| moonlit ground, road outside lamp light | 0.008 to 0.03 (12 to 50), hue 213 to 217, saturation 0.4 to 0.7 | 0.07 to 0.08, saturation 0.08 to 0.17 |
| sky | 0.015 to 0.057, hue 216 to 223, saturation 0.4 to 0.8 | 0.17, saturation 0.11 |
| clouds | 2 to 3 x the sky, same hue, shaped by the moon | flat beige painted shapes |
| headlamp pool on the road | 0.55 to 0.88, near neutral | absent past the bumper |
| road 30 to 50 m ahead in the beam | about 0.12 | dark |
| frame median (sRGB) | 15 to 50 | 78 |
| pixels under 30/255 | 35 to 70 % | 3 to 11 % |

The dark blue is consistent across frames: linear R : G : B about
0.45 : 1 : 2.4 for scotopic-range pixels; lit pixels neutral.

## Design

1. Exposure.  Physical EV100 from the moonlit ambient (as now), with a night
   key that puts a moonlit surface at display Y 0.01 to 0.03, not a day's
   mid-grey: night stays dark, as Gran Turismo 7's adaptation limit and
   Unreal's exposure-compensation curve do.  Local exposure compresses
   highlights only (Unreal: highlight contrast below 1, shadow contrast 1).
2. Night colour.  A luminance-dependent scotopic shift toward the measured
   blue (0.45 : 1 : 2.4), full in the moonlit range and none at lamp levels,
   after Filament's scotopicAdaptation (Cao et al. 2008, Kirk and O'Brien
   2011): the ambient goes blue, headlamps and windows stay neutral.  The
   moon's own light stays physical (about 4,100 K); the blue is perception.
3. Headlamps.  Road luminance under a headlamp is retroreflection (CIE 144,
   coefficient of retroreflected luminance RL), not Lambertian reflection at
   grazing incidence (which leaves a low beam at 0.05 lx horizontal at 50 m,
   about twice the moonlight).  The player's lamps add L = RL E_perp toward
   an eye near the lamp, E_perp the illuminance normal to the beam.  RL is
   set so the pool and the 30 to 50 m road meet the measured 0.55 to 0.88 and
   about 0.12 at the night exposure.  Beam: an R112-like candela table (cut
   off 0.57 degrees down, hot spot 20 to 30 kcd right of centre), no range
   window short of 150 m.  The car's own body is excluded from its lamps'
   shadows.
4. Sky.  The atmosphere driven by the moon (single scattering, as Unreal's
   second light), moonlit clear sky about 0.01 to 0.02 cd/m2 (Unreal: moon
   0.26 lx at 0.568 degrees; Jensen et al. 2001).  Stars as single-pixel
   points times the air's transmittance.
5. Clouds.  The painted backdrop's clouds are re-lit as albedo by the moon
   and the moonlit sky (about 3 to 5 x the sky's luminance, the sky's blue),
   their mask occluding stars and the moon disc.
6. Moon.  Disc at physical luminance (about 3,400 cd/m2), a bounded aureole
   drawn analytically (Unity HDRP PhysicallyBasedSky's flare zone), excluded
   from bloom, no ghosts.
7. Lighthouse beam.  Analytic in-scattering in the cone (Macklin 2010; Sun et
   al. 2005 with extinction), integrated pre-exposed, bounded so the beam
   reads as Forza's searchlight beams (display Y 0.1 to 0.3), not a white
   band.
8. Tonemap.  Hue-preserving (Khronos PBR Neutral, as now), so the blue and
   red tail lamps keep their hue.

## Verification

Each element is checked in the game's own frames (headless runs, named
shots) at the same sample points as the reference, against the table above;
the frame statistics (median, dark fraction, saturation) against the
reference frames' range.

## Results (the game's own frames, Coastline night, 1280 x 720)

| element | Forza Horizon 6 | Remastered now |
|---|---|---|
| moonlit hills, walls, sea | 0.008 to 0.03, hue 213 to 217 | 0.009 to 0.022, hue 214 to 220, saturation 0.6 |
| sky | 0.015 to 0.057 | 0.013 to 0.026 |
| clouds | 2 to 3 x the sky | 0.8 to 1.4 x the sky (the moon 11 degrees up: a cumulus base gets sin 11 of its light) |
| headlamp pool, near road | 0.55 to 0.88, near neutral | 0.78 to 0.83, hue 222 to 227, saturation 0.1 |

Clouds: volumetric cumulus (brr_metal_clouds.h) in place of the 2D slab.
Albedo: each tagged texture's mean set to its material's (texmat.csv).

Open: the bumper camera fills the lower frame with the beam's hottest
metres over a 32 x 32 road texture (an asset limit); cloud interiors read
flat where the diffusion light is uniform; the clouds cost about 1.5 to
2.5 ms at 1280 x 720 on an M4 Pro.
