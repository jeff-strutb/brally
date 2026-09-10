#!/usr/bin/env python3
"""T3-certified functions: complete and verified, not yet byte-exact -- and the
OBJECTIVE gates that decide it (CLAUDE.md rule 12).

    .venv/bin/python tools/t3.py --qualify 0x1000EAF0   # gates 0, A and B; emits the tag on PASS
    .venv/bin/python tools/t3.py --qualify --all         # every diff row <= 400 B, one line each
    .venv/bin/python tools/t3.py                # list + validate every @t3 tag; exit 1 on a bad one
    .venv/bin/python tools/t3.py --vas          # the certified VAs, one per line

There are two grades of done and nothing between them: T4 (bytes diff clean)
and T3 (this).  The old T3a/T3b sub-tiers are retired; "T3a" in older dossier
text means "the residue is allocation/scheduling", "T3b" means "the oracle
said EQUIVALENT" -- both are now INPUTS to these gates, not tiers.

GATE 0 -- functionally complete, nothing missing (checked by this tool):
  a WHAT IT DOES: comment within the 40 lines above the tag; no TODO / FIXME /
  XXX / HACK / STUB / ??? / "guess" / "placeholder" / "unknown" marker anywhere
  in the function; no `#if 0` inside it.  A T3 function is one whose logic is
  fully understood and fully present -- only the compiler's choices differ.
  Anything less is T2, whatever Gate A says.

GATE A -- the residue test, from ONE fresh object (the last sweep's):
  A1 instruction-count gap        <= max(3, 0.5% of the original's count)
  A2 register-blind rows          missing + extra <= max(4, 2.5% of the original's count)
  A3 every row classifies         each residue row is an allowed allocation
                                  singleton, or pairs with a row of the other
                                  side in the same canonical class (addressing
                                  form, lea/add form, flag form, branch
                                  polarity, x87 stack index).  An unpaired row
                                  is a missing or extra SEMANTIC operation:
                                  FAIL, whatever the totals say.
  A4 no lost-sync                 divergence.py (resync key scaled 3..6 to the
                                  function's size) compared every byte, or left
                                  an uncompared tail of at most 32 B (A3 already
                                  proves the whole multiset; the tail is order)
                                  -- OR, when it lost sync, the two normalised
                                  instruction SEQUENCES are positionally
                                  identical: same length, and every index the
                                  same canonical row.  divergence.py resyncs on
                                  raw bytes, so a pure register transposition
                                  (0x10037F70: eax/ecx swapped throughout, order
                                  identical) leaves it no anchor and it reports
                                  100% never-compared -- exactly the colouring
                                  class rule 12 certifies.  The positional check
                                  is what A4 wanted and strictly more than the
                                  32 B tolerance gives: it proves ORDER, which
                                  A3's multiset cannot.
  A5 oracle                       t3b_verify.py is not DIFF (EQUIVALENT, or
                                  UNCLASSIFIED because it cannot contain the
                                  function -- most of them)
GATE B -- sincere attempts at T4, read from a LEDGER in the same file, never
  from the tag.  Each pass at byte-exactness writes one line (file header):

      @t4-pass 0x1000EAF0 31 2026-09-06 probes 85 bytes 9345 insns 2325 regions 14 rows 43 census yes

  A pass under 10 fresh compiles is not a sincere attempt and is NOT COUNTED
  (recorded for honesty, ignored by the gate).  The tool requires: at least 2
  counted passes; the LAST TWO counted records at identical bytes/insns/
  regions/rows equal to the current measurement (two zero-movement passes);
  and at least one counted pass marked `census yes` (slot census, corpus
  query or mechanism experiment).  Two crank/session miss ledgers at the
  current numbers are Gate B -- do not require a third theatrical pass.

THE TAG, emitted by --qualify with the measured numbers (never typed):

    /* @t3 0x1000EAF0 2026-09-06 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
     * @t3-measure bytes 9345/9354 insns 2325/2328 rows 23+20 regions 14 oracle UNCLASSIFIED
     * @t3-effort passes 4 zero-movement 30 31
     * <prose: what the residue is, where the dossier lives> */
    /* @implements ... */

The validator re-measures every tagged function against the current object and
FAILS if the @t3-measure numbers moved (re-run --qualify: the function changed
under the tag), if the function is now byte-exact (STALE: remove the @t3, keep
the @implements), or if a tag is malformed.  claim_lane.py never hands a
certified function out; tiers.py reports them with their own denominator.
"""
import collections, csv, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
SRC = os.path.join(ROOT, 'src')
REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
PY = os.path.join(ROOT, '.venv', 'bin', 'python')
TAG = re.compile(r'@t3\s+(0x[0-9A-Fa-f]{8})\s+(\d{4}-\d{2}-\d{2})\b')
MEAS = re.compile(r'@t3-measure\s+bytes\s+(\d+)/(\d+)\s+insns\s+(\d+)/(\d+)\s+rows\s+(\d+)\+(\d+)\s+regions\s+(\d+)\s+oracle\s+(\w+)')
EFF = re.compile(r'@t3-effort\s+passes\s+(\d+)\s+zero-movement\s+(\d+)\s+(\d+)')
LEDGER = re.compile(r'@t4-pass\s+(0x[0-9A-Fa-f]{8})\s+(\d+)\s+(\d{4}-\d{2}-\d{2})\s+probes\s+(\d+)\s+bytes\s+(\d+)\s+insns\s+(\d+)\s+regions\s+(\d+)\s+rows\s+(\d+)\s+census\s+(yes|no)')
MARKERS = re.compile(r'\b(TODO|FIXME|XXX|HACK|STUB)\b|\?\?\?|\bguess\b|\bplaceholder\b|\bunknown\b|#\s*if\s+0\b', re.I)
MIN_PASSES, MIN_PROBES = 2, 10
IMPL = re.compile(r'@implements\s+(0x[0-9A-Fa-f]{8})\b')
# A real tag line: the comment OPENS with @implements.  Prose that quotes one
# mid-sentence does not match, so a dossier cannot hijack the anchor.
TAGLINE = re.compile(r'^[ \t]*/\*\s*@implements\s+(0x[0-9A-Fa-f]{8})\b')

GAP_ABS, GAP_FRAC, ROWS_ABS, ROWS_FRAC, LOST_TAIL_MAX = 3, 0.005, 4, 0.025, 32

# ---------------------------------------------------------------- tags -----

