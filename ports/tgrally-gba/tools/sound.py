"""sound.py RAM ROM SND F0 F1 OUT.c -- the race's sound for the GBA.

  RAM  a TGR_RAMDUMP taken in the race (ports/tgrally): which module is
       playing (its order list), the music level and the note-rate table
  ROM  the cartridge: the module itself (tools/tgrally/extract_xm.py finds it)
  SND  a TGR_SNDDUMP of the same race: the six effect voices, once a retrace
  F0..F1  the retraces the GBA replays (as convert.py's)

The music is loaded as the game's BrModLoad loads it (order list, packed
patterns, each instrument's first sample decoded from its deltas with a tail
after it: silence, or the loop again) and played on the GBA by the game's
own player (gba/sound.s).  The effects are the game's voices as it drove
them: per retrace each voice's sample, its rate and its two levels, and
where a sample (re)starts.

The GBA mixes at RATE: a note's step there is the game's (32.32 at its
21998 Hz) scaled to RATE, in Q12; an effect's the same from the game's
10999 Hz (its effects take one sample a stereo pair)."""
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../tools/tgrally'))
import extract_xm  # noqa: E402

RATE = 13379                       # the GBA's output: 224 samples a retrace
N64_RATE = 21998
TAIL = 0x4B0                       # BrModLoad's tail
SFX_TAIL = 0x400
PORTA_K = 0xEE9FCFF0B5             # BrModTick: period = K / rate


def le16(b, o):
    return b[o] | b[o + 1] << 8


def le32(b, o):
    return struct.unpack_from('<I', b, o)[0]


def mod_load(xm):
    """BrModLoad: -> (order, restart, speed, channels, patterns, samples)"""
    n, restart, ch, npat, ninst, speed = xm[0x40], xm[0x42], xm[0x44], xm[0x46], xm[0x48], xm[0x4c]
    order = list(xm[0x50:0x50 + n])
    pat = 0x3c + le32(xm, 0x3c)
    pats = []
    for i in range(npat):
        ln = le16(xm, pat + 7) + le32(xm, pat)
        pats.append(bytes(xm[pat:pat + ln]))
        pat += ln
    p, smps = pat, []
    for i in range(ninst):
        q = p + le32(xm, p)
        if xm[p + 0x1b] > 0:
            ln, loop_len, vol, typ, rel = le32(xm, q), le32(xm, q + 8), xm[q + 0xc], xm[q + 0xe], struct.unpack_from('b', xm, q + 0x10)[0]
            acc, data = 0, bytearray()
            for k in range(ln):
                acc = (acc + xm[q + 0x28 + k]) & 0xFF
                data.append(acc)
            if typ == 1:
                for k in range(TAIL):
                    data.append(data[len(data) - loop_len])
            else:
                data += bytes(TAIL)
            smps.append((bytes(data), ln, loop_len, vol, typ, rel))
            p = q + ln + 0x28
        else:
            smps.append(None)
            p = q
    return order, restart, speed, ch, pats, smps


def module_c(prefix, order, restart, speed, nch, pats, smps, level, init='0'):
    """a module as gba/sound.s plays it: its samples, patterns and order list, and
    the Song naming them (init: the player's state to start from, or 0 for
    BrModReset's)"""
    o = []
    for i, s in enumerate(smps):
        if s:
            o.append('static ' + c_bytes('%s_data%d' % (prefix, i), s[0]))
    o.append('static const SndSample %s_smp[%d] = {' % (prefix, len(smps)))
    for i, s in enumerate(smps):
        o.append('    { %s_data%d, %d, %d, %d, %d, %d },' % (prefix, i, s[1], s[2], s[3], 1 if s[4] == 1 else 0, s[5]) if s else '    { 0, 0, 0, 0, 0, 0 },')
    o.append('};')
    for i, p in enumerate(pats):
        o.append('static ' + c_bytes('%s_pat%d' % (prefix, i), p, False))
    o.append('static const uint8_t *const %s_pats[%d] = { %s };' % (prefix, len(pats), ', '.join('%s_pat%d' % (prefix, i) for i in range(len(pats)))))
    o.append('static const uint8_t %s_order[%d] = { %s };' % (prefix, len(order), ', '.join(map(str, order))))
    o.append('/* samples, patterns, order list; its length, restart, speed, channels; the game\'s music level times its fade */')
    o.append('const Song %s_song = { %s_smp, %s_pats, %s_order, %d, %d, %d, %d, %d, %s };' % (
        prefix, prefix, prefix, prefix, len(order), restart, speed, nch, level, init))
    return o


