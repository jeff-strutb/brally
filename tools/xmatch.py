"""Cross-build function matching that survives the CRT linkage difference.

WHY THIS EXISTS
===============

`crossdiff.py` matched functions by hashing their normalised bytes. That finds
everything the two builds compiled identically, and nothing else. It reported
888 BRD3D functions as `d3d_only`, and that number has now misled at least ten
separate passes, each of which independently rediscovered that some of the 888
plainly DO exist in BRGlide.

Two distinct causes were behind the 888:

  1. Extent disagreement between the two maps. Fixed in crossdiff.py by the
     64-byte prologue fallback; it recovered 70.

  2. **BRD3D statically links the CRT; BRGlide imports it from MSVCRT.dll.**
     The same source line compiles to

         E8 rel32                     (BRD3D:  direct call to the linked-in copy)
         FF 15 disp32                 (BRGlide: indirect call through the IAT)

     Different opcode, different length, and the length change cascades --
     every rel8 branch that no longer reaches becomes rel32, and the register
     allocator's spill slots move. The bytes are genuinely different and NO
     hash of them, however normalised or however the extents are trimmed, will
     ever match. The save/load family is this case; passes have been pairing
     those BY HAND via string cross-reference for months.

So this module matches on things that survive the difference:

  callsite  An already-matched pair (A,B) is two compilations of one function,
            so its call sites are in one-to-one order correspondence. Align the
            two token streams and the k-th call in A names the same callee as
            the k-th call in B. A caller that matched byte-wise can therefore
            name a callee that cannot. This is transitive, so it is iterated to
            a fixed point.

  ptrsite   The same alignment, read over RELOCATED OPERANDS instead of call
  ptrtable  targets: the function pointers a dispatcher pushes, and dispatch
            tables walked entry for entry once the two tables correspond. This
            is the only handle on the span rasterisers, which have no callers
            at all -- nothing calls them, a table holds them.

  shape     A token stream in which a call is just "a call" -- `E8 rel32` and
            `FF 15 [IAT]` produce the same token -- and every absolute address
            is just "an address". Cosine over the bag of those tokens plus
            their 3-grams. Accepted only on a MUTUAL best match with a margin
            over the runner-up on both sides, vetoed if the two functions
            reference disjoint non-empty sets of string literals, and vetoed
            outright by any call-site evidence: similarity is the weakest thing
            here and must never overrule a caller.

  strings   The literals a function references are the same in both builds, so
            a distinctive set of them identifies a function when its bytes
            cannot. This is the method the hand pairings used.

Not every correspondence is a shared function. The renderer entry points sit in
the same slot of the same caller in both builds with completely different
bodies in them; `verdict()` separates "the same source compiled twice" from
"the same slot, different code", and only the first becomes a pair.

Naming the CRT falls out of `callsite` for free: where an aligned call site is
`E8 -> X` in BRD3D and `FF 15 [MSVCRT.dll!fread]` in BRGlide, X *is* fread. That
turns the "CRT" bucket from a guess about address ranges into a named,
evidenced set, and the callers of those names are the functions that could not
match byte-wise -- which is the whole story of cause 2, closed.
"""
import sys, os, re, csv, struct, math, collections, difflib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe as pelib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG

_md = Cs(CS_ARCH_X86, CS_MODE_32)
_md.detail = True

# ---------------------------------------------------------------------------
# Pairs established BY HAND, by earlier passes, and recorded in port/ headers.
#
# These are the regression test for this file. They were found by people
# reading both disassemblies, they are load-bearing in the port, and a matcher
# that contradicts one of them is wrong no matter how much coverage it buys.
# `crossdiff.py --check` asserts every one of them.
# ---------------------------------------------------------------------------
KNOWN_PAIRS = [
    # (d3d, glide, where it is recorded)
    (0x100709A0, 0x10069930, 'br_save.h   save writer'),
    (0x10070610, 0x100695C0, 'br_save.h   save reader'),
    (0x1005CF20, 0x10055D40, 'br_save.h   season scan'),
    (0x1006F0C0, 0x10068070, 'br_phys.h   wheel/ground'),
    (0x1006F310, 0x100682C0, 'br_phys.h   wheel/ground'),
    (0x1006F4A0, 0x10068450, 'br_phys.h   wheel/ground'),
    (0x10066E90, 0x1005FF00, 'br_race.h   lap/gate'),
    (0x10018590, 0x10015B10, 'br_font.h   text emitter'),
    (0x10061720, 0x1005A7A0, 'br_phys.h   per-frame step'),
    (0x100306C0, 0x10029D70, 'br_dl.c     slice1_05 owner'),
]


