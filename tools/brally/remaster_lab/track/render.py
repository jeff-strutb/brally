import numpy as np

def render(tris, texs, size=128, view=(1.0, -1.3, 0.9), bg=(34, 34, 38)):
    """tris: list of (P[3x3], UV[3x2], key). Orthographic, z-buffered, alpha-tested."""
    if not tris: return np.full((size, size, 3), bg, np.uint8)
    v = np.array(view, float); v /= np.linalg.norm(v)
    up = np.array([0, 0, 1.0]); r = np.cross(up, v); r /= np.linalg.norm(r); u = np.cross(v, r)
    allp = np.concatenate([t[0] for t in tris])
    c = (allp.max(0) + allp.min(0)) / 2
    proj = lambda P: np.stack([(P - c) @ r, (P - c) @ u, (P - c) @ v], -1)
    pp = proj(allp)
    ext = max(np.abs(pp[:, 0]).max(), np.abs(pp[:, 1]).max(), 1e-6) * 1.08
    img = np.zeros((size, size, 3), np.float32); img[:] = bg
    zb = np.full((size, size), -np.inf)
    L = np.array([0.4, -0.3, 0.87]); L /= np.linalg.norm(L)
    for P, UV, key in tris:
        q = proj(P)
        sx = (q[:, 0] / ext * 0.5 + 0.5) * (size - 1)
        sy = (0.5 - q[:, 1] / ext * 0.5) * (size - 1)
        x0, x1 = int(max(0, np.floor(sx.min()))), int(min(size - 1, np.ceil(sx.max())))
        y0, y1 = int(max(0, np.floor(sy.min()))), int(min(size - 1, np.ceil(sy.max())))
        if x1 < x0 or y1 < y0: continue
        X, Y = np.meshgrid(np.arange(x0, x1 + 1) + 0.0, np.arange(y0, y1 + 1) + 0.0)
        d = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0])
        if abs(d) < 1e-9: continue
        w1 = ((X - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (Y - sy[0])) / d
        w2 = ((sx[1] - sx[0]) * (Y - sy[0]) - (X - sx[0]) * (sy[1] - sy[0])) / d
        w0 = 1 - w1 - w2
        m = (w0 >= -0.01) & (w1 >= -0.01) & (w2 >= -0.01)
        if not m.any(): continue
        z = w0 * q[0, 2] + w1 * q[1, 2] + w2 * q[2, 2]
        T = texs.get(key, {}).get('img')
        n = np.cross(P[1] - P[0], P[2] - P[0]); nl = np.linalg.norm(n)
        shade = 0.55 + 0.45 * abs(n @ L) / nl if nl else 1
        if T is not None:
            h, w = T.shape[:2]
            uu = w0 * UV[0, 0] + w1 * UV[1, 0] + w2 * UV[2, 0]
            vv = w0 * UV[0, 1] + w1 * UV[1, 1] + w2 * UV[2, 1]
            tx = (np.floor(uu * w).astype(int)) % w; ty = (np.floor(vv * h).astype(int)) % h
            col = T[ty, tx]
            m &= col[..., 3] >= 128
            rgb = col[..., :3].astype(np.float32)
        else:
            rgb = np.full(X.shape + (3,), 200, np.float32)
        sub = zb[y0:y1 + 1, x0:x1 + 1]
        m &= z > sub
        sub[m] = z[m]
        img[y0:y1 + 1, x0:x1 + 1][m] = rgb[m] * shade
    return img.clip(0, 255).astype(np.uint8)
