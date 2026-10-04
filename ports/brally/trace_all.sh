#!/bin/sh
# Build the traced game and run every coverage script under it, into one
# trace (build/wasm_trace/trace.bin), then the report and the spec check.
# Each script gets its own save directory. Multiplayer scripts need a peer
# the Mac host cannot provide yet; they are skipped.
set -e
cd "$(dirname "$0")/../.."
S=build/wasm_trace/runs
BR_TRACE_BUILD=1 sh ports/macos/wasm/build_wasm.sh > build/wasm_trace/build.log 2>&1
tail -1 build/wasm_trace/build.log
rm -rf $S build/wasm_trace/trace.bin
mkdir -p $S
for f in tools/brbox_scripts/*.txt; do
    n=$(basename $f .txt)
    grep -q '^peer\|^ *peer ' $f && { echo "$n skipped (peer)"; continue; }
    case $n in 6*) echo "$n skipped (multiplayer)"; continue;; esac
    mkdir -p $S/save_$n
    BR_SCRIPT=$f BR_HEADLESS=1 BR_VCLOCK=33.333 BR_SAVEDIR=$S/save_$n BR_TRACE_OUT=build/wasm_trace/trace.bin \
        perl -e 'alarm 600; exec @ARGV' build/wasm_trace/brally > $S/$n.log 2>&1 && rc=0 || rc=$?
    echo "$n rc=$rc $(grep -h 'script: end\|timed out\|never drawn\|not supported' $S/$n.log | tail -1)"
done
.venv/bin/python ports/brally/tools/trace_report.py
.venv/bin/python ports/brally/tools/catalog_check.py
