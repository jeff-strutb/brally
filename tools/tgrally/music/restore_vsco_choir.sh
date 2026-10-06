#!/bin/zsh
T=${0:a:h}; R=$(cd "$T/../../.." && pwd); mkdir -p "$R/build/tgrally/music"; cd "$R/build/tgrally/music"; export PYTHONPATH="$T${PYTHONPATH:+:$PYTHONPATH}"; mkdir -p lib/vsco lib/choir lib/mihai dl
curl -s 'https://api.github.com/repos/sgossner/VSCO-2-CE/git/trees/master?recursive=1' | python3 -c "
import json,sys,urllib.parse; d=json.load(sys.stdin)
want=('Brass/F Horn/stac','Brass/F Horn/sus','Brass/Tenor Trombone/stac','Brass/Tenor Trombone/sus','Brass/Trumpet/stac','Brass/Trumpet/sus','Brass/Tuba/stac','Brass/Tuba/sus',
'Strings/Cello Section/susvib','Strings/Viola Section/susvib','Strings/Violin Section/susVib','Strings/Cello Section/spic','Strings/Viola Section/spic','Strings/Violin Section/Spic',
'Strings/Cello Section/pizzT','Strings/Viola Section/pizz','Strings/Violin Section/Pizz','Strings/Cello Section/trem','Strings/Viola Section/trem','Strings/Violin Section/Trem',
'Strings/Solo Contrabass/Spic','Strings/Solo Contrabass/Pizz','Strings/Solo Contrabass/SusVib','Strings/Harp','Percussion/Timpani',
'VSCO 1 Percussion/drums/bass','VSCO 1 Percussion/varMetal/Cymbals/clash','VSCO 1 Percussion/varMetal/Cymbals/susp')
for t in d['tree']:
    p=t['path']
    if t['type']=='blob' and p.endswith('.wav') and '/'.join(p.split('/')[:-1]) in want:
        print('url = \"https://raw.githubusercontent.com/sgossner/VSCO-2-CE/master/%s\"\noutput = \"lib/vsco/%s\"'%(urllib.parse.quote(p),p))
" > dl/vsco.cfg
grep output dl/vsco.cfg | sed 's/output = "//;s/"$//' | xargs -I{} dirname "{}" | sort -u | while read d; do mkdir -p "$d"; done
curl -sL --retry 3 --parallel --parallel-max 12 -K dl/vsco.cfg && echo "VSCO-OK $(find lib/vsco -name '*.wav' | wc -l)"
curl -sL "https://archive.org/download/virtual-playing-orchestra-3-2-wave-files/Virtual-Playing-Orchestra3-2-wave-files.zip/" | grep -oE 'href="[^"]*Chorus[^"]*\.wav"' | sed 's/href="//;s/"$//' | sort -u | while read u; do f=$(basename "$u" | sed 's/%23/#/g'); f=${f##*%2F}; echo "url = \"https:$u\"\noutput = \"lib/choir/$f\""; done > dl/choir.cfg
curl -sL --retry 3 --parallel --parallel-max 8 -K dl/choir.cfg && echo "CHOIR-OK $(ls lib/choir | wc -l)"
curl -sL -A "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124 Safari/537.36" "https://www.mediafire.com/file/bhdzds4tgtsocdp/Vowel_Choir_SFZ.zip/file" -o dl/mihai.zip && unzip -q -o dl/mihai.zip -d lib/mihai && echo "MIHAI-OK $(find lib/mihai -name '*.wav' | wc -l)"
echo VSCO-CHOIR-DONE
