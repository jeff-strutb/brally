#!/bin/sh
# lightcheck.sh SCRIPT [EVERY] -- the Remastered lighting's two promises, on
# one brbox script (docs/brally/remaster/lighting/DESIGN.md section 11):
#
#   the game never notices it   the script run under BR_VCLOCK with
#                               BR_FLAG_LIGHTING=0 and =1, the original's data
#                               area dumped every EVERY frames (default 100):
#                               the game state in every dump identical
#                               (dumpdiff.py --state)
#   Original is unchanged       with BASE set (a build of the same renderer from
#                               before the lighting, e.g. a worktree's), SHOTBIN
#                               with the flag off against it: every shot the
#                               script takes (its marks) identical, byte for byte
#
#   env: BIN     the state runs' build (default build/brally/light-null/brally64)
#        SHOTBIN the Original run's build, a renderer that takes shots
#                (default build/brally/light-soft/brally64)
#        BASE    (optional) the baseline binary for the Original check
#        SECS    the most a run may take (default 300)
cd "$(dirname "$0")/../../.."
s=$1; every=${2:-100}; n=$(basename "$s" .txt)
BIN=${BIN:-build/brally/light-null/brally64}
SHOTBIN=${SHOTBIN:-build/brally/light-soft/brally64}
SECS=${SECS:-300}
d=build/brally/light-check/$n
rm -rf "$d"
mkdir -p "$d"
sed 's/^mark \(.*\)$/mark \1\nshot \1/' "$s" > "$d/script.txt"
run() { # name binary flag
    mkdir -p "$d/$1/save" "$d/$1/shots" "$d/$1/dumps"
    BR_VCLOCK=0.25 BR_HEADLESS=1 BR_FLAG_LIGHTING=$3 BR_DUMP_EVERY="$every:$d/$1/dumps" BR_SAVEDIR="$d/$1/save" \
        BR_SHOTS="$d/$1/shots" BR_SCRIPT="$d/script.txt" "$2" > "$d/$1/log.txt" 2>&1 &
    pid=$!
    i=0
    while kill -0 $pid 2>/dev/null; do
        i=$((i + 1))
        if [ $i -gt "$SECS" ]; then kill $pid; echo "$n: $1 run stopped after $SECS s" >> "$d/timeouts.txt"; break; fi
        sleep 1
    done
    wait $pid
}
run off "$BIN" 0 & run on "$BIN" 1 &
if [ -n "$BASE" ]; then run base "$BASE" 0 & run shots "$SHOTBIN" 0 & fi
wait
rc=0
[ -f "$d/timeouts.txt" ] && { cat "$d/timeouts.txt"; rc=1; }
nd=0
for f in $(ls "$d/off/dumps" | sort); do
    nd=$((nd + 1))
    [ -f "$d/on/dumps/$f" ] || { echo "$n: no dump $f with the lighting on"; rc=1; break; }
    .venv/bin/python ports/brally/tools/dumpdiff.py "$d/on/dumps/$f" "$d/off/dumps/$f" \
        --state --map "$(dirname "$BIN")/gen/br_data.c" > "$d/first.txt" 2>&1
    # the sound channels' records differ between any two runs, lighting or
    # not (two runs with it off differ the same way: MEASUREMENTS.md)
    if grep -v 'g_aBrSfxChan' "$d/first.txt" | grep -q '^ *[0-9]'; then
        echo "$n: game state differs with the lighting on, frame $((10#${f%.bin}))"
        head -20 "$d/first.txt"
        rc=1
        break
    fi
done
[ "$(ls "$d/on/dumps" | wc -l)" = "$(ls "$d/off/dumps" | wc -l)" ] || { echo "$n: the runs dumped different numbers of frames"; rc=1; }
[ $rc = 0 ] && echo "$n: game state identical, lighting off and on ($nd dumps, every ${every}th frame)"
if [ -n "$BASE" ]; then
    ns=0 bad=0
    for f in $(ls "$d/base/shots" | sort); do
        ns=$((ns + 1))
        cmp -s "$d/base/shots/$f" "$d/shots/shots/$f" || { echo "$n: Original differs from the baseline at shot $f"; bad=1; }
    done
    [ "$(ls "$d/base/shots" | wc -l)" = "$(ls "$d/shots/shots" | wc -l)" ] || { echo "$n: different numbers of shots than the baseline"; bad=1; }
    [ $bad = 0 ] && echo "$n: Original identical to the baseline ($ns shots)" || rc=1
fi
rm -rf "$d/off/dumps" "$d/on/dumps"
exit $rc
