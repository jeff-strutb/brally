import re
ALPH = ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"
def jdecode(s):
    n, _, body = s.partition('.'); n = int(n); out = bytearray(n); bit = 0
    for ch in body:
        v = ALPH.index(ch)
        for b in range(6):
            if bit >= n * 8: break
            if v >> b & 1: out[bit >> 3] |= 1 << (bit & 7)
            bit += 1
    return bytes(out)
def jencode(b):
    n = len(b); bits = n * 8; out = []
    for i in range(0, bits, 6):
        v = 0
        for k in range(6):
            j = i + k
            if j < bits and (b[j >> 3] >> (j & 7)) & 1: v |= 1 << k
        out.append(ALPH[v])
    return '%d.%s' % (n, ''.join(out))
def split(raw):
    x = raw.decode('latin1'); m = re.search(r'<IComponent>(.*?)</IComponent>', x, re.S); return x, m
