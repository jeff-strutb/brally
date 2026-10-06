#!/usr/bin/env python3
"""Meshy image(s)-to-3D. usage: to3d.py <id> <polycount> <texture prompt> <img>..."""
import base64, json, os, sys, time, urllib.request, pathlib

API = "https://api.meshy.ai/openapi"
KEY = next(l.split("=", 1)[1].strip() for l in open(os.path.expanduser("~/.config/readington/meshy.env")) if l.startswith("MESHY_API_KEY="))
H = {"Authorization": f"Bearer {KEY}", "Content-Type": "application/json"}
OUT = pathlib.Path(__file__).parent / "out"; OUT.mkdir(exist_ok=True)

def req(method, path, body=None):
    r = urllib.request.Request(API + path, method=method, headers=H, data=json.dumps(body).encode() if body else None)
    try:
        with urllib.request.urlopen(r, timeout=300) as f: return json.load(f)
    except urllib.error.HTTPError as e:
        sys.exit(f"{e.code}: {e.read().decode()[:800]}")

cid, poly, tp, imgs = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4:]
REMESH = poly > 0
uri = lambda p: "data:image/png;base64," + base64.b64encode(open(p, "rb").read()).decode()
multi = len(imgs) > 1
ep = "/v1/multi-image-to-3d" if multi else "/v1/image-to-3d"
led_p = OUT / f"{cid}.json"; led = json.loads(led_p.read_text()) if led_p.exists() else {}
if "task" not in led:
    body = {"ai_model": "latest", "geometry_resolution": "2k", "should_texture": True, "enable_pbr": True,
            "texture_resolution": "4k", "texture_prompt": tp, "should_remesh": REMESH, "topology": "triangle",
            **({"target_polycount": poly} if REMESH else {}), "remove_lighting": True, "target_formats": ["glb"]}
    if multi:
        body["image_urls"] = [uri(p) for p in imgs]
    else:
        body["image_url"] = uri(imgs[0]); body["texture_image_url"] = uri(imgs[0])
    led["task"] = req("POST", ep, body)["result"]; led_p.write_text(json.dumps(led))
last = None
while True:
    t = req("GET", f"{ep}/{led['task']}")
    if t["progress"] != last: print(cid, t["status"], t["progress"], flush=True); last = t["progress"]
    if t["status"] in ("SUCCEEDED", "FAILED", "CANCELED"): break
    time.sleep(8)
print(cid, t["status"], t.get("consumed_credits"), t.get("task_error"), flush=True)
if t["status"] == "SUCCEEDED":
    urllib.request.urlretrieve(t["model_urls"]["glb"], OUT / f"{cid}.glb")
    if t.get("thumbnail_url"): urllib.request.urlretrieve(t["thumbnail_url"], OUT / f"{cid}_thumb.png")
    print("saved", OUT / f"{cid}.glb", flush=True)
