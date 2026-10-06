import json, urllib.request, io, hashlib, os
from concurrent.futures import ThreadPoolExecutor
from PIL import Image, ImageDraw
raw = json.load(open("raw.json"))
os.makedirs("th", exist_ok=True)
def fetch(c):
    if not c.get("thumb"): return None
    p = "th/" + hashlib.md5(c["thumb"].encode()).hexdigest() + ".jpg"
    if not os.path.exists(p):
        try:
            d = urllib.request.urlopen(urllib.request.Request(c["thumb"], headers={"User-Agent": "Mozilla/5.0"}), timeout=40).read()
            Image.open(io.BytesIO(d)).convert("RGB").save(p)
        except Exception: return None
    c["th"] = p
    return p
allc = [c for v in raw.values() for c in v]
list(ThreadPoolExecutor(16).map(fetch, allc))
json.dump(raw, open("raw.json", "w"), indent=1)
C = 200; COLS = 8
for gi, (gap, cs) in enumerate(raw.items()):
    cs = [c for c in cs if c.get("th") and (c["faces"] or 0) >= 3000]
    cs.sort(key=lambda c: -(c["faces"] or 0))
    for n, c in enumerate(cs): c["sheet"] = f"{gi}:{n}"
    S = Image.new("RGB", (COLS * C, ((len(cs) + COLS - 1) // COLS) * (C + 28)), (30, 30, 30)); d = ImageDraw.Draw(S)
    for n, c in enumerate(cs):
        im = Image.open(c["th"]); im.thumbnail((C, C)); x, y = (n % COLS) * C, (n // COLS) * (C + 28)
        S.paste(im, (x, y)); d.text((x + 2, y + C), f"{n} {c['src'][:2]} {c['lic']} {int((c['faces'] or 0)/1000)}k", fill=(255, 255, 0))
        d.text((x + 2, y + C + 12), c["name"][:30], fill=(200, 200, 200))
    S.save(f"gap{gi}.png"); print(gi, gap, len(cs))
json.dump(raw, open("raw.json", "w"), indent=1)
