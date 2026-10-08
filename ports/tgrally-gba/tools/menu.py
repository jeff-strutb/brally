"""menu.py RAM WD ROM OUT.c -- the main menu for the GBA: mainmenu.c BrMainMenu's
screen, as frontbuttons.c BrMenu draws it.

  RAM  a TGR_RAMDUMP taken on the main menu (scripts/menu_tour.txt)
  WD   a TGR_WORLDDUMP of the same run: the screen's camera, and every triangle
       of the icons, the ring and the pointers as the game's renderer textured and
       lit them
  ROM  the cartridge: the menu's sounds and its music

What it holds, as the game has it:
  the rows: BrMainMenu's items (D_80272360), their labels and flags
  the icons: each row's model (models.py) with its textures at each brightness
       level, a triangle's from the recording (the faces it never drew, always
       facing away, left out); its parts' flags: 4 drawn both ways round (the
       part clears the cull mode), 8 the blended pass, 0x400 lit by a fixed grey
       (D_80271D9C[flags & 3]) in place of the lights
  the camera: BrScreenCameraSet's projection, to the GBA's pixels, folded with
       what BrMenu and BrMenuIconDraw put before it (Rx(-90), the 1/32 scale, the
       ring's Rx(12)); the runtime adds the spin Rz and the icon's place
  the lights: D_80271F90 (directional) and D_80271F88 (ambient), the light's way
       as it reaches a model before its spin
  the backdrop: BrRomImageDraw of the background (D_80272048) tinted by the
       screen's colours (combine 4), the panel (D_802722C0, combine 2), the ring
       (BrMenuRingDraw) and the pointers (BrMenuBackdropDraw): one 160x128 picture,
       as none of them moves
  the text: the title, each row's label and the prompts (BrFrontPromptSelect),
       as the printer draws them (hud.py glyph_set), and the A and B buttons
       (D_80271D70, D_80271D84)
  the move and choose sounds (D_001BF480, D_001BFEC0, BrSfxVoiceStart's rate
       and levels) and the front end's music (D_000EBC00, BrMusicStart)"""
import math
import os
import struct
import sys

import convert
import hud
import models
import sound
import wdump

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../tools/tgrally'))
import extract_xm  # noqa: E402

SW, SH = 160, 128
TOUR = 500                       # the tour's frames on the menu start here
REST = 700                       # a frame with the carousel at rest
TITLE = b'TOP GEAR RALLY'
R1, G1, B1, R2, G2, B2 = 0, 0, 0, 0x40, 0x40, 0x40   # BrMainMenu's tint


def rot(a, x, y, z):
    """guRotateF (a in degrees, row vectors)"""
    a = math.radians(a)
    s, c = math.sin(a), math.cos(a)
    t = 1 - c
    return [[x * x + c * (1 - x * x), x * y * t + z * s, z * x * t - y * s],
            [x * y * t - z * s, y * y + c * (1 - y * y), y * z * t + x * s],
            [z * x * t + y * s, y * z * t - x * s, z * z + c * (1 - z * z)]]


