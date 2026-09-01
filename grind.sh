#!/usr/bin/env bash
# grind.sh -- launcher for the background mutation loops (permuter + local LLM).
#
# HONEST FRAMING (2026-09-01): these loops are a LOTTERY TICKET, not a lane.
# The deterministic permuter is 0-for-95 on the near-miss frontier, and the
# project's own verdict (commit 5a4a338) is that register-allocation coloring
# walls are NOT source-permutable -- no C spelling flips them. Do not run this
# expecting to close the coloring tail; park coloring walls as honest residue.
#
# What these loops CAN occasionally catch is a small STRUCTURAL near-miss the
# generators don't cover yet. Free (no API tokens), so running them idle
# overnight costs nothing -- just don't mistake them for progress machinery.
# The real levers are the structural playbook (docs/STRUCTURAL-PLAYBOOK.md),
# generator minting, and the T3b oracle (tools/t3b_verify.py).
#
#   ./grind.sh              # start permuter fleet + local-LLM loop, in background
#   ./grind.sh --status     # show what's running and recent matches
#   ./grind.sh --stop       # stop both
set -euo pipefail
cd "$(dirname "$0")"

PERM_LOG=build/match/perm_fleet.log
AI_LOG=build/match/ai_loop.log

case "${1:-start}" in
  --stop)
    pkill -f 'tools/perm_fleet.py' 2>/dev/null && echo "stopped permuter fleet" || echo "permuter fleet not running"
    pkill -f 'tools/ai_loop.py'    2>/dev/null && echo "stopped ai loop"       || echo "ai loop not running"
    # ai.sh boots an Ollama server whose model runner holds tens of GB of RAM;
    # a stop should release that too.
    pkill -f 'ollama' 2>/dev/null && echo "stopped ollama (RAM released)" || echo "ollama not running"
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
    echo "NOTE: lottery-ticket odds -- permuter is 0/95 lifetime on the frontier."
    echo "Real levers: docs/STRUCTURAL-PLAYBOOK.md + generators + tools/t3b_verify.py"
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
