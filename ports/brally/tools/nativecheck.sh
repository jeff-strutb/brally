#!/bin/sh
# nativecheck.sh -- what a build hands the renderer and the speakers, and the
# game's state, frame by frame, for a few scripts on one virtual timeline.
# Two builds that differ only below the game (the platform layer) must give
# identical results.
#   nativecheck.sh run BIN OUTDIR [SCRIPT ...]   record (default scripts below)
#   nativecheck.sh cmp BASE BASE2 NEW            first difference per stream
#                                                (BASE2: a second baseline run,
#                                                which teaches the state noise)
# Each script runs headless under BR_VCLOCK=0.25 with BR_RRLOG (brr_null.c),
# BR_AUDIOLOG (audio.c) and BR_DUMP_EVERY=150 (script.c), the dumps kept
# compressed for nativecheck_state.py. env: SECS (default 300) per script.
cd "$(dirname "$0")/../../.."
S=tools/brally/brbox_scripts
DEFAULT="20_quickrace_drive 30_options 28_weather_rain 45_timeattack_save 61_mp_race"
SECS=${SECS:-300}

run_one() {
    bin=$1 out=$2 n=$3
    d=$out/$n
    rm -rf "$d"; mkdir -p "$d/save" "$d/shots" "$d/dump"
    cp "$S/$n.txt" "$d/script.txt"
    p=$(sed -n 's/^peer \([^ ]*\).*/\1/p' "$S/$n.txt" | head -1)
    [ -n "$p" ] && cp "$S/$p" "$d/$p"
    BR_VCLOCK=0.25 BR_SAVEDIR=$d/save BR_SHOTS=$d/shots BR_SCRIPT=$d/script.txt \
    BR_RRLOG=$d/rr.txt BR_AUDIOLOG=$d/audio.txt BR_DUMP_EVERY=150:$d/dump \
        "$bin" > "$d/log.txt" 2>&1 &
    pid=$! i=0
    while kill -0 $pid 2>/dev/null; do
        i=$((i + 1))
        if [ $i -gt "$SECS" ]; then kill $pid; echo "$n: TIMED OUT after ${SECS}s"; break; fi
        sleep 1
    done
    wait $pid; rc=$?
    python3 ports/brally/tools/nativecheck_state.py pack "$d/dump"
    echo "$n: rc=$rc $(grep -c '' "$d/rr.txt" 2>/dev/null) frames, $(grep -c '' "$d/audio.txt" 2>/dev/null) audio, $(ls "$d/dump" | grep -c gz) dumps ($(grep -E 'script: end at' "$d/log.txt" | head -1))"
}

case "$1" in
run)
    bin=$2 out=$3; shift 3
    [ -x "$bin" ] || { echo "nativecheck: no binary $bin" >&2; exit 2; }
    mkdir -p "$out"
    [ $# -gt 0 ] || set -- $DEFAULT
    for n in "$@"; do run_one "$bin" "$out" "$n" & done
    wait
    ;;
cmp)
    a=$2 a2=$3 b=$4 bad=0
    for d in "$a"/*/; do
        n=$(basename "$d")
        printf '%s ' "$n"
        python3 ports/brally/tools/nativecheck_state.py cmp "$a/$n/dump" "$a2/$n/dump" "$b/$n/dump" || bad=1
        for s in rr audio; do
            fa=$a/$n/$s.txt fb=$b/$n/$s.txt
            if [ ! -f "$fb" ]; then echo "$n $s: missing in $b"; bad=1; continue; fi
            if cmp -s "$fa" "$fb"; then
                echo "$n $s: identical ($(grep -c '' "$fa") lines)"
            else
                echo "$n $s: DIFFER, first: $(diff "$fa" "$fb" | sed -n '2p;/^---/{n;p;}' | head -2 | tr '\n' ' ')"
                bad=1
            fi
        done
    done
    exit $bad
    ;;
*) sed -n 2,10p "$0"; exit 2;;
esac
