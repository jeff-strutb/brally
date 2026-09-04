#!/usr/bin/env python3
"""Extract resources from a PE binary's .rsrc section.

Written for the SetVideo.exe port: its five wizard dialogs exist only as
DLGTEMPLATE blobs inside the shipped executable, so a port that wants the
real dialogs has to read them out of the original image.

    tools/rsrc_dump.py orig/SetVideo.exe --list
    tools/rsrc_dump.py orig/SetVideo.exe --dialogs
    tools/rsrc_dump.py orig/SetVideo.exe --dialogs --json out.json

Reads only; never writes to the binary.
"""
import argparse
import json
import struct
import sys

RT = {
    1: "CURSOR", 2: "BITMAP", 3: "ICON", 4: "MENU", 5: "DIALOG",
    6: "STRING", 7: "FONTDIR", 8: "FONT", 9: "ACCELERATOR",
    10: "RCDATA", 11: "MESSAGETABLE", 12: "GROUP_CURSOR",
    14: "GROUP_ICON", 16: "VERSION", 24: "MANIFEST",
}

# Predefined dialog control class ordinals.
DLG_CLASS = {
    0x80: "BUTTON", 0x81: "EDIT", 0x82: "STATIC",
    0x83: "LISTBOX", 0x84: "SCROLLBAR", 0x85: "COMBOBOX",
}

# Button styles (low nibble of the control style).
BS = {
    0x0: "PUSHBUTTON", 0x1: "DEFPUSHBUTTON", 0x2: "CHECKBOX",
    0x3: "AUTOCHECKBOX", 0x4: "RADIOBUTTON", 0x5: "3STATE",
    0x6: "AUTO3STATE", 0x7: "GROUPBOX", 0x8: "USERBUTTON",
    0x9: "AUTORADIOBUTTON", 0xa: "OWNERDRAW",
}


class PE:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        d = self.data
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        if d[pe:pe + 4] != b"PE\0\0":
            raise SystemExit(f"{path}: not a PE image")
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        optsz = struct.unpack_from("<H", d, pe + 20)[0]
        self.imagebase = struct.unpack_from("<I", d, pe + 24 + 28)[0]
        off = pe + 24 + optsz
        self.sections = []
        for i in range(nsec):
            b = off + i * 40
            name = d[b:b + 8].rstrip(b"\0").decode("latin1")
            vsize, va, rawsize, rawoff = struct.unpack_from("<IIII", d, b + 8)
            self.sections.append((name, va, vsize, rawoff, rawsize))

    def rva_to_off(self, rva):
        for name, va, vsize, rawoff, rawsize in self.sections:
            if va <= rva < va + max(vsize, rawsize):
                return rawoff + (rva - va)
        return None

    def rsrc_base(self):
        for name, va, vsize, rawoff, rawsize in self.sections:
            if name == ".rsrc":
                return va, rawoff, rawsize
        raise SystemExit("no .rsrc section")


def parse_resource_dir(pe):
    """Walk the three-level resource tree -> [(type, name, lang, rva, size)]."""
    rva0, off0, _ = pe.rsrc_base()
    d = pe.data
    out = []

    def entry_name(nameval):
        """High bit set = string name at that offset; else an integer id."""
        if nameval & 0x80000000:
            o = off0 + (nameval & 0x7FFFFFFF)
            n = struct.unpack_from("<H", d, o)[0]
            return d[o + 2:o + 2 + n * 2].decode("utf-16-le")
        return nameval

    def walk(diroff, depth, path):
        nnamed, nid = struct.unpack_from("<HH", d, off0 + diroff + 12)
        base = off0 + diroff + 16
        for i in range(nnamed + nid):
            nameval, offval = struct.unpack_from("<II", d, base + i * 8)
            nm = entry_name(nameval)
            if offval & 0x80000000:
                walk(offval & 0x7FFFFFFF, depth + 1, path + [nm])
            else:
                o = off0 + offval
                rva, size = struct.unpack_from("<II", d, o)
                out.append((path + [nm], rva, size))

    walk(0, 0, [])
    return out


