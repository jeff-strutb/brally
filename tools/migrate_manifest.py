"""Convert inferred port claims into explicit `@implements` lines.

DO NOT RE-RUN THIS WITHOUT READING THE SHORT-FORM WARNING BELOW.

WHAT THIS DOES AND WHAT IT REFUSES TO DO

tools/manifest.py reads explicit claims:

    /* @implements 0x1001CC00 glide BrRallyMain */

1,340 addresses in this tree are known only by INFERENCE from comment shape --
the method that has been wrong seven distinct ways and once sent an agent to
re-transcribe six functions that already existed. This migrates what can be
migrated safely and, more importantly, REFUSES the rest by name.

THE BUILD FIELD IS THE WHOLE DIFFICULTY. A bare address is not
self-describing: the same number names different functions in BRGlide.dll and
BRD3D.dll, and that has produced false "already ported" answers repeatedly --
most recently reporting a 441-byte unported TMEM allocator as ported because a
D3D-addressed header used the same number.

So the build is derived from config/shared.csv, which pairs the two images:

  the address appears ONLY as a d3d_va      -> d3d,   safe
  the address appears ONLY as a glide_va    -> glide, safe
  it appears as BOTH, for different rows    -> AMBIGUOUS. Refused.

An ambiguous address cannot be resolved from the number alone, by definition.
Guessing it would reintroduce the exact defect the manifest exists to remove,
so those are emitted to a review list for a human or an agent to settle by
reading the module -- never auto-written.

A migrated line is a claim that the symbol implements that address. This tool
does not verify that claim; it only preserves one that already existed, in a
form that can be checked. The claims themselves are what the equivalence audit
is for.

THE SHORT-FORM NAME RULE IS UNSOUND AND ITS OUTPUT HAS BEEN PURGED.

isported.py's third detection convention reads the address out of the FUNCTION
NAME -- BrUiHook81_100450F0, BrMenuCap0730. For an eight-digit name that is
sound. For a FOUR-digit one it is not: the detector CONSTRUCTS a candidate by
prefixing `0x1000` or padding to `0x100XXXX0`, which INVENTS an address rather
than reading one.

Migrating those wrote 21 false claims into the source. Ten landed in
slice2_25.c, where `BrOpt3760` became `@implements 0x10003760` -- an address
whose real code is a 111-byte realloc wrapper, unrelated to the transcribed
body. They were found by a pass writing plain-English descriptions, which had
to read each function to describe it and noticed the claims did not match.

That is the mechanism worth remembering: the manifest ended the INFERENCE
problem and did nothing about the CORRECTNESS problem, exactly as its own
banner said. A written-down claim can be checked -- and checking it is what
found these. Nothing about writing it down made it true.

Claims derived from a four-digit name are now refused.

Usage:
    migrate_manifest.py --plan     what would be written, and what is refused
    migrate_manifest.py --apply    write the safe lines into the source
"""
import sys, os, re, csv, glob, collections

sys.path.insert(0, os.path.dirname(__file__))


def legacy_index():
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        'isp', os.path.join(os.path.dirname(__file__), 'isported.py'))
    isp = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(isp)
    return isp.definitions()


def build_of():
    """addr(upper hex) -> 'glide' | 'd3d' | 'ambiguous'."""
    d3d, gl = set(), set()
    for r in csv.DictReader(open('config/shared.csv')):
        if r['d3d_va']:
            d3d.add(r['d3d_va'][2:].upper())
        if r['glide_va']:
            gl.add(r['glide_va'][2:].upper())
    # FALLBACK TO THE FUNCTION MAPS. shared.csv only lists functions that were
    # PAIRED; an address present in just one image never appears there, which
    # left 485 claims unresolvable for no good reason. The per-image maps know
    # every function start in each build, so membership in exactly one of them
    # settles the build just as firmly.
    for path, s in (('config/functions_glide.csv', gl), ('config/functions.csv', d3d)):
        if not os.path.exists(path):
            continue
        for r in csv.DictReader(open(path)):
            try:
                s.add('%08X' % int(r['va'], 16))
            except Exception:
                pass
    out = {}
    for a in d3d | gl:
        if a in d3d and a in gl:
            out[a] = 'ambiguous'
        else:
            out[a] = 'd3d' if a in d3d else 'glide'
    return out


