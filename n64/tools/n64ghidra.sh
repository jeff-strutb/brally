#!/bin/bash
# T1 drafts for Top Gear Rally: Ghidra decompiles every function in the ROM map.
# Output: build/n64/ghidra/0x80XXXXXX.c  (drafts, never project code)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GHIDRA="/opt/homebrew/opt/ghidra/libexec"
PROJ="$ROOT/build/n64/ghidra_proj"
export JAVA_HOME=/opt/homebrew/opt/openjdk@21
mkdir -p "$PROJ" "$ROOT/build/n64/ghidra"
"$GHIDRA/support/analyzeHeadless" "$PROJ" tgr \
    -import "$ROOT/build/n64/tgr_image.bin" -overwrite \
    -loader BinaryLoader -loader-baseAddr 0x80200000 \
    -processor MIPS:BE:32:default -scriptPath "$ROOT/n64/tools" \
    -postScript GhidraExportN64.java "$ROOT/build/n64/ghidra" "$ROOT/build/n64/ghidra_targets.csv"
