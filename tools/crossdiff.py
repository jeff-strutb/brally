"""Match functions between BRD3D.dll and BRGlide.dll, and classify the rest.

Both DLLs are the same game built against different renderers, so most functions
are byte-identical apart from embedded addresses. We normalise each function by
zeroing every relocated dword and every rel32 CALL/JMP displacement, then hash.

Equal hash across the two builds => shared game code, which is the real
decompilation target.

WHAT `d3d_only` USED TO MEAN, AND WHY IT WAS A TRAP
===================================================

Hashing alone reported 1,809 shared and **888 d3d_only**, and that 888 has
misled at least ten separate passes. `d3d_only` never meant "absent from
BRGlide"; it meant "did not hash the same", which is a much weaker claim, and
the residue turned out to be three completely different things mixed together:

  * the statically linked C runtime. BRD3D links it in; BRGlide imports it from
    MSVCRT.dll. Genuinely absent from BRGlide .text, and nothing to port.

  * game functions that DO exist in BRGlide and cannot ever hash the same,
    because that same CRT linkage difference rewrites their call instructions:

        E8 rel32            BRD3D,  direct call into the linked-in CRT
        FF 15 disp32        BRGlide, indirect call through the IAT

    Different opcode, different length; the length change then moves every
    later byte offset and promotes rel8 branches to rel32. No hash of the bytes
    can match, and no amount of trimming the extents helps. The save/load
    family is this case and passes had been pairing it BY HAND for months.

  * BRD3D renderer code, whose opposite number is Glide code -- correctly
    unpaired, and the only bucket the old label actually described.

So the pipeline now runs several matchers and then partitions what is left. See
`tools/xmatch.py` for how each matcher works and why it is safe. The method is
recorded per row in `matched_by`, weakest evidence last:

    matched_by     the pair is asserted because
    -------------  -------------------------------------------------------
    body           the normalised bytes hash the same, and the hash is
                   UNIQUE on both sides
    prefix         the normalised 64-byte prologues hash the same: the two
                   maps disagreed about the EXTENT, not about the function
    callsite       an aligned CALL site in an already-matched caller. Two
                   compilations of one function call the same callees in
                   the same order, so a caller that matched byte-wise can
                   name a callee that cannot
    ptrsite        the same, for an aligned RELOCATED OPERAND -- the
                   function pointers a dispatcher pushes. This is the only
                   handle on code that has no callers at all
    ptrtable       two dispatch tables that correspond entry for entry
    thunk          both sides are `jmp [IAT]` for the same import
    shape          mutual-best cosine over a token stream in which a call
                   is just "a call", with a margin over the runner-up on
                   BOTH sides, and vetoed by any call-site evidence
    strings        an identical set of rare string literals
    body+X         an ambiguous hash group (the partner is IN the group)
                   narrowed to one member by X
    body-dup:N     an ambiguous hash group that nothing narrowed. N BRGlide
                   functions are byte-identical to this one; the partner is
                   one of them and the evidence does not say which. 406
                   eleven-byte C++ EH funclets hash alike, and the old
                   `byhash[h][0]` recorded ONE BRGlide address as the
                   partner of all 406 of them

Everything still unpaired is classed rather than lumped under one misleading
label:

    crt        the statically linked C runtime, absent from BRGlide because
               BRGlide imports it. Not an address-range guess -- the entry
               points are NAMED, from aligned sites where BRD3D calls a
               function and BRGlide calls MSVCRT.dll or jumps through its IAT
    renderer   a BRGlide function occupies the same slot in the same caller,
               but the two bodies are DIFFERENT CODE: the renderer boundary.
               The BRGlide address is in the note, to read alongside and never
               to port from
    d3d_only   reachable only from those entry points: Direct3D internals
    unknown    the honest residue

Usage:
    crossdiff.py            rebuild config/shared.csv and print the report
    crossdiff.py --check    re-derive and assert every pairing this project
                            found by hand -- xmatch.KNOWN_PAIRS from port/
                            headers, and all 73 rows of the independently
                            built config/renderer_api.csv -- plus a
                            leave-one-out measurement of the similarity
                            matcher's false-pair rate. Run it after touching
                            either matcher.
"""
import sys, os, csv, struct, hashlib, collections

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe as pelib
import xmatch

# Bytes of normalised prologue used by the fallback match. See norm_funcs.
PREFIX_LEN = 64


