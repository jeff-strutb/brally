#!/bin/sh
# tgrcheck.sh -- what a build does, retrace by retrace, for a few scripts:
# the per-retrace trace (--trace: each graphics task's display list with the
# data it names, each swap, each audio buffer, the controllers) and the frame
# shown as a pixel hash (TGR_FRAMEHASH, the soft renderer), and the audio
# out (TGR_WAVDUMP), all in check mode (TGR_CHECK=1: one thread at a time on
# a counted clock, so a run is the same every time). Two builds that differ
# only below the game must give identical files.
#   tgrcheck.sh run BIN OUTDIR [SCRIPT ...]   record (default scripts below)
#   tgrcheck.sh cmp DIR_A DIR_B               first difference per file
# env: SECS (default 300) per script.
cd "$(dirname "$0")/../../.."
S=tools/tgrally/n64box_scripts
DEFAULT="champ_race options loadsave_pak attract"
SECS=${SECS:-300}

run_one() {
    bin=$1 out=$2 n=$3
    d=$out/$n
    rm -rf "$d"; mkdir -p "$d/save"
    frames=$(sed -n 's/^frames \([0-9]*\).*/\1/p' "$S/$n.txt" | head -1)
    TGR_CHECK=1 TGR_WAVDUMP=$d/audio.wav@0 \
    BR_SAVEDIR=$d/save TGR_PAK_FILE=$d/save/pak.bin TGR_FRAMEHASH=$d/frames.txt \
        "$bin" --headless --frames "${frames:-3000}" --script "$S/$n.txt" --trace "$d/trace.txt" > "$d/log.txt" 2>&1 &
    pid=$! i=0
    while kill -0 $pid 2>/dev/null; do
        i=$((i + 1))
        if [ $i -gt "$SECS" ]; then kill $pid; echo "$n: TIMED OUT after ${SECS}s"; break; fi
        sleep 1
    done
    wait $pid; rc=$?
    echo "$n: rc=$rc ${i}s, $(grep -c '' "$d/trace.txt" 2>/dev/null) trace lines, $(grep -c '' "$d/frames.txt" 2>/dev/null) frames"
}

case "$1" in
run)
    bin=$2 out=$3; shift 3
    [ -x "$bin" ] || { echo "tgrcheck: no binary $bin" >&2; exit 2; }
    mkdir -p "$out"
    [ $# -gt 0 ] || set -- $DEFAULT
    for n in "$@"; do run_one "$bin" "$out" "$n" & done
    wait
    ;;
cmp)
    a=$2 b=$3 bad=0
    for d in "$a"/*/; do
        n=$(basename "$d")
        for f in trace frames audio; do
            x=txt; [ $f = audio ] && x=wav
            [ -f "$a/$n/$f.$x" ] || { echo "$n $f: not in $a"; continue; }
            if cmp -s "$a/$n/$f.$x" "$b/$n/$f.$x"; then
                echo "$n $f: identical ($(wc -c < "$a/$n/$f.$x" | tr -d ' ') bytes)"
            elif [ $x = wav ]; then
                echo "$n $f: DIFFER, $(cmp "$a/$n/$f.$x" "$b/$n/$f.$x" | head -1)"
                bad=1
            else
                echo "$n $f: DIFFER, first: $(diff "$a/$n/$f.$x" "$b/$n/$f.$x" | sed -n '2p' | cut -c1-120)"
                bad=1
            fi
        done
    done
    exit $bad
    ;;
*) sed -n 2,10p "$0"; exit 2;;
esac
