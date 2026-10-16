"""texregion.py: the Remastered textures' surface maps, the reference light_surf.c follows
(docs/brally/remaster/textures/DESIGN.md section 4; surfparity.sh holds the two together).

For one game texture (RGBA8, its texmat class): which surface each texel is
(asphalt, road paint, grass, sand, rock, ...), from its colour in the class's
context, cleaned of the 15/16-bit dither by a 3x3 vote, then each region's
field upsampled 4x (wrapping, bicubic, a slight blur) so the borders run
smooth instead of in texel steps.

Output per texture, at 4x:
  map  RGBA8: R the lower local region index, G the higher, B where the pixel
       sits between them (0 all lower, 255 all higher; 128 the border: bilinear
       filtering keeps a crisp border at any magnification), A coverage
  var  RG8:   each of the two regions' own brightness against its mean, 128 =
       1.0, 0.5..1.5 (the original's variation inside a region, without
       colours bleeding across borders)
  pal        up to 8 regions: global surface id and the region's mean colour
"""
import numpy as np

# the surfaces (global ids)
SURF = ['none', 'asphalt', 'paint_white', 'paint_yellow', 'gravel', 'dirt', 'dirt_red', 'sand', 'sand_wet', 'foam',
        'water', 'grass', 'grass_dry', 'moss', 'rock_grey', 'rock_brown', 'rock_red', 'concrete', 'plaster', 'brick',
        'stone', 'wood', 'metal', 'rust', 'snow', 'ice', 'roof', 'leaves', 'paint_sign']
SID = {n: i for i, n in enumerate(SURF)}
# colours for the review sheets
VIS = {'none': (255, 0, 255), 'asphalt': (60, 60, 64), 'paint_white': (250, 250, 250), 'paint_yellow': (250, 210, 0),
       'gravel': (150, 140, 130), 'dirt': (130, 90, 50), 'dirt_red': (170, 70, 40), 'sand': (230, 205, 150),
       'sand_wet': (160, 135, 95), 'foam': (200, 240, 255), 'water': (30, 90, 170), 'grass': (60, 150, 40),
       'grass_dry': (170, 170, 70), 'moss': (40, 90, 40), 'rock_grey': (120, 120, 125), 'rock_brown': (120, 95, 70),
       'rock_red': (190, 100, 50), 'concrete': (175, 175, 165), 'plaster': (225, 215, 185), 'brick': (160, 60, 45),
       'stone': (140, 130, 115), 'wood': (115, 75, 35), 'metal': (150, 160, 175), 'rust': (140, 70, 30),
       'snow': (240, 245, 255), 'ice': (170, 210, 235), 'roof': (180, 80, 60), 'leaves': (30, 110, 30), 'paint_sign': (255, 255, 255)}

# texmat classes drawn in surfaces; the rest keep the game's texture
NATURAL = {'asphalt', 'dirt', 'rock', 'grass', 'sand', 'snow', 'ice', 'concrete', 'tunnel', 'water', 'foliage'}


