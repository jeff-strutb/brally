"""Order the core decompilation: recover module boundaries and a work order.

MSVC 5 emits each translation unit's functions contiguously, so a run of
adjacent functions with dense internal calls and sparse external ones is very
likely one original .c file. We cluster on that, then compute:

  * leaf functions  -- call nothing else in .text, so they decompile with no
    prerequisites and are the correct starting point;
  * call depth from RallyMain -- how deep in the engine a function sits;
  * a topological work order -- callees before callers, so every function is
    written against already-decompiled dependencies.

Writes config/modules.csv and config/workorder.csv.

Runs against BRGlide.dll (rule 0). It pointed at BRD3D.dll until 2026-09-03,
which also crashed it: config/shared.csv has no plain `va` column, it has
`d3d_va` and `glide_va`.  Both are fixed here; `BR_REF` still overrides.
"""
import sys, os, csv, struct, collections
sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DLL = os.environ.get('BR_REF', os.path.join(ROOT, 'orig', 'BRGlide.dll'))
# Glide's own function map, to match the Glide binary above. functions.csv is
# the D3D-keyed map: pairing it with these bytes disassembles the wrong bytes
# at a right-looking address, which is the exact failure rule 0 describes.
FUNCS = os.environ.get('BR_MAP', os.path.join(ROOT, 'config',
                                              'functions_glide.csv'))


def _p(*a):
    return os.path.join(ROOT, *a)


def call_graph(text, text_va, funcs):
    sizes = dict(funcs)
    g = collections.defaultdict(set)
    for va, size in funcs:
        b = text[va - text_va: va - text_va + size]
        i = 0
        while i < len(b) - 5:
            if b[i] == 0xE8:
                rel = struct.unpack('<i', b[i + 1:i + 5])[0]
                t = va + i + 5 + rel
                if t in sizes and t != va:
                    g[va].add(t)
                i += 5
            else:
                i += 1
    return g


def main():
    p = pelib.load(DLL)
    text, text_va = p.text()
    funcs = [(int(r['va'], 16), int(r['size']))
             for r in csv.DictReader(open(FUNCS))]
    funcs.sort()
    sizes = dict(funcs)
    order = [va for va, _ in funcs]
    index = {va: i for i, va in enumerate(order)}

    # shared.csv is keyed d3d_va -> glide_va; we are in GLIDE space here, so
    # class is looked up by the glide column.
    cls = {}
    sh = _p('config', 'shared.csv')
    if os.path.exists(sh):
        for r in csv.DictReader(open(sh)):
            g = (r.get('glide_va') or '').strip()
            if g:
                cls[int(g, 16)] = r.get('class', '')
    names = {}
    nm = _p('config', 'names.csv')
    if os.path.exists(nm):
        for r in csv.DictReader(open(nm)):
            names[int(r['va'], 16)] = r['name']

    g = call_graph(text, text_va, funcs)
    callers = collections.defaultdict(set)
    for a, bs in g.items():
        for b in bs:
            callers[b].add(a)

    # ---- module clustering ------------------------------------------------
    # Split between adjacent functions when the call locality breaks: neither
    # calls the other and neither shares a callee. Cheap, and it lines up with
    # translation-unit boundaries because MSVC emits objects contiguously.
    mods = []
    cur = [order[0]]
    for i in range(1, len(order)):
        a, b = order[i - 1], order[i]
        linked = (b in g.get(a, ())) or (a in g.get(b, ())) \
                 or bool(g.get(a, set()) & g.get(b, set())) \
                 or bool(callers.get(a, set()) & callers.get(b, set()))
        if linked:
            cur.append(b)
        else:
            mods.append(cur)
            cur = [b]
    mods.append(cur)

    # ---- depth from RallyMain --------------------------------------------
    entry = next((va for va, n in names.items() if n == 'RallyMain'), None)
    depth = {}
    if entry is not None:
        depth[entry] = 0
        q = collections.deque([entry])
        while q:
            v = q.popleft()
            for w in g.get(v, ()):
                if w not in depth:
                    depth[w] = depth[v] + 1
                    q.append(w)

    # ---- topological work order (callees first) --------------------------
    # Iteratively peel functions whose callees are all already placed; the
    # remainder are in cycles (mutual recursion) and get appended after.
    placed, out = set(), []
    remaining = set(order)
    while True:
        ready = sorted(v for v in remaining if not (g.get(v, set()) - placed))
        if not ready:
            break
        for v in ready:
            out.append(v)
            placed.add(v)
        remaining -= set(ready)
    cyclic = sorted(remaining)

    with open(_p('config','modules.csv'), 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['module', 'start', 'end', 'functions', 'bytes', 'shared', 'names'])
        for i, m in enumerate(mods):
            nb = sum(sizes[v] for v in m)
            ns = sum(1 for v in m if cls.get(v) == 'shared')
            nm = [names[v] for v in m if v in names]
            w.writerow([i, '0x%08X' % m[0], '0x%08X' % m[-1], len(m), nb, ns,
                        '|'.join(nm)])

    with open(_p('config','workorder.csv'), 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['rank', 'va', 'size', 'class', 'depth', 'callees', 'callers', 'name'])
        for i, va in enumerate(out + cyclic):
            w.writerow([i, '0x%08X' % va, sizes[va], cls.get(va, '?'),
                        depth.get(va, ''), len(g.get(va, ())), len(callers.get(va, ())),
                        names.get(va, '')])

    leaves = [v for v in order if not g.get(v)]
    shared = [v for v in order if cls.get(v) == 'shared']
    shared_leaves = [v for v in leaves if cls.get(v) == 'shared']
    reach = [v for v in order if v in depth]

    print("functions              %d" % len(order))
    print("  shared (core target) %d" % len(shared))
    print("  leaves (no callees)  %d   of which shared: %d" % (len(leaves), len(shared_leaves)))
    print("  reachable from RallyMain %d (max depth %d)"
          % (len(reach), max(depth.values()) if depth else -1))
    print("  in call cycles       %d" % len(cyclic))
    print()
    print("modules (contiguous clusters): %d" % len(mods))
    big = sorted(mods, key=lambda m: -sum(sizes[v] for v in m))[:12]
    print("  largest:")
    for m in big:
        nm = [names[v] for v in m if v in names]
        print("    %08X-%08X  %3d fns  %6d bytes  %s"
              % (m[0], m[-1], len(m), sum(sizes[v] for v in m), ','.join(nm[:3])))
    print()
    print("first 15 of the work order (callees before callers):")
    for i, va in enumerate(out[:15]):
        print("    %2d  %08X  %5d bytes  %-8s %s"
              % (i, va, sizes[va], cls.get(va, '?'), names.get(va, '')))


if __name__ == '__main__':
    main()
