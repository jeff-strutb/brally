"""What IS this codebase?  Classify every function by what reaches it.

WHY THIS EXISTS

This port was begun from the middle -- menus, physics, renderer -- and the
game's entry point was not read until most of a week in. Coverage has been
reported five times against a denominator whose exclusions were decided from
filenames and string scans rather than from code. 39,973 bytes inside the main
DLL are still classed `unknown`, which means nobody can say what they are.

None of that is a base for accuracy work. You cannot decide whether a
transcription is faithful to a system you have not mapped.

So: build the call graph from the real roots and label every function by which
SUBSYSTEM owns it, where a subsystem is defined by a root the disassembly
actually names -- the entry point, the window procedure, the frame dispatch
table, the sound bank loader -- not by a guess about what a name suggests.

WHAT IT PRODUCES

  config/survey.csv     va, size, owner, depth, ncallers, is_leaf
  a summary on stdout   bytes per subsystem, and what is reachable from
                        nothing at all

WHAT "OWNER" MEANS, AND ITS LIMIT

A function is attributed to the root that reaches it at the SHORTEST depth. A
function reachable from two subsystems at equal depth is marked `shared`, not
silently assigned to whichever was scanned first -- that arbitrary choice is
exactly the failure that put 406 unrelated functions behind one address earlier
in this project.

Functions no root reaches are `unreached`, and that is a finding rather than a
gap to be filled in: it means either a root is missing from the list below, or
the code is genuinely dead, or it is reached only through a dispatch table this
walker cannot see. All three are worth knowing and they are not the same thing.

INDIRECT CALLS ARE INVISIBLE HERE. The game dispatches its per-frame work
through a function pointer at 0x106E79F4 and its UI through control vtables, so
a pure direct-call walk will under-report. Known dispatch tables are seeded as
roots explicitly, and the residue is reported rather than hidden.
"""
import sys, os, csv, struct, collections
sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

DLL = 'orig/BRGlide.dll'
MAP = 'config/functions_glide.csv'

# Roots the disassembly NAMES, with the evidence for each. Not a taxonomy
# invented to look tidy -- every one of these is an address something in the
# image points at.
ROOTS = [
    ('entry',      0x1001CC00, "RallyMain -- BRGlide.dll's only export"),
    ('mainloop',   0x10019730, "0x1001CD2E, the message loop"),
    ('window',     0x10019670, "0x1001CD17, RegisterClass/CreateWindowEx"),
    ('wndproc',    0x100194C0, "lpfnWndProc in the WNDCLASS at 0x100196D0"),
    ('frame',      0x1001CF80, "0x100197E2, jmp [0x100A9900 + state*4]"),
    ('state0',     0x1001CD70, "jump table 0x100A9900[0]"),
    ('state1',     0x1001CDA0, "jump table 0x100A9900[1]"),
    ('state2',     0x1001CDB0, "jump table 0x100A9900[2] -- the RUN state"),
    ('state3',     0x1001CDD0, "jump table 0x100A9900[3]"),
    ('state4',     0x1001CE20, "jump table 0x100A9900[4]"),
    ('gamestep',   0x1002E324, "call [0x106E79F4] -- the per-frame indirect"),
    ('cfgload',    0x10063060, "0x1001CD12, the binary config read"),
    ('cfgparse',   0x10007F40, "0x1001CCB0, the ini/command-line parse"),
    ('basedir',    0x10063860, "0x1001CCA5, the install directory"),
    ('dxprobe',    0x1001D8A0, "0x1001CC50, the DirectX requirement check"),
    ('startgate',  0x10007E80, "0x1001CC91, the single-instance guard"),
    ('sfxbank',    0x1006C290, "0x1001CD77 with 0, and the race init with 1"),
]

TERMS = {'ret', 'retf', 'iret', 'jmp', 'ud2'}


def load():
    p = pelib.load(DLL)
    text, va = p.text()
    sizes = {}
    for r in csv.DictReader(open(MAP)):
        sizes[int(r['va'], 16)] = int(r['size'])
    return p, text, va, sizes


def direct_calls(md, text, tva, addr, size, lo, hi):
    """Direct call/jmp targets inside one function."""
    off = addr - tva
    if off < 0 or off + size > len(text):
        return set()
    out = set()
    for ins in md.disasm(text[off:off + size], addr):
        if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
            try:
                t = int(ins.op_str, 16)
            except ValueError:
                continue
            if lo <= t < hi:
                out.add(t)
    return out


