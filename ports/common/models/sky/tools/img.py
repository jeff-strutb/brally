#!/usr/bin/env python3
"""Meshy text-to-image / image-to-image for the Remastered skies (resumes from ledger).
usage: img.py <id> <model> <aspect|-> <prompt file> [ref.png ...]   (refs -> image-to-image)"""
import base64, json, os, sys, time, urllib.request, urllib.error, pathlib

API = "https://api.meshy.ai/openapi"
KEY = next(l.split("=", 1)[1].strip() for l in open(os.path.expanduser("~/.config/readington/meshy.env")) if l.startswith("MESHY_API_KEY="))
H = {"Authorization": f"Bearer {KEY}", "Content-Type": "application/json"}
OUT = pathlib.Path(__file__).parent.parent / "out"; OUT.mkdir(exist_ok=True)

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

cid, model, aspect, pfile, refs = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5:]
kind = "image-to-image" if refs else "text-to-image"
uri = lambda p: "data:image/png;base64," + base64.b64encode(open(p, "rb").read()).decode()
led_p = OUT / f"{cid}.json"; led = json.loads(led_p.read_text()) if led_p.exists() else {}
if "task" not in led:
    body = {"ai_model": model, "prompt": open(pfile).read().strip()}
    if aspect != "-": body["aspect_ratio"] = aspect
    if refs: body["reference_image_urls"] = [uri(p) for p in refs]
    led = {"task": req("POST", f"/v1/{kind}", body)["result"], "kind": kind, "model": model}
    led_p.write_text(json.dumps(led))
while True:
    t = req("GET", f"/v1/{led['kind']}/{led['task']}")
    if t["status"] in ("SUCCEEDED", "FAILED", "CANCELED"): break
    time.sleep(5)
print(cid, t["status"], t.get("consumed_credits"), t.get("task_error"), flush=True)
for i, u in enumerate(t.get("image_urls") or []):
    p = OUT / f"{cid}_{i}.png"; urllib.request.urlretrieve(u, p); print(p)