def norm_funcs(dll, fcsv):
    p = pelib.load(dll)
    text, text_va = p.text()
    relocs = {p.image_base + r for r in p.relocs}
    out = {}
    for r in csv.DictReader(open(fcsv)):
        va, size = int(r['va'], 16), int(r['size'])
        if size < 8:
            continue
        off = va - text_va
        b = bytearray(text[off:off + size])
        # zero relocated dwords (absolute addresses)
        for k in range(size - 3):
            if (va + k) in relocs:
                b[k:k + 4] = b'\0\0\0\0'
        # zero rel32 displacements of E8/E9
        i = 0
        while i < len(b) - 4:
            if b[i] in (0xE8, 0xE9):
                b[i + 1:i + 5] = b'\0\0\0\0'
                i += 5
            else:
                i += 1
        # A PREFIX hash as well as the whole-body one.
        #
        # The body hash covers the function's exact LENGTH, so if the two maps
        # disagree by a few bytes of trailing padding -- which they routinely
        # do, the extents coming from two independent analyses -- a genuinely
        # shared function hashes differently on the two sides and is reported
        # d3d_only. Eight separate passes hit that and each concluded, alone,
        # that shared.csv was wrong "in this one case".
        #
        # The prefix is long enough to be specific (a 64-byte normalised
        # prologue is not something two unrelated functions share) and short
        # enough to be immune to a disagreement about where the body ends.
        pre = bytes(b[:PREFIX_LEN])
        out[va] = (hashlib.sha1(bytes(b)).hexdigest(), size,
                   hashlib.sha1(pre).hexdigest() if len(pre) >= PREFIX_LEN else None)
    return p, out


def run(map_a, map_b, quiet=False):
    """The whole pipeline. Returns (A, B, pairs, groups, buckets, crt_names)."""
    say = (lambda *a: None) if quiet else (lambda *a: print(*a))
    _, Ah = norm_funcs('orig/BRD3D.dll', map_a)
    _, Bh = norm_funcs('orig/BRGlide.dll', map_b)
    A = xmatch.Build('orig/BRD3D.dll', map_a)
    B = xmatch.Build('orig/BRGlide.dll', map_b)

    ha, hb = collections.defaultdict(list), collections.defaultdict(list)
    pa, pb = collections.defaultdict(list), collections.defaultdict(list)
    for va, (h, s, pre) in Ah.items():
        ha[h].append(va)
        if pre:
            pa[pre].append(va)
    for va, (h, s, pre) in Bh.items():
        hb[h].append(va)
        if pre:
            pb[pre].append(va)

    pairs = {}          # d3d_va -> (glide_va, method)
    groups = {}         # d3d_va -> [glide_va, ...]  ambiguous hash group
    for h, av in ha.items():
        bv = hb.get(h)
        if not bv:
            continue
        if len(av) == 1 and len(bv) == 1:
            pairs[av[0]] = (bv[0], 'body')
        else:
            # The hash proves membership of the group and nothing finer.
            # Believing byhash[h][0] here is how one BRGlide address came to be
            # the recorded partner of 406 different BRD3D functions.
            for a in av:
                groups[a] = bv
    say("  body (unique)      %5d      in ambiguous hash groups %d"
        % (len(pairs), len(groups)))

    taken = {g for g, _ in pairs.values()}
    n_pre = 0
    for pre, av in pa.items():
        bv = pb.get(pre)
        if not bv or len(av) != 1 or len(bv) != 1:
            continue
        a, b = av[0], bv[0]
        if a in pairs or a in groups or b in taken:
            continue
        pairs[a] = (b, 'prefix')
        taken.add(b)
        n_pre += 1
    say("  prefix             %5d" % n_pre)

    # --- call-site correspondence, to a fixed point ----------------------
    # `slots` collects the correspondences that are real but are NOT two
    # compilations of one function: the renderer entry points, where the same
    # call site holds a Direct3D body in one build and a Glide body in the
    # other. Keeping them out of `pairs` is what stops them being used as
    # alignment anchors, and what stops shared.csv claiming they are portable.
    restrict = {a: set(bv) for a, bv in groups.items()}
    slots = {}
    allvotes = collections.defaultdict(collections.Counter)
    before = len(pairs)
    crt_votes, _ = xmatch.propagate(A, B, pairs, restrict, slots=slots,
                                    allvotes=allvotes)
    say("  callsite           %5d      (+%d renderer slots held back)"
        % (len(pairs) - before, len(slots)))

    # --- shape, then propagate again through the new anchors -------------
    covered = {g for g, _ in pairs.values()} | {v[0] for v in slots.values()}
    for bv in groups.values():
        covered.update(bv)
    ca = [a for a in A.funcs if a not in pairs and a not in groups
          and a not in slots and A.funcs[a] >= 8]
    cb = [b for b in B.funcs if b not in covered and B.funcs[b] >= 8]
    sh = xmatch.shape_match(A, B, pairs, ca, cb, allvotes)
    for a, (b, s1, s2) in sh.items():
        pairs[a] = (b, 'shape')
    say("  shape              %5d" % len(sh))

    # --- rare-string cross-reference -------------------------------------
    covered = {g for g, _ in pairs.values()} | {v[0] for v in slots.values()}
    for a, bv in groups.items():
        if a not in pairs:
            covered.update(bv)
    ca = [a for a in A.funcs if a not in pairs and a not in groups
          and a not in slots and A.funcs[a] >= 8]
    cb = [b for b in B.funcs if b not in covered and B.funcs[b] >= 8]
    st = xmatch.string_match(A, B, ca, cb)
    for a, b in st.items():
        pairs[a] = (b, 'strings')
    say("  strings            %5d" % len(st))

    before = len(pairs)
    v2, _ = xmatch.propagate(A, B, pairs, restrict, slots=slots, allvotes=allvotes)
    for va, c in v2.items():
        for n, k in c.items():
            crt_votes[va][n] += k
    say("  callsite (round 2) %5d" % (len(pairs) - before))

    # --- ambiguous groups that call-site evidence never reached ----------
    # They stay `shared` -- byte-identical code is byte-identical code -- but
    # the method records how many candidates there were, so nobody reads the
    # partner as a precise identification.
    n_dup = 0
    for a, bv in groups.items():
        if a in pairs:
            continue
        pairs[a] = (bv[0], 'body-dup:%d' % len(bv))
        n_dup += 1
    say("  body-dup           %5d" % n_dup)

    # --- name the statically linked CRT, then partition the residue ------
    crt_names = {}
    for va, c in crt_votes.items():
        if va in pairs:
            continue                    # it is game code, not CRT
        name, n = c.most_common(1)[0]
        if not name.startswith('MSVCRT'):
            # Both builds import DINPUT, WINMM &c., so a name from one of those
            # is a thunk correspondence, not a statically linked body. Only
            # MSVCRT is the linkage difference this bucket is about.
            continue
        if len(c) == 1 or n >= 3 * c.most_common(2)[1][1]:
            crt_names[va] = name
    say("  CRT entry points named from aligned MSVCRT call sites: %d"
        % len(crt_names))
    buckets = xmatch.bucket(A, B, pairs, crt_names, slots)
    return A, B, pairs, groups, buckets, crt_names


