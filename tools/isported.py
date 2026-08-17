"""Is this address already transcribed in port/?  Answer before writing code.

WHY THIS EXISTS

Five times in this project, work was started on a function the tree already
had. Three agent runs were spent re-deriving ported code; one packet found 8 of
its 10 addresses already ported AND wired; another found 6 of 8. Most recently
br_boot.c gave 0x1002E324 a "frontier" entry -- declaring the game's frame
dispatcher missing while a correct transcription of it sat in br_gamestep.c,
eleven bytes long, under the name BrGameStepInvoke.

The cost is not just the wasted run. A duplicate transcription competes with
the original for the same storage: slice7_81.c owns thirteen globals, and a
second "leave" routine would have cleared the wrong word. A duplicate that
links and runs is worse than one that does not.

The rule was written down. Prose does not get run. This does.

WHY GREP IS NOT ENOUGH, WHICH IS THE ACTUAL TRAP

Grepping the address finds MENTIONS -- a comment, a note in a header, an entry
in a table of things to do. All three of those look exactly like a port and are
not one. And modules here name functions three different ways:

    int32_t BrFoo(void);            /* 0x10041300 */      annotated declaration
    /* 0x10041300 -- what it does */                      banner over a body
    int32_t BrMenuText1300(...)                           address IN THE NAME

A scan that knows only one convention reports the other two as missing, and it
fails toward "missing", which is the dangerous direction: a false "already
ported" is caught the moment someone looks for the function, while a false
"missing" ends as a duplicate that links, runs and quietly fights the original.

So this checks all three, and separates a DEFINITION from a MENTION.

Usage:  isported.py 0x1002E324 0x10019730 ...
        isported.py --chain 0x1001CC00      # the address and everything it calls
"""
import sys, os, re, csv, glob

sys.path.insert(0, os.path.dirname(__file__))

SRC = 'port/src/*.c'
HDR = 'port/include/*.h'

# Files that DECLARE the frontier rather than implement the game. A counted
# no-op named after an address is not a transcription of it, and reporting one
# as "ported" is worse than reporting nothing: it tells the next reader the
# work is done. Caught by validation -- 0x10056260 came back "PORTED as
# BrBootFrontier_10056260", which is this project's own stub.
FRONTIER_FILES = ('br_bootfrontier.c', 'br_stubs.c')


