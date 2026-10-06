#!/bin/zsh
T=${0:a:h}; R=$(cd "$T/../../.." && pwd); mkdir -p "$R/build/tgrally/music"; cd "$R/build/tgrally/music"; export PYTHONPATH="$T${PYTHONPATH:+:$PYTHONPATH}"; touch loop_all.sh
EVENTS_DIR=loop2 venv/bin/python $T/piece_render.py title > loop2/title.render.log 2>&1; echo rendered
TRACKS=loop2/%s_tracks.npz NOTRIM=1 ORIGWAV=loop2/%s.xmlog.wav venv/bin/python -c "import mixp; print('title', mixp.mix_piece('title','loop2/title_mix.wav'))" > loop2/title.mix.log 2>&1; echo mixed
$T/exp_one.sh title Title v11 > exp_title.log 2>&1; echo ALL-DONE
