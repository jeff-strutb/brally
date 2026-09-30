#!/usr/bin/env python3
"""remaster_car.py -- packs the Remastered player car for the Mac port
(host/host_car.m) from the output of remaster_bake.py: a car body with no
wheels and one wheel, each a GLB plus its _base/_mr/_normal maps.

    remaster_car.py --body <dir>/body.glb --wheel <dir>/wheel.glb --out ports/common/models/es/pack
                    [--body-axes -x,z,y] [--wheel-axes -x,z,y] [--hubs 1.33,-1.27,0.055]
                    [--wheel-radius 0.33] [--wheel-width 0.26]

Everything is written in the game's car frame: metres, +x forward, +y left,
+z up, the origin at the car's centre at hub height (car+0x00 is that frame's
world matrix; the game's hubs sit at --hubs, which BR_CARLOG=1 prints).  The
body is scaled and placed so its brake discs land on those hubs.  The wheel
is centred on its hub, its axle along y and its outer face towards +y (the
wheel matrices car+0x40.. use the same frame).  --*-axes names, for each car
axis x, y, z, which signed model axis it is; it must be a rotation.

Outputs:
  body.rcm, wheel.rcm   vertices {pos3 nrm3 uv2 tan4} f32 + u32 indices
  proxy.rcm             the body clustered to a few thousand triangles, for
                        the sun's shadow map
  <mesh>_base.png       base colour; the body's alpha marks painted surfaces
  <mesh>_orm.png        occlusion, roughness, metal
  <mesh>_normal.png     tangent-space normals (OpenGL convention)
  livery_side.png, livery_top.png  the livery, projected in the car frame
  car.cfg               paint colour, livery extents
All images are 8-bit RGBA: the port's loader takes nothing else.
"""
import argparse, os, struct
import numpy as np
import pygltflib
from PIL import Image, ImageDraw, ImageFilter

Image.MAX_IMAGE_PIXELS = None
AX = {'x': 0, 'y': 1, 'z': 2}


def axes_matrix(spec):
    R = np.zeros((3, 3))
    for row, a in enumerate(spec.split(',')):
        R[row, AX[a.lstrip('+-')]] = -1.0 if a.startswith('-') else 1.0
    if abs(np.linalg.det(R) - 1) > 1e-6:
        raise SystemExit(f'axes {spec} are not a rotation')
    return R


def read_glb(path):
    """POSITION, NORMAL, TEXCOORD_0, TANGENT and the indices of the one mesh"""
    g = pygltflib.GLTF2().load(path)
    blob = g.binary_blob()
    def acc(i):
        a = g.accessors[i]; bv = g.bufferViews[a.bufferView]
        comps = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a.type]
        dt = np.dtype({5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a.componentType])
        off = (bv.byteOffset or 0) + (a.byteOffset or 0)
        stride = bv.byteStride or comps * dt.itemsize
        rows = np.lib.stride_tricks.as_strided(np.frombuffer(blob, np.uint8, offset=off), (a.count, comps * dt.itemsize), (stride, 1))
        v = rows.copy().view(dt).reshape(a.count, comps)
        return v.astype(np.float64) if dt == np.float32 else v.astype(np.int64)
    prims = [p for m in g.meshes for p in m.primitives]
    if len(prims) != 1:
        raise SystemExit(f'{path}: expected one primitive, found {len(prims)}')
    p = prims[0]; at = p.attributes
    if at.TANGENT is None:
        raise SystemExit(f'{path}: no tangents (remaster_bake.py exports them)')
    return acc(at.POSITION), acc(at.NORMAL), acc(at.TEXCOORD_0), acc(at.TANGENT), acc(p.indices).reshape(-1, 3)


def write_rcm(path, V, N, UV, T, F):
    data = np.hstack([V, N, UV, T]).astype(np.float32)
    with open(path, 'wb') as f:
        f.write(b'RCM1' + struct.pack('<II', len(V), F.size))
        f.write(data.tobytes()); f.write(F.astype(np.uint32).tobytes())


def orm(src, dst):
    """the model's glTF metal/roughness map (g roughness, b metal) as the
    port's occlusion/roughness/metal map, with no occlusion"""
    img = Image.open(src).convert('RGB')
    _, g, b = img.split()
    Image.merge('RGBA', (Image.new('L', img.size, 255), g, b, Image.new('L', img.size, 255))).save(dst)


def rgba(src, dst, alpha=None):
    img = Image.open(src).convert('RGB')
    a = Image.new('L', img.size, 255) if alpha is None else alpha
    Image.merge('RGBA', (*img.split(), a)).save(dst)


