#!/usr/bin/env python3
"""spusim.py SPULOG WAV [--from S] [--to S] -- the sound the SPU makes from a
run's voice writes and sound-RAM uploads (TGR_SPULOG=1: spu.log), rendered at
44.1 kHz stereo: ADPCM blocks decoded as the SPU does (filters, shift, the
loop flags), each voice stepped by its pitch, its two levels applied, mixed
and clipped.  For checking the port's sound against the native port's
(TGR_WAVDUMP)."""
import argparse
import struct
import wave

FILTERS = [(0, 0), (60, 0), (115, -52), (98, -55), (122, -60)]
RATE = 44100
TICKS = 46875.0                                  # the log's clock: N64 count / 1000 a second


class Voice:
    def __init__(self):
        self.on = False
        self.addr = 0
        self.repeat = 0
        self.pitch = 0x1000
        self.l = self.r = 0
        self.buf = []
        self.pos = 0.0
        self.s1 = self.s2 = 0
        self.flags = 0

    def key_on(self, addr):
        self.on = True
        self.addr = addr
        self.repeat = addr
        self.buf = []
        self.pos = 0.0
        self.s1 = self.s2 = 0

    def block(self, ram):
        b = ram[self.addr:self.addr + 16]
        sh, f = b[0] & 15, min(b[0] >> 4, 4)
        flags = b[1]
        if flags & 4:
            self.repeat = self.addr
        out = []
        for i in range(28):
            nib = (b[2 + i // 2] >> (4 * (i & 1))) & 15
            if nib & 8:
                nib -= 16
            s = (nib << 12) >> sh if sh <= 12 else 0
            s += (self.s1 * FILTERS[f][0] + self.s2 * FILTERS[f][1] + 32) >> 6
            s = max(-32768, min(32767, s))
            self.s2, self.s1 = self.s1, s
            out.append(s)
        if flags & 1:
            if flags & 2:
                self.addr = self.repeat
            else:
                self.addr = self.repeat
                self.on = False                  # (end without repeat: released)
        else:
            self.addr += 16
        return out

    def sample(self, ram):
        while self.pos >= len(self.buf):
            self.pos -= len(self.buf)
            self.buf = self.block(ram)
        s = self.buf[int(self.pos)]
        self.pos += self.pitch / 4096.0
        return s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log')
    ap.add_argument('wav')
    ap.add_argument('--from', dest='t0', type=float, default=0)
    ap.add_argument('--to', dest='t1', type=float, default=1e9)
    a = ap.parse_args()
    data = open(a.log, 'rb').read()
    evs = []
    i = 0
    while i + 20 <= len(data):
        t, opv, x, y, n = struct.unpack_from('<5I', data, i)
        i += 20
        payload = data[i:i + n]
        i += n
        evs.append((t / TICKS, opv & 255, opv >> 8, x, y, payload))
    ram = bytearray(0x80000)
    voices = [Voice() for _ in range(24)]
    out = bytearray()
    ei = 0
    t = 0.0
    tend = min(a.t1, evs[-1][0] if evs else 0)
    while t < tend:
        while ei < len(evs) and evs[ei][0] <= t:
            _, op, v, x, y, p = evs[ei]
            ei += 1
            if op == 1:
                ram[x:x + len(p)] = p
            elif op == 2:
                voices[v].key_on(x)
            elif op == 3:
                voices[v].on = False
            elif op == 4:
                voices[v].pitch = x
            elif op == 5:
                voices[v].l, voices[v].r = x, y
        if t >= a.t0:
            l = r = 0
            for vo in voices:
                if vo.on:
                    s = vo.sample(ram)
                    l += s * vo.l >> 14
                    r += s * vo.r >> 14
            out += struct.pack('<hh', max(-32768, min(32767, l)), max(-32768, min(32767, r)))
        t += 1.0 / RATE
    w = wave.open(a.wav, 'wb')
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(RATE)
    w.writeframes(bytes(out))
    w.close()
    print('%s: %.1f s' % (a.wav, len(out) / 4 / RATE))


if __name__ == '__main__':
    main()
