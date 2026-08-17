"""Lint the manifest against config/shared.csv's build classification.

WHY

A census of all 693 d3d-tagged manifest claims found nine that are WRONG FOR
GLIDE -- the port read BRD3D.dll where the two builds genuinely differ. Glide is
this project's declared renderer reference, so those are live bugs. Examples:
Glide clamps an iterated colour to 255.0 where D3D clamps to 1.0; Glide loads
".hnt" where D3D loads ".hnd"; Glide's font registration installs 2 textures
where D3D's installs 106.

Finding them cost a full cross-build disassembly of every claim. But FIVE of the
nine needed no disassembly at all, because `config/shared.csv` had already
classified those rows as `renderer` -- meaning "the same dispatch slot holds
DIFFERENT CODE in the two builds". A `renderer` row is by definition not shared,
so a `d3d`-tagged claim sitting on one is a CATEGORY ERROR: it says "this C
implements the address" while the pairing data says the two builds do not
implement it the same way.

That is a contradiction between two files we already have. No bytes required.

WHAT IT CHECKS

  1. CATEGORY ERROR -- a d3d-tagged claim whose shared.csv row is classed
     `renderer`. The class says the builds differ; the claim implies they do
     not. Five of the nine known bugs are exactly this.

  2. NO GLIDE PARTNER -- a d3d-tagged claim with no Glide counterpart at all.
     Thirteen of these exist and the census could not verify ANY of them.
     Arguably higher risk than average rather than lower: being unpaired can
     itself mean the function is D3D-specific.

  3. AMBIGUOUS PAIRING -- `matched_by = body-dup:N` means the D3D function is
     byte-identical to N Glide functions and THE EVIDENCE DOES NOT SAY WHICH.
     It is not a pairing, and treating it as one has already produced five
     false duplicates and three confirmed bad pairings.

WHAT IT DOES NOT CHECK, WHICH IS MOST OF THE RISK

Four of the nine bugs are in rows classed `callsite`, `prefix` or `shape` --
weak evidence classes that nonetheless assert the builds match. Nothing here
can see those; only reading both disassemblies finds them. The measured
divergence rates from the census, per class:

    body            0/484    0.0%   [0.0%,  0.8%]   <- byte-identical is SOUND
    body+ptrsite    0/71     0.0%
    body+callsite   0/32     0.0%
    callsite        3/31     9.7%   [3.3%, 24.9%]
    prefix          0/21     0.0%
    shape           1/4     25.0%   [4.6%, 69.9%]
    renderer/slot   5/6     83.3%   [43.6%, 97.0%]  <- what this file catches

So this lint covers the highest-rate class and nothing else. It is a cheap
standing guard against NEW instances of the one mistake we can detect for free,
not a clean bill of health. Anyone reading a clean run as "the manifest agrees
with Glide" is reading it wrong.

Calibrated: --selftest asserts the five known `renderer`-class bugs are the
kind of thing rule 1 fires on. If a change stops it firing, the change is wrong.

Usage:
    buildlint.py              report violations
    buildlint.py --selftest   the rule must still fire on the known cases
"""
import sys, os, csv

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
os.chdir(ROOT)

# The five d3d-tagged claims the census confirmed DIVERGENT because their
# shared.csv row is classed renderer/slot. Rule 1 must fire on these.
KNOWN_CATEGORY_ERRORS = {
    '0x10024260',    # Glide fmul +0.25 and fsubr; D3D -0.25 on Y
    '0x1001BE90',    # 534 vs 228 insn; the port is the D3D triangle path
    '0x10020FA0',    # 418 vs 241 insn, similarity 0.112
    '0x10021560',    # 396 vs 239 insn, similarity 0.306
    '0x10073820',    # D3D registers 106 textures, Glide registers 2
}


