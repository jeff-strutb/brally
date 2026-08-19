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

# RECURSIVE, and it was not. port/src/ is no longer flat -- port/src/README.md
# files modules under a responsibility folder and the sliceN_MM.c at the top
# level are the shrinking backlog, not the tree. A flat glob therefore saw 63
# files out of 123 and could not see ANY finished module: br_boot.c's
# BrRallyMain (0x1001CC00) and br_basedir.c's BrBaseDirInit (0x10063860) both
# came back "not defined anywhere", which is the precise false negative this
# tool exists to prevent and the precise reason this project keeps restarting
# work it already has. It also meant FRONTIER_FILES never fired, since
# br_bootfrontier.c is itself inside a folder.
#
# Note the pattern must be '**/*.c' with recursive=True: '**' matches zero or
# more directories, so it covers the top-level files too and the flat glob
# does not need to be kept alongside it.
# recursive=True IS REQUIRED at every call site. Without it, `**` behaves
# exactly like `*` -- so this pattern saw 61 of 124 modules, silently missing
# every loose sliceN_MM.c at the top level as well as anything nested deeper.
# A path glob that looks recursive and is not is the same class of failure as
# the non-recursive glob it replaced: the tool answers confidently about a tree
# it cannot see.
SRC = 'src/core/**/*.c'
HDR = 'include/**/*.h'

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

    for f in glob.glob(HDR, recursive=True):
        for ln in open(f, errors='ignore'):
            m = re.search(r'^\s*[A-Za-z_][\w \*]*[\s\*](\w+)\s*\([^;]*\)\s*;\s*/\*\s*0x([0-9A-Fa-f]{8})', ln)
            if m:
                add(m.group(2), m.group(1), f, 'annotated declaration')
    for f in glob.glob(SRC, recursive=True):
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
        #   5. the character before the NAME may be `*`, not only whitespace.
        #      All four definition patterns here ended `[\w \*]*\s(\w+)\s*\(`,
        #      which cannot match this tree's dominant pointer style:
        #          BrCtrlCfg *BrCtrlCfgCtor(BrCtrlCfg *pThis)
        #      because there is no whitespace immediately before the name. So
        #      `void BrCtrlCfgInit` was found and `BrCtrlCfg *BrCtrlCfgCtor`
        #      three lines away was not -- and the --chain output then called
        #      0x10062B00 and 0x10062E50 "NOT PORTED ... Clean target" while
        #      tools/whereis.py found both in slice3_42.c. An agent sent to
        #      port 0x10063060 was told two of its three callees were unported
        #      work. `[\s\*]` fixes all four.
        #      Validated against nine known answers, both directions:
        #      0x10069A90/0x10069DE0 flip to PORTED (the bug above),
        #      0x10069C90/0x10063860/0x1002E324/0x10063060 stay PORTED, and
        #      0x10070610/0x10008D60/0x10056260 stay NOT PORTED -- the last of
        #      those being the frontier stub that must never read as a port.
        #      THE PREFIX MUST BE DECORATION ONLY. Allowing arbitrary text
        #      between the opener and the address is what let a file-header
        #      INDEX match: br_sfx.c begins with a list of a dozen addresses,
        #      the regex matched whichever one sat closest to the closing */,
        #      and attributed it to the first function after the header --
        #      reporting 0x1006C290 as "PORTED as join3". A FALSE PORTED is the
        #      dangerous direction; a pass would skip a function this tree does
        #      not have.
        #
        #      A real banner puts its address on the first content line:
        #          /* 0x10041300 -- what it does
        #          /* --------------------------------
        #           * 0x1001D8A0 -- what it does
        #      so the prefix may contain only whitespace, '*', '-' and '='.
        #      Prose before the address means it is not that address's banner.
        for m in re.finditer(r'/\*[\s\*\-=]{0,120}?'
                             r'0x([0-9A-Fa-f]{8})'
                             r'(?:(?!\*/).)*?\*/\s*\n\s*(?:static\s+)?'
                             r'[A-Za-z_][\w \*]*[\s\*](\w+)\s*\(', s, re.S):
            #   6. REJECT A TABLE OF CONTENTS. A comment listing SEVERAL
            #      addresses is an INDEX, not a banner about whichever function
            #      happens to follow it. br_sfx.c opens with exactly such a
            #      list and the tool reported 0x1006C290 as "PORTED as join3"
            #      -- join3 being merely the first function after the file
            #      header. That is a FALSE PORTED, the dangerous direction: a
            #      pass would skip a function this tree does not have.
            #
            #      A real banner is about ONE address. Two or more distinct
            #      8-hex addresses in the same comment means index, so skip it;
            #      that costs nothing, because a genuine banner never needs a
            #      second address to identify its subject.
            # A COUNT OF ADDRESSES IS NOT THE TEST, and trying it broke a
            # correct case: rejecting any banner that mentions two addresses
            # threw away br_gamestep.c's
            #     /* 0x1002E324 -- `call dword ptr [0x106E79F4]` ...
            # because a banner naturally names the global its function operates
            # on. The real distinction is WHERE the address sits, not how many
            # there are, and the prefix rule above already draws it.
            add(m.group(1), m.group(2), f, 'banner over a body')
        for m in re.finditer(r'^[A-Za-z_][\w \*]*[\s\*](\w*?([0-9A-Fa-f]{8})\w*)\s*'
                             r'\([^;]*\)\s*\n?\s*\{', s, re.M):
            add(m.group(2), m.group(1), f, 'address in the name')
        # THE FOUR-DIGIT SHORT-FORM RULE IS REMOVED, and its output had to be
        # purged from the source tree.
        #
        # It CONSTRUCTED an address by prefixing `0x1000` to four hex digits
        # taken from a symbol name, or by padding to `0x100XXXX0`. That invents
        # an address rather than reading one, and it guesses twice per symbol
        # so at most one of the two can be right.
        #
        # Migrating its answers wrote 21 false `@implements` claims into the
        # source. Ten landed in slice2_25.c, where `BrOpt3760` became
        # `@implements 0x10003760` -- an address whose real code is an
        # unrelated 111-byte realloc wrapper. Another put a host-side helper
        # at 0x10063CA0, which is a replay forwarder already transcribed
        # elsewhere under its D3D number.
        #
        # None of it was caught by a tool. It was caught by a pass writing
        # plain-English descriptions, which had to READ each function in order
        # to describe it and noticed the claims did not match the bodies. That
        # is the argument for the descriptions being worth writing, quite apart
        # from preservation: they force someone to look.
        #
        # A name ending in four hex digits is a hint. Use whereis.py.
    return out