class Build:
    """One DLL plus its function map, with a disassembly cache."""

    def __init__(self, dll, fcsv):
        self.path = dll
        self.p = pelib.load(dll)
        self.text, self.text_va = self.p.text()
        self.relocs = {self.p.image_base + r for r in self.p.relocs}
        self.imports = self.p.imports
        self.funcs = {}
        for r in csv.DictReader(open(fcsv)):
            self.funcs[int(r['va'], 16)] = int(r['size'])
        self._dis = {}
        self._tok = {}
        self._str = {}
        self._callers = None
        self._ptab = None

    def body(self, va):
        off = va - self.text_va
        return self.text[off:off + self.funcs[va]]

    def dis(self, va):
        d = self._dis.get(va)
        if d is None:
            d = self._dis[va] = list(_md.disasm(self.body(va), va))
        return d

    # -- tokenisation -----------------------------------------------------
    def tokens(self, va):
        """(tokens, calls, refs) -- a build-agnostic rendering of the function.

        `refs` are the relocated operand VALUES, in instruction order: the
        function pointers a dispatcher pushes, the globals it reads, the tables
        it indexes. They align exactly as call targets do, and they are the
        only handle on the renderer, whose bodies are reached through pointers
        rather than through calls and so have no callers at all.

        Abstracted away, because these are exactly what the two builds are
        entitled to differ in:
          * how a call reaches its target (`E8 rel32` vs `FF 15 [IAT slot]`)
            -- both become the operand token `C`;
          * the value of any relocated operand (a global, a string, a vtable)
            -- becomes `A` or `[A]`.
        Branch targets INSIDE the function become instruction-index deltas, not
        byte displacements, because the call-encoding difference moves every
        byte offset after it and promotes rel8 branches to rel32.
        """
        t = self._tok.get(va)
        if t is not None:
            return t
        ins = self.dis(va)
        end = va + self.funcs[va]
        idx = {i.address: k for k, i in enumerate(ins)}
        toks, calls, refs = [], [], []
        for k, i in enumerate(ins):
            m = i.mnemonic
            parts = [m]
            for o in i.operands:
                if o.type == X86_OP_REG:
                    parts.append(i.reg_name(o.reg))
                elif o.type == X86_OP_IMM:
                    tgt = o.imm
                    if m == 'call':
                        parts.append('C')
                        calls.append((k, tgt))
                    elif m[0] == 'j' or m.startswith('loop'):
                        if va <= tgt < end and tgt in idx:
                            parts.append('L%d' % (idx[tgt] - k))
                        else:                      # tail call out of the body
                            parts.append('C')
                            calls.append((k, tgt))
                    elif self._has_reloc(i):
                        parts.append('A')
                        refs.append((k, tgt & 0xFFFFFFFF))
                    else:
                        parts.append('#%x' % (tgt & 0xFFFFFFFF))
                elif o.type == X86_OP_MEM:
                    mem = o.mem
                    disp = mem.disp & 0xFFFFFFFF
                    absolute = (mem.base == 0 and mem.index == 0)
                    if absolute and disp in self.imports:
                        if m in ('call', 'jmp'):
                            parts.append('C')
                            calls.append((k, ('imp', self.imports[disp])))
                        else:
                            parts.append('[IMP]')
                    elif absolute:
                        parts.append('[A]')
                        refs.append((k, disp))
                    else:
                        b = i.reg_name(mem.base) if mem.base else '-'
                        x = i.reg_name(mem.index) if mem.index else '-'
                        d = 'A' if self._has_reloc(i) else '%x' % disp
                        parts.append('[%s+%s*%d+%s]' % (b, x, mem.scale, d))
            toks.append(' '.join(parts))
        t = self._tok[va] = (toks, calls, refs)
        return t

    def _has_reloc(self, i):
        for k in range(i.size - 3):
            if (i.address + k) in self.relocs:
                return True
        return False

    # -- literals ---------------------------------------------------------
    def strings(self, va):
        """ASCII literals the function references through a relocated operand."""
        s = self._str.get(va)
        if s is not None:
            return s
        body = self.body(va)
        out = set()
        for k in range(len(body) - 3):
            if (va + k) not in self.relocs:
                continue
            tgt = struct.unpack_from('<I', body, k)[0]
            raw = self.p.read(tgt, 96)
            if not raw:
                continue
            m = re.match(rb'[\x09\x0a\x0d\x20-\x7e]{5,}', raw)
            if m:
                out.add(m.group().decode('latin1'))
        s = self._str[va] = frozenset(out)
        return s

    def import_thunk(self, va):
        """'MSVCRT.dll!_ftol' if this "function" is just `jmp [IAT slot]`.

        BRGlide's map contains these six-byte thunks as functions. They are the
        other half of the linkage story: BRD3D's 39-byte statically linked
        _ftol corresponds, at 232 aligned sites, to a six-byte jump through the
        import table. Recognising them turns what reads as a wild size mismatch
        into a NAME for the BRD3D function.
        """
        ins = self.dis(va)
        if len(ins) != 1 or ins[0].mnemonic != 'jmp':
            return None
        for o in ins[0].operands:
            if o.type == X86_OP_MEM and o.mem.base == 0 and o.mem.index == 0:
                return self.imports.get(o.mem.disp & 0xFFFFFFFF)
        return None

    def callers(self):
        if self._callers is None:
            c = collections.defaultdict(set)
            for va in self.funcs:
                for _, t in self.tokens(va)[1]:
                    if not isinstance(t, tuple):
                        c[t].add(va)
            self._callers = c
        return self._callers


