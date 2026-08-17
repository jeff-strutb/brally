"""The PORTED MANIFEST: an explicit record of which original addresses this
tree implements, replacing seven attempts to infer it from comments.

WHY THIS REPLACES tools/isported.py's DETECTOR

isported.py answers "is this address already transcribed" by matching the shape
of the comment above a function. That was written in a couple of minutes as a
replacement for `grep`, and it was never rebuilt when its role changed from a
convenience into the thing that decides what work agents are given.

It has now been wrong seven distinct ways, and the cost was not spread evenly:

  1. non-recursive glob                    -- saw 63 of 123 modules
  2. `[^*]*` cannot cross a line           -- matched almost no banner
  3. `.*?` ran from a mention into the
     next function                         -- right verdict, wrong name
  4. greedy prefix captured a LATER
     address on the same line              -- wrong address entirely
  5. counted this project's own frontier
     stubs as ports                        -- false PORTED
  6. cross-build match with no build tag   -- a D3D number matched a Glide
                                              banner and reported an opcode
                                              handler as the entry point
  7. prefix restricted to decoration       -- SIX ported handlers read as
                                              missing at once, and an agent
                                              was sent to re-transcribe them

Four more attempts to fix (7) each broke something that had been correct, so
the last known-good version was restored and the defect left standing.

THE DESIGN ERROR, which is the point

Comments are prose. This tree grew three naming conventions organically -- an
annotated declaration, a banner over a body, and the address inside the
function's name -- and no regex distinguishes a banner from a file-header index
from a passing mention in every case, because the distinction is not reliably
encoded in the text.

The fix is not a better parser. It is to STOP INFERRING. A module states which
original addresses it implements, in one machine-readable form, and this file
reads that statement. A claim that is written down can be checked, versioned,
and argued with; a claim inferred from comment shape can only be re-guessed.

THE FORM

Anywhere in a .c or .h under port/, a line of exactly:

    /* @implements 0xADDR BUILD SYMBOL */

  BUILD   glide | d3d   -- REQUIRED, because a bare address is not
                           self-describing: the same number names different
                           functions in the two images, and defect (6) above is
                           what happens when that is left implicit.
  SYMBOL  the C function that implements it.

Example:

    /* @implements 0x1001CC00 glide BrRallyMain */

This file also carries the LEGACY index rebuilt from isported.py's last
validated detector, so nothing is lost on day one; `--audit` reports which
addresses are known only by inference and therefore still need a manifest line.
The intent is that the legacy index shrinks to nothing.

Usage:
    manifest.py 0x1001CC00 [...]   is it implemented, and by what
    manifest.py --list             every manifest line found
    manifest.py --audit            manifest vs legacy inference, and the gap
"""
import sys, os, re, csv, glob

SRC = ('port/src/**/*.c', 'port/include/**/*.h', 'port/host/**/*.c')
RE_IMPL = re.compile(
    r'/\*\s*@implements\s+0x([0-9A-Fa-f]{8})\s+(glide|d3d)\s+(\w+)\s*\*/')


def manifest():
    """addr -> [(build, symbol, file, line), ...]. A list, because two builds
    can legitimately claim one number and that must be visible, not resolved."""
    out = {}
    for pat in SRC:
        for f in glob.glob(pat, recursive=True):
            for i, ln in enumerate(open(f, errors='ignore'), 1):
                m = RE_IMPL.search(ln)
                if m:
                    out.setdefault(m.group(1).upper(), []).append(
                        (m.group(2), m.group(3), os.path.basename(f), i))
    return out


def legacy():
    """isported.py's inferred index, kept only until the manifest covers it."""
    sys.path.insert(0, os.path.dirname(__file__))
    try:
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            'isp', os.path.join(os.path.dirname(__file__), 'isported.py'))
        isp = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(isp)
        return isp.definitions()
    except Exception as e:
        print("  (legacy detector unavailable: %s)" % e)
        return {}