def hsv(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mx, mn = rgb.max(-1), rgb.min(-1)
    d = mx - mn
    h = np.zeros_like(mx)
    m = d > 1e-6
    rr = m & (mx == r); gg = m & (mx == g) & ~rr; bb = m & ~rr & ~gg
    h[rr] = (60 * ((g - b)[rr] / d[rr])) % 360
    h[gg] = 60 * ((b - r)[gg] / d[gg]) + 120
    h[bb] = 60 * ((r - g)[bb] / d[bb]) + 240
    s = np.where(mx > 1e-6, d / np.maximum(mx, 1e-6), 0)
    return h, s, mx


def base_surface(cls, rgb, a):
    """the texture's one base surface: its class, and for rock, dirt and sand its mean colour"""
    m = rgb[a].mean(0) if a.any() else rgb.reshape(-1, 3).mean(0)
    h, s, v = (float(x[0]) for x in hsv(m[None]))
    if cls == 'rock':
        return 'rock_red' if (h < 40 and s > 0.38) else 'rock_brown' if s > 0.2 else 'rock_grey'
    if cls == 'dirt':
        return 'dirt_red' if (h < 26 and s > 0.42) else 'gravel' if s < 0.13 else 'dirt'
    return {'foliage': 'leaves', 'water': 'water', 'asphalt': 'asphalt', 'grass': 'grass', 'sand': 'sand', 'snow': 'snow', 'ice': 'ice', 'concrete': 'concrete',
            'tunnel': 'concrete', 'building': 'plaster', 'wood': 'wood', 'metal': 'metal'}[cls]


def classify(cls, rgb, a):
    """each texel's surface name, from its sRGB colour (0..1) in its class's context: the texture's
    base surface, and over it only what is plainly something else (a road's paint, grass on a
    rock or a beach, water and its foam, snow on rock, a building's brick, wood and glass)"""
    h, s, v = hsv(rgb)
    vmed, smed = np.median(v), np.median(s)
    base = base_surface(cls, rgb, a)
    out = np.full(h.shape, base, dtype=object)
    green = (h > 65) & (h <= 165) & (s > 0.10)
    blue = (h > 165) & (h <= 260) & (s > 0.18)
    grey = s < 0.13
    warm = ~green & ~blue & ~grey
    if cls == 'asphalt':
        out[warm & (s > max(0.3, smed + 0.15))] = 'dirt'
        out[green & (s > smed + 0.1)] = 'grass'
        out[(v > max(0.55, vmed + 0.22)) & (s < 0.2)] = 'paint_white'
        out[(h >= 35) & (h <= 65) & (s > 0.45) & (v > 0.45)] = 'paint_yellow'
    elif cls in ('dirt', 'rock', 'sand', 'grass'):
        if cls != 'grass':
            out[green] = 'grass'
        out[blue] = 'water'
        out[(blue | green) & (v > 0.72) & (s < 0.3) & (h > 150)] = 'foam'
        if cls == 'grass':
            out[warm & (s > smed + 0.12) & (h < 40)] = 'dirt'
        if cls != 'sand':
            out[(v > 0.8) & (s < 0.12)] = 'snow'               # a snow track's ground
    elif cls == 'snow':
        out[blue & (s > 0.25)] = 'ice'
        out[(v < 0.45) & ~blue] = 'gravel'                    # a road's ruts, bare ground
        out[(v < 0.45) & warm & (s > 0.2)] = 'rock_brown'    # rock through the snow
    elif cls == 'water':
        out[(v > 0.8) & (s < 0.2)] = 'foam'
    elif cls == 'ice':
        out[(v > 0.85) & (s < 0.1)] = 'snow'
    elif cls in ('concrete', 'tunnel'):
        out[warm & (s > 0.5) & (v > 0.5)] = 'none'          # the tunnels' lamp strips: the game's own
    elif cls == 'building':
        out[warm & (h < 25) & (s > 0.35)] = 'brick'
        out[grey & (v < 0.6)] = 'stone'
        out[warm & (v < 0.45) & (s > 0.3)] = 'wood'
        out[blue | (v < 0.15)] = 'none'                     # glass, openings: the game's own
        out[green] = 'moss'
    elif cls == 'metal':
        out[warm & (s > 0.3)] = 'rust'
    if cls in NATURAL:   # kerbs, paint: strong reds keep the game's own texels
        out[((h < 12) | (h > 330)) & (s > 0.6) & (v > 0.4)] = 'none'
    return out


THIN = {'paint_white', 'paint_yellow', 'foam'}


def vote(lab, k=1, thin_all=False):
    """3x3 majority (wrapping), the dither's single texels taken by their neighbours; a road's
    painted line or a wave's foam, a texel wide, kept where it runs on (a neighbour of its own)"""
    H, W = lab.shape
    ids = sorted(set(lab.ravel()))
    cnt = np.zeros((len(ids), H, W))
    for i, n in enumerate(ids):
        m = (lab == n).astype(float)
        for dy in range(-k, k + 1):
            for dx in range(-k, k + 1):
                cnt[i] += np.roll(np.roll(m, dy, 0), dx, 1)
        cnt[i] += 2 * m                                   # the texel's own vote breaks ties
    out = np.array(ids, dtype=object)[cnt.argmax(0)]
    for i, n in enumerate(ids):
        if thin_all or n in THIN:
            keep = (lab == n) & (cnt[i] >= 4)             # itself (3) and one neighbour
            if thin_all:                                  # a layout's art: only a lone texel goes
                keep = (lab == n) & (cnt[i] >= 4)
            out[keep] = n
    return out


def cubic_up(f, s):
    """f (H, W) upsampled s times, wrapping, Catmull-Rom"""
    H, W = f.shape
    def axis(n):
        x = (np.arange(n * s) + 0.5) / s - 0.5
        i = np.floor(x).astype(int); t = x - i
        w = [(-t ** 3 + 2 * t ** 2 - t) / 2, (3 * t ** 3 - 5 * t ** 2 + 2) / 2, (-3 * t ** 3 + 4 * t ** 2 + t) / 2, (t ** 3 - t ** 2) / 2]
        return [(i + o) % n for o in (-1, 0, 1, 2)], w
    (iy, wy), (ix, wx) = axis(H), axis(W)
    rows = sum(wy[a][:, None] * f[iy[a], :] for a in range(4))
    return sum(wx[a][None, :] * rows[:, ix[a]] for a in range(4))


def blur(f, sig):
    r = int(np.ceil(sig * 3)); x = np.arange(-r, r + 1); k = np.exp(-x * x / (2 * sig * sig)); k /= k.sum()
    g = sum(k[i] * np.roll(f, x[i], 0) for i in range(len(x)))
    return sum(k[i] * np.roll(g, x[i], 1) for i in range(len(x)))


TIE = 1e-9   # distances this close are a tie, taken by the lower index (light_surf.c the same):
             # numpy's pairwise sums and C's sequential ones differ in the last bits


def nearest(D):
    """per row of D, the first column within TIE of the row's least"""
    return (D <= D.min(1, keepdims=True) + TIE).argmax(1)


def kmeans(x, k, it=12):
    """x (n, 3): k-means from farthest-point seeds; (labels, centres)"""
    d0 = ((x - x.mean(0)) ** 2).sum(1)
    c = [x[(d0 <= d0.min() + TIE).argmax()]]
    for _ in range(1, k):
        d = np.min([((x - q) ** 2).sum(1) for q in c], 0)
        c.append(x[(d >= d.max() - TIE).argmax()])
    c = np.array(c)
    for _ in range(it):
        lab = nearest(((x[:, None] - c[None]) ** 2).sum(2))
        c = np.array([x[lab == i].mean(0) if (lab == i).any() else c[i] for i in range(k)])
    return lab, c


def cluster_surface(cls, c):
    """a layout texture's colour cluster (sRGB 0..1): the surface it is drawn in"""
    h, s, v = (float(q[0]) for q in hsv(c[None]))
    if cls == 'sign':
        return 'paint_sign'
    if cls == 'wood':
        return 'none' if v < 0.12 else 'wood'
    if cls == 'metal':
        return 'rust' if (h < 45 and s > 0.3) else 'none' if v < 0.12 else 'metal'
    if (165 < h <= 260 and s > 0.2) or v < 0.12:
        return 'none'                                   # glass, openings: the game's own
    if h < 25 and s > 0.35 and v < 0.75:
        return 'brick' if v > 0.35 else 'wood'
    if 25 <= h < 55 and s > 0.35 and v < 0.5:
        return 'wood'
    if s < 0.12 and v < 0.55:
        return 'stone'
    return 'plaster'


def cluster_labels(cls, rgb, a, kmax=8, err=0.02):
    """a layout texture's texels by colour cluster: the fewest clusters (up to kmax) whose mean
    distance from their centre is under err (0.02: a window's glass, a door, keep their own); names 'c<i>:<surface>'"""
    x = rgb[a]
    for k in range(1, kmax + 1):
        lab, c = kmeans(x, min(k, len(np.unique(x, axis=0))))
        if np.sqrt(((x - c[lab]) ** 2).sum(1)).mean() < err:
            break
    full = nearest(((rgb.reshape(-1, 1, 3) - c[None]) ** 2).sum(-1)).reshape(rgb.shape[:2])
    names = np.array(['c%d:%s' % (i, cluster_surface(cls, c[i])) for i in range(len(c))], dtype=object)
    return names[full]


LAYOUT = {'building', 'wood', 'metal'}   # (signs: the game's own)


def region_map(cls, rgba, up=4):
    """(map RGBA8 (H*up, W*up, 4), var (.., 2), palette [(surface id, linear rgb)]) or None when the
    texture keeps the game's own look"""
    if cls not in NATURAL and cls not in LAYOUT:
        return None
    rgb = rgba[..., :3].astype(float) / 255
    a = rgba[..., 3] >= 128
    if cls in LAYOUT and not a.any():
        return None
    lab = vote(cluster_labels(cls, rgb, a), thin_all=True) if cls in LAYOUT else vote(classify(cls, rgb, a))
    names = [n for n in sorted(set(lab[a].ravel()) if a.any() else []) if True]
    if not names or all(n.split(':')[-1] == 'none' for n in names):
        return None
    # the eight largest regions; any others join their nearest neighbour by vote
    cnt = sorted(((int((lab == n).sum()), n) for n in names), reverse=True)
    keep = [n for _, n in cnt[:8]]
    lin = np.where(rgb <= 0.04045, rgb / 12.92, ((rgb + 0.055) / 1.055) ** 2.4)
    H, W = lab.shape
    fields, varf, pal = [], [], []
    for n in keep:
        m = ((lab == n) & a).astype(float)
        fields.append(blur(np.clip(cubic_up(m, up), 0, 1), (0.2 if cls in LAYOUT else 0.35) * up))
        mean = (lin * m[..., None]).sum((0, 1)) / max(m.sum(), 1)
        y = lin @ [0.2126, 0.7152, 0.0722]
        ym = max(float(mean @ [0.2126, 0.7152, 0.0722]), 1e-4)
        # the region's own brightness, spread into its neighbours by normalised convolution
        num, den = blur(y * m, 1.0), blur(m, 1.0)
        v = np.where(den > 1e-3, num / np.maximum(den, 1e-3), ym) / ym
        varf.append(np.clip(cubic_up(blur(v, 0.5), up), 0.5, 1.5))
        pal.append((SID[n.split(':')[-1]], mean))
    F = np.stack(fields)                                  # (k, H*up, W*up)
    order = np.argsort(-F, 0, kind='stable')
    i0, i1 = order[0], order[1] if len(keep) > 1 else order[0]
    f0 = np.take_along_axis(F, i0[None], 0)[0]; f1 = np.take_along_axis(F, i1[None], 0)[0]
    lo, hi = np.minimum(i0, i1), np.maximum(i0, i1)
    flo = np.where(i0 == lo, f0, f1); fhi = np.where(i0 == lo, f1, f0)
    t = np.where(lo == hi, 0.0, fhi / np.maximum(flo + fhi, 1e-6))
    cov = np.clip(cubic_up(a.astype(float), up), 0, 1)
    V = np.stack(varf)
    vlo = np.take_along_axis(V, lo[None], 0)[0]; vhi = np.take_along_axis(V, hi[None], 0)[0]
    mp = np.stack([lo, hi, np.round(t * 255), np.round(cov * 255)], -1).astype(np.uint8)
    vr = np.stack([np.round(np.clip((vlo - 0.5) * 255, 0, 255)), np.round(np.clip((vhi - 0.5) * 255, 0, 255))], -1).astype(np.uint8)
    return mp, vr, pal


def preview(mp, pal, vr=None, mean=False):
    """the map as the review sheet shows it: each region's surface colour, the border where t crosses 0.5"""
    lo, hi, t = mp[..., 0], mp[..., 1], mp[..., 2] / 255
    sel = np.where(t >= 0.5, hi, lo)
    srgb = lambda c: np.where(c <= 0.0031308, 12.92 * c, 1.055 * c ** (1 / 2.4) - 0.055) * 255
    cols = np.array([srgb(np.asarray(c)) if mean else VIS[SURF[s]] for s, c in pal], dtype=float)
    out = cols[sel]
    if vr is not None:
        v = np.where(t >= 0.5, vr[..., 1], vr[..., 0]) / 255 + 0.5
        out = out * v[..., None]
    return np.clip(out, 0, 255).astype(np.uint8)