# ---------------------------------------------------------------------------
# fingerprints
# ---------------------------------------------------------------------------
def bag(B, va):
    toks = B.tokens(va)[0]
    b = collections.Counter(toks)
    for i in range(len(toks) - 2):
        b['3:' + '|'.join(toks[i:i + 3])] += 1
    return b


def cosine(a, b):
    if not a or not b:
        return 0.0
    small, large = (a, b) if len(a) < len(b) else (b, a)
    num = 0
    for k, v in small.items():
        w = large.get(k)
        if w:
            num += v * w
    if not num:
        return 0.0
    na = math.sqrt(sum(v * v for v in a.values()))
    nb = math.sqrt(sum(v * v for v in b.values()))
    return num / (na * nb)


# ---------------------------------------------------------------------------
# stage: call-site correspondence
# ---------------------------------------------------------------------------
# Below this token-stream similarity a pair is not a reliable enough alignment
# to read callee correspondence out of. Body-identical pairs score 1.0; the
# hand-recorded divergent pairs score 0.72-0.96.
ALIGN_MIN = 0.60


def align_calls(A, B, a, b, which=1):
    """Operand correspondences readable from an aligned pair (a, b).

    `which` selects the operand stream: 1 = call targets, 2 = relocated
    absolute operands. Returns [(target_a, target_b), ...] for the sites that
    line up. Identical token streams (the byte-matched majority) align
    trivially; the rest go through difflib, whose matching blocks ARE the
    aligned regions -- difflib is being used as an alignment algorithm here,
    not as a similarity score.
    """
    ta = A.tokens(a)
    tb = B.tokens(b)
    da, db = dict(ta[which]), dict(tb[which])
    if not da or not db:
        return []
    if ta[0] == tb[0]:
        return [(da[k], db[k]) for k in da if k in db]
    sm = difflib.SequenceMatcher(None, ta[0], tb[0], autojunk=False)
    if sm.quick_ratio() < ALIGN_MIN or sm.ratio() < ALIGN_MIN:
        return []
    out = []
    for i, j, n in sm.get_matching_blocks():
        for k in range(n):
            if (i + k) in da and (j + k) in db:
                out.append((da[i + k], db[j + k]))
    return out


