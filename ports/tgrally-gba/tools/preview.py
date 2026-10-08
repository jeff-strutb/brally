"""preview.py DUMP F0 F1 FRAME... -- render the reconstructed world from the
recorded camera at FRAME, on the host (flat, painter's order), as a check."""
import sys
import png
import world

W, H = 320, 240


def tex_avg(tex, key):
    w, h, px = tex[key]
    r = g = b = c = 0
    for i in range(w * h):
        if px[i * 4 + 3] > 128:
            r += px[i * 4]; g += px[i * 4 + 1]; b += px[i * 4 + 2]; c += 1
    return (r / c, g / c, b / c) if c else (128, 128, 128)


def main():
    cams, keep, tex = world.build(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))
    cache = {}
    for fr in map(int, sys.argv[4:]):
        f = min(cams, key=lambda c: abs(c - fr))
        P, vp = cams[f]
        img = [(110, 150, 210)] * (W * H)
        polys = []
        for pts, tx, key, col, st in keep:
            sc = []
            for x, y, z in pts:
                cx, cy, cz, cw = world.vmul([x, y, z, 1.0], P)
                if cw < 0.5:
                    break
                sc.append((cx / cw * vp[0] + vp[3], -cy / cw * vp[1] + vp[4], cw))
            if len(sc) < 3:
                continue
            (x0, y0, _), (x1, y1, _), (x2, y2, _) = sc
            if (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0) >= 0:
                continue
            if max(p[0] for p in sc) < 0 or min(p[0] for p in sc) > W or max(p[1] for p in sc) < 0 or min(p[1] for p in sc) > H:
                continue
            a = cache.setdefault(key, tex_avg(tex, key)) if tx and key in tex else (255, 255, 255)
            c = tuple(min(255, int(a[i] * (col[i] + col[4 + i] + col[8 + i]) / 3)) for i in range(3))
            polys.append((sc[0][2] + sc[1][2] + sc[2][2], sc, c))
        polys.sort(key=lambda p: -p[0])
        for _, sc, c in polys:
            ys = [p[1] for p in sc]
            for y in range(max(0, int(min(ys))), min(H - 1, int(max(ys))) + 1):
                yc, xs = y + 0.5, []
                for (ax, ay, _), (bx, by, _) in ((sc[0], sc[1]), (sc[1], sc[2]), (sc[2], sc[0])):
                    if ay <= yc < by or by <= yc < ay:
                        xs.append(ax + (yc - ay) * (bx - ax) / (by - ay))
                if len(xs) >= 2:
                    for x in range(max(0, int(min(xs) + 0.5)), min(W, int(max(xs) + 0.5))):
                        img[y * W + x] = c
        out = 'build/tgrally/gba/preview_%d.png' % fr
        png.write(out, W, H, img)
        print(out, len(polys))


if __name__ == '__main__':
    main()