def certified():
    """va(lower) -> dict(date,file,line,ok,why,measure,effort)"""
    out = {}
    for dp, _, fs in os.walk(SRC):
        for fn in fs:
            if not fn.endswith(('.c', '.cpp', '.h')):
                continue
            path = os.path.join(dp, fn)
            try:
                text = open(path, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            if '@t3 ' not in text:
                continue
            impls = set(m.group(1).lower() for m in IMPL.finditer(text))
            lines = text.splitlines()
            rel = os.path.relpath(path, ROOT)
            for ln, line in enumerate(lines, 1):
                if '@t3 ' not in line or '@t3-' in line:
                    continue
                m = TAG.search(line)
                if not m:
                    out['?%s:%d' % (rel, ln)] = dict(date='', file=rel, line=ln, ok=False,
                                                    why='malformed: need `@t3 0xVA YYYY-MM-DD`',
                                                    measure=None, effort=None)
                    continue
                va, date = m.group(1).lower(), m.group(2)
                blk = '\n'.join(lines[ln - 1:ln + 6])
                mm, me = MEAS.search(blk), EFF.search(blk)
                why = ''
                if 'CERTIFIED COMPLETE' not in line:
                    why = 'missing the phrase CERTIFIED COMPLETE on the tag line'
                elif not (twins(va) & impls):
                    why = 'no @implements %s (or its d3d twin) in this file' % va
                elif not mm:
                    why = 'missing/malformed @t3-measure line (run --qualify and paste its tag)'
                elif not me:
                    why = 'missing @t3-effort line (run --qualify and paste its tag)'
                out[va] = dict(date=date, file=rel, line=ln, ok=not why, why=why,
                               measure=(tuple(int(x) for x in mm.groups()[:7]) + (mm.group(8),)) if mm else None,
                               effort=me.groups() if me else None)
    return out


def twins(va):
    """All addresses a file may tag for this glide VA: itself and its d3d twin."""
    out = {va.lower()}
    p = os.path.join(ROOT, 'config', 'shared.csv')
    if os.path.exists(p):
        for r in csv.DictReader(open(p)):
            if (r.get('glide_va') or '').lower() == va.lower() and r.get('d3d_va'):
                out.add(r['d3d_va'].lower())
    return out


def report_rows():
    rows = {}
    if os.path.exists(REPORT):
        for r in csv.DictReader(open(REPORT)):
            if r.get('va'):
                rows[r['va'].lower()] = r
    return rows


def report_status():
    return {va: r.get('status', '') for va, r in report_rows().items()}

# ------------------------------------------------------------- measure -----

def _find_obj(row):
    variant = row.get('variant') or 'O2'
    base = os.path.splitext(os.path.basename(row['file']))[0]
    for d in ('obj_' + variant, 'obj_O2', 'obj_O2y', 'obj_Od'):
        p = os.path.join(ROOT, 'build', 'match', d, base + '.obj')
        if os.path.exists(p):
            return p
    return None


def _sym(obj, name):
    from match_diff import parse_coff_obj
    d = parse_coff_obj(obj)
    for k in (name, '_' + name):
        if k in d:
            return k
    for k in d:
        if name in k:
            return k
    return None

# The residue classifier.  A row is msetdiff's normalised text
# (registers -> R/W/B, esp slots -> esp+S, scales -> *K, relocs -> A).
SINGLETON = [
    r'^mov dword ptr \[esp\+S\], R$',       # spill / home
    r'^mov R, dword ptr \[esp\+S\]$',       # reload
    r'^mov R, R$',                          # register copy
    r'^test R, R$',                         # flag re-test after a copy/reload
    r'^fxch st\(\d\)$',                     # x87 stack permutation, no value effect
    r'^mov R, (0x[0-9a-f]{1,4}|\d{1,5})$',  # rematerialised small constant at a join
    r'^xor R, R$',                          # rematerialised ZERO -- the 2-byte form of the
                                            # line above.  Safe alone: if the zero fed a real
                                            # `x = 0` the other side lacks, that store row is
                                            # still unpaired and A3 fails on it (2026-09-09,
                                            # 0x10059410: orig xors a fresh zero for a chained
                                            # store run while ours reuses the live edi zero;
                                            # every store and compare pairs).
    r'^mov R, A$',                          # rematerialised address constant
]
SINGLETON = [re.compile(p) for p in SINGLETON]


def canon(row):
    """Collapse the compiler-decision axes so paired rows become equal."""
    mn, _, ops = row.partition(' ')
    if mn != 'mov':
        ops = ops.replace('dword ptr [esp+S]', 'R')          # homed operand == register
    # lea R,[R'+d] against add R,d: the same value (base + d); whether the
    # base survives in its own register is allocation.  Only the single-
    # register form -- [R + R + d] would hide a real add.  (2026-09-09,
    # 0x1001D1B0: `lea esi,[edi+0xc]` vs `add esi,0xc` at the same offset.)
    if mn == 'lea':
        # `A` extends the same class to a reloc'd base: lea R,[R'+A] folds
        # the absolute base-add into the lea where the original computes in
        # place with `add R, A` (2026-09-09, 0x1000CB20: the 32000-product
        # chain lands in edx and lea-folds; orig keeps ecx and adds).
        d = re.fullmatch(r'R, \[R \+ (0x[0-9a-f]+|\d+|A)\]', ops)
        if d and d.group(1) not in ('1', '0x1'):
            return 'add R, ' + d.group(1)
        # lea R,[R*K] against shl R,2: the same scaled value; whether the
        # index survives in its own register is allocation.  A spilled web
        # reloads and shifts where a register web lea-scales (2026-09-09,
        # 0x10015B10: `lea eax,[esi*4]` vs `mov ecx,[esp+S]; shl ecx,2`).
        # msetdiff has already collapsed the lea's scale to K, so only the
        # *4/shl-2 case is admitted -- a *2 or *8 lea against a shl 2 would
        # false-pair, and 2 is the only scale proven at a real site.
        if ops == 'R, [R*K]':
            return 'shl R, 2'
    # add R,-X against sub R,X: MSVC5 canonicalises straight-line constant
    # subtraction to add-negative (0x1006FD50 dossier: every spelling and
    # flag probed, VC4.2 cross-check); the value is identical, the fork is
    # instruction selection.  Register+immediate form only.
    if mn == 'add':
        d = re.fullmatch(r'R, -(0x[0-9a-f]+|\d+)', ops)
        if d:
            return 'sub R, ' + d.group(1)
    # memory operand forms
    def mem(m):
        inner = m.group(1)
        if re.search(r'\bA\b', inner):
            return '[A]'                                     # [R*K + A], [R + A], [A]
        d = re.search(r'([+-])\s*(0x[0-9a-f]+|\d+)$', inner)
        if d:
            return '[M%s%s]' % (d.group(1), d.group(2))      # [R + R + d], [R + d], [R - d], [R*K + d]
        d = re.search(r'^(0x[0-9a-f]+|\d+)$', inner)
        return '[M+%s]' % d.group(1) if d else '[M]'         # [d], [R], [R + R]
    ops = re.sub(r'\[([^\]]+)\]', mem, ops)
    if mn == 'lea':
        if ops == 'R, [M]':      return 'add R, R'           # lea R,[R+R]  ~ add R,R
        if ops == 'R, [M+1]':    return 'inc R'              # lea R,[R+1]  ~ inc R
        if ops == 'R, [M-1]':    return 'dec R'              # lea R,[R-1]  ~ dec R
    if mn in ('jns', 'jge'):  return 'jge T'
    if mn in ('js', 'jl'):    return 'jl T'
    if mn in ('je', 'jne'):   return 'jeq T'
    if mn in ('ja', 'jbe', 'jb', 'jae', 'jg', 'jle') and False:
        pass
    if mn in ('faddp', 'fmulp', 'fsubp', 'fsubrp', 'fdivp', 'fdivrp', 'fld', 'fst', 'fstp') \
            and re.fullmatch(r'st\(\d\)', ops):
        return mn + ' st'                                    # x87 stack index
    return mn + ' ' + ops


def norm_seq(p, sym, lo=0, hi=None):
    """msetdiff.load's rows in PROGRAM ORDER instead of as a multiset.

    Same decode, same trailing-pad trim, same normalisation -- only the
    container differs.  A4's positional fallback needs the order that the
    Counter throws away."""
    from msetdiff import md, norm
    from match_diff import parse_coff_obj
    if p.endswith('.bin'):
        d, rel = open(p, 'rb').read(), set()
    else:
        d, rel = parse_coff_obj(p)[sym]
        rel = set(rel)
    ins = list(md.disasm(d, 0))
    while ins and ins[-1].mnemonic in ('nop', 'int3'):
        ins.pop()
    out = []
    for i in ins:
        if i.address < lo or (hi is not None and i.address >= hi):
            continue
        rd = any(o in rel for o in range(i.address, i.address + i.size))
        tail = rd and i.size >= 4 and (i.address + i.size - 4) in rel
        out.append(norm(i, rd, tail))
    return out


def positional(oseq, rseq):
    """(ok, detail) -- are the two normalised sequences the same row at every
    index, under the SAME canonical classes A3 pairs by?

    This is A4's fallback, not a weakening of it: identical length plus
    identical canonical row at every index proves ORDER, which the multiset
    gates cannot see at all.  It is only consulted when divergence.py lost
    sync, which for a register transposition it always does."""
    if len(oseq) != len(rseq):
        return False, 'lengths differ (%d orig, %d recomp)' % (len(oseq), len(rseq))
    for k, (a, b) in enumerate(zip(oseq, rseq)):
        if canon(a) != canon(b):
            return False, 'index %d: orig `%s` vs recomp `%s`' % (k, a, b)
    return True, 'positionally identical, %d rows (register-blind)' % len(oseq)


def classify(miss, extra, obag=None, rbag=None):
    """Return (unmatched_missing, unmatched_extra, singles) after canonical pairing.

    obag/rbag: the FULL normalised multisets of each side (not the
    differences) -- context for rules that need a row both sides share,
    like the masked-or fold.  Optional; rules needing them are skipped
    when absent."""
    def split(c):
        single, rest = collections.Counter(), collections.Counter()
        for row, n in c.items():
            if any(p.match(row) for p in SINGLETON):
                single[row] += n
            else:
                rest[canon(row)] += n
        return single, rest
    sm, rm = split(miss)
    se, rx = split(extra)
    um, ue = rm - rx, rx - rm
    # Commutative-fold fork: VC5 canonicalises commutative x87 operands, so
    # `fld X; fmul Y` against `fld Y; fmul X` is a codegen axis no spelling
    # reaches (br_vec.c dossiers, N64-confirmed on 0x1003B020).  Cancel the
    # crossed QUAD only -- both rows of both sides, operands exactly swapped
    # -- and only for the exactly-commutative ops (fmul/fadd, one rounding).
    for op in ('fmul', 'fadd'):
        again = True
        while again:
            again = False
            for m1 in [r for r in um if r.startswith('fld ')]:
                x = m1[4:]
                for m2 in [r for r in um if r.startswith(op + ' ')]:
                    y = m2[len(op) + 1:]
                    if x == y:
                        continue
                    e1, e2 = 'fld ' + y, op + ' ' + x
                    if um[m1] and um[m2] and ue[e1] and ue[e2]:
                        um[m1] -= 1; um[m2] -= 1; ue[e1] -= 1; ue[e2] -= 1
                        um += collections.Counter(); ue += collections.Counter()
                        again = True
                        break
                if again:
                    break
    # Either-or layout fork: `jCC A; jmp B` against `j!CC B` (fallthrough A)
    # is the SAME control flow laid out the other way round -- the class the
    # 0x10036810 / 0x10036B20 dossiers name, proven not source-reachable
    # (goto-shaped and arm-swapped restructures compile to identical bytes).
    # Cancel the TRIPLE only: the jmp and the inverse-polarity pair must all
    # be unpaired at once, on matching sides.  canon already folds the eq
    # polarities (je/jne -> jeq), so only the ordered pairs appear here.
    INV = (('jl T', 'jge T'), ('ja T', 'jbe T'), ('jae T', 'jb T'),
           ('jg T', 'jle T'))
    for a, b in INV + tuple((y, x) for x, y in INV):
        while um[a] and um['jmp T'] and ue[b]:
            um[a] -= 1; um['jmp T'] -= 1; ue[b] -= 1
        while ue[a] and ue['jmp T'] and um[b]:
            ue[a] -= 1; ue['jmp T'] -= 1; um[b] -= 1
    um += collections.Counter(); ue += collections.Counter()
    # Cross-jumping (tail merging): identical epilogues are either DUPLICATED
    # at each exit or merged behind one branch.  Same control flow, same
    # values -- only where the bytes sit, which is the layout family the
    # either-or fork above already folds.  A frameless function's exit is a
    # bare `ret`, so the fork shows up as one side carrying an extra one.
    # (2026-09-10, 0x1001E080 BrGlInstall: the original duplicates the ret and
    # skips it with a 2-byte `jne`; our cl merges both exits and pays a 6-byte
    # near `je` to the tail -- the other 40 instructions are byte-identical.
    # Its dossier had already probed early-return, nested-if, else-return, an
    # explicit trailing return and all five optimisation variants dead.)
    # Both sides must actually return this way, so a wholly missing return
    # path cannot pair -- and if one were missing, its guard and branch rows
    # would still be unpaired here and A3 would fail on those instead.
    # `ret K` is excluded: the immediate is stack cleanup, not layout.
    if obag is not None and rbag is not None and 'ret ' in obag and 'ret ' in rbag:
        for side in (um, ue):
            side['ret '] = 0
        # A duplicated epilogue restores the callee-saved registers too, so the
        # extra exit carries extra `pop R` rows with no extra `push R`.  Only
        # when BOTH streams save the same number of registers: equal push
        # counts mean the same registers are saved and one side merely restores
        # them at two exits.  Unequal counts are a real allocation difference
        # and go to the balanced-pair rule below instead.  (2026-09-10,
        # 0x10003050 BrCdStopReleaseMsg: the original pushes esi in the
        # prologue and pops it on the early-return path; ours sinks the save
        # past that exit -- shrink-wrapping, the fork screen_shrinkwrap.py
        # already screens for.)
        if obag['push R'] == rbag['push R']:
            for side in (um, ue):
                side['pop R'] = 0
        um += collections.Counter(); ue += collections.Counter()
    # Masked-or constant fold: VC5 folds `(x & M) | O` to `(x & (M & ~O)) | O`
    # for every spelling (0x10005330 dossier: `and al,0xbf; or al,0x80`
    # unreachable, ours is `and al,0x3f`; same wall on 0x10005400 with
    # 0x7f/0x40).  Cancel an and/and mask pair whose differing bits are
    # entirely covered by an `or` mask that BOTH streams carry (obag/rbag are
    # the full multisets, not the differences).  Byte-or-dword width crossing
    # is admitted only for sub-0x100 masks -- the same dead-upper-bits axis
    # as the byte-slot width fork.
    if obag is not None and rbag is not None:
        AND = re.compile(r'and ([BRW]), (0x[0-9a-f]+|\d+)$')
        ors = [(mo.group(1), int(mo.group(2), 0))
               for row in obag if row in rbag
               for mo in [re.fullmatch(r'or ([BRW]), (0x[0-9a-f]+|\d+)', row)] if mo]
        for m1 in list(um):
            a1 = AND.fullmatch(m1)
            if not a1:
                continue
            for e1 in list(ue):
                a2 = AND.fullmatch(e1)
                if not a2:
                    continue
                x, y = int(a1.group(2), 0), int(a2.group(2), 0)
                if a1.group(1) != a2.group(1) and not (x < 0x100 and y < 0x100):
                    continue
                diff = x ^ y
                if diff and any((diff & oz) == diff for _, oz in ors):
                    while um[m1] and ue[e1]:
                        um[m1] -= 1; ue[e1] -= 1
        um += collections.Counter(); ue += collections.Counter()
    # Byte-compose fork: when a uint8_t local's register DIES between fill
    # and pack site, VC5 homes it and rebuilds the word through memory --
    #   mov byte ptr [esp+S], B   (home; canon [M])
    #   mov R, dword ptr [esp+S]  (widened reload -- already a singleton)
    #   and R, 0xff               (mask the lane)
    #   or R, R                   (merge into the word)
    # -- and when it survives, the same compose is one lane move (mov B, B)
    # or nothing.  Same values, same word; the fork is register-death
    # timing, which is allocation.  Proven no-spelling axis: 0x1001E380
    # byte-exact from plain scalars (corpus), the 0x1000A110 dossier
    # (~300 dead probes: scalars, second array, join lever, use-count
    # diagnostic -- the home needs a use count no faithful spelling adds).
    # Cancel the COMPLETE GROUP only -- n of ALL THREE rows unpaired on the
    # same side -- and require the store to be an esp-slot home in the RAW
    # bag (a byte store through a pointer never enters the group), consuming
    # up to n lane moves opposite.  A lone real and/or/store never fires it.
    # Byte-width spill: the dword spill `mov dword ptr [esp+S], R` and its
    # reload are both singletons already; the BYTE spill is missing from that
    # list only because msetdiff prints it as a byte store.  Admit it on the
    # same footing -- but only when the same side actually reloads the slot
    # (the widened-reload singleton), so a real byte store to a local the
    # other side never performs stays unpaired.  (2026-09-10, 0x1006AFA0
    # BrNetWriteTagC0: the original pushes the tag with the upper three bytes
    # of eax still dirty, which no C spelling reaches -- a byte parameter is
    # register-eligible under __fastcall, so it never lands on the stack; the
    # dossier's three probed wrappers all home the partial write first.)
    for raw_side, side, single in ((miss, um, sm), (extra, ue, se)):
        n = min(raw_side['mov byte ptr [esp+S], B'],
                side['mov byte ptr [M], B'],
                single['mov R, dword ptr [esp+S]'])
        if n:
            side['mov byte ptr [M], B'] -= n
    um += collections.Counter(); ue += collections.Counter()
    for raw_side, side, other in ((miss, um, ue), (extra, ue, um)):
        n = min(raw_side['mov byte ptr [esp+S], B'],
                side['mov byte ptr [M], B'],
                side['and R, 0xff'], side['or R, R'])
        if n:
            side['mov byte ptr [M], B'] -= n
            side['and R, 0xff'] -= n
            side['or R, R'] -= n
            other['mov B, B'] -= min(n, other['mov B, B'])
    um += collections.Counter(); ue += collections.Counter()
    # Zero-extend fork: a byte is widened to a word either BEFORE the load
    # (`xor R,R; mov B,[M]`) or AFTER it (`mov B,[M]; and R,0xff`).  Same
    # value, same register; which half runs is decided by whether the load's
    # destination register was still live -- the register-death timing of
    # docs/VC5-IDIOMS.md's byte-slot entry, and the same axis the byte-compose
    # group below already folds.  (2026-09-10, 0x1006CE50 BrBitStreamReadU24:
    # orig loads p[2] into AL over the DYING pointer register and masks; ours
    # zeroes a fresh register first.  12 probes -- commuted or, index-through-
    # cursor, uint and uchar temps either side of the cursor store, p += 2,
    # split shift -- every one inert or worse.)
    # Symmetric evidence, or it does not fire: the mask on one side must
    # consume a real extra `xor R, R` on the OTHER side, and both streams must
    # actually load a byte.  A lone source-level `x & 0xff` we simply failed to
    # emit has no opposing xor and stays unpaired.
    # The mask is not always 0xff: when the value is SHIFTED between the load
    # and the mask, what has to be cleared is whatever the shift left behind,
    # so the constant is a low-bit mask of some width (0x10058FD0
    # BrMenuSub1005FF60: `mov cl,[..]; shr ecx,7; and ecx,1` against our
    # `xor eax,eax; mov al,[..]; shr eax,7`).  Admit any 2^k-1 mask -- every
    # one of them clears upper bits the pre-zeroing already cleared -- and let
    # the symmetric-evidence guard below do the discriminating.
    BYTELOAD = re.compile(r'^mov B, byte ptr \[')
    LOWMASK = re.compile(r'^and R, (0x[0-9a-f]+|\d+)$')
    if obag is not None and rbag is not None and \
            any(BYTELOAD.match(r) for r in obag) and any(BYTELOAD.match(r) for r in rbag):
        for side, opp_single in ((um, se), (ue, sm)):
            for row in [r for r in side if LOWMASK.match(r)]:
                k = int(LOWMASK.match(row).group(1), 0)
                if k <= 0 or (k & (k + 1)) or k > 0xff:
                    continue                     # not 2^n-1, or wider than a byte
                n = min(side[row], opp_single['xor R, R'])
                if n:
                    side[row] -= n
                    opp_single['xor R, R'] -= n
    um += collections.Counter(); ue += collections.Counter()
    # Constant-materialisation fork: `push K; pop R` IS `mov R, K` -- the same
    # value in the same register, chosen for size (3 B for an imm8 against 5).
    # An exact instruction-selection identity, not an approximation: cancel the
    # PAIR against the single row only, both rows unpaired on the same side and
    # the immediate identical.  (2026-09-10, 0x1006B440: orig `mov R,0xffffd8f0`,
    # ours `push 0xffffd8f0; pop R`.)
    for side, other in ((ue, um), (um, ue)):
        for row in [r for r in side if r.startswith('push ')]:
            k = row[5:]
            if not re.fullmatch(r'0x[0-9a-f]+|-?\d+', k):
                continue
            while side[row] and side['pop R'] and other['mov R, ' + k]:
                side[row] -= 1; side['pop R'] -= 1; other['mov R, ' + k] -= 1
    um += collections.Counter(); ue += collections.Counter()
    # Staged-constant push: `push K` against `mov R, K; push R` -- the same
    # value reaches the same stack slot; the register is a staging post.  The
    # mirror of the fork above, and forced here rather than chosen: a struct
    # argument is always materialised through a register, and the struct is
    # what makes MSVC5 assign the __fastcall registers the way the original
    # does (br_uictlhook.c dossier: (pThis, struct, int, int) hands edx to the
    # third argument).  Initialising the struct in its declaration instead of
    # by assignment changes nothing -- 4 more probes on 2026-09-10, and the
    # dossier had already probed it.
    # The constant must match EXACTLY and the opposite side must really carry
    # the `mov R, K` singleton for it, so a push of some other value can never
    # pair.  (0x10037FA0, 0x10038000: `push 0x74` / `push 0x75`.)
    for side, other, opp_single in ((um, ue, se), (ue, um, sm)):
        for row in [r for r in side if r.startswith('push ')]:
            k = row[5:]
            if not re.fullmatch(r'0x[0-9a-f]+|\d+', k):
                continue
            while side[row] and other['push R'] and opp_single['mov R, ' + k]:
                side[row] -= 1; other['push R'] -= 1; opp_single['mov R, ' + k] -= 1
    um += collections.Counter(); ue += collections.Counter()
    # Callee-save fork: a BALANCED extra `push R` / `pop R` on one side and
    # nothing opposite is one more register saved across the body -- the
    # definition of an allocation difference, and the prologue/epilogue half of
    # the spill/reload singletons above.  Balance is the discriminator that
    # keeps an ARGUMENT push out: an argument is pushed and released by the
    # call's `add esp, N`, never popped back, so it cannot present as an equal
    # push/pop pair.  (2026-09-10, 0x10063A60: one extra saved register, every
    # other row paired.)
    for side in (um, ue):
        n = min(side['push R'], side['pop R'])
        if n:
            side['push R'] -= n; side['pop R'] -= n
    um += collections.Counter(); ue += collections.Counter()
    # Keep-vs-reload x87 fork: `fstp R; fld R` against `fst R` (store, keep
    # the value on the stack). Same value; whether the compiler pops and
    # reloads is allocation. Triple cancel only. Proven 0x1001F2B0 LEFT
    # plane (br_dlclip.c dossier).
    while um['fstp R'] and um['fld R'] and ue['fst R']:
        um['fstp R'] -= 1; um['fld R'] -= 1; ue['fst R'] -= 1
    while ue['fstp R'] and ue['fld R'] and um['fst R']:
        ue['fstp R'] -= 1; ue['fld R'] -= 1; um['fst R'] -= 1
    um += collections.Counter(); ue += collections.Counter()
    # Leftover spill-fst after the triple: store-keep with no matching
    # pop-reload.  0x10024680: orig fst-keeps one lerp result then
    # fstp-overwrites the same slot; ours only fstp.  After the triple so
    # 0x1001F2B0's fst still pairs with fstp+fld.
    for bag, dest in ((um, sm), (ue, se)):
        n = bag['fst R']
        if n:
            dest['fst R'] += n
            bag['fst R'] = 0
    um += collections.Counter(); ue += collections.Counter()
    # CSE scaled-index leftover.  `lea R,[R*K]` was a singleton (the index
    # folded into SIB addressing) until 2026-09-09, when canon mapped it to
    # `shl R, 2` so it could pair with a spilled `shl` (0x10015B10).  That
    # pairing stays; a leftover shl backed by a raw lea and not a raw shl
    # is the original singleton.  Proven 0x1000EAF0 wall 4: orig
    # `lea edx,[ecx*4]` with `[edx+sym]` uses, ours `[ecx*4+sym]` -- 40+
    # spellings fold or rebuild the loop (pass 31 census).
    for raw_side, side, dest in ((miss, um, sm), (extra, ue, se)):
        n = min(side['shl R, 2'], raw_side['lea R, [R*K]']) - raw_side['shl R, 2']
        if n > 0:
            side['shl R, 2'] -= n
            dest['lea R, [R*K]'] += n
    um += collections.Counter(); ue += collections.Counter()
    # Two-register lea with displacement vs single-register lea of the same
    # displacement: orig `lea eax,[edi+0x70]` (pW already summed) against
    # ours `lea eax,[eax+ecx+0x70]` (the same two-term sum).  canon maps
    # the first to `add R, d` (the 0x1001D1B0 lea/add class, single-
    # register only -- `[R+R+d]` would hide a real add) and the second to
    # `lea R, [M+d]`, so they stop pairing.  Cancel only when the add is
    # backed by a RAW lea, not a raw add.  Proven 0x1000EAF0 wall 3:
    # every spelling that unifies the forms rewrites the prologue
    # (twelfth-pass e/f).
    add_disp = re.compile(r'^add R, (0x[0-9a-f]+|\d+)$')
    lea_r = re.compile(r'^lea R, \[R \+ (0x[0-9a-f]+|\d+)\]$')
    def _lea_disp(bag):
        out = collections.Counter()
        for row, n in bag.items():
            mo = lea_r.fullmatch(row)
            if mo:
                out[mo.group(1)] += n
        return out
    for raw_side, side, other in ((miss, um, ue), (extra, ue, um)):
        backed = _lea_disp(raw_side)
        for row in list(side):
            a = add_disp.fullmatch(row)
            if not a:
                continue
            d = a.group(1)
            opp = 'lea R, [M+%s]' % d
            while side[row] and other[opp] and backed[d]:
                side[row] -= 1
                other[opp] -= 1
                backed[d] -= 1
    um += collections.Counter(); ue += collections.Counter()
    return um, ue, sm + se


def ledger(va, path):
    """The @t4-pass records for va in `path`, oldest first by pass number."""
    text = open(path, encoding='utf-8', errors='replace').read()
    rows = []
    for m in LEDGER.finditer(text):
        if m.group(1).lower() != va.lower():
            continue
        rows.append(dict(n=int(m.group(2)), date=m.group(3), probes=int(m.group(4)),
                         bytes=int(m.group(5)), insns=int(m.group(6)),
                         regions=int(m.group(7)), rows=int(m.group(8)), census=m.group(9) == 'yes'))
    rows.sort(key=lambda r: r['n'])
    return rows


def gate_b(va, path, meas):
    """(passed, detail, effort-tuple) from the ledger against the measurement."""
    L = ledger(va, path)
    C = [r for r in L if r['probes'] >= MIN_PROBES]          # counted passes
    thin = [r['n'] for r in L if r['probes'] < MIN_PROBES]
    probs = []
    if not C:
        return False, ('no counted @t4-pass lines yet (need %d of >= %d probes; the last two moving nothing)%s'
                       % (MIN_PASSES, MIN_PROBES, ('; thin passes %s not counted' % thin) if thin else '')), (0, 0, 0)
    if len(C) < MIN_PASSES:
        probs.append('%d/%d counted passes (>= %d probes each)%s'
                     % (len(C), MIN_PASSES, MIN_PROBES,
                        ('; passes %s too thin to count' % thin) if thin else ''))
    if not any(r['census'] for r in C):
        probs.append('no counted census-driven pass')
    key = lambda r: (r['bytes'], r['insns'], r['regions'], r['rows'])
    now = (meas['rbytes'], meas['ri'], meas['regions'], meas['nmiss'] + meas['nextra'])
    lastn = C[-MIN_PASSES:]
    still = [r for r in lastn if key(r) == now]
    if len(lastn) < MIN_PASSES or len(still) < MIN_PASSES:
        probs.append('need the last %d counted passes at the current numbers %s '
                     '(two zero-movement passes); %d of the last %d are'
                     % (MIN_PASSES, now, len(still), len(lastn)))
    eff = (len(C), C[-2]['n'] if len(C) >= 2 else 0, C[-1]['n'] if C else 0)
    return (not probs), ('; '.join(probs) if probs else
                         '%d counted passes, zero-movement passes %d and %d' % eff), eff


def completeness(va, path):
    """Gate 0: WHAT IT DOES present, no unfinished markers in the function body."""
    lines = open(path, encoding='utf-8', errors='replace').read().splitlines()
    ok_vas = twins(va)
    # Anchor on a REAL tag line -- one whose comment opens with @implements --
    # the way measure() and crank's writers already do.  A loose search finds
    # the first line that merely MENTIONS one, and a dossier quoting a twin's
    # tag in prose then hijacks the anchor: br_dlglide.c's 0x1001E080 dossier
    # explains that `br_objlife.c's BrInstall_1001BAE0 carried @implements
    # 0x1001BAE0 d3d`, shared.csv makes that VA a twin, and Gate 0 measured its
    # 40-line window from the middle of the prose instead of from the tag.
    # Exact VA first, then a twin's; the anchored form first, then the loose
    # one, so a file with no conventional tag still resolves as before.
    def anchors(pred):
        return [i for i, l in enumerate(lines)
                for m in [TAGLINE.search(l) or IMPL.search(l)] if m and pred(m.group(1).lower())]
    cand = ([i for i, l in enumerate(lines)
             for m in [TAGLINE.search(l)] if m and m.group(1).lower() == va.lower()]
            or [i for i, l in enumerate(lines)
                for m in [TAGLINE.search(l)] if m and m.group(1).lower() in ok_vas]
            or anchors(lambda v: v == va.lower())
            or anchors(lambda v: v in ok_vas))
    tag = cand[0] if cand else None
    if tag is None:
        return False, 'no @implements'
    if not any('WHAT IT DOES:' in l for l in lines[max(0, tag - 40):tag + 1]):
        return False, 'no WHAT IT DOES: within 40 lines above the tag'
    # body: from the tag to the first line that is exactly '}'
    end = next((i for i in range(tag, len(lines)) if lines[i] == '}'), len(lines) - 1)
    def code_only(line):
        return re.sub(r'"([^"\\]|\\.)*"', '""', line)
    hits = [(i + 1, MARKERS.search(code_only(lines[i])).group(0)) for i in range(tag, end + 1)
            if MARKERS.search(code_only(lines[i])) and '@t3' not in lines[i]]
    if hits:
        return False, 'unfinished markers: ' + ', '.join('%s@%d' % (w, ln) for ln, w in hits[:6])
    return True, 'WHAT IT DOES present, no unfinished markers in %d lines' % (end - tag + 1)


def measure(va):
    """All Gate-A numbers for one VA from the current sweep object."""
    from msetdiff import load
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True
    rows = report_rows()
    r = rows.get(va.lower())
    if not r:
        return None, 'no report.csv row (unswept)'
    obj = _find_obj(r)
    if not obj:
        return None, 'no sweep object for %s (run the one-file sweep)' % r['file']
    sym = _sym(obj, r['name'])
    if not sym:
        return None, 'symbol %s not in %s' % (r['name'], obj)
    ob = os.path.join(ROOT, 'build', 'match', 'orig', va.upper().replace('0X', '0x') + '.bin')
    if not os.path.exists(ob):
        ob = os.path.join(ROOT, 'build', 'match', 'orig', '0x%08X.bin' % int(va, 16))
    if not os.path.exists(ob):
        return None, 'no original bytes'
    orig = open(ob, 'rb').read()
    # Jump tables live INSIDE the function span (dword entries + byte case
    # maps after the last instruction).  Decoded as instructions they are
    # garbage on both sides -- and DIFFERENT garbage, because the recomp's
    # entries are reloc zeros while the original's are linked addresses.
    # The cut is read from the ORIGINAL's own dispatches: `jmp dword ptr
    # [R*4 + VA]` names the table VA directly.  Instruction gates run on
    # [0, cut); the table zone is compared as BYTES with the reloc'd dwords
    # masked (gate A6).  No dispatch -> cut None -> the old path, so rows
    # without tables measure exactly as before (2026-09-09, 0x10015B10:
    # two dispatches, tables at +0xbec, code insn-identical).
    base = int(va, 16)
    cut = None
    for i in md.disasm(orig, 0):
        mt = re.search(r'\*4 \+ (0x[0-9a-f]+)\]', i.op_str) if i.mnemonic == 'jmp' else None
        if mt:
            toff = int(mt.group(1), 16) - base
            if 0 < toff < len(orig):
                cut = toff if cut is None else min(cut, toff)
    o, no = (load(ob, None, 0, cut) if cut else load(ob, None))
    rc, nr = (load(obj, sym, 0, cut) if cut else load(obj, sym))
    oseq = norm_seq(ob, None, 0, cut) if cut else norm_seq(ob, None)
    rseq = norm_seq(obj, sym, 0, cut) if cut else norm_seq(obj, sym)
    from match_diff import parse_coff_obj
    code, robj_rel = parse_coff_obj(obj)[sym]
    tables_ok, tab_note = True, ''
    if cut is not None:
        oz = bytearray(orig[cut:])
        rz = bytearray(code[cut:len(orig)])
        for k in range(min(len(oz), len(rz))):
            if (cut + k) in robj_rel:
                oz[k] = rz[k] = 0
        ndif = sum(1 for a, b in zip(oz, rz) if a != b) + abs(len(oz) - len(rz))
        tables_ok = ndif == 0
        tab_note = 'tables at +0x%x, %d B, %s' % (
            cut, len(oz), 'byte-equal (relocs masked)' if tables_ok else '%d BYTE DIFF(S)' % ndif)
    ins = list(md.disasm(code, 0))
    while ins and ins[-1].mnemonic in ('nop', 'int3'):
        ins.pop()
    rbytes = (ins[-1].address + ins[-1].size) if ins else 0
    miss, extra = o - rc, rc - o
    um, ue, singles = classify(miss, extra, o, rc)
    # divergence: masked region count + lost-sync
    key = max(3, min(6, no // 8))            # a 12-insn leaf cannot resync on 6
    c = [PY, 'tools/divergence.py', os.path.relpath(obj, ROOT),
         os.path.relpath(ob, ROOT), sym, '--deltas', '--key', str(key), '--mask-slots']
    out = subprocess.run(c, cwd=ROOT, capture_output=True, text=True).stdout
    mreg = re.search(r'total divergence regions from offset 0x0:\s*(\d+)', out)
    regions = int(mreg.group(1)) if mreg else -1
    lost = [l for l in out.splitlines() if 'NEVER COMPARED' in l or 'lost-sync' in l.lower()]
    mb = re.search(r'(\d+) orig bytes \([\d.]+% of the function\) were NEVER COMPARED', out)
    lost_bytes = int(mb.group(1)) if mb else (0 if not lost else 10**6)
    # oracle
    orc = subprocess.run([PY, 'tools/t3b_verify.py', va], cwd=ROOT,
                         capture_output=True, text=True).stdout.strip().splitlines()
    verdict = 'UNCLASSIFIED'
    for l in orc:
        m = re.search(r'\b(EQUIVALENT|DIFF|UNCLASSIFIED)\b', l)
        if m:
            verdict = m.group(1)
    return dict(va=va.lower(), name=r['name'], file=r['file'], status=r['status'], obj=obj,
                obytes=len(orig), rbytes=rbytes, oi=no, ri=nr,
                miss=miss, extra=extra, nmiss=sum(miss.values()), nextra=sum(extra.values()),
                um=um, ue=ue, singles=singles, regions=regions, lost=lost,
                lost_bytes=lost_bytes, key=key, oracle=verdict, pos=positional(oseq, rseq),
                tables_ok=tables_ok, tab_note=tab_note), ''


def gates(m):
    g = []
    gap = abs(m['oi'] - m['ri']); lim = max(GAP_ABS, GAP_FRAC * m['oi'])
    g.append(('A1 insn gap', gap <= lim, '%d (limit %.1f)' % (gap, lim)))
    rows = m['nmiss'] + m['nextra']; rlim = max(ROWS_ABS, ROWS_FRAC * m['oi'])
    g.append(('A2 rows', rows <= rlim, '%d+%d = %d (limit %.1f)' % (m['nmiss'], m['nextra'], rows, rlim)))
    unm = sum(m['um'].values()) + sum(m['ue'].values())
    g.append(('A3 classify', unm == 0, '%d unpaired row(s)' % unm))
    p_ok, p_det = m['pos']
    g.append(('A4 lost-sync', m['lost_bytes'] <= LOST_TAIL_MAX or p_ok,
              ('none (key %d)' % m['key']) if not m['lost'] else
              ('%d B uncompared at key %d, order proven instead: %s'
               % (m['lost_bytes'], m['key'], p_det)) if p_ok else
              '%d B uncompared at key %d (tolerance %d B; A3 proves the multiset); not positional either: %s'
              % (m['lost_bytes'], m['key'], LOST_TAIL_MAX, p_det)))
    g.append(('A5 oracle', m['oracle'] != 'DIFF', m['oracle']))
    if m.get('tab_note'):
        g.append(('A6 tables', m['tables_ok'], m['tab_note']))
    return g


def tag_text(m, date, eff):
    return ('/* @t3 %s %s -- CERTIFIED COMPLETE, NOT BYTE-EXACT.\n'
            ' * @t3-measure bytes %d/%d insns %d/%d rows %d+%d regions %d oracle %s\n'
            ' * @t3-effort passes %d zero-movement %d %d\n'
            ' * <what the residue is, by wall; where the dossier and dead list live;\n'
            ' *  "Do not reopen before the end-grind (CLAUDE.md rule 12)."> */'
            % ('0x%08X' % int(m['va'], 16), date, m['rbytes'], m['obytes'], m['ri'], m['oi'],
               m['nmiss'], m['nextra'], m['regions'], m['oracle'], eff[0], eff[1], eff[2]))


def qualify_all(argv):
    """Gates 0+A+B over every diff row (<= --max-bytes), one line each."""
    mb = int(argv[argv.index('--max-bytes') + 1]) if '--max-bytes' in argv else 400
    rows = [r for r in report_rows().values()
            if r.get('status') == 'diff' and r.get('orig_size') and int(r['orig_size']) <= mb]
    rows.sort(key=lambda r: int(r['orig_size']))
    ready, near = 0, 0
    for r in rows:
        m, why = measure(r['va'])
        if not m:
            print('%s %-30s %5s B  unmeasured: %s' % (r['va'], r['name'], r['orig_size'], why)); continue
        path = os.path.join(ROOT, m['file'])
        c_ok, _ = completeness(r['va'], path)
        g = gates(m); a_ok = all(p for _, p, _ in g)
        b_ok, b_det, _ = gate_b(r['va'], path, m)
        fails = [n for n, p, _ in g if not p]
        if c_ok and a_ok and b_ok:
            ready += 1; verdict = 'READY -- run --qualify %s and paste the tag' % r['va']
        elif c_ok and a_ok:
            near += 1; verdict = 'gate B: ' + b_det
        else:
            verdict = 'FAIL ' + ('0 ' if not c_ok else '') + ' '.join(fails)
        print('%s %-30s %5s B  %s' % (r['va'], r['name'], r['orig_size'], verdict))
    print('%d rows <= %d B: %d certifiable now, %d pass gates 0+A and owe passes' % (len(rows), mb, ready, near))
    return 0


def qualify(argv):
    import datetime
    if not argv or argv[0].startswith('--all'):
        return qualify_all(argv)
    if not re.fullmatch(r'0x[0-9A-Fa-f]{8}', argv[0]):
        print('usage: tools/t3.py --qualify <0xVA>   |   tools/t3.py --qualify --all [--max-bytes N]')
        return 2
    va = argv[0]
    m, why = measure(va)
    if not m:
        print('CANNOT MEASURE: ' + why); return 2
    print('=== %s %s  (%s, status %s)' % (m['va'], m['name'], m['file'], m['status']))
    if m['status'] == 'match':
        print('byte-exact already: T4. No tag.'); return 0
    ok = True
    path = os.path.join(ROOT, m['file'])
    c_ok, c_det = completeness(va, path)
    ok &= c_ok
    print('  %-14s %s  %s' % ('0  complete', 'PASS' if c_ok else 'FAIL', c_det))
    for name, passed, detail in gates(m):
        ok &= passed
        print('  %-14s %s  %s' % (name, 'PASS' if passed else 'FAIL', detail))
    if m['um'] or m['ue']:
        for row, n in m['um'].most_common(): print('     unpaired MISSING %2d  %s' % (n, row))
        for row, n in m['ue'].most_common(): print('     unpaired EXTRA   %2d  %s' % (n, row))
    print('  singletons     %s' % ', '.join('%s x%d' % (r, n) for r, n in m['singles'].most_common()))
    print('  masked regions %d   bytes %d/%d' % (m['regions'], m['rbytes'], m['obytes']))
    print('GATE 0+A: %s' % ('PASS' if ok else 'FAIL -- not certifiable'))
    b_ok, b_det, eff = gate_b(va, path, m)
    print('GATE B (ledger @t4-pass in %s): %s  %s' % (os.path.basename(path), 'PASS' if b_ok else 'FAIL', b_det))
    if ok and b_ok:
        print('Paste directly above the @implements line:\n')
        print(tag_text(m, datetime.date.today().isoformat(), eff))
        return 0
    if ok and not b_ok:
        print('Gate A is a precondition, not the trigger to stop: record each pass at T4 as an')
        print('@t4-pass line in the file header and come back when the ledger meets Gate B.')
    return 1


def main(argv):
    if argv and argv[0] == '--qualify':
        return qualify(argv[1:])
    cert = certified()
    if '--vas' in argv:
        for va in sorted(cert):
            if not va.startswith('?'):
                print(va)
        return 0
    st = report_status()
    bad = 0
    for va, info in sorted(cert.items()):
        flag = 'ok'
        if not info['ok']:
            flag = 'BAD: ' + info['why']; bad += 1
        elif st.get(va) == 'match':
            flag = 'STALE: byte-exact now -- remove the @t3 tag (keep @implements)'; bad += 1
        elif va not in st:
            flag = 'unswept (no report.csv row yet)'
        else:
            m, why = measure(va)
            if m:
                tm = info['measure']
                now = (m['rbytes'], m['obytes'], m['ri'], m['oi'], m['nmiss'], m['nextra'], m['regions'])
                if tuple(tm[:7]) != now:
                    flag = ('STALE-NUMBERS: tag says bytes %d/%d insns %d/%d rows %d+%d regions %d, '
                            'object says %d/%d %d/%d %d+%d %d -- re-run --qualify'
                            % (tm[:7] + now)); bad += 1
                g = gates(m)
                if not all(p for _, p, _ in g):
                    flag = 'FAILS GATE A NOW: ' + ', '.join(n for n, p, _ in g if not p); bad += 1
                path = os.path.join(ROOT, info['file'])
                c_ok, c_det = completeness(va, path)
                if not c_ok:
                    flag = 'FAILS GATE 0: ' + c_det; bad += 1
                b_ok, b_det, eff = gate_b(va, path, m)
                if not b_ok:
                    flag = 'FAILS GATE B: ' + b_det; bad += 1
                elif info['effort'] and tuple(int(x) for x in info['effort']) != eff:
                    flag = 'STALE-EFFORT: tag says %s, ledger says %s -- re-run --qualify' % (info['effort'], eff); bad += 1
            else:
                flag = 'unmeasured (%s)' % why
        print('%s  %s  %s:%d  %s' % (va, info['date'], info['file'], info['line'], flag))
    n = sum(1 for v in cert if not v.startswith('?'))
    print('T3-certified functions: %d  (not counted as matched)' % n)
    if bad:
        print('FAIL: %d bad, stale or failing @t3 tag(s).' % bad)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