def pointer_tables(Bl):
    """Runs of >=3 consecutive relocated dwords that all point into .text.

    Returns {start_va: [target, ...]}. A run like this is a dispatch table, and
    a dispatch table is the only thing that references the span rasterisers --
    they have no callers, which is exactly why the call graph could not see
    them.
    """
    if Bl._ptab is not None:
        return Bl._ptab
    lo, hi = Bl.text_va, Bl.text_va + len(Bl.text)
    ptr = {}
    for r in Bl.relocs:
        raw = Bl.p.read(r, 4)
        if raw and len(raw) == 4:
            t = struct.unpack('<I', raw)[0]
            if lo <= t < hi:
                ptr[r] = t
    out, addrs = {}, sorted(ptr)
    i = 0
    while i < len(addrs):
        j = i
        while j + 1 < len(addrs) and addrs[j + 1] == addrs[j] + 4:
            j += 1
        if j - i + 1 >= 3:
            out[addrs[i]] = [ptr[addrs[i] + 4 * k] for k in range(j - i + 1)]
        i = j + 1
    Bl._ptab = out
    return out


def callee_agreement(A, B, pairs, a, b):
    """Of this pair's callees that are already paired, how many land in b's?

    Independent of instruction-level similarity, which is what makes it useful
    on the big functions where a cosine gets diluted.  Returns (fraction, n) or
    (None, 0) when there is nothing to measure.
    """
    sa = {t for _, t in A.tokens(a)[1] if not isinstance(t, tuple) and t in A.funcs}
    sb = {t for _, t in B.tokens(b)[1] if not isinstance(t, tuple) and t in B.funcs}
    mapped = {pairs[x][0] for x in sa if x in pairs}
    if not mapped or not sb:
        return None, 0
    return len(mapped & sb) / len(mapped), len(mapped)


# A call-site correspondence says the two functions occupy the same slot in the
# same caller. That is NOT the same claim as "the same code compiled twice":
# the renderer entry points occupy one slot and have two implementations. These
# are the lines that separate the two.
SAME_CODE = 0.55        # cosine at or above which the bodies agree
SLOT_MIN = 0.15         # below this the correspondence itself is not credible


def verdict(A, B, pairs, a, b, votes=1, crt_votes=None):
    """'same' | 'slot' | 'no' for a proposed correspondence.

    These are three different claims and conflating them is what produced the
    888 in the first place:

      same  two compilations of ONE piece of source. Portable once.
      slot  the same slot in the same caller, holding DIFFERENT code -- the
            renderer boundary. The BRGlide address is worth reading alongside
            and is worthless to port from. `config/renderer_api.csv` is a
            hand-built list of exactly these, and it is 1934 bytes of Direct3D
            against 914 of Glide in one case: a real correspondence between
            two functions that share nothing but a call site.
      no    not enough evidence, or a direct contradiction.

    A WRONG pairing is worse than no pairing, so every cheap contradiction is
    decisive AGAINST 'same'. But note which contradictions apply to which
    verdict: differing size, differing callees and differing strings all
    disprove "the same source compiled twice" while being the NORMAL state of
    affairs for two implementations of one renderer entry point. Only the CRT
    naming rules out both, because BRGlide has no such function at all.
    """
    if crt_votes:
        c = crt_votes.get(a)
        if c and sum(c.values()) >= max(2, votes):
            return 'no'
    if min(A.funcs[a], B.funcs[b]) < 8 and votes < 3:
        # A body this short is as likely to be a map artefact as a function, so
        # it needs the correspondence to be attested from several independent
        # call sites before it counts. 0x1007C8A0 has 232 of them.
        return 'no'

    la, lb = A.strings(a), B.strings(b)
    agr, n = callee_agreement(A, B, pairs, a, b)
    sa, sb = A.funcs[a], B.funcs[b]
    same_size = min(sa, sb) * 2 >= max(sa, sb)
    contradicted = ((la and lb and not (la & lb))
                    or (agr is not None and n >= 3 and agr == 0.0)
                    or not same_size)

    if not contradicted:
        if la & lb:
            return 'same'                 # shared rare literals
        if agr is not None and n >= 3 and agr >= 0.75:
            return 'same'                 # its callees are each other's
        if cosine(bag(A, a), bag(B, b)) >= SAME_CODE:
            return 'same'
    # Not the same code. Is the correspondence itself worth recording? It came
    # from an aligned site in a pair that itself aligned, and it is mutual and
    # unique -- so yes, unless it rests on a single site AND looks like noise.
    if votes >= 2 or cosine(bag(A, a), bag(B, b)) >= SLOT_MIN or not contradicted:
        return 'slot'
    return 'no'


