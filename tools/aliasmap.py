#!/usr/bin/env python3
"""aliasmap.py -- find ORIGINAL addresses that this port models with MORE THAN
ONE host object.

THE DEFECT CLASS THIS MEASURES
==============================

The original has one dword at 0x10AA2904.  This tree has nine declarations
that each claim to be it.  Three of them have real storage, and they drift
apart after the first write: a menu row publishes a phase transition into one,
the frame loop reads another, and the transition is invisible.  That is not a
hypothetical -- every instance found so far has been a real bug (see
CONVENTIONS.md, "Aliased storage: a link-clean bug").

The link is always clean, because two host names for one original address are
two distinct C symbols.  So the compiler cannot see this, and neither can a
duplicate-symbol check.  The only handle is the ADDRESS, which this tree
records in two machine-readable-enough places: a trailing comment on the
declaration, and the address baked into the identifier (pAA2904, g_brAA289C,
n0AA010).

WHAT IT MEASURES
================

Three independent channels, each reported with its own confidence, and every
extracted address is required to EXIST in config/globals.csv -- a gate that
costs nothing and kills the bulk of the false positives:

  decl-comment  a C declarator whose SAME LINE carries `/* ... 0xADDR`.
                Highest confidence: a declarator plus an address on one line is
                not prose.
  name-encoded  an identifier containing a 5-, 6- or 7-hex-digit run that
                completes to an address in globals.csv.  This tree names fields
                after the address they model, which is what makes the defect
                searchable at all.
  lead-comment  the address appears alone in the comment block IMMEDIATELY
                above a declarator, with nothing else between.  Lowest
                confidence and reported separately, because this is the shape
                that made tools/isported.py wrong seven times.

For every host name it then measures TRAFFIC over the whole port -- reads and
writes, counted on comment-stripped source.  A name with many writers next to a
name with none is the dangerous shape: the writerless name is a field nobody
fills and the reader of it is looking at a value that never changes; the
readerless name is a value nobody consumes.  Both mean the OTHER name owns the
traffic.  Ranking is by that asymmetry, not by raw count.

It also reports the TRANSPOSE -- one host name claiming several original
addresses -- because the sfx-ratio defect had that shape: the original writes
the value to 0x118EEF48 and mirrors it at 0x1184C088, and the port modelled the
first only, so the mirror had no host object and the dirty-check shadow it
feeds silently stopped re-sending the voice.

Glide and D3D addresses for ONE object are unified where the pairing can be
DERIVED (--pair, cached in config/globals_shared.csv): for a `body`-matched
function pair in config/shared.csv the two bodies are byte-identical except at
relocated dwords, so a differing 4-byte-aligned dword that is a valid global in
each image is the same object under two numbers.  `matched_by = body-dup:N` is
EXCLUDED -- it stamps one Glide address onto N D3D functions, and treating it
as a pairing manufactured five false duplicates on a previous pass.

WHAT IT CANNOT SEE
==================

  * An object with NO address recorded anywhere near its declaration.  If a
    module models 0x10AA2904 as `BrUiNav::pCurrent` with the number only in a
    paragraph forty lines up, this tool will not connect them.  It under-reports,
    and under-reporting is the DANGEROUS direction here (CONVENTIONS.md); treat
    a quiet address as unmeasured, not as clean.

  * Aliases that are not keyed by an address at all.  The phase-struct case --
    BrPhase_ {nPages@28, aPages@32, aFlags@204, sizeof 304} against BrUiPhase
    {cScreen@0, apScreen@8, aF6C@184, sizeof 272} -- is two incompatible models
    of one HEAP object, and a heap object has no original address to key on.
    This tool reaches that case only INDIRECTLY, through the type disagreement
    among the pointers that point at it (see the `types` column), and it will
    miss any such pair that no global points at.  Measured recall on the
    five-case calibration set is printed by --selftest; it is not 5/5.

  * Whether an alias is LIVE.  A field of a context struct that nothing ever
    instantiates is a latent alias, not a bug.  This tool counts textual
    traffic; it does not know which structs have instances.  Use the `sites`
    column to see where storage actually is.

  * Anything about CODE.  Two transcriptions of one original function under the
    two builds' addresses is the same defect class and a different tool
    (config/ported.csv grouped through config/shared.csv).

  * Field names are matched textually, so a field name that several unrelated
    structs share (`ratio`, `pOwner`) has its traffic pooled.  Such names are
    flagged `ambiguous` and their counts are an upper bound.

CALIBRATION
===========

`--selftest` runs the tool against the five known instances from
CONVENTIONS.md.  Three of the five are RESOLVED in the current tree, so they
cannot be rediscovered by observation; the honest test is to reinstate each
shape and confirm the tool flags it, which --selftest does with in-memory
mutations of the source text.  It prints per-case PASS/MISS and an overall
recall.  A detector that fails its calibration set is worse than none, because
its output still looks authoritative -- so if recall is below 5/5 the banner
says so and says which case is missed and why.

USAGE
    tools/aliasmap.py                 # the sweep
    tools/aliasmap.py --selftest      # calibration against the five known cases
    tools/aliasmap.py --addr 0x10AA2904
    tools/aliasmap.py --pair          # (re)derive the Glide/D3D global pairing
    tools/aliasmap.py --transpose     # one host name, several addresses
"""

