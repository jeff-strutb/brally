#!/bin/sh
# run.sh: every lighting check that needs no game run, one after another;
# exits non-zero on the first failure.
#   rt.sh      the track BVH and its traversal (CPU, GPU, hardware) against brute force
#   shadow.sh  the sun's soft shadows, contact shadows and cascade cache
set -e
cd "$(dirname "$0")"
for t in rt.sh shadow.sh; do
    echo "== $t"
    sh "./$t"
done