def wstr(d, o):
    """sz_Or_Ord: 0xFFFF + ordinal, 0x0000 empty, else NUL-terminated UTF-16."""
    first = struct.unpack_from("<H", d, o)[0]
    if first == 0xFFFF:
        return ("ord", struct.unpack_from("<H", d, o + 2)[0], o + 4)
    if first == 0x0000:
        return ("str", "", o + 2)
    s = []
    while True:
        c = struct.unpack_from("<H", d, o)[0]
        o += 2
        if c == 0:
            break
        s.append(chr(c))
    return ("str", "".join(s), o)


def parse_dialog(d, o, size):
    """Decode a dialog resource: classic DLGTEMPLATE or DLGTEMPLATEEX.

    SetVideo.exe's seven dialogs are all EX (dlgVer 1, signature 0xFFFF),
    which reorders the header, adds a help id and widens each item's control
    id from a WORD to a DWORD. Reading an EX template with the classic layout
    yields plausible-looking nonsense rather than an error, so the form is
    decided by the signature, never assumed.
    """
    ver, sig = struct.unpack_from("<HH", d, o)
    ext = (ver == 1 and sig == 0xFFFF)

    if ext:
        exstyle, style = struct.unpack_from("<II", d, o + 8)
        cdit = struct.unpack_from("<H", d, o + 16)[0]
        x, y, cx, cy = struct.unpack_from("<hhhh", d, o + 18)
        p = o + 26
    else:
        style, exstyle = struct.unpack_from("<II", d, o)
        cdit = struct.unpack_from("<H", d, o + 8)[0]
        x, y, cx, cy = struct.unpack_from("<hhhh", d, o + 10)
        p = o + 18

    _, menu, p = wstr(d, p)
    _, cls, p = wstr(d, p)
    _, title, p = wstr(d, p)

    font = None
    if style & 0x40:                       # DS_SETFONT
        pts = struct.unpack_from("<H", d, p)[0]
        if ext:
            weight = struct.unpack_from("<H", d, p + 2)[0]
            italic = d[p + 4]
            p += 6                         # pointsize, weight, italic, charset
        else:
            weight, italic = 0, 0
            p += 2
        _, face, p = wstr(d, p)
        font = {"points": pts, "weight": weight, "italic": italic,
                "face": face}

    items = []
    for _ in range(cdit):
        p = (p + 3) & ~3                   # each item starts DWORD-aligned
        if ext:
            iex, istyle = struct.unpack_from("<II", d, p + 4)
            ix, iy, icx, icy = struct.unpack_from("<hhhh", d, p + 12)
            iid = struct.unpack_from("<I", d, p + 20)[0]
            q = p + 24
        else:
            istyle, iex = struct.unpack_from("<II", d, p)
            ix, iy, icx, icy = struct.unpack_from("<hhhh", d, p + 8)
            iid = struct.unpack_from("<H", d, p + 16)[0]
            q = p + 18
        kind, clsval, q = wstr(d, q)
        klass = DLG_CLASS.get(clsval, clsval) if kind == "ord" else clsval
        kind2, text, q = wstr(d, q)
        text = f"#{text}" if kind2 == "ord" else text
        extra = struct.unpack_from("<H", d, q)[0]
        q += 2 + extra
        p = q

        sub = None
        if klass == "BUTTON":
            sub = BS.get(istyle & 0xF, f"BS_{istyle & 0xF:x}")
        # The resource compiler spells IDC_STATIC as -1 in either width.
        if iid in (0xFFFF, 0xFFFFFFFF):
            iid = -1
        items.append({
            "id": iid, "class": klass, "subtype": sub, "text": text,
            "x": ix, "y": iy, "cx": icx, "cy": icy,
            "style": istyle, "exstyle": iex,
        })

    return {
        "title": title, "x": x, "y": y, "cx": cx, "cy": cy,
        "style": style, "exstyle": exstyle, "font": font, "items": items,
    }