import csv
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG = os.path.join(ROOT, "config")
PORT = os.path.join(ROOT, "port")

# ---------------------------------------------------------------- the corpus

# Declarations are looked for here.  tests/ is scanned for TRAFFIC but not
# for declarations: a test that defines storage for a global it links against
# is not a competing model of the original, it is a link fixture.
DECL_DIRS = ["include", "src"]
TRAFFIC_DIRS = ["include", "src", "tests"]


def walk(dirs):
    out = []
    for d in dirs:
        base = os.path.join(PORT, d)
        for dirpath, _dirnames, filenames in os.walk(base):
            for fn in filenames:
                if fn.endswith((".c", ".h")):
                    out.append(os.path.join(dirpath, fn))
    return sorted(out)


def strip_comments(text):
    """Replace comment bodies with spaces, preserving line structure, so that
    traffic counts never see prose.  Also blanks string literals."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c in "\"'":
            q = c
            j = i + 1
            while j < n and text[j] != q:
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


# ------------------------------------------------------- the original's data

def load_globals():
    """The gate every extracted address must pass: it must land in a NON-.text
    section of one of the two shipped images.

    config/globals.csv is deliberately NOT the gate.  It is the set of
    addresses globals.py could decode a reference to, and it is incomplete --
    it is missing 0x105D17A4 and 0x1184C088, two of the five calibration cases.
    Gating on it drove this tool's recall to 3/5 with both misses in the
    dangerous direction, "no host object here", which is precisely how
    tools/isported.py kept reporting ported work as missing.  The section table
    is complete by construction and is used instead; globals.csv is kept only
    as a corroborator for the name-encoded channel.
    """
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    import pe  # noqa: E402

    ranges = []
    for name in ("BRD3D.dll", "BRGlide.dll"):
        p = pe.load(os.path.join(ROOT, "orig", name))
        for s in p.sections:
            nm = s.name.rstrip("\0")
            if nm in (".text", ".reloc", ".rsrc"):
                continue
            lo = p.image_base + s.vaddr
            ranges.append((lo, lo + max(s.vsize, s.raw_size)))
    ranges.sort()

    refs = set()
    with open(os.path.join(CONFIG, "globals.csv")) as f:
        for row in csv.DictReader(f):
            refs.add(int(row["addr"], 16))

    class Gate(object):
        referenced = refs

        def __contains__(self, v):
            return any(a <= v < b for a, b in ranges)

    return Gate()


# ------------------------------------------- Glide/D3D pairing, DERIVED

PAIR_CACHE = os.path.join(CONFIG, "globals_shared.csv")


def derive_pairing(verbose=True):
    """Two builds of one function are byte-identical except where a RELOCATED
    dword names an address, so the pairing is read off the relocation tables
    rather than guessed from which bytes differ: an offset that carries a
    32-bit fixup in BOTH images holds the same object under two numbers.

    `body-dup:N` rows are skipped: they stamp one Glide address onto N D3D
    functions and the evidence does not say which, so they are AMBIGUOUS and
    manufactured five false duplicates the last time they were treated as a
    pairing.  `shape` rows are a similarity, not a match, and are skipped too.
    """
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    import pe  # noqa: E402

    d3d = pe.load(os.path.join(ROOT, "orig", "BRD3D.dll"))
    gl = pe.load(os.path.join(ROOT, "orig", "BRGlide.dll"))

    def datarange(img):
        return [(img.image_base + s.vaddr,
                 img.image_base + s.vaddr + max(s.vsize, s.raw_size))
                for s in img.sections if s.name.rstrip("\0") != ".text"]

    d3d_r, gl_r = datarange(d3d), datarange(gl)

    def isdata(v, rs):
        return any(a <= v < b for a, b in rs)

    votes = defaultdict(int)
    used = 0
    with open(os.path.join(CONFIG, "shared.csv")) as f:
        for row in csv.DictReader(f):
            if row["class"] != "shared":
                continue
            mb = row.get("matched_by", "")
            if mb != "body":
                continue          # body-dup:N is AMBIGUOUS; shape is not a match
            if not row["glide_va"]:
                continue
            size = int(row["size"])
            if size < 5 or size > 65536:
                continue
            dva = int(row["d3d_va"], 16)
            gva = int(row["glide_va"], 16)
            a = d3d.read(dva, size)
            b = gl.read(gva, size)
            if a is None or b is None or len(a) != len(b) != size:
                continue
            used += 1
            drva = dva - d3d.image_base
            grva = gva - gl.image_base
            for k in range(0, size - 3):
                if (drva + k) in d3d.relocs and (grva + k) in gl.relocs:
                    va = int.from_bytes(a[k:k + 4], "little")
                    vb = int.from_bytes(b[k:k + 4], "little")
                    if va != vb and isdata(va, d3d_r) and isdata(vb, gl_r):
                        votes[(va, vb)] += 1

    rows = sorted((d, g, c) for (d, g), c in votes.items())
    with open(PAIR_CACHE, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["d3d_addr", "glide_addr", "votes"])
        for d, g, c in rows:
            w.writerow(["0x%08X" % d, "0x%08X" % g, c])
    if verbose:
        print("derived %d Glide/D3D address pairs from %d body-matched "
              "function pairs -> %s" % (len(rows), used, PAIR_CACHE))
    return rows


def load_pairing():
    if not os.path.exists(PAIR_CACHE):
        return {}
    uf = {}

    def find(x):
        while uf.get(x, x) != x:
            uf[x] = uf.get(uf[x], uf[x])
            x = uf[x]
        return x

    with open(PAIR_CACHE) as f:
        for row in csv.DictReader(f):
            d = int(row["d3d_addr"], 16)
            g = int(row["glide_addr"], 16)
            ra, rb = find(d), find(g)
            if ra != rb:
                uf[rb] = ra
            uf.setdefault(d, find(d))
            uf.setdefault(g, find(d))
    return {k: find(k) for k in uf}


# ------------------------------------------------------------- extraction

# A C declarator: <type words and stars> <name> [array] ;
DECLARATOR = re.compile(
    r"""^\s*
        (?:extern\s+|static\s+|const\s+|volatile\s+|struct\s+|union\s+|
           unsigned\s+|signed\s+)*
        (?P<type>[A-Za-z_][A-Za-z0-9_]*(?:\s+[A-Za-z_][A-Za-z0-9_]*)*)
        # The type and the name MUST be separated by whitespace or a star.
        # Without this the greedy type swallows all but the last letter of the
        # name -- `float primR;` parsed as type `float prim`, name `R`, which
        # is how the first run of this tool reported the calibration case
        # under three nonsense names.
        (?P<ptr>\s*\*+\s*|\s+)
        (?P<name>[A-Za-z_][A-Za-z0-9_]*)
        \s*(?P<arr>(?:\[[^\];]*\])*)
        \s*(?:=[^;]*)?;""",
    re.VERBOSE,
)

# A function-pointer declarator: <ret> (*name)(args);
FNPTR = re.compile(
    r"^\s*(?P<type>[A-Za-z_][A-Za-z0-9_ \t*]*?)"
    r"\(\s*\*\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\)\s*\([^;]*\)\s*;")

ADDR = re.compile(r"0x1[0-9A-Fa-f]{7}\b")
# 5, 6 or 7 hex digits inside an identifier, not preceded by another hex digit
NAMEHEX = re.compile(r"(?<![0-9A-Fa-f])([0-9A-F][0-9A-F]{4,6})(?![0-9A-Fa-f])")

# Declarators whose "name" is really a keyword or a typedef head.
NOT_A_NAME = {
    "return", "typedef", "sizeof", "if", "else", "while", "for", "do",
    "switch", "case", "break", "continue", "goto", "void",
}


class Decl:
    __slots__ = ("addr", "name", "ctype", "file", "line", "channel", "owner")

    def __init__(self, addr, name, ctype, file, line, channel, owner):
        self.addr = addr
        self.name = name
        self.ctype = ctype
        self.file = file
        self.line = line
        self.channel = channel
        self.owner = owner


def enclosing_struct(lines, i):
    """Nearest `struct X {` / `typedef struct X {` above line i, so a field can
    be reported as Owner::field rather than as a bare name."""
    depth = 0
    for j in range(i, -1, -1):
        s = lines[j]
        depth += s.count("}") - s.count("{")
        if depth < 0:
            m = re.search(r"struct\s+([A-Za-z_][A-Za-z0-9_]*)?\s*\{", s)
            if m:
                return m.group(1) or "<anon>"
            m = re.search(r"^\s*(?:typedef\s+)?(?:struct|union)\s*\{", s)
            if m:
                for k in range(j, min(j + 400, len(lines))):
                    m2 = re.match(r"^\s*\}\s*([A-Za-z_][A-Za-z0-9_]*)", lines[k])
                    if m2:
                        return m2.group(1)
                return "<anon>"
            return None
    return None


def declarator(line):
    m = FNPTR.match(line)
    if m:
        return m.group("name"), (m.group("type").strip() + " (*)()")
    m = DECLARATOR.match(line)
    if not m:
        return None
    name = m.group("name")
    if name in NOT_A_NAME:
        return None
    ctype = (m.group("type").strip() + m.group("ptr").strip()
             + m.group("arr")).strip()
    # `int foo(void);` is a function, not storage.
    if "(" in line[: line.index(name) + len(name)]:
        return None
    return name, ctype


def name_addresses(name, known, file_literals):
    """Addresses baked into an identifier (pAA2904, g_brAA289C, n0AA010).

    The section-table gate alone is far too loose here: nearly the whole
    0x10094000..0x118AD000 range is data, so an ordinary identifier containing
    a run of hex letters -- `fADDED` completes to 0x100ADDED -- would pass it.
    So the candidate must ALSO be corroborated: either globals.py saw a
    reference to it, or the literal appears somewhere in the same file.  A
    field genuinely named after an address satisfies one of those; `fADDED`
    satisfies neither.
    """
    out = []
    for m in NAMEHEX.finditer(name):
        h = m.group(1)
        for prefix in ("0x10", "0x1", "0x"):
            try:
                v = int(prefix + h, 16)
            except ValueError:
                continue
            if v < 0x10000000 or v not in known:
                continue
            if v in known.referenced or v in file_literals:
                out.append(v)
            break
    return out


def extract(files, known, text_override=None):
    decls = []
    for path in files:
        try:
            src = (text_override or {}).get(path)
            if src is None:
                with open(path, encoding="utf-8", errors="replace") as f:
                    src = f.read()
        except OSError:
            continue
        lines = src.split("\n")
        rel = os.path.relpath(path, ROOT)
        file_literals = {int(a, 16) for a in ADDR.findall(src)}
        for i, raw in enumerate(lines):
            # split the line into code and trailing comment
            cpos = raw.find("/*")
            cpos2 = raw.find("//")
            if cpos < 0 or (0 <= cpos2 < cpos):
                cpos = cpos2
            code = raw if cpos < 0 else raw[:cpos]
            comment = "" if cpos < 0 else raw[cpos:]

            d = declarator(code)
            if not d:
                continue
            name, ctype = d
            owner = enclosing_struct(lines, i)

            # A LOCAL VARIABLE is not a model of an original object.  Locals
            # are indented and have no enclosing struct; file-scope storage is
            # at column 0 and struct fields have an owner.  Without this the
            # lead-comment channel attaches whatever address the comment above
            # a function mentions to that function's first local -- which is
            # tools/isported.py's defect (3) in a new tool, and it produced
            # `cb`, `slot` and `pL` as competing models of three globals.
            if owner is None and raw[:1] in (" ", "\t"):
                continue

            seen = set()

            # --- channel 1: address in the SAME-LINE trailing comment
            for a in ADDR.findall(comment):
                v = int(a, 16)
                if v in known and v not in seen:
                    seen.add(v)
                    decls.append(Decl(v, name, ctype, rel, i + 1,
                                      "decl-comment", owner))

            # --- channel 2: address baked into the identifier
            for v in name_addresses(name, known, file_literals):
                if v not in seen:
                    seen.add(v)
                    decls.append(Decl(v, name, ctype, rel, i + 1,
                                      "name-encoded", owner))

            # --- channel 3: the comment block IMMEDIATELY above
            if not seen:
                blk, j = [], i - 1
                while j >= 0 and len(blk) < 12:
                    s = lines[j].strip()
                    if not s:
                        break
                    if s.startswith(("*", "/*")) or s.endswith("*/"):
                        blk.append(lines[j])
                        if s.startswith("/*"):
                            break
                        j -= 1
                        continue
                    break
                addrs = {int(a, 16) for ln in blk for a in ADDR.findall(ln)}
                addrs = {a for a in addrs if a in known}
                if len(addrs) == 1:
                    v = addrs.pop()
                    decls.append(Decl(v, name, ctype, rel, i + 1,
                                      "lead-comment", owner))
    return decls


# ---------------------------------------------------------------- traffic

ASSIGN = r"(?:=(?!=)|\+=|-=|\*=|/=|\|=|&=|\^=|<<=|>>=)"


def measure_traffic(names):
    """Reads and writes per host name, over comment-stripped source."""
    reads = defaultdict(int)
    writes = defaultdict(int)
    files_with = defaultdict(set)
    pats = {}
    for n in names:
        pats[n] = (
            re.compile(r"\b" + re.escape(n) + r"\b"),
            re.compile(r"\b" + re.escape(n) +
                       r"\b\s*(?:\[[^\]\n]*\]|\.[A-Za-z_][A-Za-z0-9_]*)?\s*"
                       + ASSIGN),
            re.compile(r"(?:&\s*|memset\s*\(\s*|memcpy\s*\(\s*)\b"
                       + re.escape(n) + r"\b"),
            re.compile(r"\b" + re.escape(n) + r"\b\s*(?:\+\+|--)"),
        )
    for path in walk(TRAFFIC_DIRS):
        with open(path, encoding="utf-8", errors="replace") as f:
            code = strip_comments(f.read())
        rel = os.path.relpath(path, ROOT)
        for n, (pa, pw, pamp, pinc) in pats.items():
            if n not in code:
                continue
            tot = len(pa.findall(code))
            if not tot:
                continue
            w = len(pw.findall(code)) + len(pamp.findall(code)) \
                + len(pinc.findall(code))
            writes[n] += w
            reads[n] += max(tot - w, 0)
            files_with[n].add(rel)
    return reads, writes, files_with


# ---------------------------------------------------------------- reporting

def build(known, files=None, text_override=None):
    files = files or walk(DECL_DIRS)
    decls = extract(files, known, text_override)
    pair = load_pairing()

    by_addr = defaultdict(list)
    for d in decls:
        by_addr[pair.get(d.addr, d.addr)].append(d)
    return by_addr, decls


def qualified(d):
    return ("%s::%s" % (d.owner, d.name)) if d.owner else d.name


def model_of(d):
    """A MODEL is an owning struct, or -- for file-scope storage -- the global
    itself.  An alias is two MODELS of one address, not two fields: a struct
    whose every field carries the struct's own vtable address is annotated,
    not aliased, and grouping by field name reported thirteen `models' of one
    phase vtable."""
    return d.owner or ("<global> " + d.name)


def report(by_addr, known, reads, writes, files_with, ambiguous, limit=None):
    rows = []
    for addr, ds in by_addr.items():
        models = defaultdict(list)
        for d in ds:
            models[model_of(d)].append(d)
        if len(models) < 2:
            continue
        types = {re.sub(r"\s+", " ", d.ctype) for d in ds}
        conf = max(
            {"decl-comment": 2, "name-encoded": 2, "lead-comment": 1}[d.channel]
            for d in ds)
        # THE TELL: a model whose every name has no writer, or no reader.
        # The other model owns the traffic, and the two have drifted.
        silent = []
        for m, mds in models.items():
            r = sum(reads[d.name] for d in mds)
            w = sum(writes[d.name] for d in mds)
            if w == 0 or r == 0:
                silent.append((m, "no writer" if w == 0 else "no reader"))
        rows.append((addr, models, types, silent, conf))

    rows.sort(key=lambda r: (-len(r[3]), -len(r[1]), -len(r[2]), r[0]))

    print("\n%-12s %-6s %-6s %-5s %s" %
          ("ADDRESS", "models", "types", "conf", "the tell"))
    print("-" * 100)
    n = 0
    for addr, models, types, silent, conf in rows:
        if limit and n >= limit:
            print("... %d more (use --all)" % (len(rows) - n))
            break
        n += 1
        tell = "  <-- " + "; ".join("%s %s" % (m, w) for m, w in silent) \
            if silent else ""
        print("0x%08X   %-6d %-6d %-5s%s" %
              (addr, len(models), len(types),
               "high" if conf == 2 else "low", tell[:60]))
        order = sorted(models.items(),
                       key=lambda kv: -sum(reads[d.name] + writes[d.name]
                                           for d in kv[1]))
        for m, mds in order:
            r = sum(reads[d.name] for d in mds)
            w = sum(writes[d.name] for d in mds)
            amb = " AMBIG" if any(d.name in ambiguous for d in mds) else ""
            print("               %-28s r=%-4d w=%-4d %-20s %s%s" %
                  (m[:28], r, w,
                   ",".join(sorted({d.name for d in mds}))[:20],
                   sorted({"%s:%d" % (d.file, d.line)
                           for d in mds})[0][:34], amb))
    print("\n%d addresses modelled by more than one host object" % len(rows))
    return rows


def transpose(decls, known):
    by_name = defaultdict(set)
    site = {}
    for d in decls:
        by_name[qualified(d)].add(d.addr)
        site[qualified(d)] = "%s:%d" % (d.file, d.line)
    out = [(k, v) for k, v in by_name.items() if len(v) > 1]
    print("\nTRANSPOSE -- one host name, several original addresses")
    print("(the sfx-ratio shape: the original mirrors a value and the port "
          "models one copy)")
    print("-" * 100)
    for k, v in sorted(out, key=lambda kv: -len(kv[1])):
        print("  %-34s %-4d  %s   %s" %
              (k, len(v), " ".join("0x%08X" % a for a in sorted(v)),
               site[k]))
    print("%d host names claiming more than one address" % len(out))
    return out


def find_ambiguous(decls):
    """A bare field name several unrelated structs share pools its traffic."""
    owners = defaultdict(set)
    for d in decls:
        owners[d.name].add((d.owner, d.addr))
    return {n for n, o in owners.items()
            if len({x[0] for x in o}) > 1 and len({x[1] for x in o}) > 1}


# ---------------------------------------------------------------- selftest

# The calibration set: the five instances CONVENTIONS.md records, each with
# the address to key on and -- where the case has since been RESOLVED -- the
# in-memory source mutation that reinstates the historical shape.  A resolved
# case cannot be rediscovered by observation, so reinstating it is the only
# honest way to measure recall against it.
CAL = [
    dict(label="1 lightOff/prim", addr=0x105D17A4,
         mutate=("include/br_dl.h",
                 "    float     prim[4];",
                 "    float     lightOff[3];  /* 0x105D17A4, 0x105D17B4, "
                 "0x105CE2D0 */\n    float     prim[4];")),
    dict(label="2 sfx ratio mirror", addr=0x1184C080,
         # The historical shape, exactly: the original writes the ratio to
         # 0x118EEF48 and mirrors it at 0x1184C088, and the port modelled the
         # first array only.  So the mutation DELETES the second declaration.
         mutate=("include/br_sfxsrc.h",
                 "extern BrSfxChan g_aBrSfxChanApplied[BR_SFX_CHANNELS];",
                 "/* deleted by aliasmap --selftest */")),
    dict(label="3 0x106C0964 three names", addr=0x106C0964,
         mutate=("include/slice8_83.h",
                 "/* 0x106C0964 and friends",
                 "extern void *g_brHook6C0964;   /* 0x106C0964 */\n"
                 "/* 0x106C0964 and friends")),
    dict(label="4 0x10AA2904 three objects", addr=0x10AA2904, mutate=None),
    dict(label="5 phase struct two models", addr=0x10AA2904, mutate=None,
         # Reached only indirectly: BrPhase_ and BrUiPhase are two models of a
         # HEAP object with no original address, so the handle is the type
         # disagreement among the pointers to it.
         need_types=2),
]


def selftest():
    known = load_globals()
    files = walk(DECL_DIRS)
    print("CALIBRATION -- the five known instances from CONVENTIONS.md")
    print("=" * 78)
    results = []
    for case in CAL:
        label, addr = case["label"], case["addr"]
        override = {}
        note = "observed in the tree as it stands"
        if case["mutate"]:
            rel, anchor, repl = case["mutate"]
            p = os.path.join(ROOT, rel)
            with open(p) as f:
                src = f.read()
            if anchor not in src:
                results.append((label, addr, False,
                                "MUTATION ANCHOR MISSING in %s -- the "
                                "calibration itself is stale" % rel, ""))
                continue
            override[p] = src.replace(anchor, repl, 1)
            note = "RESOLVED in tree; historical shape reinstated in %s" % rel
        by_addr, _decls = build(known, files, override)
        ds = by_addr.get(addr, [])
        n = len({d.name for d in ds})
        t = len({re.sub(r"\s+", " ", d.ctype) for d in ds})
        ok = n >= 2 and t >= case.get("need_types", 1)
        why = ""
        if not ok and n < 2:
            why = ("this address has %d host object(s), not two. The defect "
                   "here is a MISSING model, not a duplicated one -- the "
                   "original mirrors the value at a second address and the "
                   "port had no object for it at all. An address -> objects "
                   "duplicate detector cannot see an object that is absent, "
                   "and the second address is in any case only ever reached "
                   "in this tree as base+offset, never annotated on a "
                   "declaration." % n)
        results.append((label, addr, ok,
                        "%s -- %d host names, %d distinct types: %s" %
                        (note, n, t, ", ".join(sorted({qualified(d)
                                                       for d in ds}))[:90]),
                        why))
    npass = sum(1 for r in results if r[2])
    for label, addr, ok, note, why in results:
        print("  [%s] %-28s 0x%08X" % ("PASS" if ok else "MISS", label, addr))
        print("         %s" % note)
        if why:
            for line in re.findall(r".{1,66}(?:\s|$)", why):
                print("         ! %s" % line.strip())
    print("-" * 78)
    print("RECALL: %d/%d" % (npass, len(results)))
    if npass < len(results):
        print("\n*** THIS DETECTOR DOES NOT PASS ITS CALIBRATION SET. ***")
        print("Its output is a LEAD LIST, not a worklist. Each miss is "
              "explained above and in 'WHAT IT CANNOT SEE'; the misses are "
              "cases whose shape this tool is structurally blind to, not "
              "cases it looked at and got wrong.")
    return npass, len(results)


def main():
    args = sys.argv[1:]
    if "--pair" in args:
        derive_pairing()
        if len(args) == 1:
            return 0
    if "--selftest" in args:
        npass, tot = selftest()
        return 0 if npass == tot else 1

    known = load_globals()
    by_addr, decls = build(known)
    ambiguous = find_ambiguous(decls)

    if "--addr" in args:
        want = int(args[args.index("--addr") + 1], 16)
        ds = by_addr.get(want, [])
        names = sorted({d.name for d in ds})
        reads, writes, files_with = measure_traffic(names)
        print("0x%08X -- %d host declarations, %d distinct names"
              % (want, len(ds), len(names)))
        for d in sorted(ds, key=lambda d: (d.file, d.line)):
            print("  %-30s %-24s r=%-4d w=%-4d  %-12s %s:%d"
                  % (qualified(d), re.sub(r"\s+", " ", d.ctype)[:24],
                     reads[d.name], writes[d.name], d.channel, d.file, d.line))
        for n in names:
            if files_with[n]:
                print("  %-24s touched in: %s"
                      % (n, " ".join(sorted(files_with[n]))[:400]))
        return 0

    allnames = sorted({d.name for d in decls})
    reads, writes, files_with = measure_traffic(allnames)

    print("aliasmap: %d declarations bound to %d distinct original addresses"
          % (len(decls), len({d.addr for d in decls})))
    ch = defaultdict(int)
    for d in decls:
        ch[d.channel] += 1
    print("  by channel: %s" % dict(ch))
    if not load_pairing():
        print("  NOTE: no config/globals_shared.csv -- Glide and D3D numbers "
              "for one object are NOT unified. Run --pair.")

    if "--transpose" in args:
        transpose(decls, known)
        return 0

    report(by_addr, known, reads, writes, files_with, ambiguous,
           limit=None if "--all" in args else 40)
    print("\nRun tools/aliasmap.py --selftest before believing any of this.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
