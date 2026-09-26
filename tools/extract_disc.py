#!/usr/bin/env python3
"""Extract the whole Boss Rally data track into a directory the port runs from.

The retail game reads its data from the CD: it finds the drive by volume
label ("Boss Rally") and opens tracks/, cars/, sfx/, Images/ ... under that
root. A Mac build has no drive, so the CD root is this directory instead. The
current ports/macos harness still reads the per-asset extracts under
testdata/ (tools/extract_assets.sh); this tree is the root for the real boot
path (RallyMain), which needs the whole disc, not a curated subset.

Everything on the data track is copied except directx/ (the DirectX 6
redistributable -- 1,483 files the game never opens). Names keep the disc's
case; the port's file layer matches case-insensitively, as Win9x did.

Idempotent: a file already present with the right size is skipped. Nothing
here is committed (testdata/ is ignored).

Usage: tools/extract_disc.py [image.bin] [outdir]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import extract_iso as iso   # noqa: E402

SKIP = ('directx',)


def main():
    image = sys.argv[1] if len(sys.argv) > 1 else 'reference/brally/BossRally.BIN'
    out = sys.argv[2] if len(sys.argv) > 2 else 'testdata/disc'
    fh, _ = iso.open_image(image)
    n = got = 0
    for path, lba, size, isdir in iso.walk(fh):
        if path.split('/')[0].lower() in SKIP:
            continue
        dst = os.path.join(out, path)
        if isdir:
            os.makedirs(dst, exist_ok=True)
            continue
        n += 1
        if os.path.isfile(dst) and os.path.getsize(dst) == size:
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        data = iso.iso_read(fh, lba * iso.USER, size)
        if len(data) != size:
            raise SystemExit("%s: short read (%d of %d)" % (path, len(data), size))
        with open(dst + '.part', 'wb') as f:
            f.write(data)
        os.replace(dst + '.part', dst)
        got += 1
    print("disc: %d files under %s (%d written, volume %r)"
          % (n, out, got, iso.read_volume_labels(fh)
             if hasattr(iso, 'read_volume_labels') else '?'))


if __name__ == '__main__':
    main()
