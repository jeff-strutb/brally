"""Discover function boundaries in a PE's .text by EVIDENCE + FLOW.

This replaces tools/funcmap.py's method, which had three structural faults that
between them produced every documented failure mode of config/functions.csv and
config/functions_glide.csv.

1. IT SWEPT FOR 0xE8 BYTES LINEARLY.  x86 is not self-synchronising, so most
   0xE8 bytes in a 480 KB .text are not calls at all -- they are operand bytes.
   Each one still yields a plausible-looking rel32 "target", and the sweep fed
   those straight in as certain function starts.

   This is the origin of the project's most expensive map error.  The Glide text
   emitter's supposed second entry at 0x10015F0B is "called" from 0x100239FE,
   where the bytes are `C1 E8 08` -- `shr eax, 8`.  The 0xE8 is the ModRM byte
   of a shift.  Nothing calls 0x10015F0B; it is a jump target in the middle of
   one 3050-byte function, and splitting it there produced the published claim
   that the Glide emitter is a third the size of D3D's.  It is slightly larger.

   Here, call targets are only believed when the 0xE8 is at an address that
   recursive descent has already decoded as an instruction start.  Seeds that
   cannot be misaligned (PE entry, exports, relocated function pointers) start
   the process; call targets are then harvested and the walk repeated to a
   fixpoint.

2. IT PROMOTED RELOCATED .text POINTERS TO FUNCTION STARTS.  Switch-table
   entries are relocated and point mid-function; the old filter ("accept if it
   follows padding or a terminator") lets plenty through, since a case label
   very often follows the `jmp` that ends the previous case.  Here, WHERE the
   pointer LIVES decides what it is: relocated dwords stored inside .text are
   switch tables by construction and are never entry points; only those stored
   in .rdata/.data (vtables, callback tables) are.

3. IT SET EVERY EXTENT TO "BYTES UNTIL THE NEXT START".  That makes an extent a
   statement about the next function rather than this one: a missed entry
   silently fuses two functions, an invented entry silently truncates one, and
   because the padding trimmer knew 0xC3 (`ret`) but not 0xC2 (`ret imm16`),
   __thiscall pairs merged.  Here an extent is the contiguous run of the
   function's OWN instructions, found by following every path to a terminator.
   Trailing switch tables and their byte index tables are recovered and
   attributed as data, so they are excluded from the code extent.

Usage:
    python tools/funcmap2.py orig/BRGlide.dll config/functions_glide.csv
    python tools/funcmap2.py --report orig/BRGlide.dll     # analyse, no write
    python tools/funcmap2.py --json out.json orig/BRGlide.dll
"""
import sys, os, csv, json, struct, bisect
sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_OP_IMM, CS_OP_MEM

# Paths end here.  `ret imm16` is covered by the startswith('ret') test below
# and is called out because omitting it is precisely the bug that merged this
# codebase's __thiscall function pairs.
TERMINATORS = {'ret', 'retf', 'iret', 'iretd', 'ud2', 'int3'}
PAD = (0xCC, 0x90)
NOTCACHED = object()


class Image:
    def __init__(self, path):
        self.path = path
        self.p = pelib.load(path)
        self.text, self.text_va = self.p.text()
        self.lo = self.text_va
        self.hi = self.text_va + len(self.text)
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.md.detail = True
        self._ic = {}
        self.tables, self.isdata = switch_table_map(self)

    def inside(self, va):
        return self.lo <= va < self.hi

    def is_data(self, va):
        return self.inside(va) and self.isdata[va - self.text_va]

    def byte(self, va):
        return self.text[va - self.text_va]

    def insn(self, va):
        """Decode one instruction at va, cached.  False if it does not decode."""
        i = self._ic.get(va, NOTCACHED)
        if i is NOTCACHED:
            if not self.inside(va):
                return None
            o = va - self.text_va
            got = list(self.md.disasm(self.text[o:o + 16], va, count=1))
            i = self._ic[va] = got[0] if got else False
        return i


# ---------------------------------------------------------------------------
# evidence
# ---------------------------------------------------------------------------

def relocated_pointers(im):
    """Split relocated dwords that point into .text by where they LIVE.

    outside .text -> vtable / callback table entry: a function pointer.
    inside  .text -> switch-table entry: a label, never an entry point.
    """
    p = im.p
    fnptr, tablecell = set(), {}
    for rva in sorted(p.relocs):
        o = p.rva_to_off(rva)
        if o is None or o + 4 > len(p.data):
            continue
        v = struct.unpack('<I', p.data[o:o + 4])[0]
        if not im.inside(v):
            continue
        site = p.image_base + rva
        if im.inside(site):
            tablecell[site] = v
        else:
            fnptr.add(v)
    return fnptr, tablecell


