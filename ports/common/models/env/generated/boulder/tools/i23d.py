#!/usr/bin/env python3
"""Meshy image-to-3D for the Remastered lighthouse (resumes from ledger).
Same settings as the car (2k geometry, 4k PBR textures, no remesh) on meshy-7.1.
usage: i23d.py <id> <image.png>"""
import base64, json, os, sys, time, urllib.request, urllib.error, pathlib

API = "https://api.meshy.ai/openapi"
KEY = next(l.split("=", 1)[1].strip() for l in open(os.path.expanduser("~/.config/readington/meshy.env")) if l.startswith("MESHY_API_KEY="))
H = {"Authorization": f"Bearer {KEY}", "Content-Type": "application/json"}
OUT = pathlib.Path(__file__).parent.parent / "out"; OUT.mkdir(exist_ok=True)
RESERVE = 200

def req(method, path, body=None):
    for tries in range(5):
        r = urllib.request.Request(API + path, method=method, headers=H, data=json.dumps(body).encode() if body else None)
        try:
            with urllib.request.urlopen(r, timeout=180) as f: return json.load(f)
        except urllib.error.HTTPError as e:
            msg = e.read().decode()[:500]
            if e.code in (429, 500, 502, 503, 504): time.sleep(10 * (tries + 1)); continue
            sys.exit(f"{e.code}: {msg}")
        except (urllib.error.URLError, TimeoutError): time.sleep(10)
    sys.exit("gave up")

cid, img = sys.argv[1], sys.argv[2]
led_p = OUT / f"{cid}.json"; led = json.loads(led_p.read_text()) if led_p.exists() else {}
if "task" not in led:
    bal = req("GET", "/v1/balance")["balance"]
    print(f"{cid}: balance {bal}", flush=True)
    if bal <= RESERVE: sys.exit("balance at reserve")
    body = {"image_url": "data:image/png;base64," + base64.b64encode(open(img, "rb").read()).decode(),
            "ai_model": "meshy-7.1", "model_type": "standard", "geometry_resolution": "2k",
            "should_remesh": False, "should_texture": True, "enable_pbr": True,
            "texture_resolution": "4k", "remove_lighting": True, "target_formats": ["glb"]}
    led = {"task": req("POST", "/v1/image-to-3d", body)["result"]}
    led_p.write_text(json.dumps(led))
    print(f"{cid}: task {led['task']}", flush=True)
last = None
while True:
    t = req("GET", f"/v1/image-to-3d/{led['task']}")
    if t["progress"] != last: print(f"{cid} {t['status']} {t['progress']}%", flush=True); last = t["progress"]
    if t["status"] in ("SUCCEEDED", "FAILED", "CANCELED"): break
    time.sleep(10)
print(cid, t["status"], "credits", t.get("consumed_credits"), t.get("task_error"), flush=True)
if t["status"] == "SUCCEEDED":
    urllib.request.urlretrieve(t["model_urls"]["glb"], OUT / f"{cid}.glb")
    for k, u in (t.get("thumbnail_urls") or {}).items():
        urllib.request.urlretrieve(u, OUT / f"{cid}_{k}.png")
    if t.get("thumbnail_url"): urllib.request.urlretrieve(t["thumbnail_url"], OUT / f"{cid}.png")
    print(f"{cid}: done, balance {req('GET', '/v1/balance')['balance']}", flush=True)
