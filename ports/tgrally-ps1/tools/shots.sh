#!/bin/sh
# shots.sh SCRIPT FRAMES F1,F2,...: the PlayStation build's frames and the
# native port's (its soft renderer, the reference) at the same retraces, as
# PNGs in build/tgrally/ps1/run/SCRIPT/{ps1,mac}.  DuckStation runs hidden.
set -e
cd "$(dirname "$0")/../../.."
S=$1; N=$2; AT=$3
R=build/tgrally/ps1/run/$S
rm -rf $R/ps1 $R/mac; mkdir -p $R/ps1 $R/mac
ln -f build/tgrally/ps1/assets/romdata.bin $R/romdata.bin
cp tools/tgrally/n64box_scripts/$S.txt $R/
find $R -maxdepth 1 \( -name 'shot*' -o -name 'fatal.txt' \) -exec rm -f {} +
printf 'script=%s.txt\nframes=%s\nheadless=1\nTGR_SHOT_AT=%s\n' $S $N $AT > $R/run.cfg
.venv/bin/python ports/tgrally-ps1/tools/dsrun.py --cpu CachedInterpreter --ram8 --timeout ${TIMEOUT:-900} $R build/tgrally/ps1/tgrally.exe
[ -f $R/fatal.txt ] && { cat $R/fatal.txt; echo; }
for f in $(find $R -maxdepth 1 -name 'shot*.raw'); do mv $f $R/ps1/; done
.venv/bin/python ports/tgrally-ps1/tools/shot2png.py $(find $R/ps1 -name '*.raw' | sort) > /dev/null
build/tgrally/null-soft/tgrally --headless --frames $N --script tools/tgrally/n64box_scripts/$S.txt --shots $R/mac --shot-at $AT > /dev/null 2>&1
ls $R/ps1/*.png $R/mac/*.png