def existing_manifest():
    seen = set()
    for pat in ('src/core/**/*.c', 'include/**/*.h', 'ports/macos/**/*.c'):
        for f in glob.glob(pat, recursive=True):
            for ln in open(f, errors='ignore'):
                m = re.search(r'@implements\s+0x([0-9A-Fa-f]{8})', ln)
                if m:
                    seen.add(m.group(1).upper())
    return seen


def main():
    leg = legacy_index()
    bmap = build_of()
    have = existing_manifest()

    safe, ambig, unknown, shortform = [], [], [], []
    for addr, (sym, fname, how) in sorted(leg.items()):
        if addr in have:
            continue
        # Refuse anything whose address could have been invented from a
        # four-digit fragment of its own symbol name. See the warning above.
        import re as _re
        if any(('1000' + d.upper())[-8:] == addr
               or ('100' + d.upper()).ljust(8, '0')[:8] == addr
               for d in _re.findall(r'[0-9A-Fa-f]{4}(?![0-9A-Fa-f])', sym)):
            shortform.append((addr, sym, fname))
            continue
        b = bmap.get(addr)
        if b == 'ambiguous':
            ambig.append((addr, sym, fname, how))
        elif b in ('glide', 'd3d'):
            safe.append((addr, sym, fname, how, b))
        else:
            unknown.append((addr, sym, fname, how))

    print("already explicit          : %d" % len(have))
    print("migratable (build known)  : %d" % len(safe))
    print("REFUSED, ambiguous number : %d   <- must be settled by reading" % len(ambig))
    print("REFUSED, not in shared.csv: %d   <- no pairing to derive a build" % len(unknown))
    print("REFUSED, address invented from a 4-digit name: %d" % len(shortform))

    byfile = collections.Counter(x[2] for x in safe)
    print("\ntop files by migratable lines:")
    for f, n in byfile.most_common(10):
        print("   %-26s %d" % (f, n))

    if ambig:
        print("\nfirst refusals (ambiguous -- the number is valid in BOTH builds):")
        for addr, sym, fname, how in ambig[:8]:
            print("   0x%s  %-30s %s" % (addr, sym, fname))

    if '--apply' not in sys.argv:
        print("\n(--plan only; nothing written)")
        return

    # Write one line immediately above the definition the detector found.
    written = 0
    nodefn = []
    bysrc = collections.defaultdict(list)
    for addr, sym, fname, how, b in safe:
        bysrc[fname].append((addr, sym, b))
    only = None
    for a in sys.argv:
        if a.startswith('--only='):
            only = a.split('=',1)[1]
    for fname, rows in bysrc.items():
        if only and fname != only:
            continue
        paths = glob.glob('src/core/**/' + fname, recursive=True) \
              + glob.glob('include/**/' + fname, recursive=True) \
              + glob.glob('ports/macos/**/' + fname, recursive=True)
        if not paths:
            continue
        p = paths[0]
        s = open(p, errors='ignore').read()
        for addr, sym, b in rows:
            if ('@implements 0x%s' % addr) in s:
                continue
            # ANCHOR ONLY ON A DEFINITION, never a declaration. The first
            # run of this tool attached a claim to
            #     extern int BrXAtExit(void (*pfn)(void));
            # whose own comment two lines above says the PLATFORM's atexit is
            # used INSTEAD of porting it. Migrating that would have written a
            # false claim into the manifest permanently -- the legacy detector
            # counted an annotated declaration as a port, and copying its
            # answers wholesale copies its mistakes.
            #
            # A definition is followed by a brace, not a semicolon. `extern`
            # and `static` declarations are refused outright.
            m = None
            for cand in re.finditer(
                    r'^(?!\s*extern)([A-Za-z_][\w \*]*[\s\*]%s\s*\([^;{]*\)\s*\n?\s*\{)'
                    % re.escape(sym), s, re.M):
                m = cand
                break
            if not m:
                nodefn.append((addr, sym, fname))
                continue
            s = s[:m.start()] + '/* @implements 0x%s %s %s */\n' % (addr, b, sym) \
                + s[m.start():]
            written += 1
        open(p, 'w').write(s)
    print("\nwrote %d @implements lines" % written)
    print("REFUSED, no definition found: %d" % len(nodefn))
    print("  (declaration-only claims -- the legacy detector counted these as")
    print("   ports; several are functions the port deliberately does NOT")
    print("   implement, so they must be settled by reading, not migrated.)")
    for addr, sym, fname in nodefn[:8]:
        print("     0x%s  %-28s %s" % (addr, sym, fname))


if __name__ == '__main__':
    main()