def mentions(addr):
    out = []
    pat = '0x%08X' % addr
    for f in glob.glob(SRC, recursive=True) + glob.glob(HDR, recursive=True):
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


def file_build(path, _cache={}):
    """Which BUILD is this module's addresses written against?

    THE TRAP THIS CLOSES IS THE WORST ONE IN THIS TOOL'S HISTORY. `defs` is
    keyed by a bare address with no build tag, so the cross-build lookup
    happily matched a D3D number against a banner that meant the GLIDE number:

        Glide 0x1001E9F0 pairs to D3D 0x1001CC00
        Glide 0x1001CC00 is RallyMain, and br_boot.c's banner says 0x1001CC00

    so a display-list opcode handler was reported as "PORTED as BrAppArgs".
    A bare number is not self-describing -- that is written at the top of
    whereis.py and this tool did it anyway, one layer down.

    Modules here state their reference build in the file banner. Where they do
    not, return None and the caller declines to make a cross-build claim rather
    than guessing, because guessing here fails toward FALSE PORTED.
    """
    if path in _cache:
        return _cache[path]
    b = None
    try:
        head = open(path, errors='ignore').read(4000)
    except Exception:
        head = ''
    lo = head.lower()
    if 'brglide.dll' in lo and 'brd3d.dll' not in lo:
        b = 'glide'
    elif 'brd3d.dll' in lo and 'brglide.dll' not in lo:
        b = 'd3d'
    elif 'reference: brglide' in lo or 'against brglide' in lo:
        b = 'glide'
    elif 'reference: brd3d' in lo or 'against brd3d' in lo:
        b = 'd3d'
    _cache[path] = b
    return b


