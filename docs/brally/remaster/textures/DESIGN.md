# Remastered textures

Top Gear Rally Remastered's Boss Rally races with every natural and built
surface in a photographed material at its real size, to the standard of Forza
Horizon 6. A Remaster Options row (Textures: Original or Remaster) switches it
live, under either lighting, on every renderer (Metal on the Mac and the
iPhone, Vulkan and software on Windows, Vulkan on Linux and the Mac); the game
never sees it.

## 1. What the game gives us

The tracks' textures are 1999's: 16 x 16 to 128 x 128 texels, most 32 x 32 or
64 x 64, tiled along the road and up the walls. `texmat.csv` already names
each one's material (2,524 textures by the FNV-1a hash of their texels; 24
materials). Dumped from a full lap of each track (`BR_LIGHT_TEXOUT`), they
show that most textures are not one surface but several:

- a road: asphalt with its painted edge lines, centre line, a grass or dirt verge
- a beach: sand, a crest of dune grass, the foam line, the sea
- a cliff: rock with a band of grass along its top
- a wall: plaster, its windows, doors, trim and bricks drawn into it

So one material per texture (a texture's mean colour times a detail map)
cannot be right: paint would be asphalt-grained, the beach's grass sand.

## 2. Forza Horizon 6, measured

The reference is every player screenshot on the game's Steam page
(steamcommunity.com/app/2483190/screenshots, both the most popular and the
most recent listings, 152 pages): **22,510** images at 640 pixels wide.

- Each classified on the device by macOS Vision's scene classifier
  (`VNClassifyImageRequest`): road 10,873, grass 4,512, snow 1,427,
  rocks 1,081, building 1,329, dirt road 224, beach 158 (confidence 0.3 or more).
- From each surface's images, 96-pixel patches from the lower 45% of the
  frame (the ground): 33,486 patches. Kept: those even across their four
  quarters (L* standard deviation under 4) and of their surface's colour (a
  gate in L*, chroma and hue per surface). Reviewed by eye on sheets of 160.

| surface | patches kept | L* p10 / p50 / p90 | chroma p50 | Lab hue p50 | detail (1 px) p50 |
|---|---:|---|---:|---:|---:|
| asphalt | 4,097 | 15.7 / 29.6 / 50.5 | 4.4 | 88 | 0.139 |
| dirt | 241 | 29.1 / 43.3 / 61.0 | 15.2 | 69 | 0.172 |
| sand | 85 | 49.9 / 60.7 / 73.5 | 19.8 | 75 | 0.077 |
| snow | 141 | 72.5 / 81.6 / 91.0 | 4.2 | | 0.027 |
| grass | 546 | 24.9 / 36.7 / 52.5 | 22.4 | 106 | 0.201 |
| rock | 934 | 27.7 / 40.3 / 61.6 | 8.7 | 83 | 0.186 |

