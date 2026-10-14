#!/bin/sh
# fetch.sh: the skies skies.txt names, 8K Radiance .hdr from Poly Haven (CC0)
# into ph8k/, four at a time; files already present are kept.  fetch.sh 4k:
# the 4K maps into ph4k/ instead (the iPhone app's, ports/remastered/ios/build.sh).
cd "$(dirname "$0")"
R=${1:-8k}
case $R in 8k|4k) ;; *) echo "fetch.sh: 8k or 4k" >&2; exit 2 ;; esac
mkdir -p ph$R
grep -v '^#' skies.txt | awk 'NF == 3 { print $3 }' | sort -u | while read id; do
    [ -s "ph$R/${id}_$R.hdr" ] && continue
    echo "$id"
done | {
    while read id; do
        curl -sfL -A "brally-remaster/1.0" -o "ph$R/${id}_$R.hdr" "https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/$R/${id}_$R.hdr" && echo "ok $id" || echo "FAIL $id" &
        [ $(jobs -r | wc -l) -ge 4 ] && wait
    done
    wait    # in the pipeline's own shell, where the downloads are its jobs
}
