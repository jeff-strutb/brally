#!/usr/bin/env python3
"""Retexture a finished image-to-3D task from a style image (resumes from ledger).
usage: retex.py <id> <input task id> <style.png>"""
import base64, json, os, sys, time, urllib.request, urllib.error, pathlib
API = "https://api.meshy.ai/openapi"
KEY = next(l.split("=", 1)[1].strip() for l in open(os.path.expanduser("~/.config/readington/meshy.env")) if l.startswith("MESHY_API_KEY="))
H = {"Authorization": f"Bearer {KEY}", "Content-Type": "application/json"}
OUT = pathlib.Path(__file__).parent.parent / "out"
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
cid, tid, img = sys.argv[1], sys.argv[2], sys.argv[3]
led_p = OUT / f"{cid}.json"; led = json.loads(led_p.read_text()) if led_p.exists() else {}
if "task" not in led:
    body = {"input_task_id": tid, "image_style_url": "data:image/png;base64," + base64.b64encode(open(img, "rb").read()).decode(),
            "ai_model": "meshy-7", "enable_original_uv": True, "enable_pbr": True, "texture_resolution": "4k", "target_formats": ["glb"]}
    led = {"task": req("POST", "/v1/retexture", body)["result"]}; led_p.write_text(json.dumps(led))
    print(cid, "task", led["task"], flush=True)
while True:
    t = req("GET", f"/v1/retexture/{led['task']}")
    if t["status"] in ("SUCCEEDED", "FAILED", "CANCELED"): break
    time.sleep(10)
print(cid, t["status"], "credits", t.get("consumed_credits"), t.get("task_error"), flush=True)
if t["status"] == "SUCCEEDED":
    urllib.request.urlretrieve(t["model_urls"]["glb"], OUT / f"{cid}.glb")
    print("balance", req("GET", "/v1/balance")["balance"])
