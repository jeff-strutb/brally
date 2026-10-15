#!/bin/sh
# run.sh: every lighting check that needs no game run, one after another;
# exits non-zero on the first failure.
#   rt.sh      the track BVH and its traversal (CPU, GPU, hardware) against brute force
#   shadow.sh  the sun's soft shadows, contact shadows and cascade cache
#   lights.sh  clustered light culling and the lamps' shadow atlas
#   skymask.sh the backdrop's structural sky mask and the exposure meter
#   gi.sh      the irradiance probes (DDGI): furnace, a lamp's first bounce, leaks, the fold against the CPU, cost
#   taa.sh     the lit frame's TAA: jitter, reprojection, clipping, convergence (GPU vs CPU), MetalFX
#   maths.sh   the shader maths (BRDF, sky, tonemap, exposure, lamps, bloom, de-lighting) against closed forms and CPU references
#   vol.sh     the volumetric fog: its integration against closed forms, its kernels against the CPU reference
#   night.sh   the moon, the glow's gating, the track's lights
#   refl.sh    reflections and wet roads: the march, wetness and LU against references; offline compiles
#   carpaint.sh car paint's clear coat: the capture's car classification (its BRDF checks run in maths.sh)
set -e
cd "$(dirname "$0")"
for t in rt.sh shadow.sh lights.sh gi.sh taa.sh maths.sh skymask.sh vol.sh night.sh refl.sh carpaint.sh; do
    echo "== $t"
    sh "./$t"
done