def shared():
    """d3d_va -> (glide_va, class, matched_by). Keyed on the D3D address
    because that is what a d3d-tagged claim names."""
    out = {}
    with open('config/shared.csv') as fh:
        for r in csv.DictReader(fh):
            if not r['d3d_va']:
                continue
            out['0x%08X' % int(r['d3d_va'], 16)] = (
                r['glide_va'], r.get('class', ''), r.get('matched_by', ''))
    return out


def claims():
    out = []
    with open('config/ported.csv') as fh:
        for r in csv.DictReader(fh):
            out.append(('0x%08X' % int(r['address'], 16), r['build'],
                        r['symbol'], r['file']))
    return out


def scan():
    sh = shared()
    cat, nopartner, ambig = [], [], []
    for addr, build, sym, f in claims():
        if build != 'd3d':
            continue
        row = sh.get(addr)
        if row is None:
            nopartner.append((addr, sym, f, 'not in shared.csv'))
            continue
        gva, cls, mb = row
        if cls == 'renderer' or cls == 'slot':
            cat.append((addr, sym, f, cls, gva))
        elif not gva:
            nopartner.append((addr, sym, f, cls or 'd3d_only'))
        elif mb.startswith('body-dup'):
            ambig.append((addr, sym, f, mb))
    return cat, nopartner, ambig


def main():
    cat, nopartner, ambig = scan()

    if '--selftest' in sys.argv:
        hit = {c[0] for c in cat}
        missed = sorted(KNOWN_CATEGORY_ERRORS - hit)
        for a in sorted(KNOWN_CATEGORY_ERRORS & hit):
            print("  fires on %s (known DIVERGENT-PORTED-D3D)" % a)
        if missed:
            # A claim that was REPAIRED no longer exists to fire on, and that
            # is a fix, not a regression -- so say which, rather than failing
            # blind. An earlier detector here failed its selftest for exactly
            # this reason: it asserted a defect stays present.
            sh = shared()
            gone = [a for a in missed if a not in {c[0] for c in cat}]
            print("  NOT FIRING on: %s" % ', '.join(missed))
            print("  -> check each: a repaired claim is a FIX (the d3d tag was")
            print("     removed or re-pointed); a claim still tagged d3d on a")
            print("     renderer row and not reported is a DETECTOR REGRESSION.")
            for a in gone:
                row = sh.get(a)
                print("     %s  shared.csv class=%s" % (a, row[1] if row else 'ABSENT'))
            return 1
        print("selftest ok: rule 1 fires on all %d known category errors"
              % len(KNOWN_CATEGORY_ERRORS))
        return 0

    print("CATEGORY ERROR -- d3d claim on a row shared.csv classes renderer/slot.")
    print("The class means the two builds hold DIFFERENT CODE in that slot, so a")
    print("d3d reading is not valid for Glide. 5 of 9 known cross-build bugs are")
    print("this shape. (%d found)" % len(cat))
    for addr, sym, f, cls, gva in sorted(cat):
        print("  %-12s %-34s %-9s glide=%s  %s" % (addr, sym[:34], cls,
                                                   gva or '-', f))

    print()
    print("NO GLIDE PARTNER -- unverifiable against the reference build. Not")
    print("safe by default: being unpaired can itself mean D3D-specific. (%d)"
          % len(nopartner))
    for addr, sym, f, why in sorted(nopartner):
        print("  %-12s %-34s %-14s %s" % (addr, sym[:34], why, f))

    print()
    print("AMBIGUOUS PAIRING -- matched_by=body-dup:N means byte-identical to N")
    print("Glide functions with no evidence which. Already produced 5 false")
    print("duplicates and 3 confirmed bad pairings. (%d)" % len(ambig))
    for addr, sym, f, mb in sorted(ambig):
        print("  %-12s %-34s %-14s %s" % (addr, sym[:34], mb, f))

    print()
    print("Reminder: this file covers the renderer/slot class only. The census")
    print("found 4 more divergences in callsite/prefix/shape rows, which no")
    print("cross-file check can see. A clean run here is not a clean manifest.")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
