"""Score tools/xm_render.c against libopenmpt, the reference FT2 replayer.

xm_render.c is a small hand-written replayer (see the comment at the top of that
file for why it exists). Hand-written means its effect semantics are guesses
until something checks them, and a wrong guess is inaudible in the effect census
-- the module uses only effects we implement, and we implement them wrongly.
This is that check.

    brew install libopenmpt          # provides openmpt123
    python3 tools/extract_xm.py "<rom>" testdata/music_xm --keep-xm
    python3 tools/xm_oracle.py testdata/music_xm/*.xm
    python3 tools/xm_oracle.py --per-channel testdata/music_xm/xm_0EBC00.xm

libopenmpt is OpenMPT's replayer, validated against FastTracker II itself; it is
what MilkyTracker and VLC use. It is the oracle, not a second opinion.

Reading the score
-----------------
Both renders are matched in RMS (never in peak: libopenmpt hard-clips into int16
and xm_render scales to fit, so their peaks mean different things) and compared
as windowed Pearson correlation. Correlation, not a byte diff -- the two mixers
differ in interpolation and rounding, so exact equality is not on offer and is
not the goal. What the windows buy is LOCALISATION: a replayer bug shows up as a
few bad windows against a high median, and the timestamp says which rows to read.

    median >= 0.99   the effect semantics in that stretch agree
    a window << 0.9  something in those rows is implemented wrong

`--per-channel` rewrites the module with every channel but one blanked and scores
each in isolation, which turns "something around t=8s is wrong" into "channel 5
is wrong" -- then read that channel's rows with the effect dump.

`--lag` also reports the best-fit sample offset per window. A lag that grows over
time is a pitch or tempo error; a flat lag with poor correlation is not.
"""
import argparse
import glob
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

try:
    import numpy as np
except ImportError:
    sys.stderr.write("xm_oracle: needs numpy (pip install numpy)\n")
    sys.exit(2)

RATE = 44100
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class Fail(Exception):
    pass


# ------------------------------------------------------------------ the module

def u16(b, o):
    return b[o] | b[o + 1] << 8


def u32(b, o):
    return b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24


def parse_patterns(b):
    """Return (header_size, num_channels, [(hdr_off, rows, cells)...]).

    `cells` is rows x channels of (note, instr, vol, eff, par), decoded from
    either the packed or the unpacked cell encoding.
    """
    hsz = u32(b, 60)
    nch = u16(b, 68)
    npat = u16(b, 70)
    pats = []
    p = 60 + hsz
    for _ in range(npat):
        phl = u32(b, p)
        rows = u16(b, p + 5)
        psz = u16(b, p + 7)
        d = b[p + phl:p + phl + psz]
        i = 0
        cells = []
        for _r in range(rows):
            row = []
            for _c in range(nch):
                if i >= len(d):
                    row.append((0, 0, 0, 0, 0))
                    continue
                m = d[i]
                if m & 0x80:
                    i += 1
                    vals = [0, 0, 0, 0, 0]
                    for bit in range(5):
                        if m & (1 << bit):
                            vals[bit] = d[i]
                            i += 1
                    row.append(tuple(vals))
                else:
                    row.append(tuple(d[i:i + 5]))
                    i += 5
            cells.append(row)
        pats.append((p, rows, cells))
        p += phl + psz
    return hsz, nch, pats


# Effects that steer the whole song rather than one voice. Blanking a channel
# has to keep these or the isolated render plays at a different tempo and in a
# different order from the mix it is supposed to explain.
GLOBAL_EFFECTS = (0x0B, 0x0D, 0x0F)


