#!/usr/bin/env python3
"""Download the picked Sketchfab models (picks.tsv) for the Remastered
environment, one at a time.

The API token is read from ~/.config/readington/sketchfab.token (one line,
the token from sketchfab.com > Settings > Password & API).  Each model's
licence is checked again before download: anything not CC0 or CC-BY is
refused.  Prefers the original source archive (the artist's .blend/.fbx
with full textures), else the glTF.  Writes <uid>/ with the archive
unpacked, and appends the credit to CREDITS.tsv (CC-BY needs it shown).
"""
import json, os, sys, urllib.request, zipfile, io

HERE = os.path.dirname(os.path.abspath(__file__))
TOKEN = open(os.path.expanduser("~/.config/readington/sketchfab.token")).read().strip()
OK = {"CC0 Public Domain": "CC0", "CC Attribution": "CC-BY"}

def api(path):
    rq = urllib.request.Request("https://api.sketchfab.com/v3/" + path, headers={"Authorization": "Token " + TOKEN})
    return json.load(urllib.request.urlopen(rq, timeout=60))

def main():
    credits = os.path.join(HERE, "CREDITS.tsv")
    for line in open(os.path.join(HERE, "picks.tsv")):
        gap, site, uid, name, _ = line.rstrip("\n").split("\t")
        if site != "sketchfab":
            continue
        out = os.path.join(HERE, uid)
        if os.path.exists(os.path.join(out, ".done")):
            print("have", name); continue
        info = api("models/" + uid)
        lic = (info.get("license") or {}).get("label", "")
        if lic not in OK:
            print("REFUSED (licence %r)" % lic, name); continue
        dl = api("models/%s/download" % uid)
        kind = "source" if "source" in dl else "gltf" if "gltf" in dl else None
        if not kind:
            print("no download", name, list(dl)); continue
        data = urllib.request.urlopen(dl[kind]["url"], timeout=600).read()
        os.makedirs(out, exist_ok=True)
        try:
            zipfile.ZipFile(io.BytesIO(data)).extractall(out)
        except zipfile.BadZipFile:
            open(os.path.join(out, "source.bin"), "wb").write(data)
        json.dump(info, open(os.path.join(out, "info.json"), "w"), indent=1)
        open(os.path.join(out, ".done"), "w").write(kind)
        with open(credits, "a") as f:
            f.write("\t".join([gap, name, info["user"]["displayName"], OK[lic], info["viewerUrl"]]) + "\n")
        print("got", kind, "%.1f MB" % (len(data) / 1e6), name, flush=True)

if __name__ == "__main__":
    main()
