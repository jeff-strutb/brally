#!/usr/bin/env python3
"""Meshy text-to-3D for the Remastered player car. Key from ~/.config/readington/meshy.env.
usage: gen.py <id> <shape prompt> <texture prompt>   (resumes from ledger)"""
import json, os, sys, time, urllib.request, pathlib

API = "https://api.meshy.ai/openapi"
KEY = next(l.split("=", 1)[1].strip() for l in open(os.path.expanduser("~/.config/readington/meshy.env")) if l.startswith("MESHY_API_KEY="))
H = {"Authorization": f"Bearer {KEY}", "Content-Type": "application/json"}
OUT = pathlib.Path(__file__).parent / "out"
OUT.mkdir(exist_ok=True)
RESERVE = 200

def req(method, path, body=None):
    r = urllib.request.Request(API + path, method=method, headers=H, data=json.dumps(body).encode() if body else None)
    with urllib.request.urlopen(r, timeout=60) as f:
        return json.load(f)

def wait(tid, label):
    last = None
    while True:
        t = req("GET", f"/v2/text-to-3d/{tid}")
        if t["progress"] != last:
            print(f"{label} {t['status']} {t['progress']}%", flush=True); last = t["progress"]
        if t["status"] in ("SUCCEEDED", "FAILED", "CANCELED"):
            return t
        time.sleep(6)

cid, shape, tex = sys.argv[1], sys.argv[2], sys.argv[3]
led_p = OUT / f"{cid}.json"
led = json.loads(led_p.read_text()) if led_p.exists() else {}
bal = req("GET", "/v1/balance")["balance"]
print(f"{cid}: balance {bal}", flush=True)
if bal <= RESERVE:
    sys.exit("balance at reserve")
if "preview" not in led:
    led["preview"] = req("POST", "/v2/text-to-3d", {
        "mode": "preview", "prompt": shape, "ai_model": "latest",
        "topology": "triangle", "should_remesh": True, "target_polycount": 200000,
        "symmetry_mode": "on", "target_formats": ["glb"]})["result"]
    led_p.write_text(json.dumps(led))
p = wait(led["preview"], cid + " preview")
if p["status"] != "SUCCEEDED":
    sys.exit(f"preview failed: {p.get('task_error')}")
if "refine" not in led:
    led["refine"] = req("POST", "/v2/text-to-3d", {
        "mode": "refine", "preview_task_id": led["preview"], "ai_model": "latest",
        "enable_pbr": True, "texture_resolution": "4k", "texture_prompt": tex,
        "target_formats": ["glb"]})["result"]
    led_p.write_text(json.dumps(led))
r = wait(led["refine"], cid + " refine")
if r["status"] != "SUCCEEDED":
    sys.exit(f"refine failed: {r.get('task_error')}")
urllib.request.urlretrieve(r["model_urls"]["glb"], OUT / f"{cid}.glb")
if r.get("thumbnail_url"):
    urllib.request.urlretrieve(r["thumbnail_url"], OUT / f"{cid}.png")
print(f"{cid}: done, balance {req('GET', '/v1/balance')['balance']}", flush=True)
