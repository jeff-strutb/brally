#!/bin/sh
ls build/brally/remaster/lab/tex/runs/*.txt | xargs -n 1 -P 12 build/brally/remaster/lab/tex/run1.sh
echo done