def propagate(A, B, pairs, restrict=None, max_rounds=12, slots=None, allvotes=None):
    """Iterate call-site correspondence to a fixed point.

    `pairs` is mutated: {d3d_va: (glide_va, method)}. Returns the votes so the
    caller can also read the BRD3D-static-CRT names out of them.

    `restrict` narrows the admissible partners for particular BRD3D functions:
    {d3d_va: {allowed glide_va, ...}}. It carries the hash groups that are
    ambiguous -- 406 identical eleven-byte EH funclets hash alike, so the hash
    proves the partner is IN the group and nothing more. A call-site vote that
    lands inside the group picks the member; a vote outside it is a contra-
    diction and is dropped rather than believed.
    """
    restrict = restrict or {}
    slots = slots if slots is not None else {}
    allvotes = allvotes if allvotes is not None else collections.defaultdict(collections.Counter)
    taken = {g for g, _ in pairs.values()}
    crt_votes = collections.defaultdict(collections.Counter)
    added_total = 0
    ptab_a, ptab_b = pointer_tables(A), pointer_tables(B)
    method = {}
    for rnd in range(max_rounds):
        votes = collections.defaultdict(collections.Counter)
        rvotes = collections.defaultdict(collections.Counter)
        dvotes = collections.defaultdict(collections.Counter)   # data <-> data

        def cast(ta, tb, kind):
            if ta in A.funcs and tb in B.funcs:
                votes[ta][tb] += 1
                rvotes[tb][ta] += 1
                allvotes[ta][tb] += 1
                method.setdefault((ta, tb), kind)

        for a, (b, _how) in list(pairs.items()):
            for ta, tb in align_calls(A, B, a, b, 1):
                if isinstance(ta, tuple):
                    continue
                if isinstance(tb, tuple):
                    # BRD3D calls a real function where BRGlide calls an
                    # import: that names the statically linked CRT copy.
                    crt_votes[ta][tb[1]] += 1
                    continue
                thunk = tb in B.funcs and B.import_thunk(tb)
                if thunk:
                    if A.import_thunk(ta) == thunk:
                        cast(ta, tb, 'thunk')     # both are thunks for it
                    else:
                        crt_votes[ta][thunk] += 1     # ... or jumps through one
                    continue
                cast(ta, tb, 'callsite')
            # Relocated operands align the same way call targets do. A
            # dispatcher that pushes eight function pointers in both builds
            # pairs all eight of them, and those pointers are the renderer --
            # code with no callers at all, invisible to the call graph.
            for ta, tb in align_calls(A, B, a, b, 2):
                if ta in A.funcs and tb in B.funcs:
                    thunk = B.import_thunk(tb)
                    if thunk:
                        if A.import_thunk(ta) == thunk:
                            cast(ta, tb, 'thunk')
                        else:
                            crt_votes[ta][thunk] += 1
                        continue
                    cast(ta, tb, 'ptrsite')
                elif ta not in A.funcs and tb not in B.funcs:
                    dvotes[ta][tb] += 1

        # Two paired dispatch tables are two orderings of the same list. Walk
        # them together.
        for da, cnt in dvotes.items():
            db, n = cnt.most_common(1)[0]
            if len(cnt) > 1 and cnt.most_common(2)[1][1] == n:
                continue
            la, lb = ptab_a.get(da), ptab_b.get(db)
            if not la or not lb or len(la) != len(lb):
                continue
            for ta, tb in zip(la, lb):
                cast(ta, tb, 'ptrtable')

        added = 0
        for ta, cnt in votes.items():
            if ta in pairs or ta in slots:
                continue
            allow = restrict.get(ta)
            if allow is not None:
                cnt = collections.Counter({k: v for k, v in cnt.items() if k in allow})
                if not cnt:
                    continue
            best, n = cnt.most_common(1)[0]
            if len(cnt) > 1 and cnt.most_common(2)[1][1] == n:
                continue                      # two callees tie: no evidence
            if best in taken:
                continue
            rc = rvotes[best]
            rbest, rn = rc.most_common(1)[0]
            if rbest != ta or (len(rc) > 1 and rc.most_common(2)[1][1] == rn):
                continue                      # not mutual
            v = verdict(A, B, pairs, ta, best, n, crt_votes)
            if v == 'no':
                continue
            how = method.get((ta, best), 'callsite')
            if v == 'slot':
                # Same slot in the same caller, different code in it. This is
                # the renderer boundary, not shared source; it must not become
                # an anchor for further propagation.
                slots[ta] = (best, n, how)
                taken.add(best)
                continue
            pairs[ta] = (best, 'body+' + how if allow is not None else how)
            taken.add(best)
            added += 1
        added_total += added
        if not added:
            break
    return crt_votes, added_total


