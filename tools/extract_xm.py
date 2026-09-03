"""Extract the N64 soundtrack from a Top Gear Rally ROM into lossless FLAC.

The N64 build stores its music as six FastTracker II .xm modules, zlib-packed
inside the ROM. This finds them, unpacks them, renders them to PCM and encodes
the result.

    python tools/extract_xm.py "reference/tgrally/Top Gear Rally (USA).z64" \
                               testdata/music_xm

testdata/music_xm is where this belongs: it is the directory setup.sh extracts
into and therefore the one anything downstream reads. Writing anywhere else
leaves a stale export in place and looks, from outside, like the run did
nothing.

Nothing here is committed and nothing is shipped -- see "Asset policy" in README.md.
The output is regenerated on each machine from that machine's own ROM.

Container format (established by unpacking and byte-comparing all six)
---------------------------------------------------------------------
The modules are not stored raw -- a search for the "Extended Module: " signature
in the ROM finds nothing. Each one sits in a chunked zlib container, all fields
big-endian:

    +0x00  u32   total size of the container, including this header
    +0x04  u32   uncompressed size
    then repeating until the uncompressed size is reached:
      u32   compressed size of this chunk
      ...   one complete zlib stream, decompressing to 16000 bytes
            (the final chunk is short), padded to a 2-byte boundary

Splitting into fixed 16000-byte chunks is what defeats a naive signature scan:
only the first chunk begins with the module header, and it is deflated.

Rendering
---------
ffmpeg here has no libopenmpt and cannot decode XM, so tools/xm_render.c does the
rendering; this script compiles it on demand. See the comment at the top of that
file for exactly which XM features are implemented and how that set was derived
from these six modules. If the renderer reports an effect it does not implement,
this script fails loudly rather than writing audio that is quietly wrong.

That check catches a MISSING effect. It cannot catch an effect implemented
wrongly, which is the failure mode a hand-written replayer actually has -- an
Amiga-table octave error made two of these six tracks play two octaves sharp and
the effect census had nothing to say about it. `tools/xm_oracle.py` is the check
for that: it scores xm_render.c against libopenmpt, the replayer MilkyTracker and
VLC use. Run it after touching xm_render.c.

The export is 1:1
-----------------
This is a rip, not a production. It renders the order list ONCE, adds no fade,
and invents no ending -- the module is the master and looping is the player's
job, so the loop point goes in the manifest instead of being baked in. It used
to render two passes and fade out over four seconds, which is a fabricated
ending for music written to loop forever, and that was wrong: an export that
makes a musical decision has destroyed the information needed to make a
different one later. `tools/extract_cdaudio.py` is the standard here -- it
copies sectors and decides nothing.

The ONE unavoidable edit is a scale factor. The mix routinely sums past unity
(peaks reach 2.938) and FLAC is integer PCM, so something has to bring it in
range. It is kept as honest as an edit can be: ONE factor for all six tracks,
exactly 1/peak of the loudest, recorded in the manifest so it can be undone
exactly. It is uniform, so it changes no relationship between the tracks.

Levels
------
That shared factor is why the loudness the composer wrote survives. XM has no
module-level master volume -- the mix level IS the composed level -- so scaling
each track to its own peak would silently flatten six deliberately unequal
tracks. --per-track-gain restores that older behaviour for a consumer that
really wants it; --headroom-db backs the whole set off full scale. Neither is
the default, because both are opinions.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import zlib

XM_SIGNATURE = b"Extended Module: "
CHUNK_ALIGN = 2
MANIFEST_NAME = "xm.manifest.json"
RENDERER_SRC = "xm_render.c"

# ROM magic words, keyed by the byte order they imply.
Z64_MAGIC = bytes((0x80, 0x37, 0x12, 0x40))   # big-endian, native
V64_MAGIC = bytes((0x37, 0x80, 0x40, 0x12))   # byte-swapped within 16-bit words
N64_MAGIC = bytes((0x40, 0x12, 0x37, 0x80))   # little-endian 32-bit words


class Fail(Exception):
    """A user-facing error: bad input, missing tool. Reported without a traceback."""


def be32(data, off):
    return int.from_bytes(data[off:off+4], "big")


def normalise_rom(data, path):
    """Return the ROM in native big-endian order, whatever container it arrived in.

    .v64 and .n64 dumps are the same bytes in a different order; converting is
    cheaper than telling the builder to go and find a different dump.
    """
    head = bytes(data[:4])
    if head == Z64_MAGIC:
        return data
    if head == V64_MAGIC:
        out = bytearray(data)
        out[0::2], out[1::2] = data[1::2], data[0::2]
        return bytes(out)
    if head == N64_MAGIC:
        out = bytearray(len(data))
        out[0::4] = data[3::4]
        out[1::4] = data[2::4]
        out[2::4] = data[1::4]
        out[3::4] = data[0::4]
        return bytes(out)
    raise Fail("%s does not start with an N64 ROM magic word (got %s).\n"
               "Expected a .z64/.v64/.n64 dump of Top Gear Rally."
               % (path, head.hex()))


def rom_title(rom):
    """The 20-byte internal name at 0x20 of the ROM header."""
    return rom[0x20:0x34].decode("latin-1").rstrip(" \0").strip()


def unpack_container(rom, off):
    """Unpack one chunked-zlib container at `off`. Returns (payload, consumed)."""
    total = be32(rom, off)
    usize = be32(rom, off + 4)
    if usize == 0 or usize > 8 << 20:
        raise ValueError("implausible uncompressed size")
    out = bytearray()
    p = off + 8
    while len(out) < usize:
        if p + 4 > len(rom):
            raise ValueError("ran off the end of the ROM")
        csize = be32(rom, p)
        p += 4
        if csize == 0 or p + csize > len(rom):
            raise ValueError("implausible chunk size")
        out += zlib.decompress(rom[p:p+csize])
        p += csize
        p = (p + CHUNK_ALIGN - 1) & ~(CHUNK_ALIGN - 1)
    if len(out) != usize:
        raise ValueError("chunk stream overran the declared size")
    return bytes(out), (p - off, total)


def find_modules(rom):
    """Locate every XM module in the ROM. Returns [(offset, payload), ...].

    A container is only recognised if it unpacks cleanly AND the payload starts
    with the XM signature, so this cannot mistake a texture or a level for music.
    """
    found = []
    for match in re.finditer(b"\x78\xda", rom):
        off = match.start() - 12
        if off < 0:
            continue
        try:
            payload, (consumed, total) = unpack_container(rom, off)
        except (ValueError, zlib.error):
            continue
        if not payload.startswith(XM_SIGNATURE):
            continue
        # The declared total should agree with what we actually consumed; if it
        # does not, we have mis-parsed and should not trust the payload.
        if consumed != total:
            continue
        found.append((off, payload))
    return found


def build_renderer(tools_dir, workdir, quiet):
    """Compile xm_render.c if needed. Returns the path to the executable."""
    src = os.path.join(tools_dir, RENDERER_SRC)
    if not os.path.exists(src):
        raise Fail("renderer source missing: %s" % src)
    exe = os.path.join(workdir, "xm_render")
    if (os.path.exists(exe)
            and os.path.getmtime(exe) >= os.path.getmtime(src)):
        return exe

    cc = os.environ.get("CC") or shutil.which("cc") or shutil.which("clang") \
        or shutil.which("gcc")
    if not cc:
        raise Fail("no C compiler found (set $CC, or install cc/clang/gcc).\n"
                   "tools/xm_render.c has to be compiled to render the modules.")
    cmd = [cc, "-std=c99", "-O2", "-o", exe, src, "-lm"]
    if not quiet:
        print("compiling %s" % RENDERER_SRC)
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise Fail("failed to compile the renderer:\n  %s\n%s"
                   % (" ".join(cmd), proc.stderr.strip()))
    return exe


def have_ffmpeg():
    try:
        subprocess.run(["ffmpeg", "-version"], stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, check=True)
    except (OSError, subprocess.CalledProcessError):
        return False
    return True


def run_renderer(exe, xm_path, args, raw_path=None, gain=None):
    """Render (or, with raw_path None, only measure). Returns the renderer's JSON."""
    cmd = [exe, "--rate", str(args.rate), "--passes", str(args.passes),
           "--fade-ms", str(args.fade_ms)]
    if gain is not None:
        cmd += ["--gain", "%.9f" % gain]
    else:
        cmd += ["--headroom-db", str(args.headroom_db)]
    cmd += ([xm_path, raw_path] if raw_path else ["--measure", xm_path])
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise Fail("renderer failed on %s:\n%s"
                   % (os.path.basename(xm_path), proc.stderr.strip()))
    try:
        info = json.loads(proc.stdout)
    except ValueError:
        raise Fail("renderer produced unreadable output for %s:\n%s"
                   % (os.path.basename(xm_path), proc.stdout[:400]))
    if info.get("unhandled_effects"):
        raise Fail("%s uses XM effects the renderer does not implement: %s\n"
                   "The audio would be wrong, so nothing was written. Extend "
                   "tools/xm_render.c before trusting this track."
                   % (os.path.basename(xm_path), info["unhandled_effects"]))
    return info


