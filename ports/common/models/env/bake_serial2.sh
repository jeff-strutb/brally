#!/bin/sh
# One Blender bake at a time (a tree at level 3 peaks at tens of GB);
# each waits until at least 32 GB is free.
cd "$(dirname "$0")/../../../.."
while read a t0 t1 t2; do
    [ -z "$a" ] && continue
    [ -f ports/common/models/$a/bake.json ] && { echo "$a done already"; continue; }
    while :; do
        free=$(vm_stat | awk '/Pages free|Pages inactive|Pages speculative/ {gsub("\\.","",$NF); s+=$NF} END {print int(s*16384/1073741824)}')
        [ "$free" -ge 32 ] && break
        echo "$a waiting: ${free} GB free"; sleep 30
    done
    /usr/bin/time -l blender -b --python ports/brally-wasm/tools/remaster_env_bake.py -- \
        ports/common/models/env/polyhaven/models/$a/${a}_4k.blend $a ports/common/models $t0 $t1 $t2 \
        > ports/common/models/env/logs/$a.log 2>&1
    echo "$a exit $? $(grep -c BAKE_OK ports/common/models/env/logs/$a.log) ok, peak $(awk '/maximum resident/ {print int($1/1073741824)}' ports/common/models/env/logs/$a.log) GB"
done < ports/common/models/env/bake_list2.txt