def c_escape(s):
    out = []
    for ch in s:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        elif 0x20 <= ord(ch) < 0x7F:
            out.append(ch)
        else:
            # The templates are UTF-16; emit UTF-8 bytes so the caption
            # survives into a C string literal unchanged.
            out.extend(f"\\{b:03o}" for b in ch.encode("utf-8"))
    return "".join(out)


def emit_c(dialogs, path, source):
    """Write the decoded dialogs as a C table for the macOS port to render."""
    with open(path, "w") as f:
        f.write("/* GENERATED by tools/rsrc_dump.py -- do not edit.\n"
                f" * Dialog templates read out of {source}.\n"
                " * Derived from the retail binary; not committed. */\n"
                '#include "br_dialogres.h"\n\n')
        for did in sorted(dialogs):
            dlg = dialogs[did]
            f.write(f"static const BrDlgItem items_{did}[] = {{\n")
            for it in dlg["items"]:
                kind = it["subtype"] or it["class"]
                f.write(f'    {{ {it["id"]}, "{kind}", "{c_escape(it["text"])}", '
                        f'{it["x"]}, {it["y"]}, {it["cx"]}, {it["cy"]}, '
                        f'{it["style"]:#010x}u }},\n')
            f.write("};\n\n")
        f.write("const BrDlgTemplate br_dialogs[] = {\n")
        for did in sorted(dialogs):
            dlg = dialogs[did]
            f.write(f'    {{ {did}, "{c_escape(dlg["title"])}", '
                    f'{dlg["cx"]}, {dlg["cy"]}, '
                    f'{len(dlg["items"])}, items_{did} }},\n')
        f.write("};\n\n"
                "const int br_dialog_count = "
                f"{len(dialogs)};\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("--list", action="store_true", help="list every resource")
    ap.add_argument("--dialogs", action="store_true", help="decode RT_DIALOG")
    ap.add_argument("--json", help="write decoded dialogs to this file")
    ap.add_argument("--emit-c", help="write decoded dialogs as a C table")
    args = ap.parse_args()

    pe = PE(args.binary)
    entries = parse_resource_dir(pe)

    if args.list:
        for path, rva, size in entries:
            t = path[0]
            tn = RT.get(t, t) if isinstance(t, int) else t
            print(f"{str(tn):14} {str(path[1]):>10}  lang={path[2]}  "
                  f"rva={rva:#x} size={size}")

    if args.dialogs or args.json or args.emit_c:
        dialogs = {}
        for path, rva, size in entries:
            if path[0] != 5:
                continue
            off = pe.rva_to_off(rva)
            dlg = parse_dialog(pe.data, off, size)
            dialogs[path[1]] = dlg
            if args.dialogs:
                print(f"\n=== DIALOG {path[1]} (0x{path[1]:x})  "
                      f"\"{dlg['title']}\"  {dlg['cx']}x{dlg['cy']} DLU"
                      + (f"  font {dlg['font']['points']}pt "
                         f"{dlg['font']['face']}" if dlg["font"] else ""))
                for it in dlg["items"]:
                    kind = it["subtype"] or it["class"]
                    print(f"  {it['id']:>6} (0x{it['id']:03x})  {kind:<16}"
                          f" @({it['x']:>3},{it['y']:>3}) "
                          f"{it['cx']:>3}x{it['cy']:<3}  {it['text']!r}")
        if not dialogs:
            raise SystemExit(f"{args.binary}: no RT_DIALOG resources")
        if args.json:
            with open(args.json, "w") as f:
                json.dump({str(k): v for k, v in sorted(dialogs.items())},
                          f, indent=2)
            print(f"-> {args.json}", file=sys.stderr)
        if args.emit_c:
            emit_c(dialogs, args.emit_c, args.binary)
            print(f"-> {args.emit_c} ({len(dialogs)} dialogs)", file=sys.stderr)


if __name__ == "__main__":
    main()
