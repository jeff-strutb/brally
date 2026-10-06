#!/bin/zsh
# Rebuild the remaster workspace: environment, sample libraries, amp/cab, compiled tools.
set -u
T=${0:a:h}; R=$(cd "$T/../../.." && pwd); mkdir -p "$R/build/tgrally/music"; cd "$R/build/tgrally/music"; export PYTHONPATH="$T${PYTHONPATH:+:$PYTHONPATH}"; mkdir -p lib nam ir dl
python3 -m venv venv && venv/bin/pip -q install numpy scipy soundfile torch pedalboard mido py7zr && echo ENV-OK
cp $R/reference/tgrally/XM/remaster/renderer/*.nam nam/ && echo NAM-OK
cc -O2 -o xmlog xmlog.c -lm && cc -O3 -shared -fPIC -o libsynth.dylib synth.c && echo TOOLS-OK
get() { [ -s "dl/$2" ] || curl -sL --retry 3 -o "dl/$2" "$1"; }
get https://freepats.zenvoid.org/Piano/SalamanderGrandPiano/SalamanderGrandPianoV3+20161209_48khz24bit.tar.xz salamander.tar.xz && tar -xJf dl/salamander.tar.xz -C lib && echo SALAMANDER-OK
get https://drumgizmo.org/kits/DRSKit/DRSKit2_1.zip drs.zip && mkdir -p lib/drs && unzip -q -o dl/drs.zip -d lib/drs && echo DRS-OK
for n in FingerBassYR:https://github.com/freepats/electric-bass-YR/releases/download/2019-09-30/FingerBassYR-SFZ+FLAC-20190930.7z \
         PickedBassYR:https://github.com/freepats/electric-bass-YR/releases/download/2019-09-30/PickedBassYR-SFZ+FLAC-20190930.7z \
         EGuitarFSBS-clean:https://github.com/freepats/electric-guitar-FSBS-clean/releases/download/2026-08-07/EGuitarFSBS-clean-SFZ+FLAC-20260807.7z; do
  name=${n%%:*}; url=${n#*:}; get "$url" $name.7z && mkdir -p lib/$name && venv/bin/python -c "import py7zr,sys; py7zr.SevenZipFile('dl/$name.7z').extractall('lib/$name')" && echo $name-OK
done
get https://www.scienceamps.com/uploads/4/9/8/9/49897661/science_4x12_irs.zip science.zip && mkdir -p ir/science && unzip -q -o dl/science.zip -d ir/science && echo IR-OK
echo LIBS-STAGE1-DONE
