#!/usr/bin/env python3
"""nativecheck_state.py -- the game-state half of nativecheck.sh.

  pack DIR            gzip each BR_DUMP_EVERY dump in DIR (<frame>.bin)
  cmp BASE BASE2 NEW  compare NEW's dumps with BASE's, frame by frame

Two runs of one build differ in a few bytes of the data area: the 32-bit
window addresses the core stores for host memory (br_addr32.h) carry the
host allocator's offsets and window order, which move from run to run. An
aligned word that holds a window address (high byte 0x20..0x7F) in both
runs is left out, as is any word two runs of the baseline (BASE, BASE2)
already disagree on; any other byte that differs is reported, with the
global that holds it.
A global the new build declares with another layout is absent from its
dumps (script.c dumps only what keeps its layout); MASK lists those that
hold the platform's own objects rather than game state."""
import csv
import gzip
import os
import sys

LO = 0x10077000
MASK = [
    (0x10273310, 24),   # g_BrDPlayCrit: a CRITICAL_SECTION (a host mutex pointer)
    (0x105CCBD8, 0x94), # the 3dfx card's description of itself, which the native port never asks for
]


def load(d, f):
    b = bytearray(gzip.open(os.path.join(d, f)).read())
    for va, n in MASK:
        b[va - LO:va - LO + n] = bytes(n)
    return b


def globals_by_va():
    g = []
    for r in csv.reader(open("ports/brally/types/globals.csv")):
        if len(r) > 3 and r[0].startswith("0x") and r[3].isdigit():
            g.append((int(r[0], 16), int(r[3]), r[1]))
    return g


def who(g, va):
    best = None
    for a, n, name in g:
        if a <= va < a + max(n, 1) and (best is None or n < best[1]):
            best = (a, n, name)
    return "%s+0x%x" % (best[2], va - best[0]) if best else "?"


def pack(d):
    for f in sorted(os.listdir(d)):
        if f.endswith(".bin"):
            p = os.path.join(d, f)
            with open(p, "rb") as i, gzip.open(p + ".gz", "wb", 1) as o:
                o.write(i.read())
            os.remove(p)


def cmp(a, a2, b):
    g = globals_by_va()
    frames = sorted(f for f in os.listdir(a) if f.endswith(".bin.gz"))
    bad = 0
    for f in frames:
        if not os.path.exists(os.path.join(b, f)):
            print("state: frame %s missing in the new run" % f[:-7])
            return 1
        x = load(a, f)
        y = load(b, f)
        if x == y:
            continue
        noise = load(a2, f) if os.path.exists(os.path.join(a2, f)) else x
        diff = [i for i in range(len(x)) if x[i] != y[i]
                and x[i & ~3:(i & ~3) + 4] == noise[i & ~3:(i & ~3) + 4]
                and not (0x20 <= x[(i & ~3) + 3] <= 0x7F and 0x20 <= y[(i & ~3) + 3] <= 0x7F)]
        if diff:
            names = []
            for i in diff:
                n = who(g, LO + i)
                if n not in names:
                    names.append(n)
            print("state: frame %s, %d bytes differ: %s" % (f[:-7], len(diff), " ".join(names[:8])))
            bad = 1
            break
    if not bad:
        print("state: identical less noise (%d dumps)" % len(frames))
    return bad


if sys.argv[1] == "pack":
    pack(sys.argv[2])
elif sys.argv[1] == "cmp":
    sys.exit(cmp(sys.argv[2], sys.argv[3], sys.argv[4]))