def isolate_channel(b, keep):
    """Return the module with every channel but `keep` reduced to global effects.

    Patterns are rewritten in the unpacked 5-bytes-per-cell form, which is legal
    XM and which every replayer accepts; the channel count is unchanged so the
    mixer sees the same module minus the other voices.
    """
    hsz, nch, pats = parse_patterns(b)
    out = bytearray(b[:60 + hsz])
    tail_from = pats[-1][0] if pats else len(b)
    for (p, rows, cells) in pats:
        phl = u32(b, p)
        body = bytearray()
        for row in cells:
            for c, cell in enumerate(row):
                if c == keep:
                    body += bytes(cell)
                elif cell[3] in GLOBAL_EFFECTS:
                    body += bytes((0, 0, 0, cell[3], cell[4]))
                else:
                    body += b"\0\0\0\0\0"
        hdr = bytearray(b[p:p + phl])
        struct.pack_into("<H", hdr, 7, len(body))
        out += hdr + body
        tail_from = p + phl + u16(b, p + 7)
    out += b[tail_from:]
    return bytes(out)


# ----------------------------------------------------------------- rendering

def build_renderer(workdir):
    src = os.path.join(ROOT, "tools", "xm_render.c")
    exe = os.path.join(workdir, "xm_render")
    cc = os.environ.get("CC") or shutil.which("cc") or shutil.which("clang")
    if not cc:
        raise Fail("no C compiler found (set $CC)")
    r = subprocess.run([cc, "-std=c99", "-O2", "-o", exe, src, "-lm"],
                       capture_output=True, text=True)
    if r.returncode:
        raise Fail("failed to compile xm_render.c:\n%s" % r.stderr.strip())
    return exe


def render_ours(exe, xm, raw):
    r = subprocess.run([exe, "--rate", str(RATE), "--passes", "1",
                        "--fade-ms", "0", xm, raw], capture_output=True, text=True)
    if r.returncode:
        raise Fail("xm_render failed on %s:\n%s" % (xm, r.stderr.strip()))
    return np.fromfile(raw, dtype="<i2").astype(np.float32).reshape(-1, 2)


def render_oracle(xm, workdir):
    """Render with libopenmpt, matching xm_render's own mixer choices.

    --filter 2 is linear interpolation (xm_render interpolates linearly),
    --ramping 0 disables volume ramping (xm_render has none) and --dither 0
    keeps the int16 conversion deterministic. Anything left over is a real
    difference in effect semantics, which is the point.
    """
    if not shutil.which("openmpt123"):
        raise Fail("openmpt123 not on PATH -- install libopenmpt "
                   "(brew install libopenmpt)")
    r = subprocess.run(["openmpt123", "--render", "--output-type", "wav",
                        "--samplerate", str(RATE), "--channels", "2",
                        "--no-float", "--filter", "2", "--ramping", "0",
                        "--dither", "0", "--repeat", "0", "--banner", "0",
                        "--no-details", "--no-progress", "--no-meters",
                        "--force", "-q", xm],
                       capture_output=True, text=True, cwd=workdir)
    if r.returncode:
        raise Fail("openmpt123 failed on %s:\n%s"
                   % (os.path.basename(xm), r.stderr.strip()))
    wav = xm + ".wav"
    with wave.open(wav) as w:
        data = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2")
    os.remove(wav)
    return data.astype(np.float32).reshape(-1, 2)


# ------------------------------------------------------------------- scoring

def score(ref, ours, window, want_lag):
    """Windowed correlation of two renders, RMS-matched. Returns (stats, rows)."""
    n = min(len(ref), len(ours))
    a = ref[:n].mean(1)
    b = ours[:n].mean(1)
    if b.std() <= 0 or a.std() <= 0:
        return None, []
    b = b * (a.std() / b.std())
    w = int(window * RATE)
    m = n // w
    if m == 0:
        return None, []
    A = a[:m * w].reshape(m, w)
    B = b[:m * w].reshape(m, w)
    Az = A - A.mean(1, keepdims=True)
    Bz = B - B.mean(1, keepdims=True)
    denom = np.sqrt((Az ** 2).sum(1) * (Bz ** 2).sum(1))
    corr = np.where(denom > 0, (Az * Bz).sum(1) / np.maximum(denom, 1e-9), 1.0)
    rows = []
    for i in range(m):
        lag = 0
        if want_lag and corr[i] < 0.98:
            lag = best_lag(a, b, i * w, w)
        rows.append((i * w / float(RATE), float(corr[i]),
                     float(A[i].std()), float(B[i].std()), lag))
    stats = dict(median=float(np.median(corr)), p10=float(np.percentile(corr, 10)),
                 worst=float(corr.min()), windows=m,
                 frames_ref=len(ref), frames_ours=len(ours))
    return stats, rows


