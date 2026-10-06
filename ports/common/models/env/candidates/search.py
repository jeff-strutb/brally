import json, urllib.request, urllib.parse, time
GAPS = {
 "Coconut palm (Amazon, Coast)": ["coconut palm", "palm tree", "cocos nucifera"],
 "Fan palm (Coast, Desert)": ["fan palm", "washingtonia", "sabal palm"],
 "Jungle canopy tree 15-30 m (Amazon)": ["rainforest tree", "tropical tree", "jungle tree", "kapok tree", "ficus tree"],
 "Hanging vines / lianas (Amazon)": ["hanging vines", "liana", "jungle vines", "ivy"],
 "Jungle undergrowth (Amazon)": ["banana plant", "monstera", "tropical plant", "elephant ear plant"],
 "Young conifer 1.5-5 m (Mountain)": ["young spruce", "small spruce", "small fir tree", "spruce tree", "young pine"],
 "Broadleaf tree 8-13 m (Race, Coast)": ["oak tree", "maple tree", "beech tree", "linden tree", "birch tree"],
 "Cactus (Desert)": ["saguaro", "cactus", "prickly pear"],
 "Desert tree / yucca (Desert)": ["joshua tree", "yucca", "agave", "mesquite"],
}
UA = {"User-Agent": "Mozilla/5.0"}
def get(u):
    return json.load(urllib.request.urlopen(urllib.request.Request(u, headers=UA), timeout=40))
LOW = ("low poly", "lowpoly", "low-poly", "stylized", "cartoon", "toon", "voxel", "pixel")
out = {}
for gap, qs in GAPS.items():
    seen = {}
    for q in qs:
        # BlenderKit: free models, CC0 or royalty-free
        try:
            d = get("https://www.blenderkit.com/api/v1/search/?query=" + urllib.parse.quote(q) + "+asset_type:model+is_free:true+order:-score")
            for m in d.get("results", [])[:12]:
                p = m.get("dictParameters") or {}
                n = m["name"]
                if any(w in (n + " " + " ".join(m.get("tags") or [])).lower() for w in LOW): continue
                seen[("bk", m["id"])] = dict(src="BlenderKit", name=n, lic={"cc_zero": "CC0", "royalty_free": "Royalty-free"}.get(m["license"], m["license"]),
                    faces=p.get("faceCount"), dims=[p.get("dimensionX"), p.get("dimensionY"), p.get("dimensionZ")],
                    thumb=m.get("thumbnailMiddleUrl"), url="https://www.blenderkit.com/get-blenderkit/" + m["assetBaseId"] + "/",
                    score=(m.get("ratingsAverage") or {}).get("quality") or 0, author=(m.get("author") or {}).get("fullName"), q=q)
        except Exception as e:
            print("bk err", q, e)
        # Sketchfab: downloadable, CC0 and CC-BY
        for lic in ("cc0", "by"):
            try:
                d = get("https://api.sketchfab.com/v3/search?type=models&downloadable=true&sort_by=-likeCount&count=12&license=" + lic + "&q=" + urllib.parse.quote(q))
                for m in d.get("results", []):
                    n = m["name"]
                    if any(w in (n + " " + " ".join(t["name"] for t in m.get("tags", []))).lower() for w in LOW): continue
                    th = sorted(m.get("thumbnails", {}).get("images", []), key=lambda i: abs(i["width"] - 512))
                    seen[("sf", m["uid"])] = dict(src="Sketchfab", name=n, lic="CC0" if lic == "cc0" else "CC-BY",
                        faces=m.get("faceCount"), dims=None, thumb=th[0]["url"] if th else None, url=m["viewerUrl"],
                        score=m.get("likeCount", 0), author=(m.get("user") or {}).get("displayName"), q=q)
            except Exception as e:
                print("sf err", q, lic, e)
            time.sleep(0.3)
    out[gap] = list(seen.values())
    print(gap, len(seen))
json.dump(out, open("raw.json", "w"), indent=1)
