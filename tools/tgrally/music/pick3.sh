#!/bin/zsh
T=${0:a:h}; R=$(cd "$T/../../.." && pwd); mkdir -p "$R/build/tgrally/music"; cd "$R/build/tgrally/music"; export PYTHONPATH="$T${PYTHONPATH:+:$PYTHONPATH}";
AU="/Library/Audio/Plug-Ins/Components/Splice INSTRUMENT.component"
for s in micah cello umbriel; do
  venv/bin/python $T/splice_pick.py splice_${s}_au.state "$AU" > splice_pick_$s.log 2>&1; echo "$s-SAVED"
  sleep 2
done
