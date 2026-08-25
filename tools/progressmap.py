#!/usr/bin/env python3
"""Decomp progress treemap (SM64DS-style) for BRGlide.dll.

Joins config/functions_glide.csv (the full Glide function universe) against
build/match/report.csv (per-function match status) and emits a single
self-contained HTML file: one squarified treemap, boxes grouped by module,
cells sized by original byte size, colored by status.

  green = byte-exact match   amber = tagged, diffs remain   gray = untranscribed

Usage:  python3 tools/progressmap.py [-o build/match/map.html]
"""
import argparse, csv, html, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FUNCS = os.environ.get("BR_MAP", os.path.join(ROOT, "config", "functions_glide.csv"))
REPORT = os.path.join(ROOT, "build", "match", "report.csv")

GREEN, AMBER, GRAY = "#3fb950", "#d29922", "#c8ccd2"


def load():
    rep = {}
    with open(REPORT) as f:
        for r in csv.DictReader(f):
            rep[int(r["va"], 16)] = r
    funcs = []
    with open(FUNCS) as f:
        for r in csv.DictReader(f):
            va, size = int(r["va"], 16), int(r["size"])
            if size <= 0:
                continue
            m = rep.get(va)
            funcs.append({
                "va": va, "size": size,
                "name": (m and m["name"]) or r.get("name") or "",
                "file": (m and m["file"]) or "",
                "status": ("match" if m and m["status"] == "match" else
                           "diff" if m else "todo"),
                "diffs": int(m["diffs"]) if m else -1,
            })
    return funcs


def group_key(fn):
    if fn["file"]:
        p = fn["file"]
        p = p[len("src/"):] if p.startswith("src/") else p
        return p[:-2] if p.endswith(".c") else p
    return "unfiled 0x%04Xxxxx" % (fn["va"] >> 16)


# --- squarified treemap ------------------------------------------------------

def squarify(items, x, y, w, h):
    """items: list of (weight, payload), pre-sorted desc. Yields (x,y,w,h,payload)."""
    items = [it for it in items if it[0] > 0]
    total = sum(it[0] for it in items)
    if not items or total <= 0 or w <= 0 or h <= 0:
        return
    scale = w * h / total
    i = 0
    while i < len(items):
        vertical = w < h  # lay row along the shorter side
        side = w if vertical else h
        row, rs = [], 0.0
        worst = None
        j = i
        while j < len(items):
            a = items[j][0] * scale
            nrs = rs + a
            thick = nrs / side
            wr = 0.0
            for k in range(i, j + 1):
                ak = items[k][0] * scale
                cell = ak / thick
                r = max(cell / thick, thick / cell)
                wr = max(wr, r)
            if worst is not None and wr > worst:
                break
            worst, rs = wr, nrs
            row.append(items[j])
            j += 1
        thick = rs / side
        off = 0.0
        for wt, payload in row:
            length = (wt * scale) / thick
            if vertical:
                yield (x + off, y, length, thick, payload)
            else:
                yield (x, y + off, thick, length, payload)
            off += length
        if vertical:
            y += thick; h -= thick
        else:
            x += thick; w -= thick
        i = j


W, H = 1600, 900


def stats(funcs):
    return dict(
        total_b=sum(f["size"] for f in funcs),
        match_b=sum(f["size"] for f in funcs if f["status"] == "match"),
        tag_b=sum(f["size"] for f in funcs if f["status"] != "todo"),
        n_match=sum(1 for f in funcs if f["status"] == "match"),
        n=len(funcs))


def layout(funcs):
    """Yields ('group', x,y,w,h, name) and ('fn', x,y,w,h, fn, group_name)."""
    groups = {}
    for f in funcs:
        groups.setdefault(group_key(f), []).append(f)
    gitems = sorted(((sum(f["size"] for f in fs), (k, fs)) for k, fs in groups.items()),
                    reverse=True, key=lambda it: it[0])
    for gx, gy, gw, gh, (gname, fs) in squarify(gitems, 0, 0, W, H):
        pad = 1.5
        show_label = gw > 70 and gh > 26
        top = 14 if show_label else 0
        yield ("group", gx, gy, gw, gh, gname if show_label else None, None)
        ix, iy = gx + pad, gy + pad + top
        iw, ih = max(gw - 2 * pad, 0.1), max(gh - 2 * pad - top, 0.1)
        fitems = sorted(((f["size"], f) for f in fs), reverse=True, key=lambda it: it[0])
        for fx, fy, fw, fh, f in squarify(fitems, ix, iy, iw, ih):
            yield ("fn", fx, fy, max(fw - .6, .4), max(fh - .6, .4), f, gname)


def tooltip(f, gname):
    tip = "%s  0x%08X  %dB" % (f["name"] or "(unnamed)", f["va"], f["size"])
    if f["status"] == "diff":
        tip += "  %d diff bytes" % f["diffs"]
    return tip + "  [%s]" % gname


