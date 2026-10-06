#!/usr/bin/env bash
# tools/brally/perm.sh -- crank the deterministic C permuter over the near-miss frontier in
# parallel, and bank every byte-exact result. It mutates
# semantically-equivalent C, compiles under MSVC 5.0 (Wine), and keeps only what
# byte-matches the original. This is the free lever for register/coloring walls.
#
#   ./perm.sh                       # 5 parallel permuters, 7 min/function, one pass
#   ./perm.sh --forever             # crank until you Ctrl-C (skips what it already tried)
#   ./perm.sh --workers 8 --secs 900 --max-diffs 20   # more workers, deeper, tighter targets
#
# Flags pass through to tools/brally/perm_fleet.py (--workers, --secs, --iters,
# --max-fns, --min-diffs, --max-diffs, --min-size, --max-size, --forever,
# --retry). A ledger at build/brally/win32/match/perm_attempted.csv remembers what it tried.
# It skips files another loop has in flight. Ctrl-C any time; committed
# matches persist.
set -euo pipefail
cd "$(dirname "$0")/../.."

PY=.venv/bin/python
[ -x "$PY" ] || PY=python3

"$PY" tools/brally/refcheck.py || { echo "refcheck failed -- fix the corpus first."; exit 1; }
if [ ! -f build/brally/win32/match/report.csv ]; then
  echo "build/brally/win32/match/report.csv not found. Run a sweep first (tools/brally/match_sweep.py)."
  exit 1
fi

# A quick MSVC/Wine smoke test: the permuter does thousands of compiles, so fail
# early and clearly if the toolchain can't compile at all.
if ! sh tools/toolchains/wine.sh tools/toolchains/msvc5/bin/cl.exe >/dev/null 2>&1; then
  echo "warning: could not invoke MSVC 5.0 via Wine (tools/toolchains/wine.sh + tools/toolchains/msvc5/bin/cl.exe)."
  echo "The permuter needs it to compile candidates. Continuing, but expect failures if this is broken."
fi

echo "Deterministic permuter fleet -- byte-exact results are committed as they land."
echo "-----------------------------------------------------------------------------"
exec "$PY" tools/brally/perm_fleet.py "$@"
