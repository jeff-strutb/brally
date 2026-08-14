"""Extract the N64 soundtrack from a Top Gear Rally ROM into lossless FLAC.

The N64 build stores its music as six FastTracker II .xm modules, zlib-packed
inside the ROM. This finds them, unpacks them, renders them to PCM and encodes
the result.

    python tools/extract_xm.py "reference/tgrally/Top Gear Rally (USA).z64" \
                               build/audio/music_n64

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


def render_module(exe, xm_path, raw_path, args):
    cmd = [exe, "--rate", str(args.rate), "--passes", str(args.passes),
           "--fade-ms", str(args.fade_ms), xm_path, raw_path]
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
    ap.add_argument("--passes", type=int, default=2,
                    help="times to play the order list before fading (2)")
    ap.add_argument("--fade-ms", type=int, default=4000,
                    help="fade-out length in ms (4000)")
    ap.add_argument("--keep-xm", action="store_true",
                    help="also write the unpacked .xm modules next to the audio")
    ap.add_argument("--compression-level", type=int, default=8,
                    choices=range(0, 13), metavar="0-12",
                    help="flac compression effort (default 8)")
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

    entries = []
    done = skipped = 0
    for off, payload in modules:
        name = "xm_%06X.flac" % off
        dst = os.path.join(args.outdir, name)

        if fresh and os.path.exists(dst) and os.path.getsize(dst) > 0:
            entry = next((e for e in prev.get("tracks", [])
                          if e.get("file") == name), None)
            if entry is not None:
                entries.append(entry)
                skipped += 1
                continue

        xm_path = os.path.join(args.keep_xm and args.outdir or workdir,
                               "xm_%06X.xm" % off)
        with open(xm_path, "wb") as fh:
            fh.write(payload)
        raw_path = os.path.join(workdir, "xm_%06X.raw" % off)
        try:
            info = render_module(exe, xm_path, raw_path, args)
            encode_flac(raw_path, dst, args.rate, args.compression_level)
        finally:
            if os.path.exists(raw_path):
                os.remove(raw_path)
            if not args.keep_xm and os.path.exists(xm_path):
                os.remove(xm_path)

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
                 "raw_peak": info["raw_peak"], "gain": info["gain"],
                 "xm_sha256": hashlib.sha256(payload).hexdigest()}
        entries.append(entry)
        done += 1

    with open(os.path.join(args.outdir, MANIFEST_NAME), "w") as fh:
        json.dump({"fingerprint": fingerprint, "rom_sha256": rom_sha,
                   "rom_title": rom_title(rom), "sample_rate": args.rate,
                   "channels": 2, "passes": args.passes,
                   "fade_ms": args.fade_ms, "tracks": entries},
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
