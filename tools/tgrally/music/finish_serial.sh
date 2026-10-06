#!/bin/zsh
# strictly one job at a time
T=${0:a:h}; R=$(cd "$T/../../.." && pwd); mkdir -p "$R/build/tgrally/music"; cd "$R/build/tgrally/music"; export PYTHONPATH="$T${PYTHONPATH:+:$PYTHONPATH}";

$T/exp_one.sh title Title v10 > exp_title.log 2>&1; echo title-exported
TRACKS=loop2/%s_tracks.npz NOTRIM=1 ORIGWAV=loop2/%s.xmlog.wav venv/bin/python -c "import mixp; print('mountain', mixp.mix_piece('mountain','loop2/mountain_mix.wav'))" > loop2/mountain.mix.log 2>&1; echo mountain-mixed
EOF
sed -i '' 's/^touch loop_all.sh.*$//' finish_serial.sh
cat >> finish_serial.sh <<'EOF'
$T/exp_one.sh mountain Mountain v10 > exp_mountain.log 2>&1; echo mountain-exported

EVENTS_DIR=loop2 venv/bin/python $T/piece_render.py stripmine > loop2/stripmine.render.log 2>&1; echo stripmine-rendered
TRACKS=loop2/%s_tracks.npz NOTRIM=1 ORIGWAV=loop2/%s.xmlog.wav venv/bin/python -c "import mixp; print('stripmine', mixp.mix_piece('stripmine','loop2/stripmine_mix.wav'))" > loop2/stripmine.mix.log 2>&1; echo stripmine-mixed
$T/exp_one.sh stripmine "Strip Mine" v9 > exp_stripmine.log 2>&1; echo stripmine-exported
echo ALL-DONE