def pairs():
    """glide addr -> d3d addr and back, from config/shared.csv.

    WHY THIS IS HERE: the same function has two addresses, one per build, and a
    claim is tagged with the build it was read from. So asking about a GLIDE
    address whose D3D twin is already transcribed used to answer "not
    implemented" -- and that answer nearly sent work to re-transcribe eleven
    helpers that already existed under their D3D numbers. The cross-build twin
    is not the same claim, but it is the same code, so the honest answer is
    "already done, under 0x<other> in the other build" rather than "missing".

    `body-dup:N` is excluded: it means byte-identical to N Glide functions with
    no evidence which, so it is not a pairing and must not resolve to one.
    """
    g2d, d2g = {}, {}
    try:
        with open('config/shared.csv') as fh:
            for r in csv.DictReader(fh):
                if not r['glide_va'] or not r['d3d_va']:
                    continue
                if r.get('matched_by', '').startswith('body-dup'):
                    continue
                g = '%08X' % int(r['glide_va'], 16)
                d = '%08X' % int(r['d3d_va'], 16)
                g2d[g] = d
                d2g[d] = g
    except OSError:
        pass
    return g2d, d2g


def report(addr, man, leg, twin=None):
    k = '%08X' % addr
    print("0x%s" % k)
    rows = man.get(k, [])
    if rows:
        for build, sym, f, ln in rows:
            print("  IMPLEMENTED  %-5s %s   (%s:%d)  [manifest]" % (build, sym, f, ln))
        return True
    # Not claimed at this address -- but its cross-build twin might be, and that
    # is the same function. Report it so nobody re-transcribes existing code.
    if twin and twin in man:
        for build, sym, f, ln in man[twin]:
            print("  IMPLEMENTED via its %s twin 0x%s  %s   (%s:%d)" % (
                build, twin, sym, f, ln))
        print("  -> this address is not claimed, but 0x%s is the same function" % twin)
        print("     in the other build. Reuse it; do not re-transcribe.")
        return True
    if k in leg:
        sym, f, how = leg[k]
        print("  inferred     %s   (%s, by %s)" % (sym, f, how))
        print("  -> NO MANIFEST LINE. This is a guess from comment shape and has")
        print("     been wrong seven ways. Add:")
        print("       /* @implements 0x%s <glide|d3d> %s */" % (k, sym))
        return True
    print("  not implemented, and not inferred.")
    return False


def main():
    man = manifest()
    if '--emit' in sys.argv:
        # config/ported.csv is the HUMAN-READABLE record, and it is GENERATED
        # from the @implements lines in the source rather than maintained by
        # hand. That direction matters: a hand-kept index drifts the moment a
        # function moves and nothing fails when it does. Regenerate and diff to
        # check the two agree -- a non-empty diff means someone edited the
        # wrong one.
        rows = []
        for k in sorted(man):
            for build, sym, f, ln in man[k]:
                rows.append(('0x' + k, build, sym, f, ln))
        with open('config/ported.csv', 'w', newline='') as fh:
            w = csv.writer(fh)
            w.writerow(['address', 'build', 'symbol', 'file', 'line'])
            for r in rows:
                w.writerow(r)
        print("wrote config/ported.csv  (%d entries)" % len(rows))
        return
    if '--list' in sys.argv:
        for k in sorted(man):
            for build, sym, f, ln in man[k]:
                print("  0x%s %-5s %-34s %s:%d" % (k, build, sym, f, ln))
        print("\n%d manifest entries" % sum(len(v) for v in man.values()))
        return
    if '--audit' in sys.argv:
        leg = legacy()
        only_leg = sorted(set(leg) - set(man))
        print("manifest entries        : %d" % sum(len(v) for v in man.values()))
        print("addresses in manifest   : %d" % len(man))
        print("addresses ONLY inferred : %d   <- these still need a line" % len(only_leg))
        for k in only_leg[:15]:
            print("     0x%s  %s (%s)" % (k, leg[k][0], leg[k][1]))
        if len(only_leg) > 15:
            print("     ... and %d more" % (len(only_leg) - 15))
        return
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not args:
        raise SystemExit(__doc__)
    leg = legacy()
    g2d, d2g = pairs()
    for a in args:
        addr = int(a, 16)
        k = '%08X' % addr
        twin = g2d.get(k) or d2g.get(k)   # whichever build this address is in
        report(addr, man, leg, twin)
        print()


if __name__ == '__main__':
    main()
