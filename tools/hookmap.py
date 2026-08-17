"""Who INSTALLS each function pointer, and where is it called through?

WHY THIS EXISTS

tools/survey.py established that 86% of BRGlide.dll is unreachable by following
calls: the engine stores roughly 1,148 individual function pointers into
objects at run time and dispatches through them. A call graph therefore cannot
say what a subsystem is -- it answers "what does this call", never "who will
install this, and when".

This closes that gap from the other side. For every function whose address is
taken, find the INSTRUCTION THAT TAKES IT, and hence the function that performs
the installation. `F installs G` is the edge the call graph is missing, and
together the two give the real module boundaries.

HOW AN INSTALL LOOKS IN THIS BINARY

MSVC emits the address as a 32-bit immediate that the loader must relocate:

    mov  dword ptr [ecx + 8], 0x1004A580     ; store into a control's slot
    mov  dword ptr [0x10AA2904], 0x1003E7A0  ; store into a global
    push 0x100194C0                          ; hand it to RegisterClassA

Every one of those immediates sits at a relocation site inside .text. So the
set of "address-taking instructions" is exactly: relocation entries that fall
within a function's extent, whose stored dword is itself a .text function.

That is evidence, not pattern-matching -- the relocation table is the linker
telling us which dwords are addresses. It is the same evidence funcmap2 uses to
find function starts, read the other way round.

WHAT IT CANNOT SEE, stated because the survey's first two attempts both
produced confident numbers that described the tool rather than the game:

  - a pointer copied from one slot to another at run time. The install edge
    points at whoever first materialised the constant, not at the copier.
  - a pointer computed rather than taken (`base + index*4`). There is at least
    one such table in this image, the 256-entry array at 0x100A9A58.
  - which of several stores actually executes on a given run.

So `installs` is a superset of "could install" and a subset of "does install at
run time". It is a structural map, not a trace.

Usage:  hookmap.py                 # summary + config/hookmap.csv
        hookmap.py 0x1004A580      # who installs this, and who calls it
"""
import sys, os, csv, struct, collections
sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib

DLL = 'orig/BRGlide.dll'
MAP = 'config/functions_glide.csv'


def load():
    p = pelib.load(DLL)
    text, tva = p.text()
    sizes = {}
    for r in csv.DictReader(open(MAP)):
        sizes[int(r['va'], 16)] = int(r['size'])
    return p, text, tva, sizes


def containing(starts, sizes, addr):
    """The function whose extent covers addr, or None."""
    import bisect
    i = bisect.bisect_right(starts, addr) - 1
    if i < 0:
        return None
    s = starts[i]
    return s if addr < s + sizes[s] else None


def build():
    p, text, tva, sizes = load()
    lo, hi = tva, tva + len(text)
    starts = sorted(sizes)

    installs = collections.defaultdict(set)   # target -> {installer, ...}
    sites = collections.defaultdict(list)     # target -> [reloc site, ...]

    for r in sorted(p.image_base + x for x in p.relocs):
        if not (lo <= r < hi):
            continue                          # only immediates inside code
        try:
            w = struct.unpack('<I', p.read(r, 4))[0]
        except Exception:
            continue
        if not (lo <= w < hi) or w not in sizes:
            continue
        owner = containing(starts, sizes, r)
        if owner is None or owner == w:
            continue                          # ignore a self-reference
        installs[w].add(owner)
        sites[w].append(r)
    return p, sizes, starts, installs, sites


def main():
    p, sizes, starts, installs, sites = build()

    if len(sys.argv) > 1:
        for a in sys.argv[1:]:
            t = int(a, 16)
            print("0x%08X  %s bytes" % (t, sizes.get(t, '?')))
            ins = sorted(installs.get(t, ()))
            if not ins:
                print("  installed by: NOBODY -- its address is never taken in code.")
                print("    Either it is called directly, or the pointer is computed")
                print("    rather than taken (see the module banner).")
            else:
                print("  installed by %d function(s):" % len(ins))
                for f in ins:
                    print("     0x%08X  (%d bytes)" % (f, sizes.get(f, 0)))
            print()
        return

    os.makedirs('config', exist_ok=True)
    with open('config/hookmap.csv', 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['target', 'size', 'n_installers', 'installers'])
        for t in sorted(installs):
            ins = sorted(installs[t])
            w.writerow(['0x%08X' % t, sizes.get(t, 0), len(ins),
                        ' '.join('0x%08X' % f for f in ins)])

    n_targets = len(installs)
    by_installer = collections.Counter()
    for t, fs in installs.items():
        for f in fs:
            by_installer[f] += 1
    multi = sum(1 for t in installs if len(installs[t]) > 1)

    print("functions whose address is taken in code : %d" % n_targets)
    print("  installed from exactly one function    : %d" % (n_targets - multi))
    print("  installed from several                 : %d" % multi)
    print("\nTHE INSTALLERS -- functions that hand out the most pointers.")
    print("These are the engine's wiring points, and they are what a call graph")
    print("cannot find. Each is where a subsystem is assembled.\n")
    print("  %-12s %6s  %s" % ("installer", "hooks", "size"))
    for f, n in by_installer.most_common(20):
        print("  0x%08X %6d  %d bytes" % (f, n, sizes.get(f, 0)))
    print("\nwrote config/hookmap.csv")


if __name__ == '__main__':
    main()
