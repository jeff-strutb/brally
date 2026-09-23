"""brbox_glraster.py -- a debugging picture of what the boxed game drew.

Glide is a STUB in the box (tools/brbox_imports.py): nothing the game computes
depends on it.  This module only watches the calls so a script's SHOT can show
the 3D frame -- the race, the HUD, the 3D-drawn menu highlights -- when
driving the game.  It is an approximation of the Voodoo pipeline, good enough
to read a screen: textured / Gouraud triangles, perspective-correct texture
coordinates, the common colour-combine and blend modes, alpha test, a
w-buffer, the clip window.  Nothing here feeds back into the run.

It records the draw calls of the current buffer-swap frame and keeps the last
complete one; rasterising happens only when a shot asks for it.
"""
import struct

W, H = 640, 480
_LOD = [256, 128, 64, 32, 16, 8, 4, 2, 1]
_ASPECT = [(8, 1), (4, 1), (2, 1), (1, 1), (1, 2), (1, 4), (1, 8)]


def _dims(large, aspect):
    s = _LOD[large] if 0 <= large < 9 else 1
    ax, ay = _ASPECT[aspect] if 0 <= aspect < 7 else (1, 1)
    w = s if ax >= ay else max(s // ay, 1)
    h = s if ay >= ax else max(s // ax, 1)
    return w, h


def _texel_fn(fmt):
    """-> (bytes per texel, fn(raw)->(r,g,b,a))."""
    if fmt == 10:
        return 2, lambda v: ((v >> 11) << 3, ((v >> 5) & 63) << 2, (v & 31) << 3, 255)
    if fmt == 11:
        return 2, lambda v: (((v >> 10) & 31) << 3, ((v >> 5) & 31) << 3, (v & 31) << 3,
                             255 if v & 0x8000 else 0)
    if fmt == 12:
        return 2, lambda v: (((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17,
                             ((v >> 12) & 15) * 17)
    if fmt == 13:
        return 2, lambda v: (v & 255, v & 255, v & 255, v >> 8)
    if fmt == 8:
        return 2, lambda v: (((v >> 5) & 7) * 36, ((v >> 2) & 7) * 36, (v & 3) * 85, v >> 8)
    if fmt == 0:
        return 1, lambda v: ((v >> 5) * 36, ((v >> 2) & 7) * 36, (v & 3) * 85, 255)
    if fmt == 2:
        return 1, lambda v: (255, 255, 255, v)
    if fmt == 3:
        return 1, lambda v: (v, v, v, 255)
    if fmt == 4:
        return 1, lambda v: ((v & 15) * 17, (v & 15) * 17, (v & 15) * 17, (v >> 4) * 17)
    return 1, lambda v: (v, v, v, 255)


class Recorder(object):
    def __init__(self):
        self.tex = {}            # (tmu, start) -> (w, h, fmt, raw)
        self.cur_tex = None
        self.state = {}
        self.ops = []            # current frame
        self.last = []           # last complete frame
        self.clear = (0, 0, 0)

    def on_glide(self, box, name, a):
        if box.subrun:
            return
        n = name
        st = self.state
        if n == '_grTexDownloadMipMap@16':
            small, large, aspect, fmt, data = (box.rd32(a[3] + 4 * k) for k in range(5))
            w, h = _dims(large, aspect)
            bpp = 2 if fmt >= 8 else 1
            try:
                raw = box.rd(data, w * h * bpp)
            except Exception:
                return
            self.tex[(a[0], a[1])] = (w, h, fmt, raw)
        elif n == '_grTexSource@16':
            self.cur_tex = (a[0], a[1])
        elif n == '_grColorCombine@20':
            st['cc'] = (a[0], a[1], a[2], a[3])
        elif n == '_grAlphaBlendFunction@16':
            st['ab'] = (a[0], a[1])
        elif n == '_grAlphaTestFunction@4':
            st['at'] = a[0]
        elif n == '_grAlphaTestReferenceValue@4':
            st['aref'] = a[0] & 0xFF
        elif n == '_grConstantColorValue@4':
            st['const'] = a[0]
        elif n == '_grDepthBufferMode@4':
            st['dm'] = a[0]
        elif n == '_grDepthMask@4':
            st['dmask'] = a[0]
        elif n == '_grClipWindow@16':
            st['clip'] = (a[0], a[1], a[2], a[3])
        elif n == '_grBufferClear@12':
            c = a[0]
            self.ops.append(('clear', ((c >> 16) & 255, (c >> 8) & 255, c & 255)))
        elif n == '_grDrawTriangle@12':
            vs = [self._vtx(box, a[k]) for k in range(3)]
            self.ops.append(('tri', vs, self._snap()))
        elif n == '_grDrawPolygonVertexList@8':
            cnt = a[0]
            vs = [self._vtx(box, a[1] + 72 * k) for k in range(min(cnt, 64))]
            snap = self._snap()
            for k in range(1, len(vs) - 1):
                self.ops.append(('tri', [vs[0], vs[k], vs[k + 1]], snap))
        elif n == '_grBufferSwap@4':
            self.last, self.ops = self.ops, []

    def _vtx(self, box, p):
        f = struct.unpack('<12f', box.rd(p, 48))
        return f  # x y z r g b ooz a oow sow tow oow0

    def _snap(self):
        st = self.state
        return (self.cur_tex, st.get('cc'), st.get('ab'), st.get('at', 7), st.get('aref', 0),
                st.get('const', 0), st.get('dm', 0), st.get('dmask', 1), st.get('clip'))

    # -------------------------------------------------------------- raster --
    def render(self, base_rgb=None):
        fb = bytearray(base_rgb) if base_rgb else bytearray(W * H * 3)
        zb = [0.0] * (W * H)
        for op in self.last:
            if op[0] == 'clear':
                c = bytes(op[1])
                fb[:] = c * (W * H)
                zb = [0.0] * (W * H)
                continue
            self._tri(fb, zb, op[1], op[2])
        return fb

    def _tri(self, fb, zb, vs, snap):
        tex_key, cc, ab, at, aref, const, dm, dmask, clip = snap
        (x0, y0), (x1, y1), (x2, y2) = [(v[0], v[1]) for v in vs]
        # Glide apps often add a large bias to snap coordinates
        if x0 > 4096:
            bias = float(3 << 18)
            x0 -= bias; x1 -= bias; x2 -= bias
            y0 -= bias; y1 -= bias; y2 -= bias
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if area == 0 or abs(area) > 4e6:
            return
        cx0, cy0, cx1, cy1 = clip if clip else (0, 0, W, H)
        minx = max(int(min(x0, x1, x2)), cx0, 0)
        maxx = min(int(max(x0, x1, x2)) + 1, cx1, W)
        miny = max(int(min(y0, y1, y2)), cy0, 0)
        maxy = min(int(max(y0, y1, y2)) + 1, cy1, H)
        if minx >= maxx or miny >= maxy:
            return
        tex = self.tex.get(tex_key) if tex_key else None
        uses_tex = tex is not None and cc is not None and (cc[3] == 1 or cc[0] in (3, 4, 5, 6, 7, 8))
        if tex is not None and uses_tex:
            tw, th, tfmt, raw = tex
            bpp, conv = _texel_fn(tfmt)
            md = max(tw, th)
        blend = ab[:2] if ab else (4, 0)
        inv = 1.0 / area
        attr = [(v[3], v[4], v[5], v[7], v[8], v[9], v[10]) for v in vs]
        lit = cc is not None and cc[1] == 1           # factor LOCAL: modulate by vertex colour
        cconst = cc is not None and cc[3] == 2        # other = CONSTANT
        for py in range(miny, maxy):
            fy = py + 0.5
            row = py * W
            for px in range(minx, maxx):
                fx = px + 0.5
                w0 = ((x1 - fx) * (y2 - fy) - (x2 - fx) * (y1 - fy)) * inv
                w1 = ((x2 - fx) * (y0 - fy) - (x0 - fx) * (y2 - fy)) * inv
                w2 = 1.0 - w0 - w1
                if w0 < 0 or w1 < 0 or w2 < 0:
                    continue
                a0, a1, a2 = attr
                oow = w0 * a0[4] + w1 * a1[4] + w2 * a2[4]
                idx = row + px
                if dm:
                    if oow < zb[idx]:
                        continue
                if uses_tex:
                    sow = w0 * a0[5] + w1 * a1[5] + w2 * a2[5]
                    tow = w0 * a0[6] + w1 * a1[6] + w2 * a2[6]
                    s = sow / oow if oow else 0.0
                    t = tow / oow if oow else 0.0
                    u = int(s * md / 256.0) % tw
                    v = int(t * md / 256.0) % th
                    o = (v * tw + u) * bpp
                    val = raw[o] | (raw[o + 1] << 8) if bpp == 2 else raw[o]
                    r, g, b, al = conv(val)
                    if lit:
                        r = r * (w0 * a0[0] + w1 * a1[0] + w2 * a2[0]) / 255.0
                        g = g * (w0 * a0[1] + w1 * a1[1] + w2 * a2[1]) / 255.0
                        b = b * (w0 * a0[2] + w1 * a1[2] + w2 * a2[2]) / 255.0
                elif cconst:
                    r, g, b, al = (const >> 16) & 255, (const >> 8) & 255, const & 255, (const >> 24) & 255
                else:
                    r = w0 * a0[0] + w1 * a1[0] + w2 * a2[0]
                    g = w0 * a0[1] + w1 * a1[1] + w2 * a2[1]
                    b = w0 * a0[2] + w1 * a1[2] + w2 * a2[2]
                    al = w0 * a0[3] + w1 * a1[3] + w2 * a2[3]
                if at != 7 and al <= aref:
                    continue
                o3 = idx * 3
                if blend == (1, 5):            # src alpha, one minus src alpha
                    k = max(0.0, min(al, 255.0)) / 255.0
                    r = r * k + fb[o3] * (1 - k)
                    g = g * k + fb[o3 + 1] * (1 - k)
                    b = b * k + fb[o3 + 2] * (1 - k)
                elif blend == (4, 4) or blend == (1, 4):   # additive
                    k = 1.0 if blend[0] == 4 else max(0.0, min(al, 255.0)) / 255.0
                    r = fb[o3] + r * k
                    g = fb[o3 + 1] + g * k
                    b = fb[o3 + 2] + b * k
                fb[o3] = 255 if r > 255 else (0 if r < 0 else int(r))
                fb[o3 + 1] = 255 if g > 255 else (0 if g < 0 else int(g))
                fb[o3 + 2] = 255 if b > 255 else (0 if b < 0 else int(b))
                if dm and dmask:
                    zb[idx] = oow
