#!/usr/bin/env python3
"""remaster_env_place.py -- where the Remastered environment models stand, per
track, read from the retail .TRK.

Remastered replaces the flat cut-out cards (a picture of a pine, a strip of
forest, a bush) with real models at the same places.  Nothing that gameplay
reads changes: the cards have no collision (the track's collision mesh is a
separate array the game never draws), so a model standing where its card stood
is exactly as drivable-through as the card was.  This tool:

  * walks every instance's display list, attributing each triangle command to
    the texture bound when it runs;
  * for the textures listed in RULES, records the byte offset of every
    triangle command drawing them ("hide"): the runtime blanks those commands
    in a Remastered copy of the list, the original list is never touched;
  * groups those triangles into cards (shared corners, then crossed cards
    standing on one spot), and puts models where they stood: one per single
    card, a row along a forest strip, scaled to the card's height ("put").

Writes ports/common/models/placements/<track>.env, text:
    track <name> <faces> <vertices> <instances>     the header's counts, to
                                                    recognise the loaded track
    asset <k> <asset> <variant>
    hide <instance> <offset> ...                    triangle commands, bytes
                                                    from the list's start
    put <instance> <k> m00 .. m33                   row-vector world matrix
"""
import colorsys, json, math, os, random, struct, sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
TRACKS = os.path.join(ROOT, "build", "app", "extract", "disc", "tracks")
MODELS = os.path.join(ROOT, "ports", "common", "models")
BASE = 0x80025C00

CONIFER = ("fir_tree_01", "pine_tree_01", "fir_sapling_medium", "pine_sapling_medium", "fir_sapling", "pine_sapling_small")
BROADLEAF = ("island_tree_01", "island_tree_02", "island_tree_03", "tree_small_02", "jacaranda_tree")
BUSH_DRY = ("searsia_lucida", "searsia_burchellii")
# third-party (Sketchfab, CC0/CC-BY; credits in env/thirdparty/CREDITS.tsv)
CONIFER = CONIFER + ("sf_realistic_fir_trees_pack_lod",)
BROADLEAF = BROADLEAF + ("sf_fagus_sylvatica", "sf_mighty_oak_trees", "sf_deciduous_tree_with_leaves_m", "sf_linden_tree")
JUNGLE = ("sf_realistic_jungle_tree", "sf_ceiba_pentandra_tree", "sf_realistic_jungle_tree_avatar",
          "sf_chinese_banyan_ficus_microca", "sf_jungle_tree")
UNDER = ("sf_tropical_plants_pack_m02p",)
COCONUT = ("sf_coconut_palm",)
FANPALM = ("sf_realistic_hd_mexican_fan_pal", "sf_realistic_hd_mexican_fan_pal_1", "sf_realistic_hd_california_fan")
DESERT_SMALL = ("sf_realistic_hd_beaked_yucca_12", "sf_realistic_hd_spineless_yucca")

