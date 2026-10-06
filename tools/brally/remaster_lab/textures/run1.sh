#!/bin/sh
b=$(basename $1 .txt)
mkdir -p build/brally/remaster/lab/tex/sv/$b
cd "$(git rev-parse --show-toplevel)"
BR_ROOT=$PWD BR_HEADLESS=1 BR_FX=1 BR_RES=640x480 BR_VCLOCK=4 BR_SAVEDIR=build/brally/remaster/lab/tex/sv/$b BR_SCRIPT=$1 BR_TEXDUMP=build/brally/remaster/lab/tex/dump BR_FX_STAT=1 build/brally/remaster/lab/tex/brally > build/brally/remaster/lab/tex/sv/$b.log 2>&1
