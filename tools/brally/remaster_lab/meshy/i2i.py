#!/usr/bin/env python3
"""Meshy image-to-image: game renders -> modern concept views.
usage: i2i.py <id> <model> <multiview 0|1> <prompt file> <ref.png>..."""
import base64, json, os, sys, time, urllib.request, pathlib

API = "https://api.meshy.ai/openapi"
KEY = next(l.split("=", 1)[1].strip() for l in open(os.path.expanduser("~/.config/readington/meshy.env")) if l.startswith("MESHY_API_KEY="))
H = {"Authorization": f"Bearer {KEY}", "Content-Type": "application/json"}
OUT = pathlib.Path(__file__).parent / "out"; OUT.mkdir(exist_ok=True)

def req(method, path, body=None):
    r = urllib.request.Request(API + path, method=method, headers=H, data=json.dumps(body).encode() if body else None)
    try:
        with urllib.request.urlopen(r, timeout=120) as f: return json.load(f)
    except urllib.error.HTTPError as e:
        sys.exit(f"{e.code}: {e.read().decode()[:500]}")

cid, model, mv, pfile, refs = sys.argv[1], sys.argv[2], sys.argv[3] == "1", sys.argv[4], sys.argv[5:]
uri = lambda p: "data:image/png;base64," + base64.b64encode(open(p, "rb").read()).decode()
led_p = OUT / f"{cid}.json"; led = json.loads(led_p.read_text()) if led_p.exists() else {}
if "task" not in led:
    led["task"] = req("POST", "/v1/image-to-image", {"ai_model": model, "prompt": open(pfile).read().strip(),
        "reference_image_urls": [uri(p) for p in refs], "generate_multi_view": mv, **({} if mv else {"aspect_ratio": "1:1"})})["result"]
    led_p.write_text(json.dumps(led))
while True:
    t = req("GET", f"/v1/image-to-image/{led['task']}")
    if t["status"] in ("SUCCEEDED", "FAILED", "CANCELED"): break
    time.sleep(5)
print(cid, t["status"], t.get("consumed_credits"), t.get("task_error"))
for i, u in enumerate(t.get("image_urls") or []):
    urllib.request.urlretrieve(u, OUT / f"{cid}_{i}.png"); print(OUT / f"{cid}_{i}.png")