def switch_table_map(im):
    """Locate every switch table in .text from RELOCATIONS, before any code is
    disassembled, and return ({start: nbytes}, isdata bytearray).

    A jump table is a run of consecutive relocated dwords, at stride 4, whose
    values all land in .text.  The stride-4 run is what distinguishes a table
    from an ordinary relocated operand: BRGlide has 1020 relocated dwords
    living inside .text, but 720 of them are ISOLATED -- they are `push
    <handler>` immediates in SEH prologues, and lea/mov disp32 operands.  Those
    sit inside instructions, so treating "relocated dword in .text" as "table
    cell" (or, as the old map did, as a function pointer) misreads real code.

    Knowing the tables up front is what stops the .text sweep from inventing
    functions inside them.  0x100047C8 in BRGlide is the worked example: eight
    table cells that disassemble as `xlatb; cmp; adc; ...` followed by 225
    bytes of `07` index entries reading as `pop es` over and over.  The old map
    lists it as a 312-byte function.
    """
    p = im.p
    cells = []
    for rva in sorted(p.relocs):
        o = p.rva_to_off(rva)
        if o is None or o + 4 > len(p.data):
            continue
        site = p.image_base + rva
        if not im.inside(site):
            continue
        v = struct.unpack('<I', p.data[o:o + 4])[0]
        if im.inside(v):
            cells.append(site)

    tables, isdata = {}, bytearray(len(im.text))
    i = 0
    while i < len(cells):
        j = i
        while j + 1 < len(cells) and cells[j + 1] == cells[j] + 4:
            j += 1
        n = j - i + 1
        if n >= 2:                       # >=2 cells at stride 4 => a table
            start = cells[i]
            tables[start] = 4 * n
            for k in range(start - im.text_va, start - im.text_va + 4 * n):
                isdata[k] = 1
        i = j + 1
    return tables, isdata


# MSVC pads between functions with int3/nop and aligns with `mov edi,edi`
# (8B FF) and `xchg ax,ax` (66 90).  A padding filter that knows only 0xCC and
# 0x90 leaves those two bytes exposed, and a sweep then starts a "function"
# on them that runs straight into the jump table they were padding for.
ALIGN2 = (bytes([0x8B, 0xFF]), bytes([0x66, 0x90]))


def filler_len(im, va):
    """Bytes of inter-object padding/alignment at va, 0 if none."""
    o = va - im.text_va
    if o < 0 or o >= len(im.text):
        return 0
    if im.text[o] in PAD:
        return 1
    if im.text[o:o + 2] in ALIGN2:
        return 2
    return 0


def naive_call_sweep(im):
    """Every 0xE8 rel32 read linearly -- INCLUDING the misaligned ones.

    Kept only as weak corroboration for gap candidates, and to measure how much
    of the old map came from bytes that are not instructions.
    """
    t, tva, out = im.text, im.text_va, set()
    i, n = 0, len(t) - 5
    while i < n:
        if t[i] == 0xE8:
            rel = struct.unpack('<i', t[i + 1:i + 5])[0]
            tgt = tva + i + 5 + rel
            if im.inside(tgt):
                out.add(tgt)
            i += 5
        else:
            i += 1
    return out


# ---------------------------------------------------------------------------
# flow
# ---------------------------------------------------------------------------