def paint_mask(base):
    """texels of the body colour (green-dominant, saturated): the paint"""
    c = np.asarray(base.convert('RGB'), float) / 255
    mx, mn = c.max(2), c.min(2)
    sat = (mx - mn) / (mx + 1e-6)
    g_dom = (c[..., 1] - np.maximum(c[..., 0], c[..., 2])) / (mx + 1e-6)
    m = np.clip((g_dom - 0.12) / 0.15, 0, 1) * np.clip((sat - 0.25) / 0.15, 0, 1) * np.clip((mx - 0.06) / 0.06, 0, 1)
    return Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(3)).filter(ImageFilter.GaussianBlur(1.0))


def hubs(V, UV, base):
    """The four brake-disc centres of a wheel-less body (car frame): grey
    texels low in each corner of the car, the middle of each cluster's extent."""
    img = np.asarray(base.convert('RGB').resize((1024, 1024)), float) / 255
    px = img[np.clip((UV[:, 1] % 1 * 1023).astype(int), 0, 1023), np.clip((UV[:, 0] % 1 * 1023).astype(int), 0, 1023)]
    mx, mn = px.max(1), px.min(1)
    grey = ((mx - mn) < 0.08) & (mx > 0.25) & (mx < 0.9)
    lo, hi = V.min(0), V.max(0)
    low = V[:, 2] < lo[2] + 0.45 * (hi[2] - lo[2])
    side = np.abs(V[:, 1]) > 0.35 * (hi[1] - lo[1]) / 2
    out = {}
    for sx in (1, -1):
        for sy in (1, -1):
            m = grey & low & side & (np.sign(V[:, 0]) == sx) & (np.sign(V[:, 1]) == sy) & (np.abs(V[:, 0]) > 0.2 * (hi[0] - lo[0]) / 2)
            P = V[m]
            if len(P) < 50:
                raise SystemExit(f'no brake disc found at {sx},{sy}')
            hx = np.histogram(P[:, 0], bins=40)
            k = np.argmax(hx[0]); cx = (hx[1][k] + hx[1][k + 1]) / 2
            Q = P[np.abs(P[:, 0] - cx) < 0.18 * (hi[0] - lo[0]) / 4.2]
            out[(sx, sy)] = (np.percentile(Q, 5, axis=0) + np.percentile(Q, 95, axis=0)) / 2
    return out


def cluster(V, F, cell):
    q = np.floor(V / cell).astype(np.int64)
    key, inv = np.unique(q, axis=0, return_inverse=True); inv = inv.reshape(-1)
    P = np.zeros((len(key), 3)); cnt = np.zeros(len(key))
    np.add.at(P, inv, V); np.add.at(cnt, inv, 1)
    P /= cnt[:, None]
    G = inv[F]
    ok = (G[:, 0] != G[:, 1]) & (G[:, 1] != G[:, 2]) & (G[:, 0] != G[:, 2])
    return P, np.unique(np.sort(G[ok], 1), axis=0)