def overrides(path='config/shared_overrides.csv'):
    """Hand-adjudicated rows that the generator MUST NOT overwrite.

    WHY THIS EXISTS

    config/shared.csv is generated wholesale by this file, and every run
    rewrote the note column as ''. A cross-build audit then spent a long pass
    adjudicating sixteen rows FROM THE BYTES -- resolving `body-dup:N`
    ambiguity through C++ static ctor/dtor thunk triples and through the
    display-list dispatch tables, and reclassifying four rows `shared` ->
    `renderer` where the two builds genuinely differ. All sixteen carry their
    evidence in the note.

    Every one of them would have been destroyed by the next run of this
    script, silently, with no diff to notice because the file is generated and
    nobody reads its diffs.

    That is the same failure this project already fixed once for the ported
    manifest: a generated index cannot hold a human judgement, because the
    generator has no way to know the judgement happened. The answer there was
    to move the claim into the source and generate the index from it. The
    answer here is the same shape -- the adjudications live in a separate
    checked-in file, and generation applies them on top.

    WHAT BELONGS HERE: a row where reading the two disassemblies settled
    something the matcher cannot settle by itself. Not a guess, and not a
    preference. The note must carry the evidence, because the whole point is
    that the next person can check it rather than re-derive it.

    An override whose address no longer appears in the function map is
    REPORTED as possibly stale rather than dropped in silence.
    """
    out = {}
    if not os.path.exists(path):
        return out
    with open(path) as fh:
        for r in csv.DictReader(fh):
            out['0x%08X' % int(r['d3d_va'], 16)] = r
    return out


