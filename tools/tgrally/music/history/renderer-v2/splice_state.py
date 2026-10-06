import struct, re, juce_state as J
def rebuild(raw, xml_fn):
    x, m = J.split(raw); d = J.jdecode(m.group(1).strip())
    j = d.find(b'VC2!'); n = struct.unpack('<I', d[j + 4:j + 8])[0]
    xml = d[j + 8:j + 8 + n].decode('utf-8'); new = xml_fn(xml).encode('utf-8'); delta = len(new) - n
    d2 = bytearray(d[:j + 8] + new + d[j + 8 + n:])
    i = d2.find(b'CcnK'); d2[i + 4:i + 8] = struct.pack('>I', struct.unpack('>I', d2[i + 4:i + 8])[0] + delta)
    d2[j - 4:j] = struct.pack('>I', struct.unpack('>I', d2[j - 4:j])[0] + delta)
    d2[j + 4:j + 8] = struct.pack('<I', len(new))
    enc = J.jencode(bytes(d2))
    return (x[:m.start(1)] + enc + x[m.end(1):]).encode('latin1')
def xml_of(raw):
    x, m = J.split(raw); d = J.jdecode(m.group(1).strip()); j = d.find(b'VC2!'); n = struct.unpack('<I', d[j + 4:j + 8])[0]
    return d[j + 8:j + 8 + n].decode('utf-8')
