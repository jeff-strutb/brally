#!/usr/bin/env bash
# perm.sh -- crank the deterministic C permuter over the near-miss frontier in
# parallel, and bank every byte-exact result. No LLM, no API tokens: it mutates
# semantically-equivalent C, compiles under MSVC 5.0 (Wine), and keeps only what
# byte-matches the original. This is the free lever for register/coloring walls.
#
#   ./perm.sh                       # 5 parallel permuters, 7 min/function, one pass
#   ./perm.sh --forever             # crank until you Ctrl-C (skips what it already tried)
#   ./perm.sh --workers 8 --secs 900 --max-diffs 20   # more workers, deeper, tighter targets
#
# Flags pass through to tools/perm_fleet.py (--workers, --secs, --iters,
# --max-fns, --min-diffs, --max-diffs, --min-size, --max-size, --forever,
# --retry). A ledger at build/match/perm_attempted.csv remembers what it tried.
# Runs happily alongside ./ai.sh and the Grok closer loops -- they skip each
# other's in-flight files. Ctrl-C any time; committed matches persist.
set -euo pipefail
cd "$(dirname "$0")"

PY=.venv/bin/python
[ -x "$PY" ] || PY=python3

"$PY" tools/refcheck.py || { echo "refcheck failed -- fix the corpus first."; exit 1; }
if [ ! -f build/match/report.csv ]; then
  echo "build/match/report.csv not found. Run a sweep first (tools/match_sweep.py)."
  exit 1
fi

# A quick MSVC/Wine smoke test: the permuter does thousands of compiles, so fail
# early and clearly if the toolchain can't compile at all.
if ! sh tools/wine.sh tools/msvc5/bin/cl.exe >/dev/null 2>&1; then
  echo "warning: could not invoke MSVC 5.0 via Wine (tools/wine.sh + tools/msvc5/bin/cl.exe)."
  echo "The permuter needs it to compile candidates. Continuing, but expect failures if this is broken."
fi

echo "Deterministic permuter fleet -- byte-exact results are committed as they land."
echo "-----------------------------------------------------------------------------"
exec "$PY" tools/perm_fleet.py "$@"
