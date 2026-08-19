"""Which UI hook slots does a builder STORE, and which are actually INSTALLED?

WHY THIS EXISTS

Twice in one session this project dispatched work off a hand-rolled grep that
answered this question wrongly, and both times the answer was wrong in the same
direction: it under-counted what the tree already had.

  - The first scan looked for installers only in slice8_84.c and slice8_85.c.
    It reported 48 of 108 slots filled and 60 NULL. The real numbers were 99
    and 9. slice7_81.c, slice2_24.c, slice7_80.c and the host all install hooks
    too, and none of them were being looked at.

  - The second scan tried to answer "is this address already ported" by
    matching a declaration ending in `/* 0xADDR */` or a definition preceded by
    a banner comment. Modules that name a function after its address instead
    -- BrUiHook81_100450F0, BrMenuCap0730, BrOptToggle2F7C_C -- were invisible
    to it, so functions that had been ported for weeks were reported missing.

Two agents were then briefed to port functions this tree already had. One
found 8 of its 10 addresses already ported AND wired; the other found 6 of 8.
Re-porting them would not have been merely wasteful: slice7_81.c owns the
storage for thirteen globals, and a second leave routine would have cleared
the wrong word.

The rule "grep BOTH builds, and check the whole tree" was already written down.
Prose does not get run. This does.

WHAT IT REPORTS

  stored     a builder assigns pH->pXXXXXXXX into one of a control's six hook
             slots. This is DEMAND: if the slot is NULL at run time the
             control's corresponding behaviour silently does not happen.
  installed  anything anywhere assigns that slot -- `->pXXXX =` or `.pXXXX =`,
             in port/src/ or port/host/, under ANY naming convention.
  ported     a function body carrying that address exists somewhere, found by
             three independent conventions rather than one.

WHICH SLOT a builder stores into matters and is reported, because it is easy
to assume every hook is an action. It is not: +0x08 is the ACTION (0x10048180
calls it when the ACTIVATE bit is set), while +0x04 is the per-frame caption
and text setter. A brief that calls a pfn04 caption setter an "action hook"
sends an agent looking for a screen transition that was never there. That
mistake was made here too.

Usage:  hookaudit.py            # full audit
        hookaudit.py --missing  # just the slots still NULL, for a work queue
"""
import re, sys, glob, os, collections

BUILDERS = ('src/core/slice6_71.c', 'src/core/slice6_72.c', 'src/core/slice6_73.c')

# The six hook slots on a control, plus the page's. Named so the report can say
# WHICH behaviour a NULL slot costs, rather than just that one is missing.
SLOT_MEANING = {
    '04': 'per-frame (caption/text setter)',
    '08': 'ACTION (called when ACTIVATE is set)',
    '0C': 'not-current (e.g. sprfont recolour)',
    '10': 'measure/other',
    '14': 'pre-frame gate (0 aborts the page)',
    '18': 'post-frame (0 aborts the page)',
}


def demand():
    """slot -> {which hook offsets it is stored into, and how many times}."""
    out = collections.defaultdict(lambda: collections.Counter())
    for f in BUILDERS:
        if not os.path.exists(f):
            continue
        s = open(f, errors='ignore').read()
        for m in re.finditer(r'->pfn(04|08|0C|10|14|18)\s*=\s*pH->(p[0-9A-Fa-f]+)', s):
            out[m.group(2)][m.group(1)] += 1
    return out


def _called_installers():
    """Installer functions the HOST actually calls.

    THIS IS THE WHOLE POINT, AND THE FIRST VERSION OF THIS FILE GOT IT WRONG.

    v1 counted a slot as installed if any line anywhere assigned it. It
    reported 99 of 108 filled. The runtime truth at that moment was 3 of 51:
    BrUiHook85Install, BrUiHook81Install, BrUiOptInstall73 and BrUiOptInstall72
    all existed, all assigned their slots, and NOTHING CALLED ANY OF THEM. Four
    working installers sat in port/src with no caller, and that -- not the
    unported functions -- was the single largest reason Enter did nothing.

    An assignment inside a function nobody invokes fills nothing. So resolve
    which installers are actually reached from port/host/, and only credit
    slots assigned inside those.
    """
    called = set()
    host = ""
    for f in glob.glob('src/backends/macos/*.c'):
        host += open(f, errors='ignore').read()
    for m in re.finditer(r'\b(\w*(?:Install|Wire)\w*)\s*\(', host):
        called.add(m.group(1))
    # One hop: a host-called wiring function may call further installers.
    for _ in range(3):
        for f in glob.glob('src/core/*.c') + glob.glob('src/backends/macos/*.c'):
            s = open(f, errors='ignore').read()
            for fn in re.findall(r'^(?:void|int|int32_t)\s+(\w+)\s*\([^;]*\)\s*\n?\s*\{', s, re.M):
                if fn not in called:
                    continue
                body = s[s.index(fn):]
                for m in re.finditer(r'\b(\w*(?:Install|Wire)\w*)\s*\(', body[:8000]):
                    called.add(m.group(1))
    return called


