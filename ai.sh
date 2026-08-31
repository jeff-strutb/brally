#!/usr/bin/env bash
# ai.sh -- bootstrap a local LLM and crank on byte-exact matching, no API tokens.
#
# The model proposes revised C source; MSVC 5.0 (under Wine) compiles it and the
# bytes are diffed against the original -- exact, free verification. Only
# byte-exact results are committed. Runs unattended on your own machine.
#
#   ./ai.sh                      # one pass: 25 functions, 4 tries each, then exits
#   ./ai.sh --forever            # crank until you Ctrl-C (skips functions already tried)
#   AI_MODEL=deepseek-r1:70b ./ai.sh --forever --max-fns 8 --iters 10   # slow deep pass
#   ./ai.sh --max-fns 100 --iters 6 --max-size 800
#
# Flags after ./ai.sh pass through to tools/ai_loop.py (--iters, --max-fns,
# --min-size, --max-size, --forever, --retry-failed). A ledger at
# build/match/ai_attempted.csv remembers what it tried so --forever doesn't
# re-grind known fails; --retry-failed clears that. Ctrl-C any time; committed
# matches persist.
set -euo pipefail
cd "$(dirname "$0")"

MODEL="${AI_MODEL:-qwen2.5-coder:32b}"

# 1. Ollama installed?
if ! command -v ollama >/dev/null 2>&1; then
  echo "Ollama is not installed. Install it, then re-run:"
  echo "    brew install ollama       # or download from https://ollama.com/download"
  exit 1
fi

# 2. Server running? (start it in the background if not)
if ! curl -sf http://localhost:11434/api/tags >/dev/null 2>&1; then
  echo "Starting the Ollama server in the background..."
  (ollama serve >/tmp/ollama-brally.log 2>&1 &)
  for _ in $(seq 1 20); do
    curl -sf http://localhost:11434/api/tags >/dev/null 2>&1 && break
    sleep 1
  done
fi

# 3. Model pulled? (one-time ~20 GB download for the 32B Q4)
if ! ollama list | awk '{print $1}' | grep -qx "$MODEL"; then
  echo "Pulling $MODEL (one-time; ~20 GB, fits your 64 GB comfortably)..."
  ollama pull "$MODEL"
fi

# 4. Python with capstone (the repo's venv, same one the matching tools use)
PY=.venv/bin/python
[ -x "$PY" ] || PY=python3

# 5. Sanity: the matching corpus must be Glide-keyed, and report.csv must exist.
"$PY" tools/refcheck.py || { echo "refcheck failed -- fix the corpus before running."; exit 1; }
if [ ! -f build/match/report.csv ]; then
  echo "build/match/report.csv not found. Run a sweep first (tools/match_sweep.py)."
  exit 1
fi

echo "Model: $MODEL   (log of proposals below; only byte-exact matches are committed)"
echo "-----------------------------------------------------------------------------"
exec "$PY" tools/ai_loop.py --model "$MODEL" "$@"