def main():
    p, text, tva, sizes = load()
    lo, hi = tva, tva + len(text)
    md = Cs(CS_ARCH_X86, CS_MODE_32)

    # POINTER-TAKEN FUNCTIONS ARE ROOTS TOO, and without them this survey is
    # worthless: a first run attributed 87% of the image to "unreached", which
    # said nothing about the game and everything about a walker that only
    # follows `call rel32`. This engine dispatches its frame through
    # [0x106E79F4], its UI through control vtables, and its RSP through a
    # 28-entry opcode table -- none of which is a direct call.
    #
    # Every relocated dword whose value lands in .text is a function pointer
    # someone stored. That is exactly how funcmap2 finds function starts, so
    # the same evidence is reused here rather than invented.
    relocs = {p.image_base + r for r in p.relocs}
    ptr_taken = set()
    for r in sorted(relocs):
        try:
            w = struct.unpack('<I', p.read(r, 4))[0]
        except Exception:
            continue
        if lo <= w < hi and w in sizes:
            ptr_taken.add(w)
    print("  pointer-taken function starts: %d" % len(ptr_taken))

    funcs = sorted(sizes)
    graph = {}
    for a in funcs:
        graph[a] = direct_calls(md, text, tva, a, sizes[a], lo, hi)

    callers = collections.Counter()
    for a, ts in graph.items():
        for t in ts:
            callers[t] += 1

    # A pointer-taken function is reachable from whoever STORED the pointer,
    # which this walker cannot see. Attribute it to `indirect` rather than
    # pretending it is unreachable -- and follow its callees, so the subsystem
    # hanging below a vtable entry is not lost with it.
    ROOTS.append(('indirect', None, "reached via a stored function pointer"))

    # BFS from each root independently, keeping the shortest depth per root.
    reach = {}                     # va -> {owner: depth}
    for name, root, _why in ROOTS:
        if root is None:
            seen = {}
            q = collections.deque()
            for a in ptr_taken:
                seen[a] = 0; q.append(a)
            while q:
                cur = q.popleft()
                for t in graph.get(cur, ()):
                    if t not in seen:
                        seen[t] = seen[cur] + 1
                        q.append(t)
            for va, d in seen.items():
                reach.setdefault(va, {})[name] = d
            continue
        if root not in graph:
            print("  WARNING: root %s 0x%08X is not in the function map" % (name, root))
            continue
        seen = {root: 0}
        q = collections.deque([root])
        while q:
            cur = q.popleft()
            for t in graph.get(cur, ()):
                if t not in seen:
                    seen[t] = seen[cur] + 1
                    q.append(t)
        for va, d in seen.items():
            reach.setdefault(va, {})[name] = d

    # ATTRIBUTION BY SHORTEST DEPTH DOES NOT WORK HERE, and the first two runs
    # proved it twice over. Direct-call-only put 87% in "unreached", which
    # described the walker rather than the game. Adding 1,037 pointer-taken
    # roots then put 92% in "indirect", because those roots all sit at depth 0
    # and win every tiebreak. Neither number was knowledge.
    #
    # So report what this evidence CAN support, and stop there:
    #
    #   direct    reachable from the named roots by `call rel32` alone
    #   indirect  not directly reachable, but its address is STORED somewhere,
    #             so a dispatch table or vtable can reach it
    #   dead      neither. Genuinely unreferenced, or reached only through a
    #             computed target this walker cannot see.
    #
    # Exclusive subsystem ownership is reported ONLY for functions exactly one
    # named root reaches. Anything several roots reach is `several`, because
    # picking one would be the arbitrary choice that put 406 unrelated
    # functions behind a single address earlier in this project.
    named = [n for n, r, _ in ROOTS if r is not None]
    rows, tier = [], collections.Counter()
    excl = collections.Counter()
    for a in funcs:
        owners = {k: v for k, v in reach.get(a, {}).items() if k in named}
        if owners:
            t = 'direct'
            own = list(owners)[0] if len(owners) == 1 else 'several'
        elif a in ptr_taken or 'indirect' in reach.get(a, {}):
            t, own = 'indirect', '-'
        else:
            t, own = 'dead', '-'
        tier[t] += sizes[a]
        if t == 'direct':
            excl[own] += sizes[a]
        rows.append({'va': '0x%08X' % a, 'size': sizes[a], 'owner': own,
                     'depth': min(owners.values()) if owners else -1,
                     'ncallers': callers[a], 'is_leaf': 1 if not graph[a] else 0,
                     'tier': t})

    os.makedirs('config', exist_ok=True)
    with open('config/survey.csv', 'w', newline='') as fh:
        w = csv.DictWriter(fh, fieldnames=['va', 'size', 'owner', 'depth',
                                           'ncallers', 'is_leaf', 'tier'])
        w.writeheader()
        for r in rows:
            w.writerow(r)

    total = sum(tier.values())
    print("\nHOW THE IMAGE IS REACHED")
    for k in ('direct', 'indirect', 'dead'):
        print("  %-9s %9d bytes  %5.1f%%" % (k, tier[k], 100.0 * tier[k] / total))
    print("  %-9s %9d" % ('TOTAL', total))

    print("\nEXCLUSIVE OWNERSHIP (only where ONE named root reaches it)")
    for k, v in sorted(excl.items(), key=lambda kv: -kv[1]):
        print("  %-10s %9d bytes" % (k, v))

    print("\nwrote config/survey.csv")


if __name__ == '__main__':
    main()
