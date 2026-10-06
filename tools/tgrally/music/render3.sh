#!/bin/zsh
T=${0:a:h}; R=$(cd "$T/../../.." && pwd); mkdir -p "$R/build/tgrally/music"; cd "$R/build/tgrally/music"; export PYTHONPATH="$T${PYTHONPATH:+:$PYTHONPATH}";
for p in title desert stripmine; do (EVENTS_DIR=loop2 venv/bin/python $T/piece_render.py $p > loop2/$p.render.log 2>&1; TRACKS=loop2/%s_tracks.npz NOTRIM=1 ORIGWAV=loop2/%s.xmlog.wav venv/bin/python -c "
import mixp; print('$p', mixp.mix_piece('$p','loop2/${p}_mix.wav'))" > loop2/$p.mix.log 2>&1) & done; wait; echo ALLDONE
