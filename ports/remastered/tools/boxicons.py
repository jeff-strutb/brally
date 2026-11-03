"""boxicons.py: the games' boxes, for the Remaster Options' rows that choose between the games
(Music, Menus): Top Gear Rally's USA and Japanese boxes and Boss Rally's, each a box of its
front picture's proportions, its art from ports/common/assets/boxart (Boss Rally's front alone:
its back shows the front again), its four edges flat in the colour of the front's edge they meet.

    python3 ports/remastered/tools/boxicons.py [OUT.c]

writes ports/remastered/tgrally/platform/os/rm_boxes.c (or OUT.c): each box in the game's icon
model format (tgr/model.h), big-endian, every address an offset from its start, as handicons.py
writes the monitor and the console (rmoptions.c places and rebases them).  A face's picture is
held once, FACE_H texels tall in RGBA16 (ordered dither to its 5 bits), and drawn in tiles of
at most TILE_W x TILE_H texels, each loaded into TMEM by itself (G_LOADTILE from the whole
picture) under its own two triangles; neighbouring tiles share their edge texels, so the
filtered picture runs on across them.  Last the shadow on the ring (handicons.shadow_dl).

menuicons.py draws the same boxes as still pictures for Boss Rally's menus (render). """
import math
import os
import struct
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import handicons  # noqa: E402

ROOT = os.path.join(HERE, "..", "..", "..")
ART = os.path.join(ROOT, "ports", "common", "assets", "boxart")
OUT = os.path.join(ROOT, "ports", "remastered", "tgrally", "platform", "os", "rm_boxes.c")

# name, front, back (None: the front again)
BOXES = (("usa", "tgr_usa_front.jpg", "tgr_usa_back.jpg"),
         ("jpn", "tgr_jpn_front.jpg", "tgr_jpn_back.jpg"),
         ("br", "br_usa_front.jpg", None))
MAX_W, MAX_H = 2000, 1600       # model units: a landscape box this wide, any other this tall
DEPTH = 0.22                    # a box's depth, of its shorter side
FACE_H = 192                    # a face's picture, texels tall
TILE_W, TILE_H = 64, 32         # a tile: 64 x 32 RGBA16 texels fill TMEM's 4 KB
EDGE = 4                        # the front's outer texels a side's colour is taken from
BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0


class Box:
    def __init__(self, name, front, back):
        self.name = name
        f = Image.open(os.path.join(ART, front)).convert("RGB")
        b = Image.open(os.path.join(ART, back)).convert("RGB") if back else None
        self.front_src = np.asarray(f, float) / 255.0
        self.back_src = np.asarray(b, float) / 255.0 if b else None
        aspect = f.width / f.height
        if aspect >= 1:
            self.W, self.H = MAX_W, MAX_W / aspect
        else:
            self.W, self.H = MAX_H * aspect, MAX_H
        self.D = DEPTH * min(self.W, self.H)
        self.tw, self.th = int(round(FACE_H * aspect)), FACE_H
        self.front = np.asarray(f.resize((self.tw, self.th), Image.LANCZOS), float) / 255.0
        self.back = np.asarray(b.resize((self.tw, self.th), Image.LANCZOS), float) / 255.0 if b else None
        fr = self.front_src
        # the four edges: each the colour of the front's edge it meets
        self.side = {"left": fr[:, :EDGE].mean((0, 1)), "right": fr[:, -EDGE:].mean((0, 1)),
                     "top": fr[:EDGE].mean((0, 1)), "bottom": fr[-EDGE:].mean((0, 1))}


def rgba16(img):
    """big-endian RGBA16 texels, each channel dithered to 5 bits"""
    h, w, _ = img.shape
    th = BAYER[np.arange(h)[:, None] % 4, np.arange(w)[None, :] % 4][..., None]
    q = np.clip(np.floor(img * 31 + th), 0, 31).astype(np.uint16)
    v = (q[..., 0] << 11) | (q[..., 1] << 6) | (q[..., 2] << 1) | 1
    return v.astype(">u2").tobytes()


def tiles(n, step):
    """a face's tile origins and sizes along n texels: each shares its last texel with the next"""
    out, c = [], 0
    while c < n - 1:
        size = min(step, n - c)
        out.append((c, size))
        c += step - 1
    return out


def vtx(p, st, nrm):
    return struct.pack('>hhhHhhbbbB', *(int(round(x)) for x in p), 0, *(int(round(x)) for x in st),
                       *(int(round(x * 127)) for x in nrm), 255)