def main():
    # Both sides are overridable so the two maps can be varied independently.
    # That matters because a hash match requires BOTH maps to agree about the
    # function's extent: swapping one side for a rebuilt map and watching the
    # match count move measures the OTHER side's extent errors.
    # config/functions.csv is now the FLOW-derived map. The original
    # sweep-derived one is kept as config/functions_d3d_sweep.csv for
    # comparison; it has 181 defective entries in 2,632.
    map_a = os.environ.get('BR_MAP_D3D', 'config/functions.csv')
    map_b = os.environ.get('BR_MAP_GLIDE', 'config/functions_glide.csv')
    out = os.environ.get('BR_SHARED_OUT', 'config/shared.csv')

    if '--check' in sys.argv:
        return check(map_a, map_b)

    print("matching stages:")
    A, B, pairs, groups, buckets, crt_names = run(map_a, map_b)

    over = overrides()

    os.makedirs('config', exist_ok=True)
    with open(out, 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['d3d_va', 'glide_va', 'size', 'class', 'matched_by', 'note'])
        for va in sorted(A.funcs):
            if A.funcs[va] < 8:
                continue
            key = '0x%08X' % va
            if key in over:
                # A HAND-ADJUDICATED ROW WINS. See overrides() for why these
                # exist and why the generator must not overwrite them.
                o = over.pop(key)
                w.writerow([key, o['glide_va'], A.funcs[va],
                            o['class'], o['matched_by'], o['note']])
            elif va in pairs:
                g, how = pairs[va]
                w.writerow([key, '0x%08X' % g, A.funcs[va], 'shared', how, ''])
            else:
                cls, note, gva = buckets.get(va, ('unknown', '', None))
                # A `renderer` row DOES carry a BRGlide address, because the
                # correspondence is real. `matched_by` says `slot`, and the
                # class says `renderer`, so nothing reads it as shared code.
                w.writerow([key, '0x%08X' % gva if gva else '',
                            A.funcs[va], cls, 'slot' if gva else '', note])

    # An override that matched NOTHING is reported rather than dropped: it
    # means the address no longer exists in the map, so the adjudication it
    # records has quietly stopped applying to anything.
    for key in sorted(over):
        print("  WARNING: override for %s matched no row -- stale?" % key)

    # ---------------- report ----------------
    # The report, like the CSV, covers the functions big enough to be evidence
    # of anything. Stubs under 8 bytes are matched too but not tabulated.
    sizes = {va: A.funcs[va] for va in A.funcs if A.funcs[va] >= 8}
    sa = sum(sizes.values())
    npair = [va for va in pairs if va in sizes]
    ss = sum(sizes[va] for va in npair)
    bym = collections.Counter(how.split(':')[0] for va, (_g, how) in pairs.items()
                              if va in sizes)
    print("\nBRD3D functions (>=8 bytes)  %d   %d bytes" % (len(sizes), sa))
    print("BRGlide functions            %d" % sum(1 for v in B.funcs.values() if v >= 8))
    print("matched (shared game code)   %d   %d bytes  (%.1f%% of BRD3D .text)"
          % (len(npair), ss, 100.0 * ss / sa))
    for m, n in bym.most_common():
        print("    %-12s %5d" % (m, n))

    rest = collections.Counter()
    rbytes = collections.Counter()
    for va, (cls, _n, _g) in buckets.items():
        if sizes.get(va):
            rest[cls] += 1
            rbytes[cls] += sizes[va]
    print("\nunpaired  %d   %d bytes" % (sum(rest.values()), sum(rbytes.values())))
    for c in ('crt', 'renderer', 'd3d_only', 'unknown'):
        print("    %-10s %5d  %7d bytes" % (c, rest[c], rbytes[c]))
    grade = collections.Counter()
    for va, (cls, note, _g) in buckets.items():
        if cls == 'crt' and va in sizes:
            grade['named from an aligned MSVCRT call site' if note.startswith('MSVCRT')
                  else 'reached only from a named entry point' if note.startswith('reached')
                  else 'no counterpart, no game caller/callee, in the span'] += 1
    print("\n  crt, by strength of evidence:")
    for k, n in grade.most_common():
        print("    %5d  %s" % (n, k))
    names = sorted(set(crt_names.values()))
    print("    named: " + ", ".join(n.split('!')[1] for n in names[:12]) + " ...")

    # where does the residue sit?
    unk = [(va, sizes[va]) for va, r in buckets.items()
           if r[0] == 'unknown' and va in sizes]
    b32 = collections.Counter()
    for va, s in unk:
        b32[va & ~0x7FFF] += s
    print("\n`unknown` bytes by 32KB block (this is the real residue):")
    for k in sorted(b32):
        print("  %08X %7d %s" % (k, b32[k], '#' * min(60, b32[k] // 200)))


def check(map_a, map_b):
    """Assert the hand-found pairings, and cross-validate the matchers."""
    print("re-deriving...")
    A, B, pairs, groups, buckets, crt_names = run(map_a, map_b, quiet=True)

    print("\n1. pairings found BY HAND by earlier passes and recorded in port/")
    bad = 0
    for d, g, src in xmatch.KNOWN_PAIRS:
        got = pairs.get(d)
        if got and got[0] == g:
            print("   ok        0x%08X -> 0x%08X  by %-13s %s" % (d, g, got[1], src))
        elif got:
            print("   WRONG     0x%08X -> 0x%08X but should be 0x%08X   %s"
                  % (d, got[0], g, src))
            bad += 1
        else:
            print("   unpaired  0x%08X    (should be 0x%08X)   %s" % (d, g, src))
            bad += 1

    # 1b. config/renderer_api.csv is written by tools/apiboundary.py, which
    #     reads the two function maps and NOT shared.csv -- so it is a genuinely
    #     separate derivation, and the nearest thing this project has to an
    #     external answer key. What matters here is CONFLICT: a row where the
    #     two derivations name different BRGlide functions for one BRD3D
    #     function means one of them is sending someone to the wrong place.
    #     `unresolved` is only a coverage difference and is not a failure.
    print("\n1b. config/renderer_api.csv -- derived separately, by apiboundary.py")
    ok = conflict = unresolved = 0
    for r in csv.DictReader(open('config/renderer_api.csv')):
        d, g = int(r['d3d_va'], 16), int(r['glide_va'], 16)
        got = pairs.get(d)
        note = buckets.get(d, ('', '', None))
        if (got and got[0] == g) or note[2] == g:
            ok += 1
        elif got or note[2]:
            conflict += 1
            print("   CONFLICT  0x%08X -> 0x%08X, that file says 0x%08X"
                  % (d, got[0] if got else note[2], g))
            bad += 1
        else:
            unresolved += 1
    print("   agree %d   CONFLICT %d   unresolved %d" % (ok, conflict, unresolved))

    # 2. leave-one-out: can `shape` alone re-find pairs that `body` proved?
    #    Body pairs are the only large set of KNOWN-correct answers available,
    #    so they are what the similarity rule has to be measured against.
    print("\n2. leave-one-out: `shape` re-deriving 300 pairs `body` already proved")
    body = [(a, g) for a, (g, how) in pairs.items() if how == 'body']
    body.sort()
    sample = body[::max(1, len(body) // 300)][:300]
    cands_a = [a for a, _ in sample]
    cands_b = sorted({g for _, g in body})
    sh = xmatch.shape_match(A, B, {}, cands_a, cands_b)
    right = sum(1 for a, (b, _s, _s2) in sh.items() if dict(sample).get(a) == b)
    wrong = len(sh) - right
    print("   recovered %d of %d,  WRONG %d  (a wrong pair is the failure that"
          % (right, len(sample), wrong))
    print("   matters: it sends someone to port the wrong function)")

    # 3. cross-method agreement: does `shape` independently confirm the pairs
    #    `callsite` produced, and vice versa?
    print("\n3. cross-method agreement on the pairs found by call-site alignment")
    cs = [(a, g) for a, (g, how) in pairs.items() if how.endswith('callsite')]
    agree = dis = weak = 0
    for a, g in cs:
        s = xmatch.cosine(xmatch.bag(A, a), xmatch.bag(B, g))
        if s >= 0.66:
            agree += 1
        elif s >= 0.4:
            weak += 1
        else:
            dis += 1
    print("   %d call-site pairs: %d also score >=0.66 by shape, %d 0.40-0.66,"
          % (len(cs), agree, weak))
    print("   %d below 0.40 (independent methods disagreeing)" % dis)

    # 4. the CRT names have to be self-consistent: one BRD3D address must not
    #    be named as two different MSVCRT functions.
    print("\n4. CRT naming")
    print("   %d BRD3D addresses named from aligned MSVCRT call sites, %d distinct names"
          % (len(crt_names), len(set(crt_names.values()))))
    dupe = [n for n, c in collections.Counter(crt_names.values()).items() if c > 1]
    print("   names claimed by more than one address: %d %s"
          % (len(dupe), sorted(x.split('!')[1] for x in dupe)[:6]))
    lo, hi = min(crt_names), max(crt_names)
    print("   all named seeds lie in 0x%08X..0x%08X (%d bytes) -- one contiguous"
          % (lo, hi, hi - lo))
    print("   region, which is what a statically linked CRT looks like")

    print("\nRESULT: %s" % ("PASS" if bad == 0 and wrong == 0 else "FAIL (%d)" % (bad + wrong)))
    return 1 if (bad or wrong) else 0


if __name__ == '__main__':
    sys.exit(main() or 0)