# ---------------------------------------------------------------------------
# stage: shape
# ---------------------------------------------------------------------------
SHAPE_MIN = 0.66      # lowest score among the hand-recorded pairs is 0.717
SHAPE_MARGIN = 0.05   # over the runner-up, required on BOTH sides
SHAPE_MIN_INSN = 12   # short bodies are not distinctive; leave them alone


def shape_match(A, B, pairs, cands_a, cands_b, votes=None):
    """Mutual-best cosine over the abstracted token bags.

    The mutual-best-plus-margin rule is what makes this safe: the failure mode
    of a similarity score is a family of near-identical siblings, and in such a
    family nobody clears the margin, so the whole family is left unpaired
    rather than paired at random.

    `votes` are the call-site correspondences, INCLUDING the ones propagation
    declined to act on. They veto. Similarity is the weakest evidence here and
    it must never overrule a caller: 0x10073820 is 291 bytes, its true partner
    0x1006C790 is 105, and left to itself the cosine confidently proposes an
    unrelated 198-byte function that happens to look like it. The caller says
    otherwise, and the caller is right.
    """
    votes = votes or {}
    voted_b = {b for c in votes.values() for b in c}
    ba = {a: bag(A, a) for a in cands_a}
    bb = {b: bag(B, b) for b in cands_b}
    na = {a: len(A.tokens(a)[0]) for a in cands_a}
    nb = {b: len(B.tokens(b)[0]) for b in cands_b}

    fwd, rev = {}, collections.defaultdict(list)
    for a in cands_a:
        if na[a] < SHAPE_MIN_INSN or a in votes:
            continue
        scored = []
        for b in cands_b:
            if nb[b] < SHAPE_MIN_INSN or b in voted_b:
                continue
            lo, hi = (na[a], nb[b]) if na[a] < nb[b] else (nb[b], na[a])
            if lo * 10 < hi * 6:          # length ratio below 0.6: skip
                continue
            s = cosine(ba[a], bb[b])
            if s >= 0.4:
                scored.append((s, b))
        if not scored:
            continue
        scored.sort(reverse=True)
        fwd[a] = scored
        for s, b in scored:
            rev[b].append((s, a))

    out = {}
    for b in rev:
        rev[b].sort(reverse=True)
    for a, scored in fwd.items():
        s1, b = scored[0]
        s2 = scored[1][0] if len(scored) > 1 else 0.0
        if s1 < SHAPE_MIN or s1 - s2 < SHAPE_MARGIN:
            continue
        r = rev[b]
        if r[0][1] != a:
            continue
        r2 = r[1][0] if len(r) > 1 else 0.0
        if r[0][0] - r2 < SHAPE_MARGIN:
            continue
        if verdict(A, B, pairs, a, b) != 'same':
            continue
        out[a] = (b, s1, s2)
    return out


# ---------------------------------------------------------------------------
# stage: strings
# ---------------------------------------------------------------------------
# A literal that dozens of functions reference identifies nothing. Only sets
# whose members are rare on both sides are evidence.
STR_MAX_USERS = 3


def string_match(A, B, cands_a, cands_b):
    ua = collections.Counter()
    ub = collections.Counter()
    for a in A.funcs:
        for s in A.strings(a):
            ua[s] += 1
    for b in B.funcs:
        for s in B.strings(b):
            ub[s] += 1

    def key(Bl, va, u):
        rare = frozenset(s for s in Bl.strings(va) if u[s] <= STR_MAX_USERS)
        return rare if len(rare) >= 2 else None

    ka = {}
    for a in cands_a:
        k = key(A, a, ua)
        if k:
            ka.setdefault(k, []).append(a)
    kb = {}
    for b in cands_b:
        k = key(B, b, ub)
        if k:
            kb.setdefault(k, []).append(b)
    out = {}
    for k, av in ka.items():
        bv = kb.get(k)
        if bv and len(av) == 1 and len(bv) == 1 and \
                verdict(A, B, {}, av[0], bv[0]) != 'no':
            out[av[0]] = bv[0]
    return out