def shared_gain(exe, xm_paths, args, say):
    """One gain for the whole soundtrack, from the loudest module's peak.

    Scaling each module to its own peak would equalise six tracks the composer
    did not write as equally loud -- XM carries no module-level master volume,
    so the mix level IS the composed level and the only honest thing to do is
    scale them all by the same number. --per-track-gain opts out.
    """
    peaks = {}
    for p in xm_paths:
        peaks[p] = run_renderer(exe, p, args)["raw_peak"]
    top = max(peaks.values()) if peaks else 0.0
    if top <= 0.0:
        return 1.0, peaks
    g = 10.0 ** (args.headroom_db / 20.0) / top
    say("shared gain %.6f (loudest module peaks at %.3f; spread %.3f..%.3f)"
        % (g, top, min(peaks.values()), top))
    return g, peaks


def encode_flac(raw_path, dst, rate, level):
    tmp = dst + ".part"
    # -f flac is explicit because the temporary name ends in .part, and ffmpeg
    # otherwise picks the muxer from the extension and fails.
    cmd = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
           "-f", "s16le", "-ar", str(rate), "-ac", "2", "-i", raw_path,
           "-c:a", "flac", "-compression_level", str(level), "-f", "flac", tmp]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        if os.path.exists(tmp):
            os.remove(tmp)
        raise Fail("ffmpeg failed encoding %s:\n%s"
                   % (os.path.basename(dst), proc.stderr.strip()))
    os.replace(tmp, dst)