def walk(im, entry, others):
    """Follow every path from `entry` until each reaches a terminator.

    `others` is the set of addresses known to be OTHER functions; a jmp/jcc to
    one of those is a tail call, not part of this body.

    Returns (insns, calls, tables, tails, bad, jmptgts, term) where insns maps
    instruction address -> length, tables maps switch-table address -> byte
    length, `bad` is the first address that failed to decode, and `term` counts
    paths that ended at a real terminator.  `term` is what tells a function
    apart from a data blob that happens to disassemble for a few bytes.
    """
    insns, calls, tables, tails = {}, set(), {}, set()
    jmptgts = set()
    bad = None
    term = 0
    stack = [entry]
    while stack:
        va = stack.pop()
        while True:
            if va in insns or not im.inside(va):
                break
            if im.is_data(va):
                # walked into a jump table; this is not code, and it is not a
                # terminator either -- say so rather than counting it as one
                if bad is None:
                    bad = va
                break
            ins = im.insn(va)
            if not ins:
                if bad is None:
                    bad = va
                break
            insns[va] = ins.size
            m = ins.mnemonic
            nxt = va + ins.size

            if m in TERMINATORS or m.startswith('ret'):
                if m != 'int3':
                    term += 1
                break

            if m == 'call':
                op = ins.operands[0] if ins.operands else None
                if op is not None and op.type == CS_OP_IMM and im.inside(op.imm):
                    calls.add(op.imm)
                # `call` immediately followed by int3 padding is a noreturn
                # call; the bytes after it belong to the next function.
                if im.inside(nxt) and im.byte(nxt) == 0xCC:
                    break
                va = nxt
                continue

            if m[0] == 'j':
                op = ins.operands[0] if ins.operands else None
                if op is not None and op.type == CS_OP_IMM:
                    tgt = op.imm
                    if im.inside(tgt):
                        if m == 'jmp':
                            jmptgts.add(tgt)
                        if tgt != entry and tgt in others:
                            tails.add(tgt)          # tail call
                        elif tgt not in insns:
                            stack.append(tgt)
                    if m == 'jmp':
                        term += 1
                        break
                    va = nxt
                    continue
                if (op is not None and op.type == CS_OP_MEM
                        and op.mem.scale == 4 and op.mem.index != 0):
                    tbl = op.mem.disp & 0xFFFFFFFF
                    tgts, k = [], 0
                    while k <= 1024:
                        cell = tbl + k * 4
                        if (cell - im.p.image_base) not in im.p.relocs:
                            break
                        v = im.p.u32(cell)
                        if v is None or not im.inside(v):
                            break
                        tgts.append(v)
                        k += 1
                    if tgts:
                        tables[tbl] = 4 * len(tgts)
                        for t in tgts:
                            if t not in insns and t not in others:
                                stack.append(t)
                term += 1
                break                                # indirect jmp ends a path

            va = nxt
    return insns, calls, tables, tails, bad, jmptgts, term


def index_tables_after(im, tables, limit):
    """MSVC precedes a sparse `jmp [reg*4+tbl]` with `movzx eax, byte [eax+IDX]`
    and parks IDX immediately after ITS OWN dword table -- so a function with
    two switch statements lays out table, index, table, index, not both tables
    then both indexes.  Scanning only after the last dword table therefore finds
    at most one of them.

    Recovering these is not needed to size the FUNCTION -- the code extent ends
    before any of it -- but it keeps the trailing data region accounted for, so
    the .text sweep does not mistake an index table for undiscovered code.

    The Glide text emitter is the worked example: code ends at 0x100166FA, then
    table 0x100166FC (52 B), index 0x10016730 (0x4A B), a 2-byte `8B FF` align,
    table 0x1001677C (52 B), index 0x100167B0 (0x4A B), ending 0x100167FA.
    """
    out = {}
    for tbl, n in sorted(tables.items()):
        ncase = n // 4
        va, run = tbl + n, 0
        while im.inside(va) and va < limit and run < 4096:
            b = im.byte(va)
            if b >= ncase or b == 0xCC:
                break
            va += 1
            run += 1
        if run:
            out[tbl + n] = run
    return out


PROLOGUES = [
    bytes([0x8B, 0xFF, 0x55, 0x8B, 0xEC]),        # hotpatch pad + frame
    bytes([0x55, 0x8B, 0xEC]),                    # push ebp; mov ebp, esp
    bytes([0x6A, 0xFF]),                          # push -1        (SEH)
    bytes([0x53, 0x8B, 0xDC]),                    # push ebx; mov ebx, esp
    bytes([0x83, 0xEC]),                          # sub esp, imm8
    bytes([0x81, 0xEC]),                          # sub esp, imm32
]


def prologue_rank(im, va):
    """0 = no prologue, higher = stronger."""
    o = va - im.text_va
    w = im.text[o:o + 8]
    for k, sig in enumerate(PROLOGUES):
        if w.startswith(sig):
            return len(PROLOGUES) - k
    return 0


# ---------------------------------------------------------------------------
# extents
# ---------------------------------------------------------------------------

