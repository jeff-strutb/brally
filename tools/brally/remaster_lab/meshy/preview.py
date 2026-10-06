#!/usr/bin/env python3
"""Quick numpy preview of a GLB: preview.py in.glb out.png ex ey ez [tx ty tz] [fov]
Axes as stored in the file (glTF: Y up)."""
import sys, math, numpy as np, trimesh
from PIL import Image

s = trimesh.load(sys.argv[1])
g = list(s.geometry.values())[0] if hasattr(s, 'geometry') else s
V = np.asarray(g.vertices, float); F = np.asarray(g.faces); UV = np.asarray(g.visual.uv, float)
tex = np.asarray(g.visual.material.baseColorTexture.convert('RGB').resize((1024, 1024)), float) / 255
eye = np.array([float(x) for x in sys.argv[3:6]])
tgt = np.array([float(x) for x in sys.argv[6:9]]) if len(sys.argv) > 8 else (V.min(0) + V.max(0)) / 2
fov = float(sys.argv[9]) if len(sys.argv) > 9 else 30
up = np.array([0, 1, 0.0])
W = H = 1024
f = tgt - eye; f /= np.linalg.norm(f); r = np.cross(f, up); r /= np.linalg.norm(r); u = np.cross(r, f)
c = V - eye; zc = c @ f; fl = 0.5 * H / math.tan(math.radians(fov) / 2)
sx = W / 2 + fl * (c @ r) / zc; sy = H / 2 - fl * (c @ u) / zc
col = np.ones((H, W, 3)); zb = np.full((H, W), np.inf)
L = np.array([0.5, 0.8, 0.6]); L /= np.linalg.norm(L)
fn = np.cross(V[F[:, 1]] - V[F[:, 0]], V[F[:, 2]] - V[F[:, 0]]); fn /= np.linalg.norm(fn, axis=1, keepdims=True) + 1e-12
for i, t in enumerate(F):
    X, Y, Z = sx[t], sy[t], zc[t]
    if (Z <= 0.01).any(): continue
    x0, x1 = max(0, int(X.min())), min(W - 1, int(X.max()) + 1); y0, y1 = max(0, int(Y.min())), min(H - 1, int(Y.max()) + 1)
    if x0 > x1 or y0 > y1: continue
    den = (Y[1] - Y[2]) * (X[0] - X[2]) + (X[2] - X[1]) * (Y[0] - Y[2])
    if abs(den) < 1e-12: continue
    gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
    l0 = ((Y[1] - Y[2]) * (gx - X[2]) + (X[2] - X[1]) * (gy - Y[2])) / den
    l1 = ((Y[2] - Y[0]) * (gx - X[2]) + (X[0] - X[2]) * (gy - Y[2])) / den
    l2 = 1 - l0 - l1; m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
    if not m.any(): continue
    z = 1 / (l0 / Z[0] + l1 / Z[1] + l2 / Z[2]); sub = zb[y0:y1 + 1, x0:x1 + 1]; m &= z < sub
    if not m.any(): continue
    w = np.stack([l0 / Z[0], l1 / Z[1], l2 / Z[2]], -1) * z[..., None]
    uv = w @ UV[t]; tx = np.clip((uv[..., 0] % 1) * 1023, 0, 1023).astype(int); ty = np.clip((1 - uv[..., 1] % 1) * 1023, 0, 1023).astype(int)
    n = fn[i] if fn[i] @ (eye - V[t[0]]) > 0 else -fn[i]
    sh = 0.35 + 0.65 * max(0, n @ L)
    rgb = tex[ty, tx] * sh
    sub[m] = z[m]; col[y0:y1 + 1, x0:x1 + 1][m] = rgb[m]
Image.fromarray((col * 255).astype(np.uint8)).save(sys.argv[2])