def best_lag(a, b, off, w, span=400):
    """Sample offset that best aligns `b` to `a` in this window."""
    if off < span or off + w + span >= len(b):
        return 0
    x = a[off:off + w]
    x = x - x.mean()
    xn = np.sqrt((x * x).sum())
    best, arg = -2.0, 0
    for lag in range(-span, span + 1, 4):
        y = b[off + lag:off + lag + w]
        y = y - y.mean()
        d = xn * np.sqrt((y * y).sum())
        if d <= 0:
            continue
        c = float((x * y).sum() / d)
        if c > best:
            best, arg = c, lag
    return arg


def report(label, stats, rows, top, want_lag):
    if stats is None:
        print("%-26s (silent)" % label)
        return
    drift = stats["frames_ours"] - stats["frames_ref"]
    print("%-26s median %.4f  p10 %.4f  worst %.4f   length %+d frames"
          % (label, stats["median"], stats["p10"], stats["worst"], drift))
    bad = sorted(rows, key=lambda r: r[1])[:top]
    bad = [r for r in bad if r[1] < 0.98]
    if not bad:
        return
    head = "     t(s)   corr   rms_ref  rms_ours"
    print(head + ("   lag" if want_lag else ""))
    for t, c, ra, rb, lag in sorted(bad):
        line = "  %7.2f  %.3f  %8.1f  %8.1f" % (t, c, ra, rb)
        print(line + ("  %5d" % lag if want_lag else ""))


# ---------------------------------------------------------------------- main

def run_one(exe, workdir, path, args, label=None, data=None):
    xm = os.path.join(workdir, os.path.basename(path))
    if data is None:
        shutil.copyfile(path, xm)
    else:
        with open(xm, "wb") as fh:
            fh.write(data)
    ref = render_oracle(xm, workdir)
    ours = render_ours(exe, xm, os.path.join(workdir, "ours.raw"))
    stats, rows = score(ref, ours, args.window, args.lag)
    report(label or os.path.basename(path), stats, rows, args.top, args.lag)
    return stats


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Score xm_render.c against libopenmpt (the reference FT2 replayer).")
    ap.add_argument("modules", nargs="+", help=".xm files (or globs)")
    ap.add_argument("--per-channel", action="store_true",
                    help="also score each channel in isolation")
    ap.add_argument("--window", type=float, default=0.25,
                    help="correlation window in seconds (0.25)")
    ap.add_argument("--top", type=int, default=12,
                    help="worst windows to list per module (12)")
    ap.add_argument("--lag", action="store_true",
                    help="also fit a sample offset per bad window")
    args = ap.parse_args(argv)

    paths = []
    for m in args.modules:
        paths.extend(sorted(glob.glob(m)) or [m])
    paths = [p for p in paths if os.path.exists(p)]
    if not paths:
        raise Fail("no such module(s)")

    workdir = tempfile.mkdtemp(prefix="xm_oracle.")
    try:
        exe = build_renderer(workdir)
        for path in paths:
            run_one(exe, workdir, path, args)
            if args.per_channel:
                with open(path, "rb") as fh:
                    b = fh.read()
                nch = u16(b, 68)
                for c in range(nch):
                    run_one(exe, workdir, path, args,
                            label="    channel %2d" % c, data=isolate_channel(b, c))
            print("")
    finally:
        shutil.rmtree(workdir, ignore_errors=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Fail as exc:
        sys.stderr.write("xm_oracle: %s\n" % exc)
        sys.exit(2)
    except KeyboardInterrupt:
        sys.exit(130)