def model(box):
    W, H, D, hz = box.W, box.H, box.D, handicons.HOVER
    blob = bytearray(8 + 20 * 2)
    struct.pack_into('>II', blob, 0, 2, 0)

    def align8():
        while len(blob) % 8:
            blob.append(0)

    def put(b):
        align8()
        at = len(blob)
        blob.extend(b)
        return at
    tex_front = put(rgba16(box.front))
    tex_back = put(rgba16(box.back)) if box.back is not None else tex_front
    x0, x1, y0, y1, z0, z1 = -W / 2, W / 2, -D / 2, D / 2, hz, hz + H
    # the edges: their corners counter-clockwise from outside, and their normals
    sides = (("left", [(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)], (-1, 0, 0)),
             ("right", [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], (1, 0, 0)),
             ("top", [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)),
             ("bottom", [(x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0)], (0, 0, -1)))
    side_tex, side_v = [], []
    for key, corners, n in sides:
        r5, g5, b5 = handicons.srgb5(box.side[key])
        side_tex.append(put(struct.pack('>4H', *([(r5 << 11) | (g5 << 6) | (b5 << 1) | 1] * 4))))
        side_v.append(put(b''.join(vtx(p, (0, 0), n) for p in corners)))
    # the faces' tiles: each four corners (top left, top right, bottom right, bottom left as its
    # picture has them), texture coordinates from the tile's own corner
    faces = []
    for tex, back in ((tex_front, False), (tex_back, True)):
        y, n = (y1, (0, 1, 0)) if back else (y0, (0, -1, 0))
        ts = []
        for r0, h in tiles(box.th, TILE_H):
            for c0, w in tiles(box.tw, TILE_W):
                corner = []
                for (u, v) in ((c0, r0), (c0 + w - 1, r0), (c0 + w - 1, r0 + h - 1), (c0, r0 + h - 1)):
                    fx = u / (box.tw - 1)
                    x = (W / 2 - fx * W) if back else (-W / 2 + fx * W)
                    z = z1 - v / (box.th - 1) * H
                    corner.append(vtx((x, y, z), ((u - c0) * 32, (v - r0) * 32), n))
                ts.append((c0, r0, w, h, put(b''.join(corner))))
        faces.append((tex, ts))
    align8()
    dl = len(blob)

    def cmd(w0, w1):
        blob.extend(struct.pack('>II', w0, w1))
    cmd(0xBB000001, 0xFFFFFFFF)                             # G_TEXTURE: on, tile 0, scale 1
    for tex, v in zip(side_tex, side_v):                    # the edges, as the console's colours
        cmd(0xFD100000, tex)
        cmd(0xF5100000, 0x07000000)
        cmd(0xE6000000, 0)
        cmd(0xF3000000, 0x07003000)
        cmd(0xE7000000, 0)
        cmd(0xF5100200, 0x00F00020)
        cmd(0xF2002002, 0x0000E002)
        cmd(0x04000000 | (4 << 10) | (4 * 16 - 1), v)
        cmd(0xB1000000 | (0 << 16) | (2 << 8) | 4, (0 << 16) | (4 << 8) | 6)
    for tex, ts in faces:
        cmd(0xFD100000 | (box.tw - 1), tex)                 # G_SETTIMG: the whole picture
        for c0, r0, w, h, v in ts:
            line = (w * 2 + 7) // 8
            cmd(0xF5100000 | (line << 9), 0x07000000)       # G_SETTILE: the load tile
            cmd(0xE6000000, 0)
            cmd(0xF4000000 | (c0 << 2) << 12 | (r0 << 2), 0x07000000 | ((c0 + w - 1) << 2) << 12 | ((r0 + h - 1) << 2))
            cmd(0xE7000000, 0)
            cmd(0xF5100000 | (line << 9), 0x00080200)       # the render tile, clamped both ways
            cmd(0xF2000000, ((w - 1) << 2) << 12 | ((h - 1) << 2))
            cmd(0x04000000 | (4 << 10) | (4 * 16 - 1), v)
            # bottom left, bottom right, top right; bottom left, top right, top left
            cmd(0xB1000000 | (3 * 2 << 16) | (2 * 2 << 8) | 1 * 2, (3 * 2 << 16) | (1 * 2 << 8) | 0 * 2)
    cmd(0xB8000000, 0)
    # flags 4: both sides drawn (BrModelDraw clears the culling for it), as handicons' parts
    struct.pack_into('>IHHfff', blob, 8, dl, 4, 0, 0.0, 0.0, 0.0)
    ring = [(int(round(x)), int(round(y))) for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
    sdl = handicons.shadow_dl(blob, ring)
    struct.pack_into('>IHHfff', blob, 8 + 20, sdl, 8, 0, 0.0, 0.0, 0.0)
    ntiles = sum(len(ts) for _t, ts in faces)
    return bytes(blob), ntiles


def boxes():
    return [Box(*b) for b in BOXES]


# ---- a still picture of a box (Boss Rally's menus: menuicons.py) --------------------------------
def render(box, max_w, max_h, yaw, pitch, light, ambient, ss=4):
    """the box seen from `yaw` degrees to its left and `pitch` above, in perspective from five
    times its size away, lit by `light` (toward it) over `ambient`, fitted in max_w x max_h
    pixels, ss x ss samples a pixel: an RGBA image (alpha the coverage)"""
    W, H, D = box.W, box.H, box.D
    lo, hi = np.array((-W / 2, -D / 2, 0.0)), np.array((W / 2, D / 2, H))
    centre = (lo + hi) / 2
    a, p = math.radians(yaw), math.radians(pitch)
    cam = centre + 5 * max(W, H) * np.array((-math.sin(a) * math.cos(p), -math.cos(a) * math.cos(p), math.sin(p)))
    f = centre - cam
    f /= np.linalg.norm(f)
    r = np.cross(f, (0, 0, 1))
    r /= np.linalg.norm(r)
    u = np.cross(r, f)
    corners = np.array([(x, y, z) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])])
    v = corners - cam
    xs, ys = (v @ r) / (v @ f), (v @ u) / (v @ f)
    k = min(max_w / (xs.max() - xs.min()), max_h / (ys.max() - ys.min()))
    w_px, h_px = int(math.floor((xs.max() - xs.min()) * k)), int(math.floor((ys.max() - ys.min()) * k))
    cx, cy = (xs.max() + xs.min()) / 2, (ys.max() + ys.min()) / 2
    gi, gj = np.meshgrid((np.arange(w_px * ss) + 0.5) / ss, (np.arange(h_px * ss) + 0.5) / ss)
    sx = cx + (gi - w_px / 2) / k
    sy = cy - (gj - h_px / 2) / k
    d = f[None, None, :] + sx[..., None] * r + sy[..., None] * u
    # the ray against the box's slabs
    with np.errstate(divide="ignore", invalid="ignore"):
        t0 = (lo - cam) / d
        t1 = (hi - cam) / d
    tn, tf = np.minimum(t0, t1), np.maximum(t0, t1)
    tnear, axis = tn.max(-1), tn.argmax(-1)
    hit = tnear < tf.min(-1)
    P = cam + d * tnear[..., None]
    sgn = np.sign(np.take_along_axis(d, axis[..., None], -1)[..., 0])
    nrm = np.zeros(P.shape)
    np.put_along_axis(nrm, axis[..., None], -sgn[..., None], -1)
    col = np.zeros(P.shape)
    for key, ax, s in (("left", 0, -1), ("right", 0, 1), ("bottom", 2, -1), ("top", 2, 1)):
        m = hit & (axis == ax) & (-sgn == s)
        col[m] = box.side[key]
    for img, s in ((box.front_src, -1), (box.back_src if box.back_src is not None else box.front_src, 1)):
        m = hit & (axis == 1) & (-sgn == s)
        fx = (P[..., 0] + W / 2) / W
        if s == 1:
            fx = 1 - fx
        fy = (H - P[..., 2]) / H
        col[m] = bilinear(img, fx[m], fy[m])
    L = np.array(light, float) / np.linalg.norm(light)
    shade = ambient + (1 - ambient) * np.clip(nrm @ L, 0, 1)
    col = col * shade[..., None] * hit[..., None]
    a_ = hit.reshape(h_px, ss, w_px, ss).mean((1, 3))
    c = col.reshape(h_px, ss, w_px, ss, 3).sum((1, 3)) / np.maximum(hit.reshape(h_px, ss, w_px, ss).sum((1, 3)), 1)[..., None]
    rgba = np.dstack([np.clip(c * 255 + 0.5, 0, 255), a_ * 255 + 0.5]).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA")