def definitions():
    """addr -> (symbol, file). Three conventions, none trusted alone."""
    out = {}

    def add(addr, name, f, how):
        if os.path.basename(f) in FRONTIER_FILES:
            return                      # a stub is not a port
        out.setdefault(addr.upper(), (name, os.path.basename(f), how))

    for f in glob.glob(HDR):
        for ln in open(f, errors='ignore'):
            m = re.search(r'^\s*[A-Za-z_][\w \*]*\s(\w+)\s*\([^;]*\)\s*;\s*/\*\s*0x([0-9A-Fa-f]{8})', ln)
            if m:
                add(m.group(2), m.group(1), f, 'annotated declaration')
    for f in glob.glob(SRC):
        s = open(f, errors='ignore').read()
        # NON-GREEDY TO THE COMMENT TERMINATOR, and this was wrong first time.
        # The original pattern used [^*]* to reach the closing */, which cannot
        # cross a line -- and every banner in this tree is multi-line with a
        # leading ' * ' on each row. So it matched almost nothing, and the tool
        # reported 0x1002E324 (BrGameStepInvoke, a correct transcription eleven
        # bytes long) as NOT PORTED. That is the exact false-negative this file
        # was written to prevent, produced by the file itself, and it was caught
        # only because the first thing done with it was to check it against a
        # known answer. Do that with every detector here.
        # Two constraints, and the second one was learned by getting it wrong
        # twice in a row on the same address:
        #   1. the address must sit on the SAME LINE as the comment opener,
        #      which is the convention here ("/* 0x1002E324 -- ...").
        #   2. the body of the comment must not contain a closing */, i.e. stop
        #      at the FIRST terminator. Without that, `.*?` with re.S happily
        #      ran from a passing MENTION of an address inside one banner,
        #      through the end of that banner, and attached the address to
        #      whatever function came next -- reporting 0x1002E324 as ported
        #      "as BrAppStateColdInit". The right verdict for the wrong reason
        #      is still a wrong tool, and it would have mis-attributed every
        #      address discussed in prose anywhere in this tree.
        #   3. the address must be the FIRST token after the opener. With a
        #      greedy [^\n]{0,40} prefix the engine preferred the LONGER match
        #      and captured 0x106E79F4 -- an address mentioned later on the very
        #      same line, inside `call dword ptr [0x106E79F4]` -- instead of the
        #      0x1002E324 the banner is about. Three wrong answers on one
        #      address, each from a different regex subtlety.
        #   4. the address need only be the FIRST ADDRESS in the banner, not
        #      the first token. Requiring `/* 0x...` missed every banner that
        #      opens with a rule line:
        #          /* ----------------------------------------
        #           * 0x1001D8A0 -- what it does
        #      which is a common style here, so a finished transcription read
        #      as NOT PORTED. Found by an agent whose own new module the tool
        #      failed to see. The prefix is bounded and must not contain an
        #      earlier 0xXXXXXXXX, which keeps rule (3)'s guarantee: the
        #      address captured is the one the banner is about.
        for m in re.finditer(r'/\*(?:(?!\*/)(?!0x[0-9A-Fa-f]{8}).){0,200}?'
                             r'0x([0-9A-Fa-f]{8})'
                             r'(?:(?!\*/).)*?\*/\s*\n\s*(?:static\s+)?'
                             r'[A-Za-z_][\w \*]*\s(\w+)\s*\(', s, re.S):
            add(m.group(1), m.group(2), f, 'banner over a body')
        for m in re.finditer(r'^[A-Za-z_][\w \*]*\s(\w*?([0-9A-Fa-f]{8})\w*)\s*'
                             r'\([^;]*\)\s*\n?\s*\{', s, re.M):
            add(m.group(2), m.group(1), f, 'address in the name')
        # short forms: BrMenuText1300 for 0x10041300, BrOpt37D0 for 0x100437D0
        for m in re.finditer(r'^[A-Za-z_][\w \*]*\s(\w*?([0-9A-Fa-f]{4})\w*)\s*'
                             r'\([^;]*\)\s*\n?\s*\{', s, re.M):
            frag = m.group(2).upper()
            add(('1000' + frag)[-8:], m.group(1), f, 'short address in the name')
            add(('100' + frag).ljust(8, '0')[:8], m.group(1), f, 'short address in the name')
    return out


def mentions(addr):
    out = []
    pat = '0x%08X' % addr
    for f in glob.glob(SRC) + glob.glob(HDR):
        for i, ln in enumerate(open(f, errors='ignore'), 1):
            if pat.lower() in ln.lower():
                out.append((os.path.basename(f), i, ln.strip()[:88]))
    return out


def size_of(addr):
    for path in ('config/functions_glide.csv', 'config/functions.csv'):
        if not os.path.exists(path):
            continue
        for r in csv.DictReader(open(path)):
            try:
                if int(r['va'], 16) == addr:
                    return int(r['size'])
            except Exception:
                pass
    return None


def report(addr, defs):
    key = '%08X' % addr
    print('0x%s   %s bytes' % (key, size_of(addr) if size_of(addr) else '?'))
    if key in defs:
        name, f, how = defs[key]
        print('  PORTED as %s  (%s, found by %s)' % (name, f, how))
        print('  -> DO NOT re-transcribe. Wire it, or extend it.')
        return True
    ms = mentions(addr)
    if ms:
        print('  not defined anywhere -- but MENTIONED %d time(s):' % len(ms))
        for f, i, ln in ms[:4]:
            print('     %s:%d  %s' % (f, i, ln))
        print('  -> a mention is not a port. Check each before assuming either way.')
    else:
        print('  NOT PORTED, and not mentioned. Clean target.')
    return False


def call_targets(addr):
    """Direct call targets inside one function, via the disassembler."""
    import subprocess
    py = sys.executable
    out = subprocess.run([py, os.path.join(os.path.dirname(__file__), 'dumpasm.py'),
                          hex(addr)], capture_output=True, text=True).stdout
    return sorted({int(m, 16) for m in
                   re.findall(r'call\s+0x([0-9a-f]{8})', out)})


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not args:
        raise SystemExit(__doc__)
    defs = definitions()
    if '--chain' in sys.argv:
        root = int(args[0], 16)
        print('=== %s and its direct callees ===\n' % hex(root))
        report(root, defs)
        print()
        n = 0
        for t in call_targets(root):
            if report(t, defs):
                n += 1
            print()
        print('%d of the callees are already ported.' % n)
        return
    for a in args:
        report(int(a, 16), defs)
        print()


if __name__ == '__main__':
    main()