# texture (N64 address of its texels) -> what the card shows.  Only cards
# identified with certainty; everything else stays the game's own.
VINES = ("sf_liana_tree", "sf_vine_branch_3d_model_free")
RULES = {
    "bonus": {
        0x8003F3F0: ("strip", BROADLEAF),
        0x8003A4F0: ("strip", BROADLEAF),
        0x8003FBF0: ("strip", BROADLEAF),
        0x800403F0: ("strip", BROADLEAF),
        0x800394F0: ("strip", BUSH_DRY),
    },
    "mountain": {
        0x8006F770: ("strip", CONIFER),
        0x8006EF70: ("strip", CONIFER),
        0x8006E770: ("strip", CONIFER),
        0x80054720: ("strip", CONIFER),     # a row of pine tops
        0x80078130: ("strip", CONIFER),
        0x80043AE0: ("strip", CONIFER),
        0x80076930: ("strip", CONIFER),
        0x80053F20: ("strip", CONIFER),
        0x80078930: ("strip", CONIFER),
        0x80079930: ("strip", CONIFER),
        0x80077130: ("strip", CONIFER),
        0x8007AF30: ("strip", CONIFER),
        0x80077930: ("strip", CONIFER),
        0x80064F30: ("single", CONIFER),    # one conifer
        0x80073170: ("single", CONIFER),
        0x8007E570: ("single", CONIFER),    # a slim conifer
    },
    "race": {
        0x80041370: ("strip", BROADLEAF),
        0x8003C6F0: ("strip", BROADLEAF),
        0x80040B70: ("strip", BROADLEAF),
        0x80030670: ("strip", BROADLEAF),
        0x8003BEF0: ("strip", BROADLEAF),
        0x8003B6F0: ("single", BROADLEAF),  # a broadleaf tree
        0x80038F70: ("single", BROADLEAF),
        0x8005BCF0: ("single", BROADLEAF),
    },
    "desert": {
        0x80074B60: ("single", FANPALM),
        0x8004B808: ("strip", BUSH_DRY),    # a clump of bushes
        0x80071960: ("strip", BROADLEAF),   # a row of trees along the road
        0x800755E0: ("single", DESERT_SMALL),   # a small crossed card: agave / yucca
        0x80072E60: ("single", FANPALM),    # a town palm
    },
    "amazon": {
        0x80054638: ("strip", JUNGLE + UNDER),
        0x80054B78: ("strip", JUNGLE + UNDER),
        0x800450B8: ("strip", JUNGLE + UNDER),
        0x80045B38: ("strip", JUNGLE + UNDER),
        0x80053138: ("strip", UNDER),
        0x80053BB8: ("strip", JUNGLE),
        0x8007B580: ("strip", JUNGLE + UNDER),
        0x80040738: ("strip", UNDER),
        0x800411B8: ("strip", UNDER),
        0x800B0EF0: ("strip", UNDER),
        0x8004CCB8: ("strip", JUNGLE),
        0x80031AB8: ("strip", JUNGLE + UNDER),
        0x80048AB8: ("strip", UNDER),
        0x8005A238: ("strip", JUNGLE),
        0x80061B38: ("strip", VINES),
        0x8003E7B8: ("strip", UNDER),
        0x8003F238: ("strip", UNDER),
        0x80041C38: ("strip", JUNGLE + UNDER),  # jungle walls along the road
        0x8003DD38: ("strip", JUNGLE + UNDER),
        0x8003D2B8: ("strip", JUNGLE + UNDER),
        0x8004C4B8: ("strip", JUNGLE),          # a painted tree line
        0x800874C8: ("strip", UNDER),           # a strip of palm fronds
        0x80086748: ("crown", COCONUT),         # a palm crown, seen from below
    },
    "coast": {
        0x80062D70: ("strip", BUSH_DRY),
        0x800632B0: ("strip", BUSH_DRY),
        0x800607F0: ("single", BROADLEAF),  # a broadleaf crown
    },
}

def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]

def off(a):
    return a - BASE if a >= 0x80000000 else None

