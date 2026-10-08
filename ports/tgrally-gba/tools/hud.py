"""hud.py RAM ROM HST F0 F1 OUT.c -- the race HUD for the GBA.

  RAM   a TGR_RAMDUMP taken in the race: the text printer's tables and font
        (drawing/textstate.c: the character map, the glyph edges, the two
        fonts' IA8 glyph pages and their shading ramps)
  ROM   the cartridge: the car's dial face, palettes and rev lamps
  HST   a TGR_HUDSTATE of the race: what the HUD reads, each frame
  F0..F1  the retraces the GBA replays

Where everything goes is the game's own logic, ported here as it is in
racing/racehud.c (BrHudDraw, BrHudDialDraw, BrHudTimesDraw, BrHudLapDraw)
and drawing/textstate.c (BrTextPrint, BrTextWidth, BrTextEmitString): the
strings, sizes, colours and alignment, each glyph's rectangle and advance.
gba/hud.s runs the same logic each retrace; here it finds every glyph the
race draws (character, size, colours) so each can be made once.  A glyph is
drawn as BrTextEmitString has the RDP draw it (the port's rcp.c and soft
renderer, which model the RDP): a texture rectangle over the glyph's tile of
the font page, IA8, bilinear (the game's filter), its texture coordinates
stepped from the rectangle's s, t and slopes; two cycles: the shading ramp
(tile 1, IA8 in TMEM as LOADBLOCK leaves it) mixes the environment and
primitive colours, the glyph's intensity multiplies that (with the alternate
combine, less the ramp), the glyph's alpha is its cover.
The dial face and lamps are the ROM's images and palette as BrHudDialDraw
loads them; the needle is its quad.

The GBA's screen is the N64's 320x240 at 3/4 by 2/3: the images are scaled
so (area average, cover over a half kept)."""
import struct
import sys

SX, SY = 3 / 4, 2 / 3
DIAL = 4                         # the car record whose dial the HUD shows (TYPE-SP)


# ---- the text printer (drawing/textstate.c) ----------------------------------------
class Text:
    def __init__(self, ram):
        def u32s(a, n):
            return list(struct.unpack_from('<%di' % n, ram, a & 0x7FFFFF))
        self.map = ram[0x2A1740:0x2A1740 + 96]
        self.small = u32s(0x802A187C, 59)
        self.large = u32s(0x802A17A0, 55)
        self.highlight = 0                       # D_8028BDC0
        self.alt = 0                             # D_8028BDC8
        self.align = 0                           # D_8028BDC4: 0 left, 1 right, 2 centre
        self.custom = None                       # D_8028BDCC..: (env, prim)
        self.size = 15                           # D_803519D8

    def width(self, s, size):                    # BrTextWidth
        w = 0
        div, pad, tbl = (20, 4, self.small) if size < 25 else (40, 7, self.large)
        i = 0
        while i < len(s):
            c = s[i]
            if c < 0x21 or c >= 0x80:
                w += size * 12 // 40
            else:
                if c == 0x25 and i + 1 < len(s):
                    n = s[i + 1]
                    if n == 0x25:
                        i += 1
                    elif n in (0x69, 0x6E):
                        i += 2
                        continue
                    elif i + 2 < len(s):
                        i += 3
                        continue
                g = self.map[s[i] - 0x21]
                w += (tbl[g + 1] - tbl[g] - pad + 1) * size // div
            i += 1
        return w

    def print(self, s, x, y):                    # BrTextPrint
        if self.align == 2:
            x -= self.width(s, self.size) >> 1
        elif self.align == 1:
            x -= self.width(s, self.size)
        return self.emit(s, x, y)

    PRIM = {'r': (0xbe, 0, 0), 'o': (0xcd, 0x5f, 0), 'O': (0xff, 0x78, 0), 'y': (0xff, 0xf5, 0),
            'Y': (0xff, 0xfa, 0x80), 'g': (0, 0x96, 0), 'b': (0, 0, 0xc8), 'p': (0xc8, 0, 0xc8),
            '1': (0xff, 0xff, 0xff), 'w': (0xff, 0xff, 0xff), '5': (0x80, 0x80, 0x80), '0': (0, 0, 0)}
    ENV = {'r': (0xc8, 0, 0), 'o': (0xcd, 0x5f, 0), 'O': (0xff, 0x78, 0), 'y': (0xd2, 0xbe, 0),
           'Y': (0xd2, 0xc8, 0x69), 'g': (0, 0x96, 0), 'b': (0, 0, 0xc8), 'p': (0xc8, 0, 0xc8),
           '1': (0xff, 0xff, 0xff), 'w': (0xff, 0xff, 0xff), '5': (0x80, 0x80, 0x80), '0': (0, 0, 0)}

    def emit(self, s, x, y):                     # BrTextEmitString: -> [(glyph, size, colours, x0, y0, x1, y1)]
        size = self.size
        y -= size * 30 // 40
        cell, pad, tbl = (20, 4, self.small) if size < 25 else (40, 7, self.large)
        if self.custom:
            env, prim = self.custom
        elif self.highlight:
            env, prim = (0xff, 0x7f, 0), (0xff, 0xff, 0x7f)
        else:
            env, prim = (0xc8, 0, 0), (0xe6, 0xe6, 0)
        out, i = [], 0
        while i < len(s):
            c = s[i]
            if c != 0x20:
                if c == 0x25 and i + 1 < len(s):
                    n = s[i + 1]
                    if n == 0x25:
                        i += 1
                        c = s[i]
                    elif n in (0x69, 0x6E):
                        i += 2
                        continue
                    elif i + 2 < len(s):
                        p, e = chr(s[i + 1]), chr(s[i + 2])
                        prim = self.PRIM.get(p, prim)
                        env = self.ENV.get(e, env)
                        i += 3
                        continue
                if 0x21 <= c < 0x80:
                    g = self.map[c - 0x21]
                    uls = tbl[g]
                    w = tbl[g + 1] - uls + 1
                    n = w * size // cell
                    out.append((g, size, (prim, env, self.alt), x, y, x + n, y + size))
                    x += (w - pad) * size // cell
            else:
                x += size * 12 // 40 + 1
            i += 1
        return out


