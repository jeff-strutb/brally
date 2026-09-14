#!/usr/bin/env python3
"""Convert a raw 2352-byte-sector CD data track to a 2048-byte-sector ISO.

Detects Mode 1 vs Mode 2/Form 1 per sector from the 16-byte header and
extracts the 2048-byte user data of each sector. Audio tracks are not
handled -- point this at the data track only.

Usage:
  python3 tools/cdrip.py <track.bin> <out.iso>
"""
import sys
import os

RAW = 2352
SYNC = b"\x00" + b"\xff" * 10 + b"\x00"


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    size = os.path.getsize(src)
    if size % RAW:
        sys.exit(f"{src}: size {size} is not a multiple of {RAW}")
    nsect = size // RAW
    modes = {}
    with open(src, "rb") as f, open(dst, "wb") as out:
        for i in range(nsect):
            sec = f.read(RAW)
            if sec[:12] != SYNC:
                sys.exit(f"sector {i}: bad sync {sec[:12].hex()}")
            mode = sec[15]
            if mode == 1:
                user = sec[16:16 + 2048]
            elif mode == 2:
                # XA: 8-byte subheader follows the header; submode bit 5 = Form 2
                submode = sec[18]
                if submode & 0x20:
                    sys.exit(f"sector {i}: Mode 2 Form 2 (2324 B user data) -- "
                             f"not an ISO9660 data sector")
                user = sec[24:24 + 2048]
            else:
                sys.exit(f"sector {i}: unknown mode {mode}")
            modes[mode] = modes.get(mode, 0) + 1
            out.write(user)
    print(f"{dst}: {nsect} sectors, modes {modes}")


if __name__ == "__main__":
    main()
