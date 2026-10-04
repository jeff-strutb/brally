#!/bin/sh
# suite.sh [SCRIPT ...] -- run brbox scenario scripts against the 64-bit build.
#
# Each script runs headless with its own save directory and shot directory
# under build/portable/suite/<name>/, at most $JOBS at a time, for at most
# $SECS seconds. A summary line per script: ok (the script reached `end`,
# or the game quit by itself with 0), exit codes otherwise, and for a crash
# the faulting frames from lldb.
#   env: JOBS (default 6), SECS (default 600), BIN (default build/portable/brally64)
cd "$(dirname "$0")/../../.."
JOBS=${JOBS:-6}
SECS=${SECS:-600}
BIN=${BIN:-build/portable/brally64}
OUT=build/portable/suite
mkdir -p $OUT
[ $# -gt 0 ] || set -- tools/brbox_scripts/*.txt

run_one() {
    s=$1
    n=$(basename "$s" .txt)
    d=$OUT/$n
    rm -rf "$d"; mkdir -p "$d/save" "$d/shots"
    # a script another one runs as its `peer` is only half of a pair
    if ! grep -q '^peer' "$s" && grep -lq "^peer $n.txt" "$(dirname "$s")"/*.txt 2>/dev/null; then
        echo "$n: skipped (runs as the peer of $(grep -l "^peer $n.txt" "$(dirname "$s")"/*.txt | head -1 | xargs basename))" > "$d/result.txt"
        return
    fi
    sed 's/^mark \(.*\)$/mark \1\nshot \1/' "$s" > "$d/script.txt"
    VCLOCK=
    p=$(sed -n 's/^peer \([^ ]*\).*/\1/p' "$s" | head -1)
    if [ -n "$p" ]; then
        # a pair: the other copy's script beside this one, both on one
        # virtual timeline (peersync.c)
        sed 's/^mark \(.*\)$/mark \1\nshot \1/' "$(dirname "$s")/$p" > "$d/$p"
        VCLOCK=0.25
    fi
    BR_VCLOCK=$VCLOCK BR_SAVEDIR=$d/save BR_SHOTS=$d/shots BR_SCRIPT=$d/script.txt timeout_run "$d" &
    wait $!
}

timeout_run() {
    d=$1
    if [ -n "$BR_VCLOCK" ]; then export BR_VCLOCK; else unset BR_VCLOCK; fi
    BR_SAVEDIR=$d/save BR_SHOTS=$d/shots BR_SCRIPT=$d/script.txt $BIN > "$d/log.txt" 2>&1 &
    pid=$!
    i=0
    while kill -0 $pid 2>/dev/null; do
        i=$((i + 1))
        if [ $i -gt $SECS ]; then kill $pid 2>/dev/null; break; fi
        sleep 1
    done
    wait $pid; rc=$?
    n=$(basename "$d")
    exec > "$d/result.txt" 2>&1
    last=$(grep 'script: frame' "$d/log.txt" | tail -1 | cut -c1-80)
    peer=
    if [ -f "$d/save/peer/peer.log" ]; then
        peer="; peer: $(grep -E 'script: end at|timed out' "$d/save/peer/peer.log" | tail -1)"
    fi
    if grep -q 'script: end at' "$d/log.txt"; then
        echo "$n: ok ($(grep 'script: end at' "$d/log.txt")$peer)"
    elif [ $rc -eq 0 ]; then
        echo "$n: ok (the game quit with 0 after $last)"
    else
        echo "$n: rc=$rc last: $last $(grep -E 'timed out|never drawn|not supported|cannot' "$d/log.txt" | head -1)"
        if [ $rc -ge 129 ] && [ $rc -ne 143 ]; then
            printf 'settings set target.env-vars BR_SAVEDIR=%s/save2 BR_SHOTS=%s/shots BR_SCRIPT=%s/script.txt\nprocess launch -e /dev/null -o /dev/null\nbt 8\nquit\n' "$d" "$d" "$d" > "$d/lldb.txt"
            rm -rf "$d/save2"; mkdir -p "$d/save2"; cp -R "$d/save/." "$d/save2/" 2>/dev/null
            lldb -b -s "$d/lldb.txt" $BIN 2>&1 | grep -E 'stop reason|frame #[0-5]' | head -7 | sed 's/^/    /'
        fi
    fi
}

export OUT BIN SECS
export -f run_one timeout_run 2>/dev/null
for s in "$@"; do
    while [ "$(jobs -r | wc -l)" -ge "$JOBS" ]; do sleep 1; done
    run_one "$s" &
done
wait
for s in "$@"; do cat "$OUT/$(basename "$s" .txt)/result.txt" 2>/dev/null; done
