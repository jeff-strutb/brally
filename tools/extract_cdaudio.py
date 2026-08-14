"""Extract the Boss Rally soundtrack from a retail disc image into lossless FLAC.

The retail PC game streams its music as Redbook CD audio: tracks 2..13 of the game
disc. This reads the image the builder supplies and writes one FLAC per music track.

    python tools/extract_cdaudio.py reference/brally/BossRally.cue build/audio/music

Nothing here is committed and nothing is shipped -- see "Asset policy" in README.md.
The output is regenerated on each machine from that machine's own disc image.

Source format
-------------
A .cue + .BIN pair from a mixed-mode disc. Track 1 is MODE1/2352 (the game data
track); tracks 2..n are AUDIO. Every sector in the image is 2352 bytes. For an AUDIO
track those 2352 bytes are *entirely* payload -- raw 16-bit stereo little-endian PCM
at 44100 Hz, 588 frames per sector, no sync header and no subchannel data. So an
audio track is extracted by copying bytes, with no decoding step at all.

INDEX times are MM:SS:FF at 75 frames/second and are absolute positions within the
image (track 1 starts at 00:00:00, so there is no 150-sector pregap offset to undo).
A track runs from its own INDEX 01 to the next track's INDEX 01, and the last track
runs to the end of the image.

Requires ffmpeg on PATH, only as the FLAC encoder -- it is fed finished PCM.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

SECTOR_BYTES = 2352
SAMPLE_RATE = 44100
CHANNELS = 2
FRAMES_PER_SECOND = 75
MANIFEST_NAME = "cdaudio.manifest.json"

# Read the image in large blocks; this is pure I/O and the tracks are ~40 MB each.
COPY_BLOCK = 4 << 20


class Fail(Exception):
    """A user-facing error: bad input, missing tool. Reported without a traceback."""


def msf_to_sector(text):
    """MM:SS:FF -> absolute sector index. 75 frames per second."""
    m = re.fullmatch(r"(\d+):(\d+):(\d+)", text)
    if not m:
        raise Fail("malformed INDEX time %r (expected MM:SS:FF)" % text)
    mins, secs, frames = (int(g) for g in m.groups())
    if secs >= 60 or frames >= FRAMES_PER_SECOND:
        raise Fail("out-of-range INDEX time %r" % text)
    return (mins * 60 + secs) * FRAMES_PER_SECOND + frames


def parse_cue(cue_path):
    """Return (bin_path, [(number, mode, start_sector), ...]) from a .cue sheet.

    Only the subset a single-FILE mixed-mode game disc uses is understood: one FILE
    line, TRACK lines, and INDEX 01. INDEX 00 (pregap) is deliberately ignored --
    audio inside a pregap belongs to the *previous* track, and treating INDEX 00 as
    the start would prepend the tail of the preceding track to each file.
    """
    bin_name = None
    tracks = []
    pending = None
    with open(cue_path, "r", encoding="latin-1") as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.strip()
            m = re.match(r'FILE\s+"([^"]+)"', line, re.I)
            if m:
                if bin_name is not None:
                    raise Fail("%s:%d: multi-FILE cue sheets are not supported"
                               % (cue_path, lineno))
                bin_name = m.group(1)
                continue
            m = re.match(r"TRACK\s+(\d+)\s+(\S+)", line, re.I)
            if m:
                pending = (int(m.group(1)), m.group(2).upper())
                continue
            m = re.match(r"INDEX\s+01\s+(\S+)", line, re.I)
            if m and pending is not None:
                tracks.append((pending[0], pending[1], msf_to_sector(m.group(1))))
                pending = None
    if bin_name is None:
        raise Fail("%s: no FILE line found -- is this really a cue sheet?" % cue_path)
    if not tracks:
        raise Fail("%s: no TRACK/INDEX 01 pairs found" % cue_path)
    bin_path = os.path.join(os.path.dirname(os.path.abspath(cue_path)), bin_name)
    return bin_path, tracks


def resolve_source(path):
    """Accept either the .cue or the .BIN and return (bin_path, tracks)."""
    if not os.path.exists(path):
        raise Fail("no such file: %s\n"
                   "Supply your own retail disc image; see README.md "
                   "\"Getting the game data\"." % path)
    if os.path.isdir(path):
        raise Fail("%s is a directory; pass the .cue file" % path)

    root, ext = os.path.splitext(path)
    if ext.lower() != ".cue":
        # Given the .BIN -- the cue sheet is what carries the track table, so it is
        # required regardless. Look for it beside the image.
        for cand in (root + ".cue", root + ".CUE"):
            if os.path.exists(cand):
                path = cand
                break
        else:
            raise Fail("%s has no matching .cue sheet beside it.\n"
                       "The cue sheet holds the track table; the .BIN alone does not "
                       "say where the audio tracks start." % path)

    bin_path, tracks = parse_cue(path)
    if not os.path.exists(bin_path):
        raise Fail("cue sheet %s references %s, which does not exist"
                   % (path, os.path.basename(bin_path)))
    size = os.path.getsize(bin_path)
    if size % SECTOR_BYTES:
        raise Fail("%s is %d bytes, not a whole number of %d-byte sectors.\n"
                   "This does not look like a raw 2352-byte-sector image."
                   % (bin_path, size, SECTOR_BYTES))
    return bin_path, tracks


def track_spans(bin_path, tracks):
    """Annotate each track with (start_sector, sector_count), in image order."""
    total = os.path.getsize(bin_path) // SECTOR_BYTES
    out = []
    for i, (num, mode, start) in enumerate(tracks):
        end = tracks[i + 1][2] if i + 1 < len(tracks) else total
        if start > end:
            raise Fail("track %d starts at sector %d, after its end %d "
                       "-- cue sheet track order is wrong" % (num, start, end))
        if end > total:
            raise Fail("track %d runs to sector %d but the image only has %d.\n"
                       "The image is truncated or does not match this cue sheet."
                       % (num, end, total))
        out.append((num, mode, start, end - start))
    return out


def have_ffmpeg():
    try:
        subprocess.run(["ffmpeg", "-version"], stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, check=True)
    except (OSError, subprocess.CalledProcessError):
        return False
    return True


def source_fingerprint(bin_path, spans):
    """Identify the source cheaply: size plus a hash of the audio track table.

    Deliberately NOT a hash of the whole 600 MB image -- that would cost more than
    the extraction it is meant to let us skip.
    """
    h = hashlib.sha256()
    h.update(b"%d\n" % os.path.getsize(bin_path))
    for num, mode, start, count in spans:
        h.update(b"%d %s %d %d\n" % (num, mode.encode(), start, count))
    return h.hexdigest()


def load_manifest(outdir):
    try:
        with open(os.path.join(outdir, MANIFEST_NAME), "r") as fh:
            return json.load(fh)
    except (OSError, ValueError):
        return None


def encode_track(bin_path, start, count, dst, level):
    """Copy one track's sectors out of the image and through ffmpeg into FLAC.

    Written to a .part file and renamed on success, so an interrupted run never
    leaves a short file that a later run would mistake for finished work.
    """
    tmp = dst + ".part"
    nbytes = count * SECTOR_BYTES
    # -f flac is explicit because the temporary name ends in .part, and ffmpeg
    # otherwise picks the muxer from the extension and fails.
    proc = subprocess.Popen(
        ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
         "-f", "s16le", "-ar", str(SAMPLE_RATE), "-ac", str(CHANNELS), "-i", "pipe:0",
         "-c:a", "flac", "-compression_level", str(level), "-f", "flac", tmp],
        stdin=subprocess.PIPE)
    digest = hashlib.sha256()
    try:
        with open(bin_path, "rb") as fh:
            fh.seek(start * SECTOR_BYTES)
            left = nbytes
            while left:
                block = fh.read(min(COPY_BLOCK, left))
                if not block:
                    raise Fail("unexpected end of image while reading sector "
                               "%d" % (start + (nbytes - left) // SECTOR_BYTES))
                digest.update(block)
                proc.stdin.write(block)
                left -= len(block)
        proc.stdin.close()
    except BrokenPipeError:
        proc.stdin.close()
        proc.wait()
        raise Fail("ffmpeg exited early while encoding %s" % os.path.basename(dst))
    except BaseException:
        proc.kill()
        proc.wait()
        if os.path.exists(tmp):
            os.remove(tmp)
        raise
    if proc.wait() != 0:
        if os.path.exists(tmp):
            os.remove(tmp)
        raise Fail("ffmpeg failed encoding %s" % os.path.basename(dst))
    os.replace(tmp, dst)
    return nbytes, digest.hexdigest()


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Extract Boss Rally CD audio tracks to FLAC.",
        epilog="Requires ffmpeg on PATH. Output is gitignored build data.")
    ap.add_argument("source", help="path to BossRally.cue (or the .BIN beside it)")
    ap.add_argument("outdir", help="directory to write track NN.flac into")
    ap.add_argument("-f", "--force", action="store_true",
                    help="re-encode even if the output is already up to date")
    ap.add_argument("-q", "--quiet", action="store_true", help="only report errors")
    ap.add_argument("--compression-level", type=int, default=8,
                    choices=range(0, 13), metavar="0-12",
                    help="flac compression effort (default 8)")
    args = ap.parse_args(argv)

    def say(msg):
        if not args.quiet:
            print(msg)

    bin_path, tracks = resolve_source(args.source)
    spans = track_spans(bin_path, tracks)
    audio = [s for s in spans if s[1] == "AUDIO"]
    if not audio:
        raise Fail("%s lists no AUDIO tracks -- this is a data-only image" % args.source)
    if not have_ffmpeg():
        raise Fail("ffmpeg not found on PATH; it is required as the FLAC encoder")

    os.makedirs(args.outdir, exist_ok=True)
    fingerprint = source_fingerprint(bin_path, spans)
    prev = load_manifest(args.outdir)
    fresh = (not args.force and prev is not None
             and prev.get("source_fingerprint") == fingerprint)

    say("source: %s (%d sectors, %d audio tracks)"
        % (os.path.basename(bin_path), os.path.getsize(bin_path) // SECTOR_BYTES,
           len(audio)))

    entries = []
    done = skipped = 0
    for num, _mode, start, count in audio:
        name = "track%02d.flac" % num
        dst = os.path.join(args.outdir, name)
        seconds = count / float(FRAMES_PER_SECOND)
        if fresh and os.path.exists(dst) and os.path.getsize(dst) > 0:
            entry = next((e for e in prev.get("tracks", []) if e.get("file") == name),
                         None)
            if entry is not None:
                entries.append(entry)
                skipped += 1
                continue
        _nbytes, pcm_sha = encode_track(bin_path, start, count, dst,
                                        args.compression_level)
        say("  %s  %2d:%05.2f  %8.1f MB"
            % (name, int(seconds // 60), seconds % 60,
               os.path.getsize(dst) / 1e6))
        entries.append({"file": name, "track": num, "start_sector": start,
                        "sectors": count, "seconds": round(seconds, 3),
                        "pcm_sha256": pcm_sha})
        done += 1

    with open(os.path.join(args.outdir, MANIFEST_NAME), "w") as fh:
        json.dump({"source_fingerprint": fingerprint,
                   "sample_rate": SAMPLE_RATE, "channels": CHANNELS,
                   "tracks": entries}, fh, indent=2, sort_keys=True)
        fh.write("\n")

    say("%d encoded, %d already up to date -> %s" % (done, skipped, args.outdir))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Fail as exc:
        sys.stderr.write("extract_cdaudio: %s\n" % exc)
        sys.exit(2)
    except KeyboardInterrupt:
        sys.exit(130)
