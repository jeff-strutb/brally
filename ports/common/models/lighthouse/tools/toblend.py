#!/usr/bin/env python3
"""Fetch a finished Meshy task's model as a .blend (format conversion only, no remesh).
usage: toblend.py <id> <task_id>   -> ../source/<id>_raw.blend (resumes from ledger)"""
import json, os, sys, time, urllib.request, urllib.error, pathlib
API = "https://api.meshy.ai/openapi"
KEY = next(l.split("=", 1)[1].strip() for l in open(os.path.expanduser("~/.config/readington/meshy.env")) if l.startswith("MESHY_API_KEY="))
H = {"Authorization": f"Bearer {KEY}", "Content-Type": "application/json"}
SRC = pathlib.Path(__file__).parent.parent / "source"; SRC.mkdir(exist_ok=True)
def req(method, path, body=None):
    r = urllib.request.Request(API + path, method=method, headers=H, data=json.dumps(body).encode() if body else None)
    try:
        with urllib.request.urlopen(r, timeout=180) as f: return json.load(f)
    except urllib.error.HTTPError as e: sys.exit(f"{e.code}: {e.read().decode()[:500]}")
cid, tid = sys.argv[1], sys.argv[2]
led_p = SRC / f"{cid}_raw.meshy.json"; led = json.loads(led_p.read_text()) if led_p.exists() else {"task": tid}
if "convert" not in led:
    led["convert"] = req("POST", "/v1/remesh", {"input_task_id": tid, "convert_format_only": True, "target_formats": ["blend"]})["result"]
    led_p.write_text(json.dumps(led))
while True:
    t = req("GET", f"/v1/remesh/{led['convert']}")
    if t["status"] in ("SUCCEEDED", "FAILED", "CANCELED"): break
    time.sleep(5)
print(cid, t["status"], "credits", t.get("consumed_credits"), t.get("task_error"), list((t.get("model_urls") or {}).keys()), flush=True)
if t["status"] == "SUCCEEDED":
    urllib.request.urlretrieve(t["model_urls"]["blend"], SRC / f"{cid}_raw.blend")
    print("balance", req("GET", "/v1/balance")["balance"])