def sha256_file(path):
    """Hash the rendered PCM, so a re-run can be shown to have changed nothing.

    tools/extract_cdaudio.py records the same thing for the disc tracks; a rip
    that cannot be checked is not much of a rip.
    """
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_manifest(outdir):
    try:
        with open(os.path.join(outdir, MANIFEST_NAME), "r") as fh:
            return json.load(fh)
    except (OSError, ValueError):
        return None


def settings_fingerprint(rom_sha, args):
    h = hashlib.sha256()
    h.update(rom_sha.encode())
    h.update(b"|%d|%d|%d|%d" % (args.rate, args.passes, args.fade_ms,
                                args.compression_level))
    h.update(b"|%.4f|%d" % (args.headroom_db, args.per_track_gain))
    return h.hexdigest()


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Extract and render the Top Gear Rally XM soundtrack to FLAC.",
        epilog="Requires ffmpeg and a C compiler. Output is gitignored build data.")
    ap.add_argument("rom", help="path to the Top Gear Rally ROM (.z64/.v64/.n64)")
    ap.add_argument("outdir", help="directory to write the FLAC files into")
    ap.add_argument("-f", "--force", action="store_true",
                    help="re-render even if the output is already up to date")
    ap.add_argument("-q", "--quiet", action="store_true", help="only report errors")
    ap.add_argument("--rate", type=int, default=44100, help="sample rate (44100)")
    ap.add_argument("--passes", type=int, default=1,
                    help="times to play the order list (1 -- a faithful rip; "
                         "raise it only to make a standalone listening copy)")
    ap.add_argument("--fade-ms", type=int, default=0,
                    help="fade-out length in ms (0 -- the module has no ending, "
                         "and inventing one destroys the loop)")
    ap.add_argument("--keep-xm", action="store_true",
                    help="also write the unpacked .xm modules next to the audio")
    ap.add_argument("--compression-level", type=int, default=8,
                    choices=range(0, 13), metavar="0-12",
                    help="flac compression effort (default 8)")
    ap.add_argument("--headroom-db", type=float, default=0.0,
                    help="peak the loudest track lands on, in dBFS (0.0, i.e. "
                         "scale by exactly 1/peak and nothing more)")
    ap.add_argument("--per-track-gain", action="store_true",
                    help="scale each track to its own peak instead of scaling "
                         "the whole set by one gain; this discards the relative "
                         "loudness the modules were written with")
    args = ap.parse_args(argv)

    def say(msg):
        if not args.quiet:
            print(msg)

    if not os.path.exists(args.rom):
        raise Fail("no such file: %s\n"
                   "Supply your own ROM; see README.md \"Getting the game data\"."
                   % args.rom)
    if os.path.isdir(args.rom):
        raise Fail("%s is a directory; pass the ROM file" % args.rom)

    with open(args.rom, "rb") as fh:
        raw_rom = fh.read()
    if len(raw_rom) < 0x1000:
        raise Fail("%s is only %d bytes; that is not a ROM"
                   % (args.rom, len(raw_rom)))
    rom = normalise_rom(raw_rom, args.rom)
    rom_sha = hashlib.sha256(rom).hexdigest()
    say("rom: %s (%.1f MB, \"%s\")"
        % (os.path.basename(args.rom), len(rom) / 1e6, rom_title(rom)))

    if not have_ffmpeg():
        raise Fail("ffmpeg not found on PATH; it is required as the FLAC encoder")

    os.makedirs(args.outdir, exist_ok=True)
    fingerprint = settings_fingerprint(rom_sha, args)
    prev = load_manifest(args.outdir)
    fresh = (not args.force and prev is not None
             and prev.get("fingerprint") == fingerprint)

    modules = find_modules(rom)
    if not modules:
        raise Fail("no XM modules found in %s.\n"
                   "Either this is not Top Gear Rally, or it is a revision whose "
                   "music is packed differently." % args.rom)
    say("found %d XM module(s)" % len(modules))

    workdir = os.path.join(args.outdir, ".work")
    os.makedirs(workdir, exist_ok=True)
    exe = build_renderer(os.path.dirname(os.path.abspath(__file__)), workdir,
                         args.quiet)

    # Unpack every module up front: the shared gain is a property of the whole
    # set, so it cannot be decided one track at a time.
    xm_dir = args.outdir if args.keep_xm else workdir
    xm_paths = {}
    for off, payload in modules:
        xm_paths[off] = os.path.join(xm_dir, "xm_%06X.xm" % off)
        with open(xm_paths[off], "wb") as fh:
            fh.write(payload)

    todo = [off for off, _ in modules
            if not (fresh
                    and os.path.exists(os.path.join(args.outdir, "xm_%06X.flac" % off))
                    and os.path.getsize(os.path.join(args.outdir, "xm_%06X.flac" % off)) > 0
                    and any(e.get("file") == "xm_%06X.flac" % off
                            for e in (prev or {}).get("tracks", [])))]

    gain = None
    if todo and not args.per_track_gain:
        gain, _peaks = shared_gain(exe, [xm_paths[o] for o, _ in modules], args, say)

    entries = []
    done = skipped = 0
    for off, payload in modules:
        name = "xm_%06X.flac" % off
        dst = os.path.join(args.outdir, name)

        if off not in todo:
            entry = next((e for e in prev.get("tracks", [])
                          if e.get("file") == name), None)
            if entry is not None:
                entries.append(entry)
                skipped += 1
                continue

        xm_path = xm_paths[off]
        raw_path = os.path.join(workdir, "xm_%06X.raw" % off)
        try:
            info = run_renderer(exe, xm_path, args, raw_path, gain)
            pcm_sha = sha256_file(raw_path)
            encode_flac(raw_path, dst, args.rate, args.compression_level)
        finally:
            if os.path.exists(raw_path):
                os.remove(raw_path)

        secs = info["seconds"]
        say("  %s  %2d:%05.2f  %2dch %-6s  %6.1f MB  %s"
            % (name, int(secs // 60), secs % 60, info["channels"],
               info["frequency"], os.path.getsize(dst) / 1e6,
               info["name"] or "-"))
        entry = {"file": name, "rom_offset": "0x%06X" % off,
                 "module_name": info["name"], "channels": info["channels"],
                 "patterns": info["patterns"], "instruments": info["instruments"],
                 "frequency_table": info["frequency"],
                 "order_length": info["order_length"], "restart": info["restart"],
                 "seconds": round(secs, 3), "frames": info["frames"],
                 "loop_start_frame": info["loop_start_frame"],
                 "restart_frame": info["restart_frame"],
                 "raw_peak": info["raw_peak"], "gain": info["gain"],
                 "xm_sha256": hashlib.sha256(payload).hexdigest(),
                 "pcm_sha256": pcm_sha}
        entries.append(entry)
        done += 1

    if not args.keep_xm:
        for path in xm_paths.values():
            if os.path.exists(path):
                os.remove(path)

    with open(os.path.join(args.outdir, MANIFEST_NAME), "w") as fh:
        json.dump({"fingerprint": fingerprint, "rom_sha256": rom_sha,
                   "rom_title": rom_title(rom), "sample_rate": args.rate,
                   "channels": 2, "passes": args.passes,
                   "fade_ms": args.fade_ms, "headroom_db": args.headroom_db,
                   "gain_mode": "per-track" if args.per_track_gain else "shared",
                   "shared_gain": gain, "tracks": entries},
                  fh, indent=2, sort_keys=True)
        fh.write("\n")

    say("%d rendered, %d already up to date -> %s" % (done, skipped, args.outdir))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Fail as exc:
        sys.stderr.write("extract_xm: %s\n" % exc)
        sys.exit(2)
    except KeyboardInterrupt:
        sys.exit(130)