def measure(im, entries):
    """Flow-walk every entry and give each a contiguous code extent."""
    ent = set(entries)
    starts = sorted(ent)
    info = {}
    for e in starts:
        insns, calls, tables, tails, bad, jmptgts, term = walk(im, e, ent)
        idx = bisect.bisect_right(starts, e)
        nextent = starts[idx] if idx < len(starts) else im.hi

        addrs = sorted(a for a in insns if a >= e)
        end, detached, crossed = e, [], []
        for a in addrs:
            if a >= nextent:
                # Flow ran into a different function.  Do not swallow it; the
                # old map's over-long extents are exactly this, unreported.
                crossed.append(a)
                continue
            if a > end:
                # A gap inside one function's own body is only legitimate if
                # it is alignment filler or one of the function's own switch
                # tables.  Anything else is a detached block, and it is
                # reported rather than swallowed.
                q, ok = end, True
                while q < a:
                    if im.is_data(q):
                        q += 1
                        continue
                    f = filler_len(im, q)
                    if not f:
                        ok = False
                        break
                    q += f
                if not ok:
                    detached.append(a)
                    continue
            end = max(end, a + insns[a])
        mine = {t: n for t, n in im.tables.items() if end <= t < nextent}
        idxt = index_tables_after(im, mine, nextent)
        # A direct `jmp` whose target is outside this body is a tail call, so
        # the target is another function's entry point.  (A `jmp` INSIDE the
        # body is just a branch -- promoting those is the 0x10015F0B bug.)
        tails = set(tails) | {t for t in jmptgts if not (e <= t < end)}
        info[e] = dict(size=end - e, end=end, insns=len(insns), calls=calls,
                       tables=mine, idxtables=idxt, tails=tails, bad=bad,
                       term=term, detached=detached, crossed=crossed,
                       below=sorted(a for a in insns if a < e),
                       interior=set(a for a in insns if e < a < end))
    return info


def owned_map(im, info):
    """1 = code, 2 = attributed data (switch/index tables)."""
    own = bytearray(len(im.text))
    for e, d in info.items():
        for a in range(e - im.text_va, d['end'] - im.text_va):
            own[a] = 1
        for t, ln in list(d['tables'].items()) + list(d['idxtables'].items()):
            for a in range(t - im.text_va, t - im.text_va + ln):
                if 0 <= a < len(own):
                    own[a] = own[a] or 2
    return own


# ---------------------------------------------------------------------------
# the build
# ---------------------------------------------------------------------------

def build(path, log=sys.stderr):
    im = Image(path)
    fnptr, tablecell = relocated_pointers(im)
    naive = naive_call_sweep(im)

    def say(*a):
        if log:
            print(*a, file=log)

    # -- seeds that cannot be misaligned -----------------------------------
    seeds = set()
    ep = im.p.image_base + im.p.entry_rva
    if im.inside(ep):
        seeds.add(ep)
    seeds |= {v for v in im.p.exports if im.inside(v)}
    seeds |= fnptr
    say("seeds: entry+exports %d, relocated fn pointers %d"
        % (len(seeds) - len(fnptr), len(fnptr)))

    # -- alternate recursive descent and gap-filling to one fixpoint --------
    #
    # These two phases feed each other and must not be run once each: a
    # function found by filling a gap has call sites of its own, and the
    # functions those reach open further gaps.  Running descent to a fixpoint
    # and only then filling gaps stops after the first generation and leaves
    # most of .text unattributed.
    confirmed = set(seeds)
    realcalls, added_weak = set(), set()
    datab = set()               # .text offsets the sweep judged to be data
    info = measure(im, confirmed)
    for rnd in range(200):
        # (a) harvest call targets seen at real instruction sites, plus the
        #     targets of tail-call jumps that leave the body
        found, tailed = set(), set()
        for d in info.values():
            found |= d['calls']
            tailed |= d['tails']
        realcalls |= found
        new = (found | tailed) - confirmed
        if new:
            confirmed |= new
            added_weak |= (new - found)
            info = measure(im, confirmed)
            say("  round %d: +%d call/tail targets (total %d)"
                % (rnd + 1, len(new), len(confirmed)))
            continue

        # (b) no new targets: sweep whatever .text is still unattributed.
        #
        #     MSVC lays functions out back to back, so an unattributed run
        #     between two known functions is made of WHOLE functions: its first
        #     non-padding byte is a function start whether or not it matches a
        #     prologue pattern.  Gating this on a prologue table is what left
        #     30 KB of plain `mov eax,[esp+4]` and `push esi` bodies stranded
        #     -- those are ordinary __cdecl functions with no frame pointer.
        own = owned_map(im, info)
        cands = set()
        i, n = 0, len(own)
        while i < n:
            if own[i] or im.isdata[i] or i in datab:
                i += 1
                continue
            va = im.text_va + i
            f = filler_len(im, va)
            if f:
                i += f
                continue
            ins, _c, _tb, _tl, bad, _jt, term = walk(im, va, confirmed | cands)
            mx = max((a + s for a, s in ins.items()), default=va)
            # A run of bytes is only a FUNCTION if it disassembles all the way
            # and every path it opens reaches a terminator.  .text also carries
            # descriptor tables (16-byte records of relocated .data pointers,
            # e.g. at 0x10072A50 and 0x10073B00 in BRGlide) which decode for a
            # few bytes and then hit an invalid opcode.  Sweeping those in
            # produced 44 "functions" made of table rows.
            if not ins or term == 0 or (bad is not None and va <= bad < mx):
                datab.add(i)
                i += 1
                continue
            cands.add(va)
            i = max(mx - im.text_va, i + 1)
        cands -= confirmed
        if not cands:
            break
        confirmed |= cands
        added_weak |= cands
        info = measure(im, confirmed)
        say("  round %d: +%d swept from unattributed runs (total %d)"
            % (rnd + 1, len(cands), len(confirmed)))

    # -- demote entries that turned out to be mid-flow labels ---------------
    # A weak candidate can land inside a function discovered later.  A real
    # call target never gets demoted -- something calls it, so it is an entry,
    # and if it also sits inside another body that is a finding, not an error.
    for _ in range(8):
        interior = set()
        for d in info.values():
            interior |= d['interior']
        drop = {e for e in confirmed if e in interior and e not in realcalls
                and e not in seeds}
        if not drop:
            break
        confirmed -= drop
        added_weak -= drop
        info = measure(im, confirmed)
        say("  demoted %d entries lying inside another function's flow"
            % len(drop))

    ev = dict(fnptr=fnptr, tablecell=tablecell, naive=naive,
              realcalls=realcalls, weak=added_weak, seeds=seeds,
              data=set(im.text_va + i for i in datab))
    return im, info, ev