def mul3(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def image(snap, rec):
    """a BrRomImage record (native: start, end, data, w, h, fmt, siz) -> (w, h, texel(x, y)
    -> (r, g, b, a) 0..255): BrRomImageDraw puts an image's first row at the bottom"""
    start, end, data, w, h, fmt, siz = struct.unpack('<3I2H2B', snap.bytes(rec, 18))
    if (fmt, siz) == (4, 0):                         # I4
        raw = snap.bytes(data, w * h // 2)

        def get(x, y):
            b = raw[(y * w + x) // 2]
            v = (b >> 4 if x % 2 == 0 else b & 15) * 17
            return (v, v, v, 255)
    elif (fmt, siz) == (3, 2):                       # IA16
        raw = snap.bytes(data, w * h * 2)

        def get(x, y):
            i, a = raw[(y * w + x) * 2], raw[(y * w + x) * 2 + 1]
            return (i, i, i, a)
    elif (fmt, siz) == (0, 2):                       # RGBA16
        raw = snap.bytes(data, w * h * 2)

        def get(x, y):
            v = struct.unpack_from('>H', raw, (y * w + x) * 2)[0]
            return ((v >> 11 & 31) * 255 // 31, (v >> 6 & 31) * 255 // 31, (v >> 1 & 31) * 255 // 31, 255 if v & 1 else 0)
    else:
        sys.exit('menu.py: image format %d/%d' % (fmt, siz))
    return w, h, lambda x, y: get(x, h - 1 - y)


def place(canvas, w, h, get, x0, y0, dw, dh, combine):
    """BrRomImageDraw at (x0, y0), dw by dh, into the 320x240 canvas: each pixel the
    average of the texels under it, over the canvas by its alpha"""
    for y in range(dh):
        for x in range(dw):
            sx0, sx1 = x * w // dw, max(x * w // dw + 1, (x + 1) * w // dw)
            sy0, sy1 = y * h // dh, max(y * h // dh + 1, (y + 1) * h // dh)
            cs = [get(u, v) for v in range(sy0, sy1) for u in range(sx0, sx1)]
            c = combine(tuple(sum(q[k] for q in cs) / len(cs) for k in range(4)))
            if c[3] <= 0:
                continue
            a = c[3] / 255.0
            p = canvas[y0 + y][x0 + x]
            canvas[y0 + y][x0 + x] = tuple(p[k] * (1 - a) + c[k] * a for k in range(3))


def to_screen(P, r, j):
    """a recorded vertex (post-modelview) through the projection: N64 pixels (320x240)"""
    v = [sum(r[4 * j + k] * P[k * 4 + c] for k in range(3)) + P[12 + c] for c in range(4)]
    return (v[0] / v[3] * 160 + 160, -v[1] / v[3] * 120 + 120)


def gouraud(canvas, pts, cols):
    """a smooth-shaded triangle into the canvas: the pixels whose centres it covers"""
    (x0, y0), (x1, y1), (x2, y2) = pts
    den = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
    if not den:
        return
    for y in range(max(0, int(min(y0, y1, y2))), min(240, int(max(y0, y1, y2)) + 1)):
        for x in range(max(0, int(min(x0, x1, x2))), min(320, int(max(x0, x1, x2)) + 1)):
            px, py = x + 0.5, y + 0.5
            b1 = ((px - x0) * (y2 - y0) - (x2 - x0) * (py - y0)) / den
            b2 = ((x1 - x0) * (py - y0) - (px - x0) * (y1 - y0)) / den
            b0 = 1 - b1 - b2
            if min(b0, b1, b2) < 0:
                continue
            canvas[y][x] = tuple(b0 * cols[0][k] + b1 * cols[1][k] + b2 * cols[2][k] for k in range(3))


class BlendTextures(convert.Textures):
    """the blended pass's textures: each texel already halved (gba/raster.s adds it to
    half the pixel under it), clear where the game's would be all but clear"""

    def get(self, key, ms, mt, level):
        rows, W, H = self.image(key, ms, mt)
        w0, h0 = self.tex[key][0] * (2 if ms else 1), self.tex[key][1] * (2 if mt else 1)
        su, sv = W / convert.pow2(w0), H / convert.pow2(h0)
        k = (key, ms, mt, level)
        if k not in self.index:
            f = level / convert.LEVELS
            data = []
            for row in rows:
                for x in range(convert.TEXMAX):
                    r_, g, b, a = row[x % W]
                    c = convert.rgb555((r_ * f, g * f, b * f))
                    data.append(0x8000 if a < 32 else (c >> 1) & 0x3DEF)
            self.index[k] = len(self.out)
            self.out.append((W.bit_length() - 1, H.bit_length() - 1, 2, data))
        return self.index[k], su, sv


def main():
    ram_p, wd_p, rom_p, out_p = sys.argv[1:5]
    snap = models.Snapshot(ram_p)
    ram = snap.ram
    rom = extract_xm.normalise_rom(open(rom_p, 'rb').read(), rom_p)
    cams, wtris, tex, camlist = wdump.read(wd_p)
    P, vp = cams[REST]
    if vp[:2] != (320.0, 240.0) or vp[3:5] != (320.0, 240.0):
        sys.exit('menu.py: the viewport is not the full screen')
    menu_tris = [t for t in wtris if t[0] >= TOUR]
    look = models.looks(menu_tris)
    o = ['/* the main menu (tools/menu.py), from the game: BrMainMenu\'s rows and icons, the',
         '   screen\'s camera, lights, backdrop, text, buttons, sounds and music */',
         '#include "menu.h"', '']

    # ---- the rows and their icons ----
    texs, btexs = convert.Textures(tex), BlendTextures(tex)
    grey = [snap.bytes(0x80271D9C + 4 * k, 4)[1] for k in range(4)]   # (0x00GGGGGG as bytes: 00 40 40 40 ..)
    rows, icons, slots, slot_of = [], [], [], {}
    for i in range(7):
        it = snap.be32(0x80272360 + 4 * i)
        lab, flags, icon = snap.le32(it), snap.le32(it + 4), snap.le32(it + 8)
        a = lab & 0x7FFFFF
        rows.append((ram[a:ram.index(b'\0', a)], flags))
        icons.append(models.load(snap, icon))
    icon_addr = set()
    out_models = []
    for mi, m in enumerate(icons):
        vidx, verts, tris = {}, [], []
        for pflags, pos, ptris in m.parts:
            if any(pos):
                sys.exit('menu.py: a part away from its model\'s origin')
            for t in ptris:
                k = tuple(sorted(t))
                icon_addr.add(k)
                lk = look.get(k)
                if not lk or not lk[0] or lk[1] not in tex:
                    continue                             # never drawn on the tour: always facing away
                textured, key, st, tile, col = lk
                s0, t0, ss, ts, cs, ct, ms, mt = tile[:8]
                blend = bool(pflags & 8)
                tx = btexs if blend else texs
                lv = []
                for level in range(1, convert.LEVELS + 1):
                    ti, su, sv = tx.get(key, ms, mt, level)
                    lv.append((blend, ti))
                slot = slot_of.get(tuple(lv))
                if slot is None:
                    slot = slot_of[tuple(lv)] = len(slots)
                    slots.append(tuple(lv))
                wb, hb = tx.out[lv[0][1]][:2]
                uv = [((st[a][0] * ss - s0) * su * 16, (st[a][1] * ts - t0) * sv * 16) for a in t]
                W, H = 16 << wb, 16 << hb
                du = math.floor(min(u for u, v in uv) / W) * W
                dv = math.floor(min(v for u, v in uv) / H) * H
                uv = [(u - du, v - dv) for u, v in uv]
                ix = []
                for a in t:
                    if a not in vidx:
                        vidx[a] = len(verts)
                        verts.append(m.verts[a])
                    ix.append(vidx[a])
                fl = (1 if pflags & 4 else 0) | (2 if blend else 0) | (4 if pflags & 0x400 else 0)
                tris.append((ix, fl, grey[pflags & 3] if pflags & 0x400 else 0, slot,
                             [max(-32768, min(32767, int(round(c)))) for p in uv for c in p]))
        out_models.append((verts, tris))
    # a front face's winding on the GBA's screen (y down): the drawn one-sided triangles' in the recording
    one_sided = {}
    for m in icons:
        for pflags, pos, ptris in m.parts:
            for t in ptris:
                one_sided[tuple(sorted(t))] = (not pflags & 4, t)
    front = [0, 0]
    for fr, textured, wb_, key, r, ci in menu_tris:
        k = tuple(sorted(r[30:33]))
        if k in one_sided and one_sided[k][0]:
            pt = {r[30 + j]: to_screen(camlist[ci][1], r, j) for j in range(3)}
            (x0, y0), (x1, y1), (x2, y2) = (pt[a] for a in one_sided[k][1])
            front[(x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0) > 0] += 1
    if min(front) * 50 > max(front):
        sys.exit('menu.py: the drawn faces wind both ways: %r' % front)
    front_sign = 1 if front[1] > front[0] else -1

    if max(len(v) for v, t in out_models) > 480:
        sys.exit('menu.py: an icon has %d vertices: gba/menu.c keeps 480 (s_mv)' % max(len(v) for v, t in out_models))
    for mi, (verts, tris) in enumerate(out_models):
        vs = []
        for v in verts:
            n = math.sqrt(v[5] ** 2 + v[6] ** 2 + v[7] ** 2) or 1.0
            vs.append('{%d,%d,%d,%d,%d,%d,0}' % (v[0], v[1], v[2], *(int(round(c / n * 127)) for c in v[5:8])))
        o.append('static const MVert s_v%d[%d] = {%s};' % (mi, len(verts), ','.join(vs)))
        o.append('static const MTri s_t%d[%d] = {%s};' % (mi, len(tris), ','.join(
            '{%d,%d,%d,%d,%d,%d,0,{%s}}' % (t[0][0], t[0][1], t[0][2], t[1], t[2], t[3], ','.join(map(str, t[4]))) for t in tris)))
    o.append('const MModel g_menu_icon[7] = {%s};' % ','.join(
        '{s_v%d,%d,s_t%d,%d}' % (i, len(v), i, len(t)) for i, (v, t) in enumerate(out_models)))
    nt = len(texs.out)
    o.append('const uint16_t g_menu_slot[%d][8] = {%s};' % (len(slots), ','.join(
        '{%s}' % ','.join(str(ti + nt if b else ti) for b, ti in s) for s in slots)))
    alltex = texs.out + btexs.out
    for i, (wb, hb, alpha, data) in enumerate(alltex):
        o.append('static const uint16_t s_tex%d[%d] = {%s};' % (i, len(data), ','.join(map(str, data))))
    o.append('const Tex g_menu_tex[%d] = {%s};' % (len(alltex), ','.join(
        '{s_tex%d,%d,%d,%d,%d,%d}' % (i, ((1 << wb) - 1) << 1, ((1 << hb) - 1) << 7, wb, hb, int(alpha))
        for i, (wb, hb, alpha, data) in enumerate(alltex))))
    o.append('const MenuRow g_menu_row[7] = {%s};' % ','.join('{"%s",%d}' % (l.decode(), f) for l, f in rows))
    o.append('const int g_menu_front = %d;' % front_sign)

    # ---- the camera and the lights ----
    # the projection to the GBA's pixels times W: X = 80 (x + w), Y = 64 (w - y) (BrViewportSet's
    # full screen: 640x480 framebuffer pixels, scale and offset half of each)
    Pg = [[80 * (P[k * 4] + P[k * 4 + 3]), 64 * (P[k * 4 + 3] - P[k * 4 + 1]), P[k * 4 + 3]] for k in range(4)]
    lin = mul3(rot(-90, 1, 0, 0), rot(12, 1, 0, 0))           # BrMenuIconDraw's Rx(-90), BrMenu's Rx(12)
    Q = [[sum(lin[k][j] * Pg[j][c] for j in range(3)) / 32 for c in range(3)] for k in range(3)]
    rx12 = rot(12, 1, 0, 0)
    K = [sum(90 * rx12[1][j] * Pg[j][c] for j in range(3)) + Pg[3][c] for c in range(3)]   # its y, 240 * 6 / 16
    Px = [sum(rx12[0][j] * Pg[j][c] for j in range(3)) for c in range(3)]                  # its x, the same way

    def q12(v):
        return int(round(v * 4096))
    o.append('/* a model point (after its spin) to the GBA\'s pixels times W, Q12: the rows for x, y, z,')
    o.append('   the icon\'s x (N64 pixels), and the constant (its y and the camera) */')
    o.append('const int32_t g_menu_cam[5][3] = {%s};' % ','.join(
        '{%s}' % ','.join(str(q12(v)) for v in row) for row in Q + [Px, K]))
    ld = snap.bytes(0x80271F90, 16)
    dirv = struct.unpack('3b', ld[8:11])
    ln = math.sqrt(sum(c * c for c in dirv))
    L = [c / ln for c in dirv]
    G = [sum(lin[k][j] * L[j] for j in range(3)) for k in range(3)]   # the light's way in the model before its spin
    amb = snap.bytes(0x80271F88, 4)[0]
    o.append('/* the light\'s way as it meets a model before its spin (Q12), its level, the ambient\'s */')
    o.append('const MenuLight g_menu_light = {{%d,%d,%d},%d,%d};' % (q12(G[0]), q12(G[1]), q12(G[2]), ld[0], amb))

    # ---- the backdrop: background, panel, ring, pointers ----
    canvas = [[(0.0, 0.0, 0.0)] * 320 for _ in range(240)]
    w, h, get = image(snap, 0x80272048)
    place(canvas, w, h, get, 0, 0, 320, 240,
          lambda c: (R1 + (R2 - R1) * c[0] / 255, G1 + (G2 - G1) * c[1] / 255, B1 + (B2 - B1) * c[2] / 255, 255))
    pw, ph, pget = image(snap, 0x802722C0)
    place(canvas, pw, ph, pget, 25, 14, 280, 32, lambda c: c)   # D_80271FB4.. as BrMainMenu sets them
    for fr, textured, wb_, key, r, ci in wtris:
        if fr == REST and tuple(sorted(r[30:33])) not in icon_addr:
            if textured:
                sys.exit('menu.py: a textured triangle in the ring or the pointers')
            pts = [to_screen(camlist[ci][1], r, j) for j in range(3)]
            gouraud(canvas, pts, [[min(255, r[12 + 4 * j + k] * 255) for k in range(3)] for j in range(3)])
    bg = []
    for y in range(SH):
        for x in range(SW):
            acc, wsum = [0.0, 0.0, 0.0], 0.0
            ya, yb = y * 240 / SH, (y + 1) * 240 / SH
            for yy in range(int(ya), min(240, -int(-yb))):
                wy = min(yy + 1, yb) - max(yy, ya)
                for xx in (2 * x, 2 * x + 1):
                    for k in range(3):
                        acc[k] += canvas[yy][xx][k] * wy
                    wsum += wy
            bg.append(convert.rgb555([v / wsum for v in acc]))
    o.append('const uint16_t g_menu_backdrop[%d] __attribute__((aligned(4))) = {%s};' % (SW * SH, ','.join(map(str, bg))))

    # ---- the text: the title, the labels, the prompts; the buttons ----
    tx = hud.Text(ram)
    need = {}

    def say(s, x, y):
        for g in tx.print(s, x, y):
            need.setdefault(g[:3], g[3:])
    tx.highlight = tx.alt = 0
    tx.align, tx.size = 2, 30
    say(b'%ry' + TITLE, 160, 240 // 6 - 2)
    tx.size = 20
    for lab, f in rows:
        say(b'%ry' + lab, 160, 240 * 20 // 64)
    tx.align, tx.size = 0, 11
    say(b'%wwSelect', 0x73, 240 * 19 // 20 - 3)
    say(b'%wwGo Back', 0xaf, 240 * 19 // 20 - 3)
    glyph_lines, banks = hud.glyph_set(ram, tx, need, 'g_menu', 0)
    o += glyph_lines
    tile0 = 0
    for line in glyph_lines:
        if '_glyph_tiles[' in line:
            tile0 = int(line.split('[')[1].split(']')[0]) // 32
    pal = [0] * 256
    for bi, b in enumerate(banks):
        for k, c in enumerate(b):
            pal[bi * 16 + 1 + k] = c
    btn_tiles, btns = bytearray(), []
    for bi, (rec, bx) in enumerate(((0x80271D70, 100), (0x80271D84, 160))):   # BrFrontPromptSelect
        bw, bh, bget = image(snap, rec)
        px = {}
        for y in range(12):
            for x in range(12):
                cs = [bget(u, v) for v in range(y * bh // 12, (y + 1) * bh // 12) for u in range(x * bw // 12, (x + 1) * bw // 12)]
                a = sum(c[3] for c in cs) / len(cs) / 255
                if a > 0:
                    px[(216 + y) * 320 + bx + x] = tuple(sum(c[k] * c[3] for c in cs) / (a * 255 * len(cs)) for k in range(3)) + (a,)
        gx, gy, gw, gh, img = hud.scale(px, bx, 216, bx + 12, 228)
        cols = [hud.rgb555(c) for row in img for c in row if c]
        q = hud.quantize(cols, 15)
        bp = sorted(set(q.values()))
        bank = len(banks) + bi
        for k, c in enumerate(bp):
            pal[bank * 16 + 1 + k] = c
        btns.append((gx, gy, tile0 + len(btn_tiles) // 32, bank))
        btn_tiles += hud.tiles(img, 16, 16, 4, lambda r, x, y: 0 if y >= len(r) or x >= len(r[y]) or r[y][x] is None
                               else bp.index(q[hud.rgb555(r[y][x])]) + 1)
    o.append('const uint16_t g_menu_pal[256] = {%s};' % ','.join(map(str, pal)))
    o.append('const uint8_t g_menu_btn_tiles[%d] __attribute__((aligned(4))) = {%s};' % (len(btn_tiles), ','.join(map(str, btn_tiles))))
    o.append('const int g_menu_btn_bytes = %d;' % len(btn_tiles))
    o.append('/* the A and B buttons: GBA place, first tile (after the glyphs\'), palette bank */')
    o.append('const MenuSprite g_menu_btn[2] = {%s};' % ','.join('{%d,%d,%d,%d}' % b for b in btns))

    # ---- the sounds and the music ----
    efx = snap.bytes(0x802A49CC, 1)[0] * snap.bytes(0x802A49D0, 1)[0]
    lvl = min(255, efx * 0x20 >> 16)                        # BrSfxVoiceStart's 0x20 each side, the effects level and fade
    rate = int(round((1 << 32) * (sound.N64_RATE / 2) / sound.RATE / (1 << 20)))   # its rate 1.0
    o.append('const int g_menu_sfx_rate = %d, g_menu_sfx_level = %d;' % (rate, lvl))
    for name, off in (('g_menu_move', 0x1BF480), ('g_menu_choose', 0x1BFEC0)):   # BrRomUnpack, silence after
        data, _ = extract_xm.unpack_container(rom, off)
        data = data + bytes(sound.SFX_TAIL)
        o.append('static ' + sound.c_bytes('%s_data' % name, data))
        o.append('const SndSample %s = { %s_data, %d, 0, 0, 0, 0 };' % (name, name, len(data) - sound.SFX_TAIL))
    xm, _ = extract_xm.unpack_container(rom, 0xEBC00)
    order, restart, speed, nch, pats, smps = sound.mod_load(xm)
    level = snap.bytes(0x802A49C4, 1)[0] * snap.bytes(0x802A49C8, 1)[0]
    o += sound.module_c('g_menu', order, restart, speed, nch, pats, smps, level)
    open(out_p, 'w').write('\n'.join(o) + '\n')
    print('%s: icons %s triangles, %d textures (%d KB), %d glyphs, front %r, music %d channels level %d, sfx level %d' % (
        out_p, '/'.join(str(len(t)) for v, t in out_models), len(alltex), sum(len(t[3]) * 2 for t in alltex) // 1024,
        len(need), front, nch, level, lvl))


if __name__ == '__main__':
    main()
