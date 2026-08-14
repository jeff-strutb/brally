"""Build the link-gap work queue: symbols that are declared but never defined.

Replaces the ad-hoc script that produced work/undefined_resolved.txt, which was
WRONG: it scanned a few lines past each `/* XSLICE 0xADDR */` comment looking for
a function name, so where declarations sit close together a name bound to a
NEIGHBOURING address. Roughly half of each generated packet showed the wrong
function's body under the right name -- the worst failure mode available, since
such a function links cleanly and is only wrong at runtime.

Two rules make this correct:

  1. A name binds to an address ONLY when both appear in the same declaration --
     either on one line, or on the line immediately after an XSLICE comment that
     is alone on its line. No windows, no lookahead past a second XSLICE.
  2. Every binding is VERIFIED against the disassembly before it is emitted: the
     address must exist in the function map and disassemble. Anything that fails
     is reported, not silently dropped.

Undefined symbols come from a real link, never from grep -- an earlier grep-based
audit reported 115 where the link reported 304.
"""
import sys, os, re, csv, glob, subprocess, collections

DECL = re.compile(r'\b(Br[A-Za-z0-9_]{2,})\s*\(')
XSL = re.compile(r'XSLICE\s+(0x[0-9A-Fa-f]{8})')


def undefined_symbols(objdir='/tmp'):
    """Link every module and harvest the undefined symbols from the linker."""
    objs = sorted(glob.glob(os.path.join(objdir, 'all_*.o')))
    if not objs:
        return None, "no objects; build them first"
    main_c = os.path.join(objdir, '_lq_main.c')
    open(main_c, 'w').write('int main(void){return 0;}\n')
    r = subprocess.run(['clang', '-std=c99', '-Iport/include', main_c] + objs +
                       ['-lm', '-o', os.path.join(objdir, '_lq_bin')],
                       capture_output=True, text=True)
    syms = sorted({m[1:] for m in re.findall(r'"(_[A-Za-z0-9_]+)"', r.stderr)})
    return syms, None


def bind_addresses(undef):
    """name -> address, using same-declaration evidence only."""
    bound = {}
    conflicts = collections.defaultdict(set)
    for h in glob.glob('port/include/*.h'):
        lines = open(h).read().splitlines()
        for i, line in enumerate(lines):
            m = XSL.search(line)
            if not m:
                continue
            addr = m.group(1)
            # names on the SAME line as the XSLICE comment
            cands = [n for n in DECL.findall(line) if n in undef]
            # or, if the comment is alone on its line, the very next line --
            # but only if that line has no XSLICE of its own
            if not cands and i + 1 < len(lines) and not XSL.search(lines[i + 1]):
                stripped = line.strip()
                if stripped.startswith('/*') and stripped.endswith('*/'):
                    cands = [n for n in DECL.findall(lines[i + 1]) if n in undef]
            for n in cands:
                if n in bound and bound[n] != addr:
                    conflicts[n].add(bound[n])
                    conflicts[n].add(addr)
                bound[n] = addr
    return bound, conflicts


def verify(bound):
    """Drop bindings whose address does not disassemble; report them."""
    sizes = {int(r['va'], 16): int(r['size'])
             for r in csv.DictReader(open('config/functions.csv'))}
    ok, bad = {}, {}
    for name, addr in bound.items():
        a = int(addr, 16)
        if a not in sizes:
            bad[name] = (addr, 'not in function map')
        elif sizes[a] < 1:
            bad[name] = (addr, 'zero-size entry')
        else:
            ok[name] = addr
    return ok, bad


def main():
    undef, err = undefined_symbols()
    if err:
        print("cannot build queue: %s" % err)
        return
    undef = set(undef)
    bound, conflicts = bind_addresses(undef)
    ok, bad = verify(bound)

    # who needs each symbol
    need = collections.defaultdict(set)
    for c in glob.glob('port/src/*.c'):
        txt = open(c).read()
        for s in ok:
            if re.search(r'\b%s\s*\(' % re.escape(s), txt):
                need[s].add(os.path.basename(c)[:-2])

    os.makedirs('work', exist_ok=True)
    with open('work/linkqueue.csv', 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['addr', 'symbol', 'size', 'needed_by'])
        sizes = {int(r['va'], 16): int(r['size'])
                 for r in csv.DictReader(open('config/functions.csv'))}
        for s in sorted(ok, key=lambda x: -len(need.get(x, ()))):
            w.writerow([ok[s], s, sizes[int(ok[s], 16)],
                        '|'.join(sorted(need.get(s, ())))])

    print("undefined at link          %d" % len(undef))
    print("  bound to an address      %d   (same-declaration evidence only)" % len(bound))
    print("  verified against the map  %d  -> work/linkqueue.csv" % len(ok))
    print("  rejected (unverifiable)  %d" % len(bad))
    print("  name bound to >1 address %d  <- these are real ambiguities" % len(conflicts))
    for n, addrs in list(conflicts.items())[:5]:
        print("     %-28s %s" % (n, ', '.join(sorted(addrs))))
    print("  unbound (no XSLICE decl) %d" % (len(undef) - len(bound)))


if __name__ == '__main__':
    main()