def main():
    argv = sys.argv[1:]
    report = '--report' in argv
    jsonout = None
    if '--json' in argv:
        k = argv.index('--json')
        jsonout = argv[k + 1]
        del argv[k:k + 2]
    argv = [a for a in argv if not a.startswith('--')]
    path = argv[0] if argv else 'orig/BRGlide.dll'
    out = argv[1] if len(argv) > 1 else None

    im, info, ev = build(path)
    rows = sorted(info.items())
    total = len(im.text)
    code = sum(d['size'] for _, d in rows)
    data = sum(sum(d['tables'].values()) + sum(d['idxtables'].values())
               for _, d in rows)
    own = owned_map(im, info)
    blob = {a - im.text_va for a in ev['data']}
    pad = unacc = 0
    i = 0
    while i < total:
        if own[i] or i in blob:
            i += 1
            continue
        f = filler_len(im, im.text_va + i)
        if f:
            pad += f
            i += f
        else:
            unacc += 1
            i += 1
    nz = sorted(d['size'] for _, d in rows if d['size'])

    print("%s" % path)
    print("  .text                %d bytes at %08X" % (total, im.text_va))
    print("  functions            %d" % len(rows))
    print("    from real calls    %d" % len(ev['realcalls'] &
                                            set(x for x, _ in rows)))
    print("    from fn pointers   %d" % len(ev['fnptr'] & set(x for x, _ in rows)))
    print("    weak (gap+prolog)  %d" % len(ev['weak']))
    print("  naive 0xE8 sweep     %d targets, of which %d are NOT real calls"
          % (len(ev['naive']), len(ev['naive'] - ev['realcalls'])))
    print("  code bytes           %d (%.1f%%)" % (code, 100.0 * code / total))
    print("  attributed tables    %d bytes" % data)
    print("  inter-function pad   %d bytes" % pad)
    print("  data blobs in .text  %d bytes (refused by the sweep)"
          % len(ev['data']))
    print("  UNACCOUNTED          %d bytes (%.2f%%)"
          % (unacc, 100.0 * unacc / total))
    print("  median size          %d   max %d" % (nz[len(nz) // 2], nz[-1]))
    print("  extents crossing the next entry  %d"
          % sum(1 for _, d in rows if d['crossed']))
    print("  with detached blocks             %d"
          % sum(1 for _, d in rows if d['detached']))
    print("  undecodable byte inside          %d"
          % sum(1 for _, d in rows if d['bad']))

    if jsonout:
        with open(jsonout, 'w') as f:
            json.dump({'%08X' % e: dict(
                size=d['size'], insns=d['insns'],
                tables={'%08X' % t: n for t, n in d['tables'].items()},
                crossed=['%08X' % a for a in d['crossed'][:4]],
                detached=['%08X' % a for a in d['detached'][:4]],
                bad=('%08X' % d['bad']) if d['bad'] else None)
                for e, d in rows}, f)

    if out and not report:
        with open(out, 'w', newline='') as f:
            w = csv.writer(f)
            w.writerow(['va', 'size', 'name'])
            for e, d in rows:
                w.writerow(['0x%08X' % e, d['size'], im.p.exports.get(e, '')])
        print("  wrote %s" % out)


if __name__ == '__main__':
    main()