def bilinear(img, fx, fy):
    h, w, _ = img.shape
    x = np.clip(fx, 0, 1) * (w - 1)
    y = np.clip(fy, 0, 1) * (h - 1)
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    x1, y1 = np.minimum(x0 + 1, w - 1), np.minimum(y0 + 1, h - 1)
    ax, ay = (x - x0)[:, None], (y - y0)[:, None]
    top = img[y0, x0] * (1 - ax) + img[y0, x1] * ax
    bot = img[y1, x0] * (1 - ax) + img[y1, x1] * ax
    return top * (1 - ay) + bot * ay


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else OUT
    with open(out, "w") as f:
        f.write("/* rm_boxes.c: the games' boxes for the Remaster Options' Music and Menus rows, built by\n")
        f.write(" * ports/remastered/tools/boxicons.py: each in the game's own icon model format (tgr/model.h),\n")
        f.write(" * big-endian, every address an offset from its start (rmoptions.c places and rebases\n")
        f.write(" * them).  Generated: do not edit. */\n")
        for box in boxes():
            b, ntiles = model(box)
            print(f"boxicons: {box.name} {box.tw}x{box.th} texels a face, {ntiles} tiles, {len(b)} bytes")
            f.write(f"const unsigned int rm_box_{box.name}_len = {len(b)};\n")
            f.write(f"const unsigned char rm_box_{box.name}[{len(b)}] = {{\n")
            for i in range(0, len(b), 32):
                f.write("    " + ",".join(str(x) for x in b[i:i + 32]) + ",\n")
            f.write("};\n")


if __name__ == "__main__":
    main()
