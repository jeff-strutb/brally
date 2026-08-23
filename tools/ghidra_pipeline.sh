#!/bin/bash
# Full pipeline: Ghidra decompile -> export C -> ready for auto_decomp transforms
# Run from repo root.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GHIDRA="/opt/homebrew/opt/ghidra/libexec"
GHIDRA_PROJ="/private/tmp/ghidra_brglide"
EXPORT_DIR="$ROOT/build/ghidra_decomp"

export JAVA_HOME=/opt/homebrew/opt/openjdk@21

mkdir -p "$GHIDRA_PROJ" "$EXPORT_DIR"

echo "=== Step 1: Import + full analysis ==="
"$GHIDRA/support/analyzeHeadless" \
    "$GHIDRA_PROJ" BRGlide_proj \
    -import "$ROOT/orig/BRGlide.dll" \
    -overwrite \
    -postScript "$ROOT/tools/ghidra_export.py" "$EXPORT_DIR" "$ROOT/config/functions_glide.csv" \
    2>&1

echo ""
echo "=== Done ==="
echo "Decompiled C files in: $EXPORT_DIR"
ls "$EXPORT_DIR" | wc -l
echo "files exported"