# ---------------------------------------------------------------------------
# stage: buckets
# ---------------------------------------------------------------------------
def bucket(A, B, pairs, crt_names, slots=None):
    """Partition the still-unpaired BRD3D functions. This is the whole point.

    Returns {d3d_va: (bucket, note)} over every BRD3D function not in `pairs`.
    The note is the EVIDENCE, and it goes into shared.csv so a reader can weigh
    it instead of trusting the label:

      crt        the statically linked C runtime. BRGlide imports these from
                 MSVCRT.dll, so they are genuinely absent from its .text: a
                 LINKAGE difference, not a code difference, and nothing to
                 port. Three grades of evidence, weakest last.
      renderer   an entry point whose BRGlide counterpart is a DIFFERENT
                 IMPLEMENTATION -- the renderer boundary. The correspondence is
                 real and the BRGlide address is in the note; the code is not
                 shared and must not be ported from either side as if it were.
      d3d_only   reachable only from those entry points: the Direct3D
                 implementation's own internals. Nothing in BRGlide to pair.
      unknown    none of the above. This is the honest residue, and it is what
                 a pass looking for unported game code should read.
    """
    slots = slots or {}
    callers = A.callers()
    callee = collections.defaultdict(set)
    for va in A.funcs:
        for _, t in A.tokens(va)[1]:
            if not isinstance(t, tuple) and t in A.funcs:
                callee[va].add(t)
    game = set(pairs)
    unpaired = {va for va in A.funcs if va not in pairs}
    out = {}

    # -- crt, grade 1: NAMED. An aligned call site has BRD3D calling this
    #    function where BRGlide calls a named MSVCRT export. That is not a
    #    guess about address ranges; it is the two builds telling us.
    for va, name in crt_names.items():
        out[va] = ('crt', name.replace('.dll!', '!'), None)

    # -- crt, grade 2: only reachable from the CRT. Walk down the call graph
    #    from the named entry points, through unpaired functions only.
    crt = set(crt_names)
    work = list(crt)
    while work:
        v = work.pop()
        for c in callee[v]:
            if c not in crt and c in unpaired:
                crt.add(c)
                work.append(c)
                out[c] = ('crt', 'reached only from the named CRT', None)

    # -- crt, grade 3: inside the span the named entry points occupy, with no
    #    call relationship to any function that exists in BOTH builds, and
    #    referencing no literal that appears anywhere in BRGlide.dll. Weaker
    #    than grades 1-2 and labelled as such, but each clause is checkable:
    #    game code has game callers, game callees or game strings.
    if crt_names:
        lo, hi = min(crt_names), max(crt_names)
        graw = open(B.path, 'rb').read()
        for va in sorted(unpaired):
            if va in out or not (lo <= va <= hi):
                continue
            if (callers.get(va) or set()) & game or callee[va] & game:
                continue
            if any(s.encode('latin1') in graw for s in A.strings(va)):
                continue
            out[va] = ('crt', 'no counterpart, no game caller or callee, '
                              'inside the named-CRT span', None)

    # -- renderer: an aligned site puts this function and a BRGlide function
    #    in the same slot of the same caller, but the two bodies are not the
    #    same code. That is the renderer boundary: one API, two implementations.
    #    The BRGlide address is recorded because it is the thing to READ
    #    alongside -- never the thing to port from.
    for va, (g, n, how) in slots.items():
        if va in pairs or va in out:
            continue
        out[va] = ('renderer', 'same slot, %d aligned %s%s; DIFFERENT CODE'
                               % (n, how, '' if n == 1 else 's'), g)

    # -- d3d_only: the Direct3D implementation's own internals. Reachable
    #    downward from a renderer entry point, unpaired, and called by nothing
    #    that exists in both builds -- if a function that exists in BRGlide
    #    called it, it would not be renderer-private.
    ren = {va for va, r in out.items() if r[0] == 'renderer'}
    work = list(ren)
    while work:
        v = work.pop()
        for c in callee[v]:
            if c in out or c not in unpaired:
                continue
            if (callers.get(c) or set()) & game:
                continue
            out[c] = ('d3d_only', 'reached from a renderer entry point; no '
                                  'caller that exists in BRGlide', None)
            work.append(c)

    for va in unpaired:
        out.setdefault(va, ('unknown', '', None))
    return out
