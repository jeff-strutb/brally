#!/usr/bin/env python3
"""Optional: rebuild the N64 soundtrack's modules with better copies of some
of their samples (package_app.sh --hq-samples DIR; off by default).

    upgrade_samples.py DIR moduledir

Top Gear Rally's modules borrow most of their samples from older tracker
music and the Amiga ST-XX sample disks. A few of those sources survive at a
higher rate, or 16-bit, than the copy in the ROM, which was downsampled (and
sometimes pitched and amplified) to fit the cartridge. hq_samples.json, beside
this script, lists them: which sample of which module, the source and where it
was published, and how the ROM's copy relates to it. DIR holds the builder's
own copies of those sources as WAV files, named as the list names them; none
are tracked in git (Asset policy, README.md).

Each listed sample is replaced in place in moduledir/xm_XXXXXX.xm and nothing
else in the module changes:
  data    the source, as 16-bit, scaled to the ROM copy's level (a least-squares
          fit of the ROM copy against the source read at the ROM's rate,
          leaving out the ROM copy's clipped peaks);
  pitch   relative note and finetune raised by 12*log2(rate) semitones, rate
          being how many source samples one ROM sample spans, so every note
          plays at the pitch it did;
  loop    the source's own loop points (the ROM's, times rate).
A WAV that is missing or is not the listed file (SHA-256) stops the build: a
partial upgrade must not look like a complete one.
"""
import hashlib
import json
import math
import os
import struct
import sys
import wave

HERE = os.path.dirname(os.path.abspath(__file__))


def read_wav(path):
    with wave.open(path, 'rb') as w:
        if w.getnchannels() != 1:
            sys.exit('upgrade_samples: %s is not mono' % path)
        width, frames = w.getsampwidth(), w.readframes(w.getnframes())
    if width == 1:
        return [(b - 128) * 256 for b in frames]
    if width == 2:
        return list(struct.unpack('<%dh' % (len(frames) // 2), frames))
    sys.exit('upgrade_samples: %s is %d-bit' % (path, width * 8))


def samples(m):
    """Every sample header in module m, in order: (header offset, data offset,
    data length in bytes)."""
    hs = struct.unpack_from('<I', m, 60)[0]
    npat, nins = struct.unpack_from('<HH', m, 70)
    q = 60 + hs
    for _ in range(npat):
        h, = struct.unpack_from('<I', m, q)
        ps, = struct.unpack_from('<H', m, q + 7)
        q += h + ps
    out = []
    for _ in range(nins):
        isz, = struct.unpack_from('<I', m, q)
        n, = struct.unpack_from('<H', m, q + 27)
        if not n:
            q += isz
            continue
        shs, = struct.unpack_from('<I', m, q + 29)
        q += isz
        hdrs = [q + j * shs for j in range(n)]
        q += n * shs
        for h in hdrs:
            ln, = struct.unpack_from('<I', m, h)
            out.append((h, q, ln))
            q += ln
    return out


def decode(m, h, d, ln):
    """A sample's data as 16-bit-scaled integers."""
    out, acc = [], 0
    if m[h + 14] & 16:
        for (v,) in struct.iter_unpack('<h', m[d:d + ln]):
            acc = (acc + v + 32768) % 65536 - 32768
            out.append(acc)
    else:
        for v in struct.unpack('%db' % ln, m[d:d + ln]):
            acc = (acc + v + 128) % 256 - 128
            out.append(acc * 256)
    return out


def level(rom, src, rate):
    """Gain that brings src to rom's level, fitted where rom did not clip."""
    num = den = 0.0
    for i, r in enumerate(rom):
        if abs(r) >= 120 * 256:
            continue
        y = src[min(int(i * rate), len(src) - 1)]
        num += y * r
        den += y * y
    return num / den if den else 1.0


def upgrade(m, entries, wavdir):
    m = bytearray(m)
    table = samples(m)
    # rewrite from the last sample back, so earlier offsets stay valid
    for e in sorted(entries, key=lambda e: -e['sample']):
        h, d, ln = table[e['sample']]
        name = m[h + 18:h + 40].split(b'\0')[0].decode('latin-1').rstrip()
        rom = decode(m, h, d, ln)
        if name != e['rom_name'] or len(rom) != e['rom_length']:
            sys.exit('upgrade_samples: %s sample %d is "%s" (%d), not "%s" (%d)'
                     % (e['module'], e['sample'], name, len(rom), e['rom_name'], e['rom_length']))
        path = os.path.join(wavdir, e['file'])
        if not os.path.isfile(path):
            sys.exit('upgrade_samples: missing %s' % path)
        if hashlib.sha256(open(path, 'rb').read()).hexdigest() != e['sha256']:
            sys.exit('upgrade_samples: %s is not the listed file' % path)
        src = read_wav(path)
        rate = e['rate'][0] / e['rate'][1]
        g = level(rom, src, rate)
        pcm = [max(-32768, min(32767, round(v * g))) for v in src]
        delta, prev = [], 0
        for v in pcm:
            delta.append((v - prev + 32768) % 65536 - 32768)
            prev = v
        ft, = struct.unpack_from('<b', m, h + 13)
        rel, = struct.unpack_from('<b', m, h + 16)
        pitch = rel * 128 + ft + round(12 * math.log2(rate) * 128)
        rel2 = (pitch + 64) // 128
        typ = m[h + 14] & 3
        if e['loop'] is None:
            typ, ls, ll = 0, 0, 0
        else:
            ls, ll = e['loop']
        struct.pack_into('<III', m, h, 2 * len(pcm), 2 * ls, 2 * ll)
        struct.pack_into('<bB', m, h + 13, pitch - rel2 * 128, typ | 16)
        struct.pack_into('<b', m, h + 16, rel2)
        m[d:d + ln] = struct.pack('<%dh' % len(delta), *delta)
        print('  %s #%d %-22s %6d -> %6d samples, 16-bit, level x%.3f'
              % (e['module'], e['sample'], name[:22], len(rom), len(pcm), g))
    return bytes(m)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    wavdir, moddir = sys.argv[1], sys.argv[2]
    spec = json.load(open(os.path.join(HERE, 'hq_samples.json')))['samples']
    by_module = {}
    for e in spec:
        by_module.setdefault(e['module'], []).append(e)
    for mod, entries in sorted(by_module.items()):
        path = os.path.join(moddir, mod + '.xm')
        data = upgrade(open(path, 'rb').read(), entries, wavdir)
        with open(path, 'wb') as f:
            f.write(data)
    print('hq samples: %d replaced in %d modules' % (len(spec), len(by_module)))


if __name__ == '__main__':
    main()