def read_snd(path):
    d = open(path, 'rb').read()
    i, frames, samples, music, last = 0, {}, {}, {}, None
    while i < len(d):
        t = d[i:i + 1]
        i += 1
        if t == b'S':
            st, ln = struct.unpack_from('<II', d, i)
            i += 8
            samples[st] = d[i:i + ln]
            i += ln
        elif t == b'F':
            h = struct.unpack_from('<6I', d, i)
            i += 24
            frames[h[0]] = (h, [struct.unpack_from('<8I', d, i + k * 32) for k in range(6)])
            i += 192
            last = h[0]
        elif t == b'M':
            music[last] = d[i:i + 564]
            i += 564
        else:
            sys.exit('sound.py: bad record %r' % t)
    return frames, samples, music


def c_bytes(name, b, sample=True):
    """a sample offset by 128 (gba/sound.s reads it unsigned), or plain bytes"""
    out = ['const uint8_t %s[%d] = {' % (name, len(b))]
    for k in range(0, len(b), 32):
        out.append(','.join(str(x ^ 0x80 if sample else x) for x in b[k:k + 32]) + ',')
    out.append('};')
    return '\n'.join(out)


def main():
    ram_path, rom_path, snd_path, f0, f1, out_path = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
    d = open(ram_path, 'rb').read()
    ram = d[64:]

    def u32(a):
        return struct.unpack_from('<I', ram, a & 0x7FFFFF)[0]

    def u8(a):
        return ram[a & 0x7FFFFF]

    st = 0x80378FA0                                     # BrModState
    olen, oaddr = u8(st + 0x11), u32(st + 8)
    playing = bytes(ram[oaddr & 0x7FFFFF:(oaddr & 0x7FFFFF) + olen])
    rom = open(rom_path, 'rb').read()
    xm = None
    for off, payload in extract_xm.find_modules(rom):
        if payload[0x40] == olen and payload[0x50:0x50 + olen] == playing:
            xm, xm_off = payload, off
    if xm is None:
        sys.exit('sound.py: the playing module is not in the ROM')
    order, restart, speed, nch, pats, smps = mod_load(xm)
    level = u8(0x802A49C4) * u8(0x802A49C8)            # music level times fade
    rates = [struct.unpack_from('<Q', ram, (0x80379568 & 0x7FFFFF) + k * 8)[0] for k in range(120)]
    scale = N64_RATE / RATE
    q12 = [min(0xFFFFFFFF, int(round(r * scale / (1 << 20)))) for r in rates]

    frames, sdata, music = read_snd(snd_path)
    sfx_keys = sorted(sdata)
    sfx_index = {k: i for i, k in enumerate(sfx_keys)}
    sfx_meta = {}                                       # start -> (len, loop): the widest seen
    for fr, (h, vs) in frames.items():
        for v in vs:
            if v[5] and v[5] in sdata:
                ln, lp = sfx_meta.get(v[5], (0, 0))
                sfx_meta[v[5]] = (max(ln, v[6]), max(lp, v[7]))
    trace, prev = [], [None] * 6
    sfx_scale = (N64_RATE / 2) / RATE
    last = None
    for fr in range(f0, f1 + 1):
        rec = frames.get(fr, last)
        last = rec
        row = []
        for k in range(6):
            if rec is None:
                row.append((0xFF, 0, 0, 0, 0, 0))
                prev[k] = None
                continue
            h, vs = rec
            pos, frac, rhi, rlo, base, start, ln, loop = vs[k]
            rate = rhi * 2 ** 32 + rlo
            if not rate or start not in sdata:
                row.append((0xFF, 0, 0, 0, 0, 0))
                prev[k] = None
                continue
            off = max(0, min(pos - start, sfx_meta[start][0] + SFX_TAIL - 1))
            p = prev[k]
            begin = p is None or p[0] != start or (off < p[1] and not loop)
            prev[k] = (start, off)
            lft = h[1] * (base >> 16) * h[2] >> 16       # the effects level and fade: hi is the left
            rgt = h[1] * (base & 0xFFFF) * h[2] >> 16
            r12 = min(0xFFFF, int(round(rate * sfx_scale / (1 << 20))))
            row.append((sfx_index[start], min(lft, 255), min(rgt, 255), 1 if begin else 0, r12, off if begin else 0))
        trace.append(row)

    # the player as the game had it at F0: where BrModLoad put each pattern and sample
    # (from the order list's address on) turns its pointers into offsets
    m = music[f0]
    base = struct.unpack_from('<I', m, 144 + 8)[0]
    a = (base + len(order) + 3) & ~3
    pat_at = []
    for p in pats:
        pat_at.append(a)
        a += len(p)
    a = (a + 3) & ~3
    smp_at = []
    for sm in smps:
        if sm:
            smp_at.append(a + 0x28)
            a = (a + 0x28 + len(sm[0]) + 3) & ~3
        else:
            smp_at.append(None)
    x0, x2, x4 = struct.unpack_from('<3H', m, 144)
    row, opos = struct.unpack_from('<I', m, 144 + 12)[0], m[144 + 0x10]
    cur = pat_at[order[opos]] if opos < len(order) else 0
    init = ['const ModInit g_mod_init = {', '    %d, %d, %d, %d, %d,' % (x0, x2, x4, opos, row - cur if cur and row >= cur else 0), '    {']
    for c in range(6):
        ch = m[c * 0x18:(c + 1) * 0x18]
        target = struct.unpack_from('<Q', ch, 0)[0]
        porta, slide = struct.unpack_from('<hh', ch, 8)
        t12 = int(round(target * scale / (1 << 20)))
        init.append('        { %d, %d, %d, %d, %d, %d, { %d, %d, %d }, %d, %d, 0, %d, %d, %d },' % (
            ch[0xC], ch[0xD], ch[0xE], ch[0xF], ch[0x10], ch[0x11], ch[0x12], ch[0x13], ch[0x14], ch[0x15], ch[0x16], porta, slide, t12))
    init.append('    },')
    init.append('    {')
    for c in range(6):
        v = m[164 + c * 0x18:164 + (c + 1) * 0x18]
        pos, frac = struct.unpack_from('<II', v, 0)
        rate = struct.unpack_from('<Q', v, 8)[0]
        basevol = struct.unpack_from('<i', v, 0x14)[0]
        sm = m[c * 0x18 + 0xD]
        if sm and smp_at[sm - 1] and rate and pos >= smp_at[sm - 1]:
            off = ((pos - smp_at[sm - 1]) << 12) | (frac >> 20)
            init.append('        { %d, %d, %d, %d },' % (sm, off, int(round(rate * scale / (1 << 20))), basevol))
        else:
            init.append('        { 0, 0, 0, %d },' % basevol)
    init.append('    },')
    init.append('};')

    o = ['/* the race\'s sound, from the game (tools/sound.py): module 0x%06X and the effect voices */' % xm_off,
         '#include "sound.h"', '']
    o += init
    o += module_c('g_race', order, restart, speed, nch, pats, smps, level, '&g_mod_init')
    o.append('const uint32_t g_note_rate[120] = { %s };' % ', '.join(map(str, q12)))
    o.append('const uint32_t g_porta_k = %d;          /* BrModTick\'s K in these units */' % int(round(PORTA_K * scale / (1 << 20))))
    for i, k in enumerate(sfx_keys):
        ln, lp = sfx_meta.get(k, (len(sdata[k]) - 0x800, 0))
        data = bytearray(sdata[k][:ln])
        for j in range(SFX_TAIL):
            data.append(data[len(data) - lp] if lp else 0)
        o.append(c_bytes('s_sfx_data%d' % i, data))
    o.append('const SndSample g_sfx_smp[%d] = {' % len(sfx_keys))
    for i, k in enumerate(sfx_keys):
        ln, lp = sfx_meta.get(k, (len(sdata[k]) - 0x800, 0))
        o.append('    { s_sfx_data%d, %d, %d, 0, %d, 0 },' % (i, ln, lp, 1 if lp else 0))
    o.append('};')
    o.append('const int g_sfx_frames = %d;' % len(trace))
    o.append('const SfxVoice g_sfx_trace[%d][6] = {' % len(trace))
    for row in trace:
        o.append('{' + ','.join('{%d,%d,%d,%d,%d,%d}' % v for v in row) + '},')
    o.append('};')
    open(out_path, 'w').write('\n'.join(o) + '\n')
    print('%s: module 0x%06X (%d channels, %d patterns, %d samples %d KB), %d effect samples %d KB, %d retraces' % (
        out_path, xm_off, nch, len(pats), sum(1 for s in smps if s), sum(len(s[0]) for s in smps if s) // 1024,
        len(sfx_keys), sum(len(v) for v in sdata.values()) // 1024, len(trace)))


if __name__ == '__main__':
    main()
