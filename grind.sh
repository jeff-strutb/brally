#!/usr/bin/env bash
# grind.sh -- one command to run the brute-force COLORING work.
#
# Coloring/scheduling walls (identical instruction multiset, different register
# allocation or float operand order) are NOT hand-solvable by reading asm -- no
# source edit reaches them. They are solved by compute: mutate semantically
# equivalent C, compile under real MSVC 5.0 (Wine), keep only byte-exact hits.
# Zero API tokens. This launches both engines and leaves them running.
#
#   ./grind.sh              # start permuter fleet + local-LLM loop, in background
#   ./grind.sh --status     # show what's running and recent matches
#   ./grind.sh --stop       # stop both
#
# Structural functions (wrong/missing CODE) are a DIFFERENT job -- those are for
# an AI following docs/STRUCTURAL-PLAYBOOK.md, not for this script.
set -euo pipefail
cd "$(dirname "$0")"

PERM_LOG=build/match/perm_fleet.log
AI_LOG=build/match/ai_loop.log

case "${1:-start}" in
  --stop)
    pkill -f 'tools/perm_fleet.py' 2>/dev/null && echo "stopped permuter fleet" || echo "permuter fleet not running"
    pkill -f 'tools/ai_loop.py'    2>/dev/null && echo "stopped ai loop"       || echo "ai loop not running"
    ;;
  --status)
    echo "=== processes ==="
    pgrep -fl 'tools/perm_fleet.py' || echo "permuter fleet: not running"
    pgrep -fl 'tools/ai_loop.py'    || echo "ai loop: not running"
    echo "=== recent permuter matches ==="
    grep -i 'MATCH\|byte-exact\|committed' "$PERM_LOG" 2>/dev/null | tail -10 || echo "(no log yet)"
    echo "=== recent ai matches ==="
    grep -i 'MATCH\|byte-exact\|committed' "$AI_LOG" 2>/dev/null | tail -10 || echo "(no log yet)"
    ;;
  start|"")
    if pgrep -f 'tools/perm_fleet.py' >/dev/null; then
      echo "permuter fleet already running (./grind.sh --stop to restart)"
    else
      nohup ./perm.sh --forever --workers 6 > "$PERM_LOG" 2>&1 &
      echo "permuter fleet -> $PERM_LOG (PID $!)"
    fi
    if pgrep -f 'tools/ai_loop.py' >/dev/null; then
      echo "ai loop already running"
    else
      nohup ./ai.sh --forever --max-fns 12 > "$AI_LOG" 2>&1 &
      echo "ai loop -> $AI_LOG (PID $!)"
    fi
    echo "Both commit byte-exact results as they land. ./grind.sh --status to watch."
    ;;
  *)
    echo "usage: ./grind.sh [--status|--stop]"; exit 1 ;;
esac
