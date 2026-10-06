#!/bin/zsh
T=${0:a:h}; R=$(cd "$T/../../.." && pwd); mkdir -p "$R/build/tgrally/music"; cd "$R/build/tgrally/music"; export PYTHONPATH="$T${PYTHONPATH:+:$PYTHONPATH}";
AU="/Library/Audio/Plug-Ins/Components/Splice INSTRUMENT.component"
venv/bin/python $T/splice_pick.py splice_lapsteel_au.state "$AU" > splice_pick_lap.log 2>&1
echo LAP-SAVED
venv/bin/python $T/splice_pick.py splice_umbriel_au.state "$AU" > splice_pick_umb.log 2>&1
echo UMB-SAVED
