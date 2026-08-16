"""Answer "is this address already ported, and where?" -- for BOTH builds.

WHY THIS EXISTS

Four separate passes this project set out to port a function, found nothing
when they grepped its address, wrote a working implementation, and only then
discovered the tree already had it -- under the OTHER build's address. The
clipper is the clearest case: four of its seven planes were sitting in
slice1_03.c under their D3D addresses while a pass hunted the Glide ones. That
pass threw its own work away, which was right, and expensive.

The rule was written into CONVENTIONS.md ("grep BOTH builds' addresses"). Prose
does not get run. This does.

WHAT IT CHECKS, in descending order of confidence:

  1. config/shared.csv -- the paired address, and HOW it was matched. A `body`
     match is byte-identical after normalisation; a `prefix` match means the two
     maps disagreed about the extent, not about the function.
  2. Both addresses, grepped across port/ -- source, headers and tests.
  3. If neither is paired: a STRING CROSS-REFERENCE. D3D statically links the
     CRT and Glide imports it, so the same function can differ in instruction
     encoding and length -- `E8 rel32` against `FF 15 disp32` -- and never match
     byte-wise however the extents are trimmed. That is not a map error and no
     amount of hashing fixes it. What does fix it is that both builds reference
     the same string literals, so the strings a function uses identify it across
     builds when its bytes cannot.

Usage:  whereis.py 0x1006F310 [0x...]
"""
import sys, os, re, csv, struct, subprocess

sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib

SHARED = 'config/shared.csv'


def pairings(addr):
    """EVERY reading of this address: [(paired, side, how), ...].

    A plain number is not self-describing. The same value can be a valid
    function address in BOTH builds, naming two unrelated functions -- the two
    images are the same size order and their .text ranges overlap almost
    entirely. An earlier version of this tool searched the D3D column first and
    returned on the first hit, so it confidently reported the Glide partner of
    a D3D function when it had been handed a GLIDE address. It did that twice
    in one session and both answers were wrong.

    Now every match is returned and the caller prints all of them, so an
    ambiguous address is visibly ambiguous instead of silently resolved.
    """
    out = []
    if not os.path.exists(SHARED):
        return out
    for r in csv.DictReader(open(SHARED)):
        d = int(r['d3d_va'], 16)
        g = int(r['glide_va'], 16) if r['glide_va'] else None
        how = r.get('matched_by', '')
        if d == addr:
            out.append((g, 'glide', how, 'read as a D3D address'))
        if g == addr:
            out.append((d, 'd3d', how, 'read as a GLIDE address'))
    return out


def grep_port(addr):
    """Every port/ line mentioning this address."""
    pat = '0x%08X' % addr
    try:
        out = subprocess.run(['grep', '-rniE', pat, 'port/'],
                             capture_output=True, text=True).stdout
    except Exception:
        return []
    return [l for l in out.splitlines() if l.strip()][:8]


def strings_of(dll, addr, size):
    """ASCII literals a function references, via its relocated operands."""
    p = pelib.load(dll)
    text, text_va = p.text()
    off = addr - text_va
    if off < 0 or off + size > len(text):
        return set()
    body = text[off:off + size]
    relocs = {p.image_base + r for r in p.relocs}
    found = set()
    for k in range(size - 3):
        if (addr + k) not in relocs:
            continue
        target = struct.unpack_from('<I', body, k)[0]
        try:
            raw = p.read(target, 64)
        except Exception:
            continue
        if not raw:            # target outside any mapped section
            continue
        m = re.match(rb'[\x20-\x7e]{4,}', raw)
        if m:
            found.add(m.group().decode('latin1'))
    return found


def size_of(csvpath, addr):
    if not os.path.exists(csvpath):
        return None
    for r in csv.DictReader(open(csvpath)):
        if int(r['va'], 16) == addr:
            return int(r['size'])
    return None


def report(addr):
    print("0x%08X" % addr)

    ps = pairings(addr)
    if len(ps) > 1:
        print("  AMBIGUOUS -- this number is a valid address in BOTH builds.")
        print("              Both readings are shown; pick the one whose build")
        print("              you actually have in hand.")
    for paired, side, how, why in ps:
        print("  paired    0x%08X (%s)   matched by %-6s   [%s]"
              % (paired if paired else 0, side, how, why))
        if how == 'prefix':
            print("            ^ prefix match: the two maps disagreed about the")
            print("              EXTENT, not about the function.")
    if not ps:
        print("  paired    (none in shared.csv)")

    cands = [(addr, 'this')] + [(p, 'paired/%s' % side) for p, side, _, _ in ps if p]
    for a, label in cands:
        if a is None:
            continue
        hits = grep_port(a)
        print("  port/     0x%08X (%s): %s"
              % (a, label, ("%d hit(s)" % len(hits)) if hits else "NOTHING"))
        for h in hits[:4]:
            print("              %s" % h[:110])

    if ps:
        return

    # No pairing: try strings. This is the CRT-linkage case.
    d3d_sz = size_of('config/functions.csv', addr)
    if not d3d_sz:
        print("  strings   (address not in the D3D map)")
        return
    mine = strings_of('orig/BRD3D.dll', addr, d3d_sz)
    if not mine:
        print("  strings   (this function references no ASCII literals)")
        return
    print("  strings   %s" % ", ".join(sorted(mine)[:4]))

    graw = open('orig/BRGlide.dll', 'rb').read()
    hits = [s for s in mine if s.encode('latin1') in graw]
    if hits:
        print("            %d of %d also appear in BRGlide -- so the function"
              % (len(hits), len(mine)))
        print("            almost certainly EXISTS there under another address.")
        print("            Find it by locating those strings and back-tracking")
        print("            their references; do NOT conclude d3d_only.")
    else:
        print("            none appear in BRGlide -- plausibly genuinely D3D-only.")


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for a in sys.argv[1:]:
        report(int(a, 16))
        print()


if __name__ == '__main__':
    main()