# ---- a glyph as the RDP draws it (BrTextEmitString's rectangle) ----------------------
def wrap(i, n, clamp, mask, cw):                  # rdr_soft.c's wrap (no mirror here)
    if clamp and cw > 0:
        i = 0 if i < 0 else cw - 1 if i >= cw else i
    if mask > 0:
        m = i % mask
        return n - 1 if m >= n else m
    return 0 if i < 0 else n - 1 if i >= n else i


def bilerp(tex, w, h, x, y, cs, ms, cws, ct, mt, cwt):
    """rdr_soft.c's sample with the filter on: three texels of the 2x2"""
    x0, y0 = int(x // 1), int(y // 1)
    fx, fy = x - x0, y - y0
    xa, xb = wrap(x0, w, cs, ms, cws), wrap(x0 + 1, w, cs, ms, cws)
    ya, yb = wrap(y0, h, ct, mt, cwt), wrap(y0 + 1, h, ct, mt, cwt)
    p00, p10, p01, p11 = tex[ya][xa], tex[ya][xb], tex[yb][xa], tex[yb][xb]
    if fx + fy < 1:
        return [(p00[k] + fx * (p10[k] - p00[k]) + fy * (p01[k] - p00[k])) / 255.0 for k in range(4)]
    return [(p11[k] + (1 - fx) * (p01[k] - p11[k]) + (1 - fy) * (p10[k] - p11[k])) / 255.0 for k in range(4)]


def ramp_texels(ram, addr):
    """the shading ramp as tile 1 reads it: loaded (LOADBLOCK, no line swaps) to TMEM
    0x1B0 and read as IA8 (SETTILE fmt 3 siz 1), 8 by 40 on a one-word line, an odd
    row's words swapped as the RDP reads them"""
    tm = bytes(ram[addr & 0x7FFFFF:(addr & 0x7FFFFF) + 320])
    rows = []
    for y in range(40):
        row = []
        for x in range(8):
            v = tm[(y * 8 + x) ^ (4 if y & 1 else 0)]
            row.append(((v >> 4) * 17,) * 3 + ((v & 15) * 17,))
        rows.append(row)
    return rows


class Fonts:
    def __init__(self, ram, tx):
        self.ram, self.tx = ram, tx
        self.page = {False: (0x8029DA00, 392, 20, 4, tx.small), True: (0x8028FE00, 704, 40, 7, tx.large)}
        self.ramp = {(False, 0): ramp_texels(ram, 0x8028C070), (False, 1): ramp_texels(ram, 0x8028C1B0),
                     (True, 0): ramp_texels(ram, 0x8028BDF0), (True, 1): ramp_texels(ram, 0x8028BF30)}

    def draw(self, g, size, colours, x0, y0, x1, y1):
        """-> dict (y * 320 + x) -> (r, g, b, a) of the glyph's rectangle at (x0, y0)"""
        (prim, env, alt) = colours
        large = size >= 25
        base, texw, cell, pad, tbl = self.page[large]
        row = cell if g < 27 else 0
        uls = tbl[g]
        w = tbl[g + 1] - uls + 1
        n = w * size // cell
        page = self.ram[base & 0x7FFFFF:]
        glyph = [[((page[(row + y) * texw + uls + x] >> 4) * 17,) * 3 + ((page[(row + y) * texw + uls + x] & 15) * 17,)
                  for x in range(w)] for y in range(cell)]
        ramp = self.ramp[(large, alt)]
        dsdx = ((w << 10) // n) / 1024.0
        num = (cell << 10) - (1 << 10)
        dtdy = -(num // size) / 1024.0
        s0, t0 = 0.5, (cell - 1) + 0.5
        pr, en = [c / 255.0 for c in prim], [c / 255.0 for c in env]
        out = {}
        for py in range(y0, y1):
            for px in range(x0, x1):
                s = s0 + (px - x0) * dsdx
                t = t0 + (py - y0) * dtdy
                g0 = bilerp(glyph, w, cell, s - 0.5, t - 0.5, True, 64, w, True, 64, cell)
                r1 = bilerp(ramp, 8, 40, s - 0.5, t - 0.5, False, 8, 8, True, 64, 40)
                comb = [min(1, max(0, (pr[k] - en[k]) * r1[k] + en[k])) for k in range(3)]
                if alt:
                    col = [min(1, max(0, (comb[k] - r1[k]) * g0[k])) for k in range(3)]
                else:
                    col = [min(1, max(0, comb[k] * g0[k])) for k in range(3)]
                a = g0[3]
                if a > 0 and 0 <= px < 320 and 0 <= py < 240:
                    out[py * 320 + px] = (col[0] * 255, col[1] * 255, col[2] * 255, a)
        return out


def hud_time(t):                                # BrHudTimeDraw's numbers
    c = int(t * 100.0)
    s = c // 100
    return s // 60, s - s // 60 * 60, c - s * 100


def f32(x):
    return struct.unpack('<f', struct.pack('<I', x))[0]


def draws(tx, st):
    """racehud.c's BrHudDraw for one frame (one player, race mode 0..2, the
    view's own camera): the glyphs, as the game places them"""
    (fr, mode, ff10, mph, lay, night, nlaps, vx, vy, vw, vh, panel, laps, race_t, best_t, total_t, left_t,
     pos, speed, rev, e38, e40, kind, cam3) = st
    out = []
    tx.highlight, tx.align = 0, 1                # BrHudTimesDraw
    tx.size = 15
    y, dy = vy + 20, 30 if lay == 1 else 0
    for label, t, yy in ((b'%15TOTAL TIME', total_t, y), (b'%15BEST LAP' if laps >= nlaps else b'%15LAP TIME',
                                                            best_t if laps >= nlaps else race_t, y + dy)):
        m, sec, cs = hud_time(f32(t))
        out += tx.print(b'%%ww%d\'%02d"%02d' % (m, sec, cs), 296, yy + 15)
        out += tx.print(label, 296, yy)
    x = vx + 16                                  # BrHudLapDraw
    if laps < nlaps:
        tx.highlight, tx.align, tx.size = 0, 0, 15
        out += tx.print(b'%%y1LAP %d/%d' % (laps + 1, nlaps), x, vy + 5 + 15)
    y = vy + vh - 12
    x -= 2
    tx.alt, tx.align = 1, 0
    tx.custom = ((0xff, 0xf0, 0x7d), (0xff, 0x78, 0))
    buf = b'%d' % (pos + 1)
    suffix, adj = {0: (b'st', -3), 1: (b'nd', 1), 2: (b'rd', 0)}.get(pos, (b'th', 1))
    tx.size = 40
    w = tx.width(buf, 40)
    out += tx.print(buf, x - 1, y - 1)
    tx.size = 20
    out += tx.print(suffix, x + adj + w + 3, y - 15)
    tx.alt = 0
    sp = max(f32(speed), 0.0)                    # BrHudDraw: the speed
    tx.highlight, tx.align = 0, 1
    v = ('%.0f' % (sp / 1.609344 if mph else sp)).encode()
    x, y = 266, vy + vh - 4 - panel
    tx.size = 20
    out += tx.print(b'%yw' + v, x if mph else x - 3, y - 3)
    tx.size, tx.align = 15, 0
    out += tx.print(b'%wwmph' if mph else b'%wwkph', x if mph else x - 3, y - 3)
    return out


def scale(px, x0, y0, x1, y1, sharp=False):
    """an N64 rectangle's pixels (dict index -> rgba, 320 wide) at the GBA's scale:
    -> (gx, gy, gw, gh, rows of (r, g, b) or None).  The area's average, or (sharp: the
    dial's art, one-pixel strokes and digits) the pixel that stands out most from the
    rest of the area, bright on dark or dark on light, so a stroke survives"""
    gx0, gy0 = int(x0 * SX), int(y0 * SY)
    gx1, gy1 = -int(-x1 * SX), -int(-y1 * SY)
    rows = []
    for gy in range(gy0, gy1):
        row = []
        for gx in range(gx0, gx1):
            sx0, sx1, sy0, sy1 = gx / SX, (gx + 1) / SX, gy / SY, (gy + 1) / SY
            acc, cov, area = [0.0, 0.0, 0.0], 0.0, 0.0
            for yy in range(int(sy0), -int(-sy1)):
                for xx in range(int(sx0), -int(-sx1)):
                    wgt = (min(xx + 1, sx1) - max(xx, sx0)) * (min(yy + 1, sy1) - max(yy, sy0))
                    if wgt <= 0:
                        continue
                    area += wgt
                    p = px.get(yy * 320 + xx) if 0 <= xx < 320 and 0 <= yy < 240 else None
                    if p:
                        cov += wgt * p[3]
                        for k in range(3):
                            acc[k] += wgt * p[3] * p[k]
            if sharp and cov > 0:
                cand = [px.get(yy * 320 + xx) for yy in range(int(sy0), -int(-sy1)) for xx in range(int(sx0), -int(-sx1))
                        if 0 <= xx < 320 and 0 <= yy < 240]
                cand = [c for c in cand if c]
                lum = [0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2] for c in cand]
                mean = sum(lum) / len(lum)
                row.append(tuple(cand[max(range(len(cand)), key=lambda k: abs(lum[k] - mean))][:3]))
                continue
            row.append(tuple(v / cov for v in acc) if area and cov / area > 0.5 else None)
        rows.append(row)
    return gx0, gy0, gx1 - gx0, gy1 - gy0, rows


def rgb555(c):
    return (int(c[0]) >> 3) | (int(c[1]) >> 3) << 5 | (int(c[2]) >> 3) << 10


def quantize(colours, n):
    """median cut of RGB555 colours to at most n"""
    cols = sorted(set(colours))
    if len(cols) <= n:
        return {c: c for c in cols}
    boxes = [cols]
    while len(boxes) < n:
        boxes.sort(key=lambda b: -len(b))
        b = boxes.pop(0)
        if len(b) < 2:
            boxes.append(b)
            break
        ch = max(range(3), key=lambda k: max(c >> 5 * k & 31 for c in b) - min(c >> 5 * k & 31 for c in b))
        b.sort(key=lambda c: c >> 5 * ch & 31)
        boxes += [b[:len(b) // 2], b[len(b) // 2:]]
    out = {}
    for b in boxes:
        avg = [sum(c >> 5 * k & 31 for c in b) // len(b) for k in range(3)]
        rep = avg[0] | avg[1] << 5 | avg[2] << 10
        for c in b:
            out[c] = rep
    return out


def tiles(img, w, h, bpp, index):
    """an image (rows of palette indices) as GBA sprite tiles, 1D order"""
    out = bytearray()
    for ty in range(0, h, 8):
        for tx in range(0, w, 8):
            for y in range(8):
                if bpp == 8:
                    out += bytes(index(img, tx + x, ty + y) for x in range(8))
                else:
                    for x in range(0, 8, 2):
                        out.append(index(img, tx + x, ty + y) | index(img, tx + x + 1, ty + y) << 4)
    return out


def main():
    ram_p, rom_p, hst_p, f0, f1, out_p = sys.argv[1:7]
    f0, f1 = int(f0), int(f1)
    ram = open(ram_p, 'rb').read()[64:]
    rom = open(rom_p, 'rb').read()
    d = open(hst_p, 'rb').read()
    i, states, dial = 0, {}, None
    while i < len(d):
        t = d[i:i + 1]
        i += 1
        if t == b'D':
            dial = d[i:i + 0x60]
            i += 0x60
        else:
            w = struct.unpack_from('<24I', d, i)
            i += 96 + 48
            states[w[0]] = w
    tx = Text(ram)
    # every glyph the race draws, and where it is first drawn
    need = {}
    for fr in sorted(states):
        if f0 <= fr <= f1:
            for g in draws(tx, states[fr]):
                key = g[:3]
                if key not in need:
                    need[key] = (fr, g[3:])
    # each drawn once, as the RDP draws it, at the GBA's scale
    fonts = Fonts(ram, tx)
    glyph_img = {}
    for key, (fr, r) in need.items():
        x0, y0, x1, y1 = r
        glyph_img[key] = scale(fonts.draw(key[0], key[1], key[2], x0, y0, x1, y1), x0, y0, x1, y1)
    # glyph palettes: one 16-colour bank a colour set
    schemes = sorted({k[2] for k in glyph_img})
    banks, glyphs = [], []
    for si, sc in enumerate(schemes):
        cols = [rgb555(c) for k, g in glyph_img.items() if k[2] == sc for row in g[4] for c in row if c]
        q = quantize(cols, 15)
        pal = sorted(set(q.values()))
        banks.append(pal)
        for k in sorted(g for g in glyph_img if g[2] == sc):
            gx, gy, gw, gh, rows = glyph_img[k]
            glyphs.append((k, si, gw, gh, rows, q, pal))
    # the dial: BrHudDialDraw's images, from ROM.  Not the race car's own: the TYPE-SP's
    # (D_8028AE0C[DIAL]), whose gear number is the largest of the game's dials and so
    # reads on the GBA's screen
    dial = bytes(ram[0x28AE0C + DIAL * 0x60:0x28AE0C + (DIAL + 1) * 0x60])
    rom_dial = struct.unpack_from('<I', dial, 0x20)[0]
    dw, dh, lx, ly, lw, lh, mode, nx, ny = dial[0x28], dial[0x29], struct.unpack_from('b', dial, 0x2a)[0], \
        struct.unpack_from('b', dial, 0x2b)[0], dial[0x2c], dial[0x2d], dial[0x2e], dial[0x2f], dial[0x30]
    nmax, nrest = struct.unpack_from('<ff', dial, 0x34)
    night = states[f0][5] if f0 in states else 0
    palr = [struct.unpack_from('>H', rom, rom_dial + night * 0x200 + 2 * k)[0] for k in range(256)]

    def ci8(off, w, h):
        """a CI8 image as BrRomImageDraw puts it on the screen: its strips go up from
        the bottom, t falling, so the image's first row is the bottom one"""
        px = {}
        for y in range(h):
            for x in range(w):
                c = palr[rom[off + (h - 1 - y) * w + x]]
                if c & 1:
                    px[y * 320 + x] = ((c >> 11 & 31) * 255 / 31, (c >> 6 & 31) * 255 / 31, (c >> 1 & 31) * 255 / 31, 1.0)
        return px
    dial_x, dial_y = 296 - dw, 8 + 224 - dh - 4          # (the view: from the state)
    face = scale(ci8(rom_dial + 0x400, dw, dh), 0, 0, dw, dh, True)
    lamp_frames = max(st[21] for st in states.values()) + 2
    lamps = [scale(ci8(rom_dial + 0x400 + dw * dh + k * lw * lh, lw, lh), 0, 0, lw, lh, True) for k in range(lamp_frames)]
    dcols = [rgb555(c) for img in [face] + lamps for row in img[4] for c in row if c]
    dq = quantize(dcols, 127)
    dpal = sorted(set(dq.values()))

    o = ['/* the race HUD (tools/hud.py): the text printer\'s tables, the glyphs as the game',
         '   draws them, the car\'s dial from ROM, and what the HUD reads each retrace */',
         '#include "hud.h"', '']
    o.append('const uint8_t g_txt_map[96] = { %s };' % ', '.join(map(str, tx.map)))
    o.append('const int32_t g_txt_small[59] = { %s };' % ', '.join(map(str, tx.small)))
    o.append('const int32_t g_txt_large[55] = { %s };' % ', '.join(map(str, tx.large)))
    # BrTextEmitString's and BrTextWidth's arithmetic for the sizes the HUD uses (15, 20, 40):
    # each glyph's advance ((w - pad) * size / cell: BrTextWidth's measure is the same sum),
    # a space's width and advance, the lift (size * 30 / 40)
    adv = bytearray()
    for size in (15, 20, 40):
        cell, pad, tbl = (20, 4, tx.small) if size < 25 else (40, 7, tx.large)
        for g in range(64):
            if g + 1 < len(tbl):
                w = tbl[g + 1] - tbl[g] + 1
                adv.append(((w - pad) * size // cell) & 0xFF)
            else:
                adv.append(0)
    o.append('const uint8_t g_txt_adv[3 * 64] = { %s };' % ', '.join(map(str, adv)))
    o.append('const uint8_t g_txt_space[3][4] = { %s };  /* space width, space advance, lift */' % ', '.join(
        '{ %d, %d, %d, 0 }' % (sz * 12 // 40, sz * 12 // 40 + 1, sz * 30 // 40) for sz in (15, 20, 40)))
    # the OBJ palette: glyph banks 0.., the needle's bank, the dial in 128..255
    pal = [0] * 256
    for bi, b in enumerate(banks):
        for k, c in enumerate(b):
            pal[bi * 16 + 1 + k] = c
    needle_bank = len(banks)
    pal[needle_bank * 16 + 1] = 0 | 31 << 5                 # the needle's green (0, 0xFF, 0)
    for k, c in enumerate(dpal):
        pal[129 + k] = c
    o.append('const uint16_t g_hud_pal[256] = { %s };' % ', '.join(map(str, pal)))
    # glyph tiles (4bpp, 16 wide; 32 for the large font)
    data, gl = bytearray(), []
    for (key, si, gw, gh, rows, q, bp) in glyphs:
        sw = 16 if gw <= 16 and gh <= 16 else 32
        sh = 16 if sw == 16 else 32
        def idx(img, x, y, rows=rows, q=q, bp=bp):
            c = rows[y][x] if y < len(rows) and x < len(rows[y]) else None
            return 0 if c is None else bp.index(q[rgb555(c)]) + 1
        gl.append((key, si, len(data) // 32, sw, sh, gw, gh))
        data += tiles(rows, sw, sh, 4, idx)
    o.append('const uint8_t g_glyph_tiles[%d] __attribute__((aligned(4))) = { %s };' % (len(data), ', '.join(map(str, data))))
    o.append('/* by colour set, size and glyph: bank, first tile, sprite size (16 or 32), GBA size */')
    o.append('const HudGlyph g_glyphs[%d] = {' % len(gl))
    for key, si, t0, sw, sh, gw, gh in gl:
        o.append('    { %d, %d, %d, %d, %d, %d, %d },' % (si, key[1], key[0], sw, t0, gw, gh))
    o.append('};')
    o.append('const int g_nglyphs = %d;' % len(gl))
    # a glyph by colour set, size (15, 20, 40) and font glyph: its entry + 1, 0 if the race never draws it
    sizes = (15, 20, 40)
    lut = bytearray(len(schemes) * 3 * 64)
    for k, (key, si, t0, sw, sh, gw, gh) in enumerate(gl):
        if key[1] in sizes:
            lut[(si * 3 + sizes.index(key[1])) * 64 + key[0]] = k + 1
    o.append('const uint8_t g_glyph_lut[%d] = { %s };' % (len(lut), ', '.join(map(str, lut))))
    o.append('/* the colour sets, as (prim, env, alt): their banks */')
    o.append('const HudScheme g_schemes[%d] = {' % len(schemes))
    for sc in schemes:
        o.append('    { 0x%06X, 0x%06X, %d },' % (sc[0][0] << 16 | sc[0][1] << 8 | sc[0][2], sc[1][0] << 16 | sc[1][1] << 8 | sc[1][2], sc[2]))
    o.append('};')
    o.append('const int g_nschemes = %d, g_needle_bank = %d;' % (len(schemes), needle_bank))
    # dial face and lamp frames (8bpp, 64x64 and 16x16)
    def dimg(img, sw):
        rows = img[4]
        def idx(r, x, y):
            c = rows[y][x] if y < len(rows) and x < len(rows[y]) else None
            return 0 if c is None else 129 + dpal.index(dq[rgb555(c)])
        return tiles(rows, sw, sw, 8, idx)
    o.append('const uint8_t g_dial_face[%d] __attribute__((aligned(4))) = { %s };' % (64 * 64, ', '.join(map(str, dimg(face, 64)))))
    lt = bytearray()
    for l in lamps:
        lt += dimg(l, 16)
    o.append('const uint8_t g_dial_lamps[%d] __attribute__((aligned(4))) = { %s };' % (len(lt), ', '.join(map(str, lt))))
    o.append('const int g_dial_lamp_frames = %d;' % lamp_frames)
    import math
    # the needle: BrHudDialDraw's quad at angle 0 (tip 20 at +-0.05, base 7 at +-0.3), 64x64 4bpp, centred
    quad = [(20 * math.cos(-0.05), 20 * math.sin(-0.05)), (20 * math.cos(0.05), 20 * math.sin(0.05)),
            (7 * math.cos(0.3), 7 * math.sin(0.3)), (7 * math.cos(-0.3), 7 * math.sin(-0.3))]

    def inside(px_, py_):
        sgn = []
        for k in range(4):
            ax, ay = quad[k]
            bx, by = quad[(k + 1) % 4]
            sgn.append((bx - ax) * (py_ - ay) - (by - ay) * (px_ - ax))
        return all(s_ >= 0 for s_ in sgn) or all(s_ <= 0 for s_ in sgn)
    nrows = [[1 if inside(x - 32 + 0.5, y - 32 + 0.5) else 0 for x in range(64)] for y in range(64)]
    nt = tiles(nrows, 64, 64, 4, lambda r, x, y: r[y][x])
    o.append('const uint8_t g_needle[%d] __attribute__((aligned(4))) = { %s };' % (len(nt), ', '.join(map(str, nt))))
    o.append('/* the dial (racehud.c BrHudDialDraw): its place, the lamps\' offset, the needle\'s centre, angles (Q12 radians) */')
    o.append('const HudDial g_dial = { %d, %d, %d, %d, %d, %d, %d, %d, %d, %d };' % (
        dial_x, dial_y, dw, dh, lx, ly, nx, ny, int(round(nrest * 4096)), int(round(nmax * 4096))))
    # what the HUD reads, a retrace at a time (the frame drawn last before it)
    o.append('const HudState g_hud_state[%d] = {' % (f1 - f0 + 1))
    last = None
    for fr in range(f0, f1 + 1):
        st = states.get(fr, last)
        last = st
        (fr_, mode, ff10, mph, lay, night_, nlaps, vx, vy, vw, vh, panel, laps, race_t, best_t, total_t, left_t,
         pos, speed, rev, e38, e40, kind, cam3) = st
        tm, ts, tc = hud_time(f32(total_t))
        lm, ls, lc = hud_time(f32(best_t if laps >= nlaps else race_t))
        sp = max(f32(speed), 0.0)
        v = int('%.0f' % (sp / 1.609344 if mph else sp))
        lamp = e40 + 1 if f32(e38) >= 0 else 0
        o.append('    { %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d },' % (
            tm, ts, tc, lm, ls, lc, laps, nlaps, pos, v, int(f32(rev)), lamp, (mph & 1) | (1 if ff10 else 0) << 1))
    o.append('};')
    o.append('const int g_hud_frames = %d;' % (f1 - f0 + 1))
    st0 = states[min(k for k in states if k >= f0)]
    o.append('/* the player\'s view (D_8031B2C8[0]: x, y, w, h) and its panel\'s height (D_8028C7B4[0].h) */')
    o.append('const int32_t g_view[5] = { %d, %d, %d, %d, %d };' % (st0[7], st0[8], st0[9], st0[10], st0[11]))
    o.append('const int g_glyph_bytes = %d;' % len(data))
    # sin in Q12 by a 1024th of a turn (the needle's angle)
    o.append('const int16_t g_sin1024[1024] = { %s };' % ', '.join(str(int(round(math.sin(k * 2 * math.pi / 1024) * 4096))) for k in range(1024)))
    o.append('const uint32_t g_rand_seed = %d;            /* BrRandStep\'s state in the race (D_8028B790) */' % struct.unpack_from('<I', ram, 0x28B790)[0])
    open(out_p, 'w').write('\n'.join(o) + '\n')
    print('%s: %d glyphs in %d colour sets (%d bytes), dial %dx%d with %d lamp frames, %d retraces' % (
        out_p, len(gl), len(schemes), len(data), dw, dh, lamp_frames, f1 - f0 + 1))


if __name__ == '__main__':
    main()
