"""menuicons.py: the games' boxes on Boss Rally's Remaster Options page
(rm_menuopts.cpp), into ports/remastered/brally/src/core/menus/rm_menuicons.c.

Top Gear Rally's USA and Japanese boxes and Boss Rally's, the boxes Top Gear
Rally's options screen turns (boxicons.py, their art and proportions), here
still pictures, as the page's pictures are: each seen from YAW degrees to its
left and PITCH above, its front, its left edge and its top showing, lit from
the upper left (LIGHT over AMBIENT), fitted in BOX_W x BOX_H pixels, 4x4
samples a pixel.

The menus are 16-bit pictures (RGB565) blitted with a colour key, so each
pixel becomes:
  the key       where the picture leaves the lobe showing (alpha under a half)
  its colour    over the lobe's blue (43, 80, 117) by its alpha, so the
                antialiased edge sits on the lobe as it would drawn there

    .venv/bin/python ports/remastered/tools/menuicons.py [DIR]
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import boxicons  # noqa: E402

ROOT = os.path.join(HERE, "..", "..", "..")
OUT = os.path.join(ROOT, "ports", "remastered", "brally", "src", "core", "menus", "rm_menuicons.c")
LOBE = (43, 80, 117)
KEY = 0xF81F                    # magenta: no pixel of any picture comes out as it
BOX_W, BOX_H = 76, 72           # a box's picture at most, in the menus' pixels
YAW, PITCH = 30.0, 12.0         # seen from the left and above
LIGHT = (-0.45, -0.55, 0.70)    # from the upper left, in front
AMBIENT = 0.7
SS = 4                          # samples a pixel, each way


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def pixels(im):
    w, h = im.size
    px = im.load()
    out = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < 128:
                out.append(KEY)
                continue
            k = a / 255.0
            v = rgb565(*[round(v * k + l * (1 - k)) for v, l in zip((r, g, b), LOBE)])
            out.append(v if v != KEY else KEY ^ 0x20)
    return w, h, out


def main():
    icons = [(b.name + "box", boxicons.render(b, BOX_W, BOX_H, YAW, PITCH, LIGHT, AMBIENT, SS))
             for b in boxicons.boxes()]
    if len(sys.argv) > 1:                         # a look at them: menuicons.py DIR
        for name, im in icons:
            im.save(os.path.join(sys.argv[1], "rm_%sicon.png" % name))
    with open(OUT, "w") as f:
        f.write("/* rm_menuicons.c: the games' boxes on the Remaster Options page\n"
                " * (rm_menuopts.cpp) in RGB565, the key where the lobe shows.  Written by\n"
                " * ports/remastered/tools/menuicons.py: do not edit. */\n")
        f.write("const unsigned short rm_menuicon_key = 0x%04X;\n" % KEY)
        for name, im in icons:
            w, h, px = pixels(im)
            f.write("const int rm_%sicon_w = %d, rm_%sicon_h = %d;\n" % (name, w, name, h))
            f.write("const unsigned short rm_%sicon[%d] = {\n" % (name, w * h))
            for i in range(0, len(px), 12):
                f.write("    " + ", ".join("0x%04X" % v for v in px[i:i + 12]) + ",\n")
            f.write("};\n")
            print("menuicons: %s %dx%d" % (name, w, h))
    print("menuicons: -> %s" % os.path.relpath(OUT, ROOT))


if __name__ == "__main__":
    main()