def livery(out, x0, xl, z0, zh, y0, yw):
    """The livery as two decal images in the car frame: the sides (x across,
    z up) and the top (x across, y down the image).  Red, white and blue
    ribbons, drawn as smooth curves; everything else transparent."""
    S = 4
    red, white, blue = (200, 18, 36, 255), (242, 242, 240, 255), (22, 44, 150, 255)
    W, H = 2048, int(2048 * zh / xl)
    img = Image.new('RGBA', (W * S, H * S), (0, 0, 0, 0)); d = ImageDraw.Draw(img)
    px = lambda x, z: ((x - x0) / xl * W * S, (1 - (z - z0) / zh) * H * S)
    # three ribbons sweeping from low behind the front wheel up over the rear quarter
    for k, col in enumerate((blue, white, red)):
        off = (1 - k) * 0.13
        pts = [px(1.05 - 2.9 * t, 0.0 + 0.62 * (t ** 1.35) + off) for t in np.linspace(0, 1, 200)]
        top = [(a, b - 0.055 * H * S / zh) for a, b in pts]
        bot = [(a, b + 0.055 * H * S / zh) for a, b in pts]
        d.polygon(top + bot[::-1], fill=col)
    img.resize((W, H), Image.LANCZOS).save(os.path.join(out, 'livery_side.png'))
    Ht = int(2048 * yw / xl)
    img = Image.new('RGBA', (W * S, Ht * S), (0, 0, 0, 0)); d = ImageDraw.Draw(img)
    py = lambda x, y: ((x - x0) / xl * W * S, (y - y0) / yw * Ht * S)
    for k, col in enumerate((red, white, blue)):
        ya, yb = -0.27 + 0.18 * k, -0.27 + 0.18 * (k + 1) - 0.01
        d.rectangle([py(x0, ya), py(x0 + xl, yb)], fill=col)
    img.resize((W, Ht), Image.LANCZOS).save(os.path.join(out, 'livery_top.png'))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--body', required=True); ap.add_argument('--wheel', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--body-axes', default='-x,z,y'); ap.add_argument('--wheel-axes', default='-x,z,y')
    ap.add_argument('--hubs', default='1.33,-1.27,0.055', help="the game's front x, rear x, hub z (BR_CARLOG)")
    ap.add_argument('--wheel-radius', type=float, default=0.33)
    ap.add_argument('--wheel-width', type=float, default=0.26)
    ap.add_argument('--paint', default='0.02,0.2,0.06', help='linear rgb')
    ap.add_argument('--top-from', type=float, default=0.42, help='height the top ribbon starts at')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    stem = lambda p: os.path.splitext(p)[0]

    # the body, fitted to the game's hubs
    V, N, UV, T, F = read_glb(a.body)
    R = axes_matrix(a.body_axes)
    V = V @ R.T; N = N @ R.T; T = np.hstack([T[:, :3] @ R.T, T[:, 3:4]])
    base = Image.open(stem(a.body) + '_base.png')
    H = hubs(V, UV, base)
    fx, rx, hz = (float(v) for v in a.hubs.split(','))
    front = (H[(1, 1)] + H[(1, -1)]) / 2; rear = (H[(-1, 1)] + H[(-1, -1)]) / 2
    s = (fx - rx) / (front[0] - rear[0])
    mid = (front + rear) / 2
    shift = np.array([(fx + rx) / 2, 0, hz]) - np.array([mid[0], (V.min(0)[1] + V.max(0)[1]) / 2, mid[2]]) * s
    V = V * s + shift
    for k, c in sorted(H.items()):
        print('hub', k, np.round(c * s + shift, 3))
    write_rcm(os.path.join(a.out, 'body.rcm'), V, N, UV, T, F)
    rgba(stem(a.body) + '_base.png', os.path.join(a.out, 'body_base.png'), paint_mask(base))
    orm(stem(a.body) + '_mr.png', os.path.join(a.out, 'body_orm.png'))
    rgba(stem(a.body) + '_normal.png', os.path.join(a.out, 'body_normal.png'))
    bl, bh = V.min(0), V.max(0)
    print(f'body {len(F)} tris, extent x {bl[0]:.3f}..{bh[0]:.3f} y {bl[1]:.3f}..{bh[1]:.3f} z {bl[2]:.3f}..{bh[2]:.3f}')
    P, G = cluster(V, F, 0.08)
    write_rcm(os.path.join(a.out, 'proxy.rcm'), P, np.zeros_like(P), np.zeros((len(P), 2)), np.zeros((len(P), 4)), G)
    print(f'proxy {len(G)} tris')

    # the wheel: hub at the origin, radius and width to the game's
    Vw, Nw, UVw, Tw, Fw = read_glb(a.wheel)
    Rw = axes_matrix(a.wheel_axes)
    Vw = Vw @ Rw.T
    lo, hi = Vw.min(0), Vw.max(0); c = (lo + hi) / 2
    r = max(hi[0] - lo[0], hi[2] - lo[2]) / 2
    sw = a.wheel_radius / r
    ky = (a.wheel_width / 2) / ((hi[1] - lo[1]) / 2 * sw)
    K = np.diag([1, ky, 1])
    Vw = ((Vw - c) * sw) @ K.T
    Nw = (Nw @ Rw.T) @ np.linalg.inv(K); Nw /= np.linalg.norm(Nw, axis=1, keepdims=True)
    Tw = np.hstack([(Tw[:, :3] @ Rw.T) @ K.T, Tw[:, 3:4]])
    Tw[:, :3] /= np.linalg.norm(Tw[:, :3], axis=1, keepdims=True) + 1e-12
    write_rcm(os.path.join(a.out, 'wheel.rcm'), Vw, Nw, UVw, Tw, Fw)
    rgba(stem(a.wheel) + '_base.png', os.path.join(a.out, 'wheel_base.png'))
    orm(stem(a.wheel) + '_mr.png', os.path.join(a.out, 'wheel_orm.png'))
    rgba(stem(a.wheel) + '_normal.png', os.path.join(a.out, 'wheel_normal.png'))
    print(f'wheel {len(Fw)} tris, radius {a.wheel_radius} width {a.wheel_width}')

    x0, xl = bl[0] - 0.05, bh[0] - bl[0] + 0.1
    z0, zh = bl[2], bh[2] - bl[2] + 0.05
    y0, yw = -(bh[1] - bl[1]) / 2 - 0.05, bh[1] - bl[1] + 0.1
    livery(a.out, x0, xl, z0, zh, y0, yw)
    with open(os.path.join(a.out, 'car.cfg'), 'w') as f:
        f.write('# written by ports/macos/tools/remaster_car.py\n')
        f.write('paint %s\n' % a.paint.replace(',', ' '))
        f.write(f'livery_side {x0:.4f} {xl:.4f} {z0:.4f} {zh:.4f}\n')
        f.write(f'livery_top {y0:.4f} {yw:.4f} {a.top_from:.3f} {a.top_from + 0.1:.3f}\n')
        f.write('wheel_flip 1\n')


if __name__ == '__main__':
    main()