def installed():
    """Hook slots assigned inside an installer the host actually calls.

    Both `->pXXXX =` and `.pXXXX =` -- the host writes g_navHooks.p10045AF0
    with a dot, and the first version of this check used only `->`, which is
    why two host-installed slots were reported NULL.
    """
    called = _called_installers()
    out = {}
    for f in sorted(glob.glob('src/core/*.c') + glob.glob('src/backends/macos/*.c')):
        s = open(f, errors='ignore').read()
        # split into function bodies so a slot is credited only when the
        # enclosing installer is reachable
        for m in re.finditer(r'^(?:void|int|int32_t|static\s+\w+)\s+(\w+)\s*\([^;]*\)\s*\n?\s*\{',
                             s, re.M):
            fn = m.group(1)
            end = s.find('\n}', m.end())
            body = s[m.end():end if end > 0 else len(s)]
            reachable = (fn in called) or os.path.dirname(f).endswith('host')
            for a in re.finditer(r'(?:->|\.)(p[0-9A-Fa-f]{8})\s*=', body):
                if reachable:
                    out.setdefault(a.group(1), []).append(os.path.basename(f))
    return out


def ported():
    """Addresses with a real body, found three ways instead of one.

    Any single convention misses whole modules -- that is the bug this file
    exists to prevent, so no one of these is trusted alone.
    """
    out = {}

    def add(addr, name, f):
        out.setdefault(addr.upper(), (name, os.path.basename(f)))

    for f in glob.glob('include/*.h'):
        for ln in open(f, errors='ignore'):
            # 1. declaration annotated with its address
            m = re.search(r'^\s*[A-Za-z_][\w \*]*\s(\w+)\s*\([^;]*\)\s*;\s*/\*\s*0x([0-9A-Fa-f]{8})', ln)
            if m:
                add(m.group(2), m.group(1), f)
    for f in glob.glob('src/core/*.c'):
        s = open(f, errors='ignore').read()
        # 2. definition under a banner naming the address
        for m in re.finditer(r'/\*\s*0x([0-9A-Fa-f]{8})[^*]*\*/\s*\n\s*(?:static\s+)?[A-Za-z_][\w \*]*\s(\w+)\s*\(', s):
            add(m.group(1), m.group(2), f)
        # 3. the address embedded in the FUNCTION NAME -- BrUiHook81_100450F0,
        #    BrMenuCap0730, BrOptToggle2F7C_C. Invisible to 1 and 2, and this
        #    is precisely the convention that made two briefs wrong.
        for m in re.finditer(r'^[A-Za-z_][\w \*]*\s(\w*?([0-9A-Fa-f]{4,8})\w*)\s*\([^;]*\)\s*\n?\s*\{', s, re.M):
            frag = m.group(2).upper()
            if len(frag) == 8:
                add(frag, m.group(1), f)
            elif len(frag) >= 4:
                add(('1000' + frag)[-8:], m.group(1), f)
                add(('100' + frag).ljust(8, '0')[:8], m.group(1), f)
    return out


def main():
    only_missing = '--missing' in sys.argv
    dem, inst, port = demand(), installed(), ported()

    miss = []
    for slot, slots_used in sorted(dem.items()):
        addr = slot[1:].upper()
        where = inst.get(slot)
        if where:
            if not only_missing:
                pass
            continue
        miss.append((addr, slots_used, port.get(addr)))

    print("hook slots builders store : %d" % len(dem))
    print("slots installed           : %d" % (len(dem) - len(miss)))
    print("slots STILL NULL          : %d" % len(miss))
    if not miss:
        print("\nnothing missing.")
        return
    print("\nSTILL NULL -- what each costs, and whether the body already exists:\n")
    for addr, slots_used, have in sorted(miss):
        kinds = ", ".join("+0x%s %s" % (k, SLOT_MEANING.get(k, '?'))
                          for k in sorted(slots_used))
        print("  0x%s  %s" % (addr, kinds))
        if have:
            print("      ALREADY PORTED as %s (%s) -- WIRE IT, do not re-port."
                  % (have[0], have[1]))
        else:
            print("      no body found -- needs transcribing "
                  "(confirm with whereis.py first)")


if __name__ == '__main__':
    main()
