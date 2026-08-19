#!/usr/bin/env python3
"""Check every float constant in port/ that names an original address against
the bytes actually in the shipped images.

CONVENTIONS.md: "Read float constants out of the binary rather than assuming
them. This is cheap and has repeatedly been load-bearing."  This is that check,
run over the whole tree instead of one packet at a time, so the DENOMINATOR is
reportable and not just the hits.

What it matches: a float/double initialiser, or a float literal used inline,
that carries an original address in a comment on the same line -- which is the
annotation convention every packet in this tree already uses, e.g.

    static const float BrK08F0A8 = 1.0f;   /* 0x1008F0A8 */
    float k = pEnv->dt * 0.3f;             /* 0x1008F5C4 */
    extern float g_BrK08F548;              /* 0x1008F548 == 3C6A0EA1 ... */

VALIDATED BEFORE USE, per CONVENTIONS.md's rule that a detector which has not
been checked against a known answer is not evidence.  `--selftest` runs it
against four addresses whose answers are established independently:

    0x1008F548  3C6A0EA1  the 1/70 axis scale (was ported as 1/80)
    0x1008F518  3FAAAAAB  4/3               (was ported as 1.0f)
    0x1008F51C  42652EE0  180/pi            (was "NOT ESTABLISHED")
    0x1008F514  40000000  2.0f              (was DERIVED, and right)

and additionally feeds itself a value it knows to be wrong, to confirm the
comparison can report a mismatch at all.  A checker that only ever says OK is
the failure mode this project keeps hitting.

Reporting rules that matter:

  - An address is looked up in BOTH images.  Most of this tree is transcribed
    from BRD3D.dll, but the same number names different objects in the two
    builds (CONVENTIONS.md), so a hit is only counted where the address lands
    in a CONSTANT section (.rdata/.text) of that image.  An address that is
    .data in one image and .rdata in the other is resolved to the .rdata one.
  - An address that resolves in NEITHER image as constant data is reported
    UNRESOLVED and counted separately.  It is not counted as a pass.
  - Comparison is on the 32-bit PATTERN, not on a tolerance.  These are
    transcriptions of specific bytes.

Usage:
    tools/constcheck.py [--selftest] [--verbose] [paths...]
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGES = [('d3d', os.path.join(ROOT, 'orig', 'BRD3D.dll')),
          ('glide', os.path.join(ROOT, 'orig', 'BRGlide.dll'))]

# Sections whose bytes are fixed in the shipped image.  .data is initialised
# too, but it is writable and the game overwrites it, so a float found there
# is not evidence about a constant.
# .rdata ONLY, and this gate is the whole difference between a usable tool and
# a noise generator.  The first version accepted .text too, on the theory that
# .text is also fixed bytes.  It is -- but the annotation `/* 0x10064920 */`
# beside a line of C names the INSTRUCTION the line came from, not a constant,
# and an instruction address lands in .text.  So every such comment "resolved",
# got read as a float, and 257 of 1274 lines were reported WRONG against
# machine code reinterpreted as IEEE754.  MSVC puts float literals in .rdata;
# requiring .rdata discards the instruction annotations by construction.
CONST_SECTIONS = ('.rdata',)

# A float literal: 1.0f, -0.5, 3.051804378628731e-05f, 1.0f / 128.0f
NUM = r'[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?[fF]?'
EXPR = r'%s(?:\s*/\s*%s)?' % (NUM, NUM)

DECL = re.compile(
    r'^\s*(?:static\s+)?(?:const\s+)?(float|double)\s+'
    r'([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(' + EXPR + r')\s*;'
    r'.*?/\*[^*]*?(0x1[0-9A-Fa-f]{7})')

# Same declaration, but with NO address in the comment.  The address is then
# taken from the NAME, because this tree names these constants after their
# address in at least three styles -- g_BrK08F548, BrK08F0A8, kF300 -- and
# CONVENTIONS.md's rule is never to encode one naming convention.  The recall
# arm of --selftest is what caught this: g_BrK08F548's DEFINITION carries no
# address in its comment (only the extern in the header does), so the
# address-in-comment rule alone missed the single most load-bearing constant
# in the tree while reporting a confident total.
DECL_NOADDR = re.compile(
    r'^\s*(?:static\s+)?(?:const\s+)?(float|double)\s+'
    r'([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(' + EXPR + r')\s*;')
# A six-hex-digit tail completes to 0x10______, the images' image base page.
# Six and not four: four is ambiguous and would invent addresses out of names
# like `kF300` that happen to end in hex.  Those are reached by their comment.
NAME_ADDR = re.compile(r'([0-9A-Fa-f]{6})$')

# A float literal on a line whose trailing comment is nothing but an address.
# Kept, but it only survives the .rdata gate above when the address really is
# a constant slot -- which is what makes the two rules safe together.  The
# literal must look like a FLOAT (contain a '.' or an exponent); a bare integer
# on such a line is an index or a count, never an .rdata float.
FLOATNUM = r'[-+]?(?:\d+\.\d*|\.\d+)(?:[eE][-+]?\d+)?[fF]?'
FLOATEXPR = r'%s(?:\s*/\s*%s)?' % (FLOATNUM, FLOATNUM)
INLINE = re.compile(
    r'(' + FLOATEXPR + r')[^;/]*;\s*/\*\s*(0x1[0-9A-Fa-f]{7})\s*\*/')


def evaluate(src):
    """Evaluate a C float literal expression as a Python float."""
    return eval(re.sub(r'([0-9.])[fF]\b', r'\1', src))


def to_f32(x):
    try:
        return struct.unpack('<f', struct.pack('<f', x))[0]
    except OverflowError:
        return float('inf') if x > 0 else float('-inf')


def bits32(x):
    try:
        return struct.unpack('<I', struct.pack('<f', x))[0]
    except OverflowError:
        return 0x7F800000 if x > 0 else 0xFF800000


class Images(object):
    def __init__(self):
        self.imgs = []
        for tag, path in IMAGES:
            if os.path.exists(path):
                self.imgs.append((tag, pe.load(path)))

    def lookup(self, va, width):
        """Return (tag, section, bytes) preferring an image where the address
        is in a constant section.  None if no image maps it at all."""
        best = None
        for tag, p in self.imgs:
            s = p.sect_for_rva(va - p.image_base)
            if s is None:
                continue
            b = p.read(va, width)
            if b is None or len(b) < width:
                continue
            if s.name in CONST_SECTIONS:
                return (tag, s.name, b)
            if best is None:
                best = (tag, s.name, b)
        return best


def scan(paths):
    hits = []
    for base in paths:
        for dirpath, _dirs, files in os.walk(base):
            for fn in sorted(files):
                if not fn.endswith(('.c', '.h')):
                    continue
                full = os.path.join(dirpath, fn)
                with open(full, 'r', errors='replace') as fh:
                    for n, line in enumerate(fh, 1):
                        m = DECL.search(line)
                        if m:
                            ty, name, src, va = m.groups()
                            hits.append((full, n, ty, name, src, int(va, 16)))
                            continue
                        m = DECL_NOADDR.match(line)
                        if m:
                            ty, name, src = m.groups()
                            a = NAME_ADDR.search(name)
                            if a:
                                hits.append((full, n, ty, name, src,
                                             0x10000000 | int(a.group(1), 16)))
                                continue
                        m = INLINE.search(line)
                        if m:
                            src, va = m.groups()
                            # An UNSUFFIXED C literal is a double, and the
                            # original then loads `qword ptr`.  Getting this
                            # wrong produced the tool's only "mismatch" on its
                            # first clean run: 0x1008F438 read as a dword is
                            # 00000000, and read as the qword it actually is
                            # (0x406FE00000000000) it is exactly the 255.0 the
                            # source says.  The suffix is the width.
                            ty = 'float' if src.rstrip().endswith(('f', 'F')) \
                                 else 'double'
                            hits.append((full, n, ty, '<inline>',
                                         src, int(va, 16)))
    return hits


def check(hits, imgs, verbose=False):
    ok = wrong = unresolved = 0
    bad = []
    for full, n, ty, name, src, va in hits:
        width = 8 if ty == 'double' else 4
        got = imgs.lookup(va, width)
        rel = os.path.relpath(full, ROOT)
        if got is None or got[1] not in CONST_SECTIONS:
            unresolved += 1
            where = 'unmapped' if got is None else '%s %s' % (got[0], got[1])
            if verbose:
                print('  UNRESOLVED %-44s %08X  %-13s (%s)'
                      % ('%s:%d' % (rel, n), va, name, where))
            continue
        tag, sect, b = got
        try:
            want = evaluate(src)
        except Exception:
            unresolved += 1
            continue
        # BOTH WIDTHS, and this is deliberate rather than lax.
        #
        # C cannot tell you which the original loaded.  `fld dword ptr` and
        # `fld qword ptr` both become a plain literal here, and this tree
        # WIDENS float constants to double on purpose, because the x87
        # registers are 53-bit and `double` models them exactly -- so
        # `v * 15.714285850524902` in br_sfx.c is a dword constant written as
        # the double it becomes once loaded, and `(double)*pCur * 255.0` in
        # slice2_16.c is a genuine qword one.  A rule keyed on the `f` suffix
        # calls one of those two wrong whichever way it is written.  Trying
        # both and reporting the width that matched keeps the tool from
        # crying wolf, which is the failure that gets a checker ignored.
        hit = None
        f32 = imgs.lookup(va, 4)
        if f32 and f32[1] in CONST_SECTIONS:
            iv = struct.unpack('<I', f32[2])[0]
            if bits32(to_f32(want)) == iv:
                hit = ('dword', '%.9g (%08X)' % (
                    struct.unpack('<f', f32[2])[0], iv))
        if hit is None:
            f64 = imgs.lookup(va, 8)
            if f64 and f64[1] in CONST_SECTIONS and \
                    struct.pack('<d', want) == f64[2]:
                hit = ('qword', '%.17g' % struct.unpack('<d', f64[2])[0])
        if hit is not None:
            ok += 1
            if verbose:
                print('  ok   %-5s %-40s %08X  %-13s %s'
                      % (hit[0], '%s:%d' % (rel, n), va, name, hit[1]))
        else:
            wrong += 1
            iv = struct.unpack('<I', b[:4])[0]
            shown = 'dword %.9g (%08X)' % (struct.unpack('<f', b[:4])[0], iv)
            wrong_list_entry = (rel, n, name, va, src, shown, tag)
            bad.append(wrong_list_entry)
    return ok, wrong, unresolved, bad


def selftest(imgs):
    """Check the reader against four independently established answers, and
    confirm it can report a mismatch."""
    cases = [(0x1008F548, 0x3C6A0EA1), (0x1008F518, 0x3FAAAAAB),
             (0x1008F51C, 0x42652EE0), (0x1008F514, 0x40000000)]
    fails = 0
    for va, want in cases:
        got = imgs.lookup(va, 4)
        if got is None:
            print('SELFTEST FAIL %08X: not mapped' % va)
            fails += 1
            continue
        tag, sect, b = got
        have = struct.unpack('<I', b)[0]
        status = 'ok' if have == want else 'FAIL'
        if have != want:
            fails += 1
        print('  selftest %-4s %08X  want %08X  got %08X  [%s %s]'
              % (status, va, want, have, tag, sect))
    # Negative control: the checker must be able to say "wrong".
    fake = [('<selftest>', 0, 'float', 'k', '0.0125', 0x1008F548)]
    _o, w, _u, _b = check(fake, imgs)
    print('  selftest %-4s negative control (1/80 against 0x1008F548)'
          % ('ok' if w == 1 else 'FAIL'))
    if w != 1:
        fails += 1

    # RECALL.  Every scan written in this project has been wrong on first use
    # and always in the same direction -- under-reporting what is there.  So
    # check that the scanner FINDS declarations known to exist, in each of the
    # three naming styles the tree uses, before believing any count it prints.
    want_found = [('src/core/slice2_12.c', 'BrK08F0A8'),
                  ('src/core/slice2_15.c', 'kF308'),
                  ('src/core/slice2_19.c', 'g_BrK08F548')]
    found = set()
    for full, _n, _t, name, _s, _va in scan([os.path.join(ROOT, 'port')]):
        found.add((os.path.relpath(full, ROOT), name))
    for want in want_found:
        hit = want in found
        print('  selftest %-4s recall %s %s' % ('ok' if hit else 'FAIL',
                                                want[0], want[1]))
        if not hit:
            fails += 1
    return fails


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    verbose = '--verbose' in sys.argv
    imgs = Images()
    if not imgs.imgs:
        print('no images under orig/')
        return 2
    if '--selftest' in sys.argv:
        print('SELFTEST')
        f = selftest(imgs)
        print('selftest: %s' % ('PASS' if f == 0 else '%d FAILURES' % f))
        if f:
            return 1
        print()
    paths = args or [os.path.join(ROOT, 'port')]
    hits = scan(paths)
    ok, wrong, unresolved, bad = check(hits, imgs, verbose)
    if bad:
        print('MISMATCHES -- the source says one thing and the image another:')
        for rel, n, name, va, src, shown, tag in bad:
            print('  %s:%d' % (rel, n))
            print('      %-14s %08X  source %-24s image %s  [%s]'
                  % (name, va, src, shown, tag))
    print()
    print('checked %d address-annotated float constants: %d match, %d WRONG, '
          '%d unresolved' % (len(hits), ok, wrong, unresolved))
    return 1 if wrong else 0


if __name__ == '__main__':
    sys.exit(main())