def walk(b, o):
    """(offset, texels address, [triangles as vertex-slot triples]) per tri command,
    and the vertex slots' positions as each command sees them."""
    vbuf = [None] * 64
    timg = tex = 0
    fmt = siz = tw = th = 0
    out = []
    start = o
    while o + 8 <= len(b):
        w0, w1 = be32(b, o), be32(b, o + 4)
        op = w0 >> 24
        if op == 0xFD:
            timg = w1
        elif op == 0xF3:
            tex = timg
        elif op == 0xF5 and (w1 >> 24 & 7) == 0:
            fmt, siz = w0 >> 21 & 7, w0 >> 19 & 3
        elif op == 0xF2 and (w1 >> 24 & 7) == 0:
            tw = (w1 >> 12 & 0xFFF) // 4 - (w0 >> 12 & 0xFFF) // 4 + 1
            th = (w1 & 0xFFF) // 4 - (w0 & 0xFFF) // 4 + 1
        elif op == 0x04:
            n = (w0 >> 10) & 0x3F
            v0 = ((w0 >> 16) & 0xFF) // 2
            vo = off(w1)
            if vo is not None:
                for k in range(n):
                    if v0 + k < 64:
                        vbuf[v0 + k] = struct.unpack_from(">hhh", b, vo + 16 * k)
        elif op in (0xBF, 0xB1):
            tris = []
            for w in ([w0, w1] if op == 0xB1 else [w1]):
                p = [vbuf[(w >> s & 0xFF) // 2] for s in (16, 8, 0)]
                if all(q is not None for q in p):
                    tris.append(p)
            out.append((o - start, tex, tris, (fmt, siz, tw, th)))
        elif op == 0xB8:
            break
        o += 8
    return out

def cards(P):
    """Group triangles (n,3,3 world points) into cards: shared corners, then
    cards standing on the same spot (crossed pairs)."""
    n = len(P)
    parent = list(range(n))
    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    seen = {}
    for i in range(n):
        for v in P[i]:
            k = tuple(np.round(v, 2))
            if k in seen:
                parent[find(i)] = find(seen[k])
            else:
                seen[k] = i
    groups = {}
    for i in range(n):
        groups.setdefault(find(i), []).append(i)
    G = [np.concatenate([P[i] for i in g]) for g in groups.values()]
    # crossed cards: xy centres closer than a third of the smaller width
    merged = True
    while merged:
        merged = False
        for a in range(len(G)):
            for c in range(a + 1, len(G)):
                ca, cc = G[a][:, :2].mean(0), G[c][:, :2].mean(0)
                wa = np.ptp(G[a][:, :2], 0).max()
                wc = np.ptp(G[c][:, :2], 0).max()
                if np.linalg.norm(ca - cc) < max(0.3, min(wa, wc) / 3):
                    G[a] = np.concatenate([G[a], G[c]])
                    del G[c]
                    merged = True
                    break
            if merged:
                break
    return G

def road_points(b):
    """The racing line's ring (header +0x70, include/br_ai.h): every node's
    points as (centre, half width), following next and sibling links."""
    seen = set()
    stack = [be32(b, 0x70), be32(b, 0x74)]
    nodes = []
    while stack:
        a = stack.pop()
        o = off(a) if a else None
        if o is None or o in seen or o + 0x40 > len(b):
            continue
        seen.add(o)
        n = struct.unpack_from(">H", b, o + 0x14)[0]
        run = []
        for k in range(n + 1):
            q = o + 0x40 + 0x28 * k
            if q + 0x24 > len(b):
                break
            L = np.array(struct.unpack_from(">3f", b, q))
            C = np.array(struct.unpack_from(">3f", b, q + 0x0C))
            R = np.array(struct.unpack_from(">3f", b, q + 0x18))
            run.append((C, max(np.linalg.norm(L - C), np.linalg.norm(R - C))))
        nodes.append(run)
        stack += [be32(b, o), be32(b, o + 4)]
    # every node's points are in order along the road: fill in between them
    # every metre, so a spot between two samples is measured against the
    # road itself and not the nearest sample (they are up to 70 m apart)
    C, W = [], []
    for run in nodes:
        for (c0, w0), (c1, w1) in zip(run, run[1:]):
            n = max(1, int(np.linalg.norm(c1[:2] - c0[:2])))
            for k in range(n):
                t = k / n
                C.append(c0 + (c1 - c0) * t); W.append(w0 + (w1 - w0) * t)
        if run:
            C.append(run[-1][0]); W.append(run[-1][1])
    return np.array(C), np.array(W)

class Ground:
    """The drawn ground: every upward-facing triangle of the track's own
    scenery (cards and other cut-outs excluded), for the height under a spot."""
    def __init__(self, tris):
        T = np.array(tris)
        n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
        up = n[:, 2] / np.maximum(np.linalg.norm(n, axis=1), 1e-9) > 0.35
        self.T = T[up]
        self.lo = self.T[:, :, :2].min(1)
        self.hi = self.T[:, :, :2].max(1)
    def height(self, x, y, near_z):
        m = (self.lo[:, 0] <= x) & (self.hi[:, 0] >= x) & (self.lo[:, 1] <= y) & (self.hi[:, 1] >= y)
        best = None
        for t in self.T[m]:
            (ax, ay, az), (bx, by, bz), (cx, cy, cz) = t
            d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(d) < 1e-9:
                continue
            l1 = ((by - cy) * (x - cx) + (cx - bx) * (y - cy)) / d
            l2 = ((cy - ay) * (x - cx) + (ax - cx) * (y - cy)) / d
            l3 = 1 - l1 - l2
            if min(l1, l2, l3) < -1e-4:
                continue
            z = l1 * az + l2 * bz + l3 * cz
            if abs(z - near_z) < 25 and (best is None or abs(z - near_z) < abs(best - near_z)):
                best = z
        return best

# Ground scatter, by what the ground's texture shows.  Per kind: models,
# clumps per square metre, natural scale range, draw distance (m), and how
# close to the racing line it may stand (m beyond the road's half width).
# Nothing a car would be stopped by in reality: scatter has no collision, and
# the game lets a car drive almost anywhere, so boulders would be driven
# through (measured 2026-09-30: moss rocks in open drivable grass).  Grass,
# flowers, ferns and pebbles only.
GRASS = [("grass_medium_01", 1.8, (1.0, 1.6), 90), ("grass_medium_02", 0.5, (1.0, 1.5), 110),
         ("dandelion_01", 0.04, (1.0, 1.3), 50), ("celandine_01", 0.04, (1.0, 1.3), 50),
         ("weed_plant_02", 0.06, (1.0, 1.4), 60),
         ("shrub_01", 0.015, (2.0, 3.5), 160), ("shrub_02", 0.01, (0.5, 0.9), 160)]
EARTH = [("grass_bermuda_01", 0.3, (1.0, 1.6), 70), ("nettle_plant", 0.03, (1.0, 1.4), 60),
         ("shrub_sorrel_01", 0.04, (1.2, 1.8), 50), ("namaqualand_stones_01", 0.05, (0.8, 1.5), 45),
         ("searsia_lucida", 0.006, (0.6, 1.0), 180)]
FOREST = [("fern_02", 0.08, (0.9, 1.4), 90), ("shrub_03", 0.02, (2.0, 3.0), 150)]
SAND = [("namaqualand_stones_01", 0.06, (0.8, 1.6), 45), ("searsia_burchellii", 0.003, (0.5, 0.8), 180)]
SCATTER = {
    "mountain": {"grass": GRASS + FOREST, "earth": EARTH, "foliage": GRASS + FOREST},
    "race": {"grass": GRASS, "earth": EARTH, "foliage": GRASS},
    "amazon": {"grass": GRASS + FOREST, "earth": EARTH, "foliage": GRASS + FOREST},
    "bonus": {"grass": GRASS, "earth": EARTH, "foliage": GRASS},
    "coast": {"grass": GRASS, "earth": EARTH, "sand": SAND, "foliage": GRASS},
    "desert": {"earth": [("namaqualand_stones_01", 0.06, (0.8, 1.6), 35), ("shrub_sorrel_01", 0.02, (1.0, 1.6), 30)],
               "sand": SAND},
}
# painted-foliage walls: what grows in front of them, per track
WALL_TREES = {"amazon": JUNGLE, "mountain": CONIFER, "race": BROADLEAF, "coast": BROADLEAF, "bonus": BROADLEAF,
              "desert": BROADLEAF}
ROAD_CLEAR = {"grass": 1.5, "earth": 4.0, "sand": 4.0, "slope": 10.0, "foliage": 3.0}
# stones keep further off: the racing line's width does not cover every
# dirt road's full width, and loose stones on a road read as debris
STONES = ("namaqualand_stones_01", "sand_rocks_small_01", "stone_01")
STONE_CLEAR = 8.0

_TEXMAT = None

def texmat():
    """ports/macos/wasm/texmat.csv: what every game texture shows, by the
    FNV-1a of its large level as the game uploads it (host_glide.m)."""
    global _TEXMAT
    if _TEXMAT is None:
        _TEXMAT = {}
        for l in open(os.path.join(ROOT, "ports", "macos", "wasm", "texmat.csv")):
            if l.startswith("#") or "," not in l:
                continue
            h, m = l.split(",")[:2]
            _TEXMAT[int(h, 16)] = m.strip()
    return _TEXMAT

# catalogue material -> scatter kind
MAT_KIND = {"grass": "grass", "dirt": "earth", "sand": "sand", "rock": "rock", "foliage": "foliage", "snow": None}

def catalogue_kind(b, tex, tf):
    """The texture's material from the catalogue: RGBA16 textures only (a CI
    texture's palette is filled at run time, so its upload cannot be
    reproduced here).  The RGBA8 the game uploads is the 5551 colour
    expanded (host_glide.m decode(), ARGB1555), level 0, rows as in RAM
    after the odd-row swap is undone."""
    fmt, siz, w, h = tf
    o = off(tex) if tex else None
    if o is None or fmt != 0 or siz != 2 or not (0 < w <= 256 and 0 < h <= 256) or o + 2 * w * h > len(b):
        return None, False
    raw = bytearray(b[o:o + 2 * w * h]); stride = w * 2
    for y in range(1, h, 2):
        r0 = y * stride
        for q in range(r0, r0 + stride - 7, 8):
            raw[q:q + 8] = raw[q + 4:q + 8] + raw[q:q + 4]
    v = np.frombuffer(bytes(raw), ">u2").astype(np.uint64)
    px = ((v >> 11 & 31) * 255 // 31) | (((v >> 6 & 31) * 255 // 31) << 8) | (((v >> 1 & 31) * 255 // 31) << 16) | ((v & 1) * 255 << 24)
    hsh = 2166136261
    for x in px.tolist():
        hsh = ((hsh ^ x) * 16777619) & 0xFFFFFFFF
    m = texmat().get(hsh)
    if m is None:
        return None, False
    return MAT_KIND.get(m), True

def texture_kind(b, tex, tf):
    k, known = catalogue_kind(b, tex, tf)
    if known:
        return k
    return colour_kind(b, tex, tf)

def colour_kind(b, tex, tf):
    """What a ground texture shows, from its mean colour (RGBA16 only;
    CI and the rest are left alone)."""
    fmt, siz, w, h = tf
    o = off(tex) if tex else None
    if o is None or fmt != 0 or siz != 2 or not (0 < w <= 256 and 0 < h <= 256):
        return None
    n = w * h
    if o + n * 2 > len(b):
        return None
    v = np.frombuffer(b, ">u2", n, o).astype(np.int32)
    a = v & 1
    if a.mean() < 0.9:
        return None
    r, g, bl = (v >> 11 & 31) / 31.0, (v >> 6 & 31) / 31.0, (v >> 1 & 31) / 31.0
    hue, sat, val = colorsys.rgb_to_hsv(r.mean(), g.mean(), bl.mean())
    hue *= 360
    if sat < 0.08:
        return "rock"                                  # grey: rock, road, concrete
    if 50 <= hue <= 170:
        return "grass"                                 # olive to green, dry or lush
    if hue < 50 or hue > 340:
        return "sand" if val > 0.62 else "earth"
    return None                                        # blue/purple: water, paint

_VAR = {}

def variants(asset):
    if asset in _VAR:
        return _VAR[asset]
    j = json.load(open(os.path.join(MODELS, asset, "bake.json")))
    _VAR[asset] = [(v["name"], v["bounds"][1][2] - v["bounds"][0][2]) for v in j["variants"]]
    return _VAR[asset]

def yaw_scale(pos, yaw, s):
    c, n = math.cos(yaw) * s, math.sin(yaw) * s
    return [c, n, 0, 0, -n, c, 0, 0, 0, 0, s, 0, pos[0], pos[1], pos[2], 1]

stats = []

def main(track):
    b = open(os.path.join(TRACKS, track + ".trk"), "rb").read()
    rules = RULES.get(track, {})
    ia, ci = be32(b, 0x60), be32(b, 0x64)
    rng = random.Random(track)
    assets = {}
    lines = [f"track {track} {be32(b, 0x08)} {be32(b, 0x10)} {ci}"]
    hides, puts = [], []
    road_c, road_w = road_points(b)
    # the drawn ground, from every instance (first pass)
    gtris, gsrc = [], []
    for i in range(ci):
        r = off(ia) + i * 0x54
        M = np.array(struct.unpack_from(">16f", b, r), dtype=np.float64).reshape(4, 4)
        dl = be32(b, r + 0x44)
        if not dl:
            continue
        for _, tex, tris, tf in walk(b, off(dl)):
            if tex in rules:
                continue
            for t in tris:
                w = (np.hstack([np.array(t, float), np.ones((3, 1))]) @ M)[:, :3]
                gtris.append(w)
                gsrc.append((i, tex, tf))
    ground = Ground(gtris)

    # a 4 m grid over the road samples, for the nearest one quickly
    cell = 4.0
    grid = {}
    for k, c in enumerate(road_c):
        grid.setdefault((int(c[0] // cell), int(c[1] // cell)), []).append(k)

    def clear_of_road(p, margin):
        reach = int((road_w.max() + margin) // cell) + 1
        gx, gy = int(p[0] // cell), int(p[1] // cell)
        best, bk = 1e18, -1
        for dx in range(-reach, reach + 1):
            for dy in range(-reach, reach + 1):
                for k in grid.get((gx + dx, gy + dy), ()):
                    d = (road_c[k][0] - p[0]) ** 2 + (road_c[k][1] - p[1]) ** 2
                    if d < best:
                        best, bk = d, k
        if bk < 0:
            d = np.linalg.norm(road_c[:, :2] - p[:2], axis=1)
            bk = int(np.argmin(d)); best = d[bk] ** 2
        return math.sqrt(best) > road_w[bk] + margin, road_c[bk]

    for i in range(ci):
        r = off(ia) + i * 0x54
        M = np.array(struct.unpack_from(">16f", b, r), dtype=np.float64).reshape(4, 4)
        dl = be32(b, r + 0x44)
        if not dl:
            continue
        cmds = walk(b, off(dl))
        hit = [c for c in cmds if c[1] in rules]
        if not hit:
            continue
        hides.append(f"hide {i} " + " ".join(str(c[0]) for c in hit))
        by_rule = {}
        for _, tex, tris, _tf in hit:
            for t in tris:
                w = np.hstack([np.array(t, float), np.ones((3, 1))]) @ M
                by_rule.setdefault(rules[tex], []).append(w[:, :3])
        for (kind, pool), P in by_rule.items():
            for g in cards(P):
                z0, h = g[:, 2].min(), np.ptp(g[:, 2])
                if h < 0.3:
                    continue
                xy = g[:, :2]
                c = xy.mean(0)
                if kind == "strip":
                    # a strip is a picture of forest: its row of trees, and more
                    # rows behind it, away from the road, as dense as the picture
                    u_, s_, vt = np.linalg.svd(xy - c, full_matrices=False)
                    ax = vt[0]
                    t = (xy - c) @ ax
                    w = np.ptp(t)
                    _, rc = clear_of_road(np.array([c[0], c[1], z0]), 0)
                    away = np.array([-ax[1], ax[0]])
                    if np.dot(away, c - rc[:2]) < 0:
                        away = -away
                    spacing = max(1.2, h * 0.32)
                    rows = 3
                    spots, hs = [], []
                    for row in range(rows):
                        n = max(1, int(round(w / spacing)))
                        for k in range(n):
                            q = c + ax * (t.min() + (k + 0.5) * w / n + rng.uniform(-0.35, 0.35) * w / n) \
                                  + away * (row * spacing * 0.9 + rng.uniform(-0.25, 0.25) * spacing)
                            spots.append(q)
                            hs.append(h * rng.uniform(0.75, 1.05))
                elif kind == "crown":
                    # the card is the crown only: the tree runs from the
                    # ground under it to the crown's top
                    zg = ground.height(c[0], c[1], z0)
                    ztop = g[:, 2].max()
                    if zg is None or ztop - zg < 1.0:
                        continue
                    spots, hs = [c], [ztop - zg]
                    z0 = zg
                else:
                    spots, hs = [c], [h]
                # every tree on the drawn ground under it, and none on or beside
                # the racing line (an original card there is kept where it was)
                keep = []
                for q, ht in zip(spots, hs):
                    z = ground.height(q[0], q[1], z0)
                    zz = z if z is not None else z0
                    ok, _ = clear_of_road(np.array([q[0], q[1], zz]), 3.0)
                    if kind in ("single", "crown") or ok:
                        keep.append((np.array([q[0], q[1], zz]), ht))
                spots = [k_[0] for k_ in keep]
                hs = [k_[1] for k_ in keep]
                for p, ht in zip(spots, hs):
                    # the model whose real height is nearest the card's, so it is
                    # scaled as little as possible: a 20 m pine shrunk to 3 m has
                    # needles a sixth their size, a 4 m tree stretched to 12 m has
                    # leaves three times theirs.  Among those within 15 % of the
                    # best fit, one at random.
                    cands = [(asset, vn, vh) for asset in pool for vn, vh in variants(asset)]
                    err = [abs(math.log(ht / vh)) for _, _, vh in cands]
                    best = min(err)
                    near = [c for c, e in zip(cands, err) if e <= best + 0.15]
                    asset, vname, vh = near[rng.randrange(len(near))]
                    stats.append((track, asset, ht / vh))
                    k = assets.setdefault((asset, vname), len(assets))
                    m = yaw_scale((p[0], p[1], p[2]), rng.uniform(0, 2 * math.pi), ht / vh)
                    puts.append(f"put {i} {k} " + " ".join(f"{x:.6g}" for x in m))
    # ---- ground scatter ------------------------------------------------------
    sc = SCATTER.get(track, {})
    nscatter = 0
    if sc:
        kinds = {}
        for (inst, tex, tf), t in zip(gsrc, gtris):
            key = (tex, tf)
            if key not in kinds:
                kinds[key] = texture_kind(b, tex, tf)
            kind = kinds[key]
            n = np.cross(t[1] - t[0], t[2] - t[0])
            area = np.linalg.norm(n) / 2
            if area < 0.05:
                continue
            nz = n[2] / (2 * area)
            if nz < 0:
                continue
            # banks up to about 50 degrees carry plants too; steeper is rock
            # face -- or, painted with foliage, a wall of trees (below)
            if kind == "foliage" and nz < 0.64:
                kind = "fwall"
            elif kind in ("grass", "earth", "sand", "foliage") and nz < 0.64:
                kind = None
            if kind == "fwall":
                pool = WALL_TREES.get(track)
                if not pool:
                    continue
                # trees standing at the wall's foot, as tall as the wall: a
                # painted forest becomes a real one in front of it
                zlo, zhi = t[:, 2].min(), t[:, 2].max()
                hwall = zhi - zlo
                if hwall < 2.0:
                    continue
                lam = area * 0.02
                cnt = int(lam) + (1 if rng.random() < lam - int(lam) else 0)
                for _ in range(cnt):
                    u1, u2 = rng.random(), rng.random()
                    if u1 + u2 > 1:
                        u1, u2 = 1 - u1, 1 - u2
                    q = t[0] + u1 * (t[1] - t[0]) + u2 * (t[2] - t[0])
                    ok, _ = clear_of_road(q, 2.0)
                    if not ok:
                        continue
                    want = hwall * rng.uniform(0.9, 1.25)
                    cands = [(a, vn, vh) for a in pool for vn, vh in variants(a)]
                    err = [abs(math.log(want / vh)) for _, _, vh in cands]
                    best = min(err)
                    near = [c for c, e in zip(cands, err) if e <= best + 0.15]
                    asset, vname, vh = near[rng.randrange(len(near))]
                    k = assets.setdefault((asset, vname), len(assets))
                    m = yaw_scale((q[0], q[1], zlo), rng.uniform(0, 2 * math.pi), want / vh)
                    puts.append(f"put {inst} {k} " + " ".join(f"{x:.6g}" for x in m))
                    nscatter += 1
                continue
            elif kind == "rock":
                kind = "slope" if 0.35 < nz < 0.75 else None
            pool = sc.get(kind)
            if not pool:
                continue
            for asset, dens, (s0, s1), maxd in pool:
                lam = area * dens
                cnt = int(lam) + (1 if rng.random() < lam - int(lam) else 0)
                for _ in range(cnt):
                    u1, u2 = rng.random(), rng.random()
                    if u1 + u2 > 1:
                        u1, u2 = 1 - u1, 1 - u2
                    q = t[0] + u1 * (t[1] - t[0]) + u2 * (t[2] - t[0])
                    ok, _ = clear_of_road(q, STONE_CLEAR if asset in STONES else ROAD_CLEAR.get(kind, 2.0))
                    if not ok:
                        continue
                    vs = variants(asset)
                    vname, vh = vs[rng.randrange(len(vs))]
                    k = assets.setdefault((asset, vname), len(assets))
                    m = yaw_scale((q[0], q[1], q[2] - 0.02), rng.uniform(0, 2 * math.pi), rng.uniform(s0, s1))
                    puts.append(f"put {inst} {k} " + " ".join(f"{x:.6g}" for x in m) + f" {maxd}")
                    nscatter += 1
        print(f"{track}: {sum(1 for p_ in puts if p_.split()[2] in {str(v) for (a, _), v in assets.items() if a in WALL_TREES.get(track, ())})} wall trees")
        print(f"{track}: {nscatter} scatter models, ground kinds " +
              str({k: sum(1 for v in kinds.values() if v == k) for k in ('grass', 'earth', 'sand', 'foliage', 'rock', None)}))
    for (asset, vname), k in sorted(assets.items(), key=lambda kv: kv[1]):
        lines.append(f"asset {k} {asset} {vname}")
    lines += hides + puts
    os.makedirs(os.path.join(MODELS, "placements"), exist_ok=True)
    with open(os.path.join(MODELS, "placements", track + ".env"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{track}: {len(hides)} instances with cards hidden, {len(puts)} models, {len(assets)} model variants")

if __name__ == "__main__":
    for t in (sys.argv[1:] or sorted(RULES)):
        main(t)
    # models scaled more than 40 % from their real size: a gap in the sources
    far = [x for x in stats if not 1 / 1.4 <= x[2] <= 1.4]
    print(f"{len(stats)} models, {len(far)} scaled beyond 40 %:")
    for t in sorted({x[0] for x in far}):
        sc = sorted(x[2] for x in far if x[0] == t)
        print(f"  {t}: {len(sc)}, scale {sc[0]:.2f}..{sc[-1]:.2f}")
