#!/bin/zsh
# verify a loop render is fresh, export it, and make a preview MP3
T=${0:a:h}; R=$(cd "$T/../../../.." && pwd); mkdir -p "$R/build/tgrally/music"; cd "$R/build/tgrally/music"; export PYTHONPATH="$T${PYTHONPATH:+:$PYTHONPATH}"; p=$1; name=$2; ver=$3; extra=${4:-0}
[ loop2/${p}_tracks.npz -nt loop_all.sh ] || { echo "$p: tracks are stale"; exit 1; }
grep -c Traceback loop2/$p.render.log loop2/$p.mix.log | grep -v ":0" && exit 1
tail -c 3000 loop2/$p.render.log | grep -o "jungle_kit': ([^)]*)" | sort | uniq -c | tr '\n' ' '; echo
venv/bin/python $T/keycheck.py $p loop2/%s_tracks.npz 2>&1 | grep -v -i warn | grep -v "shift +0" | grep -v "hats\|break\|kick\|perc\|cym\|kit"
venv/bin/python $T/export_loop.py $p 2>&1 | grep -v -i warn
M=$R/ports/common/music
t=$(python3 -c "import json;d=json.load(open('$M/remastered.json'));e=[x for x in [d['title']]+d['race'] if x['file']=='remastered_$p.flac'][0];print(e['loop_start']/48000+$extra)")
ffmpeg -nostdin -hide_banner -loglevel error -y -i $M/remastered_$p.flac -t $t -af "afade=t=out:st=$(python3 -c "print($t-3)"):d=3,aresample=44100" -sample_fmt s16p -c:a libmp3lame -b:a 320k "mp3/TGR Remastered - $name $ver.mp3" && echo "mp3 ok"
