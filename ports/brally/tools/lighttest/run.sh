#!/bin/sh
# run.sh: every lighting check that needs no game run, one after another;
# exits non-zero on the first failure.
#   rt.sh      the track BVH and its traversal (CPU, GPU, hardware) against brute force
#   shadow.sh  the sun's soft shadows, contact shadows and cascade cache
#   lights.sh  clustered light culling and the lamps' shadow atlas
#   maths.sh   the shader maths (BRDF, sky, tonemap, exposure, lamps, bloom, de-lighting) against closed forms and CPU references
#   vol.sh     the volumetric fog: its integration against closed forms, its kernels against the CPU reference
set -e
cd "$(dirname "$0")"
for t in rt.sh shadow.sh lights.sh maths.sh vol.sh; do
    echo "== $t"
    sh "./$t"
done