def render_svg(funcs, out_path):
    s = stats(funcs)
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" '
             'font-family="sans-serif">' % (W, H + 30),
             '<rect width="%d" height="%d" fill="#161b22"/>' % (W, H + 30)]
    for kind, x, y, w, h, obj, gname in layout(funcs):
        if kind == "group":
            if obj:
                parts.append('<text x="%.1f" y="%.1f" font-size="10" fill="#8b949e">%s</text>'
                             % (x + 3, y + 10.5, html.escape(obj.split("/")[-1])))
        else:
            color = {"match": GREEN, "diff": AMBER, "todo": GRAY}[obj["status"]]
            parts.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s">'
                         '<title>%s</title></rect>'
                         % (x, y, w, h, color, html.escape(tooltip(obj, gname))))
    parts.append('<text x="8" y="%d" font-size="13" fill="#e6edf3">'
                 'byte-exact %d/%d functions &#183; %s / %s bytes exact (%.1f%%) &#183; '
                 '%s bytes transcribed (%.1f%%)</text></svg>'
                 % (H + 20, s["n_match"], s["n"], "{:,}".format(s["match_b"]),
                    "{:,}".format(s["total_b"]), 100.0 * s["match_b"] / s["total_b"],
                    "{:,}".format(s["tag_b"]), 100.0 * s["tag_b"] / s["total_b"]))
    with open(out_path, "w") as f:
        f.write("\n".join(parts))
    print("%s: svg snapshot" % out_path)


def render(funcs, out_path):
    s = stats(funcs)
    total_b, match_b, tag_b = s["total_b"], s["match_b"], s["tag_b"]
    n_match = s["n_match"]

    ngroups = 0
    cells = []
    for kind, x, y, w, h, obj, gname in layout(funcs):
        if kind == "group":
            ngroups += 1
            cells.append(
                '<div class="g" style="left:%.1fpx;top:%.1fpx;width:%.1fpx;height:%.1fpx">%s</div>'
                % (x, y, w, h,
                   '<span class="gl">%s</span>' % html.escape(obj.split("/")[-1]) if obj else ""))
        else:
            color = {"match": GREEN, "diff": AMBER, "todo": GRAY}[obj["status"]]
            cells.append(
                '<div class="f" title="%s" style="left:%.1fpx;top:%.1fpx;width:%.1fpx;'
                'height:%.1fpx;background:%s"></div>'
                % (html.escape(tooltip(obj, gname), quote=True), x, y, w, h, color))

    page = """<!doctype html><meta charset="utf-8"><title>BRGlide decomp map</title>
<style>
 body{margin:0;background:#0d1117;color:#e6edf3;font:13px/1.5 -apple-system,Segoe UI,sans-serif}
 header{padding:14px 20px 10px}
 h1{font-size:16px;margin:0 0 6px}
 .stats span{margin-right:22px;color:#9da7b3}
 .stats b{color:#e6edf3}
 .bar{height:8px;border-radius:4px;background:%(gray)s;overflow:hidden;display:flex;margin:8px 0 2px;max-width:820px}
 .legend i{display:inline-block;width:10px;height:10px;border-radius:2px;margin:0 5px 0 18px;vertical-align:-1px}
 #map{position:relative;width:%(W)dpx;height:%(H)dpx;margin:10px 20px 24px;background:#161b22;border-radius:6px;overflow:hidden}
 .g{position:absolute;outline:1px solid #0d1117}
 .gl{position:absolute;top:0;left:3px;font-size:10px;color:#8b949e;white-space:nowrap;overflow:hidden;max-width:95%%;z-index:2}
 .f{position:absolute;border-radius:1px}
 .f:hover{outline:1px solid #fff;z-index:3}
</style>
<header>
 <h1>BRGlide.dll — matching decomp progress</h1>
 <div class="stats">
  <span>byte-exact: <b>%(nm)d / %(nf)d</b> functions (%(nmp).1f%%)</span>
  <span>bytes exact: <b>%(mb)s / %(tb)s</b> (%(mbp).1f%%)</span>
  <span>bytes transcribed (tagged): <b>%(gb)s</b> (%(gbp).1f%%)</span>
 </div>
 <div class="bar"><i style="width:%(mbp).2f%%;background:%(green)s"></i><i style="width:%(dbp).2f%%;background:%(amber)s"></i></div>
 <div class="legend"><i style="background:%(green)s"></i>byte-exact<i style="background:%(amber)s"></i>tagged, diffs remain<i style="background:%(gray)s"></i>untranscribed</div>
</header>
<div id="map">%(cells)s</div>
""" % dict(W=W, H=H, cells="".join(cells), green=GREEN, amber=AMBER, gray=GRAY,
           nm=n_match, nf=len(funcs), nmp=100.0 * n_match / len(funcs),
           mb="{:,}".format(match_b), tb="{:,}".format(total_b),
           mbp=100.0 * match_b / total_b, gb="{:,}".format(tag_b),
           gbp=100.0 * tag_b / total_b, dbp=100.0 * (tag_b - match_b) / total_b)
    with open(out_path, "w") as f:
        f.write(page)
    print("%s: %d functions, %d groups" % (out_path, len(funcs), ngroups))
    print("byte-exact %d/%d functions, %d/%d bytes (%.1f%%)"
          % (n_match, len(funcs), match_b, total_b, 100.0 * match_b / total_b))


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", default=os.path.join(ROOT, "build", "match", "map.html"))
    ap.add_argument("--svg", help="also write an SVG snapshot (for the README)")
    a = ap.parse_args()
    funcs = load()
    render(funcs, a.o)
    if a.svg:
        render_svg(funcs, a.svg)
