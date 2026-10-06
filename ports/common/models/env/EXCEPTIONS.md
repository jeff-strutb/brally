# Remastered environment: what Poly Haven does not cover

Running list. Everything in `manifest.json` is sourced from Poly Haven (CC0) and
lives under `polyhaven/`. What follows has no confident Poly Haven match and is
parked until we deal with it. Track names are the `.trk` files; counts are
placed pieces from the track model catalog.

## Landmarks and hero props: Meshy (curated prompts, car-standard pipeline)

Unique to Boss Rally, so they stay ours. Collapse Decimate -> Subsurf 1 -> Collapse, as with the cars.

| Track | Object |
|---|---|
| amazon | thatched huts (two variants), wooden plank / rope bridges, bridge supports |
| coast | lighthouse, windmill, stunt biplane, gazebo (octagonal), stone houses, round stone tower, harbour buildings |
| desert | hotel blocks, casino towers ("HOTEL" vertical sign tower), ring gate, stone arch bridge, steel truss bridge, hot-air balloons |
| mine | mine carts, derrick / loader rig, drawbridge, headframe and scaffolding, mine buildings |
| mountain | log cabin, sailboats |
| race | striped hospitality tents, pit garages, start gantry, grandstands |
| bonus | colosseum arch wall, traffic-light gantry |
| gamewin | city facades, BOSS billboard frame |

## Vegetation Poly Haven lacks

| Need | Where | Note |
|---|---|---|
| Palm trees (coconut / fan palm) | amazon, coast | none on Poly Haven. palm_bark / palm_tree_bark surfaces are already fetched |
| Jungle canopy trees (large broadleaf) | amazon | island_tree_* are too temperate for the Amazon |
| Hanging vines / lianas | amazon | |
| Dense hedge and fern walls lining the road | amazon, mountain | could be built from fern_02 + shrub scatter; decide when placing |
| Cacti / desert yucca | desert | only if the original desert cards turn out to be cacti (check) |
| Conifers 1.5-5 m tall | mountain | Poly Haven has saplings to 1.3 m and trees from 6 m; 142 mountain spots are scaled 0.47-2.27x meanwhile |
| Broadleaf trees 8-13 m tall | race, coast | Poly Haven has 2.6-5 m trees and a 19.5 m jacaranda; 35 spots scaled 0.6-2.2x meanwhile |

## Signs and billboards

New high-resolution art on modelled panels and frames. Real brands on the race
circuit (Budweiser, Toyota and others) need replacement art.

## Rebuilt from the original shapes, dressed with fetched surfaces

Not a sourcing gap, listed so nothing is forgotten: road-edge walls, retaining
walls, fences and railings that follow the road, catch fences, curbs, tunnels,
plank walkways. Geometry comes from the original track; materials come from
`polyhaven/textures/`.

## Free candidates for the gaps (2026-09-30)

Shortlist of CC0 / CC-BY models from Sketchfab and BlenderKit. Search data:
candidates/raw.json (415 candidates; candidates/search.py made it,
candidates/shortlist.json is the shortlist, candidates/sheets.py drew the
preview sheets). BlenderKit "royalty-free" excluded:
not shippable as loose files. CC-BY needs a credits line.

### Picked but rejected after import (2026-09-30)

- King Coconut: the fruit, not a palm.  Banana Tree: a potted plant.
- Prickly Pear (Opuntia dillenii) a/b: partial scans (one pad, a flowering sprig), not whole plants.
- Opuntia (Prickly Pear) Cactus: file carries a scene backdrop thousands of units wide.
- Agave Americana: the scan includes its ground slab.
Still open: a whole prickly-pear cactus and an agave without a base.
