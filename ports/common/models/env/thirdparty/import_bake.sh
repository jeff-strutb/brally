#!/bin/sh
# import -> bake (authored-model route, no subdivision) -> far views; one at a time
cd "$(dirname "$0")/../../../../.."
T=ports/common/models/env/thirdparty
while IFS="$(printf '\t')" read a uid gap mode; do
    [ -f ports/common/models/$a/bake.json ] && grep -q impostor ports/common/models/$a/bake.json && { echo "$a done"; continue; }
    f=$(find "$T/$uid/gltf" -name "scene.gltf" | head -1)
    blender -b --python ports/brally-wasm/tools/remaster_env_import.py -- "$f" $a $T/blend/$a.blend ${mode:-one} > $T/logs_$a.import.log 2>&1 || { echo "$a IMPORT FAIL"; continue; }
    /usr/bin/time -l blender -b --python ports/brally-wasm/tools/remaster_env_bake.py -- $T/blend/$a.blend $a ports/common/models 150000 40000 10000 > $T/logs_$a.bake.log 2>&1 || { echo "$a BAKE FAIL"; continue; }
    blender -b --python ports/brally-wasm/tools/remaster_env_impostor.py -- ports/common/models $a 512 > $T/logs_$a.imp.log 2>&1 || { echo "$a IMPOSTOR FAIL"; continue; }
    echo "$a ok $(grep -h VARIANT $T/logs_$a.import.log | wc -l | tr -d ' ') variants, peak $(awk '/maximum resident/ {print int($1/1073741824)}' $T/logs_$a.bake.log) GB"
done < $T/assets.tsv
