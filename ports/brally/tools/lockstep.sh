#!/bin/sh
# lockstep.sh SCRIPT [EVERY]  -- the 64-bit core against the 32-bit lane
#
# Runs one brbox script once on each build under BR_VCLOCK, each dumping the
# original's data area every EVERY frames (default 100; BR_DUMP_EVERY), and
# reports the first dumped frame whose game state differs (dumpdiff.py
# --state) and what differs there. One pass of each build per script: the
# 32-bit lane is the slow one.
#   env: CORE (default build/portable_null/brally64, a RENDER=null build),
#        LANE (default build/wasm/brally), MAP (that core's gen/br_data.c)
cd "$(dirname "$0")/../../.."
s=$1; every=${2:-100}; n=$(basename "$s" .txt)
CORE=${CORE:-build/portable_null/brally64}
LANE=${LANE:-build/wasm/brally}
MAP=${MAP:-$(dirname "$CORE")/gen/br_data.c}
d=build/portable/lockstep/$n
rm -rf "$d"; mkdir -p "$d/core/save" "$d/core/shots" "$d/core/dumps" "$d/lane/save" "$d/lane/shots" "$d/lane/dumps"
grep -v '^shot\|^mark' "$s" > "$d/script.txt"
run() { # name binary
    BR_VCLOCK=0.25 BR_HEADLESS=1 BR_DUMP_EVERY="$every:$d/$1/dumps" BR_SAVEDIR="$d/$1/save" \
        BR_SHOTS="$d/$1/shots" BR_SCRIPT="$d/script.txt" "$2" > "$d/$1/log.txt" 2>&1
}
run core "$CORE" & run lane "$LANE" & wait
for f in $(ls "$d/core/dumps" | sort); do
    [ -f "$d/lane/dumps/$f" ] || continue
    if ! .venv/bin/python ports/brally/tools/dumpdiff.py "$d/core/dumps/$f" "$d/lane/dumps/$f" \
            --state --map "$MAP" > "$d/first.txt"; then
        echo "$n: game state differs at frame $((10#${f%.bin}))"
        head -20 "$d/first.txt"
        rm -rf "$d/core/dumps" "$d/lane/dumps"
        exit 1
    fi
done
echo "$n: identical game state at every ${every}th frame ($(ls "$d/core/dumps" | wc -l | tr -d ' ') dumps)"
rm -rf "$d/core/dumps" "$d/lane/dumps"