Detail: the standard deviation of luminance less its 3 x 3 mean, over the
mean (relative fine contrast at a chase camera's distances). What the sheets
show: asphalt an even, nearly neutral dark grey, its aggregate visible near
the car, markings crisp (white, and orange-yellow centre lines); grass an
olive yellow-green, never emerald; sand pale and smooth; snow very smooth and
near white; rock grey-brown with strong relief.

## 3. How AAA games texture terrain (sources)

- Forza Horizon 5: ground, road, architecture and geology from tileable
  photogrammetry-derived materials (Substance Designer); road wetness from a
  height map, road mask and flow map (Adobe, "Forza Horizon 5: crafting rich
  and diverse Mexican biomes").
- Texel density: about 1,000 px/m first-person and 512 px/m third-person on
  console (Far Cry 4's adaptive virtual texture: 10 texels/cm, GDC 2015).
  A chase camera near the road: about 1,000 px/m at the car.
- Repetition: hex-tiling, three samples on a triangle grid moved and turned
  at random, weights sharpened (Mikkelsen 2022, JCGT 11(3); after Heitz and
  Neyret 2018). Triplanar projection for cliffs, weights |n|^k.
- Borders between materials: height blends (Ghost Recon Wildlands' terrain,
  GDC 2017): the material whose height is greater at a texel wins near the border.
- Normal mapping onto a projection: UDN or whiteout blends (Golus 2017).
- Real albedos (linear): asphalt 0.04 to 0.18, soil 0.15 to 0.30, sand 0.40,
  grass 0.20 to 0.25, rock 0.30 to 0.40, fresh snow 0.70 to 0.90, white road
  paint about 0.8 (rFactor 2's material chart).

## 4. Design

**Surface maps** (`platform/light/light_surf.c`, the reference
`ports/remastered/tools/texregion.py`). Made when the game uploads a texture,
off the render thread, kept by hash for the run (nothing derived from the
game's data ships):

1. Each texel's surface from its colour (HSV) in its material's context. Natural
   ground is one base surface for the whole texture, chosen by its mean colour
   (rock: red sandstone, brown or grey; dirt: red, brown or gravel), and over it
   only what is plainly something else: road paint (relative to the texture's
   own median brightness and saturation), grass on rock or sand, water and foam.
   A layout (buildings, signs, wood, metal) is its colours' fewest k-means
   clusters (at most 8, mean error under 0.035), each a surface by its colour.
2. The 15/16-bit dither's lone texels voted out (3 x 3, wrapping); a texel-wide
   painted line or foam line kept where it runs on.
3. Each region's field upsampled 4x (Catmull-Rom, wrapping, a slight blur),
   stored as the two strongest regions and where the texel sits between them,
   so a border stays crisp under bilinear filtering at any magnification and
   runs smoothly instead of in texel steps.
4. Each region's mean colour, and its own light and dark by normalised
   convolution (no colour bleeds across a border).

The C port and the reference agree on every texture dumped (1,018 maps): on
each, at most 0.1% of map texels (worst 0.09%) differ in the region drawn,
a region's weight or the variation by more than 1.5/255, all at three-region
junctions where two fields tie to the last bits. Exact ties (equidistant
colours in k-means) are broken the same way in both, toward the lower index.

**Photographed sets** (`ports/common/surfaces`: `surfaces.txt`, `fetch.py`,
`bake.py`; Poly Haven, CC0). Chosen against section 2: each candidate's Lab
mean and its detail measured at 64 px/m, the patch scale.

| surface | set | metres | detail at 64 px/m (FH6) |
|---|---|---:|---|
| asphalt, road paint | asphalt_pit_lane | 2.0 | 0.098 (0.139) |
| dirt | dry_ground_rocks | 4.0 | 0.205 (0.172) |
| red dirt | red_dirt_mud_01 | 1.5 | 0.171 (0.172) |
| gravel | rocky_trail | 2.0 | 0.129 |
| sand, foam | sand_01 | 1.5 | 0.073 (0.077) |
| grass, moss | leafy_grass | 2.0 | 0.118 (0.201 with 3D blades) |
| dry grass | withered_grass | 2.0 | 0.103 |
| grey / brown / red rock | rock_face_04 (x0.75) / rock_face_03 / cliff_side | 2.0 / 2.7 / 1.83 | 0.27 / 0.11 / 0.15 (0.186) |
| snow, ice | snow_02 | 2.0 | 0.032 (0.027) |
| concrete; fine grain under art | brushed_concrete | 2.5 | |
| plaster | painted_plaster_wall | 2.0 | |
| wood | weathered_planks | 2.0 | |

**Variety.** Forza's world is never one material end to end: a road is
freshly laid in one stretch, worn pale in the next, patched further on; a
field is lush, then sparse, then worn to gravel. So each surface has up to
three sets, laid over the ground in patches tens of metres across by a noise
field of its own (`surfset`): asphalt pit-lane (the usual), asphalt_track
(newer, 0.82 as bright), worn_asphalt (paler, 1.15) in 70 m patches; dirt
dry_ground_rocks, rocky_trail_02 and brown_mud_dry in 35 m; grass
leafy_grass, grass_ground and grass_path_2 in 25 m; two kinds of grey and of
brown rock; two sands, two gravels, two red dirts. Where two variants meet
the one whose height stands higher takes the ground (a band of 0.1 of the
field, moved by the heights' difference times 0.35), so they flow into each
other with no seam. Over all natural ground, 8% lighter and darker over some
120 m.

The first asphalt pick, clean_asphalt (chosen for its neutral colour), measured
0.028 and drew the road near the car flat (4% luminance variation where FH6's
is 14%); replaced.

Normals are kept as their x and y (RG8; the roughness is not read): 26 sets
at 2048 are 0.87 GB of GPU memory with their mips on the Mac, 0.22 GB at 1024
on the iPhone.

Baked: the albedo as detail only (each texel's linear colour over a wrapping
Gaussian of a sixteenth of the tile, so the scan's large blotches, which would
mark every repeat, are gone; outliers soft-limited at 2.5 sigma in log
luminance; mean exactly 1), the height, the normal's x and y (OpenGL). Raw
files (a one-line header, then the texels) that plain C reads on every
platform: photographed detail barely compresses, so PNG saved nothing. 2048
on the desktops (baked/, 624 MB), 1024 on the iPhone (baked1k/, 156 MB).

**One shading, every renderer.** The maps, the constants, the sets' texels
and their mip chains are portable C (`light_surf_draw.c`); the shading is
written once in C (`light_surf_shade.c`, the software renderer's own) and
line for line in Metal (`brr_metal_surf.h`) and Vulkan's GLSL
(`shaders/surf.glsl`). The noise is an integer hash, so the variants' patches
fall in the same places on every GPU. The world positions come from the
lighting's capture of the camera's triangles (glide.c), which now runs
whenever the textures are on.

- under the Remastered lighting (Metal): the G-buffer takes the surfaces as
  albedo and their relief in its normal
- under the game's own lighting (every renderer): the combiners read the
  surfaces as their texel; the face's normal from the world position's
  derivatives; the relief as a light from above and to one side (the
  direction the 1999 art paints), shading 1 + 0.8 n.l clamped to 0.6..1.4

**Drawing.** A draw whose texture's map is ready:

- the region pair at the texel (nearest) and where it sits (bilinear)
- each region's set projected in world space at its real size (1 unit is
  about a metre): from above, and from the two sides, weights |n|^4, a
  projection under 2% skipped; hex-tiled, three samples turned (ground) or
  only moved (planks, brushed concrete) at random, weights to the 8th power;
  explicit gradients turned with each sample, 16x anisotropic, trilinear
- albedo: the region's colour (its mean, scaled as the lighting scales the
  whole texture to its material's albedo, `lit_albk`; paint 0.70 white and
  0.45 yellow, snow 0.80, foam 0.60) x the set's detail x the region's own
  variation from the game texture (asphalt 15%: Forza's even roads, not the
  1999 streaks)
- the border: crisp at any magnification (its screen derivative); on natural
  ground moved by the two heights (weight 0.35) with a 0.06 softness
- relief: the set's normal along the projection's axes added to the shaded
  normal (UDN) at the surface's strength; mip averaging shortens it with
  distance, which is its fade

Untouched: effects (smoke, dust, sparks), sky (the lighting's skies),
lamps, water, foliage cards, cars, tyres, glass and openings in walls.

**The grade.** Each surface's region colour is scaled in luminance and in
Oklab chroma (hue kept) by factors measured, not chosen
(`ports/remastered/tools/lighttest/surfcalib.py`): headless frames of every
track in sun, the same frames again with `BR_SURF_VIEW=2 BR_LIGHT_VIEW=1`
(each surface's albedo in a colour of its own id, everything else black; its
hue read on linear values) as masks. Both measures leave the exposure out
(a first, absolute comparison asked for asphalt at 0.29 of its brightness,
which was only the lighting's exposure being brighter than the screenshots'):
a surface's luminance over its own frame's median, against Forza's patches
over their screenshots' (asphalt 1.04, dirt 1.12, grass 0.92, rock 1.01), and
its saturation as chroma over lightness (asphalt 0.16, dirt 0.35, grass 0.73,
rock 0.16). Under the Remastered lighting, from seven tracks:

| surface | luminance | chroma |
|---|---:|---:|
| asphalt | 1.00 | 1.00 (Forza's 3.4x more is a hue the art does not share, 143 against 59: more would bring back its mauve) |
| dirt, gravel | 1.10 | 0.45 |
| grass | 0.60 | 1.50 (the most either way: 1.52 measured) |
| brown and grey rock | 1.08 | 0.55 |
| red rock, red dirt | 0.83, 1.10 | 0.45 (a floor: Forza's rock is grey granite, and red sandstone stays red) |
| snow | 1.00 (compared at the upper quartile: snowy frames' medians are snow) | 0.40 |
| sand | 1.00 (too few pixels measured) | 1.00 |

Under the game's own lighting the same frames, the masks from the lit runs
(the game's frames are the same whatever the lighting), give their own
factors (`lum_o`, `chroma_o`).

## 5. Checks

- `ports/remastered/tools/lighttest/surfparity.sh DUMPDIR`: the C maps against the reference on every texture a run dumped (`BR_LIGHT_TEXOUT=DUMPDIR`).
- Headless frames of each track (`BR_FLAG_TEXTURES=0` and `=1`, the same
  frames under the virtual clock), and the road's L*, chroma and fine detail
  measured against section 2's table.
- `BR_SURF_VIEW=1`: the sets' detail alone on grey, to see what reaches the
  G-buffer.