def pairing(addr):
    """Both builds' readings of this number, from config/shared.csv.

    THE DEFECT THIS CLOSES, and it was wrong in BOTH dangerous directions at
    once on a single chain:

      Glide 0x10003320 is CHK_FReadOpen and is NOT ported. This tool said
      "PORTED as BrChkFileExists -- DO NOT re-transcribe", because BrChkFileExists
      carries the D3D address 0x10003320 in its name. Following that would have
      SKIPPED a real function.

      Glide 0x10003680 IS CHK_FileExists and IS ported. This tool said "clean
      target", because the D3D number for it is different. Following that would
      have DUPLICATED an existing transcription -- the failure mode that ends
      with two routines fighting over one global.

    The cause was matching address TEXT with no idea which image the number
    belongs to. slice1_01.h says in its first line that it is written against
    BRD3D.dll; most modules here use D3D addresses; some use Glide. A bare
    number is not self-describing and never was.
    """
    out = []
    if not os.path.exists('config/shared.csv'):
        return out
    for r in csv.DictReader(open('config/shared.csv')):
        try:
            d = int(r['d3d_va'], 16)
            g = int(r['glide_va'], 16) if r['glide_va'] else None
        except Exception:
            continue
        if d == addr and g is not None:
            out.append(('glide', g))
        if g == addr:
            out.append(('d3d', d))
    return out


def report(addr, defs):
    key = '%08X' % addr
    print('0x%s   %s bytes' % (key, size_of(addr) if size_of(addr) else '?'))
    if key in defs:
        name, f, how = defs[key]
        print('  PORTED as %s  (%s, found by %s)' % (name, f, how))
        ps = pairing(addr)
        if ps:
            print('  AMBIGUOUS: this number also reads as %s'
                  % ', '.join('0x%08X in %s' % (o, side) for side, o in ps))
            print('  -> confirm WHICH BUILD the module is written against before')
            print('     trusting this. A name carrying one build\'s address says')
            print('     nothing about the other build\'s function at that number.')
        print('  -> DO NOT re-transcribe. Wire it, or extend it.')
        return True
    # Before declaring anything absent, look up the SAME FUNCTION under the
    # other build's address. That is where the false "clean target" came from.
    for side, other in pairing(addr):
        k2 = '%08X' % other
        if k2 in defs:
            name, f, how = defs[k2]
            # `side` is the build `other` belongs to. Only believe the match if
            # the file that defines it is written against THAT build. Unknown
            # or mismatched -> say so and do not claim a port.
            import glob as _g
            cand = [q for q in _g.glob('src/core/**/' + f, recursive=True)
                    + _g.glob('include/**/' + f, recursive=True)]
            fb = file_build(cand[0]) if cand else None
            if fb is not None and fb != side:
                print('  a %s-build module (%s) defines 0x%s, but that number is'
                      % (fb, f, k2))
                print('     the counterpart only when read as %s. NOT a match --'
                      % side.upper())
                print('     the same number names different functions in the two')
                print('     builds. Confirm with tools/whereis.py.')
                continue
            print('  PORTED under its %s address 0x%s, as %s  (%s)'
                  % (side.upper(), k2, name, f))
            print('  -> DO NOT re-transcribe. This number and 0x%s are the '
                  'same function' % k2)
            print('     in the two builds. Confirm with tools/whereis.py.')
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


BANNER = """
!! isported.py INFERS from comment shape and IS NOT AUTHORITATIVE. !!

   Use tools/manifest.py. This tool exists only to enumerate the residue that
   has no @implements line yet.

   It has been wrong eight distinct ways, including reporting an already-ported
   function as a clean target (which causes duplicate transcriptions) and
   reporting one build's address under the other build's function. Its
   four-digit name rule wrote 21 false claims into the source before it was
   removed.

   Treat every answer below as a HYPOTHESIS to confirm with whereis.py.
"""


def main():
    print(BANNER)
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
